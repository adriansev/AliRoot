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

/// \file PeakFinder.cxx
/// \author Felix Weiglhofer

#include "GPUTPCCFPeakFinder.h"

#include "Array2D.h"
#include "CfUtils.h"
#include "PackedCharge.h"
#include "TPCPadGainCalib.h"

using namespace GPUCA_NAMESPACE::gpu;
using namespace GPUCA_NAMESPACE::gpu::tpccf;

template <>
GPUdii() void GPUTPCCFPeakFinder::Thread<0>(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUSharedMemory& smem, processorType& clusterer)
{
  Array2D<PackedCharge> chargeMap(reinterpret_cast<PackedCharge*>(clusterer.mPchargeMap));
  Array2D<uint8_t> isPeakMap(clusterer.mPpeakMap);
  findPeaksImpl(get_num_groups(0), get_local_size(0), get_group_id(0), get_local_id(0), smem, chargeMap, clusterer.mPpadIsNoisy, clusterer.mPpositions, clusterer.mPmemory->counters.nPositions, clusterer.Param().rec, *clusterer.GetConstantMem()->calibObjects.tpcPadGain, clusterer.mPisPeak, isPeakMap);
}

GPUdii() bool GPUTPCCFPeakFinder::isPeak(
  GPUSharedMemory& smem,
  Charge q,
  const ChargePos& pos,
  uint16_t N,
  const Array2D<PackedCharge>& chargeMap,
  const GPUSettingsRec& calib,
  ChargePos* posBcast,
  PackedCharge* buf)
{
  uint16_t ll = get_local_id(0);

  bool belowThreshold = (q <= calib.tpc.cfQMaxCutoff);

  uint16_t lookForPeaks;
  uint16_t partId = CfUtils::partition<SCRATCH_PAD_WORK_GROUP_SIZE>(
    smem,
    ll,
    belowThreshold,
    SCRATCH_PAD_WORK_GROUP_SIZE,
    &lookForPeaks);

  if (partId < lookForPeaks) {
    posBcast[partId] = pos;
  }
  GPUbarrier();

  CfUtils::blockLoad<PackedCharge>(
    chargeMap,
    lookForPeaks,
    SCRATCH_PAD_WORK_GROUP_SIZE,
    ll,
    0,
    N,
    cfconsts::InnerNeighbors,
    posBcast,
    buf);

  if (belowThreshold) {
    return false;
  }

  // Ensure q has the same float->int32_t->float conversion error
  // as values in chargeMap, so identical charges are actually identical
  q = PackedCharge(q).unpack();

  int32_t idx = N * partId;
  bool peak = true;
  peak = peak && buf[idx + 0].unpack() <= q;
  peak = peak && buf[idx + 1].unpack() <= q;
  peak = peak && buf[idx + 2].unpack() <= q;
  peak = peak && buf[idx + 3].unpack() <= q;
  peak = peak && buf[idx + 4].unpack() < q;
  peak = peak && buf[idx + 5].unpack() < q;
  peak = peak && buf[idx + 6].unpack() < q;
  peak = peak && buf[idx + 7].unpack() < q;

  return peak;
}

GPUd() void GPUTPCCFPeakFinder::findPeaksImpl(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUSharedMemory& smem,
                                              const Array2D<PackedCharge>& chargeMap,
                                              const uint8_t* padHasLostBaseline,
                                              const ChargePos* positions,
                                              SizeT digitnum,
                                              const GPUSettingsRec& calib,
                                              const TPCPadGainCalib& gainCorrection, // Only used for globalPad() function
                                              uint8_t* isPeakPredicate,
                                              Array2D<uint8_t>& peakMap)
{
  SizeT idx = get_global_id(0);

  // For certain configurations dummy work items are added, so the total
  // number of work items is dividable by 64.
  // These dummy items also compute the last digit but discard the result.
  ChargePos pos = positions[CAMath::Min(idx, (SizeT)(digitnum - 1))];
  Charge charge = pos.valid() ? chargeMap[pos].unpack() : Charge(0);

  bool hasLostBaseline = padHasLostBaseline[gainCorrection.globalPad(pos.row(), pos.pad())];
  charge = (hasLostBaseline) ? 0.f : charge;

  uint8_t peak = isPeak(smem, charge, pos, SCRATCH_PAD_SEARCH_N, chargeMap, calib, smem.posBcast, smem.buf);

  // Exit early if dummy. See comment above.
  bool iamDummy = (idx >= digitnum);
  if (iamDummy) {
    return;
  }

  isPeakPredicate[idx] = peak;

  if (pos.valid()) {
    peakMap[pos] = (uint8_t(charge > calib.tpc.cfInnerThreshold) << 1) | peak;
  }
}
