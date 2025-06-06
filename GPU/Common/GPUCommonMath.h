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

/// \file GPUCommonMath.h
/// \author David Rohr, Sergey Gorbunov

#ifndef GPUCOMMONMATH_H
#define GPUCOMMONMATH_H

#include "GPUCommonDef.h"

#if defined(__CUDACC__) && !defined(__clang__) && !defined(GPUCA_GPUCODE_COMPILEKERNELS) && !defined(GPUCA_GPUCODE_HOSTONLY)
#include <sm_20_atomic_functions.h>
#endif

#if !defined(GPUCA_GPUCODE_DEVICE)
#include <cmath>
#include <algorithm>
#include <atomic>
#endif

#if !defined(GPUCA_GPUCODE_COMPILEKERNELS) && (!defined(GPUCA_GPUCODE_DEVICE) || defined(__CUDACC__) || defined(__HIPCC__))
#include <cstdint>
#endif

namespace GPUCA_NAMESPACE
{
namespace gpu
{

class GPUCommonMath
{
 public:
  GPUd() static float2 MakeFloat2(float x, float y); // TODO: Find better appraoch that is constexpr

  template <class T>
  GPUhd() static T Min(const T x, const T y);
  template <class T>
  GPUhd() static T Max(const T x, const T y);
  template <class T, class S, class R>
  GPUd() static T MinWithRef(T x, T y, S refX, S refY, R& r);
  template <class T, class S, class R>
  GPUd() static T MaxWithRef(T x, T y, S refX, S refY, R& r);
  template <class T, class S, class R>
  GPUd() static T MaxWithRef(T x, T y, T z, T w, S refX, S refY, S refZ, S refW, R& r);
  template <class T>
  GPUdi() static T Clamp(const T v, const T lo, const T hi)
  {
    return Max(lo, Min(v, hi));
  }
  GPUhdni() static float Sqrt(float x);
  GPUd() static float InvSqrt(float x);
  template <class T>
  GPUhd() static T Abs(T x);
  GPUd() static float ASin(float x);
  GPUd() static float ACos(float x);
  GPUd() static float ATan(float x);
  GPUhd() static float ATan2(float y, float x);
  GPUd() static float Sin(float x);
  GPUd() static float Cos(float x);
  GPUhdni() static void SinCos(float x, float& s, float& c);
  GPUhdni() static void SinCosd(double x, double& s, double& c);
  GPUd() static float Tan(float x);
  GPUd() static float Pow(float x, float y);
  GPUd() static float Log(float x);
  GPUd() static float Exp(float x);
  GPUhdni() static float Copysign(float x, float y);
  GPUd() static constexpr float TwoPi() { return 6.2831853f; }
  GPUd() static constexpr float Pi() { return 3.1415927f; }
  GPUd() static float Round(float x);
  GPUd() static float Floor(float x);
  GPUd() static uint32_t Float2UIntReint(const float& x);
  GPUd() static uint32_t Float2UIntRn(float x);
  GPUd() static int32_t Float2IntRn(float x);
  GPUd() static float Modf(float x, float y);
  GPUd() static bool Finite(float x);
  GPUd() static uint32_t Clz(uint32_t val);
  GPUd() static uint32_t Popcount(uint32_t val);

  GPUhdni() static float Hypot(float x, float y);
  GPUhdni() static float Hypot(float x, float y, float z);
  GPUhdni() static float Hypot(float x, float y, float z, float w);

  template <typename T>
  GPUhd() static void Swap(T& a, T& b);

  template <class T>
  GPUdi() static T AtomicExch(GPUglobalref() GPUgeneric() GPUAtomic(T) * addr, T val)
  {
    return GPUCommonMath::AtomicExchInternal(addr, val);
  }

  template <class T>
  GPUdi() static bool AtomicCAS(GPUglobalref() GPUgeneric() GPUAtomic(T) * addr, T cmp, T val)
  {
    return GPUCommonMath::AtomicCASInternal(addr, cmp, val);
  }

