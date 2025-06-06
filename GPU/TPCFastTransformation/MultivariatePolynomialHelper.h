// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file MultivariatePolynomialHelper.h
/// \author Matthias Kleiner <mkleiner@ikf.uni-frankfurt.de>

#ifndef ALICEO2_TPC_MULTIVARIATEPOLYNOMIALHELPER
#define ALICEO2_TPC_MULTIVARIATEPOLYNOMIALHELPER

#include "GPUCommonDef.h"

#if !defined(GPUCA_GPUCODE)
#include <vector>
#include <string>
#include <cassert>
#if !defined(GPUCA_NO_FMT)
#include <fmt/format.h>
#endif
#endif

class TLinearFitter;

namespace GPUCA_NAMESPACE::gpu
{

#if !defined(GPUCA_GPUCODE)

/// simple struct to enable writing the MultivariatePolynomial to file
struct MultivariatePolynomialContainer {

  /// constructor
  /// \param dim number of dimensions of the polynomial
  /// \param degree degree of the polynomials
  /// \param nParameters number of parameters
  /// \param params parmaeters
  MultivariatePolynomialContainer(const uint32_t dim, const uint32_t degree, const uint32_t nParameters, const float params[/* nParameters*/], const bool interactionOnly) : mDim{dim}, mDegree{degree}, mParams{params, params + nParameters}, mInteractionOnly{interactionOnly} {};

  /// for ROOT I/O
  MultivariatePolynomialContainer() = default;

  const uint32_t mDim{};              ///< number of dimensions of the polynomial
  const uint32_t mDegree{};           ///< degree of the polynomials
  const std::vector<float> mParams{}; ///< parameters of the polynomial
  const bool mInteractionOnly{};      ///< consider only interaction terms
};
#endif

/// Helper class for calculating the number of parameters for a multidimensional polynomial
class MultivariatePolynomialParametersHelper
{
 public:
  /// \returns number of parameters for given dimension and degree of polynomials
  /// calculates the number of parameters for a multivariate polynomial for given degree: nParameters = (n+d-1 d) -> binomial coefficient
  /// see: https://mathoverflow.net/questions/225953/number-of-polynomial-terms-for-certain-degree-and-certain-number-of-variables
  GPUd() static constexpr uint32_t getNParametersAllTerms(const uint32_t degree, const uint32_t dim) { return (degree == 0) ? binomialCoeff(dim - 1, 0) : binomialCoeff(dim - 1 + degree, degree) + getNParametersAllTerms(degree - 1, dim); }

  /// \returns the number of parameters for interaction terms only (see: https://en.wikipedia.org/wiki/Combination)
  GPUd() static constexpr uint32_t getNParametersInteractionOnly(const uint32_t degree, const uint32_t dim) { return (degree == 0) ? binomialCoeff(dim - 1, 0) : binomialCoeff(dim, degree) + getNParametersInteractionOnly(degree - 1, dim); }

  GPUd() static constexpr uint32_t getNParameters(const uint32_t degree, const uint32_t dim, const bool interactionOnly)
  {
    if (interactionOnly) {
      return getNParametersInteractionOnly(degree, dim);
    } else {
      return getNParametersAllTerms(degree, dim);
    }
  }

 private:
  /// calculate factorial of n
  /// \return returns n!
  GPUd() static constexpr uint32_t factorial(const uint32_t n) { return (n == 0) || (n == 1) ? 1 : n * factorial(n - 1); }

  /// calculates binomial coefficient
  /// \return returns (n k)
  GPUd() static constexpr uint32_t binomialCoeff(const uint32_t n, const uint32_t k) { return factorial(n) / (factorial(k) * factorial(n - k)); }
};

/// Helper struct for evaluating a multidimensional polynomial using compile time evaluated formula
/// Compile time method to extract the formula is obtained from run time method (check combination_with_repetiton() and evalPol())
/// by performing all loops during compile time and replacing the array to keep track of the dimensions for given term (pos[FMaxdegree + 1])
/// to a simple uint32_t called Pos where each digit represents the dimension for a given term e.g. pos = 2234 -> x[2]*x[2]*x[3]*x[4]
///
template <uint32_t Dim, uint32_t Degree, bool InteractionOnly>
class MultivariatePolynomialHelper : public MultivariatePolynomialParametersHelper
{
  static constexpr uint16_t FMaxdim = 10;   ///< maximum dimensionality of the polynomials (number of different digits: 0,1,2,3....9 )
  static constexpr uint16_t FMaxdegree = 9; ///< maximum degree of the polynomials (maximum number of digits in unsigned integer - 1)

#if !defined(GPUCA_GPUCODE)
  static_assert(Dim <= MultivariatePolynomialHelper<Dim, Degree, InteractionOnly>::FMaxdim && Degree <= MultivariatePolynomialHelper<Dim, Degree, InteractionOnly>::FMaxdegree, "Max. number of dimensions or degrees exceeded!");
#endif

