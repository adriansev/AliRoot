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

/// \file GPUTPCGlobalMergerComponent.cxx
/// \author David Rohr, Sergey Gorbunov, Matthias Kretz

#include "GPUTPCGlobalMergerComponent.h"
#include "GPUReconstruction.h"
#include "GPUChainTracking.h"
#include "GPUTPCSliceOutput.h"

#include "GPUTPCDef.h"

#include "GPUTPCGMMerger.h"
#include "GPUTPCGMMergedTrack.h"

#include "AliHLTTPCDefinitions.h"
#include "GPUTPCDefinitions.h"
#include "AliHLTTPCGeometry.h"

#include "AliExternalTrackParam.h"
#include "AliCDBEntry.h"
#include "AliCDBManager.h"
#include "TObjString.h"
#include "TObjArray.h"
#include "AliHLTExternalTrackParam.h"

#include <climits>
#include <cstdlib>
#include <cerrno>

using namespace GPUCA_NAMESPACE::gpu;
using namespace std;

// ROOT macro for the implementation of ROOT specific class methods
ClassImp(GPUTPCGlobalMergerComponent);

const GPUChainTracking* GPUTPCGlobalMergerComponent::fgCurrentMergerReconstruction = nullptr;

GPUTPCGlobalMergerComponent::GPUTPCGlobalMergerComponent() : AliHLTProcessor(), fSolenoidBz(0), fClusterErrorCorrectionY(0), fClusterErrorCorrectionZ(0), fNWays(1), fNWaysOuter(0), fNoClear(false), fBenchmark("GlobalMerger"), fRec(nullptr), fChain(nullptr)
{
  // see header file for class documentation
}

GPUTPCGlobalMergerComponent::GPUTPCGlobalMergerComponent(const GPUTPCGlobalMergerComponent&) : AliHLTProcessor(), fSolenoidBz(0), fClusterErrorCorrectionY(0), fClusterErrorCorrectionZ(0), fNWays(1), fNWaysOuter(0), fNoClear(false), fBenchmark("GlobalMerger"), fRec(nullptr), fChain(nullptr)
{
  // dummy
}

GPUTPCGlobalMergerComponent& GPUTPCGlobalMergerComponent::operator=(const GPUTPCGlobalMergerComponent&)
{
  // dummy
  return *this;
}

GPUTPCGlobalMergerComponent::~GPUTPCGlobalMergerComponent()
{
  if (fRec) {
    delete fRec;
  }
};

// Public functions to implement AliHLTComponent's interface.
// These functions are required for the registration process

const char* GPUTPCGlobalMergerComponent::GetComponentID()
{
  // see header file for class documentation
  return "TPCCAGlobalMerger";
}

void GPUTPCGlobalMergerComponent::GetInputDataTypes(AliHLTComponentDataTypeList& list)
{
  // see header file for class documentation
  list.clear();
  list.push_back(GPUTPCDefinitions::fgkTrackletsDataType);
}

AliHLTComponentDataType GPUTPCGlobalMergerComponent::GetOutputDataType()
{
  // see header file for class documentation
  return kAliHLTMultipleDataType;
}

int32_t GPUTPCGlobalMergerComponent::GetOutputDataTypes(AliHLTComponentDataTypeList& tgtList)
{
  // see header file for class documentation

  tgtList.clear();
  tgtList.push_back(kAliHLTDataTypeTrack | kAliHLTDataOriginTPC);
  tgtList.push_back(AliHLTTPCDefinitions::TracksOuterDataType() | kAliHLTDataOriginTPC);
  return tgtList.size();
}

void GPUTPCGlobalMergerComponent::GetOutputDataSize(unsigned long& constBase, double& inputMultiplier)
{
  // see header file for class documentation
  // XXX TODO: Find more realistic values.
  constBase = 0;
  inputMultiplier = 1.0;
}

AliHLTComponent* GPUTPCGlobalMergerComponent::Spawn()
{
  // see header file for class documentation
  return new GPUTPCGlobalMergerComponent;
}

void GPUTPCGlobalMergerComponent::SetDefaultConfiguration()
{
  // Set default configuration for the CA merger component
  // Some parameters can be later overwritten from the OCDB

  fSolenoidBz = -5.00668;
  fClusterErrorCorrectionY = 0;
  fClusterErrorCorrectionZ = 0;
  fNWays = 1;
  fNWaysOuter = 0;
  fNoClear = false;
  fBenchmark.Reset();
  fBenchmark.SetTimer(0, "total");
  fBenchmark.SetTimer(1, "reco");
}