  template <class T>
  GPUdi() static T AtomicAdd(GPUglobalref() GPUgeneric() GPUAtomic(T) * addr, T val)
  {
    return GPUCommonMath::AtomicAddInternal(addr, val);
  }
  template <class T>
  GPUdi() static void AtomicMax(GPUglobalref() GPUgeneric() GPUAtomic(T) * addr, T val)
  {
    GPUCommonMath::AtomicMaxInternal(addr, val);
  }
  template <class T>
  GPUdi() static void AtomicMin(GPUglobalref() GPUgeneric() GPUAtomic(T) * addr, T val)
  {
    GPUCommonMath::AtomicMinInternal(addr, val);
  }
  template <class T>
  GPUdi() static T AtomicExchShared(GPUsharedref() GPUgeneric() GPUAtomic(T) * addr, T val)
  {
    return GPUCommonMath::AtomicExchInternal(addr, val);
  }
  template <class T>
  GPUdi() static T AtomicAddShared(GPUsharedref() GPUgeneric() GPUAtomic(T) * addr, T val)
  {
    return GPUCommonMath::AtomicAddInternal(addr, val);
  }
  template <class T>
  GPUdi() static void AtomicMaxShared(GPUsharedref() GPUgeneric() GPUAtomic(T) * addr, T val)
  {
    GPUCommonMath::AtomicMaxInternal(addr, val);
  }
  template <class T>
  GPUdi() static void AtomicMinShared(GPUsharedref() GPUgeneric() GPUAtomic(T) * addr, T val)
  {
    GPUCommonMath::AtomicMinInternal(addr, val);
  }
  GPUd() static int32_t Mul24(int32_t a, int32_t b);
  GPUd() static float FMulRZ(float a, float b);

  template <int32_t I, class T>
  GPUd() constexpr static T nextMultipleOf(T val);

  template <typename... Args>
  GPUdi() static float Sum2(float w, Args... args)
  {
    if constexpr (sizeof...(Args) == 0) {
      return w * w;
    } else {
      return w * w + Sum2(args...);
    }
    return 0;
  }

