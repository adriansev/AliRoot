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

/// \file GPUDefConstantsAndSettings.h
/// \author David Rohr

// This files contains compile-time constants affecting the GPU algorithms / reconstruction results.
// Architecture-dependant compile-time constants affecting the performance without changing the results are stored in GPUDefGPUParameters.h

#ifndef GPUDEFCONSTANTSANDSETTINGS_H
#define GPUDEFCONSTANTSANDSETTINGS_H

// clang-format off

#include "GPUCommonDef.h"

#if !defined(GPUCA_STANDALONE) && !defined(GPUCA_ALIROOT_LIB) && !defined(GPUCA_O2_LIB) && !defined(GPUCA_O2_INTERFACE)
  #error You are using the CA GPU tracking without defining the build type (O2/AliRoot/Standalone). If you are running an O2 ROOT macro, please include GPUO2Interface.h first!
#endif

#if (defined(GPUCA_ALIROOT_LIB) && defined(GPUCA_O2_LIB)) || (defined(GPUCA_ALIROOT_LIB) && defined(GPUCA_STANDALONE)) || (defined(GPUCA_O2_LIB) && defined(GPUCA_STANDALONE))
  #error Invalid Compile Definitions, need to build for either AliRoot or O2 or Standalone!
#endif

#define GPUCA_TRACKLET_SELECTOR_MIN_HITS_B5(QPTB5) (CAMath::Abs(QPTB5) > 10 ? 10 : (CAMath::Abs(QPTB5) > 5 ? 15 : 29)) // Minimum hits should depend on Pt, low Pt tracks can have few hits. 29 Hits default, 15 for < 200 mev, 10 for < 100 mev

#define GPUCA_MERGER_MAX_TRACK_CLUSTERS 1000          // Maximum number of clusters a track may have after merging

#define GPUCA_MAXN 40                                 // Maximum number of neighbor hits to consider in one row in neightbors finder
#define GPUCA_MIN_TRACK_PTB5_DEFAULT 0.010f           // Default setting for minimum track Pt at some places (at B=0.5T)
#define GPUCA_MIN_TRACK_PTB5_REJECT_DEFAULT 0.050f    // Default setting for Pt (at B=0.5T) where tracks are rejected

#define GPUCA_MAX_SIN_PHI_LOW 0.99f                   // Limits for maximum sin phi during fit
#define GPUCA_MAX_SIN_PHI 0.999f                      // Must be preprocessor define because c++ pre 11 cannot use static constexpr for initializes

#define GPUCA_MIN_BIN_SIZE 2.f                        // Minimum bin size in TPC fast access grid
#define GPUCA_MAX_BIN_SIZE 1000.f                     // Maximum bin size in TPC fast access grid

#define GPUCA_TPC_COMP_CHUNK_SIZE 1024                // Chunk size of sorted unattached TPC cluster in compression

#define TPC_MAX_TIME_BIN_TRIGGERED 600

#if defined(GPUCA_NSLICES) || defined(GPUCA_ROW_COUNT)
  #error GPUCA_NSLICES or GPUCA_ROW_COUNT already defined, do not include GPUTPCGeometry.h before!
#endif
#if defined(GPUCA_HAVE_O2HEADERS) && defined(GPUCA_TPC_GEOMETRY_O2) && !(defined(ROOT_VERSION_CODE) && ROOT_VERSION_CODE < 393216)
  //Use definitions from the O2 headers if available for nicer code and type safety
  #include "DataFormatsTPC/Constants.h"
  #define GPUCA_NSLICES o2::tpc::constants::MAXSECTOR
  #define GPUCA_ROW_COUNT o2::tpc::constants::MAXGLOBALPADROW
#else
  //Define it manually, if O2 headers not available, ROOT5, and OpenCL 1.2, which do not know C++11.
  #define GPUCA_NSLICES 36
  #ifdef GPUCA_TPC_GEOMETRY_O2
    #define GPUCA_ROW_COUNT 152
  #else
    #define GPUCA_ROW_COUNT 159
  #endif
#endif

//#define GPUCA_MERGER_BY_MC_LABEL                    // Use MC labels for TPC track merging - for performance studies
//#define GPUCA_FULL_CLUSTERDATA                      // Store all cluster information in the cluster data, also those not needed for tracking.
//#define GPUCA_TPC_RAW_PROPAGATE_PAD_ROW_TIME        // Propagate Pad, Row, Time cluster information to GM
//#define GPUCA_GM_USE_FULL_FIELD                     // Use offline magnetic field during GMPropagator prolongation

// clang-format on

#endif
