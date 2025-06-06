# **************************************************************************
# * Copyright(c) 1998-2014, ALICE Experiment at CERN, All rights reserved. *
# *                                                                        *
# * Author: The ALICE Off-line Project.                                    *
# * Contributors are mentioned in the code where appropriate.              *
# *                                                                        *
# * Permission to use, copy, modify and distribute this software and its   *
# * documentation strictly for non-commercial purposes is hereby granted   *
# * without fee, provided that the above copyright notice appears in all   *
# * copies and that both the copyright notice and this permission notice   *
# * appear in the supporting documentation. The authors make no claims     *
# * about the suitability of this software for any purpose. It is          *
# * provided "as is" without express or implied warranty.                  *
# **************************************************************************

#--------------------------------------------------------------------------#
# Set Basic CMake Configuration                                            #
#--------------------------------------------------------------------------#

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

# AliRoot will refuse to compile against a wrong ROOT version
set(ROOT_VERSION_MIN_5 "5.34.30")
set(ROOT_VERSION_MIN_6 "6.04.00")

# Without EVE, ROOT v5.34.17 is sufficient
list(FIND EXCLUDE_MODULES "EVE" excleve)
if (excleve GREATER -1)
  set(ROOT_VERSION_MIN_5 "5.34.17")
endif()

foreach(p CMP0005  # Properly escape preprocessor definitions (v2.6, v3.0.2)
          CMP0025  # Compiler id for Apple Clang is now AppleClang (v3.0)
          CMP0042  # MACOSX_RPATH is enabled by default (v3.0)
          CMP0053  # Simplify variable reference and escape sequence evaluation (v3.1.3)
          CMP0068  # RPATH settings on macOS do not affect install_name (v3.9 and newer)
       )
if(POLICY ${p})
    cmake_policy(SET ${p} NEW)
  endif()
endforeach()

project(AliRoot CXX C)
enable_testing()

message(STATUS "CMake platform: ${CMAKE_SYSTEM}")
message(STATUS "Build folder: ${AliRoot_BINARY_DIR}")
message(STATUS "Source folder: ${AliRoot_SOURCE_DIR}")
message(STATUS "Installation folder: ${CMAKE_INSTALL_PREFIX}")

# CMake supports different build types by default. We want the DEBUG build type
# to have "-g -O0" flags: by default it only has "-g"
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -D__DEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

# Build type for coverage builds
set(CMAKE_CXX_FLAGS_COVERAGE "-g -O2 -fprofile-arcs -ftest-coverage")
set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_COVERAGE}")
set(CMAKE_Fortran_FLAGS_COVERAGE "-g -O2 -fprofile-arcs -ftest-coverage")
set(CMAKE_LINK_FLAGS_COVERAGE "--coverage -fprofile-arcs  -fPIC")

MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_COVERAGE
    CMAKE_C_FLAGS_COVERAGE
    CMAKE_Fortran_FLAGS_COVERAGE
    CMAKE_LINK_FLAGS_COVERAGE)

# You can change the build type using
# cmake -DCMAKE_BUILD_TYPE=DEBUG | RELEASE | RELWITHDEBINFO | MINSIZEREL ...
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
endif(NOT CMAKE_BUILD_TYPE)
string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)
set(CMAKE_ALLOWED_BUILD_TYPES DEBUG RELEASE RELWITHDEBINFO COVERAGE CUSTOM_EXTERNAL)
list(FIND CMAKE_ALLOWED_BUILD_TYPES "${CMAKE_BUILD_TYPE}" VALID_BUILD_TYPE)
if(${VALID_BUILD_TYPE} EQUAL -1)
  string(REPLACE ";" ", " CMAKE_ALLOWED_BUILD_TYPES_FLAT "${CMAKE_ALLOWED_BUILD_TYPES}")
  message(FATAL_ERROR "Invalid build type ${CMAKE_BUILD_TYPE}. Use one of: ${CMAKE_ALLOWED_BUILD_TYPES_FLAT}.")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE} (${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}})")

if(CMAKE_INSTALL_PREFIX STREQUAL "${AliRoot_SOURCE_DIR}")
  message(FATAL_ERROR "Please choose a different installation point than the source tree!")
endif()

# Path to additonal modules
set(CMAKE_MODULE_PATH "${AliRoot_SOURCE_DIR}/cmake")

# Check some compiler flags
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wno-error=misleading-indentation CC_WNOERROR_MISLEADING_INDENTATION)

