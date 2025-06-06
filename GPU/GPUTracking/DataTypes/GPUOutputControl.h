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

/// \file GPUOutputControl.h
/// \author David Rohr

#ifndef GPUOUTPUTCONTROL_H
#define GPUOUTPUTCONTROL_H

#include "GPUCommonDef.h"
#include <cstddef>
#include <functional>
#include <new>

namespace GPUCA_NAMESPACE
{
namespace gpu
{

// This defines an output region. ptrBase points to a memory buffer, which should have a proper alignment.
// Since DPL does not respect the alignment of data types, we do not impose anything specic but just use void*, but it should be >= 64 bytes ideally.
// The size defines the maximum possible buffer size when GPUReconstruction is called, and returns the number of filled bytes when it returns.
// If the buffer size is exceeded, size is set to 1
// ptrCurrent must equal ptr if set (or nullptr), and can be incremented by GPUReconstruction step by step if multiple buffers are used.
// If ptr == nullptr, there is no region defined and GPUReconstruction will write its output to an internal buffer.
// If allocator is set, it is called as a callback to provide a ptr to the memory.

struct GPUOutputControl {
  GPUOutputControl() = default;
  void set(void* p, size_t s)
  {
    reset();
    ptrBase = ptrCurrent = p;
    size = s;
  }
  void set(const std::function<void*(size_t)>& a)
  {
    reset();
    allocator = a;
  }
  void reset()
  {
    new (this) GPUOutputControl;
  }
  bool useExternal() { return size || allocator; }
  bool useInternal() { return !useExternal(); }
  void checkCurrent()
  {
    if (ptrBase && ptrCurrent == nullptr) {
      ptrCurrent = ptrBase;
    }
  }

  void* ptrBase = nullptr;                          // Base ptr to memory pool, occupied size is ptrCurrent - ptr
  void* ptrCurrent = nullptr;                       // Pointer to free Output Space
  size_t size = 0;                                  // Max Size of Output Data if Pointer to output space is given
  std::function<void*(size_t)> allocator = nullptr; // Allocator callback
};

struct GPUTrackingOutputs {
  GPUOutputControl compressedClusters;
  GPUOutputControl clustersNative;
  GPUOutputControl tpcTracks;
  GPUOutputControl clusterLabels;
  GPUOutputControl sharedClusterMap;
  GPUOutputControl tpcOccupancyMap;
  GPUOutputControl tpcTracksO2;
  GPUOutputControl tpcTracksO2ClusRefs;
  GPUOutputControl tpcTracksO2Labels;
  GPUOutputControl tpcTriggerWords;

  static constexpr size_t count() { return sizeof(GPUTrackingOutputs) / sizeof(GPUOutputControl); }
  GPUOutputControl* asArray() { return (GPUOutputControl*)this; }
  size_t getIndex(const GPUOutputControl& v) { return &v - (const GPUOutputControl*)this; }
  static int32_t getIndex(GPUOutputControl GPUTrackingOutputs::*v) { return &(((GPUTrackingOutputs*)(0x10000))->*v) - (GPUOutputControl*)(0x10000); }
};

} // namespace gpu
} // namespace GPUCA_NAMESPACE

#endif