int32_t GPUTPCGlobalMergerComponent::ReadConfigurationString(const char* arguments)
{
  // Set configuration parameters for the CA merger component from the string

  int32_t iResult = 0;
  if (!arguments) {
    return iResult;
  }

  TString allArgs = arguments;
  TString argument;
  int32_t bMissingParam = 0;

  TObjArray* pTokens = allArgs.Tokenize(" ");

  int32_t nArgs = pTokens ? pTokens->GetEntries() : 0;

  for (int32_t i = 0; i < nArgs; i++) {
    argument = ((TObjString*)pTokens->At(i))->GetString();
    if (argument.IsNull()) {
      continue;
    }

    if (argument.CompareTo("-solenoidBz") == 0) {
      if ((bMissingParam = (++i >= pTokens->GetEntries()))) {
        break;
      }
      HLTWarning("argument -solenoidBz is deprecated, magnetic field set up globally (%f)", GetBz());
      continue;
    }

    if (argument.CompareTo("-errorCorrectionY") == 0) {
      if ((bMissingParam = (++i >= pTokens->GetEntries()))) {
        break;
      }
      fClusterErrorCorrectionY = ((TObjString*)pTokens->At(i))->GetString().Atof();
      HLTInfo("Cluster Y error correction factor set to: %f", fClusterErrorCorrectionY);
      continue;
    }

    if (argument.CompareTo("-errorCorrectionZ") == 0) {
      if ((bMissingParam = (++i >= pTokens->GetEntries()))) {
        break;
      }
      fClusterErrorCorrectionZ = ((TObjString*)pTokens->At(i))->GetString().Atof();
      HLTInfo("Cluster Z error correction factor set to: %f", fClusterErrorCorrectionZ);
      continue;
    }

    if (argument.CompareTo("-nways") == 0) {
      if ((bMissingParam = (++i >= pTokens->GetEntries()))) {
        break;
      }
      fNWays = ((TObjString*)pTokens->At(i))->GetString().Atoi();
      HLTInfo("nways set to: %d", fNWays);
      continue;
    }

    if (argument.CompareTo("-nwaysouter") == 0) {
      fNWaysOuter = 1;
      HLTInfo("nwaysouter enabled");
      continue;
    }

    if (argument.CompareTo("-noclear") == 0) {
      fNoClear = true;
      HLTInfo("noclear enabled");
      continue;
    }

    HLTError("Unknown option \"%s\"", argument.Data());
    iResult = -EINVAL;
  }
  delete pTokens;

  if (bMissingParam) {
    HLTError("Specifier missed for parameter \"%s\"", argument.Data());
    iResult = -EINVAL;
  }

  return iResult;
}

int32_t GPUTPCGlobalMergerComponent::ReadCDBEntry(const char* cdbEntry, const char* chainId)
{
  // see header file for class documentation

  const char* defaultNotify = "";

  if (!cdbEntry) {
    cdbEntry = "HLT/ConfigTPC/TPCCAGlobalMerger";
    defaultNotify = " (default)";
    chainId = 0;
  }

  HLTInfo("configure from entry \"%s\"%s, chain id %s", cdbEntry, defaultNotify, (chainId != nullptr && chainId[0] != 0) ? chainId : "<none>");
  AliCDBEntry* pEntry = AliCDBManager::Instance()->Get(cdbEntry); //,GetRunNo());

  if (!pEntry) {
    HLTError("cannot fetch object \"%s\" from CDB", cdbEntry);
    return -EINVAL;
  }

  TObjString* pString = dynamic_cast<TObjString*>(pEntry->GetObject());

  if (!pString) {
    HLTError("configuration object \"%s\" has wrong type, required TObjString", cdbEntry);
    return -EINVAL;
  }

  HLTInfo("received configuration object string: \"%s\"", pString->GetString().Data());

  return ReadConfigurationString(pString->GetString().Data());
}