 private:
  template <class S, class T>
  GPUd() static uint32_t AtomicExchInternal(S* addr, T val);
  template <class S, class T>
  GPUd() static bool AtomicCASInternal(S* addr, T cmp, T val);
  template <class S, class T>
  GPUd() static uint32_t AtomicAddInternal(S* addr, T val);
  template <class S, class T>
  GPUd() static void AtomicMaxInternal(S* addr, T val);
  template <class S, class T>
  GPUd() static void AtomicMinInternal(S* addr, T val);
};

typedef GPUCommonMath CAMath;

// CHOICE Syntax: CHOICE(Host, CUDA&HIP, OpenCL)
#if defined(GPUCA_GPUCODE_DEVICE) && (defined(__CUDACC__) || defined(__HIPCC__)) // clang-format off
    #define CHOICE(c1, c2, c3) (c2) // Select second option for CUDA and HIP
#elif defined(GPUCA_GPUCODE_DEVICE) && defined (__OPENCL__)
    #define CHOICE(c1, c2, c3) (c3) // Select third option for OpenCL
#else
    #define CHOICE(c1, c2, c3) (c1) // Select first option for Host
#endif // clang-format on

template <int32_t I, class T>
GPUdi() constexpr T GPUCommonMath::nextMultipleOf(T val)
{
  if constexpr (I & (I - 1)) {
    T tmp = val % I;
    if (tmp) {
      val += I - tmp;
    }
    return val;
  } else {
    return (val + I - 1) & ~(T)(I - 1);
  }
  return 0; // BUG: Cuda complains about missing return value with constexpr if
}

GPUdi() float2 GPUCommonMath::MakeFloat2(float x, float y)
{
#if !defined(GPUCA_GPUCODE) || defined(__OPENCL__) || defined(__OPENCL_HOST__)
  float2 ret = {x, y};
  return ret;
#else
  return make_float2(x, y);
#endif // GPUCA_GPUCODE
}

GPUdi() float GPUCommonMath::Modf(float x, float y) { return CHOICE(fmodf(x, y), fmodf(x, y), fmod(x, y)); }

GPUdi() uint32_t GPUCommonMath::Float2UIntReint(const float& x)
{
#if defined(GPUCA_GPUCODE_DEVICE) && (defined(__CUDACC__) || defined(__HIPCC__))
  return __float_as_uint(x);
#elif defined(GPUCA_GPUCODE_DEVICE) && defined(__OPENCL__)
  return as_uint(x);
#else
  return reinterpret_cast<const uint32_t&>(x);
#endif
}

GPUdi() uint32_t GPUCommonMath::Float2UIntRn(float x) { return (uint32_t)(int32_t)(x + 0.5f); }
GPUdi() float GPUCommonMath::Floor(float x) { return CHOICE(floorf(x), floorf(x), floor(x)); }

#ifdef GPUCA_NO_FAST_MATH
GPUdi() float GPUCommonMath::Round(float x) { return CHOICE(roundf(x), roundf(x), round(x)); }
GPUdi() int32_t GPUCommonMath::Float2IntRn(float x) { return (int32_t)Round(x); }
GPUdi() bool GPUCommonMath::Finite(float x) { return CHOICE(std::isfinite(x), isfinite(x), true); }
GPUhdi() float GPUCommonMath::Sqrt(float x) { return CHOICE(sqrtf(x), (float)sqrt((double)x), sqrt(x)); }
GPUdi() float GPUCommonMath::ATan(float x) { return CHOICE((float)atan((double)x), (float)atan((double)x), atan(x)); }
GPUhdi() float GPUCommonMath::ATan2(float y, float x) { return CHOICE((float)atan2((double)y, (double)x), (float)atan2((double)y, (double)x), atan2(y, x)); }
GPUdi() float GPUCommonMath::Sin(float x) { return CHOICE((float)sin((double)x), (float)sin((double)x), sin(x)); }
GPUdi() float GPUCommonMath::Cos(float x) { return CHOICE((float)cos((double)x), (float)cos((double)x), cos(x)); }
GPUdi() float GPUCommonMath::Tan(float x) { return CHOICE((float)tanf((double)x), (float)tanf((double)x), tan(x)); }
GPUdi() float GPUCommonMath::Pow(float x, float y) { return CHOICE((float)pow((double)x, (double)y), pow((double)x, (double)y), pow(x, y)); }
GPUdi() float GPUCommonMath::ASin(float x) { return CHOICE((float)asin((double)x), (float)asin((double)x), asin(x)); }
GPUdi() float GPUCommonMath::ACos(float x) { return CHOICE((float)acos((double)x), (float)acos((double)x), acos(x)); }
GPUdi() float GPUCommonMath::Log(float x) { return CHOICE((float)log((double)x), (float)log((double)x), log(x)); }
GPUdi() float GPUCommonMath::Exp(float x) { return CHOICE((float)exp((double)x), (float)exp((double)x), exp(x)); }
#else
GPUdi() float GPUCommonMath::Round(float x) { return CHOICE(roundf(x), rintf(x), rint(x)); }
GPUdi() int32_t GPUCommonMath::Float2IntRn(float x) { return CHOICE((int32_t)Round(x), __float2int_rn(x), (int32_t)Round(x)); }
GPUdi() bool GPUCommonMath::Finite(float x) { return CHOICE(std::isfinite(x), true, true); }
GPUhdi() float GPUCommonMath::Sqrt(float x) { return CHOICE(sqrtf(x), sqrtf(x), sqrt(x)); }
GPUdi() float GPUCommonMath::ATan(float x) { return CHOICE(atanf(x), atanf(x), atan(x)); }
GPUhdi() float GPUCommonMath::ATan2(float y, float x) { return CHOICE(atan2f(y, x), atan2f(y, x), atan2(y, x)); }
GPUdi() float GPUCommonMath::Sin(float x) { return CHOICE(sinf(x), sinf(x), sin(x)); }
GPUdi() float GPUCommonMath::Cos(float x) { return CHOICE(cosf(x), cosf(x), cos(x)); }
GPUdi() float GPUCommonMath::Tan(float x) { return CHOICE(tanf(x), tanf(x), tan(x)); }
GPUdi() float GPUCommonMath::Pow(float x, float y) { return CHOICE(powf(x, y), powf(x, y), pow(x, y)); }
GPUdi() float GPUCommonMath::ASin(float x) { return CHOICE(asinf(x), asinf(x), asin(x)); }
GPUdi() float GPUCommonMath::ACos(float x) { return CHOICE(acosf(x), acosf(x), acos(x)); }
GPUdi() float GPUCommonMath::Log(float x) { return CHOICE(logf(x), logf(x), log(x)); }
GPUdi() float GPUCommonMath::Exp(float x) { return CHOICE(expf(x), expf(x), exp(x)); }
#endif

GPUhdi() void GPUCommonMath::SinCos(float x, float& s, float& c)
{
#if defined(GPUCA_NO_FAST_MATH) && !defined(__OPENCL__)
  s = sin((double)x);
  c = cos((double)x);
#elif !defined(GPUCA_GPUCODE_DEVICE) && defined(__APPLE__)
  __sincosf(x, &s, &c);
#elif !defined(GPUCA_GPUCODE_DEVICE) && (defined(__GNU_SOURCE__) || defined(_GNU_SOURCE) || defined(GPUCA_GPUCODE))
  sincosf(x, &s, &c);
#else
  CHOICE((void)((s = sinf(x)) + (c = cosf(x))), sincosf(x, &s, &c), s = sincos(x, &c));
#endif
}

GPUhdi() void GPUCommonMath::SinCosd(double x, double& s, double& c)
{
#if !defined(GPUCA_GPUCODE_DEVICE) && defined(__APPLE__)
  __sincos(x, &s, &c);
#elif !defined(GPUCA_GPUCODE_DEVICE) && (defined(__GNU_SOURCE__) || defined(_GNU_SOURCE) || defined(GPUCA_GPUCODE))
  sincos(x, &s, &c);
#else
  CHOICE((void)((s = sin(x)) + (c = cos(x))), sincos(x, &s, &c), s = sincos(x, &c));
#endif
}

GPUdi() uint32_t GPUCommonMath::Clz(uint32_t x)
{
#if (defined(__GNUC__) || defined(__clang__) || defined(__CUDACC__) || defined(__HIPCC__))
  return x == 0 ? 32 : CHOICE(__builtin_clz(x), __clz(x), __builtin_clz(x)); // use builtin if available
#else
  for (int32_t i = 31; i >= 0; i--) {
    if (x & (1u << i)) {
      return (31 - i);
    }
  }
  return 32;
#endif
}

GPUdi() uint32_t GPUCommonMath::Popcount(uint32_t x)
{
#if (defined(__GNUC__) || defined(__clang__) || defined(__CUDACC__) || defined(__HIPCC__)) && !defined(__OPENCL__) // TODO: remove OPENCL when reported SPIR-V bug is fixed
  // use builtin if available
  return CHOICE(__builtin_popcount(x), __popc(x), __builtin_popcount(x));
#else
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
#endif
}

GPUhdi() float GPUCommonMath::Hypot(float x, float y)
{
  return Sqrt(x * x + y * y);
}

GPUhdi() float GPUCommonMath::Hypot(float x, float y, float z)
{
  return Sqrt(x * x + y * y + z * z);
}

GPUhdi() float GPUCommonMath::Hypot(float x, float y, float z, float w)
{
  return Sqrt(x * x + y * y + z * z + w * w);
}

template <typename T>
GPUd() void _swap(T& a, T& b)
{
  T tmp = a;
  a = b;
  b = tmp;
}

template <typename T>
GPUhdi() void GPUCommonMath::Swap(T& a, T& b)
{
  CHOICE(std::swap(a, b), _swap<T>(a, b), _swap<T>(a, b));
}

template <class T>
GPUhdi() T GPUCommonMath::Min(const T x, const T y)
{
  return CHOICE(std::min(x, y), min(x, y), min(x, y));
}

template <class T>
GPUhdi() T GPUCommonMath::Max(const T x, const T y)
{
  return CHOICE(std::max(x, y), max(x, y), max(x, y));
}

template <class T, class S, class R>
GPUdi() T GPUCommonMath::MinWithRef(T x, T y, S refX, S refY, R& r)
{
  if (x < y) {
    r = refX;
    return x;
  }
  r = refY;
  return y;
}

template <class T, class S, class R>
GPUdi() T GPUCommonMath::MaxWithRef(T x, T y, S refX, S refY, R& r)
{
  if (x > y) {
    r = refX;
    return x;
  }
  r = refY;
  return y;
}

template <class T, class S, class R>
GPUdi() T GPUCommonMath::MaxWithRef(T x, T y, T z, T w, S refX, S refY, S refZ, S refW, R& r)
{
  T retVal = x;
  S retRef = refX;
  if (y > retVal) {
    retVal = y;
    retRef = refY;
  }
  if (z > retVal) {
    retVal = z;
    retRef = refZ;
  }
  if (w > retVal) {
    retVal = w;
    retRef = refW;
  }
  r = retRef;
  return retVal;
}

GPUdi() float GPUCommonMath::InvSqrt(float _x)
{
#if defined(GPUCA_NO_FAST_MATH) || defined(__OPENCL__)
  return 1.f / Sqrt(_x);
#elif defined(__CUDACC__) || defined(__HIPCC__)
  return __frsqrt_rn(_x);
#elif defined(__FAST_MATH__)
  return 1.f / sqrtf(_x);
#else
  union {
    float f;
    int32_t i;
  } x = {_x};
  const float xhalf = 0.5f * x.f;
  x.i = 0x5f3759df - (x.i >> 1);
  x.f = x.f * (1.5f - xhalf * x.f * x.f);
  return x.f;
#endif
}

template <>
GPUhdi() float GPUCommonMath::Abs<float>(float x)
{
  return CHOICE(fabsf(x), fabsf(x), fabs(x));
}

#if !defined(__OPENCL__) || defined(cl_khr_fp64)
template <>
GPUhdi() double GPUCommonMath::Abs<double>(double x)
{
  return CHOICE(fabs(x), fabs(x), fabs(x));
}
#endif

template <>
GPUhdi() int32_t GPUCommonMath::Abs<int32_t>(int32_t x)
{
  return CHOICE(abs(x), abs(x), abs(x));
}

GPUhdi() float GPUCommonMath::Copysign(float x, float y)
{
#if defined(__OPENCL__)
  return copysign(x, y);
#elif defined(GPUCA_GPUCODE) && !defined(__OPENCL__)
  return copysignf(x, y);
#else
  return std::copysignf(x, y);
#endif // GPUCA_GPUCODE
}

template <class S, class T>
GPUdi() uint32_t GPUCommonMath::AtomicExchInternal(S* addr, T val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCL__) && (!defined(__clang__) || defined(GPUCA_OPENCL_CLANG_C11_ATOMICS))
  return ::atomic_exchange(addr, val);
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  return ::atomic_xchg(addr, val);
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC__))
  return ::atomicExch(addr, val);
