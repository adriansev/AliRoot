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

/// \file GPUTPCTrackletConstructor.cxx
/// \author Sergey Gorbunov, David Rohr

#define GPUCA_CADEBUG 0

#include "GPUTPCDef.h"
#include "GPUTPCGrid.h"
#include "GPUTPCHit.h"
#include "GPUTPCTrackParam.h"
#include "GPUTPCTracker.h"
#include "GPUTPCTracklet.h"
#include "GPUTPCTrackletConstructor.h"
#include "GPUTPCGlobalTracking.h"
#include "CorrectionMapsHelper.h"
#ifdef GPUCA_HAVE_O2HEADERS
#include "CalibdEdxContainer.h"
#endif // GPUCA_HAVE_O2HEADERS
#include "GPUParam.inc"
#include "GPUCommonMath.h"

using namespace GPUCA_NAMESPACE::gpu;

GPUdii() void GPUTPCTrackletConstructor::InitTracklet(GPUTPCTrackParam& GPUrestrict() tParam)
{
  // Initialize Tracklet Parameters using default values
  tParam.InitParam();
}

GPUd() bool GPUTPCTrackletConstructor::CheckCov(GPUTPCTrackParam& GPUrestrict() tParam)
{
  bool ok = 1;
  const float* c = tParam.Cov();
  for (int32_t i = 0; i < 15; i++) {
    ok = ok && CAMath::Finite(c[i]);
  }
  for (int32_t i = 0; i < 5; i++) {
    ok = ok && CAMath::Finite(tParam.Par()[i]);
  }
  ok = ok && (tParam.X() > 50);
  if (c[0] <= 0 || c[2] <= 0 || c[5] <= 0 || c[9] <= 0 || c[14] <= 0) {
    ok = 0;
  }
  return (ok);
}

