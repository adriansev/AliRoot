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

/// \file GPUDisplay.cxx
/// \author David Rohr

#ifndef GPUCA_NO_ROOT
#include "Rtypes.h" // Include ROOT header first, to use ROOT and disable replacements
#endif

#include "GPUDisplay.h"
#include "GPUO2DataTypes.h"
#include "GPUTPCClusterData.h"
#include "GPUTPCConvertImpl.h"
#include "GPUTRDGeometry.h"
#include "GPUTRDTrackletWord.h"
#include "GPUParam.inc"

#ifdef GPUCA_HAVE_O2HEADERS
#include "DataFormatsTOF/Cluster.h"
#include "DataFormatsITSMFT/ROFRecord.h"
#include "DataFormatsTPC/TrackTPC.h"
#include "TOFBase/Geo.h"
#include "ITSBase/GeometryTGeo.h"
#endif
#ifdef GPUCA_O2_LIB
#include "ITSMFTBase/DPLAlpideParam.h"
#endif

using namespace GPUCA_NAMESPACE::gpu;

void GPUDisplay::DrawGLScene_updateEventData()
{
  mTimerDraw.ResetStart();
  if (mIOPtrs->clustersNative) {
    mCurrentClusters = mIOPtrs->clustersNative->nClustersTotal;
  } else {
    mCurrentClusters = 0;
    for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
      mCurrentClusters += mIOPtrs->nClusterData[iSlice];
    }
  }
  if (mNMaxClusters < mCurrentClusters) {
    mNMaxClusters = mCurrentClusters;
    mGlobalPosPtr.reset(new float4[mNMaxClusters]);
    mGlobalPos = mGlobalPosPtr.get();
  }

  mCurrentSpacePointsTRD = mIOPtrs->nTRDTracklets;
  if (mCurrentSpacePointsTRD > mNMaxSpacePointsTRD) {
    mNMaxSpacePointsTRD = mCurrentSpacePointsTRD;
    mGlobalPosPtrTRD.reset(new float4[mNMaxSpacePointsTRD]);
    mGlobalPosPtrTRD2.reset(new float4[mNMaxSpacePointsTRD]);
    mGlobalPosTRD = mGlobalPosPtrTRD.get();
    mGlobalPosTRD2 = mGlobalPosPtrTRD2.get();
  }

  mCurrentClustersITS = mIOPtrs->itsClusters ? mIOPtrs->nItsClusters : 0;
  if (mNMaxClustersITS < mCurrentClustersITS) {
    mNMaxClustersITS = mCurrentClustersITS;
    mGlobalPosPtrITS.reset(new float4[mNMaxClustersITS]);
    mGlobalPosITS = mGlobalPosPtrITS.get();
  }

  mCurrentClustersTOF = mIOPtrs->nTOFClusters;
  if (mNMaxClustersTOF < mCurrentClustersTOF) {
    mNMaxClustersTOF = mCurrentClustersTOF;
    mGlobalPosPtrTOF.reset(new float4[mNMaxClustersTOF]);
    mGlobalPosTOF = mGlobalPosPtrTOF.get();
  }

  uint32_t nTpcMergedTracks = mConfig.showTPCTracksFromO2Format ? mIOPtrs->nOutputTracksTPCO2 : mIOPtrs->nMergedTracks;
  if ((size_t)nTpcMergedTracks > mTRDTrackIds.size()) {
    mTRDTrackIds.resize(nTpcMergedTracks);
  }
  if (mIOPtrs->nItsTracks > mITSStandaloneTracks.size()) {
    mITSStandaloneTracks.resize(mIOPtrs->nItsTracks);
  }
  for (uint32_t i = 0; i < nTpcMergedTracks; i++) {
    mTRDTrackIds[i] = -1;
  }
  auto tmpDoTRDTracklets = [&](auto* trdTracks) {
    for (uint32_t i = 0; i < mIOPtrs->nTRDTracks; i++) {
      if (trdTracks[i].getNtracklets()) {
        mTRDTrackIds[trdTracks[i].getRefGlobalTrackIdRaw()] = i;
      }
    }
  };
  if (mIOPtrs->trdTracksO2) {
#ifdef GPUCA_HAVE_O2HEADERS
    tmpDoTRDTracklets(mIOPtrs->trdTracksO2);
#endif
  } else {
    tmpDoTRDTracklets(mIOPtrs->trdTracks);
  }
  if (mIOPtrs->nItsTracks) {
    std::fill(mITSStandaloneTracks.begin(), mITSStandaloneTracks.end(), true);
    if (mIOPtrs->tpcLinkITS) {
      for (uint32_t i = 0; i < nTpcMergedTracks; i++) {
        if (mIOPtrs->tpcLinkITS[i] != -1) {
          mITSStandaloneTracks[mIOPtrs->tpcLinkITS[i]] = false;
        }
      }
    }
  }

  if (mCfgH.trackFilter) {
    uint32_t nTracks = mConfig.showTPCTracksFromO2Format ? mIOPtrs->nOutputTracksTPCO2 : mIOPtrs->nMergedTracks;
    mTrackFilter.resize(nTracks);
    std::fill(mTrackFilter.begin(), mTrackFilter.end(), true);
    if (buildTrackFilter()) {
      SetInfo("Error running track filter from %s", mConfig.filterMacros[mCfgH.trackFilter - 1].c_str());
    } else {
      uint32_t nFiltered = 0;
      for (uint32_t i = 0; i < mTrackFilter.size(); i++) {
        nFiltered += !mTrackFilter[i];
      }
      if (mUpdateTrackFilter) {
        SetInfo("Applied track filter %s - filtered %u / %u", mConfig.filterMacros[mCfgH.trackFilter - 1].c_str(), nFiltered, (uint32_t)mTrackFilter.size());
      }
    }
  }
  mUpdateTrackFilter = false;

  mMaxClusterZ = 0;
  GPUCA_OPENMP(parallel for num_threads(getNumThreads()) reduction(max : mMaxClusterZ))
  for (int32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
    int32_t row = 0;
    uint32_t nCls = mParam->par.earlyTpcTransform ? mIOPtrs->nClusterData[iSlice] : mIOPtrs->clustersNative ? mIOPtrs->clustersNative->nClustersSector[iSlice]
                                                                                                            : 0;
    for (uint32_t i = 0; i < nCls; i++) {
      int32_t cid;
      if (mParam->par.earlyTpcTransform) {
        const auto& cl = mIOPtrs->clusterData[iSlice][i];
        cid = cl.id;
        row = cl.row;
      } else {
        cid = mIOPtrs->clustersNative->clusterOffset[iSlice][0] + i;
        while (row < GPUCA_ROW_COUNT - 1 && mIOPtrs->clustersNative->clusterOffset[iSlice][row + 1] <= (uint32_t)cid) {
          row++;
        }
      }
      if (cid >= mNMaxClusters) {
        throw std::runtime_error("Cluster Buffer Size exceeded");
      }
      float4* ptr = &mGlobalPos[cid];
      if (mParam->par.earlyTpcTransform) {
        const auto& cl = mIOPtrs->clusterData[iSlice][i];
        mParam->Slice2Global(iSlice, (mCfgH.clustersOnNominalRow ? mParam->tpcGeometry.Row2X(row) : cl.x) + mCfgH.xAdd, cl.y, cl.z, &ptr->x, &ptr->y, &ptr->z);
      } else {
        float x, y, z;
        const auto& cln = mIOPtrs->clustersNative->clusters[iSlice][0][i];
        GPUTPCConvertImpl::convert(*mCalib->fastTransform, *mParam, iSlice, row, cln.getPad(), cln.getTime(), x, y, z);
        if (mCfgH.clustersOnNominalRow) {
          x = mParam->tpcGeometry.Row2X(row);
        }
        mParam->Slice2Global(iSlice, x + mCfgH.xAdd, y, z, &ptr->x, &ptr->y, &ptr->z);
      }

      if (fabsf(ptr->z) > mMaxClusterZ) {
        mMaxClusterZ = fabsf(ptr->z);
      }
      ptr->z += iSlice < 18 ? mCfgH.zAdd : -mCfgH.zAdd;
      ptr->x *= GL_SCALE_FACTOR;
      ptr->y *= GL_SCALE_FACTOR;
      ptr->z *= GL_SCALE_FACTOR;
      ptr->w = tCLUSTER;
    }
  }

  int32_t trdTriggerRecord = -1;
  float trdZoffset = 0;
  GPUCA_OPENMP(parallel for num_threads(getNumThreads()) reduction(max : mMaxClusterZ) firstprivate(trdTriggerRecord, trdZoffset))
  for (int32_t i = 0; i < mCurrentSpacePointsTRD; i++) {
    while (mParam->par.continuousTracking && trdTriggerRecord < (int32_t)mIOPtrs->nTRDTriggerRecords - 1 && mIOPtrs->trdTrackletIdxFirst[trdTriggerRecord + 1] <= i) {
      trdTriggerRecord++;
#ifdef GPUCA_HAVE_O2HEADERS
      float trdTime = mIOPtrs->trdTriggerTimes[trdTriggerRecord] * 1e3 / o2::constants::lhc::LHCBunchSpacingNS / o2::tpc::constants::LHCBCPERTIMEBIN;
      trdZoffset = fabsf(mCalib->fastTransformHelper->getCorrMap()->convVertexTimeToZOffset(0, trdTime, mParam->continuousMaxTimeBin));
#endif
    }
    const auto& sp = mIOPtrs->trdSpacePoints[i];
    int32_t iSec = trdGeometry()->GetSector(mIOPtrs->trdTracklets[i].GetDetector());
    float4* ptr = &mGlobalPosTRD[i];
    mParam->Slice2Global(iSec, sp.getX() + mCfgH.xAdd, sp.getY(), sp.getZ(), &ptr->x, &ptr->y, &ptr->z);
    ptr->z += ptr->z > 0 ? trdZoffset : -trdZoffset;
    if (fabsf(ptr->z) > mMaxClusterZ) {
      mMaxClusterZ = fabsf(ptr->z);
    }
    ptr->x *= GL_SCALE_FACTOR;
    ptr->y *= GL_SCALE_FACTOR;
    ptr->z *= GL_SCALE_FACTOR;
    ptr->w = tTRDCLUSTER;
    ptr = &mGlobalPosTRD2[i];
    mParam->Slice2Global(iSec, sp.getX() + mCfgH.xAdd + 4.5f, sp.getY() + 1.5f * sp.getDy(), sp.getZ(), &ptr->x, &ptr->y, &ptr->z);
    ptr->z += ptr->z > 0 ? trdZoffset : -trdZoffset;
    if (fabsf(ptr->z) > mMaxClusterZ) {
      mMaxClusterZ = fabsf(ptr->z);
    }
    ptr->x *= GL_SCALE_FACTOR;
    ptr->y *= GL_SCALE_FACTOR;
    ptr->z *= GL_SCALE_FACTOR;
    ptr->w = tTRDCLUSTER;
  }

  GPUCA_OPENMP(parallel for num_threads(getNumThreads()) reduction(max : mMaxClusterZ))
  for (int32_t i = 0; i < mCurrentClustersTOF; i++) {
#ifdef GPUCA_HAVE_O2HEADERS
    float4* ptr = &mGlobalPosTOF[i];
    mParam->Slice2Global(mIOPtrs->tofClusters[i].getSector(), mIOPtrs->tofClusters[i].getX() + mCfgH.xAdd, mIOPtrs->tofClusters[i].getY(), mIOPtrs->tofClusters[i].getZ(), &ptr->x, &ptr->y, &ptr->z);
    float ZOffset = 0;
    if (mParam->par.continuousTracking) {
      float tofTime = mIOPtrs->tofClusters[i].getTime() * 1e-3 / o2::constants::lhc::LHCBunchSpacingNS / o2::tpc::constants::LHCBCPERTIMEBIN;
      ZOffset = fabsf(mCalib->fastTransformHelper->getCorrMap()->convVertexTimeToZOffset(0, tofTime, mParam->continuousMaxTimeBin));
      ptr->z += ptr->z > 0 ? ZOffset : -ZOffset;
    }
    if (fabsf(ptr->z) > mMaxClusterZ) {
      mMaxClusterZ = fabsf(ptr->z);
    }
    ptr->x *= GL_SCALE_FACTOR;
    ptr->y *= GL_SCALE_FACTOR;
    ptr->z *= GL_SCALE_FACTOR;
    ptr->w = tTOFCLUSTER;
#endif
  }

  if (mCurrentClustersITS) {
#ifdef GPUCA_HAVE_O2HEADERS
    float itsROFhalfLen = 0;
#ifdef GPUCA_O2_LIB // Not available in standalone benchmark
    if (mParam->par.continuousTracking) {
      const auto& alpParams = o2::itsmft::DPLAlpideParam<o2::detectors::DetID::ITS>::Instance();
      itsROFhalfLen = alpParams.roFrameLengthInBC / (float)o2::tpc::constants::LHCBCPERTIMEBIN / 2;
    }
#endif
    int32_t i = 0;
    for (uint32_t j = 0; j < mIOPtrs->nItsClusterROF; j++) {
      float ZOffset = 0;
      if (mParam->par.continuousTracking) {
        o2::InteractionRecord startIR = o2::InteractionRecord(0, mIOPtrs->settingsTF && mIOPtrs->settingsTF->hasTfStartOrbit ? mIOPtrs->settingsTF->tfStartOrbit : 0);
        float itsROFtime = mIOPtrs->itsClusterROF[j].getBCData().differenceInBC(startIR) / (float)o2::tpc::constants::LHCBCPERTIMEBIN;
        ZOffset = fabsf(mCalib->fastTransformHelper->getCorrMap()->convVertexTimeToZOffset(0, itsROFtime + itsROFhalfLen, mParam->continuousMaxTimeBin));
      }
      if (i != mIOPtrs->itsClusterROF[j].getFirstEntry()) {
        throw std::runtime_error("Inconsistent ITS data, number of clusters does not match ROF content");
      }
      for (int32_t k = 0; k < mIOPtrs->itsClusterROF[j].getNEntries(); k++) {
        float4* ptr = &mGlobalPosITS[i];
        const auto& cl = mIOPtrs->itsClusters[i];
        auto* itsGeo = o2::its::GeometryTGeo::Instance();
        auto p = cl.getXYZGlo(*itsGeo);
        ptr->x = p.X();
        ptr->y = p.Y();
        ptr->z = p.Z();
        ptr->z += ptr->z > 0 ? ZOffset : -ZOffset;
        if (fabsf(ptr->z) > mMaxClusterZ) {
          mMaxClusterZ = fabsf(ptr->z);
        }
        ptr->x *= GL_SCALE_FACTOR;
        ptr->y *= GL_SCALE_FACTOR;
        ptr->z *= GL_SCALE_FACTOR;
        ptr->w = tITSCLUSTER;
        i++;
      }
    }
#endif
  }
}
