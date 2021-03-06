// $Id$

//**************************************************************************
//* This file is property of and copyright by the ALICE HLT Project        * 
//* ALICE Experiment at CERN, All rights reserved.                         *
//*                                                                        *
//* Primary Authors: Federico Ronchetti                                    *
//*                                                                        *
//* Permission to use, copy, modify and distribute this software and its   *
//* documentation strictly for non-commercial purposes is hereby granted   *
//* without fee, provided that the above copyright notice appears in all   *
//* copies and that both the copyright notice and this permission notice   *
//* appear in the supporting documentation. The authors make no claims     *
//* about the suitability of this software for any purpose. It is          *
//* provided "as is" without express or implied warranty.                  *
//**************************************************************************

/** @file   AliHLTEMCALAgent.cxx
 @author Federico Ronchetti
 @date   
 @brief  Agent of the libAliHLTEMCAL library
 */

#include "AliHLTEMCALAgent.h"
#include "AliHLTConfiguration.h"
#include "AliHLTEMCALDefinitions.h"
#include "AliHLTOUT.h"
#include "AliHLTOUTHandlerChain.h"
#include "AliHLTErrorGuard.h"
#include "AliRunLoader.h"
#include "AliDAQ.h"
#include "AliCDBManager.h"
#include "AliCDBEntry.h"


// #include "AliHLTEMCALConstant.h"
#include "AliHLTEMCALConstants.h"
#include "AliHLTEMCALMapper.h"

/** global instance for agent registration */
AliHLTEMCALAgent gAliHLTEMCALAgent;

// component headers
//#include "AliHLTEMCALCalibrationComponent.h"
#include "AliHLTCaloClusterAnalyser.h"
#include "AliHLTEMCALClusterizerComponent.h"
#include "AliHLTEMCALDigitMakerComponent.h"
//#include "AliHLTEMCALESDEntriesMakerComponent.h"
//#include "AliHLTEMCALHistogramProducerComponent.h"
//#include "AliHLTEMCALModuleCalibrationProcessorComponent.h"
//#include "AliHLTEMCALMonitorTriggerComponent.h"
#include "AliHLTEMCALRawAnalyzerComponent.h"
#include "AliHLTEMCALRawAnalyzerStandardComponent.h"
#include "AliHLTEMCALRawAnalyzerStandardFastComponent.h"
#include "AliHLTEMCALRawAnalyzerPeakFinderComponent.h"
#include "AliHLTEMCALRawAnalyzerComponentTRU.h"
#include "AliHLTEMCALRawAnalyzerComponentSTU.h"
#include "AliHLTEMCALTriggerDataMakerComponent.h"
//#include "AliHLTEMCALRcuCalibrationProcessorComponent.h"
//#include "AliHLTEMCALRcuDAComponent.h"
#include "AliHLTEMCALRawAnalyzerCrudeComponent.h"
#include "AliHLTEMCALRawAnalyzerLMSComponent.h"
#include "AliHLTEMCALRawAnalyzerFastFitComponent.h"
#include "AliHLTEMCALRawAnalyzerNNComponent.h"
#include "AliHLTEMCALClusterizerComponentNbyN.h"
#include "AliHLTEMCALTriggerMakerComponent.h"
#include "AliHLTEMCALTriggerQAComponent.h"
#include "AliHLTEMCALDigitsMonitorComponent.h"

/** ROOT macro for the implementation of ROOT specific class methods */
ClassImp(AliHLTEMCALAgent)

AliHLTEMCALAgent::AliHLTEMCALAgent() : AliHLTModuleAgent("EMCAL")
  , fRawDataHandler(NULL)
  , fMappers()
{
    // see header file for class documentation
    // or
    // refer to README to build package
    // or
    // visit http://web.ift.uib.no/~kjeks/doc/alice-hlt
}

AliHLTEMCALAgent::~AliHLTEMCALAgent()
{
    // see header file for class documentation
}

UInt_t AliHLTEMCALAgent::GetDetectorMask() const
{
  return AliDAQ::kEMCAL;
}

