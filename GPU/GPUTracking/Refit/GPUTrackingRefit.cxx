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

/// \file GPUTrackingRefit.cxx
/// \author David Rohr

#define GPUCA_CADEBUG 0

#include "GPUTrackingRefit.h"
#include "GPUO2DataTypes.h"
#include "GPUTPCGMTrackParam.h"
#include "GPUTPCGMMergedTrack.h"
#include "GPUTPCGMPropagator.h"
#include "GPUConstantMem.h"
#include "ReconstructionDataFormats/Track.h"
#include "CorrectionMapsHelper.h"
#include "DetectorsBase/Propagator.h"
#include "DataFormatsTPC/TrackTPC.h"
#include "GPUParam.inc"
#include "GPUCommonArray.h"
#include "GPUParam.h"
#include "GPUTrackParamConvert.h"
#include "GPUCommonTypeTraits.h"

using namespace GPUCA_NAMESPACE::gpu;
using namespace o2::track;
using namespace o2::base;
using namespace o2::tpc;

static constexpr int32_t kIGNORE_ENDS = 3;

#define IgnoreErrors(SNP)                                                                                            \
  if (mIgnoreErrorsOnTrackEnds) {                                                                                    \
    if (abs(i - stop) <= kIGNORE_ENDS && (CAMath::Abs(SNP) >= Propagator::MAX_SIN_PHI || abs(start - stop) < 30)) {  \
      break;                                                                                                         \
    }                                                                                                                \
    if (abs(i - start) <= kIGNORE_ENDS && (CAMath::Abs(SNP) >= Propagator::MAX_SIN_PHI || abs(start - stop) < 30)) { \
      continue;                                                                                                      \
    }                                                                                                                \
  }
// End IgnoreErrors

#ifndef GPUCA_GPUCODE
void GPUTrackingRefitProcessor::InitializeProcessor()
{
}

void GPUTrackingRefitProcessor::RegisterMemoryAllocation()
{
  AllocateAndInitializeLate();
}

void GPUTrackingRefitProcessor::SetMaxData(const GPUTrackingInOutPointers& io)
{
}
#endif

namespace
{
template <class T>
struct refitTrackTypes;
template <>
struct refitTrackTypes<GPUTPCGMTrackParam> {
  using propagator = GPUTPCGMPropagator;
};
template <>
struct refitTrackTypes<TrackParCov> {
  using propagator = const Propagator*;
};
} // anonymous namespace

template <>
GPUd() void GPUTrackingRefit::initProp<GPUgeneric() GPUTPCGMPropagator>(GPUTPCGMPropagator& prop) // FIXME: GPUgeneric() needed to make the clang spirv output link correctly
{
  prop.SetMaterialTPC();
  prop.SetMaxSinPhi(GPUCA_MAX_SIN_PHI);
  prop.SetToyMCEventsFlag(false);
  prop.SetSeedingErrors(false);
  prop.SetFitInProjections(mPparam->rec.fitInProjections != 0);
  prop.SetPropagateBzOnly(false);
  prop.SetPolynomialField(&mPparam->polynomialField);
  prop.SetMatLUT(mPmatLUT);
}

template <>
GPUd() void GPUTrackingRefit::initProp<const Propagator * GPUgeneric()>(const Propagator*& prop) // FIXME: GPUgeneric() needed to make the clang spirv output link correctly
{
  prop = mPpropagator;
}

template <class T, class S, class U>
GPUd() void GPUTrackingRefit::convertTrack(T& trk, const S& trkX, U& prop, float* chi2)
{
  trk = trkX;
}