# AliRoot version extracted from Git repository
# Sets 4 git variables
#  - GIT_REFSPEC - complete name of the current reference
#  - ALIROOT_BRANCH - name of the branch or tag extracted from the current reference
#  - GIT_SHA1 - current hash in the long format
#  - GIT_SHORT_SHA1 - current hash in the short format
#  - ALIROOT_VERSION = ALIROOT_BRANCH
#  - ALIROOT_REVISION = GIT_SHORT_SHA1
include(CheckGitVersion)

# - CLANG_MAJOR.CLANG_MINOR or
# - GCC_MAJOR.GCC_MINOR.GCC_PATCH
include(CheckCompiler)

# Utility to generate PARfiles
include(cmake/GenParFile.cmake)

set(ALILIBSTESTED "" CACHE INTERNAL "ALILIBSTESTED" FORCE)
include(AddLibraryTested)

include(cmake/modules/MacroOptionalAddSubdirectory.cmake)

# Shared library suffix
if (NOT CMAKE_SYSTEM_NAME STREQUAL Windows)
  set(CMAKE_SHARED_LIBRARY_SUFFIX .so)
endif (NOT CMAKE_SYSTEM_NAME STREQUAL Windows)

if(NOT ALIROOT_ONLINE_MODE)
  # Not in "online mode": optimize install speed on macOS and fix install dirs

  # Be sure about where libraries and binaries are put
  set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
  set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")

  # Build targets with install rpath on Mac to dramatically speed up installation
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
  if("${isSystemDir}" STREQUAL "-1")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      message(STATUS "RPATH settings ***")
      set(CMAKE_INSTALL_RPATH "@loader_path/../lib")
    endif()
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  endif()
  unset(isSystemDir)
endif()

# HLT/ZMQ and HLT/BASE/util need X11
find_package(X11)
if(X11_FOUND)
  get_filename_component(X11_LIB_DIR ${X11_X11_LIB} DIRECTORY)
  message("Found X11: ${X11_LIB_DIR} ${X11_INCLUDE_DIR}")
endif(X11_FOUND)

# Optional: Doxygen for reference doc generation
set(DOXYGEN_EXECUTABLE "")
find_package(Doxygen)
if(DOXYGEN_FOUND AND DOXYGEN_DOT_FOUND)
  message(STATUS "Doxygen ${DOXYGEN_VERSION} and Graphviz found: ${DOXYGEN_EXECUTABLE}, ${DOXYGEN_DOT_EXECUTABLE}")
  set(CAN_GENERATE_DOC TRUE)
elseif(DOXYGEN_ONLY)
  message(FATAL_ERROR "Doxygen and Graphviz not found. Cannot generate documentation")
else()
  message(STATUS "Doxygen and Graphviz not found. Disabling support for documentation generation")
endif()

# Doxygen documentation is always included
add_subdirectory(doxygen)

if(DEFINED DOXYGEN_ONLY AND DOXYGEN_ONLY)
  # In this case we do not prepare targets for the rest, but for Doxygen only.
  # This is useful because no checks are performed, and it means that Doxygen
  # documentation can be generated on a relatively "dumb" server that has as
  # requirements only cmake, doxygen and graphviz.
  message(WARNING "Preparing build for Doxygen documentation only as requested. AliRoot will not be built!")