int AliHLTEMCALAgent::CreateConfigurations(AliHLTConfigurationHandler* handler,
                                           AliRawReader* /*rawReader*/,
                                           AliRunLoader* /*runloader*/) const
{
    // see header file for class documentation
    if (handler) 
    {
        //      const char* cdbEntry="EMCAL/Calib/Parameters";
        //AliCDBManager* pMan=AliCDBManager::Instance();
        //AliEMCALParam* pEMCALParam=NULL;
        
        //       if (pMan) 
        // 	{
        // 	  AliCDBEntry *pEntry = pMan->Get(cdbEntry);
        // 	  if (pEntry && 
        // 	      pEntry->GetObject() &&
        // 	      (pEMCALParam=dynamic_cast<AliEMCALParam*>(pEntry->GetObject()))) 
        // 	    {
        // 	    } else 
        // 	    {
        // 	      HLTWarning("can not load AliEMCALParam from CDB entry %s", cdbEntry);
        // 	    }
        // 	}
        
        Int_t ddlOffset = 4608; 
        
        TString mergerInput;
        TString sinkClusterInput;
        TString emInput;
        TString tmInput;   // Input for trigger maker
        TString tdInput;   // Input for trigger data maker
        TString arg;
        
        TString clInput, rps;

        for (int module = 0; module <= AliDAQ::NumberOfDdls("EMCAL"); module++)
        {
           TString publisher, ra, ta, dm;
           // raw data publisher components
           publisher.Form("EMCAL-RP_%02d", module);
           arg.Form("-verbose -minid %d -datatype 'DDL_RAW ' 'EMCA'  -dataspec %d ", ddlOffset + module, module);
                
           if(rps.Length()) rps += " ";
           rps += publisher;
           handler->CreateConfiguration(publisher.Data(), "AliRawReaderPublisher", NULL , arg.Data());
        }
                
       // Raw analyzer
       arg = "";
       handler->CreateConfiguration("EMCAL-RA", "EmcalRawCrude", rps.Data(), arg.Data());
                
       // Raw analyzer for TRU data
       arg = "";
       handler->CreateConfiguration("EMCAL-TRU", "EmcalTruAnalyzer", rps.Data(), arg.Data());
       if(tdInput.Length() > 0) tdInput += " ";
       tdInput += "EMCAL-TRU";

       // STU raw analyser
       handler->CreateConfiguration("EMCAL-STU", "EmcalStuAnalyzer", rps.Data(), "");
       tdInput += " EMCAL-STU";

       // digit maker components
       arg="";
       arg.Form("-sethighgainfactor 0.0153 -setlowgainfactor 0.2448 -setdigitthresholds 0.005 0.002");
       handler->CreateConfiguration("EMCAL-DM", "EmcalDigitMaker", "EMCAL-RA", arg.Data());
       if(tmInput.Length() > 0) tmInput += " ";
       tmInput+ "EMCAL-DM";

       // digit monitor componentl
       handler->CreateConfiguration("EMCAL-DQA", "EmcalDigitsMonitor", "EMCAL-DM", "");

            
       arg = "";
       arg.Form("-digitthreshold 0.005 -recpointthreshold 0.1 -modulemode");
       handler->CreateConfiguration("EMCAL-CF", "EmcalClusterizer", clInput.Data(), arg.Data());
            
       //ca.Form("EMCAL-CA_%02d", module);
       //arg = " ";
       //handler->CreateConfiguration(ca.Data(), "CaloClusterAnalyser", cl.Data(), arg.Data());
            

        // Tigger data merger
       handler->CreateConfiguration("EMCAL-TRG", "EmcalTriggerDataMaker", tdInput.Data(), "");
       tmInput += " EMCAL-TRG";

       handler->CreateConfiguration("EMCAL-TM", "EmcalTriggerMaker", tmInput.Data(), "");
        
       TString em;
        
        // tracker finder components
        
        // later
        //      em.Form("EMCAL-EM");
        //arg = " ";
        //handler->CreateConfiguration(em.Data(), "EmcalEsdEntriesMaker", emInput.Data(), " ");
        
    }
    return 0;
}

const char* AliHLTEMCALAgent::GetReconstructionChains(AliRawReader* /*rawReader*/,
                                                      AliRunLoader* runloader) const
{
    // see header file for class documentation
    if (runloader) {
        // reconstruction chains for AliRoot simulation
        // Note: run loader is only available while running embedded into
        // AliRoot simulation
        
        // if (runloader->GetLoader("EMCALLoader") != NULL)
        //     return "EMCAL-EM";
    }
    return NULL;
}

const char* AliHLTEMCALAgent::GetRequiredComponentLibraries() const
{
    // see header file for class documentation
    return NULL;
}

int AliHLTEMCALAgent::RegisterComponents(AliHLTComponentHandler* pHandler) const
{
    // see header file for class documentation
    if (!pHandler) return -EINVAL;
    
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerStandardComponent);
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerStandardFastComponent);
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerCrudeComponent);
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerLMSComponent);
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerPeakFinderComponent);
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerFastFitComponent);
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerNNComponent);
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerComponentTRU);
    pHandler->AddComponent(new AliHLTEMCALRawAnalyzerComponentSTU);
    pHandler->AddComponent(new AliHLTEMCALTriggerDataMakerComponent);
    pHandler->AddComponent(new AliHLTEMCALDigitMakerComponent);
    pHandler->AddComponent(new AliHLTEMCALClusterizerComponent);
    pHandler->AddComponent(new AliHLTEMCALClusterizerComponentNbyN);
    //pHandler->AddComponent(new AliHLTCaloClusterAnalyserComponent);			 
    //pHandler->AddComponent(new AliHLTEMCALESDEntriesMakerComponent);
    pHandler->AddComponent(new AliHLTEMCALTriggerMakerComponent);
    pHandler->AddComponent(new AliHLTEMCALTriggerQAComponent);
    pHandler->AddComponent(new AliHLTEMCALDigitsMonitorComponent);
    return 0;
}


