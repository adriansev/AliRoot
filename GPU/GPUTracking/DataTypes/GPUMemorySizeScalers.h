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

/// \file GPUMemorySizeScalers.h
/// \author David Rohr

#ifndef O2_GPU_GPUMEMORYSIZESCALERS_H
#define O2_GPU_GPUMEMORYSIZESCALERS_H

#include "GPUDef.h"

namespace GPUCA_NAMESPACE::gpu
{

struct GPUMemorySizeScalers {
  // Input sizes
  size_t nTPCdigits = 0;
  size_t nTPCHits = 0;
  size_t nTRDTracklets = 0;
  size_t nITSTracks = 0;

  // General scaling factor
  double factor = 1;
  double temporaryFactor = 1;
  bool conservative = 0;

  // Offset
  double offset = 1000.;
  double hitOffset = 20000;

  // Scaling Factors
  double tpcPeaksPerDigit = 0.2;
  double tpcClustersPerPeak = 0.9;
  double tpcStartHitsPerHit = 0.08;
  double tpcTrackletsPerStartHit = 0.8;
  double tpcTrackletHitsPerHit = 5;
  double tpcSectorTracksPerHit = 0.02;
  double tpcSectorTrackHitsPerHit = 0.8;
  double tpcSectorTrackHitsPerHitWithRejection = 1.0;
  double tpcMergedTrackPerSliceTrack = 0.9;
  double tpcMergedTrackHitPerSliceHit = 1.1;
  size_t tpcCompressedUnattachedHitsBase1024[3] = {900, 900, 500}; // No ratio, but integer fraction of 1024 for exact computation

  // Upper limits
  size_t tpcMaxPeaks = 20000000;
  size_t tpcMaxClusters = 320000000;
  size_t tpcMaxSectorClusters = 30000000;
  size_t tpcMaxStartHits = 650000;
  size_t tpcMaxRowStartHits = 100000;
  size_t tpcMinRowStartHits = 4000;
  size_t tpcMaxTracklets = 520000;
  size_t tpcMaxTrackletHits = 35000000;
  size_t tpcMaxSectorTracks = 130000;
  size_t tpcMaxSectorTrackHits = 5900000;
  size_t tpcMaxMergedTracks = 3000000;
  size_t tpcMaxMergedTrackHits = 200000000;
  size_t availableMemory = 20500000000;
  bool returnMaxVal = false;

  void rescaleMaxMem(size_t newAvailableMemory);
  inline size_t getValue(size_t maxVal, size_t val)
  {
    return returnMaxVal ? maxVal : (std::min<size_t>(maxVal, offset + val) * factor * temporaryFactor);
  }

  inline size_t NTPCPeaks(size_t tpcDigits, bool perSector = false) { return getValue(perSector ? tpcMaxPeaks : (GPUCA_NSLICES * tpcMaxPeaks), hitOffset + tpcDigits * tpcPeaksPerDigit); }
  inline size_t NTPCClusters(size_t tpcDigits, bool perSector = false) { return getValue(perSector ? tpcMaxSectorClusters : tpcMaxClusters, (conservative ? 1.0 : tpcClustersPerPeak) * NTPCPeaks(tpcDigits, perSector)); }
  inline size_t NTPCStartHits(size_t tpcHits) { return getValue(tpcMaxStartHits, tpcHits * tpcStartHitsPerHit); }
  inline size_t NTPCRowStartHits(size_t tpcHits) { return getValue(tpcMaxRowStartHits, std::max<size_t>(NTPCStartHits(tpcHits) * (tpcHits < 30000000 ? 20 : 12) / GPUCA_ROW_COUNT, tpcMinRowStartHits)); }
  inline size_t NTPCTracklets(size_t tpcHits) { return getValue(tpcMaxTracklets, NTPCStartHits(tpcHits) * tpcTrackletsPerStartHit); }
  inline size_t NTPCTrackletHits(size_t tpcHits) { return getValue(tpcMaxTrackletHits, hitOffset + tpcHits * tpcTrackletHitsPerHit); }
  inline size_t NTPCSectorTracks(size_t tpcHits) { return getValue(tpcMaxSectorTracks, tpcHits * tpcSectorTracksPerHit); }
  inline size_t NTPCSectorTrackHits(size_t tpcHits, uint8_t withRejection = 0) { return getValue(tpcMaxSectorTrackHits, tpcHits * (withRejection ? tpcSectorTrackHitsPerHitWithRejection : tpcSectorTrackHitsPerHit)); }
  inline size_t NTPCMergedTracks(size_t tpcSliceTracks) { return getValue(tpcMaxMergedTracks, tpcSliceTracks * (conservative ? 1.0 : tpcMergedTrackPerSliceTrack)); }
  inline size_t NTPCMergedTrackHits(size_t tpcSliceTrackHitss) { return getValue(tpcMaxMergedTrackHits, tpcSliceTrackHitss * tpcMergedTrackHitPerSliceHit); }
  inline size_t NTPCUnattachedHitsBase1024(int32_t type) { return (returnMaxVal || conservative) ? 1024 : std::min<size_t>(1024, tpcCompressedUnattachedHitsBase1024[type] * factor * temporaryFactor); }
};

} // namespace GPUCA_NAMESPACE::gpu

#endif