#elif defined(WITH_OPENMP)
  uint32_t old;
  __atomic_exchange(addr, &val, &old, __ATOMIC_SEQ_CST);
  return old;
#else
  return reinterpret_cast<std::atomic<T>*>(addr)->exchange(val);
#endif
}

template <class S, class T>
GPUdi() bool GPUCommonMath::AtomicCASInternal(S* addr, T cmp, T val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCL__) && (!defined(__clang__) || defined(GPUCA_OPENCL_CLANG_C11_ATOMICS))
  return ::atomic_compare_exchange(addr, cmp, val) == cmp;
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  return ::atomic_cmpxchg(addr, cmp, val) == cmp;
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC__))
  return ::atomicCAS(addr, cmp, val) == cmp;
#elif defined(WITH_OPENMP)
  return __atomic_compare_exchange(addr, &cmp, &val, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
  return reinterpret_cast<std::atomic<T>*>(addr)->compare_exchange_strong(cmp, val);
#endif
}

template <class S, class T>
GPUdi() uint32_t GPUCommonMath::AtomicAddInternal(S* addr, T val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCL__) && (!defined(__clang__) || defined(GPUCA_OPENCL_CLANG_C11_ATOMICS))
  return ::atomic_fetch_add(addr, val);
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  return ::atomic_add(addr, val);
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC__))
  return ::atomicAdd(addr, val);
