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

/// \file GPUDisplayInterpolation.cxx
/// \author David Rohr

#include <cstdio>
#include "GPUDisplay.h"

using namespace GPUCA_NAMESPACE::gpu;

void GPUDisplay::opengl_spline::create(const vecpod<float>& x, const vecpod<float>& y)
{
  ma.clear();
  mb.clear();
  mc.clear();
  md.clear();
  mx.clear();
  if (x.size() != y.size() || x.size() < 2) {
    return;
  }
  int32_t k = x.size() - 1;
  if (mVerbose) {
    for (uint32_t i = 0; i < x.size(); i++) {
      GPUInfo("Point %u: %f --> %f", i, x[i], y[i]);
    }
  }
  ma.resize(k + 1);
  mb.resize(k + 1);
  mc.resize(k + 1);
  md.resize(k + 1);
  mx.resize(k + 1);
  vecpod<float> h(k + 1), alpha(k + 1), l(k + 1), mu(k + 1), z(k + 1);
  for (int32_t i = 0; i <= k; i++) {
    ma[i] = y[i];
  }
  for (int32_t i = 0; i < k; i++) {
    h[i] = x[i + 1] - x[i];
  }
  for (int32_t i = 1; i < k; i++) {
    alpha[i] = 3.f / h[i] * (ma[i + 1] - ma[i]) - 3.f / h[i - 1] * (ma[i] - ma[i - 1]);
  }
  l[0] = l[k] = 1;
  mu[0] = z[0] = z[k] = mc[k] = 0;
  for (int32_t i = 1; i < k; i++) {
    l[i] = 2.f * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1];
    mu[i] = h[i] / l[i];
    z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
  }
  for (int32_t i = k - 1; i >= 0; i--) {
    mc[i] = z[i] - mu[i] * mc[i + 1];
    mb[i] = (ma[i + 1] - ma[i]) / h[i] - h[i] / 3.f * (mc[i + 1] + 2.f * mc[i]);
    md[i] = (mc[i + 1] - mc[i]) / (3.f * h[i]);
  }
  for (int32_t i = 0; i <= k; i++) {
    mx[i] = x[i];
  }
}

float GPUDisplay::opengl_spline::evaluate(float x)
{
  int32_t base = 0;
  const int32_t k = mx.size() - 1;
  if (k < 0) {
    return (0);
  }
  while (base < k - 1 && x > mx[base + 1]) {
    base++;
  }
  float retVal = ma[base];
  x -= mx[base];
  const float xx = x;
  retVal += mb[base] * x;
  x *= xx;
  retVal += mc[base] * x;
  x *= xx;
  retVal += md[base] * x;
  if (mVerbose) {
    GPUInfo("Evaluate: %f --> %f (basepoint %d)", xx, retVal, base);
  }
  return (retVal);
}