else()

  # Standard CMake inclusions and checks if we are actually building AliRoot,
  # i.e. not only the documentation

  # Checking first for DIM, DATE, AMORE and daqDA
  # in case ROOT Extra static library needs to be enabled

  # DATE
  # date-config needs DIMDIR and ODIR set
  # Setting variables in cache to be accesible by ccmake
  set(DATE_CONFIG CACHE STRING "date-config script location")
  set(DIMDIR CACHE STRING "dim installation folder")
  set(ODIR CACHE STRING "dim plaform, usually \"linux\"")

  if(DIMDIR AND ODIR)
    find_package(DIM)
  endif()

  if(DATE_CONFIG)
    if(DIM_FOUND)
      find_package(DATE)
    else()
      message(FATAL_ERROR "DATE enabled but no DIMDIR and ODIR set. Please set DIMDIR and ODIR")
    endif()
  endif()

  # daqDA
  # Setting variables in cache to be accesible by ccmake
  set(daqDA CACHE STRING "daqDA library folder installation")
  if(daqDA)
    find_package(daqDA)
  endif(daqDA)

  # AMORE -> requires ROOT to be installed
  # Setting variables in cache to be accesible by ccmake
  set(AMORE_CONFIG CACHE STRING "amore-config script location")
  if(AMORE_CONFIG)
    if(ROOTSYS)
      find_package(AMORE)
    else()
      message(FATAL_ERROR "AMORE enabled but no ROOTSYS defined")
    endif()
  endif(AMORE_CONFIG)

  # id DARPM we enable DA if it is not set
  # Setting variables in cache to be accesible by ccmake
  set(DARPM CACHE STRING "Enable DARPM: ON")
  set(DA CACHE STRING "Enable DA: ON")
  if(DARPM)
    if(NOT DA)
      set(DA ON)
    endif(NOT DA)
  endif(DARPM)

  # DA is enabled
  # Setting variables in cache to be accesible by ccmake
  set(ALIROOT_STATIC CACHE STRING "ENABLE static building of AliRoot: ON")
  if(DA)
    if(NOT DIM_FOUND)
      set(DA FALSE)
      message(FATAL_ERROR "DAs enabled but no DIMDIR and ODIR set. Please set DIMDIR to DIM installation and ODIR to platform (default linux)")
    endif()

    if(NOT DATE_FOUND)
      set(DA FALSE)
      message(FATAL_ERROR "DAs enabled but no DATE support found. Please point to your date installation using \"DATE_CONFIG\" and \"DATE_ENV\" variables")
    endif()

    if(NOT daqDA_FOUND)
      set(DA FALSE)
      message(FATAL_ERROR "DAs enabled but no daqDA support found. Please point to your daqDA installation using \"daqDA\" variable")
    endif()

    if(NOT AMORE_FOUND)
      set(DA FALSE)
      message(FATAL_ERROR "DAs enabled but no AMORE support found. Please point to your AMORE installation using \"AMORE_CONFIG\" variable")
    endif()

    # Enable static libraries
    set(ALIROOT_STATIC TRUE)
    message(STATUS "DAs enabled")
  endif(DA)

  # MDC rpm creation enables the static build
  # Setting variables in cache to be accesible by ccmake
  set(MDCRPM CACHE STRING "Enable mdc rpm build")
  if(MDCRPM)
    set(ALIROOT_STATIC TRUE)
    message(STATUS "AliMDC RPM enabled. AliRoot static build enabled")
  endif(MDCRPM)

  # ROOT configuration mandatory
  # FindRoot has to be after the enabling of DA due to libRootExtra
  # Do not put it above DA
  # Setting variables in cache to be accesible by ccmake
  set(ROOTSYS CACHE STRING "ROOT installation folder")
  if(ROOTSYS)
    find_package(ROOT REQUIRED)

    if(ROOT_VERSION_MAJOR EQUAL 5)
      set(ROOT_VERSION_MIN "${ROOT_VERSION_MIN_5}")
    elseif(ROOT_VERSION_MAJOR EQUAL 6)
      set(ROOT_VERSION_MIN "${ROOT_VERSION_MIN_6}")

      # find VMC standalone
      # VMC standalone will fail if ROOT was found with VMC
      find_package(VMC)
      if(VMC_FOUND)
        message(STATUS "Using standalone VMC")
        # AliRoot's FindROOT module does not actually provide targets but only does link_libraries(ROOT_LIB_DIR).
        # So AliRoot finds libs by name in that dir but not as a target.
        # On the other hand VMCLibrary is linked against targets ROOT::Core etc which come in with a find_package(ROOT CONFIG).
        # So Let's slightly modify the VMCLibrary target properties...
        set(VMC_DEPS Core EG Geom Physics)
        set_target_properties(VMCLibrary PROPERTIES INTERFACE_LINK_LIBRARIES "${VMC_DEPS}")
        # Alias target before VMC was also found by name "VMC"
        add_library(VMC ALIAS VMCLibrary)
        include_directories(${VMC_INCLUDE_DIRS})
      endif()

    else()
      message(FATAL_ERROR "AliRoot works with ROOT 5 or 6 only")
    endif()

    if(ROOT_VERSION_NORM VERSION_LESS "${ROOT_VERSION_MIN}")
      message(FATAL_ERROR "AliRoot requires at least ROOT ${ROOT_VERSION_MIN} (you have ${ROOT_VERSION_NORM})")
    endif()

    # ROOT must be built with XML2 support
    if(NOT ROOT_HASXML)
      message(FATAL_ERROR "ROOT was not built with xml2 support. Please reinstall or rebuild ROOT with xml2 support")
    endif(NOT ROOT_HASXML)


  else(ROOTSYS)
    message(FATAL_ERROR "ROOT installation not found!\nPlease point to the ROOT installation using -DROOTSYS=ROOT_INSTALL_DIR")
  endif(ROOTSYS)

  # Enable C++11 by default if found in ROOT
  if(ROOT_HAS_CXX11 AND NOT DISABLE_CXX11)
    message(STATUS "Enabling C++11")
    if (NOT ${CMAKE_CXX_FLAGS} MATCHES "-std=c\\+\\+11")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()
  endif()

  # Enable C++14 by default if found in ROOT
  if(ROOT_HAS_CXX14 AND NOT DISABLE_CXX14)
    message(STATUS "Enabling C++14")
    if (NOT ${CMAKE_CXX_FLAGS} MATCHES "-std=c\\+\\+14")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    endif()
  endif()

  # FTGL: prefer library from ROOT, look for both libFTGL and libftgl
  unset(LIB_FTGL)
  if(${ROOT_FEATURES} MATCHES "builtin_ftgl")
    message(STATUS "Using FTGL from ROOT")
    find_library(LIB_FTGL NAMES FTGL PATHS "${ROOT_LIBDIR}" NO_DEFAULT_PATH)
  else()
    message(STATUS "Using FTGL from the system")
    find_library(LIB_FTGL NAMES ftgl FTGL)
  endif()
  if(NOT LIB_FTGL)
    message(FATAL_ERROR "Required library FTGL not found. Please install it, or reconfigure ROOT with --enable-builtin-ftgl")
  endif()

  # ROOT dictionaries and maps
  include(CMakeALICE)

  # If no Fortran, i.e on Windows
  include(CheckLanguage)

  if(DISABLE_FORTRAN)
    message(STATUS "Fortran support disabled by user")
  else(DISABLE_FORTRAN)
    check_language(Fortran)
    if(CMAKE_Fortran_COMPILER)
      enable_language(Fortran OPTIONAL)
      message(STATUS "Fortran compiler is ${CMAKE_Fortran_COMPILER}")
    else()
      set(DISABLE_FORTRAN TRUE)
    endif()
  endif(DISABLE_FORTRAN)

  # FastJet
  find_package(FASTJET)

  # ZEROMQ
  if(ROOT_VERSION_MAJOR GREATER 5)
    find_package(ZeroMQ)
  endif()

  # Generating the AliRoot-config.cmake file
  configure_file(${PROJECT_SOURCE_DIR}/cmake/AliRoot-config.cmake.in ${CMAKE_BINARY_DIR}/version/AliRoot-config.cmake @ONLY)
  install(FILES ${PROJECT_BINARY_DIR}/version/AliRoot-config.cmake DESTINATION etc)

  # General flags -> Should be moved into a configuration file
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  # Avoid problems with -fPIE (set automatically by the previous line).
  set(CMAKE_CXX_COMPILE_OPTIONS_PIE "")
  set(CMAKE_C_COMPILE_OPTIONS_PIE "")
  set(CMAKE_Fortran_COMPILE_OPTIONS_PIE "")
  set(CMAKE_NO_SYSTEM_FROM_IMPORTED TRUE)

  # Use CMake cache as pseudo-"global" scope for those variables (list of PARfiles, list of DA RPMs)
  set(ALIPARFILES "" CACHE INTERNAL "ALIPARFILES" FORCE)
  set(ALIDARPMS "" CACHE INTERNAL "ALIDARPMS" FORCE)

  # AliRoot base modules
  add_subdirectory(STEER)
  add_subdirectory(RAW)
  add_subdirectory(ANALYSIS)

  # Include Vc ...
  # try to find an external Vc
  find_package(Vc 0.7)
  if(Vc_FOUND) # an external Vc was found
    if("${Vc_VERSION}" VERSION_GREATER "1.0")
      message(STATUS "External Vc version ${Vc_VERSION} (>= 1.0) found")
      # Redefinition of a macro originally under the Vc/ directory which we don't include
      macro(ALICE_UseVc)
        include_directories(SYSTEM "${Vc_INCLUDE_DIR}")
        add_definitions(${Vc_DEFINITIONS})
        string(REPLACE ";" " " TMP_CMAKE_CXX_VC_FLAGS "${Vc_COMPILE_FLAGS} ${Vc_ARCHITECTURE_FLAGS}")
        set(CMAKE_CXX_FLAGS "${TMPCMAKE_CXX_VC_FLAGS} ${CMAKE_CXX_FLAGS}")
      endmacro()
      link_directories(${Vc_LIB_DIR})
   else()
     message(WARNING "External Vc >= 1.0 not found: falling back on Vc >= 0.7")
     message(STATUS "External Vc version ${Vc_VERSION} (>= 0.7) found")
     # put here the macros suitable for Vc version 0.7
     macro(ALICE_UseVc)
      include_directories(SYSTEM "${Vc_INCLUDE_DIR}")
      add_definitions(${Vc_DEFINITIONS})
     endmacro()
      link_directories(${Vc_LIB_DIR})
   endif() #Version > 1.0
  else()
    # finally we fallback to the deprecated internal Vc
    message(WARNING "You are using AliRoot's builtin Vc: this is deprecated and it will be dropped soon")
    # Include Vc own cmake
    include(Vc/Vc.cmake)
    if(NOT ROOT_HASVC)
      add_subdirectory(Vc)
    endif()
  endif(Vc_FOUND)

  # AliRoot common macros
  add_subdirectory(macros)

  # AliRoot modules
  set(ALIROOT_MODULES
      ACORDE AD BCM DRGEN EMCAL EVE EVGEN FASTSIM FIT FMD HLT HMPID ITS ITSMFT
      MONITOR MUON PHOS PMD STARLIGHT STAT STRUCT T0 TEvtGen TOF TPC TRD TRIGGER
      TTherminator VZERO ZDC ALIGN data GRP OCDB QAref)

  # Unneeded modules to disable on DARPM/SHUTTLE builds
  set(ALIROOT_MODULES_NOONLINE
      MONITOR EVGEN DIME DRGEN PYTHIA6 PYTHIA8 STARLIGHT TAmpt TDPMjet TEPEMGEN
      THbtp THerwig THijing THydjet TPHIC TTherminator TUHKMgen)

  # AliRoot modules requiring OpenGL
  set(ALIROOT_MODULES_OPENGL
      EVE)
  if(ROOT_HASOPENGL)
    list(APPEND ALIROOT_MODULES "${ALIROOT_MODULES_FORTRAN}")
  else()
    string(REPLACE ";" " " ALIROOT_MODULES_OPENGL_FLAT "${ALIROOT_MODULES_OPENGL}")
    message(WARNING "The following modules will not be built because ROOT has no OpenGL support: ${ALIROOT_MODULES_OPENGL_FLAT}")
  endif()

  # AliRoot modules requiring Fortran
  set(ALIROOT_MODULES_FORTRAN
      DIME HERWIG HIJING LHAPDF MICROCERN PYTHIA6 PYTHIA8 TAmpt
      TEPEMGEN THbtp THerwig THijing THydjet TPHIC TUHKMgen)
  if(NOT DISABLE_FORTRAN)
    list(INSERT ALIROOT_MODULES 0 "${ALIROOT_MODULES_FORTRAN}")
    find_package(DPMJET)
    if (DPMJET_FOUND)
      list(INSERT ALIROOT_MODULES 0 "TDPMjet")
    elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/DPMJET)
      message(WARNING "Using DPMJET from AliRoot. This is deprecated and it will be removed soon!")
      list(INSERT ALIROOT_MODULES 0 "DPMJET" "TDPMjet")
    else()
      message(WARNING "Not using DPMJET! No external package or builtin found!")
    endif()
  else()
    string(REPLACE ";" " " ALIROOT_MODULES_FORTRAN_FLAT "${ALIROOT_MODULES_FORTRAN}")
    message(WARNING "The following modules require Fortran and will not be built: ${ALIROOT_MODULES_FORTRAN_FLAT}")
  endif()

  option(ALIROOT_ONLINE_MODE "Build HLT GPU tracker using CUDA" OFF)
  if((SHUTTLE OR DARPM OR "$ENV{ALIROOT_ONLINE_MODE}") AND NOT "$ENV{HLT_ONLINE_MODE}")
    set(ALIROOT_ONLINE_MODE ON)
  endif()

  if(ALIROOT_ONLINE_MODE)
    # Disable some unneeded modules in "online mode"
    string(REPLACE ";" " " ALIROOT_MODULES_NOONLINE_FLAT "${ALIROOT_MODULES_NOONLINE}")
    message(WARNING "Online build detected: disabling unneeded modules: ${ALIROOT_MODULES_NOONLINE_FLAT}")
    list(APPEND EXCLUDE_MODULES "${ALIROOT_MODULES_NOONLINE}")
  endif()

  # Selectively exclude modules (with -DEXCLUDE_MODULES="mod1;mod2"...)
  foreach(mod ${ALIROOT_MODULES})
    list(FIND EXCLUDE_MODULES "${mod}" excl)
    if (excl EQUAL -1)
      macro_optional_add_subdirectory(${mod})
    else()
      list(REMOVE_ITEM ALIROOT_MODULES ${mod})
      message(WARNING "Excluding module ${mod} as per explicit configuration")
    endif()
  endforeach()

  # Enable SHUTTLE compilation
  # Check if DIMDIR and ODIR are set
  # # Setting variables in cache to be accesible by ccmake
  set(SHUTTLE CACHE STRING "Enable SHUTTLE build : ON")
  if(SHUTTLE)
    if(NOT ROOT_HASMONALISA)
      message(FATAL_ERROR "ROOT was not built with MonALISA support")
    endif()
    if(NOT ROOT_HASLDAP)
      message(FATAL_ERROR "ROOT was not built with LDAP support")
    endif()
    find_package(ApMon)
    if(ROOT_HASALIEN STREQUAL "no")
      message(FATAL_ERROR "Shuttle needs ROOT built with AliEn support. Please built ROOT with AliEn support. Do not forget to point -DALIEN= to your AliEn installation")
    endif()

    if(DIM_FOUND AND ALIEN)
      add_subdirectory(SHUTTLE)
    else()
      message(FATAL_ERROR "SHUTTLE not enabled! Please specify DIMDIR, ODIR and ALIEN")
    endif()
  endif(SHUTTLE)

  # Enable SHUTTLE/TestShuttle compilation
  # independently from SHUTTLE (and from monalisa, ldap, dim)
  # Setting variables in cache to be accesible by ccmake
  set(TESTSHUTTLE CACHE STRING "Enable TestShuttle build : ON")
  if(TESTSHUTTLE)
    message(STATUS "TestShuttle enabled")
    add_subdirectory(SHUTTLE/TestShuttle)
  endif(TESTSHUTTLE)

  add_subdirectory(ALIROOT)
  add_subdirectory(TFluka)

  # List modules with PARfiles
  string(REPLACE ";" " " ALIPARFILES_FLAT "${ALIPARFILES}")
  message(STATUS "PARfile target enabled for the following modules: ${ALIPARFILES_FLAT}")

  # List enabled DAs
  if (NOT "${ALIDARPMS}" STREQUAL "" AND DARPM)
    string(REPLACE ";" " " ALIDARPMS_FLAT "${ALIDARPMS}")
    message(STATUS "The following DA RPMs can be created individually with 'make <name>da.rpm' or altogether with 'make daqDA-all-rpm': ${ALIDARPMS_FLAT}")
    add_custom_target("daqDA-all-rpm" COMMENT "Creating all RPMs")
    add_dependencies("daqDA-all-rpm" ${ALIDARPMS})
  endif()

  # Install Find macros for common packages to be further used by AliPhysics
  install(FILES
    cmake/CMakeALICE.cmake
    cmake/FindROOT.cmake
    cmake/FindFASTJET.cmake
    cmake/FindZeroMQ.cmake
    cmake/GetGitRevisionDescription.cmake
    cmake/GetGitRevisionDescription.cmake.in
    DESTINATION cmake)

  foreach(TEST_LIB ${ALILIBSTESTED})
    add_test(load_library_${TEST_LIB}
      env
      PATH=$ENV{PATH}
      ROOTSYS=$ENV{ROOTSYS}
      LD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib:${ZEROMQ}/lib:$ENV{LD_LIBRARY_PATH}
      DYLD_LIBRARY_PATH=${CMAKE_INSTALL_PREFIX}/lib:${ZEROMQ}/lib:$ENV{DYLD_LIBRARY_PATH}
      ROOT_HIST=0
      root -n -l -b -q "${CMAKE_INSTALL_PREFIX}/test/load_library/LoadLib.C(\"lib${TEST_LIB}\")")
  endforeach()

  # Install test directory
  install(DIRECTORY test/ DESTINATION test USE_SOURCE_PERMISSIONS)

endif(DEFINED DOXYGEN_ONLY AND DOXYGEN_ONLY)