#elif defined(WITH_OPENMP)
  return __atomic_add_fetch(addr, val, __ATOMIC_SEQ_CST) - val;
#else
  return reinterpret_cast<std::atomic<T>*>(addr)->fetch_add(val);
#endif
}

template <class S, class T>
GPUdi() void GPUCommonMath::AtomicMaxInternal(S* addr, T val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCL__) && (!defined(__clang__) || defined(GPUCA_OPENCL_CLANG_C11_ATOMICS))
  ::atomic_fetch_max(addr, val);
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  ::atomic_max(addr, val);
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC__))
  ::atomicMax(addr, val);
#else
  S current;
  while ((current = *(volatile S*)addr) < val && !AtomicCASInternal(addr, current, val)) {
  }
#endif // GPUCA_GPUCODE
}

template <class S, class T>
GPUdi() void GPUCommonMath::AtomicMinInternal(S* addr, T val)
{
#if defined(GPUCA_GPUCODE) && defined(__OPENCL__) && (!defined(__clang__) || defined(GPUCA_OPENCL_CLANG_C11_ATOMICS))
  ::atomic_fetch_min(addr, val);
#elif defined(GPUCA_GPUCODE) && defined(__OPENCL__)
  ::atomic_min(addr, val);
#elif defined(GPUCA_GPUCODE) && (defined(__CUDACC__) || defined(__HIPCC__))
  ::atomicMin(addr, val);
#else
  S current;
  while ((current = *(volatile S*)addr) > val && !AtomicCASInternal(addr, current, val)) {
  }
#endif // GPUCA_GPUCODE
}

