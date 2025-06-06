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

/// \file GPUROOTDump.h
/// \author David Rohr

#ifndef GPUROOTDUMP_H
#define GPUROOTDUMP_H

#include "GPUCommonDef.h"
#if !defined(GPUCA_NO_ROOT) && !defined(GPUCA_GPUCODE)
#include "GPUROOTDumpCore.h"
#include <TTree.h>
#include <TNtuple.h>
#include <memory>
#include <stdexcept>
#else
class TNtuple;
#endif

namespace GPUCA_NAMESPACE
{
namespace gpu
{
#if !defined(GPUCA_NO_ROOT) && !defined(GPUCA_GPUCODE)
namespace
{
template <class S>
struct internal_Branch {
  template <typename... Args>
  static void Branch(S* p, Args... args)
  {
  }
};
template <>
struct internal_Branch<TTree> {
  template <typename... Args>
  static void Branch(TTree* p, Args... args)
  {
    p->Branch(args...);
  }
};
} // namespace

template <class T, typename... Args>
class GPUROOTDump : public GPUROOTDump<Args...>
{
 public:
  template <typename... Names>
  static GPUROOTDump<T, Args...>& get(const char* name1, Names... names) // return always the same instance, identified by template
  {
    static GPUROOTDump<T, Args...> instance(name1, names...);
    return instance;
  }
  template <typename... Names>
  static GPUROOTDump<T, Args...> getNew(const char* name1, Names... names) // return new individual instance
  {
    return GPUROOTDump<T, Args...>(name1, names...);
  }
  void Fill(const T& o, Args... args)
  {
    mObj = o;
    GPUROOTDump<Args...>::Fill(args...);
  }

 protected:
  using GPUROOTDump<Args...>::mTree;
  template <typename... Names>
  GPUROOTDump(const char* name1, Names... names) : GPUROOTDump<Args...>(names...)
  {
    mTree->Branch(name1, &mObj);
  }

 private:
  T mObj;
};

template <class T>
class GPUROOTDump<T> : public GPUROOTDumpBase
{
 public:
  static GPUROOTDump<T>& get(const char* name) // return always the same instance, identified by template
  {
    static GPUROOTDump<T> instance(name);
    return instance;
  }
  static GPUROOTDump<T> getNew(const char* name) // return new individual instance
  {
    return GPUROOTDump<T>(name);
  }

  void write() override { mTree->Write(); }

  void Fill(const T& o)
  {
    mObj = o;
    mTree->Fill();
  }

 protected:
  GPUROOTDump(const char* name1, const char* nameTree = nullptr)
  {
    if (nameTree == nullptr) {
      nameTree = name1;
    }
    mTree = new TTree(nameTree, nameTree);
    mTree->Branch(name1, &mObj);
  }
  TTree* mTree = nullptr;

 private:
  T mObj;
};

template <>
class GPUROOTDump<TNtuple> : public GPUROOTDumpBase
{
 public:
  static GPUROOTDump<TNtuple>& get(const char* name, const char* options)
  {
    static GPUROOTDump<TNtuple> instance(name, options);
    return instance;
  }
  static GPUROOTDump<TNtuple> getNew(const char* name, const char* options)
  {
    return GPUROOTDump<TNtuple>(name, options);
  }

  void write() override { mNTuple->Write(); }

  template <typename... Args>
  void Fill(Args... args)
  {
    mNTuple->Fill(args...);
  }

 private:
  GPUROOTDump(const char* name, const char* options)
  {
    mNTuple = new TNtuple(name, name, options);
  }
  TNtuple* mNTuple;
};
#else
template <typename... Args>
class GPUROOTDump
{
 public:
  template <typename... Names>
  GPUd() void Fill(Args... args) const
  {
  }
  template <typename... Names>
  GPUd() static GPUROOTDump<Args...>& get(Args... args)
  {
    return *(GPUROOTDump<Args...>*)(size_t)(1024); // Will never be used, return just some reference, which must not be nullptr by specification
  }
  template <typename... Names>
  GPUd() static GPUROOTDump<Args...>& getNew(Args... args)
  {
    return *(GPUROOTDump<Args...>*)(size_t)(1024); // Will never be used, return just some reference, which must not be nullptr by specification
  }
};
#endif
} // namespace gpu
} // namespace GPUCA_NAMESPACE

#endif
