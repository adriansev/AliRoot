void aod(){

    gSystem->Load("libboost_unit_test_framework");
    gSystem->Load("libzmq");
    
    gSystem->Load("libANALYSIS");
    gSystem->Load("libANALYSISalice");

    if( gSystem->Load("libCORRFW") <0) {
      cerr << "Error: AliPhysics is not installed, no AOD test is possible!" << endl;
      return;
    }
    gSystem->Load("libPWGHFbase");
    gSystem->Load("libPWGmuon");
    gSystem->Load("libESDfilter");
    gSystem->Load("libTender");
    gSystem->Load("libPWGPP");

    AliGeomManager::LoadGeometry("geometry.root");
    AliEMCALGeometry::GetInstance("EMCAL_COMPLETEV1");

    gROOT->Macro("${ALICE_ROOT}/STEER/macros/CreateAODfromESD.C(\"AliESDs.root\",\"AliAOD.root\",\"local://$ALIROOT_OCDB_ROOT/OCDB\",\"local://.\")");
}