#if (defined(__CUDACC__) || defined(__HIPCC__)) && !defined(G__ROOT)
#define GPUCA_HAVE_ATOMIC_MINMAX_FLOAT
template <>
GPUdii() void GPUCommonMath::AtomicMaxInternal(GPUglobalref() GPUgeneric() GPUAtomic(float) * addr, float val)
{
  if (val == -0.f) {
    val = 0.f;
  }
  if (val >= 0) {
    AtomicMaxInternal((GPUAtomic(int32_t)*)addr, __float_as_int(val));
  } else {
    AtomicMinInternal((GPUAtomic(uint32_t)*)addr, __float_as_uint(val));
  }
}
template <>
GPUdii() void GPUCommonMath::AtomicMinInternal(GPUglobalref() GPUgeneric() GPUAtomic(float) * addr, float val)
{
  if (val == -0.f) {
    val = 0.f;
  }
  if (val >= 0) {
    AtomicMinInternal((GPUAtomic(int32_t)*)addr, __float_as_int(val));
  } else {
    AtomicMaxInternal((GPUAtomic(uint32_t)*)addr, __float_as_uint(val));
  }
}
#endif

#undef CHOICE

} // namespace gpu
} // namespace GPUCA_NAMESPACE

#endif // GPUCOMMONMATH_H