int32_t GPUTPCGlobalMergerComponent::Configure(const char* cdbEntry, const char* chainId, const char* commandLine)
{
  // Configure the component
  // There are few levels of configuration,
  // parameters which are set on one step can be overwritten on the next step

  //* read hard-coded values

  SetDefaultConfiguration();

  //* read the default CDB entry

  int32_t iResult = ReadCDBEntry(nullptr, chainId);
  if (iResult) {
    return iResult;
  }

  //* read magnetic field

  fSolenoidBz = GetBz();

  //* read the actual CDB entry if required

  iResult = (cdbEntry) ? ReadCDBEntry(cdbEntry, chainId) : 0;
  if (iResult) {
    return iResult;
  }

  //* read extra parameters from input (if they are)

  if (commandLine && commandLine[0] != '\0') {
    HLTInfo("received configuration string from HLT framework: \"%s\"", commandLine);
    iResult = ReadConfigurationString(commandLine);
    if (iResult) {
      return iResult;
    }
  }

  fRec = GPUReconstruction::CreateInstance("CPU", true);
  if (fRec == nullptr) {
    return -EINVAL;
  }
  fChain = fRec->AddChain<GPUChainTracking>();

  // Initialize the merger

  GPUSettingsGRP grp;
  GPUSettingsRec rec;
  GPUSettingsProcessing devProc;
  grp.solenoidBzNominalGPU = fSolenoidBz;
  if (fClusterErrorCorrectionY > 1.e-4) {
    rec.tpc.clusterError2CorrectionY = fClusterErrorCorrectionY * fClusterErrorCorrectionY;
  }
  if (fClusterErrorCorrectionZ > 1.e-4) {
    rec.tpc.clusterError2CorrectionZ = fClusterErrorCorrectionZ * fClusterErrorCorrectionZ;
  }
  rec.tpc.nWays = fNWays;
  rec.tpc.nWaysOuter = fNWaysOuter;
  rec.tpc.mergerInterpolateErrors = false;
  rec.nonConsecutiveIDs = true;
  rec.tpc.mergerReadFromTrackerDirectly = false;
  devProc.ompThreads = 1;
  devProc.ompKernels = false;

  GPURecoStepConfiguration steps;
  steps.steps.set(GPUDataTypes::RecoStep::TPCMerging);
  steps.inputs.set(GPUDataTypes::InOutType::TPCSectorTracks);
  steps.outputs.set(GPUDataTypes::InOutType::TPCMergedTracks);

  fRec->SetSettings(&grp, &rec, &devProc, &steps);
  fChain->LoadClusterErrors();
  if (fRec->Init()) {
    return -EINVAL;
  }

  return 0;
}

int32_t GPUTPCGlobalMergerComponent::DoInit(int argc, const char** argv)
{
  // see header file for class documentation

  TString arguments = "";
  for (int32_t i = 0; i < argc; i++) {
    if (!arguments.IsNull()) {
      arguments += " ";
    }
    arguments += argv[i];
  }

  int32_t retVal = Configure(nullptr, nullptr, arguments.Data());

  return retVal;
}

int32_t GPUTPCGlobalMergerComponent::Reconfigure(const char* cdbEntry, const char* chainId)
{
  // Reconfigure the component from OCDB

  return Configure(cdbEntry, chainId, nullptr);
}

int32_t GPUTPCGlobalMergerComponent::DoDeinit()
{
  // see header file for class documentation
  if (fChain == fgCurrentMergerReconstruction) {
    fgCurrentMergerReconstruction = nullptr;
  }
  delete fRec;
  fRec = nullptr;

  return 0;
}

