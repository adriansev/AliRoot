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

/// \file GPUTPCGMO2Output.cxx
/// \author David Rohr

#include "GPUTPCDef.h"
#include "GPUTPCGMO2Output.h"
#include "GPUCommonAlgorithm.h"
#include "DataFormatsTPC/TrackTPC.h"
#include "DataFormatsTPC/Constants.h"
#include "DataFormatsTPC/PIDResponse.h"
#include "TPCFastTransform.h"
#include "CorrectionMapsHelper.h"

#ifndef GPUCA_GPUCODE
#include "SimulationDataFormat/ConstMCTruthContainer.h"
#include "SimulationDataFormat/MCCompLabel.h"
#include "GPUQAHelper.h"
#endif

using namespace o2::gpu;
using namespace o2::tpc;
using namespace o2::tpc::constants;

GPUdi() static constexpr uint8_t getFlagsReject() { return GPUTPCGMMergedTrackHit::flagReject | GPUTPCGMMergedTrackHit::flagNotFit; }
GPUdi() static uint32_t getFlagsRequired(const GPUSettingsRec& rec) { return rec.tpc.dropSecondaryLegsInOutput ? gputpcgmmergertypes::attachGoodLeg : gputpcgmmergertypes::attachZero; }

template <>
GPUdii() void GPUTPCGMO2Output::Thread<GPUTPCGMO2Output::prepare>(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& smem, processorType& GPUrestrict() merger)
{
  const GPUTPCGMMergedTrack* tracks = merger.OutputTracks();
  const uint32_t nTracks = merger.NOutputTracks();
  const GPUTPCGMMergedTrackHit* trackClusters = merger.Clusters();
  const GPUdEdxInfo* tracksdEdx = merger.OutputTracksdEdx();

  constexpr uint8_t flagsReject = getFlagsReject();
  const uint32_t flagsRequired = getFlagsRequired(merger.Param().rec);
  bool cutOnTrackdEdx = merger.Param().par.dodEdx && merger.Param().dodEdxDownscaled && merger.Param().rec.tpc.minTrackdEdxMax2Tot > 0.f;

  GPUTPCGMMerger::tmpSort* GPUrestrict() trackSort = merger.TrackSortO2();
  uint2* GPUrestrict() tmpData = merger.ClusRefTmp();
  for (uint32_t i = get_global_id(0); i < nTracks; i += get_global_size(0)) {
    if (!tracks[i].OK()) {
      continue;
    }
    uint32_t nCl = 0;
    for (uint32_t j = 0; j < tracks[i].NClusters(); j++) {
      if ((trackClusters[tracks[i].FirstClusterRef() + j].state & flagsReject) || (merger.ClusterAttachment()[trackClusters[tracks[i].FirstClusterRef() + j].num] & flagsRequired) != flagsRequired) {
        continue;
      }
      if (merger.Param().rec.tpc.dropSecondaryLegsInOutput && trackClusters[tracks[i].FirstClusterRef() + j].leg != trackClusters[tracks[i].FirstClusterRef() + tracks[i].NClusters() - 1].leg) {
        continue;
      }
      nCl++;
    }
    if (nCl == 0) {
      continue;
    }
    if (merger.Param().rec.tpc.dropSecondaryLegsInOutput && nCl + 2 < GPUCA_TRACKLET_SELECTOR_MIN_HITS_B5(tracks[i].GetParam().GetQPt() * merger.Param().qptB5Scaler)) { // Give 2 hits tolerance in the primary leg, compared to the full fit of the looper
      continue;
    }
    if (merger.Param().rec.tpc.minNClustersFinalTrack != -1 && nCl < (uint32_t)merger.Param().rec.tpc.minNClustersFinalTrack) {
      continue;
    }
    if (cutOnTrackdEdx && (tracksdEdx[i].dEdxMaxTPC < merger.Param().rec.tpc.minTrackdEdxMax || tracksdEdx[i].dEdxMaxTPC < tracksdEdx[i].dEdxTotTPC * merger.Param().rec.tpc.minTrackdEdxMax2Tot) && !(tracksdEdx[i].dEdxMaxTPC == 0 && CAMath::Abs(tracks[i].GetParam().GetDzDs()) > 0.03f)) {
      continue;
    }
    uint32_t myId = CAMath::AtomicAdd(&merger.Memory()->nO2Tracks, 1u);
    tmpData[i] = {nCl, CAMath::AtomicAdd(&merger.Memory()->nO2ClusRefs, nCl + (nCl + 1) / 2)};
    trackSort[myId] = {i, (merger.Param().par.earlyTpcTransform || tracks[i].CSide()) ? tracks[i].GetParam().GetTZOffset() : -tracks[i].GetParam().GetTZOffset()};
  }
}

