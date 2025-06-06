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

/// \file AliHLTGPUDumpComponent.cxx
/// \author David Rohr

#include "AliHLTGPUDumpComponent.h"

#include "AliGeomManager.h"
#include "GPUReconstruction.h"
#include "GPUChainTracking.h"
#include "AliHLTTPCDefinitions.h"
#include "GPUTPCMCInfo.h"
#include "GPUTPCGMMergedTrackHit.h"
#include "AliHLTTPCClusterXYZ.h"
#include "AliHLTTPCClusterMCData.h"
#include "GPUTPCClusterData.h"
#include "AliHLTTPCRawCluster.h"
#include "AliHLTTPCGeometry.h"
#include "AliRunLoader.h"
#include "AliHeader.h"
#include "AliStack.h"
#include "AliExternalTrackParam.h"
#include "AliTrackReference.h"
#include "AliHLTTRDDefinitions.h"
#include "GPUTRDTrackletWord.h"
#include "GPUTRDTrackletLabels.h"
#include "TPCFastTransform.h"
#include "CorrectionMapsHelper.h"
#include "TPCFastTransformManager.h"
#include "AliRecoParam.h"
#include "AliTPCTransform.h"
#include "AliTPCcalibDB.h"
#include "AliCDBManager.h"
#include "AliGRPObject.h"
#include "AliCDBEntry.h"
#include "AliRunInfo.h"
#include "AliEventInfo.h"
#include "AliRawEventHeaderBase.h"
#include "AliTPCRecoParam.h"
#include <TGeoGlobalMagField.h>
#include <TVirtualMC.h>

#include "TTree.h"
#include "TParticle.h"
#include "TParticlePDG.h"
#include "TPDGCode.h"

using namespace GPUCA_NAMESPACE::gpu;

AliHLTGPUDumpComponent::AliHLTGPUDumpComponent() : fSolenoidBz(0.f), fRec(nullptr), fChain(nullptr), fFastTransformManager(new TPCFastTransformManager), fCalib(nullptr), fRecParam(nullptr), fOfflineRecoParam(), fOrigTransform(nullptr), fIsMC(false), fInitTimestamp(0.)
{
  fRec = GPUReconstruction::CreateInstance();
  fChain = fRec->AddChain<GPUChainTracking>();
}

AliHLTGPUDumpComponent::~AliHLTGPUDumpComponent()
{
  delete fRec;
  delete fFastTransformManager;
}

const char* AliHLTGPUDumpComponent::GetComponentID() { return "GPUDump"; }

void AliHLTGPUDumpComponent::GetInputDataTypes(vector<AliHLTComponentDataType>& list)
{
  list.clear();
  list.push_back(AliHLTTPCDefinitions::RawClustersDataType());
  list.push_back(AliHLTTPCDefinitions::ClustersXYZDataType());
  list.push_back(AliHLTTPCDefinitions::AliHLTDataTypeClusterMCInfo());
  list.push_back(AliHLTTRDDefinitions::fgkTRDTrackletDataType);
  list.push_back(AliHLTTRDDefinitions::fgkTRDMCTrackletDataType);
}

AliHLTComponentDataType AliHLTGPUDumpComponent::GetOutputDataType() { return AliHLTTPCDefinitions::RawClustersDataType(); }

void AliHLTGPUDumpComponent::GetOutputDataSize(unsigned long& constBase, double& inputMultiplier)
{
  constBase = 10000;     // minimum size
  inputMultiplier = 0.6; // size relative to input
}

AliHLTComponent* AliHLTGPUDumpComponent::Spawn() { return new AliHLTGPUDumpComponent; }

