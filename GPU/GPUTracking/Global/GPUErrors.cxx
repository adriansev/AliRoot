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

/// \file GPUErrors.cxx
/// \author David Rohr

#include "GPUErrors.h"
#include "GPUDataTypes.h"
#include "GPUCommonMath.h"
#include "GPUDefMacros.h"
#include "GPULogging.h"

using namespace GPUCA_NAMESPACE::gpu;

#define GPUCA_MAX_ERRORS 255u

GPUd() void GPUErrors::raiseError(uint32_t code, uint32_t param1, uint32_t param2, uint32_t param3) const
{
  uint32_t pos = CAMath::AtomicAdd(mErrors, 1u);
  if (pos < GPUCA_MAX_ERRORS) {
    mErrors[4 * pos + 1] = code;
    mErrors[4 * pos + 2] = param1;
    mErrors[4 * pos + 3] = param2;
    mErrors[4 * pos + 4] = param3;
  }
}

#ifndef GPUCA_GPUCODE

#include <cstring>
#include <unordered_map>

uint32_t GPUErrors::getMaxErrors()
{
  return GPUCA_MAX_ERRORS;
}

void GPUErrors::clear()
{
  memset(mErrors, 0, GPUCA_MAX_ERRORS * sizeof(*mErrors));
}

static std::unordered_map<uint32_t, const char*> errorNames = {
#define GPUCA_ERROR_CODE(num, name, ...) {num, GPUCA_M_STR(name)},
#include "GPUErrorCodes.h"
#undef GPUCA_ERROR_CODE
};

void GPUErrors::printErrors(bool silent)
{
  for (uint32_t i = 0; i < std::min(*mErrors, GPUCA_MAX_ERRORS); i++) {
    uint32_t errorCode = mErrors[4 * i + 1];
    const auto& it = errorNames.find(errorCode);
    const char* errorName = it == errorNames.end() ? "INVALID ERROR CODE" : it->second;
    if (silent && i) {
      GPUWarning("GPU Error Code (%u:%u) %s : %u / %u / %u", i, errorCode, errorName, mErrors[4 * i + 2], mErrors[4 * i + 3], mErrors[4 * i + 4]);
    } else if (silent) {
      GPUAlarm("GPU Error Code (%u:%u) %s : %u / %u / %u", i, errorCode, errorName, mErrors[4 * i + 2], mErrors[4 * i + 3], mErrors[4 * i + 4]);
    } else {
      GPUError("GPU Error Code (%u:%u) %s : %u / %u / %u", i, errorCode, errorName, mErrors[4 * i + 2], mErrors[4 * i + 3], mErrors[4 * i + 4]);
    }
  }
  if (*mErrors > GPUCA_MAX_ERRORS) {
    if (silent) {
      GPUWarning("Additional errors occured (codes not stored)");
    } else {
      GPUError("Additional errors occured (codes not stored)");
    }
  }
}

uint32_t GPUErrors::getNErrors() const
{
  return std::min(*mErrors, GPUCA_MAX_ERRORS);
}

const uint32_t* GPUErrors::getErrorPtr() const
{
  return mErrors + 1;
}

#endif