GPUd() void GPUTPCTrackletConstructor::StoreTracklet(int32_t /*nBlocks*/, int32_t /*nThreads*/, int32_t /*iBlock*/, int32_t /*iThread*/, GPUsharedref() GPUSharedMemory& s, GPUTPCThreadMemory& GPUrestrict() r, GPUconstantref() GPUTPCTracker& GPUrestrict() tracker, GPUTPCTrackParam& GPUrestrict() tParam, calink* rowHits)
{
  // reconstruction of tracklets, tracklet store step
  const uint32_t nHits = r.mLastRow + 1 - r.mFirstRow;
  if (nHits == 0 || r.mNHits == 0 || (r.mNHits < GPUCA_TRACKLET_SELECTOR_MIN_HITS_B5(tParam.QPt() * tracker.Param().qptB5Scaler) || !CheckCov(tParam) || CAMath::Abs(tParam.GetQPt() * tracker.Param().qptB5Scaler) > tracker.Param().rec.maxTrackQPtB5)) {
    CADEBUG(printf("    Rejected: nHits %d QPt %f MinHits %d MaxQPt %f CheckCov %d\n", r.mNHits, tParam.QPt(), GPUCA_TRACKLET_SELECTOR_MIN_HITS_B5(tParam.QPt() * tracker.Param().qptB5Scaler), tracker.Param().rec.maxTrackQPtB5, (int32_t)CheckCov(tParam)));
    return;
  }

  /*GPUInfo("Tracklet %d: Hits %3d NDF %3d Chi %8.4f Sign %f Cov: %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f %2.4f", r.mISH, r.mNHits, tParam.GetNDF(), tParam.GetChi2(), tParam.GetSignCosPhi(),
          tParam.Cov()[0], tParam.Cov()[1], tParam.Cov()[2], tParam.Cov()[3], tParam.Cov()[4], tParam.Cov()[5], tParam.Cov()[6], tParam.Cov()[7], tParam.Cov()[8], tParam.Cov()[9],
          tParam.Cov()[10], tParam.Cov()[11], tParam.Cov()[12], tParam.Cov()[13], tParam.Cov()[14]);*/

  uint32_t hitout = CAMath::AtomicAdd(tracker.NRowHits(), nHits);
  if (hitout + nHits > tracker.NMaxRowHits()) {
    tracker.raiseError(GPUErrors::ERROR_TRACKLET_HIT_OVERFLOW, tracker.ISlice(), hitout + nHits, tracker.NMaxRowHits());
    CAMath::AtomicExch(tracker.NRowHits(), tracker.NMaxRowHits());
    return;
  }
  uint32_t itrout = CAMath::AtomicAdd(tracker.NTracklets(), 1u);
  if (itrout >= tracker.NMaxTracklets()) {
    tracker.raiseError(GPUErrors::ERROR_TRACKLET_OVERFLOW, tracker.ISlice(), itrout, tracker.NMaxTracklets());
    CAMath::AtomicExch(tracker.NTracklets(), tracker.NMaxTracklets());
    return;
  }

  GPUglobalref() GPUTPCTracklet& GPUrestrict() tracklet = tracker.Tracklets()[itrout];

  CADEBUG(printf("    Storing tracklet: %d rows\n", nHits));

  tracklet.SetFirstRow(r.mFirstRow);
  tracklet.SetLastRow(r.mLastRow);
  tracklet.SetFirstHit(hitout);
  tracklet.SetParam(tParam.GetParam());
  int32_t w = tracker.CalculateHitWeight(r.mNHits, tParam.GetChi2());
  tracklet.SetHitWeight(w);
#ifdef __HIPCC__ // Todo: fixme!
  for (int32_t iRow = r.mFirstRow - 1; ++iRow <= r.mLastRow; /*iRow++*/) {
#else
  for (int32_t iRow = r.mFirstRow; iRow <= r.mLastRow; iRow++) {
#endif
    calink ih = rowHits[iRow];
    tracker.TrackletRowHits()[hitout + (iRow - r.mFirstRow)] = ih;
    if (ih != CALINK_INVAL && ih != CALINK_DEAD_CHANNEL) {
      CA_MAKE_SHARED_REF(GPUTPCRow, row, tracker.Row(iRow), s.mRows[iRow]);
      tracker.MaximizeHitWeight(row, ih, w);
    }
  }
}

template <class T>
GPUdic(2, 1) void GPUTPCTrackletConstructor::UpdateTracklet(int32_t /*nBlocks*/, int32_t /*nThreads*/, int32_t /*iBlock*/, int32_t /*iThread*/, GPUsharedref() T& s, GPUTPCThreadMemory& GPUrestrict() r, GPUconstantref() GPUTPCTracker& GPUrestrict() tracker, GPUTPCTrackParam& GPUrestrict() tParam, int32_t iRow, calink& rowHit, calink* rowHits)
{
  // reconstruction of tracklets, tracklets update step
  CA_MAKE_SHARED_REF(GPUTPCRow, row, tracker.Row(iRow), s.mRows[iRow]);

  float y0 = row.Grid().YMin();
  float stepY = row.HstepY();
  float z0 = row.Grid().ZMin() - tParam.ZOffset();
  float stepZ = row.HstepZ();

  if (r.mStage == 0) { // fitting part
    do {
      if (iRow < r.mStartRow || r.mCurrIH == CALINK_INVAL) {
        break;
      }
      if ((iRow - r.mStartRow) & 1) {
        rowHit = CALINK_INVAL;
        break; // SG!!! - jump over the row
      }

      cahit2 hh = CA_TEXTURE_FETCH(cahit22, gAliTexRefu2, tracker.HitData(row), r.mCurrIH);

      int32_t seedIH = r.mCurrIH;
      r.mCurrIH = CA_TEXTURE_FETCH(calink, gAliTexRefs, tracker.HitLinkUpData(row), r.mCurrIH);

      float x = row.X();
      float y = y0 + hh.x * stepY;
      float z = z0 + hh.y * stepZ;
      if (iRow != r.mStartRow || !tracker.Param().par.continuousTracking) {
        tParam.ConstrainZ(z, tracker.ISlice(), z0, r.mLastZ);
        tracker.GetConstantMem()->calibObjects.fastTransformHelper->TransformXYZ(tracker.ISlice(), iRow, x, y, z);
      }
      if (iRow == r.mStartRow) {
        if (tracker.Param().par.continuousTracking) {
          float refZ = ((z > 0) ? tracker.Param().rec.tpc.defaultZOffsetOverR : -tracker.Param().rec.tpc.defaultZOffsetOverR) * x;
          float zTmp = refZ;
          tracker.GetConstantMem()->calibObjects.fastTransformHelper->TransformXYZ(tracker.ISlice(), iRow, x, y, zTmp);
          z += zTmp - refZ; // Add zCorrection (=zTmp - refZ) to z, such that zOffset is set such, that transformed (z - zOffset) becomes refZ
          tParam.SetZOffset(z - refZ);
          tParam.SetZ(refZ);
          r.mLastZ = refZ;
        } else {
          tParam.SetZ(z);
          r.mLastZ = z;
          tParam.SetZOffset(0.f);
        }
        tParam.SetX(x);
        tParam.SetY(y);
        r.mLastY = y;
        CADEBUG(printf("Tracklet %5d: FIT INIT  ROW %3d X %8.3f -", r.mISH, iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));
      } else {
        float dx = x - tParam.X();
        float dy, dz;
        if (r.mNHits >= 10) {
          dy = y - tParam.Y();
          dz = z - tParam.Z();
        } else {
          dy = y - r.mLastY;
          dz = z - r.mLastZ;
        }
        r.mLastY = y;
        r.mLastZ = z;

        float ri = 1.f / CAMath::Sqrt(dx * dx + dy * dy);
        if (iRow == r.mStartRow + 2) {
          tParam.SetSinPhi(dy * ri);
          tParam.SetSignCosPhi(dx);
          tParam.SetDzDs(dz * ri);
          float err2Y, err2Z;
          tracker.GetErrors2Seeding(iRow, tParam, -1.f, err2Y, err2Z); // Use correct time
          tParam.SetCov(0, err2Y);
          tParam.SetCov(2, err2Z);
        }
        float sinPhi, cosPhi;
        if (r.mNHits >= 10 && CAMath::Abs(tParam.SinPhi()) < GPUCA_MAX_SIN_PHI_LOW) {
          sinPhi = tParam.SinPhi();
          cosPhi = CAMath::Sqrt(1 - sinPhi * sinPhi);
        } else {
          sinPhi = dy * ri;
          cosPhi = dx * ri;
        }
        CADEBUG(printf("%14s: FIT TRACK ROW %3d X %8.3f -", "", iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));
        if (!tParam.TransportToX(x, sinPhi, cosPhi, tracker.Param().bzCLight, GPUCA_MAX_SIN_PHI)) {
          rowHit = CALINK_INVAL;
          break;
        }
        CADEBUG(printf("%5s hits %3d: FIT PROP  ROW %3d X %8.3f -", "", r.mNHits, iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));
        float err2Y, err2Z;
        tracker.GetErrors2Seeding(iRow, tParam.GetZ(), sinPhi, tParam.GetDzDs(), -1.f, err2Y, err2Z); // TODO: Use correct time

        if (r.mNHits >= 10) {
          const float sErr2 = tracker.Param().GetSystematicClusterErrorIFC2(x, tParam.GetY(), tParam.GetZ(), tracker.ISlice() >= 18);
          err2Y += sErr2;
          err2Z += sErr2;
          const float kFactor = tracker.Param().rec.tpc.hitPickUpFactor * tracker.Param().rec.tpc.hitPickUpFactor * 3.5f * 3.5f;
          float sy2 = kFactor * (tParam.Err2Y() + err2Y);
          float sz2 = kFactor * (tParam.Err2Z() + err2Z);
          if (sy2 > tracker.Param().rec.tpc.hitSearchArea2) {
            sy2 = tracker.Param().rec.tpc.hitSearchArea2;
          }
          if (sz2 > tracker.Param().rec.tpc.hitSearchArea2) {
            sz2 = tracker.Param().rec.tpc.hitSearchArea2;
          }
          dy = y - tParam.Y();
          dz = z - tParam.Z();
          if (dy * dy > sy2 || dz * dz > sz2) {
            if (++r.mNMissed >= tracker.Param().rec.tpc.trackFollowingMaxRowGapSeed) {
              r.mCurrIH = CALINK_INVAL;
            }
            rowHit = CALINK_INVAL;
            break;
          }
        }

        if (!tParam.Filter(y, z, err2Y, err2Z, GPUCA_MAX_SIN_PHI_LOW)) {
          rowHit = CALINK_INVAL;
          break;
        }
        CADEBUG(printf("%14s: FIT FILT  ROW %3d X %8.3f -", "", iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));
      }
      rowHit = seedIH;
      r.mNHitsEndRow = ++r.mNHits;
      r.mLastRow = iRow;
      r.mEndRow = iRow;
      r.mNMissed = 0;
    } while (0);

    /*printf("Extrapolate Row %d X %f Y %f Z %f SinPhi %f DzDs %f QPt %f", iRow, tParam.X(), tParam.Y(), tParam.Z(), tParam.SinPhi(), tParam.DzDs(), tParam.QPt());
            for (int32_t i = 0;i < 15;i++) printf(" C%d=%6.2f", i, tParam.GetCov(i));
            printf("\n");*/

    if (r.mCurrIH == CALINK_INVAL) {
      r.mStage = 1;
      r.mLastY = tParam.Y(); // Store last spatial position here to start inward following from here
      r.mLastZ = tParam.Z();
      if (CAMath::Abs(tParam.SinPhi()) > GPUCA_MAX_SIN_PHI) {
        r.mGo = 0;
      }
    }
  } else { // forward/backward searching part
    do {
      if (r.mStage == 2 && iRow > r.mEndRow) {
        break;
      }
      if (r.mNMissed > tracker.Param().rec.tpc.trackFollowingMaxRowGap) {
        r.mGo = 0;
        break;
      }

      r.mNMissed++;

      float x = row.X();
      {
        float tmpY, tmpZ;
        if (!tParam.GetPropagatedYZ(tracker.Param().bzCLight, x, tmpY, tmpZ)) {
          r.mGo = 0;
          rowHit = CALINK_INVAL;
          break;
        }
        tParam.ConstrainZ(tmpZ, tracker.ISlice(), z0, r.mLastZ);
        tracker.GetConstantMem()->calibObjects.fastTransformHelper->InverseTransformYZtoX(tracker.ISlice(), iRow, tmpY, tmpZ, x);
      }

      CADEBUG(printf("%14s: SEA TRACK ROW %3d X %8.3f -", "", iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));
      if (!tParam.TransportToX(x, tParam.SinPhi(), tParam.GetCosPhi(), tracker.Param().bzCLight, GPUCA_MAX_SIN_PHI_LOW)) {
        r.mGo = 0;
        rowHit = CALINK_INVAL;
        break;
      }
      CADEBUG(printf("%14s: SEA PROP  ROW %3d X %8.3f -", "", iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));

      bool found = false;
      float yUncorrected = tParam.GetY(), zUncorrected = tParam.GetZ();
      do {
        if (row.NHits() < 1) {
          rowHit = CALINK_INVAL;
          break;
        }

#ifndef GPUCA_TEXTURE_FETCH_CONSTRUCTOR
        GPUglobalref() const cahit2* hits = tracker.HitData(row);
        GPUglobalref() const calink* firsthit = tracker.FirstHitInBin(row);
#endif //! GPUCA_TEXTURE_FETCH_CONSTRUCTOR
        tracker.GetConstantMem()->calibObjects.fastTransformHelper->InverseTransformYZtoNominalYZ(tracker.ISlice(), iRow, yUncorrected, zUncorrected, yUncorrected, zUncorrected);

        if (tracker.Param().rec.tpc.rejectEdgeClustersInSeeding && tracker.Param().rejectEdgeClusterByY(yUncorrected, iRow, CAMath::Sqrt(tParam.Err2Y()))) {
          rowHit = CALINK_INVAL;
          break;
        }
        calink best = CALINK_INVAL;

        float err2Y, err2Z;
        tracker.GetErrors2Seeding(iRow, *((GPUTPCTrackParam*)&tParam), -1.f, err2Y, err2Z); // TODO: Use correct time
        if (r.mNHits >= 10) {
          const float sErr2 = tracker.Param().GetSystematicClusterErrorIFC2(x, tParam.GetY(), tParam.GetZ(), tracker.ISlice() >= 18);
          err2Y += sErr2;
          err2Z += sErr2;
        }
        if (CAMath::Abs(yUncorrected) < x * GPUTPCRow::getTPCMaxY1X()) { // search for the closest hit
          const float kFactor = tracker.Param().rec.tpc.hitPickUpFactor * tracker.Param().rec.tpc.hitPickUpFactor * 7.0f * 7.0f;
          const float maxWindow2 = tracker.Param().rec.tpc.hitSearchArea2;
          const float sy2 = CAMath::Min(maxWindow2, kFactor * (tParam.Err2Y() + err2Y));
          const float sz2 = CAMath::Min(maxWindow2, kFactor * (tParam.Err2Z() + err2Z));

          int32_t bin, ny, nz;
          row.Grid().GetBinArea(yUncorrected, zUncorrected + tParam.ZOffset(), CAMath::Sqrt(sy2), CAMath::Sqrt(sz2), bin, ny, nz);
          float ds = 1e6f;

#ifdef __HIPCC__ // Todo: fixme!
          for (int32_t k = -1; ++k <= nz; /*k++*/) {
#else
          for (int32_t k = 0; k <= nz; k++) {
#endif
            int32_t nBinsY = row.Grid().Ny();
            int32_t mybin = bin + k * nBinsY;
            uint32_t hitFst = CA_TEXTURE_FETCH(calink, gAliTexRefu, firsthit, mybin);
            uint32_t hitLst = CA_TEXTURE_FETCH(calink, gAliTexRefu, firsthit, mybin + ny + 1);
#ifdef __HIPCC__ // Todo: fixme!
            for (uint32_t ih = hitFst - 1; ++ih < hitLst; /*ih++*/) {
#else
            for (uint32_t ih = hitFst; ih < hitLst; ih++) {
#endif
              cahit2 hh = CA_TEXTURE_FETCH(cahit2, gAliTexRefu2, hits, ih);
              float y = y0 + hh.x * stepY;
              float z = z0 + hh.y * stepZ;
              float dy = y - yUncorrected;
              float dz = z - zUncorrected;
              if (dy * dy < sy2 && dz * dz < sz2) {
                float dds = tracker.Param().rec.tpc.trackFollowingYFactor * CAMath::Abs(dy) + CAMath::Abs(dz);
                if (dds < ds) {
                  ds = dds;
                  best = ih;
                }
              }
            }
          }
        } // end of search for the closest hit

        if (best == CALINK_INVAL) {
          if (r.mNHits == 0 && r.mStage < 3) {
            if (rowHit == CALINK_INVAL || rowHit == CALINK_DEAD_CHANNEL) {
              break;
            }
            best = rowHit;
          } else {
            rowHit = CALINK_INVAL;
            break;
          }
        }

        cahit2 hh = CA_TEXTURE_FETCH(cahit2, gAliTexRefu2, hits, best);
        float y = y0 + hh.x * stepY + tParam.GetY() - yUncorrected;
        float z = z0 + hh.y * stepZ + tParam.GetZ() - zUncorrected;

        CADEBUG(printf("%14s: SEA Hit %5d (%8.3f %8.3f), Res %f %f\n", "", best, y, z, tParam.Y() - y, tParam.Z() - z));

        calink oldHit = (r.mStage == 2 && iRow >= r.mStartRow) ? rowHit : CALINK_INVAL;
        if (oldHit != best && !tParam.Filter(y, z, err2Y, err2Z, GPUCA_MAX_SIN_PHI_LOW, oldHit != CALINK_INVAL) && r.mNHits != 0) {
          rowHit = CALINK_INVAL;
          break;
        }
        found = true;
        rowHit = best;
        r.mNHits++;
        r.mNMissed = 0;
        CADEBUG(printf("%5s hits %3d: SEA FILT  ROW %3d X %8.3f -", "", r.mNHits, iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));
        if (r.mStage == 1) {
          r.mLastRow = iRow;
        } else {
          r.mFirstRow = iRow;
        }
      } while (false);
      (void)found;
#if defined(GPUCA_HAVE_O2HEADERS)
      if (!found && tracker.GetConstantMem()->calibObjects.dEdxCalibContainer) {
        uint32_t pad = CAMath::Float2UIntRn(tracker.Param().tpcGeometry.LinearY2Pad(tracker.ISlice(), iRow, yUncorrected));
        if (pad < tracker.Param().tpcGeometry.NPads(iRow) && tracker.GetConstantMem()->calibObjects.dEdxCalibContainer->isDead(tracker.ISlice(), iRow, pad)) {
          r.mNMissed--;
          rowHit = CALINK_DEAD_CHANNEL;
        }
      }
#endif
    } while (0);
  }
  if (r.mNHits == 8 && r.mNMissed == 0 && rowHit != CALINK_INVAL && rowHit != CALINK_DEAD_CHANNEL && rowHits && tracker.Param().par.continuousTracking && rowHits[r.mFirstRow] != CALINK_INVAL && rowHits[r.mFirstRow] != CALINK_DEAD_CHANNEL && rowHits[r.mLastRow] != CALINK_INVAL && rowHits[r.mLastRow] != CALINK_DEAD_CHANNEL) {
    const GPUglobalref() GPUTPCRow& GPUrestrict() row1 = tracker.Row(r.mFirstRow);
    const GPUglobalref() GPUTPCRow& GPUrestrict() row2 = tracker.Row(r.mLastRow);
    GPUglobalref() const cahit2* hits1 = tracker.HitData(row1);
    GPUglobalref() const cahit2* hits2 = tracker.HitData(row2);
    const cahit2 hh1 = CA_TEXTURE_FETCH(cahit2, gAliTexRefu2, hits1, rowHits[r.mFirstRow]);
    const cahit2 hh2 = CA_TEXTURE_FETCH(cahit2, gAliTexRefu2, hits2, rowHits[r.mLastRow]);
    const float z1 = row1.Grid().ZMin() + hh1.y * row1.HstepZ();
    const float z2 = row2.Grid().ZMin() + hh2.y * row2.HstepZ();
    float oldOffset = tParam.ZOffset();
    tParam.ShiftZ(z1, z2, tracker.Param().tpcGeometry.Row2X(r.mFirstRow), tracker.Param().tpcGeometry.Row2X(r.mLastRow), tracker.Param().bzCLight, tracker.Param().rec.tpc.defaultZOffsetOverR);
    r.mLastZ -= tParam.ZOffset() - oldOffset;
    CADEBUG(printf("Shifted z from %f to %f\n", oldOffset, tParam.ZOffset()));
  }
}

GPUdic(2, 1) void GPUTPCTrackletConstructor::DoTracklet(GPUconstantref() GPUTPCTracker& GPUrestrict() tracker, GPUsharedref() GPUTPCTrackletConstructor::GPUSharedMemory& s, GPUTPCThreadMemory& GPUrestrict() r)
{
  int32_t iRow = 0, iRowEnd = GPUCA_ROW_COUNT;
  GPUTPCTrackParam tParam;
  calink rowHits[GPUCA_ROW_COUNT];
  if (r.mGo) {
    GPUTPCHitId id = tracker.TrackletStartHits()[r.mISH];

    r.mStartRow = r.mEndRow = r.mFirstRow = r.mLastRow = id.RowIndex();
    r.mCurrIH = id.HitIndex();
    r.mNMissed = 0;
    iRow = r.mStartRow;
    GPUTPCTrackletConstructor::InitTracklet(tParam);
  }
  r.mStage = 0;
  r.mNHits = 0;
  CADEBUG(printf("Start tracklet\n"));

#ifdef __HIPCC__ // Todo: fixme!
  for (int32_t iStage = -1; ++iStage < 2; /*iStage++*/) {
#else
  for (int32_t iStage = 0; iStage < 2; iStage++) {
#endif
    for (; iRow != iRowEnd; iRow += r.mStage == 2 ? -1 : 1) {
      if (!r.mGo) {
        break;
      }
      UpdateTracklet(0, 0, 0, 0, s, r, tracker, tParam, iRow, rowHits[iRow], rowHits);
    }
    if (!r.mGo && r.mStage == 2) {
      for (; iRow >= r.mStartRow; iRow--) {
        rowHits[iRow] = CALINK_INVAL;
      }
    }
    if (r.mStage == 2) {
      StoreTracklet(0, 0, 0, 0, s, r, tracker, tParam, rowHits);
    } else {
      r.mStage = 2;
      r.mNMissed = 0;
      iRow = r.mEndRow;
      iRowEnd = -1;
      float x = tracker.Row(r.mEndRow).X();
      {
        float tmpY, tmpZ;
        if (tParam.GetPropagatedYZ(tracker.Param().bzCLight, x, tmpY, tmpZ)) {
          if (tracker.ISlice() < GPUCA_NSLICES / 2 ? (tmpZ < 0) : (tmpZ > 0)) {
            tmpZ = 0;
          } else if (tracker.ISlice() < GPUCA_NSLICES / 2 ? (tmpZ > GPUTPCGeometry::TPCLength()) : (tmpZ < -GPUTPCGeometry::TPCLength())) {
            tmpZ = tracker.ISlice() < GPUCA_NSLICES / 2 ? GPUTPCGeometry::TPCLength() : -GPUTPCGeometry::TPCLength();
          }
          tracker.GetConstantMem()->calibObjects.fastTransformHelper->InverseTransformYZtoX(tracker.ISlice(), iRow, tmpY, tmpZ, x);
        } else {
          r.mGo = 0;
          continue;
        }
      }
      if ((r.mGo = (tParam.TransportToX(x, tracker.Param().bzCLight, GPUCA_MAX_SIN_PHI) && tParam.Filter(r.mLastY, r.mLastZ, tParam.Err2Y() * 0.5f, tParam.Err2Z() * 0.5f, GPUCA_MAX_SIN_PHI_LOW, true)))) {
        CADEBUG(printf("%14s: SEA BACK  ROW %3d X %8.3f -", "", iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));
        float err2Y, err2Z;
        tracker.GetErrors2Seeding(r.mEndRow, tParam, -1.f, err2Y, err2Z); // TODO: Use correct time
        if (tParam.GetCov(0) < err2Y) {
          tParam.SetCov(0, err2Y);
        }
        if (tParam.GetCov(2) < err2Z) {
          tParam.SetCov(2, err2Z);
        }
        CADEBUG(printf("%14s: SEA ADJUS ROW %3d X %8.3f -", "", iRow, tParam.X()); for (int32_t i = 0; i < 5; i++) { printf(" %8.3f", tParam.Par()[i]); } printf(" -"); for (int32_t i = 0; i < 15; i++) { printf(" %8.3f", tParam.Cov()[i]); } printf("\n"));
        r.mNHits -= r.mNHitsEndRow;
      }
    }
  }
  CADEBUG(printf("End tracklet\n"));
}

template <>
GPUdii() void GPUTPCTrackletConstructor::Thread<GPUTPCTrackletConstructor::singleSlice>(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& sMem, processorType& GPUrestrict() tracker)
{
  if (get_local_id(0) == 0) {
    sMem.mNStartHits = *tracker.NStartHits();
  }
  CA_SHARED_CACHE(&sMem.mRows[0], tracker.SliceDataRows(), GPUCA_ROW_COUNT * sizeof(GPUTPCRow));
  GPUbarrier();

  GPUTPCThreadMemory rMem;
  for (rMem.mISH = get_global_id(0); rMem.mISH < sMem.mNStartHits; rMem.mISH += get_global_size(0)) {
    rMem.mGo = 1;
    DoTracklet(tracker, sMem, rMem);
  }
}

template <>
GPUdii() void GPUTPCTrackletConstructor::Thread<GPUTPCTrackletConstructor::allSlices>(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& sMem, processorType& GPUrestrict() tracker0)
{
  GPUconstantref() GPUTPCTracker* GPUrestrict() pTracker = &tracker0;
#ifdef GPUCA_GPUCODE
  int32_t mySlice = get_group_id(0) % GPUCA_NSLICES;
  int32_t currentSlice = -1;

  if (get_local_id(0) == 0) {
    sMem.mNextStartHitFirstRun = 1;
  }
  GPUCA_UNROLL(, U())
  for (uint32_t iSlice = 0; iSlice < GPUCA_NSLICES; iSlice++) {
    GPUconstantref() GPUTPCTracker& GPUrestrict() tracker = pTracker[mySlice];

    GPUTPCThreadMemory rMem;

    while ((rMem.mISH = FetchTracklet(tracker, sMem)) != -2) {
      if (rMem.mISH >= 0 && get_local_id(0) < GPUCA_GET_THREAD_COUNT(GPUCA_LB_GPUTPCTrackletConstructor)) {
        rMem.mISH += get_local_id(0);
      } else {
        rMem.mISH = -1;
      }

      if (mySlice != currentSlice) {
        if (get_local_id(0) == 0) {
          sMem.mNStartHits = *tracker.NStartHits();
        }
        CA_SHARED_CACHE(&sMem.mRows[0], tracker.SliceDataRows(), GPUCA_ROW_COUNT * sizeof(GPUTPCRow));
        GPUbarrier();
        currentSlice = mySlice;
      }

      if (rMem.mISH >= 0 && rMem.mISH < sMem.mNStartHits) {
        rMem.mGo = true;
        DoTracklet(tracker, sMem, rMem);
      }
    }
    if (++mySlice >= GPUCA_NSLICES) {
      mySlice = 0;
    }
  }
#else
  for (int32_t iSlice = 0; iSlice < GPUCA_NSLICES; iSlice++) {
    Thread<singleSlice>(nBlocks, nThreads, iBlock, iThread, sMem, pTracker[iSlice]);
  }
#endif
}

#ifdef GPUCA_GPUCODE

GPUd() int32_t GPUTPCTrackletConstructor::FetchTracklet(GPUconstantref() GPUTPCTracker& GPUrestrict() tracker, GPUsharedref() GPUSharedMemory& sMem)
{
  const uint32_t nStartHit = *tracker.NStartHits();
  GPUbarrier();
  if (get_local_id(0) == 0) {
    int32_t firstStartHit = -2;
    if (sMem.mNextStartHitFirstRun == 1) {
      firstStartHit = (get_group_id(0) - tracker.ISlice()) / GPUCA_NSLICES * GPUCA_GET_THREAD_COUNT(GPUCA_LB_GPUTPCTrackletConstructor);
      sMem.mNextStartHitFirstRun = 0;
    } else {
      if (tracker.GPUParameters()->nextStartHit < nStartHit) {
        firstStartHit = CAMath::AtomicAdd<uint32_t>(&tracker.GPUParameters()->nextStartHit, GPUCA_GET_THREAD_COUNT(GPUCA_LB_GPUTPCTrackletConstructor));
      }
    }
    sMem.mNextStartHitFirst = firstStartHit < (int32_t)nStartHit ? firstStartHit : -2;
  }
  GPUbarrier();
  return (sMem.mNextStartHitFirst);
}

#endif // GPUCA_GPUCODE

template <> // FIXME: GPUgeneric() needed to make the clang spirv output link correctly
GPUd() int32_t GPUTPCTrackletConstructor::GPUTPCTrackletConstructorGlobalTracking<GPUgeneric() GPUTPCGlobalTracking::GPUSharedMemory>(GPUconstantref() GPUTPCTracker& GPUrestrict() tracker, GPUsharedref() GPUTPCGlobalTracking::GPUSharedMemory& sMem, GPUTPCTrackParam& GPUrestrict() tParam, int32_t row, int32_t increment, int32_t iTracklet, calink* rowHits)
{
  GPUTPCThreadMemory rMem;
  rMem.mISH = iTracklet;
  rMem.mStage = 3;
  rMem.mNHits = rMem.mNMissed = 0;
  rMem.mGo = 1;
  while (rMem.mGo && row >= 0 && row < GPUCA_ROW_COUNT) {
    UpdateTracklet(1, 1, 0, 0, sMem, rMem, tracker, tParam, row, rowHits[row], nullptr);
    row += increment;
  }
  if (!CheckCov(tParam)) {
    rMem.mNHits = 0;
  }
  return (rMem.mNHits);
}