int AliHLTEMCALAgent::GetHandlerDescription(AliHLTComponentDataType dt,
                                            AliHLTUInt32_t spec,
                                            AliHLTOUTHandlerDesc& desc) const
{
    // see header file for class documentation

    // raw data blocks to be fed into offline reconstruction
    if (dt==(kAliHLTDataTypeDDLRaw|kAliHLTDataOriginEMCAL)) 
    {
        AliHLTEMCALMapper* pMapper=GetMapper(spec);
    
        if(pMapper && pMapper->GetDDLFromSpec(spec) >= 0)
        {
            desc=AliHLTOUTHandlerDesc(kRawReader, dt, GetModuleId());
            return 1;
        } 
	else if (pMapper==NULL)
        {
	    ALIHLTERRORGUARD(5, "failed to create EMCAL mapper");
            return 0;
        }
        else 
        {
            HLTWarning("Handler can not process data inconsistent with a single EMCAL DDL from specification % d", spec);
            return 0;
        }
    }
    return 0;
}

AliHLTOUTHandler* AliHLTEMCALAgent::GetOutputHandler(AliHLTComponentDataType dt,
                                                     AliHLTUInt32_t /*spec*/)
{
    // see header file for class documentation
    
    // raw data blocks to be fed into offline reconstruction
    if (dt==(kAliHLTDataTypeDDLRaw|kAliHLTDataOriginEMCAL)) 
    {
        if (!fRawDataHandler) 
        {
            fRawDataHandler = new AliHLTEMCALAgent::AliHLTEMCALRawDataHandler(this);
        }
        return fRawDataHandler;
    }
    
    return NULL;
}

int AliHLTEMCALAgent::DeleteOutputHandler(AliHLTOUTHandler* pInstance)
{
    // see header file for class documentation
    if (pInstance==NULL) return -EINVAL;
    
    if (pInstance==fRawDataHandler) {
        delete fRawDataHandler;
        fRawDataHandler=NULL;
    }
    return 0;
}

AliHLTEMCALAgent::AliHLTEMCALRawDataHandler::AliHLTEMCALRawDataHandler(AliHLTEMCALAgent* pAgent)
  : fpAgent(pAgent)
{
    // see header file for class documentation
}

AliHLTEMCALAgent::AliHLTEMCALRawDataHandler::~AliHLTEMCALRawDataHandler()
{
    // see header file for class documentation
}

int AliHLTEMCALAgent::AliHLTEMCALRawDataHandler::ProcessData(AliHLTOUT* pData)
{
    // see header file for class documentation
    if (!pData) return -EINVAL;
    
    AliHLTComponentDataType dt = kAliHLTVoidDataType;
    AliHLTUInt32_t spec=kAliHLTVoidDataSpec;
    
    AliHLTEMCALMapper* pMapper=fpAgent?fpAgent->GetMapper(spec):NULL;
    if (!pMapper) {
      ALIHLTERRORGUARD(5, "%s", fpAgent?"can not retrieve EMCAL mapper from agent":"agent not available to retrieve EMCAL mapper");
      return -ENODEV;
    }
    
    int iResult = pData->GetDataBlockDescription(dt, spec);
    if (iResult>=0) 
    {
        int ddl = -1;
        if((ddl = pMapper->GetDDLFromSpec(spec)) >=0)
        {
            iResult = ddl;
        }
    } 
    else 
    {
        HLTError("Handler can not process data inconsistent with a single EMCAL DDL from specification % d", spec);
        iResult=-EBADMSG;
    }
    return iResult;
}

AliHLTEMCALMapper* AliHLTEMCALAgent::GetMapper(AliHLTUInt32_t spec) const
{
  // get the mapper instance for a specification
  std::map<AliHLTUInt32_t, AliHLTEMCALMapper*>::const_iterator element=fMappers.find(spec);
  if (element!=fMappers.end()) return element->second;

  AliHLTEMCALMapper* mapper=new AliHLTEMCALMapper(spec);
  if (!mapper) return NULL;
  mapper->InitDDLSpecificationMapping();
  const_cast<AliHLTEMCALAgent*>(this)->fMappers[spec]=mapper;
  return mapper;
}
