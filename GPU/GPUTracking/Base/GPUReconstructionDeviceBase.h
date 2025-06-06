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

/// \file GPUReconstructionDeviceBase.h
/// \author David Rohr

#ifndef GPURECONSTRUCTIONDEVICEBASE_H
#define GPURECONSTRUCTIONDEVICEBASE_H

#include "GPUReconstructionCPU.h"
#include <pthread.h>
#include "GPUReconstructionHelpers.h"
#include "GPUChain.h"
#include <vector>

namespace GPUCA_NAMESPACE
{
namespace gpu
{
#if !(defined(__CLING__) || defined(__ROOTCLING__) || defined(G__ROOT))
extern template class GPUReconstructionKernels<GPUReconstructionCPUBackend>;
#endif

class GPUReconstructionDeviceBase : public GPUReconstructionCPU
{
 public:
  ~GPUReconstructionDeviceBase() override;

  const GPUParam* DeviceParam() const { return &mDeviceConstantMem->param; }
  struct deviceConstantMemRegistration {
    deviceConstantMemRegistration(void* (*reg)())
    {
      GPUReconstructionDeviceBase::getDeviceConstantMemRegistratorsVector().emplace_back(reg);
    }
  };

 protected:
  GPUReconstructionDeviceBase(const GPUSettingsDeviceBackend& cfg, size_t sizeCheck);

  int32_t InitDevice() override;
  virtual int32_t InitDevice_Runtime() = 0;
  int32_t ExitDevice() override;
  virtual int32_t ExitDevice_Runtime() = 0;
  int32_t registerMemoryForGPU_internal(const void* ptr, size_t size) override;
  int32_t unregisterMemoryForGPU_internal(const void* ptr) override;
  void unregisterRemainingRegisteredMemory();

  virtual const GPUTPCTracker* CPUTracker(int32_t iSlice) { return &processors()->tpcTrackers[iSlice]; }

  int32_t GPUDebug(const char* state = "UNKNOWN", int32_t stream = -1, bool force = false) override = 0;
  size_t TransferMemoryInternal(GPUMemoryResource* res, int32_t stream, deviceEvent* ev, deviceEvent* evList, int32_t nEvents, bool toGPU, const void* src, void* dst) override;
  size_t GPUMemCpy(void* dst, const void* src, size_t size, int32_t stream, int32_t toGPU, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int32_t nEvents = 1) override = 0;
  size_t GPUMemCpyAlways(bool onGpu, void* dst, const void* src, size_t size, int32_t stream, int32_t toGPU, deviceEvent* ev = nullptr, deviceEvent* evList = nullptr, int32_t nEvents = 1) override;
  size_t WriteToConstantMemory(size_t offset, const void* src, size_t size, int32_t stream = -1, deviceEvent* ev = nullptr) override = 0;

  int32_t StartHelperThreads() override;
  int32_t StopHelperThreads() override;
  void RunHelperThreads(int32_t (GPUReconstructionHelpers::helperDelegateBase::*function)(int32_t, int32_t, GPUReconstructionHelpers::helperParam*), GPUReconstructionHelpers::helperDelegateBase* functionCls, int32_t count) override;
  int32_t HelperError(int32_t iThread) const override { return mHelperParams[iThread].error; }
  int32_t HelperDone(int32_t iThread) const override { return mHelperParams[iThread].done; }
  void WaitForHelperThreads() override;
  void ResetHelperThreads(int32_t helpers) override;
  void ResetThisHelperThread(GPUReconstructionHelpers::helperParam* par);

  int32_t GetGlobalLock(void*& pLock);
  void ReleaseGlobalLock(void* sem);

  static void* helperWrapper_static(void* arg);
  void* helperWrapper(GPUReconstructionHelpers::helperParam* par);

  int32_t mDeviceId = -1;                                         // Device ID used by backend
  GPUReconstructionHelpers::helperParam* mHelperParams = nullptr; // Control Struct for helper threads
  int32_t mNSlaveThreads = 0;                                     // Number of slave threads currently active

  struct DebugEvents {
    deviceEvent DebugStart, DebugStop; // Debug timer events
  };
  DebugEvents* mDebugEvents = nullptr;

  std::vector<void*> mDeviceConstantMemList;
  static std::vector<void* (*)()>& getDeviceConstantMemRegistratorsVector()
  {
    static std::vector<void* (*)()> deviceConstantMemRegistrators{};
    return deviceConstantMemRegistrators;
  }
  void runConstantRegistrators();
};

inline size_t GPUReconstructionDeviceBase::GPUMemCpyAlways(bool onGpu, void* dst, const void* src, size_t size, int32_t stream, int32_t toGPU, deviceEvent* ev, deviceEvent* evList, int32_t nEvents)
{
  if (onGpu) {
    return GPUMemCpy(dst, src, size, stream, toGPU, ev, evList, nEvents);
  } else {
    return GPUReconstructionCPU::GPUMemCpyAlways(false, dst, src, size, stream, toGPU, ev, evList, nEvents);
  }
}
} // namespace gpu
} // namespace GPUCA_NAMESPACE

#endif