 public:
  /// evaluates the polynomial for given parameters and coordinates
  /// \param par parameters of the polynomials
  /// \param x input coordinates
  GPUd() static constexpr float evalPol(GPUgeneric() const float par[/*number of parameters*/], const float x[/*number of dimensions*/]) { return par[0] + loopDegrees<1>(par, x); }

  /// \return returns number of dimensions of the polynomials
  GPUd() static constexpr uint32_t getDim() { return Dim; }

  /// \return returns the degree of the polynomials
  GPUd() static constexpr uint32_t getDegree() { return Degree; }

  /// \return returns whether only interaction terms are considered
  GPUd() static constexpr bool isInteractionOnly() { return InteractionOnly; }

 private:
  /// computes power of 10
  GPUd() static constexpr uint32_t pow10(const uint32_t n) { return n == 0 ? 1 : 10 * pow10(n - 1); }

  /// helper for modulo to extract the digit in an integer a at position b (can be obtained with pow10(digitposition)): e.g. a=1234 b=pow10(2)=100 -> returns 2
  GPUd() static constexpr uint32_t mod10(const uint32_t a, const uint32_t b) { return (a / b) % 10; }

  /// resetting digits of pos for given position to refDigit
  GPUd() static constexpr uint32_t resetIndices(const uint32_t degreePol, const uint32_t pos, const uint32_t leftDigit, const uint32_t iter, const uint32_t refDigit);

  GPUd() static constexpr uint32_t getNewPos(const uint32_t degreePol, const uint32_t pos, const uint32_t digitPos);

  /// calculates term e.g. x^3*y
  /// \tparam DegreePol max degree of the polynomials
  /// \pos decoded information about the current term e.g. 1233 -> x[1]*x[2]*x[3]*x[3] (otherwise an array could be used)
  template <uint32_t DegreePol>
  GPUd() static constexpr float prodTerm(const float x[], const uint32_t pos);

  /// helper function for checking for interaction terms
  template <uint32_t DegreePol, uint32_t posNew>
  static constexpr bool checkInteraction();

  /// calculate sum of the terms for given degree -> summation of the par[]*x^4 + par[]*x^3*y + par[]*x^3*z + par[]*x^2*y^2..... terms
  /// \tparam DegreePol max degree of the polynomials
  /// \Pos decoded information about the current term e.g. 1233 -> x[1]*x[2]*x[3]*x[3] (otherwise an array could be used)
  /// \tparam Index index for accessing the parameters
  template <uint32_t DegreePol, uint32_t Pos, uint32_t Index>
  GPUd() static constexpr float sumTerms(GPUgeneric() const float par[], const float x[]);

  /// loop over the degrees of the polynomials (for formula see https://math.stackexchange.com/questions/1234240/equation-that-defines-multi-dimensional-polynomial)
  /// \tparam degree iteration of the loop which starts from 1 to the max degree of the polynomial e.g. Iter=4 -> summation of the par[]*x^4 + par[]*x^3*y + par[]*x^3*z + par[]*x^2*y^2..... terms
  /// \param par parameters of the pokynomial
  /// \param x input coordinates
  template <uint32_t DegreePol>
  GPUd() static constexpr float loopDegrees(GPUgeneric() const float par[], const float x[]);
};

#if !defined(GPUCA_GPUCODE) // disable specialized class for GPU, as it should be only used for the fitting!
/// Helper struct for evaluating a multidimensional polynomial using run time evaluated formula
template <>
class MultivariatePolynomialHelper<0, 0, false> : public MultivariatePolynomialParametersHelper
{
 public:
  /// constructor
  /// \param nDim dimensionality of the polynomials
  /// \param degree degree of the polynomials
  MultivariatePolynomialHelper(const uint32_t nDim, const uint32_t degree, const bool interactionOnly) : mDim{nDim}, mDegree{degree}, mInteractionOnly{interactionOnly} { assert(mDegree <= FMaxdegree); };