int32_t GPUTPCGlobalMergerComponent::DoEvent(const AliHLTComponentEventData& evtData, const AliHLTComponentBlockData* blocks, AliHLTComponentTriggerData& /*trigData*/, AliHLTUInt8_t* outputPtr, AliHLTUInt32_t& size, AliHLTComponentBlockDataList& outputBlocks)
{
  // see header file for class documentation
  int32_t iResult = 0;
  uint32_t maxBufferSize = size;

  size = 0;

  if (!outputPtr) {
    return -ENOSPC;
  }
  if (!IsDataEvent()) {
    return 0;
  }
  fBenchmark.StartNewEvent();
  fBenchmark.Start(0);

  fChain->GetTPCMerger().Clear();

  int32_t nSlicesSet = 0;
  const AliHLTComponentBlockData* const blocksEnd = blocks + evtData.fBlockCnt;
  for (const AliHLTComponentBlockData* block = blocks; block < blocksEnd; ++block) {
    if (block->fDataType != GPUTPCDefinitions::fgkTrackletsDataType) {
      continue;
    }

    fBenchmark.AddInput(block->fSize);

    int32_t slice = AliHLTTPCDefinitions::GetMinSliceNr(*block);
    if (slice < 0 || slice >= AliHLTTPCGeometry::GetNSlice()) {
      HLTError("invalid slice number %d extracted from specification 0x%08lx,  skipping block of type %s", slice, block->fSpecification, DataType2Text(block->fDataType).c_str());
      // just remember the error, if there are other valid blocks ignore the error, return code otherwise
      iResult = -EBADF;
      continue;
    }

    if (slice != AliHLTTPCDefinitions::GetMaxSliceNr(*block)) {
      // the code was not written for/ never used with multiple slices in one data block/ specification
      HLTWarning("specification 0x%08lx indicates multiple slices in data block %s: never used before, please audit the code", block->fSpecification, DataType2Text(block->fDataType).c_str());
    }
    GPUTPCSliceOutput* sliceOut = reinterpret_cast<GPUTPCSliceOutput*>(block->fPtr);
    fChain->GetTPCMerger().SetSliceData(slice, sliceOut);
    nSlicesSet++;
  }
  if (nSlicesSet != 36) {
    if (nSlicesSet != 0) {
      HLTError("Incomplete input data");
      return (-EINVAL);
    }
    return 0;
  }
  fBenchmark.Start(1);
  fChain->RunTPCTrackingMerger();
  if (fChain->CheckErrorCodes()) {
    return (-EINVAL);
  }
  fBenchmark.Stop(1);

  // Fill output
  uint32_t mySize = 0;
  {
    AliHLTTracksData* outPtr = (AliHLTTracksData*)(outputPtr);
    AliHLTExternalTrackParam* currOutTrack = outPtr->fTracklets;
    mySize = ((AliHLTUInt8_t*)currOutTrack) - ((AliHLTUInt8_t*)outputPtr);
    outPtr->fCount = 0;
    int32_t nTracks = fChain->GetTPCMerger().NOutputTracks();

    for (int32_t itr = 0; itr < nTracks; itr++) {
      // convert GPUTPCGMMergedTrack to AliHLTTrack

      const GPUTPCGMMergedTrack& track = fChain->GetTPCMerger().OutputTracks()[itr];
      if (!track.OK()) {
        continue;
      }
      uint32_t dSize = sizeof(AliHLTExternalTrackParam) + track.NClusters() * sizeof(uint32_t);

      if (mySize + dSize > maxBufferSize) {
        HLTWarning("Output buffer size exceed (buffer size %d, current size %d), %d tracks are not stored", maxBufferSize, mySize, nTracks - itr + 1);
        iResult = -ENOSPC;
        break;
      }

      // first convert to AliExternalTrackParam

      AliExternalTrackParam tp;
      track.GetParam().GetExtParam(tp, track.GetAlpha());

      // normalize the angle to +-Pi

      currOutTrack->fAlpha = tp.GetAlpha() - CAMath::Round(tp.GetAlpha() / CAMath::TwoPi()) * CAMath::TwoPi();
      currOutTrack->fX = tp.GetX();
      currOutTrack->fY = tp.GetY();
      currOutTrack->fZ = tp.GetZ();
      currOutTrack->fLastX = track.LastX();
      currOutTrack->fLastY = track.LastY();
      currOutTrack->fLastZ = track.LastZ();

      currOutTrack->fq1Pt = tp.GetSigned1Pt();
      currOutTrack->fSinPhi = tp.GetSnp();
      currOutTrack->fTgl = tp.GetTgl();
      for (int32_t i = 0; i < 15; i++) {
        currOutTrack->fC[i] = tp.GetCovariance()[i];
      }
      currOutTrack->fTrackID = itr;
      currOutTrack->fFlags = 0;
      currOutTrack->fNPoints = 0;
      for (int32_t i = 0; i < track.NClusters(); i++) {
        if (fChain->GetTPCMerger().Clusters()[track.FirstClusterRef() + i].state & GPUTPCGMMergedTrackHit::flagReject) {
          continue;
        }
        currOutTrack->fPointIDs[currOutTrack->fNPoints++] = fChain->GetTPCMerger().Clusters()[track.FirstClusterRef() + i].num;
      }
      dSize = sizeof(AliHLTExternalTrackParam) + currOutTrack->fNPoints * sizeof(uint32_t);

      currOutTrack = (AliHLTExternalTrackParam*)(((Byte_t*)currOutTrack) + dSize);
      mySize += dSize;
      outPtr->fCount++;
    }

    AliHLTComponentBlockData resultData;
    FillBlockData(resultData);
    resultData.fOffset = 0;
    resultData.fSize = mySize;
    resultData.fDataType = kAliHLTDataTypeTrack | kAliHLTDataOriginTPC;
    resultData.fSpecification = AliHLTTPCDefinitions::EncodeDataSpecification(0, 35, 0, 5);
    outputBlocks.push_back(resultData);
    fBenchmark.AddOutput(resultData.fSize);

    size = resultData.fSize;
  }

  if (fNWays > 1 && fNWaysOuter) {
    uint32_t newSize = 0;
    AliHLTTracksData* outPtr = (AliHLTTracksData*)(outputPtr + size);
    AliHLTExternalTrackParam* currOutTrack = outPtr->fTracklets;
    newSize = ((AliHLTUInt8_t*)currOutTrack) - (outputPtr + size);
    outPtr->fCount = 0;
    int32_t nTracks = fChain->GetTPCMerger().NOutputTracks();

    for (int32_t itr = 0; itr < nTracks; itr++) {
      const GPUTPCGMMergedTrack& track = fChain->GetTPCMerger().OutputTracks()[itr];
      if (!track.OK()) {
        continue;
      }
      uint32_t dSize = sizeof(AliHLTExternalTrackParam);

      if (mySize + newSize + dSize > maxBufferSize) {
        HLTWarning("Output buffer size exceed (buffer size %d, current size %d), %d tracks are not stored", maxBufferSize, mySize + newSize + dSize, nTracks - itr + 1);
        iResult = -ENOSPC;
        break;
      }

      // first convert to AliExternalTrackParam

      AliExternalTrackParam tp;
      track.GetParam().GetExtParam(tp, track.GetAlpha());

      // normalize the angle to +-Pi

      currOutTrack->fAlpha = track.OuterParam().alpha - CAMath::Round(tp.GetAlpha() / CAMath::TwoPi()) * CAMath::TwoPi();
      currOutTrack->fX = track.OuterParam().X;
      currOutTrack->fY = track.OuterParam().P[0];
      currOutTrack->fZ = track.OuterParam().P[1];
      currOutTrack->fLastX = track.LastX();
      currOutTrack->fLastY = track.LastY();
      currOutTrack->fLastZ = track.LastZ();

      currOutTrack->fq1Pt = track.OuterParam().P[4];
      currOutTrack->fSinPhi = track.OuterParam().P[2];
      currOutTrack->fTgl = track.OuterParam().P[3];
      for (int32_t i = 0; i < 15; i++) {
        currOutTrack->fC[i] = track.OuterParam().C[i];
      }
      currOutTrack->fTrackID = itr;
      currOutTrack->fFlags = 0;
      currOutTrack->fNPoints = 0;

      currOutTrack = (AliHLTExternalTrackParam*)(((Byte_t*)currOutTrack) + dSize);
      newSize += dSize;
      outPtr->fCount++;
    }

    AliHLTComponentBlockData resultData;
    FillBlockData(resultData);
    resultData.fOffset = mySize;
    resultData.fSize = newSize;
    resultData.fDataType = AliHLTTPCDefinitions::TracksOuterDataType() | kAliHLTDataOriginTPC;
    resultData.fSpecification = AliHLTTPCDefinitions::EncodeDataSpecification(0, 35, 0, 5);
    outputBlocks.push_back(resultData);
    fBenchmark.AddOutput(resultData.fSize);

    size = resultData.fSize;
  }

  HLTInfo("CAGlobalMerger:: output %d tracks / %d hits", fChain->GetTPCMerger().NOutputTracks(), fChain->GetTPCMerger().NOutputTrackClusters());

  if (fNoClear) {
    fgCurrentMergerReconstruction = fChain;
  } else {
    fChain->GetTPCMerger().Clear();
  }

  fBenchmark.Stop(0);
  HLTInfo(fBenchmark.GetStatistics());
  return iResult;
}

const GPUTPCGMMerger* GPUTPCGlobalMergerComponent::GetCurrentMerger()
{
  if (fgCurrentMergerReconstruction == nullptr) {
    return nullptr;
  }
  return &fgCurrentMergerReconstruction->GetTPCMerger();
}