int32_t AliHLTGPUDumpComponent::DoInit(int argc, const char** argv)
{
  fSolenoidBz = GetBz();
  fIsMC = TVirtualMC::GetMC();

  if (!AliGeomManager::GetGeometry()) {
    AliGeomManager::LoadGeometry();
  }
  if (!AliGeomManager::GetGeometry()) {
    HLTFatal("Can not initialise geometry");
  }

  fCalib = AliTPCcalibDB::Instance();
  if (!fCalib) {
    HLTFatal("Calibration not found");
  }
  fCalib->SetRun(GetRunNo());
  fCalib->UpdateRunInformations(GetRunNo());

  const AliMagF* field = (AliMagF*)TGeoGlobalMagField::Instance()->GetField();
  fCalib->SetExBField(field);

  if (!fCalib->GetTransform()) {
    HLTFatal("No TPC transformation found");
  }

  AliGRPObject* pGRP = 0;
  AliCDBEntry* entry = AliCDBManager::Instance()->Get("GRP/GRP/Data");
  if (!entry) {
    HLTFatal("No GRP object found in data base");
  }
  pGRP = dynamic_cast<AliGRPObject*>(entry->GetObject());

  if (!pGRP) {
    HLTFatal("Unknown format of the GRP object in data base");
  }

  AliRunInfo runInfo(pGRP->GetLHCState(), pGRP->GetBeamType(), pGRP->GetBeamEnergy(), pGRP->GetRunType(), pGRP->GetDetectorMask());
  AliEventInfo evInfo;
  evInfo.SetEventType(AliRawEventHeaderBase::kPhysicsEvent);

  entry = AliCDBManager::Instance()->Get("TPC/Calib/RecoParam");
  if (!entry) {
    HLTFatal("No TPC reco param entry found in data base");
  }
  TObject* recoParamObj = entry->GetObject();
  if (dynamic_cast<TObjArray*>(recoParamObj)) {
    TObjArray* copy = (TObjArray*)(static_cast<TObjArray*>(recoParamObj)->Clone());
    fOfflineRecoParam.AddDetRecoParamArray(1, copy);
  } else if (dynamic_cast<AliDetectorRecoParam*>(recoParamObj)) {
    AliDetectorRecoParam* copy = (AliDetectorRecoParam*)static_cast<AliDetectorRecoParam*>(recoParamObj)->Clone();
    fOfflineRecoParam.AddDetRecoParam(1, copy);
  } else {
    HLTFatal("Unknown format of the TPC Reco Param entry in the data base");
  }

  fOfflineRecoParam.SetEventSpecie(&runInfo, evInfo, 0);
  fRecParam = const_cast<AliTPCRecoParam*>(reinterpret_cast<const AliTPCRecoParam*>(fOfflineRecoParam.GetDetRecoParam(1)));
  if (!fRecParam) {
    HLTFatal("No TPC Reco Param entry found for the given event specification");
  }
  fCalib->GetTransform()->SetCurrentRecoParam(fRecParam);
  fInitTimestamp = GetTimeStamp();

  return 0;
}

int32_t AliHLTGPUDumpComponent::DoDeinit() { return 0; }

int32_t AliHLTGPUDumpComponent::Reconfigure(const char* cdbEntry, const char* chainId) { return 0; }