template <>
GPUdii() void GPUTPCGMO2Output::Thread<GPUTPCGMO2Output::sort>(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& smem, processorType& GPUrestrict() merger)
{
#ifndef GPUCA_SPECIALIZE_THRUST_SORTS
  if (iThread || iBlock) {
    return;
  }
  GPUTPCGMMerger::tmpSort* GPUrestrict() trackSort = merger.TrackSortO2();
  auto comp = [](const auto& a, const auto& b) { return (a.y > b.y); };
  GPUCommonAlgorithm::sortDeviceDynamic(trackSort, trackSort + merger.Memory()->nO2Tracks, comp);
#endif
}

#if defined(GPUCA_SPECIALIZE_THRUST_SORTS) && !defined(GPUCA_GPUCODE_COMPILEKERNELS) // Specialize GPUTPCGMO2Output::Thread<GPUTPCGMO2Output::sort>
struct GPUTPCGMO2OutputSort_comp {
  GPUd() bool operator()(const GPUTPCGMMerger::tmpSort& a, const GPUTPCGMMerger::tmpSort& b)
  {
    return (a.y > b.y);
  }
};

template <>
inline void GPUCA_KRNL_BACKEND_CLASS::runKernelBackendInternal<GPUTPCGMO2Output, GPUTPCGMO2Output::sort>(const krnlSetupTime& _xyz)
{
  thrust::device_ptr<GPUTPCGMMerger::tmpSort> trackSort(mProcessorsShadow->tpcMerger.TrackSortO2());
  ThrustVolatileAsyncAllocator alloc(this);
  thrust::sort(GPUCA_THRUST_NAMESPACE::par(alloc).on(mInternals->Streams[_xyz.x.stream]), trackSort, trackSort + processors()->tpcMerger.NOutputTracksTPCO2(), GPUTPCGMO2OutputSort_comp());
}
#endif // GPUCA_SPECIALIZE_THRUST_SORTS - Specialize GPUTPCGMO2Output::Thread<GPUTPCGMO2Output::sort>

