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

/// \file GPUTPCMCTrack.cxx
/// \author Sergey Gorbunov, Ivan Kisel, David Rohr

#include "GPUTPCMCTrack.h"
#include "GPUCommonMath.h"
#include "TDatabasePDG.h"
#include "TParticle.h"

GPUTPCMCTrack::GPUTPCMCTrack() : fPDG(0), fP(0), fPt(0), mNHits(0), fNMCPoints(0), fFirstMCPointID(0), fNReconstructed(0), fSet(0), fNTurns(0)
{
  //* Default constructor
  for (int32_t i = 0; i < 7; i++) {
    fPar[i] = 0;
    fTPCPar[i] = 0;
  }
}

GPUTPCMCTrack::GPUTPCMCTrack(const TParticle* part) : fPDG(0), fP(0), fPt(0), mNHits(0), fNMCPoints(0), fFirstMCPointID(0), fNReconstructed(0), fSet(0), fNTurns(0)
{
  //* Constructor from TParticle

  for (int32_t i = 0; i < 7; i++) {
    fPar[i] = 0;
  }
  for (int32_t i = 0; i < 7; i++) {
    fTPCPar[i] = 0;
  }
  fP = 0;
  fPt = 0;

  if (!part) {
    return;
  }
  TLorentzVector mom, vtx;
  part->ProductionVertex(vtx);
  part->Momentum(mom);
  fPar[0] = part->Vx();
  fPar[1] = part->Vy();
  fPar[2] = part->Vz();
  fP = part->P();
  fPt = part->Pt();
  double pi = (fP > 1.e-4) ? 1. / fP : 0;
  fPar[3] = part->Px() * pi;
  fPar[4] = part->Py() * pi;
  fPar[5] = part->Pz() * pi;
  fPar[6] = 0;
  fPDG = part->GetPdgCode();
  if (CAMath::Abs(fPDG) < 100000) {
    TParticlePDG* pPDG = TDatabasePDG::Instance()->GetParticle(fPDG);
    if (pPDG) {
      fPar[6] = pPDG->Charge() / 3.0 * pi;
    }
  }
}

void GPUTPCMCTrack::SetTPCPar(float X, float Y, float Z, float Px, float Py, float Pz)
{
  //* Set parameters at TPC entrance

  for (int32_t i = 0; i < 7; i++) {
    fTPCPar[i] = 0;
  }

  fTPCPar[0] = X;
  fTPCPar[1] = Y;
  fTPCPar[2] = Z;
  double p = CAMath::Sqrt(Px * Px + Py * Py + Pz * Pz);
  double pi = (p > 1.e-4) ? 1. / p : 0;
  fTPCPar[3] = Px * pi;
  fTPCPar[4] = Py * pi;
  fTPCPar[5] = Pz * pi;
  fTPCPar[6] = 0;
  if (CAMath::Abs(fPDG) < 100000) {
    TParticlePDG* pPDG = TDatabasePDG::Instance()->GetParticle(fPDG);
    if (pPDG) {
      fTPCPar[6] = pPDG->Charge() / 3.0 * pi;
    }
  }
}