int32_t AliHLTGPUDumpComponent::DoEvent(const AliHLTComponentEventData& evtData, const AliHLTComponentBlockData* blocks, AliHLTComponentTriggerData& /*trigData*/, AliHLTUInt8_t* outputPtr, AliHLTUInt32_t& size, vector<AliHLTComponentBlockData>& outputBlocks)
{
  if (GetFirstInputBlock(kAliHLTDataTypeSOR) || GetFirstInputBlock(kAliHLTDataTypeEOR)) {
    return 0;
  }

  if (evtData.fBlockCnt <= 0) {
    HLTWarning("no blocks in event");
    return 0;
  }

  // Prepare everything for all slices
  const AliHLTTPCClusterMCData* clusterLabels[NSLICES][NPATCHES] = {nullptr};
  const AliHLTTPCClusterXYZData* clustersXYZ[NSLICES][NPATCHES] = {nullptr};
  const AliHLTTPCRawClusterData* clustersRaw[NSLICES][NPATCHES] = {nullptr};
  bool labelsPresent = false;
  const GPUTRDTrackletWord* TRDtracklets = nullptr;
  int32_t nTRDTrackletsTotal = 0;

  for (uint64_t ndx = 0; ndx < evtData.fBlockCnt; ndx++) {
    const AliHLTComponentBlockData& pBlock = blocks[ndx];
    int32_t slice = AliHLTTPCDefinitions::GetMinSliceNr(pBlock);
    int32_t patch = AliHLTTPCDefinitions::GetMinPatchNr(pBlock);
    if (pBlock.fDataType == AliHLTTPCDefinitions::RawClustersDataType()) {
      clustersRaw[slice][patch] = (const AliHLTTPCRawClusterData*)pBlock.fPtr;
    } else if (pBlock.fDataType == AliHLTTPCDefinitions::ClustersXYZDataType()) {
      clustersXYZ[slice][patch] = (const AliHLTTPCClusterXYZData*)pBlock.fPtr;
    } else if (pBlock.fDataType == AliHLTTPCDefinitions::AliHLTDataTypeClusterMCInfo()) {
      clusterLabels[slice][patch] = (const AliHLTTPCClusterMCData*)pBlock.fPtr;
      labelsPresent = true;
    } else if (pBlock.fDataType == AliHLTTRDDefinitions::fgkTRDTrackletDataType) {
      TRDtracklets = reinterpret_cast<const GPUTRDTrackletWord*>(pBlock.fPtr);
      nTRDTrackletsTotal = pBlock.fSize / sizeof(GPUTRDTrackletWord);
    }
  }

  std::vector<AliHLTTPCRawCluster> rawClusters[NSLICES];
  std::vector<GPUTPCClusterData> clusterData[NSLICES];

  int32_t nClustersTotal = 0;
  for (int32_t slice = 0; slice < NSLICES; slice++) {
    int32_t nClustersSliceTotal = 0;
    clusterData[slice].clear();
    rawClusters[slice].clear();
    for (int32_t patch = 0; patch < 6; patch++) {
      if (clustersXYZ[slice][patch]) {
        nClustersSliceTotal += clustersXYZ[slice][patch]->fCount;
      }
    }
    GPUTPCClusterData cluster;
    for (int32_t patch = 0; patch < 6; patch++) {
      if (clustersXYZ[slice][patch] != nullptr && clustersRaw[slice][patch] != nullptr) {
        const AliHLTTPCClusterXYZData& clXYZ = *clustersXYZ[slice][patch];
        const AliHLTTPCRawClusterData& clRaw = *clustersRaw[slice][patch];

        if (clXYZ.fCount != clRaw.fCount) {
          HLTError("Number of entries in raw and xyz clusters are not mached %d vs %d", clXYZ.fCount, clRaw.fCount);
          continue;
        }

        const int32_t firstRow = AliHLTTPCGeometry::GetFirstRow(patch);
        for (int32_t ic = 0; ic < clXYZ.fCount; ic++) {
          const AliHLTTPCClusterXYZ& c = clXYZ.fClusters[ic];
          const AliHLTTPCRawCluster& cRaw = clRaw.fClusters[ic];
          if (fabsf(c.GetZ()) > 300) {
            continue;
          }
          if (c.GetX() < 1.f) {
            continue; // cluster xyz position was not calculated for whatever reason
          }
          cluster.id = AliHLTTPCGeometry::CreateClusterID(slice, patch, ic);
          cluster.x = c.GetX();
          cluster.y = c.GetY();
          cluster.z = c.GetZ();
          cluster.row = firstRow + cRaw.GetPadRow();
          cluster.flags = cRaw.GetFlags();
          if (cRaw.GetSigmaPad2() < kAlmost0 || cRaw.GetSigmaTime2() < kAlmost0) {
            cluster.flags |= GPUTPCGMMergedTrackHit::flagSingle;
          }
          cluster.amp = cRaw.GetCharge();
#ifdef GPUCA_FULL_CLUSTERDATA
          cluster.pad = cRaw.GetPad();
          cluster.time = cRaw.GetTime();
          cluster.ampMax = cRaw.GetQMax();
          cluster.sigmaPad2 = cRaw.GetSigmaPad2();
          cluster.sigmaTime2 = cRaw.GetSigmaTime2();
#endif
          AliHLTTPCRawCluster tmp = cRaw;
          tmp.fPadRow += firstRow;
          if ((uint32_t)cluster.amp >= 25 * 1024) {
            GPUError("Invalid cluster charge, truncating (%d >= %d)", (int32_t)cluster.amp, 25 * 1024);
            cluster.amp = 25 * 1024 - 1;
          }
          if ((uint32_t)tmp.GetCharge() >= 25 * 1024) {
            GPUError("Invalid raw cluster charge, truncating (%d >= %d)", (int32_t)tmp.GetCharge(), 25 * 1024);
            tmp.SetCharge(25 * 1024 - 1);
          }
          if ((uint32_t)tmp.GetQMax() >= 1024) {
            GPUError("Invalid raw cluster charge max, truncating (%d >= %d)", (int32_t)tmp.GetQMax(), 1024);
            tmp.SetQMax(1024 - 1);
          }
          clusterData[slice].emplace_back(cluster);
          rawClusters[slice].emplace_back(tmp);

          nClustersTotal++;
        }
      }
    }
    HLTDebug("Read %d->%d hits for slice %d", nClustersSliceTotal, (int32_t)clusterData[slice].size(), slice);
  }

  if (nClustersTotal < 100) {
    return (0);
  }
  fChain->ClearIOPointers();

  for (int32_t i = 0; i < NSLICES; i++) {
    fChain->mIOPtrs.nClusterData[i] = clusterData[i].size();
    fChain->mIOPtrs.clusterData[i] = clusterData[i].data();
    fChain->mIOPtrs.nRawClusters[i] = rawClusters[i].size();
    fChain->mIOPtrs.rawClusters[i] = rawClusters[i].data();
    HLTDebug("Slice %d - Clusters %d", i, (int32_t)clusterData[i].size());
  }

  std::vector<AliHLTTPCClusterMCLabel> labels;
  std::vector<GPUTPCMCInfo> mcInfo;

  if (labelsPresent) {
    // Write cluster labels
    for (uint32_t iSlice = 0; iSlice < NSLICES; iSlice++) {
      GPUTPCClusterData* pCluster = clusterData[iSlice].data();
      for (uint32_t iPatch = 0; iPatch < NPATCHES; iPatch++) {
        if (clusterLabels[iSlice][iPatch] == nullptr || clustersXYZ[iSlice][iPatch] == nullptr || clusterLabels[iSlice][iPatch]->fCount != clustersXYZ[iSlice][iPatch]->fCount) {
          continue;
        }
        const AliHLTTPCClusterXYZData& clXYZ = *clustersXYZ[iSlice][iPatch];
        for (int32_t ic = 0; ic < clXYZ.fCount; ic++) {
          if (pCluster->id != AliHLTTPCGeometry::CreateClusterID(iSlice, iPatch, ic)) {
            continue;
          }
          pCluster->id = labels.size();
          labels.push_back(clusterLabels[iSlice][iPatch]->fLabels[ic]);
          pCluster++;
        }
      }
    }

    if (labels.size() != nClustersTotal) {
      HLTFatal("Error getting cluster MC labels (%d labels, %d clusters)", (int32_t)labels.size(), nClustersTotal);
      return (-EINVAL);
    }

    fChain->mIOPtrs.nMCLabelsTPC = labels.size();
    fChain->mIOPtrs.mcLabelsTPC = labels.data();
    HLTDebug("Number of mc labels %d", (int32_t)labels.size());

    // Write MC tracks
    bool OK = false;
    do {
      AliRunLoader* rl = AliRunLoader::Instance();
      if (rl == nullptr) {
        HLTFatal("error: RL");
        break;
      }

      rl->LoadKinematics();
      rl->LoadTrackRefs();

      int32_t nTracks = rl->GetHeader()->GetNtrack();
      mcInfo.resize(nTracks);

      AliStack* stack = rl->Stack();
      if (stack == nullptr) {
        HLTFatal("error: stack");
        break;
      }
      TTree* TR = rl->TreeTR();
      if (TR == nullptr) {
        HLTFatal("error: TR");
        break;
      }
      TBranch* branch = TR->GetBranch("TrackReferences");
      if (branch == nullptr) {
        HLTFatal("error: branch");
        break;
      }

      int32_t nPrimaries = stack->GetNprimary();

      std::vector<AliTrackReference*> trackRefs(nTracks, nullptr);
      TClonesArray* tpcRefs = nullptr;
      branch->SetAddress(&tpcRefs);
      int32_t nr = TR->GetEntries();
      for (int32_t r = 0; r < nr; r++) {
        TR->GetEvent(r);
        for (int32_t i = 0; i < tpcRefs->GetEntriesFast(); i++) {
          AliTrackReference* tpcRef = (AliTrackReference*)tpcRefs->UncheckedAt(i);
          if (tpcRef->DetectorId() != AliTrackReference::kTPC) {
            continue;
          }
          if (tpcRef->Label() < 0 || tpcRef->Label() >= nTracks) {
            HLTFatal("Invalid reference %d / %d", tpcRef->Label(), nTracks);
            continue;
          }
          if (trackRefs[tpcRef->Label()] != nullptr) {
            continue;
          }
          trackRefs[tpcRef->Label()] = new AliTrackReference(*tpcRef);
        }
      }

      memset(mcInfo.data(), 0, nTracks * sizeof(mcInfo[0]));

      for (int32_t i = 0; i < nTracks; i++) {
        mcInfo[i].pid = -100;
        TParticle* particle = (TParticle*)stack->Particle(i);
        if (particle == nullptr) {
          continue;
        }
        if (particle->GetPDG() == nullptr) {
          continue;
        }

        int32_t charge = (int32_t)particle->GetPDG()->Charge();
        int32_t prim = stack->IsPhysicalPrimary(i);
        int32_t hasPrimDaughter = particle->GetFirstDaughter() != -1 && particle->GetFirstDaughter() < nPrimaries;

        mcInfo[i].charge = charge;
        mcInfo[i].prim = prim;
        mcInfo[i].primDaughters = hasPrimDaughter;
        mcInfo[i].genRadius = sqrt(particle->Vx() * particle->Vx() + particle->Vy() * particle->Vy() + particle->Vz() * particle->Vz());

        Int_t pid = -1;
        if (TMath::Abs(particle->GetPdgCode()) == kElectron) {
          pid = 0;
        }
        if (TMath::Abs(particle->GetPdgCode()) == kMuonMinus) {
          pid = 1;
        }
        if (TMath::Abs(particle->GetPdgCode()) == kPiPlus) {
          pid = 2;
        }
        if (TMath::Abs(particle->GetPdgCode()) == kKPlus) {
          pid = 3;
        }
        if (TMath::Abs(particle->GetPdgCode()) == kProton) {
          pid = 4;
        }
        mcInfo[i].pid = pid;

        AliTrackReference* ref = trackRefs[i];
        if (ref) {
          mcInfo[i].x = ref->X();
          mcInfo[i].y = ref->Y();
          mcInfo[i].z = ref->Z();
          mcInfo[i].pX = ref->Px();
          mcInfo[i].pY = ref->Py();
          mcInfo[i].pZ = ref->Pz();
        }

        // if (ref) HLTImportant("Particle %d: Charge %d, Prim %d, PrimDaughter %d, Pt %f %f ref %p\n", i, charge, prim, hasPrimDaughter, ref->Pt(), particle->Pt(), ref);
      }
      for (int32_t i = 0; i < nTracks; i++) {
        delete trackRefs[i];
      }

      OK = true;
    } while (false);

    if (!OK) {
      HLTFatal("Error accessing MC data");
      return (-EINVAL);
    }

    fChain->mIOPtrs.nMCInfosTPC = mcInfo.size();
    fChain->mIOPtrs.mcInfosTPC = mcInfo.data();
    static const GPUTPCMCInfoCol mcColInfo = {0, (uint32_t)mcInfo.size()};
    fChain->mIOPtrs.mcInfosTPCCol = &mcColInfo;
    fChain->mIOPtrs.nMCInfosTPCCol = 1;
    HLTDebug("Number of MC infos: %d", (int32_t)mcInfo.size());
  }
  uint32_t clusterNum = 0;
  for (uint32_t slice = 0; slice < NSLICES; slice++) {
    for (int32_t k = 0; k < fChain->mIOPtrs.nClusterData[slice]; k++) {
      clusterData[slice][k].id = clusterNum++;
    }
  }

  fChain->mIOPtrs.nTRDTracklets = nTRDTrackletsTotal;
  std::vector<GPUTRDTrackletWord> tracklets(nTRDTrackletsTotal);
  for (int32_t i = 0; i < nTRDTrackletsTotal; i++) {
    tracklets[i] = TRDtracklets[i];
  }
  std::sort(tracklets.data(), tracklets.data() + nTRDTrackletsTotal);
  fChain->mIOPtrs.trdTracklets = tracklets.data();

  fChain->mIOPtrs.nTRDTriggerRecords = 1;
  static float t = 0.f;
  static int32_t o = 0;
  fChain->mIOPtrs.trdTriggerTimes = &t;
  fChain->mIOPtrs.trdTrackletIdxFirst = &o;

  HLTDebug("Number of TRD tracklets: %d", (int32_t)nTRDTrackletsTotal);

  static int32_t nEvent = 0;
  char filename[256];
  std::ofstream out;

  if (nEvent == 0) {
    std::unique_ptr<TPCFastTransform> fFastTransformIRS(new TPCFastTransform);
    int64_t TimeStamp = (getenv("DUMP_TIMESTAMP_SOR") && atoi(getenv("DUMP_TIMESTAMP_SOR"))) ? fInitTimestamp : GetTimeStamp();
    if (fIsMC && !fRecParam->GetUseCorrectionMap()) {
      TimeStamp = 0;
    }
    if (fFastTransformManager->create(*fFastTransformIRS, fCalib->GetTransform(), TimeStamp)) {
      HLTFatal("Initialisation of Fast Transformation failed with error %s", fFastTransformManager->getLastError());
    }
    std::unique_ptr<CorrectionMapsHelper> tmpHelper;
    tmpHelper->setCorrMap(fFastTransformIRS.get());
    fChain->SetTPCFastTransform(std::move(fFastTransformIRS), std::move(tmpHelper));

    fRec->SetSettings(fSolenoidBz);
    fRec->DumpSettings();
  }

  snprintf(filename, 256, GPUCA_EVDUMP_FILE ".%d.dump", nEvent++);
  fChain->DumpData(filename);
  return (0);
}
