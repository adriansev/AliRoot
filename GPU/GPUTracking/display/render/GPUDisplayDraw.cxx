//**************************************************************************\
//* This file is property of and copyright by the ALICE Project            *\
//* ALICE Experiment at CERN, All rights reserved.                         *\
//*                                                                        *\
//* Primary Authors: Matthias Richter <Matthias.Richter@ift.uib.no>        *\
//*                  for The ALICE HLT Project.                            *\
//*                                                                        *\
//* Permission to use, copy, modify and distribute this software and its   *\
//* documentation strictly for non-commercial purposes is hereby granted   *\
//* without fee, provided that the above copyright notice appears in all   *\
//* copies and that both the copyright notice and this permission notice   *\
//* appear in the supporting documentation. The authors make no claims     *\
//* about the suitability of this software for any purpose. It is          *\
//* provided "as is" without express or implied warranty.                  *\
//**************************************************************************

/// \file GPUDisplayDraw.cxx
/// \author David Rohr

#ifndef GPUCA_NO_ROOT
#include "Rtypes.h" // Include ROOT header first, to use ROOT and disable replacements
#endif

#include "GPUDisplay.h"
#include "GPUTRDGeometry.h"
#include "GPUO2DataTypes.h"
#include "GPUTRDTracker.h"
#include "GPUTRDTrackletWord.h"
#include "GPUQA.h"
#include "GPUTPCClusterData.h"
#include "GPUTPCConvertImpl.h"
#include "GPUTPCGMPropagator.h"
#include "GPUTPCMCInfo.h"
#include "GPUParam.inc"
#include "GPUCommonMath.h"

#include <type_traits>

#ifdef GPUCA_HAVE_O2HEADERS
#include "DataFormatsITS/TrackITS.h"
#include "SimulationDataFormat/MCCompLabel.h"
#include "SimulationDataFormat/ConstMCTruthContainer.h"
#include "GPUTrackParamConvert.h"
#endif

#ifdef WITH_OPENMP
#include <omp.h>
#endif

using namespace GPUCA_NAMESPACE::gpu;

#define GET_CID(slice, i) (mParam->par.earlyTpcTransform ? mIOPtrs->clusterData[slice][i].id : (mIOPtrs->clustersNative->clusterOffset[slice][0] + i))

const GPUTRDGeometry* GPUDisplay::trdGeometry() { return (GPUTRDGeometry*)mCalib->trdGeometry; }
const GPUTPCTracker& GPUDisplay::sliceTracker(int32_t iSlice) { return mChain->GetTPCSliceTrackers()[iSlice]; }

inline void GPUDisplay::insertVertexList(std::pair<vecpod<int32_t>*, vecpod<uint32_t>*>& vBuf, size_t first, size_t last)
{
  if (first == last) {
    return;
  }
  vBuf.first->emplace_back(first);
  vBuf.second->emplace_back(last - first);
}
inline void GPUDisplay::insertVertexList(int32_t iSlice, size_t first, size_t last)
{
  std::pair<vecpod<int32_t>*, vecpod<uint32_t>*> vBuf(mVertexBufferStart + iSlice, mVertexBufferCount + iSlice);
  insertVertexList(vBuf, first, last);
}

inline void GPUDisplay::drawPointLinestrip(int32_t iSlice, int32_t cid, int32_t id, int32_t id_limit)
{
  mVertexBuffer[iSlice].emplace_back(mGlobalPos[cid].x, mGlobalPos[cid].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPos[cid].z);
  float curVal;
  while ((curVal = mGlobalPos[cid].w) < id_limit) {
    if (GPUCommonMath::AtomicCAS(&mGlobalPos[cid].w, curVal, (float)id)) {
      break;
    }
    curVal = mGlobalPos[cid].w;
  }
}

