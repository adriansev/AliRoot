void CreateNoiseRunHisto(Int_t iLDC=0){

  // macro to create histogram as if from pulser data

  AliTOFGeometry * geom = new AliTOFGeometry();
  AliTOFDecoder * decoder = new AliTOFDecoder();
  Int_t array0[2160];
  Int_t array1[2208];
  Int_t array2[2160];
  Int_t array3[2208];
  Int_t array4[2160];
  Int_t array5[2208];
  // simulating data from LDC reading DDL # iLDC*6+{0,1,2,3,4,5}
  Int_t iDDL[6];
  for (Int_t j=0;j<6;j++){
    iDDL[j]=j+iLDC*6;
  }
  decoder->GetArrayDDL(array0,iDDL[0]);
  cout << " DDL " << iDDL[0] << " done " << endl;
  decoder->GetArrayDDL(array1,iDDL[1]);
  cout << " DDL " << iDDL[1] << " done " << endl;
  decoder->GetArrayDDL(array2,iDDL[2]);
  cout << " DDL " << iDDL[2] << " done " << endl;
  decoder->GetArrayDDL(array3,iDDL[3]);
  cout << " DDL " << iDDL[3] << " done " << endl;
  decoder->GetArrayDDL(array4,iDDL[4]);
  cout << " DDL " << iDDL[4] << " done " << endl;
  decoder->GetArrayDDL(array5,iDDL[5]);
  cout << " DDL " << iDDL[5] << " done " << endl;

  // debugging printings
  for (Int_t i=0;i<2160;i++){
    cout << " array0[" << i << "] = " << array0[i] << endl;
    cout << " array2[" << i << "] = " << array2[i] << endl;
    cout << " array4[" << i << "] = " << array4[i] << endl;
  }
  for (Int_t i=0;i<2208;i++){
    cout << " array1[" << i << "] = " << array1[i] << endl;
    cout << " array3[" << i << "] = " << array3[i] << endl;
    cout << " array5[" << i << "] = " << array5[i] << endl;
  }

  static const Int_t size = AliTOFGeometry::NPadXSector()*AliTOFGeometry::NSectors();
  cout << " size = " << size << endl;
  TH1F::AddDirectory(0);
  TH1F * htofNoise = new TH1F("hTOFnoise","histo with signals on TOF during noise", size,-0.5,size-0.5);
  for (Int_t ibin=1;ibin<=size;ibin++){
    htofNoise->SetBinContent(ibin,-1);
  }

  for (Int_t i=0;i<2160;i++){
    if (htofNoise->GetBinContent(array0[i]+1)<0) htofNoise->SetBinContent(array0[i]+1,0);
    if (htofNoise->GetBinContent(array2[i]+1)<0) htofNoise->SetBinContent(array2[i]+1,0);
    if (htofNoise->GetBinContent(array4[i]+1)<0) htofNoise->SetBinContent(array4[i]+1,0);
  }

  for (Int_t i=0;i<2208;i++){
    if (htofNoise->GetBinContent(array1[i]+1)<0) htofNoise->SetBinContent(array1[i]+1,0);
    if (htofNoise->GetBinContent(array3[i]+1)<0) htofNoise->SetBinContent(array3[i]+1,0);
    if (htofNoise->GetBinContent(array5[i]+1)<0) htofNoise->SetBinContent(array5[i]+1,0);
  }

  const Int_t ndead =3;
  Int_t idead[18]; // indexes of dead channels
  Float_t ifloatdead[ndead]; // indexes of dead channels
  TRandom *rnd = new TRandom(4357);
  TDatime *dt=new TDatime();
  TDatime ciccio;
  rnd->SetSeed(ciccio.Get());
  rnd->RndmArray(ndead,ifloatdead);

  for (Int_t i=0;i<ndead;i++){
    Int_t index = Int_t(ifloatdead[i]*2160);
    idead[i*3] = array0[index];
    idead[i*3+1] = array2[index];
    idead[i*3+2] = array4[index];
    cout << " channels " << idead[i*3] << ", " << idead[i*3+1] << ", " << idead[i*3+2] << ", " << " will be noisy " << endl;
  }

  for (Int_t i=0;i<ndead;i++){
    Int_t index = Int_t(ifloatdead[i]*2208);
    idead[i*3+9] = array1[index];
    idead[i*3+9+1] = array3[index];
    idead[i*3+9+2] = array5[index];
    cout << " channels " << idead[i*3+9] << ", " << idead[i*3+9+1] << ", " << idead[i*3+9+2] << ", " << " will be noisy " << endl;
  }

  for (Int_t i=0;i<ndead*6;i++){
    cout << " noisy[" << i << "] = " << idead[i] << endl;
  }

  Bool_t tobeskipped=kFALSE;
  for (Int_t i=0;i<size;i++){
    tobeskipped = kFALSE;
    for (Int_t j=0;j<ndead*6;j++){
      if (i==idead[j]) {
	tobeskipped=kTRUE;
	cout << " noisy channel " << i << " with idead = " << idead[j] << endl;
	break;
      }
    }
    if (tobeskipped && (htofNoise->GetBinContent(i+1)!=-1)) htofNoise->SetBinContent(i+1,2.2);
    else {
      //      cout << " filling channel " << i << endl;
      //      htofNoise->Fill(i,10000);
    }
  }
  TCanvas *c = new TCanvas("c","c",-2,30,500,500);
  htofNoise->Draw();
  char filename[100];
  snprintf(filename,100,"$ALICE_ROOT/TOF/ShuttleInput/TOFoutNoiseLDC_%02i.root",iLDC);
  TFile *fileout = new TFile(filename,"RECREATE");
  htofNoise->Write("");
  fileout->Close();
}