  /// default constructor
  MultivariatePolynomialHelper() = default;

  /// Destructor
  ~MultivariatePolynomialHelper() = default;

  /// printing the formula of the polynomial
  void print() const;

  /// \return the formula as a string
  std::string getFormula() const;

  /// \return returns the formula which can be used for the TLinearFitter
  std::string getTLinearFitterFormula() const;

  /// \return returns the terms which are used to evaluate the polynomial
  std::vector<std::string> getTerms() const;

  /// performs fit of input points (this function can be used for mutliple fits without creating the TLinearFitter for each fit again)
  /// \return returns parameters of the fit
  /// \param fitter fitter which is used to fit the points ()
  /// \param x position of the points of length 'Dim * nPoints' (structured as in https://root.cern.ch/doc/master/classTLinearFitter.html)
  /// \param y values which will be fitted of length 'nPoints'
  /// \param error error of weigths of the points (if empty no weights are applied)
  /// \param clearPoints set to true if the fit is performed on new data points
  static std::vector<float> fit(TLinearFitter& fitter, std::vector<double>& x, std::vector<double>& y, std::vector<double>& error, const bool clearPoints);

  /// performs fit of input points
  /// \return returns parameters of the fit
  /// \param x position of the points of length 'Dim * nPoints' (structured as in https://root.cern.ch/doc/master/classTLinearFitter.html)
  /// \param y values which will be fitted of length 'nPoints'
  /// \param error error of weigths of the points (if empty no weights are applied)
  /// \param clearPoints set to true if the fit is performed on new data points
  std::vector<float> fit(std::vector<double>& x, std::vector<double>& y, std::vector<double>& error, const bool clearPoints) const;

  /// evaluating the polynomial
  /// \param par coefficients of the polynomial
  /// \param x input coordinates
  float evalPol(const float par[/*number of parameters*/], const float x[/*number of dimensions*/]) const { return evalPol(par, x, mDegree, mDim, mInteractionOnly); }

  /// evalutes the polynomial
  float evalPol(const float par[], const float x[], const uint32_t degree, const uint32_t dim, const bool interactionOnly) const;

  /// \return returns number of dimensions of the polynomials
  uint32_t getDim() const { return mDim; }

  /// \return returns the degree of the polynomials
  uint32_t getDegree() const { return mDegree; }

  /// \return returns whether only interaction terms are considered
  bool isInteractionOnly() const { return mInteractionOnly; }

 protected:
  uint32_t mDim{};         ///< dimensionality of the polynomial
  uint32_t mDegree{};      ///< maximum degree of the polynomial
  bool mInteractionOnly{}; ///< flag if only interaction terms are used

 private:
  static constexpr uint16_t FMaxdegree = 9; ///< maximum degree of the polynomials (can be increased if desired: size of array in combination_with_repetiton: pos[FMaxdegree + 1])

