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

/// \file GPUCommonRtypes.h
/// \author David Rohr

#ifndef GPUCOMMONRTYPES_H
#define GPUCOMMONRTYPES_H

#include "GPUCommonDef.h"

#if defined(GPUCA_STANDALONE) || (defined(GPUCA_O2_LIB) && !defined(GPUCA_O2_INTERFACE) && !defined(DEBUG_STREAMER)) || defined(GPUCA_GPUCODE) // clang-format off
  #if !defined(ROOT_Rtypes) && !defined(__CLING__)
    #define GPUCOMMONRTYPES_H_ACTIVE
    struct MUST_NOT_USE_Rtypes_h {};
    typedef MUST_NOT_USE_Rtypes_h TClass;
    #define ClassDef(name,id)
    #define ClassDefNV(name, id)
    #define ClassDefOverride(name, id)
    #define ClassImp(name)
    #define templateClassImp(name)
    #ifndef GPUCA_GPUCODE_DEVICE
      #include <iostream>
    #endif
  #endif
#else
  #include "Rtypes.h"
#endif // clang-format off

#endif