template <>
GPUdii() void GPUTPCGMO2Output::Thread<GPUTPCGMO2Output::output>(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& smem, processorType& GPUrestrict() merger)
{
  constexpr float MinDelta = 0.1f;
  const GPUTPCGMMergedTrack* tracks = merger.OutputTracks();
  GPUdEdxInfo* tracksdEdx = merger.OutputTracksdEdx();
  const int32_t nTracks = merger.NOutputTracksTPCO2();
  const GPUTPCGMMergedTrackHit* trackClusters = merger.Clusters();
  constexpr uint8_t flagsReject = getFlagsReject();
  const uint32_t flagsRequired = getFlagsRequired(merger.Param().rec);
  TrackTPC* outputTracks = merger.OutputTracksTPCO2();
  uint32_t* clusRefs = merger.OutputClusRefsTPCO2();

  GPUTPCGMMerger::tmpSort* GPUrestrict() trackSort = merger.TrackSortO2();
  uint2* GPUrestrict() tmpData = merger.ClusRefTmp();
  float const SNPThresh = 0.999990f;

  for (int32_t iTmp = get_global_id(0); iTmp < nTracks; iTmp += get_global_size(0)) {
    TrackTPC oTrack;
    const int32_t i = trackSort[iTmp].x;
    auto snpIn = tracks[i].GetParam().GetSinPhi();
    if (snpIn > SNPThresh) {
      snpIn = SNPThresh;
    } else if (snpIn < -SNPThresh) {
      snpIn = -SNPThresh;
    }
    oTrack.set(tracks[i].GetParam().GetX(), tracks[i].GetAlpha(),
               {tracks[i].GetParam().GetY(), tracks[i].GetParam().GetZ(), snpIn, tracks[i].GetParam().GetDzDs(), tracks[i].GetParam().GetQPt()},
               {tracks[i].GetParam().GetCov(0),
                tracks[i].GetParam().GetCov(1), tracks[i].GetParam().GetCov(2),
                tracks[i].GetParam().GetCov(3), tracks[i].GetParam().GetCov(4), tracks[i].GetParam().GetCov(5),
                tracks[i].GetParam().GetCov(6), tracks[i].GetParam().GetCov(7), tracks[i].GetParam().GetCov(8), tracks[i].GetParam().GetCov(9),
                tracks[i].GetParam().GetCov(10), tracks[i].GetParam().GetCov(11), tracks[i].GetParam().GetCov(12), tracks[i].GetParam().GetCov(13), tracks[i].GetParam().GetCov(14)});

    oTrack.setChi2(tracks[i].GetParam().GetChi2());
    auto& outerPar = tracks[i].OuterParam();
    if (merger.Param().par.dodEdx && merger.Param().dodEdxDownscaled) {
      oTrack.setdEdx(tracksdEdx[i]);
    }

    auto snpOut = outerPar.P[2];
    if (snpOut > SNPThresh) {
      snpOut = SNPThresh;
    } else if (snpOut < -SNPThresh) {
      snpOut = -SNPThresh;
    }
    oTrack.setOuterParam(o2::track::TrackParCov(
      outerPar.X, outerPar.alpha,
      {outerPar.P[0], outerPar.P[1], snpOut, outerPar.P[3], outerPar.P[4]},
      {outerPar.C[0], outerPar.C[1], outerPar.C[2], outerPar.C[3], outerPar.C[4], outerPar.C[5],
       outerPar.C[6], outerPar.C[7], outerPar.C[8], outerPar.C[9], outerPar.C[10], outerPar.C[11],
       outerPar.C[12], outerPar.C[13], outerPar.C[14]}));

    if (merger.Param().par.dodEdx && merger.Param().dodEdxDownscaled && merger.Param().rec.tpc.enablePID) {
      PIDResponse pidResponse{};
      auto pid = pidResponse.getMostProbablePID(oTrack, merger.Param().rec.tpc.PID_EKrangeMin, merger.Param().rec.tpc.PID_EKrangeMax, merger.Param().rec.tpc.PID_EPrangeMin, merger.Param().rec.tpc.PID_EPrangeMax, merger.Param().rec.tpc.PID_EDrangeMin, merger.Param().rec.tpc.PID_EDrangeMax, merger.Param().rec.tpc.PID_ETrangeMin, merger.Param().rec.tpc.PID_ETrangeMax, merger.Param().rec.tpc.PID_useNsigma, merger.Param().rec.tpc.PID_sigma);
      auto pidRemap = merger.Param().rec.tpc.PID_remap[pid];
      if (pidRemap >= 0) {
        pid = pidRemap;
      }
      oTrack.setPID(pid, true);
      oTrack.getParamOut().setPID(pid, true);
    }

    uint32_t nOutCl = tmpData[i].x;
    uint32_t clBuff = tmpData[i].y;
    oTrack.setClusterRef(clBuff, nOutCl);
    uint32_t* clIndArr = &clusRefs[clBuff];
    uint8_t* sectorIndexArr = reinterpret_cast<uint8_t*>(clIndArr + nOutCl);
    uint8_t* rowIndexArr = sectorIndexArr + nOutCl;

    uint32_t nOutCl2 = 0;
    float t1 = 0, t2 = 0;
    int32_t sector1 = 0, sector2 = 0;
    const o2::tpc::ClusterNativeAccess* GPUrestrict() clusters = merger.GetConstantMem()->ioPtrs.clustersNative;
    for (uint32_t j = 0; j < tracks[i].NClusters(); j++) {
      if ((trackClusters[tracks[i].FirstClusterRef() + j].state & flagsReject) || (merger.ClusterAttachment()[trackClusters[tracks[i].FirstClusterRef() + j].num] & flagsRequired) != flagsRequired) {
        continue;
      }
      if (merger.Param().rec.tpc.dropSecondaryLegsInOutput && trackClusters[tracks[i].FirstClusterRef() + j].leg != trackClusters[tracks[i].FirstClusterRef() + tracks[i].NClusters() - 1].leg) {
        continue;
      }
      int32_t clusterIdGlobal = trackClusters[tracks[i].FirstClusterRef() + j].num;
      int32_t sector = trackClusters[tracks[i].FirstClusterRef() + j].slice;
      int32_t globalRow = trackClusters[tracks[i].FirstClusterRef() + j].row;
      int32_t clusterIdInRow = clusterIdGlobal - clusters->clusterOffset[sector][globalRow];
      clIndArr[nOutCl2] = clusterIdInRow;
      sectorIndexArr[nOutCl2] = sector;
      rowIndexArr[nOutCl2] = globalRow;
      if (nOutCl2 == 0) {
        t1 = clusters->clustersLinear[clusterIdGlobal].getTime();
        sector1 = sector;
      }
      if (++nOutCl2 == nOutCl) {
        t2 = clusters->clustersLinear[clusterIdGlobal].getTime();
        sector2 = sector;
      }
    }

    bool cce = tracks[i].CCE() && ((sector1 < MAXSECTOR / 2) ^ (sector2 < MAXSECTOR / 2));
    float time0 = 0.f, tFwd = 0.f, tBwd = 0.f;
    if (merger.Param().par.continuousTracking) {
      time0 = tracks[i].GetParam().GetTZOffset();
      if (cce) {
        bool lastSide = trackClusters[tracks[i].FirstClusterRef()].slice < MAXSECTOR / 2;
        float delta = 0.f;
        for (uint32_t iCl = 1; iCl < tracks[i].NClusters(); iCl++) {
          auto& cacl1 = trackClusters[tracks[i].FirstClusterRef() + iCl];
          if (lastSide ^ (cacl1.slice < MAXSECTOR / 2)) {
            auto& cl1 = clusters->clustersLinear[cacl1.num];
            auto& cl2 = clusters->clustersLinear[trackClusters[tracks[i].FirstClusterRef() + iCl - 1].num];
            delta = CAMath::Abs(cl1.getTime() - cl2.getTime()) * 0.5f;
            if (delta < MinDelta) {
              delta = MinDelta;
            }
            break;
          }
        }
        tFwd = tBwd = delta;
      } else {
        // estimate max/min time increments which still keep track in the physical limits of the TPC
        const float tmin = CAMath::Min(t1, t2);
        const float maxDriftTime = merger.GetConstantMem()->calibObjects.fastTransformHelper->getCorrMap()->getMaxDriftTime(t1 > t2 ? sector1 : sector2);
        const float clusterT0 = merger.GetConstantMem()->calibObjects.fastTransformHelper->getCorrMap()->getT0();
        const float tmax = CAMath::Min(tmin + maxDriftTime, CAMath::Max(t1, t2));
        float delta = 0.f;
        if (time0 + maxDriftTime < tmax) {
          delta = tmax - time0 - maxDriftTime;
        }
        if (tmin - clusterT0 < time0 + delta) {
          delta = tmin - clusterT0 - time0;
        }
        if (delta != 0.f) {
          time0 += delta;
          const float deltaZ = merger.GetConstantMem()->calibObjects.fastTransformHelper->getCorrMap()->convDeltaTimeToDeltaZinTimeFrame(sector2, delta);
          oTrack.setZ(oTrack.getZ() + deltaZ);
        }
        tFwd = tmin - clusterT0 - time0;
        tBwd = time0 - tmax + maxDriftTime;
      }
    }
    if (tBwd < 0.f) {
      tBwd = 0.f;
    }
    oTrack.setTime0(time0);
    oTrack.setDeltaTBwd(tBwd);
    oTrack.setDeltaTFwd(tFwd);
    if (cce) {
      oTrack.setHasCSideClusters();
      oTrack.setHasASideClusters();
    } else if (tracks[i].CSide()) {
      oTrack.setHasCSideClusters();
    } else {
      oTrack.setHasASideClusters();
    }
    outputTracks[iTmp] = oTrack;
  }
}