// Generic
template <>
GPUd() void GPUTrackingRefit::convertTrack<GPUTPCGMTrackParam, TrackParCov, GPUTPCGMPropagator>(GPUTPCGMTrackParam& trk, const TrackParCov& trkX, GPUTPCGMPropagator& prop, float* chi2)
{
  convertTrackParam(trk, trkX);
  prop.SetTrack(&trk, trkX.getAlpha());
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<TrackParCov, GPUTPCGMTrackParam, GPUTPCGMPropagator>(TrackParCov& trk, const GPUTPCGMTrackParam& trkX, GPUTPCGMPropagator& prop, float* chi2)
{
  convertTrackParam(trk, trkX);
  trk.setAlpha(prop.GetAlpha());
}
// GPUTPCGMMergedTrack input
template <>
GPUd() void GPUTrackingRefit::convertTrack<TrackParCov, GPUTPCGMMergedTrack, const Propagator*>(TrackParCov& trk, const GPUTPCGMMergedTrack& trkX, const Propagator*& prop, float* chi2)
{
  initProp(prop);
  convertTrackParam(trk, trkX.GetParam());
  trk.setAlpha(trkX.GetAlpha());
  *chi2 = trkX.GetParam().GetChi2();
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<GPUTPCGMMergedTrack, TrackParCov, const Propagator*>(GPUTPCGMMergedTrack& trk, const TrackParCov& trkX, const Propagator*& prop, float* chi2)
{
  convertTrackParam(trk.Param(), trkX);
  trk.SetAlpha(trkX.getAlpha());
  trk.Param().SetChi2(*chi2);
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<GPUTPCGMTrackParam, GPUTPCGMMergedTrack, GPUTPCGMPropagator>(GPUTPCGMTrackParam& trk, const GPUTPCGMMergedTrack& trkX, GPUTPCGMPropagator& prop, float* chi2)
{
  initProp(prop);
  trk = trkX.GetParam();
  prop.SetTrack(&trk, trkX.GetAlpha());
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<GPUTPCGMMergedTrack, GPUTPCGMTrackParam, GPUTPCGMPropagator>(GPUTPCGMMergedTrack& trk, const GPUTPCGMTrackParam& trkX, GPUTPCGMPropagator& prop, float* chi2)
{
  trk.SetParam(trkX);
  trk.SetAlpha(prop.GetAlpha());
}
// TrackTPC input
template <>
GPUd() void GPUTrackingRefit::convertTrack<TrackParCov, TrackTPC, const Propagator*>(TrackParCov& trk, const TrackTPC& trkX, const Propagator*& prop, float* chi2)
{
  initProp(prop);
  convertTrack<TrackParCov, TrackParCov, const Propagator*>(trk, trkX, prop, nullptr);
  *chi2 = trkX.getChi2();
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<TrackTPC, TrackParCov, const Propagator*>(TrackTPC& trk, const TrackParCov& trkX, const Propagator*& prop, float* chi2)
{
  convertTrack<TrackParCov, TrackParCov, const Propagator*>(trk, trkX, prop, nullptr);
  trk.setChi2(*chi2);
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<GPUTPCGMTrackParam, TrackTPC, GPUTPCGMPropagator>(GPUTPCGMTrackParam& trk, const TrackTPC& trkX, GPUTPCGMPropagator& prop, float* chi2)
{
  initProp(prop);
  convertTrack<GPUTPCGMTrackParam, TrackParCov, GPUTPCGMPropagator>(trk, trkX, prop, nullptr);
  trk.SetChi2(trkX.getChi2());
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<TrackTPC, GPUTPCGMTrackParam, GPUTPCGMPropagator>(TrackTPC& trk, const GPUTPCGMTrackParam& trkX, GPUTPCGMPropagator& prop, float* chi2)
{
  convertTrack<TrackParCov, GPUTPCGMTrackParam, GPUTPCGMPropagator>(trk, trkX, prop, nullptr);
  trk.setChi2(trkX.GetChi2());
}
// TrackParCovWithArgs input
template <>
GPUd() void GPUTrackingRefit::convertTrack<TrackParCov, GPUTrackingRefit::TrackParCovWithArgs, const Propagator*>(TrackParCov& trk, const GPUTrackingRefit::TrackParCovWithArgs& trkX, const Propagator*& prop, float* chi2)
{
  initProp(prop);
  convertTrack<TrackParCov, TrackParCov, const Propagator*>(trk, trkX.trk, prop, nullptr);
  *chi2 = trkX.chi2 ? *trkX.chi2 : 0.f;
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<GPUTrackingRefit::TrackParCovWithArgs, TrackParCov, const Propagator*>(GPUTrackingRefit::TrackParCovWithArgs& trk, const TrackParCov& trkX, const Propagator*& prop, float* chi2)
{
  convertTrack<TrackParCov, TrackParCov, const Propagator*>(trk.trk, trkX, prop, nullptr);
  if (trk.chi2) {
    *trk.chi2 = *chi2;
  }
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<GPUTPCGMTrackParam, GPUTrackingRefit::TrackParCovWithArgs, GPUTPCGMPropagator>(GPUTPCGMTrackParam& trk, const GPUTrackingRefit::TrackParCovWithArgs& trkX, GPUTPCGMPropagator& prop, float* chi2)
{
  initProp(prop);
  convertTrack<GPUTPCGMTrackParam, TrackParCov, GPUTPCGMPropagator>(trk, trkX.trk, prop, nullptr);
  trk.SetChi2(trkX.chi2 ? *trkX.chi2 : 0.f);
}
template <>
GPUd() void GPUTrackingRefit::convertTrack<GPUTrackingRefit::TrackParCovWithArgs, GPUTPCGMTrackParam, GPUTPCGMPropagator>(GPUTrackingRefit::TrackParCovWithArgs& trk, const GPUTPCGMTrackParam& trkX, GPUTPCGMPropagator& prop, float* chi2)
{
  convertTrack<TrackParCov, GPUTPCGMTrackParam, GPUTPCGMPropagator>(trk.trk, trkX, prop, chi2);
  if (trk.chi2) {
    *trk.chi2 = trkX.GetChi2();
  }
}

GPUd() static const float* getPar(const GPUTPCGMTrackParam& trk) { return trk.GetPar(); }
GPUd() static const float* getPar(const TrackParCov& trk) { return trk.getParams(); }

template <class T, class S>
GPUd() int32_t GPUTrackingRefit::RefitTrack(T& trkX, bool outward, bool resetCov)
{
  CADEBUG(int32_t ii; printf("\nRefitting track\n"));
  typename refitTrackTypes<S>::propagator prop;
  S trk;
  float TrackParCovChi2 = 0.f;
  convertTrack<S, T, typename refitTrackTypes<S>::propagator>(trk, trkX, prop, &TrackParCovChi2);
  int32_t begin = 0, count;
  float tOffset;
  if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
    count = trkX.NClusters();
    if (trkX.Looper()) {
      int32_t leg = mPtrackHits[trkX.FirstClusterRef() + trkX.NClusters() - 1].leg;
      for (int32_t i = trkX.NClusters() - 2; i > 0; i--) {
        if (mPtrackHits[trkX.FirstClusterRef() + i].leg != leg) {
          begin = i + 1;
          break;
        }
      }
    }
    tOffset = trkX.GetParam().GetTZOffset();
  } else if constexpr (std::is_same_v<T, TrackTPC>) {
    count = trkX.getNClusters();
    tOffset = trkX.getTime0();
  } else if constexpr (std::is_same_v<T, TrackParCovWithArgs>) {
    count = trkX.clusRef.getEntries();
    tOffset = trkX.time0;
  } else {
    static_assert("Invalid template");
  }
  if constexpr (std::is_same_v<S, GPUTPCGMTrackParam>) {
    CADEBUG(printf("\t%21sInit    Alpha %8.3f    , X %8.3f - Y %8.3f, Z %8.3f   -   QPt %7.2f (%7.2f), SP %5.2f (%5.2f)   ---   Cov sY %8.3f sZ %8.3f sSP %8.3f sPt %8.3f\n", "", prop.GetAlpha(), trk.GetX(), trk.Par()[0], trk.Par()[1], trk.Par()[4], prop.GetQPt0(), trk.Par()[2], prop.GetSinPhi0(), sqrtf(trk.Cov()[0]), sqrtf(trk.Cov()[2]), sqrtf(trk.Cov()[5]), sqrtf(trk.Cov()[14])));
  }

  int32_t direction = outward ? -1 : 1;
  int32_t start = outward ? count - 1 : begin;
  int32_t stop = outward ? begin - 1 : count;
  const ClusterNative* cl = nullptr;
  uint8_t sector = 255, row = 255;
  int32_t lastSector = -1, currentSector = -1, currentRow = -1;
  int16_t clusterState = 0, nextState = 0;
  int32_t nFitted = 0;
  float sumInvSqrtCharge = 0.f;
  int32_t nAvgCharge = 0;

  for (int32_t i = start; i != stop; i += cl ? 0 : direction) {
    float x = 0, y = 0, z = 0, charge = 0; // FIXME: initialization unneeded, but GCC incorrectly produces uninitialized warnings otherwise
    float time = 0.f, invCharge = 0.f, invSqrtCharge = 0.f; // Same here...
    int32_t clusters = 0;
    while (true) {
      if (!cl) {
        CADEBUG(ii = i);
        if constexpr (std::is_same_v<T, GPUTPCGMMergedTrack>) {
          const auto& hit = mPtrackHits[trkX.FirstClusterRef() + i];
          cl = &mPclusterNative->clustersLinear[hit.num];
          if (hit.state & (GPUTPCGMMergedTrackHit::flagReject | GPUTPCGMMergedTrackHit::flagNotFit)) {
            cl = nullptr;
            if (i + direction != stop) {
              i += direction;
              continue;
            }
            break;
          }
          row = hit.row;
          sector = hit.slice;
          nextState = mPclusterState[hit.num];
        } else if constexpr (std::is_same_v<T, TrackTPC>) {
          cl = &trkX.getCluster(mPtrackHitReferences, i, *mPclusterNative, sector, row);
          nextState = mPclusterState[cl - mPclusterNative->clustersLinear];
        } else if constexpr (std::is_same_v<T, TrackParCovWithArgs>) {
          cl = &TrackTPC::getCluster(mPtrackHitReferences, i, *mPclusterNative, sector, row, trkX.clusRef);
          nextState = mPclusterState[cl - mPclusterNative->clustersLinear];
        } else {
          static_assert("Invalid template");
        }
      }
      if (clusters == 0 || (row == currentRow && sector == currentSector)) {
        if (clusters == 1) {
          x *= charge;
          y *= charge;
          z *= charge;
        }
        if (clusters == 0) {
          mPfastTransformHelper->Transform(sector, row, cl->getPad(), cl->getTime(), x, y, z, tOffset);
          CADEBUG(printf("\tHit %3d/%3d Row %3d: Cluster Alpha %8.3f %3d, X %8.3f - Y %8.3f, Z %8.3f - State %d\n", ii, count, row, mPparam->Alpha(sector), (int32_t)sector, x, y, z, (int32_t)nextState));
          currentRow = row;
          currentSector = sector;
          charge = cl->qTot;
          clusterState = nextState;
          time = cl->getTime();
          invSqrtCharge = CAMath::InvSqrt(cl->qMax);
          invCharge = (1.f / cl->qMax);
        } else {
          float xx, yy, zz;
          mPfastTransformHelper->Transform(sector, row, cl->getPad(), cl->getTime(), xx, yy, zz, tOffset);
          CADEBUG(printf("\tHit %3d/%3d Row %3d: Cluster Alpha %8.3f %3d, X %8.3f - Y %8.3f, Z %8.3f - State %d\n", ii, count, row, mPparam->Alpha(sector), (int32_t)sector, xx, yy, zz, (int32_t)nextState));
          x += xx * cl->qTot;
          y += yy * cl->qTot;
          z += zz * cl->qTot;
          charge += cl->qTot;
          clusterState |= nextState;
        }
        cl = nullptr;
        clusters++;
        if (i + direction != stop) {
          i += direction;
          continue;
        }
      }
      break;
    }
    if (clusters == 0) {
      continue;
    } else if (clusters > 1) {
      x /= charge;
      y /= charge;
      z /= charge;
      CADEBUG(printf("\tMerged Hit  Row %3d: Cluster Alpha %8.3f %3d, X %8.3f - Y %8.3f, Z %8.3f - State %d\n", row, mPparam->Alpha(sector), (int32_t)sector, x, y, z, (int32_t)clusterState));
    }

    float invAvgCharge = (sumInvSqrtCharge += invSqrtCharge) / ++nAvgCharge;
    invAvgCharge *= invAvgCharge;

    if constexpr (std::is_same_v<S, GPUTPCGMTrackParam>) {
      if (prop.PropagateToXAlpha(x, mPparam->Alpha(currentSector), !outward)) {
        IgnoreErrors(trk.GetSinPhi());
        return -2;
      }
      if (resetCov) {
        trk.ResetCovariance();
      } else if (lastSector != -1 && (lastSector < 18) != (sector < 18)) {
        if (mPparam->rec.tpc.addErrorsCECrossing) {
          if (mPparam->rec.tpc.addErrorsCECrossing >= 2) {
            trk.AddCovDiagErrorsWithCorrelations(mPparam->rec.tpc.errorsCECrossing);
          } else {
            trk.AddCovDiagErrors(mPparam->rec.tpc.errorsCECrossing);
          }
        } else if (trk.Cov()[2] < 0.5f) {
          trk.Cov()[2] = 0.5f;
        }
      }
      CADEBUG(printf("\t%21sPropaga Alpha %8.3f    , X %8.3f - Y %8.3f, Z %8.3f   -   QPt %7.2f (%7.2f), SP %5.2f (%5.2f)   ---   Res %8.3f %8.3f   ---   Cov sY %8.3f sZ %8.3f sSP %8.3f sPt %8.3f   -   YPt %8.3f\n", "", prop.GetAlpha(), x, trk.Par()[0], trk.Par()[1], trk.Par()[4], prop.GetQPt0(), trk.Par()[2], prop.GetSinPhi0(), trk.Par()[0] - y, trk.Par()[1] - z, sqrtf(trk.Cov()[0]), sqrtf(trk.Cov()[2]), sqrtf(trk.Cov()[5]), sqrtf(trk.Cov()[14]), trk.Cov()[10]));
      lastSector = sector;
      if (prop.Update(y, z, row, *mPparam, clusterState, 0, nullptr, true, sector, time, invAvgCharge, invCharge)) {
        IgnoreErrors(trk.GetSinPhi());
        return -3;
      }
      trk.ConstrainSinPhi();
      CADEBUG(printf("\t%21sFit     Alpha %8.3f    , X %8.3f - Y %8.3f, Z %8.3f   -   QPt %7.2f (%7.2f), SP %5.2f (%5.2f), DzDs %5.2f %16s    ---   Cov sY %8.3f sZ %8.3f sSP %8.3f sPt %8.3f   -   YPt %8.3f\n", "", prop.GetAlpha(), x, trk.Par()[0], trk.Par()[1], trk.Par()[4], prop.GetQPt0(), trk.Par()[2], prop.GetSinPhi0(), trk.Par()[3], "", sqrtf(trk.Cov()[0]), sqrtf(trk.Cov()[2]), sqrtf(trk.Cov()[5]), sqrtf(trk.Cov()[14]), trk.Cov()[10]));
    } else if constexpr (std::is_same_v<S, TrackParCov>) {
      if (!trk.rotate(mPparam->Alpha(currentSector))) {
        IgnoreErrors(trk.getSnp());
        return -1;
      }
      if (!prop->PropagateToXBxByBz(trk, x, GPUCA_MAX_SIN_PHI_LOW)) {
        IgnoreErrors(trk.getSnp());
        return -2;
      }
      if (lastSector != -1 && (lastSector < 18) != (sector < 18)) {
        if (mPparam->rec.tpc.addErrorsCECrossing) {
          trk.updateCov(mPparam->rec.tpc.errorsCECrossing, mPparam->rec.tpc.addErrorsCECrossing >= 2);
        } else if (trk.getCov()[2] < 0.5f) {
          trk.setCov(0.5f, 2);
        }
      }
      if (resetCov) {
        trk.resetCovariance();
        float bzkG = prop->getNominalBz(), qptB5Scale = CAMath::Abs(bzkG) > 0.1f ? CAMath::Abs(bzkG) / 5.006680f : 1.f;
        float q2pt2 = trk.getQ2Pt() * trk.getQ2Pt(), q2pt2Wgh = q2pt2 * qptB5Scale * qptB5Scale;
        float err2 = (100.f + q2pt2Wgh) / (1.f + q2pt2Wgh) * q2pt2; // -> 100 for high pTs, -> 1 for low pTs.
        trk.setCov(err2, 14);                                       // 100% error
        TrackParCovChi2 = 0.f;
      }
      CADEBUG(printf("\t%21sPropaga Alpha %8.3f    , X %8.3f - Y %8.3f, Z %8.3f   -   QPt %7.2f (%7.2f), SP %5.2f (%5.2f)   ---   Res %8.3f %8.3f   ---   Cov sY %8.3f sZ %8.3f sSP %8.3f sPt %8.3f   -   YPt %8.3f\n", "", trk.getAlpha(), x, trk.getParams()[0], trk.getParams()[1], trk.getParams()[4], trk.getParams()[4], trk.getParams()[2], trk.getParams()[2], trk.getParams()[0] - y, trk.getParams()[1] - z, sqrtf(trk.getCov()[0]), sqrtf(trk.getCov()[2]), sqrtf(trk.getCov()[5]), sqrtf(trk.getCov()[14]), trk.getCov()[10]));
      gpu::gpustd::array<float, 2> p = {y, z};
      gpu::gpustd::array<float, 3> c = {0, 0, 0};
      GPUTPCGMPropagator::GetErr2(c[0], c[2], *mPparam, getPar(trk)[2], getPar(trk)[3], z, x, y, currentRow, clusterState, sector, time, invAvgCharge, invCharge, false);
      TrackParCovChi2 += trk.getPredictedChi2(p, c);
      if (!trk.update(p, c)) {
        IgnoreErrors(trk.getSnp());
        return -3;
      }
      CADEBUG(printf("\t%21sFit     Alpha %8.3f    , X %8.3f - Y %8.3f, Z %8.3f   -   QPt %7.2f (%7.2f), SP %5.2f (%5.2f), DzDs %5.2f %16s    ---   Cov sY %8.3f sZ %8.3f sSP %8.3f sPt %8.3f   -   YPt %8.3f\n", "", trk.getAlpha(), x, trk.getParams()[0], trk.getParams()[1], trk.getParams()[4], trk.getParams()[4], trk.getParams()[2], trk.getParams()[2], trk.getParams()[3], "", sqrtf(trk.getCov()[0]), sqrtf(trk.getCov()[2]), sqrtf(trk.getCov()[5]), sqrtf(trk.getCov()[14]), trk.getCov()[10]));
    } else {
      static_assert("Invalid template");
    }
    resetCov = false;
    nFitted++;
  }
  if constexpr (std::is_same_v<S, GPUTPCGMTrackParam>) {
    float alpha = prop.GetAlpha();
    trk.MoveToReference(prop, *mPparam, alpha);
    trk.NormalizeAlpha(alpha);
    prop.SetAlpha(alpha);
  } else if constexpr (std::is_same_v<S, TrackParCov>) {
    static constexpr float kDeg2Rad = M_PI / 180.f;
    static constexpr float kSectAngle = 2 * M_PI / 18.f;
    if (mPparam->rec.tpc.trackReferenceX <= 500) {
      if (prop->PropagateToXBxByBz(trk, mPparam->rec.tpc.trackReferenceX)) {
        if (CAMath::Abs(trk.getY()) > trk.getX() * CAMath::Tan(kSectAngle / 2.f)) {
          float newAlpha = trk.getAlpha() + CAMath::Round(CAMath::ATan2(trk.getY(), trk.getX()) / kDeg2Rad / 20.f) * kSectAngle;
          GPUTPCGMTrackParam::NormalizeAlpha(newAlpha);
          trk.rotate(newAlpha) && prop->PropagateToXBxByBz(trk, mPparam->rec.tpc.trackReferenceX);
        }
      }
    }
  } else {
    static_assert("Invalid template");
  }

  convertTrack<T, S, typename refitTrackTypes<S>::propagator>(trkX, trk, prop, &TrackParCovChi2);
  return nFitted;
}

#if !defined(GPUCA_GPUCODE) || defined(GPUCA_GPUCODE_DEVICE) // FIXME: DR: WORKAROUND to avoid CUDA bug creating host symbols for device code.
template GPUdni() int32_t GPUTrackingRefit::RefitTrack<GPUTPCGMMergedTrack, TrackParCov>(GPUTPCGMMergedTrack& trk, bool outward, bool resetCov);
template GPUdni() int32_t GPUTrackingRefit::RefitTrack<GPUTPCGMMergedTrack, GPUTPCGMTrackParam>(GPUTPCGMMergedTrack& trk, bool outward, bool resetCov);
template GPUdni() int32_t GPUTrackingRefit::RefitTrack<TrackTPC, TrackParCov>(TrackTPC& trk, bool outward, bool resetCov);
template GPUdni() int32_t GPUTrackingRefit::RefitTrack<TrackTPC, GPUTPCGMTrackParam>(TrackTPC& trk, bool outward, bool resetCov);
template GPUdni() int32_t GPUTrackingRefit::RefitTrack<GPUTrackingRefit::TrackParCovWithArgs, TrackParCov>(GPUTrackingRefit::TrackParCovWithArgs& trk, bool outward, bool resetCov);
template GPUdni() int32_t GPUTrackingRefit::RefitTrack<GPUTrackingRefit::TrackParCovWithArgs, GPUTPCGMTrackParam>(GPUTrackingRefit::TrackParCovWithArgs& trk, bool outward, bool resetCov);
#endif

#ifndef GPUCA_GPUCODE
void GPUTrackingRefit::SetPtrsFromGPUConstantMem(const GPUConstantMem* v, GPUParam* p)
{
  mPclusterState = v->ioPtrs.mergedTrackHitStates;
  mPclusterNative = v->ioPtrs.clustersNative;
  mPtrackHits = v->ioPtrs.mergedTrackHits;
  mPfastTransformHelper = v->calibObjects.fastTransformHelper;
  mPmatLUT = v->calibObjects.matLUT;
  mPparam = p ? p : &v->param;
}
#endif