GPUDisplay::vboList GPUDisplay::DrawSpacePointsTRD(int32_t iSlice, int32_t select, int32_t iCol)
{
  size_t startCount = mVertexBufferStart[iSlice].size();
  size_t startCountInner = mVertexBuffer[iSlice].size();

  if (iCol == 0) {
    for (uint32_t i = 0; i < mIOPtrs->nTRDTracklets; i++) {
      int32_t iSec = trdGeometry()->GetSector(mIOPtrs->trdTracklets[i].GetDetector());
      bool draw = iSlice == iSec && mGlobalPosTRD[i].w == select;
      if (draw) {
        mVertexBuffer[iSlice].emplace_back(mGlobalPosTRD[i].x, mGlobalPosTRD[i].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPosTRD[i].z);
        mVertexBuffer[iSlice].emplace_back(mGlobalPosTRD2[i].x, mGlobalPosTRD2[i].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPosTRD2[i].z);
      }
    }
  }

  insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

GPUDisplay::vboList GPUDisplay::DrawSpacePointsTOF(int32_t iSlice, int32_t select, int32_t iCol)
{
  size_t startCount = mVertexBufferStart[iSlice].size();
  size_t startCountInner = mVertexBuffer[iSlice].size();

  if (iCol == 0 && iSlice == 0) {
    for (uint32_t i = 0; i < mIOPtrs->nTOFClusters; i++) {
      mVertexBuffer[iSlice].emplace_back(mGlobalPosTOF[i].x, mGlobalPosTOF[i].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPosTOF[i].z);
    }
  }

  insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

GPUDisplay::vboList GPUDisplay::DrawSpacePointsITS(int32_t iSlice, int32_t select, int32_t iCol)
{
  size_t startCount = mVertexBufferStart[iSlice].size();
  size_t startCountInner = mVertexBuffer[iSlice].size();

  if (iCol == 0 && iSlice == 0 && mIOPtrs->itsClusters) {
    for (uint32_t i = 0; i < mIOPtrs->nItsClusters; i++) {
      mVertexBuffer[iSlice].emplace_back(mGlobalPosITS[i].x, mGlobalPosITS[i].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPosITS[i].z);
    }
  }

  insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

GPUDisplay::vboList GPUDisplay::DrawClusters(int32_t iSlice, int32_t select, uint32_t iCol)
{
  size_t startCount = mVertexBufferStart[iSlice].size();
  size_t startCountInner = mVertexBuffer[iSlice].size();
  if (mOverlayTFClusters.size() > 0 || iCol == 0 || mNCollissions) {
    const int32_t firstCluster = (mOverlayTFClusters.size() > 1 && iCol > 0) ? mOverlayTFClusters[iCol - 1][iSlice] : 0;
    const int32_t lastCluster = (mOverlayTFClusters.size() > 1 && iCol + 1 < mOverlayTFClusters.size()) ? mOverlayTFClusters[iCol][iSlice] : (mParam->par.earlyTpcTransform ? mIOPtrs->nClusterData[iSlice] : mIOPtrs->clustersNative ? mIOPtrs->clustersNative->nClustersSector[iSlice] : 0);
    const bool checkClusterCollision = mQA && mNCollissions && mOverlayTFClusters.size() == 0 && mIOPtrs->clustersNative && mIOPtrs->clustersNative->clustersMCTruth;
    for (int32_t cidInSlice = firstCluster; cidInSlice < lastCluster; cidInSlice++) {
      const int32_t cid = GET_CID(iSlice, cidInSlice);
#ifdef GPUCA_TPC_GEOMETRY_O2
      if (checkClusterCollision) {
        const auto& labels = mIOPtrs->clustersNative->clustersMCTruth->getLabels(cid);
        if (labels.size() ? (iCol != mQA->GetMCLabelCol(labels[0])) : (iCol != 0)) {
          continue;
        }
      }
#else
      (void)checkClusterCollision;
#endif
      if (mCfgH.hideUnmatchedClusters && mQA && mQA->SuppressHit(cid)) {
        continue;
      }
      bool draw = mGlobalPos[cid].w == select;

      if (mCfgH.markAdjacentClusters) {
        const int32_t attach = mIOPtrs->mergedTrackHitAttachment[cid];
        if (attach) {
          if (mCfgH.markAdjacentClusters >= 32) {
            if (mQA && mQA->clusterRemovable(attach, mCfgH.markAdjacentClusters == 33)) {
              draw = select == tMARKED;
            }
          } else if ((mCfgH.markAdjacentClusters & 2) && (attach & gputpcgmmergertypes::attachTube)) {
            draw = select == tMARKED;
          } else if ((mCfgH.markAdjacentClusters & 1) && (attach & (gputpcgmmergertypes::attachGood | gputpcgmmergertypes::attachTube)) == 0) {
            draw = select == tMARKED;
          } else if ((mCfgH.markAdjacentClusters & 4) && (attach & gputpcgmmergertypes::attachGoodLeg) == 0) {
            draw = select == tMARKED;
          } else if ((mCfgH.markAdjacentClusters & 16) && (attach & gputpcgmmergertypes::attachHighIncl)) {
            draw = select == tMARKED;
          } else if (mCfgH.markAdjacentClusters & 8) {
            if (fabsf(mIOPtrs->mergedTracks[attach & gputpcgmmergertypes::attachTrackMask].GetParam().GetQPt()) > 20.f) {
              draw = select == tMARKED;
            }
          }
        }
      } else if (mCfgH.markClusters) {
        int16_t flags;
        if (mParam->par.earlyTpcTransform) {
          flags = mIOPtrs->clusterData[iSlice][cidInSlice].flags;
        } else {
          flags = mIOPtrs->clustersNative->clustersLinear[cid].getFlags();
        }
        const bool match = flags & mCfgH.markClusters;
        draw = (select == tMARKED) ? (match) : (draw && !match);
      } else if (mCfgH.markFakeClusters) {
        const bool fake = (mQA->HitAttachStatus(cid));
        draw = (select == tMARKED) ? (fake) : (draw && !fake);
      }
      if (draw) {
        mVertexBuffer[iSlice].emplace_back(mGlobalPos[cid].x, mGlobalPos[cid].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPos[cid].z);
      }
    }
  }
  insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

GPUDisplay::vboList GPUDisplay::DrawLinks(const GPUTPCTracker& tracker, int32_t id, bool dodown)
{
  int32_t iSlice = tracker.ISlice();
  if (mCfgH.clustersOnly) {
    return (vboList(0, 0, iSlice));
  }
  size_t startCount = mVertexBufferStart[iSlice].size();
  size_t startCountInner = mVertexBuffer[iSlice].size();
  for (int32_t i = 0; i < GPUCA_ROW_COUNT; i++) {
    const GPUTPCRow& row = tracker.Data().Row(i);

    if (i < GPUCA_ROW_COUNT - 2) {
      const GPUTPCRow& rowUp = tracker.Data().Row(i + 2);
      for (int32_t j = 0; j < row.NHits(); j++) {
        if (tracker.Data().HitLinkUpData(row, j) != CALINK_INVAL) {
          const int32_t cid1 = GET_CID(iSlice, tracker.Data().ClusterDataIndex(row, j));
          const int32_t cid2 = GET_CID(iSlice, tracker.Data().ClusterDataIndex(rowUp, tracker.Data().HitLinkUpData(row, j)));
          drawPointLinestrip(iSlice, cid1, id);
          drawPointLinestrip(iSlice, cid2, id);
        }
      }
    }

    if (dodown && i >= 2) {
      const GPUTPCRow& rowDown = tracker.Data().Row(i - 2);
      for (int32_t j = 0; j < row.NHits(); j++) {
        if (tracker.Data().HitLinkDownData(row, j) != CALINK_INVAL) {
          const int32_t cid1 = GET_CID(iSlice, tracker.Data().ClusterDataIndex(row, j));
          const int32_t cid2 = GET_CID(iSlice, tracker.Data().ClusterDataIndex(rowDown, tracker.Data().HitLinkDownData(row, j)));
          drawPointLinestrip(iSlice, cid1, id);
          drawPointLinestrip(iSlice, cid2, id);
        }
      }
    }
  }
  insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

GPUDisplay::vboList GPUDisplay::DrawSeeds(const GPUTPCTracker& tracker)
{
  int32_t iSlice = tracker.ISlice();
  if (mCfgH.clustersOnly) {
    return (vboList(0, 0, iSlice));
  }
  size_t startCount = mVertexBufferStart[iSlice].size();
  for (uint32_t i = 0; i < *tracker.NStartHits(); i++) {
    const GPUTPCHitId& hit = tracker.TrackletStartHit(i);
    size_t startCountInner = mVertexBuffer[iSlice].size();
    int32_t ir = hit.RowIndex();
    calink ih = hit.HitIndex();
    do {
      const GPUTPCRow& row = tracker.Data().Row(ir);
      const int32_t cid = GET_CID(iSlice, tracker.Data().ClusterDataIndex(row, ih));
      drawPointLinestrip(iSlice, cid, tSEED);
      ir += 2;
      ih = tracker.Data().HitLinkUpData(row, ih);
    } while (ih != CALINK_INVAL);
    insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
  }
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

GPUDisplay::vboList GPUDisplay::DrawTracklets(const GPUTPCTracker& tracker)
{
  int32_t iSlice = tracker.ISlice();
  if (mCfgH.clustersOnly) {
    return (vboList(0, 0, iSlice));
  }
  size_t startCount = mVertexBufferStart[iSlice].size();
  for (uint32_t i = 0; i < *tracker.NTracklets(); i++) {
    const GPUTPCTracklet& tracklet = tracker.Tracklet(i);
    size_t startCountInner = mVertexBuffer[iSlice].size();
    float4 oldpos;
    for (int32_t j = tracklet.FirstRow(); j <= tracklet.LastRow(); j++) {
      const calink rowHit = tracker.TrackletRowHits()[tracklet.FirstHit() + (j - tracklet.FirstRow())];
      if (rowHit != CALINK_INVAL && rowHit != CALINK_DEAD_CHANNEL) {
        const GPUTPCRow& row = tracker.Data().Row(j);
        const int32_t cid = GET_CID(iSlice, tracker.Data().ClusterDataIndex(row, rowHit));
        oldpos = mGlobalPos[cid];
        drawPointLinestrip(iSlice, cid, tTRACKLET);
      }
    }
    insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
  }
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

GPUDisplay::vboList GPUDisplay::DrawTracks(const GPUTPCTracker& tracker, int32_t global)
{
  int32_t iSlice = tracker.ISlice();
  if (mCfgH.clustersOnly) {
    return (vboList(0, 0, iSlice));
  }
  size_t startCount = mVertexBufferStart[iSlice].size();
  for (uint32_t i = (global ? tracker.CommonMemory()->nLocalTracks : 0); i < (global ? *tracker.NTracks() : tracker.CommonMemory()->nLocalTracks); i++) {
    GPUTPCTrack& track = tracker.Tracks()[i];
    size_t startCountInner = mVertexBuffer[iSlice].size();
    for (int32_t j = 0; j < track.NHits(); j++) {
      const GPUTPCHitId& hit = tracker.TrackHits()[track.FirstHitID() + j];
      const GPUTPCRow& row = tracker.Data().Row(hit.RowIndex());
      const int32_t cid = GET_CID(iSlice, tracker.Data().ClusterDataIndex(row, hit.HitIndex()));
      drawPointLinestrip(iSlice, cid, tSLICETRACK + global);
    }
    insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
  }
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

void GPUDisplay::DrawTrackITS(int32_t trackId, int32_t iSlice)
{
#ifdef GPUCA_HAVE_O2HEADERS
  const auto& trk = mIOPtrs->itsTracks[trackId];
  for (int32_t k = 0; k < trk.getNClusters(); k++) {
    int32_t cid = mIOPtrs->itsTrackClusIdx[trk.getFirstClusterEntry() + k];
    mVertexBuffer[iSlice].emplace_back(mGlobalPosITS[cid].x, mGlobalPosITS[cid].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPosITS[cid].z);
    mGlobalPosITS[cid].w = tITSATTACHED;
  }
#endif
}

GPUDisplay::vboList GPUDisplay::DrawFinalITS()
{
  const int32_t iSlice = 0;
  size_t startCount = mVertexBufferStart[iSlice].size();
  for (uint32_t i = 0; i < mIOPtrs->nItsTracks; i++) {
    if (mITSStandaloneTracks[i]) {
      size_t startCountInner = mVertexBuffer[iSlice].size();
      DrawTrackITS(i, iSlice);
      insertVertexList(iSlice, startCountInner, mVertexBuffer[iSlice].size());
    }
  }
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

template <class T>
void GPUDisplay::DrawFinal(int32_t iSlice, int32_t /*iCol*/, GPUTPCGMPropagator* prop, std::array<vecpod<int32_t>, 2>& trackList, threadVertexBuffer& threadBuffer)
{
  auto& vBuf = threadBuffer.vBuf;
  auto& buffer = threadBuffer.buffer;
  uint32_t nTracks = std::max(trackList[0].size(), trackList[1].size());
  if (mCfgH.clustersOnly) {
    nTracks = 0;
  }
  for (uint32_t ii = 0; ii < nTracks; ii++) {
    int32_t i = 0;
    const T* track = nullptr;
    int32_t lastCluster = -1;
    while (true) {
      if (ii >= trackList[0].size()) {
        break;
      }
      i = trackList[0][ii];
      int32_t nClusters;
      if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
        track = &mIOPtrs->mergedTracks[i];
        nClusters = track->NClusters();
      } else if constexpr (std::is_same_v<T, o2::tpc::TrackTPC>) {
        track = &mIOPtrs->outputTracksTPCO2[i];
        nClusters = track->getNClusters();
        if (!mIOPtrs->clustersNative) {
          break;
        }
      } else {
        throw std::runtime_error("invalid type");
      }

      size_t startCountInner = mVertexBuffer[iSlice].size();
      bool drawing = false;

      if constexpr (std::is_same_v<T, o2::tpc::TrackTPC>) {
        if (!mCfgH.drawTracksAndFilter && !(mCfgH.drawTPCTracks || (mCfgH.drawITSTracks && mIOPtrs->tpcLinkITS && mIOPtrs->tpcLinkITS[i] != -1) || (mCfgH.drawTRDTracks && mIOPtrs->tpcLinkTRD && mIOPtrs->tpcLinkTRD[i] != -1) || (mCfgH.drawTOFTracks && mIOPtrs->tpcLinkTOF && mIOPtrs->tpcLinkTOF[i] != -1))) {
          break;
        }
        if (mCfgH.drawTracksAndFilter && ((mCfgH.drawITSTracks && !(mIOPtrs->tpcLinkITS && mIOPtrs->tpcLinkITS[i] != -1)) || (mCfgH.drawTRDTracks && !(mIOPtrs->tpcLinkTRD && mIOPtrs->tpcLinkTRD[i] != -1)) || (mCfgH.drawTOFTracks && !(mIOPtrs->tpcLinkTOF && mIOPtrs->tpcLinkTOF[i] != -1)))) {
          break;
        }
      }

      if (mCfgH.trackFilter && !mTrackFilter[i]) {
        break;
      }

      // Print TOF part of track
      if constexpr (std::is_same_v<T, o2::tpc::TrackTPC>) {
        if (mIOPtrs->tpcLinkTOF && mIOPtrs->tpcLinkTOF[i] != -1 && mIOPtrs->nTOFClusters) {
          int32_t cid = mIOPtrs->tpcLinkTOF[i];
          drawing = true;
          mVertexBuffer[iSlice].emplace_back(mGlobalPosTOF[cid].x, mGlobalPosTOF[cid].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPosTOF[cid].z);
          mGlobalPosTOF[cid].w = tTOFATTACHED;
        }
      }

      // Print TRD part of track
      auto tmpDoTRDTracklets = [&](const auto& trk) {
        for (int32_t k = 5; k >= 0; k--) {
          int32_t cid = trk.getTrackletIndex(k);
          if (cid < 0) {
            continue;
          }
          drawing = true;
          mVertexBuffer[iSlice].emplace_back(mGlobalPosTRD2[cid].x, mGlobalPosTRD2[cid].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPosTRD2[cid].z);
          mVertexBuffer[iSlice].emplace_back(mGlobalPosTRD[cid].x, mGlobalPosTRD[cid].y * mYFactor, mCfgH.projectXY ? 0 : mGlobalPosTRD[cid].z);
          mGlobalPosTRD[cid].w = tTRDATTACHED;
        }
      };
      if (std::is_same_v<T, GPUTPCGMMergedTrack> || (!mIOPtrs->tpcLinkTRD && mIOPtrs->trdTracksO2)) {
        if (mChain && ((int32_t)mConfig.showTPCTracksFromO2Format == (int32_t)mChain->GetProcessingSettings().trdTrackModelO2) && mTRDTrackIds[i] != -1 && mIOPtrs->nTRDTracklets) {
          if (mIOPtrs->trdTracksO2) {
#ifdef GPUCA_HAVE_O2HEADERS
            tmpDoTRDTracklets(mIOPtrs->trdTracksO2[mTRDTrackIds[i]]);
#endif
          } else {
            tmpDoTRDTracklets(mIOPtrs->trdTracks[mTRDTrackIds[i]]);
          }
        }
      } else if constexpr (std::is_same_v<T, o2::tpc::TrackTPC>) {
        if (mIOPtrs->tpcLinkTRD && mIOPtrs->tpcLinkTRD[i] != -1 && mIOPtrs->nTRDTracklets) {
          if ((mIOPtrs->tpcLinkTRD[i] & 0x40000000) ? mIOPtrs->nTRDTracksITSTPCTRD : mIOPtrs->nTRDTracksTPCTRD) {
            const auto* container = (mIOPtrs->tpcLinkTRD[i] & 0x40000000) ? mIOPtrs->trdTracksITSTPCTRD : mIOPtrs->trdTracksTPCTRD;
            const auto& trk = container[mIOPtrs->tpcLinkTRD[i] & 0x3FFFFFFF];
            tmpDoTRDTracklets(trk);
          }
        }
      }

      // Print TPC part of track
      int32_t separateGlobalTracksLimit = (mCfgH.separateGlobalTracks ? tGLOBALTRACK : TRACK_TYPE_ID_LIMIT);
      uint32_t lastSide = -1;
      for (int32_t k = 0; k < nClusters; k++) {
        if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
          if (mCfgH.hideRejectedClusters && (mIOPtrs->mergedTrackHits[track->FirstClusterRef() + k].state & GPUTPCGMMergedTrackHit::flagReject)) {
            continue;
          }
        }
        int32_t cid;
        if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
          cid = mIOPtrs->mergedTrackHits[track->FirstClusterRef() + k].num;
        } else {
          cid = &track->getCluster(mIOPtrs->outputClusRefsTPCO2, k, *mIOPtrs->clustersNative) - mIOPtrs->clustersNative->clustersLinear;
        }
        int32_t w = mGlobalPos[cid].w;
        if (drawing) {
          if (mCfgH.splitCETracks && lastSide != (mGlobalPos[cid].z < 0)) {
            insertVertexList(vBuf[0], startCountInner, mVertexBuffer[iSlice].size());
            drawing = false;
            lastCluster = -1;
          } else {
            drawPointLinestrip(iSlice, cid, tFINALTRACK, separateGlobalTracksLimit);
          }
        }
        if (w == separateGlobalTracksLimit) {
          if (drawing) {
            insertVertexList(vBuf[0], startCountInner, mVertexBuffer[iSlice].size());
          }
          drawing = false;
        } else {
          if (!drawing) {
            startCountInner = mVertexBuffer[iSlice].size();
            if (lastCluster != -1 && (!mCfgH.splitCETracks || lastSide == (mGlobalPos[cid].z < 0))) {
              int32_t lastcid;
              if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
                lastcid = mIOPtrs->mergedTrackHits[track->FirstClusterRef() + lastCluster].num;
              } else {
                lastcid = &track->getCluster(mIOPtrs->outputClusRefsTPCO2, lastCluster, *mIOPtrs->clustersNative) - mIOPtrs->clustersNative->clustersLinear;
              }
              drawPointLinestrip(iSlice, lastcid, tFINALTRACK, separateGlobalTracksLimit);
            }
            drawPointLinestrip(iSlice, cid, tFINALTRACK, separateGlobalTracksLimit);
          }
          drawing = true;
        }
        lastCluster = k;
        lastSide = mGlobalPos[cid].z < 0;
      }

      // Print ITS part of track
      if constexpr (std::is_same_v<T, o2::tpc::TrackTPC>) {
        if (mIOPtrs->tpcLinkITS && mIOPtrs->tpcLinkITS[i] != -1 && mIOPtrs->nItsTracks && mIOPtrs->nItsClusters) {
          DrawTrackITS(mIOPtrs->tpcLinkITS[i], iSlice);
        }
      }
      insertVertexList(vBuf[0], startCountInner, mVertexBuffer[iSlice].size());
      break;
    }

    if (!mIOPtrs->clustersNative) {
      continue;
    }

    // Propagate track paramters / plot MC tracks
    for (int32_t iMC = 0; iMC < 2; iMC++) {
      if (iMC) {
        if (ii >= trackList[1].size()) {
          continue;
        }
        i = trackList[1][ii];
      } else {
        if (track == nullptr) {
          continue;
        }
        if (lastCluster == -1) {
          continue;
        }
      }

      size_t startCountInner = mVertexBuffer[iSlice].size();
      for (int32_t inFlyDirection = 0; inFlyDirection < 2; inFlyDirection++) {
        GPUTPCGMPhysicalTrackModel trkParam;
        float ZOffset = 0;
        float x = 0;
        float alphaOrg = 0;
        if (iMC == 0) {
          if (!inFlyDirection && mIOPtrs->tpcLinkITS && mIOPtrs->tpcLinkITS[i] != -1) {
            continue;
          }
          if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
            trkParam.Set(track->GetParam());
            alphaOrg = mParam->Alpha(iSlice);
          } else {
            GPUTPCGMTrackParam t;
            convertTrackParam(t, *track);
            alphaOrg = track->getAlpha();
            trkParam.Set(t);
          }

          if (mParam->par.earlyTpcTransform) {
            if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
              x = mIOPtrs->mergedTrackHitsXYZ[track->FirstClusterRef() + lastCluster].x;
              ZOffset = track->GetParam().GetTZOffset();
            }
          } else {
            float y, z;
            if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
              auto cl = mIOPtrs->mergedTrackHits[track->FirstClusterRef() + lastCluster];
              const auto& cln = mIOPtrs->clustersNative->clustersLinear[cl.num];
              GPUTPCConvertImpl::convert(*mCalib->fastTransform, *mParam, cl.slice, cl.row, cln.getPad(), cln.getTime(), x, y, z);
              ZOffset = mCalib->fastTransformHelper->getCorrMap()->convVertexTimeToZOffset(iSlice, track->GetParam().GetTZOffset(), mParam->continuousMaxTimeBin);
            } else {
              uint8_t sector, row;
              auto cln = track->getCluster(mIOPtrs->outputClusRefsTPCO2, lastCluster, *mIOPtrs->clustersNative, sector, row);
              GPUTPCConvertImpl::convert(*mCalib->fastTransform, *mParam, sector, row, cln.getPad(), cln.getTime(), x, y, z);
              ZOffset = mCalib->fastTransformHelper->getCorrMap()->convVertexTimeToZOffset(sector, track->getTime0(), mParam->continuousMaxTimeBin);
            }
          }
        } else {
          const GPUTPCMCInfo& mc = mIOPtrs->mcInfosTPC[i];
          if (mc.charge == 0.f) {
            break;
          }
          if (mc.pid < 0) {
            break;
          }

          alphaOrg = mParam->Alpha(iSlice);
          float c = cosf(alphaOrg);
          float s = sinf(alphaOrg);
          float mclocal[4];
          x = mc.x;
          float y = mc.y;
          mclocal[0] = x * c + y * s;
          mclocal[1] = -x * s + y * c;
          float px = mc.pX;
          float py = mc.pY;
          mclocal[2] = px * c + py * s;
          mclocal[3] = -px * s + py * c;
          float charge = mc.charge > 0 ? 1.f : -1.f;

          x = mclocal[0];
#ifdef GPUCA_TPC_GEOMETRY_O2
          trkParam.Set(mclocal[0], mclocal[1], mc.z, mclocal[2], mclocal[3], mc.pZ, -charge); // TODO: DR: unclear to me why we need -charge here
          if (mParam->par.continuousTracking) {
            ZOffset = fabsf(mCalib->fastTransformHelper->getCorrMap()->convVertexTimeToZOffset(0, mc.t0, mParam->continuousMaxTimeBin)) * (mc.z < 0 ? -1 : 1);
          }
#else
          if (fabsf(mc.z) > GPUTPCGeometry::TPCLength()) {
            ZOffset = mc.z > 0 ? (mc.z - GPUTPCGeometry::TPCLength()) : (mc.z + GPUTPCGeometry::TPCLength());
          }
          trkParam.Set(mclocal[0], mclocal[1], mc.z - ZOffset, mclocal[2], mclocal[3], mc.pZ, charge);
#endif
        }
        float z0 = trkParam.Z();
        if (iMC && inFlyDirection == 0) {
          buffer.clear();
        }
        if (x < 1) {
          break;
        }
        if (fabsf(trkParam.SinPhi()) > 1) {
          break;
        }
        float alpha = alphaOrg;
        vecpod<vtx>& useBuffer = iMC && inFlyDirection == 0 ? buffer : mVertexBuffer[iSlice];
        int32_t nPoints = 0;

        while (nPoints++ < 5000) {
          if ((inFlyDirection == 0 && x < 0) || (inFlyDirection && x * x + trkParam.Y() * trkParam.Y() > (iMC ? (450 * 450) : (300 * 300)))) {
            break;
          }
          if (fabsf(trkParam.Z() + ZOffset) > mMaxClusterZ + (iMC ? 0 : 0)) {
            break;
          }
          if (fabsf(trkParam.Z() - z0) > (iMC ? GPUTPCGeometry::TPCLength() : GPUTPCGeometry::TPCLength())) {
            break;
          }
          if (inFlyDirection) {
            if (fabsf(trkParam.SinPhi()) > 0.4f) {
              float dalpha = asinf(trkParam.SinPhi());
              trkParam.Rotate(dalpha);
              alpha += dalpha;
            }
            x = trkParam.X() + 1.f;
            if (!mCfgH.propagateLoopers) {
              float diff = fabsf(alpha - alphaOrg) / (2.f * CAMath::Pi());
              diff -= floor(diff);
              if (diff > 0.25f && diff < 0.75f) {
                break;
              }
            }
          }
          float B[3];
          prop->GetBxByBz(alpha, trkParam.GetX(), trkParam.GetY(), trkParam.GetZ(), B);
          float dLp = 0;
          if (trkParam.PropagateToXBxByBz(x, B[0], B[1], B[2], dLp)) {
            break;
          }
          if (fabsf(trkParam.SinPhi()) > 0.9f) {
            break;
          }
          float sa = sinf(alpha), ca = cosf(alpha);
          float drawX = trkParam.X() + mCfgH.xAdd;
          useBuffer.emplace_back((ca * drawX - sa * trkParam.Y()) * GL_SCALE_FACTOR, (ca * trkParam.Y() + sa * drawX) * mYFactor * GL_SCALE_FACTOR, mCfgH.projectXY ? 0 : (trkParam.Z() + ZOffset) * GL_SCALE_FACTOR);
          x += inFlyDirection ? 1 : -1;
        }

        if (inFlyDirection == 0) {
          if (iMC) {
            for (int32_t k = (int32_t)buffer.size() - 1; k >= 0; k--) {
              mVertexBuffer[iSlice].emplace_back(buffer[k]);
            }
          } else {
            insertVertexList(vBuf[1], startCountInner, mVertexBuffer[iSlice].size());
            startCountInner = mVertexBuffer[iSlice].size();
          }
        }
      }
      insertVertexList(vBuf[iMC ? 3 : 2], startCountInner, mVertexBuffer[iSlice].size());
    }
  }
}

GPUDisplay::vboList GPUDisplay::DrawGrid(const GPUTPCTracker& tracker)
{
  int32_t iSlice = tracker.ISlice();
  size_t startCount = mVertexBufferStart[iSlice].size();
  size_t startCountInner = mVertexBuffer[iSlice].size();
  for (int32_t i = 0; i < GPUCA_ROW_COUNT; i++) {
    const GPUTPCRow& row = tracker.Data().Row(i);
    for (int32_t j = 0; j <= (signed)row.Grid().Ny(); j++) {
      float z1 = row.Grid().ZMin();
      float z2 = row.Grid().ZMax();
      float x = row.X() + mCfgH.xAdd;
      float y = row.Grid().YMin() + (float)j / row.Grid().StepYInv();
      float zz1, zz2, yy1, yy2, xx1, xx2;
      mParam->Slice2Global(tracker.ISlice(), x, y, z1, &xx1, &yy1, &zz1);
      mParam->Slice2Global(tracker.ISlice(), x, y, z2, &xx2, &yy2, &zz2);
      if (iSlice < 18) {
        zz1 += mCfgH.zAdd;
        zz2 += mCfgH.zAdd;
      } else {
        zz1 -= mCfgH.zAdd;
        zz2 -= mCfgH.zAdd;
      }
      mVertexBuffer[iSlice].emplace_back(xx1 * GL_SCALE_FACTOR, yy1 * GL_SCALE_FACTOR * mYFactor, zz1 * GL_SCALE_FACTOR);
      mVertexBuffer[iSlice].emplace_back(xx2 * GL_SCALE_FACTOR, yy2 * GL_SCALE_FACTOR * mYFactor, zz2 * GL_SCALE_FACTOR);
    }
    for (int32_t j = 0; j <= (signed)row.Grid().Nz(); j++) {
      float y1 = row.Grid().YMin();
      float y2 = row.Grid().YMax();
      float x = row.X() + mCfgH.xAdd;
      float z = row.Grid().ZMin() + (float)j / row.Grid().StepZInv();
      float zz1, zz2, yy1, yy2, xx1, xx2;
      mParam->Slice2Global(tracker.ISlice(), x, y1, z, &xx1, &yy1, &zz1);
      mParam->Slice2Global(tracker.ISlice(), x, y2, z, &xx2, &yy2, &zz2);
      if (iSlice < 18) {
        zz1 += mCfgH.zAdd;
        zz2 += mCfgH.zAdd;
      } else {
        zz1 -= mCfgH.zAdd;
        zz2 -= mCfgH.zAdd;
      }
      mVertexBuffer[iSlice].emplace_back(xx1 * GL_SCALE_FACTOR, yy1 * GL_SCALE_FACTOR * mYFactor, zz1 * GL_SCALE_FACTOR);
      mVertexBuffer[iSlice].emplace_back(xx2 * GL_SCALE_FACTOR, yy2 * GL_SCALE_FACTOR * mYFactor, zz2 * GL_SCALE_FACTOR);
    }
  }
  insertVertexList(tracker.ISlice(), startCountInner, mVertexBuffer[iSlice].size());
  return (vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice));
}

GPUDisplay::vboList GPUDisplay::DrawGridTRD(int32_t sector)
{
  // TODO: tilted pads ignored at the moment
  size_t startCount = mVertexBufferStart[sector].size();
  size_t startCountInner = mVertexBuffer[sector].size();
#ifdef GPUCA_HAVE_O2HEADERS
  auto* geo = trdGeometry();
  if (geo) {
    int32_t trdsector = NSLICES / 2 - 1 - sector;
    float alpha = geo->GetAlpha() / 2.f + geo->GetAlpha() * trdsector;
    if (trdsector >= 9) {
      alpha -= 2 * CAMath::Pi();
    }
    for (int32_t iLy = 0; iLy < GPUTRDTracker::EGPUTRDTracker::kNLayers; ++iLy) {
      for (int32_t iStack = 0; iStack < GPUTRDTracker::EGPUTRDTracker::kNStacks; ++iStack) {
        int32_t iDet = geo->GetDetector(iLy, iStack, trdsector);
        auto matrix = geo->GetClusterMatrix(iDet);
        if (!matrix) {
          continue;
        }
        auto pp = geo->GetPadPlane(iDet);
        for (int32_t i = 0; i < pp->GetNrows(); ++i) {
          float xyzLoc1[3];
          float xyzLoc2[3];
          float xyzGlb1[3];
          float xyzGlb2[3];
          xyzLoc1[0] = xyzLoc2[0] = geo->AnodePos();
          xyzLoc1[1] = pp->GetCol0();
          xyzLoc2[1] = pp->GetColEnd();
          xyzLoc1[2] = xyzLoc2[2] = pp->GetRowPos(i) - pp->GetRowPos(pp->GetNrows() / 2);
          matrix->LocalToMaster(xyzLoc1, xyzGlb1);
          matrix->LocalToMaster(xyzLoc2, xyzGlb2);
          float x1Tmp = xyzGlb1[0];
          xyzGlb1[0] = xyzGlb1[0] * cosf(alpha) + xyzGlb1[1] * sinf(alpha);
          xyzGlb1[1] = -x1Tmp * sinf(alpha) + xyzGlb1[1] * cosf(alpha);
          float x2Tmp = xyzGlb2[0];
          xyzGlb2[0] = xyzGlb2[0] * cosf(alpha) + xyzGlb2[1] * sinf(alpha);
          xyzGlb2[1] = -x2Tmp * sinf(alpha) + xyzGlb2[1] * cosf(alpha);
          mVertexBuffer[sector].emplace_back(xyzGlb1[0] * GL_SCALE_FACTOR, xyzGlb1[1] * GL_SCALE_FACTOR * mYFactor, xyzGlb1[2] * GL_SCALE_FACTOR);
          mVertexBuffer[sector].emplace_back(xyzGlb2[0] * GL_SCALE_FACTOR, xyzGlb2[1] * GL_SCALE_FACTOR * mYFactor, xyzGlb2[2] * GL_SCALE_FACTOR);
        }
        for (int32_t j = 0; j < pp->GetNcols(); ++j) {
          float xyzLoc1[3];
          float xyzLoc2[3];
          float xyzGlb1[3];
          float xyzGlb2[3];
          xyzLoc1[0] = xyzLoc2[0] = geo->AnodePos();
          xyzLoc1[1] = xyzLoc2[1] = pp->GetColPos(j) + pp->GetColSize(j) / 2.f;
          xyzLoc1[2] = pp->GetRow0() - pp->GetRowPos(pp->GetNrows() / 2);
          xyzLoc2[2] = pp->GetRowEnd() - pp->GetRowPos(pp->GetNrows() / 2);
          matrix->LocalToMaster(xyzLoc1, xyzGlb1);
          matrix->LocalToMaster(xyzLoc2, xyzGlb2);
          float x1Tmp = xyzGlb1[0];
          xyzGlb1[0] = xyzGlb1[0] * cosf(alpha) + xyzGlb1[1] * sinf(alpha);
          xyzGlb1[1] = -x1Tmp * sinf(alpha) + xyzGlb1[1] * cosf(alpha);
          float x2Tmp = xyzGlb2[0];
          xyzGlb2[0] = xyzGlb2[0] * cosf(alpha) + xyzGlb2[1] * sinf(alpha);
          xyzGlb2[1] = -x2Tmp * sinf(alpha) + xyzGlb2[1] * cosf(alpha);
          mVertexBuffer[sector].emplace_back(xyzGlb1[0] * GL_SCALE_FACTOR, xyzGlb1[1] * GL_SCALE_FACTOR * mYFactor, xyzGlb1[2] * GL_SCALE_FACTOR);
          mVertexBuffer[sector].emplace_back(xyzGlb2[0] * GL_SCALE_FACTOR, xyzGlb2[1] * GL_SCALE_FACTOR * mYFactor, xyzGlb2[2] * GL_SCALE_FACTOR);
        }
      }
    }
  }
#endif
  insertVertexList(sector, startCountInner, mVertexBuffer[sector].size());
  return (vboList(startCount, mVertexBufferStart[sector].size() - startCount, sector));
}

size_t GPUDisplay::DrawGLScene_updateVertexList()
{
  for (int32_t i = 0; i < NSLICES; i++) {
    mVertexBuffer[i].clear();
    mVertexBufferStart[i].clear();
    mVertexBufferCount[i].clear();
  }

  for (int32_t i = 0; i < mCurrentClusters; i++) {
    mGlobalPos[i].w = tCLUSTER;
  }
  for (int32_t i = 0; i < mCurrentSpacePointsTRD; i++) {
    mGlobalPosTRD[i].w = tTRDCLUSTER;
  }

  for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
    for (int32_t i = 0; i < N_POINTS_TYPE; i++) {
      mGlDLPoints[iSlice][i].resize(mNCollissions);
    }
    for (int32_t i = 0; i < N_FINAL_TYPE; i++) {
      mGlDLFinal[iSlice].resize(mNCollissions);
    }
  }
  GPUCA_OPENMP(parallel num_threads(getNumThreads()))
  {
#ifdef WITH_OPENMP
    int32_t numThread = omp_get_thread_num();
    int32_t numThreads = omp_get_num_threads();
#else
    int32_t numThread = 0, numThreads = 1;
#endif
    if (mChain && (mChain->GetRecoSteps() & GPUDataTypes::RecoStep::TPCSliceTracking)) {
      GPUCA_OPENMP(for)
      for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
        GPUTPCTracker& tracker = (GPUTPCTracker&)sliceTracker(iSlice);
        tracker.SetPointersDataLinks(tracker.LinkTmpMemory());
        mGlDLLines[iSlice][tINITLINK] = DrawLinks(tracker, tINITLINK, true);
        tracker.SetPointersDataLinks(mChain->rec()->Res(tracker.MemoryResLinks()).Ptr());
      }
      GPUCA_OPENMP(barrier)

      GPUCA_OPENMP(for)
      for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
        const GPUTPCTracker& tracker = sliceTracker(iSlice);

        mGlDLLines[iSlice][tLINK] = DrawLinks(tracker, tLINK);
        mGlDLLines[iSlice][tSEED] = DrawSeeds(tracker);
        mGlDLLines[iSlice][tTRACKLET] = DrawTracklets(tracker);
        mGlDLLines[iSlice][tSLICETRACK] = DrawTracks(tracker, 0);
        mGlDLGrid[iSlice] = DrawGrid(tracker);
        if (iSlice < NSLICES / 2) {
          mGlDLGridTRD[iSlice] = DrawGridTRD(iSlice);
        }
      }
      GPUCA_OPENMP(barrier)

      GPUCA_OPENMP(for)
      for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
        const GPUTPCTracker& tracker = sliceTracker(iSlice);
        mGlDLLines[iSlice][tGLOBALTRACK] = DrawTracks(tracker, 1);
      }
      GPUCA_OPENMP(barrier)
    }
    mThreadTracks[numThread].resize(mNCollissions);
    for (int32_t i = 0; i < mNCollissions; i++) {
      for (int32_t j = 0; j < NSLICES; j++) {
        for (int32_t k = 0; k < 2; k++) {
          mThreadTracks[numThread][i][j][k].clear();
        }
      }
    }
    if (mConfig.showTPCTracksFromO2Format) {
#ifdef GPUCA_TPC_GEOMETRY_O2
      uint32_t col = 0;
      GPUCA_OPENMP(for)
      for (uint32_t i = 0; i < mIOPtrs->nOutputTracksTPCO2; i++) {
        uint8_t sector, row;
        if (mIOPtrs->clustersNative) {
          mIOPtrs->outputTracksTPCO2[i].getCluster(mIOPtrs->outputClusRefsTPCO2, 0, *mIOPtrs->clustersNative, sector, row);
        } else {
          sector = 0;
        }
        if (mQA && mIOPtrs->outputTracksTPCO2MC) {
          col = mQA->GetMCLabelCol(mIOPtrs->outputTracksTPCO2MC[i]);
        }
        mThreadTracks[numThread][col][sector][0].emplace_back(i);
      }
#endif
    } else {
      GPUCA_OPENMP(for)
      for (uint32_t i = 0; i < mIOPtrs->nMergedTracks; i++) {
        const GPUTPCGMMergedTrack* track = &mIOPtrs->mergedTracks[i];
        if (track->NClusters() == 0) {
          continue;
        }
        if (mCfgH.hideRejectedTracks && !track->OK()) {
          continue;
        }
        int32_t slice = mIOPtrs->mergedTrackHits[track->FirstClusterRef() + track->NClusters() - 1].slice;
        uint32_t col = 0;
        if (mQA) {
          const auto& label = mQA->GetMCTrackLabel(i);
#ifdef GPUCA_TPC_GEOMETRY_O2
          col = mQA->GetMCLabelCol(label);
#else
          while (label.isValid() && col < mOverlayTFClusters.size() && mOverlayTFClusters[col][NSLICES] < label.track) {
            col++;
          }
#endif
        }
        mThreadTracks[numThread][col][slice][0].emplace_back(i);
      }
    }
    for (uint32_t col = 0; col < mIOPtrs->nMCInfosTPCCol; col++) {
      GPUCA_OPENMP(for)
      for (uint32_t i = mIOPtrs->mcInfosTPCCol[col].first; i < mIOPtrs->mcInfosTPCCol[col].first + mIOPtrs->mcInfosTPCCol[col].num; i++) {
        const GPUTPCMCInfo& mc = mIOPtrs->mcInfosTPC[i];
        if (mc.charge == 0.f) {
          continue;
        }
        if (mc.pid < 0) {
          continue;
        }

        float alpha = atan2f(mc.y, mc.x);
        if (alpha < 0) {
          alpha += 2 * CAMath::Pi();
        }
        int32_t slice = alpha / (2 * CAMath::Pi()) * 18;
        if (mc.z < 0) {
          slice += 18;
        }
        mThreadTracks[numThread][col][slice][1].emplace_back(i);
      }
    }
    GPUCA_OPENMP(barrier)

    GPUTPCGMPropagator prop;
    prop.SetMaxSinPhi(.999);
    prop.SetMaterialTPC();
    prop.SetPolynomialField(&mParam->polynomialField);

    GPUCA_OPENMP(for)
    for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
      for (int32_t iCol = 0; iCol < mNCollissions; iCol++) {
        mThreadBuffers[numThread].clear();
        for (int32_t iSet = 0; iSet < numThreads; iSet++) {
#ifdef GPUCA_HAVE_O2HEADERS
          if (mConfig.showTPCTracksFromO2Format) {
            DrawFinal<o2::tpc::TrackTPC>(iSlice, iCol, &prop, mThreadTracks[iSet][iCol][iSlice], mThreadBuffers[numThread]);
          } else
#endif
          {
            DrawFinal<GPUTPCGMMergedTrack>(iSlice, iCol, &prop, mThreadTracks[iSet][iCol][iSlice], mThreadBuffers[numThread]);
          }
        }
        vboList* list = &mGlDLFinal[iSlice][iCol][0];
        for (int32_t i = 0; i < N_FINAL_TYPE; i++) {
          size_t startCount = mVertexBufferStart[iSlice].size();
          for (uint32_t j = 0; j < mThreadBuffers[numThread].start[i].size(); j++) {
            mVertexBufferStart[iSlice].emplace_back(mThreadBuffers[numThread].start[i][j]);
            mVertexBufferCount[iSlice].emplace_back(mThreadBuffers[numThread].count[i][j]);
          }
          list[i] = vboList(startCount, mVertexBufferStart[iSlice].size() - startCount, iSlice);
        }
      }
    }

    GPUCA_OPENMP(barrier)
    GPUCA_OPENMP(for)
    for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
      for (int32_t i = 0; i < N_POINTS_TYPE_TPC; i++) {
        for (int32_t iCol = 0; iCol < mNCollissions; iCol++) {
          mGlDLPoints[iSlice][i][iCol] = DrawClusters(iSlice, i, iCol);
        }
      }
    }
  }
  // End omp parallel

  mGlDLFinalITS = DrawFinalITS();

  for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
    for (int32_t i = N_POINTS_TYPE_TPC; i < N_POINTS_TYPE_TPC + N_POINTS_TYPE_TRD; i++) {
      for (int32_t iCol = 0; iCol < mNCollissions; iCol++) {
        mGlDLPoints[iSlice][i][iCol] = DrawSpacePointsTRD(iSlice, i, iCol);
      }
    }
  }

  for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
    for (int32_t i = N_POINTS_TYPE_TPC + N_POINTS_TYPE_TRD; i < N_POINTS_TYPE_TPC + N_POINTS_TYPE_TRD + N_POINTS_TYPE_TOF; i++) {
      for (int32_t iCol = 0; iCol < mNCollissions; iCol++) {
        mGlDLPoints[iSlice][i][iCol] = DrawSpacePointsTOF(iSlice, i, iCol);
      }
    }
    break; // TODO: Only slice 0 filled for now
  }

  for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
    for (int32_t i = N_POINTS_TYPE_TPC + N_POINTS_TYPE_TRD + N_POINTS_TYPE_TOF; i < N_POINTS_TYPE_TPC + N_POINTS_TYPE_TRD + N_POINTS_TYPE_TOF + N_POINTS_TYPE_ITS; i++) {
      for (int32_t iCol = 0; iCol < mNCollissions; iCol++) {
        mGlDLPoints[iSlice][i][iCol] = DrawSpacePointsITS(iSlice, i, iCol);
      }
    }
    break; // TODO: Only slice 0 filled for now
  }

  mUpdateVertexLists = 0;
  size_t totalVertizes = 0;
  for (int32_t i = 0; i < NSLICES; i++) {
    totalVertizes += mVertexBuffer[i].size();
  }
  if (totalVertizes > 0xFFFFFFFF) {
    throw std::runtime_error("Display vertex count exceeds 32bit uint32_t counter");
  }
  size_t needMultiVBOSize = mBackend->needMultiVBO();
  mUseMultiVBO = needMultiVBOSize && (totalVertizes * sizeof(mVertexBuffer[0][0]) >= needMultiVBOSize);
  if (!mUseMultiVBO) {
    size_t totalYet = mVertexBuffer[0].size();
    mVertexBuffer[0].resize(totalVertizes);
    for (int32_t i = 1; i < GPUCA_NSLICES; i++) {
      for (uint32_t j = 0; j < mVertexBufferStart[i].size(); j++) {
        mVertexBufferStart[i][j] += totalYet;
      }
      memcpy(&mVertexBuffer[0][totalYet], &mVertexBuffer[i][0], mVertexBuffer[i].size() * sizeof(mVertexBuffer[i][0]));
      totalYet += mVertexBuffer[i].size();
      mVertexBuffer[i].clear();
    }
  }
  mBackend->loadDataToGPU(totalVertizes);
  for (int32_t i = 0; i < (mUseMultiVBO ? GPUCA_NSLICES : 1); i++) {
    mVertexBuffer[i].clear();
  }
  return totalVertizes;
}