template <>
GPUdii() void GPUTPCGMO2Output::Thread<GPUTPCGMO2Output::mc>(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& smem, processorType& GPUrestrict() merger)
{
#ifndef GPUCA_GPUCODE
  const o2::tpc::ClusterNativeAccess* GPUrestrict() clusters = merger.GetConstantMem()->ioPtrs.clustersNative;
  if (clusters == nullptr || clusters->clustersMCTruth == nullptr) {
    return;
  }
  if (merger.OutputTracksTPCO2MC() == nullptr) {
    return;
  }

  auto labelAssigner = GPUTPCTrkLbl(clusters->clustersMCTruth, 0.1f);
  uint32_t* clusRefs = merger.OutputClusRefsTPCO2();
  for (uint32_t i = get_global_id(0); i < merger.NOutputTracksTPCO2(); i += get_global_size(0)) {
    labelAssigner.reset();
    const auto& trk = merger.OutputTracksTPCO2()[i];
    for (int32_t j = 0; j < trk.getNClusters(); j++) {
      uint8_t sectorIndex, rowIndex;
      uint32_t clusterIndex;
      trk.getClusterReference(clusRefs, j, sectorIndex, rowIndex, clusterIndex);
      uint32_t clusterIdGlobal = clusters->clusterOffset[sectorIndex][rowIndex] + clusterIndex;
      labelAssigner.addLabel(clusterIdGlobal);
    }
    merger.OutputTracksTPCO2MC()[i] = labelAssigner.computeLabel();
  }
#endif
}
