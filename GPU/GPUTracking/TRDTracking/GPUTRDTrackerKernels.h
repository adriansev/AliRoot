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

/// \file GPUTRDTrackerKernels.h
/// \author David Rohr

#ifndef GPUTRDTRACKERKERNELSCA_H
#define GPUTRDTRACKERKERNELSCA_H

#include "GPUGeneralKernels.h"

namespace GPUCA_NAMESPACE
{
namespace gpu
{

class GPUTRDTrackerKernels : public GPUKernelTemplate
{
 public:
  enum K { defaultKernel = 0,
           gpuVersion = 0,
           o2Version = 1 };
  GPUhdi() constexpr static GPUDataTypes::RecoStep GetRecoStep() { return GPUCA_RECO_STEP::TRDTracking; }
  template <int32_t iKernel = defaultKernel, class T>
  GPUd() static void Thread(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& smem, processorType& processors, T* externalInstance = nullptr);
};
} // namespace gpu
} // namespace GPUCA_NAMESPACE

#endif // GPUTRDTRACKERKERNELSCA_H
