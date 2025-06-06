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

/// \file GPUReconstructionKernelList.h
/// \author David Rohr

// No header protection, this may be used multiple times
#include "GPUReconstructionKernelMacros.h"

#if !defined(GPUCA_ALIROOT_LIB) || !defined(GPUCA_GPUCODE)
#define GPUCA_KRNL_NOALIROOT
#endif

// clang-format off
$<JOIN:$<TARGET_PROPERTY:O2_GPU_KERNELS,O2_GPU_KERNELS>,>
// clang-format on

#ifdef GPUCA_KRNL_NOALIROOT
#undef GPUCA_KRNL_NOALIROOT
#endif
