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

/// \file GPUTPCDecompression.h
/// \author Gabriele Cimador

#ifndef GPUTPCDECOMPRESSION_H
#define GPUTPCDECOMPRESSION_H

#include "GPUDef.h"
#include "GPUProcessor.h"
#include "GPUCommonMath.h"
#include "GPUParam.h"
#include "GPUO2DataTypes.h"

#ifdef GPUCA_HAVE_O2HEADERS
#include "DataFormatsTPC/CompressedClusters.h"
#else
namespace o2::tpc
{
struct CompressedClustersPtrs {
};
struct CompressedClusters {
};
struct CompressedClustersFlat {
};
} // namespace o2::tpc
#endif

namespace GPUCA_NAMESPACE::gpu
{

class GPUTPCDecompression : public GPUProcessor
{
  friend class GPUTPCDecompressionKernels;
  friend class GPUTPCDecompressionUtilKernels;
  friend class GPUChainTracking;
  friend class TPCClusterDecompressionCore;

 public:
#ifndef GPUCA_GPUCODE
  void InitializeProcessor();
  void RegisterMemoryAllocation();
  void SetMaxData(const GPUTrackingInOutPointers& io);

  void* SetPointersInputGPU(void* mem);
  void* SetPointersTmpNativeBuffersGPU(void* mem);
  void* SetPointersTmpNativeBuffersOutput(void* mem);
  void* SetPointersTmpNativeBuffersInput(void* mem);
  void* SetPointersTmpClusterNativeAccessForFiltering(void* mem);
  void* SetPointersInputClusterNativeAccess(void* mem);
  void* SetPointersNClusterPerSectorRow(void* mem);

#endif

 protected:
  constexpr static uint32_t NSLICES = GPUCA_NSLICES;
  o2::tpc::CompressedClusters mInputGPU;

  uint32_t mMaxNativeClustersPerBuffer;
  uint32_t mNClusterNativeBeforeFiltering;
  uint32_t* mNativeClustersIndex;
  uint32_t* mUnattachedClustersOffsets;
  uint32_t* mAttachedClustersOffsets;
  uint32_t* mNClusterPerSectorRow;
  o2::tpc::ClusterNative* mTmpNativeClusters;
  o2::tpc::ClusterNative* mNativeClustersBuffer;
  o2::tpc::ClusterNativeAccess* mClusterNativeAccess;

  template <class T>
  void SetPointersCompressedClusters(void*& mem, T& c, uint32_t nClA, uint32_t nTr, uint32_t nClU, bool reducedClA);

  int16_t mMemoryResInputGPU = -1;
  int16_t mResourceTmpIndexes = -1;
  int16_t mResourceTmpClustersOffsets = -1;
  int16_t mResourceTmpBufferBeforeFiltering = -1;
  int16_t mResourceClusterNativeAccess = -1;
  int16_t mResourceNClusterPerSectorRow = -1;
};
} // namespace GPUCA_NAMESPACE::gpu
#endif // GPUTPCDECOMPRESSION_H