  /// helper function to get all combinations
  template <class Type>
  Type combination_with_repetiton(const uint32_t degree, const uint32_t dim, const float par[], int32_t& indexPar, const float x[], const bool interactionOnly) const;
};
#endif

//=================================================================================
//============================ inline implementations =============================
//=================================================================================

template <uint32_t Dim, uint32_t Degree, bool InteractionOnly>
GPUd() constexpr uint32_t MultivariatePolynomialHelper<Dim, Degree, InteractionOnly>::resetIndices(const uint32_t degreePol, const uint32_t pos, const uint32_t leftDigit, const uint32_t iter, const uint32_t refDigit)
{
  if (iter <= degreePol) {
    const int32_t powTmp = pow10(leftDigit);
    const int32_t rightDigit = mod10(pos, powTmp);
    const int32_t posTmp = pos - (rightDigit - refDigit) * powTmp;
    return resetIndices(degreePol, posTmp, leftDigit - 1, iter + 1, refDigit);
  }
  return pos;
}

template <uint32_t Dim, uint32_t Degree, bool InteractionOnly>
GPUd() constexpr uint32_t MultivariatePolynomialHelper<Dim, Degree, InteractionOnly>::getNewPos(const uint32_t degreePol, const uint32_t pos, const uint32_t digitPos)
{
  if (degreePol > digitPos) {
    // check if digit of current position is at is max position
    if (mod10(pos, pow10(digitPos)) == Dim) {
      // increase digit of left position
      const uint32_t leftDigit = digitPos + 1;
      const uint32_t posTmp = pos + pow10(leftDigit);
      const uint32_t refDigit = mod10(posTmp, pow10(digitPos + 1));

      // resetting digits to the right if digit exceeds number of dimensions
      const uint32_t posReset = resetIndices(degreePol, posTmp, leftDigit - 1, degreePol - digitPos, refDigit);

      // check next digit
      return getNewPos(degreePol, posReset, digitPos + 1);
    }
    return getNewPos(degreePol, pos, digitPos + 1);
  }
  return pos;
}

template <uint32_t Dim, uint32_t Degree, bool InteractionOnly>
template <uint32_t DegreePol>
GPUd() constexpr float MultivariatePolynomialHelper<Dim, Degree, InteractionOnly>::prodTerm(const float x[], const uint32_t pos)
{
  if constexpr (DegreePol > 0) {
    // extract index of the dimension which is decoded in the digit
    const uint32_t index = mod10(pos, pow10(DegreePol - 1));
    return x[index] * prodTerm<DegreePol - 1>(x, pos);
  }
  return 1;
}

template <uint32_t Dim, uint32_t Degree, bool InteractionOnly>
template <uint32_t DegreePol, uint32_t posNew>
constexpr bool MultivariatePolynomialHelper<Dim, Degree, InteractionOnly>::checkInteraction()
{
  if constexpr (DegreePol > 1) {
    constexpr bool isInteraction = mod10(posNew, pow10(DegreePol - 1)) == mod10(posNew, pow10(DegreePol - 2));
    if constexpr (isInteraction) {
      return true;
    }
    return checkInteraction<DegreePol - 1, posNew>();
  }
  return false;
}

template <uint32_t Dim, uint32_t Degree, bool InteractionOnly>
template <uint32_t DegreePol, uint32_t Pos, uint32_t Index>
GPUd() constexpr float MultivariatePolynomialHelper<Dim, Degree, InteractionOnly>::sumTerms(GPUgeneric() const float par[], const float x[])
{
  // checking if the current position is reasonable e.g. if the max dimension is x[4]: for Pos=15 -> x[1]*x[5] the position is set to 22 -> x[2]*x[2]
  constexpr uint32_t posNew = getNewPos(DegreePol, Pos, 0);
  if constexpr (mod10(posNew, pow10(DegreePol)) != 1) {

    // check if all digits in posNew are unequal: For interaction_only terms with x[Dim]*x[Dim]... etc. can be skipped
    if constexpr (InteractionOnly && checkInteraction<DegreePol, posNew>()) {
      return sumTerms<DegreePol, posNew + 1, Index>(par, x);
    }

    // sum up the term for corrent term and set posotion for next combination
    return par[Index] * prodTerm<DegreePol>(x, posNew) + sumTerms<DegreePol, posNew + 1, Index + 1>(par, x);
  }
  return 0;
}

template <uint32_t Dim, uint32_t Degree, bool InteractionOnly>
template <uint32_t DegreePol>
GPUd() constexpr float MultivariatePolynomialHelper<Dim, Degree, InteractionOnly>::loopDegrees(GPUgeneric() const float par[], const float x[])
{
  if constexpr (DegreePol <= Degree) {
    constexpr uint32_t index{getNParameters(DegreePol - 1, Dim, InteractionOnly)}; // offset of the index for accessing the parameters
    return sumTerms<DegreePol, 0, index>(par, x) + loopDegrees<DegreePol + 1>(par, x);
  }
  return 0;
}

} // namespace GPUCA_NAMESPACE::gpu

#endif
