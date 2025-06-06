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

set(SRCS)
set(LIBDEPS)
if(ALIROOT_ONLINE_MODE)
    message(WARNING "Online build. Disabling TPC space-charge base library.")
else()
    set(ALIGPU_BUILD_TYPE "ALIROOT")
    add_subdirectory(${AliRoot_SOURCE_DIR}/TPC/TPCSpaceChargeBase TPCSpaceChargeBase)
    include_directories(${AliRoot_SOURCE_DIR}/TPC/TPCSpaceChargeBase)
    add_definitions(-DHAVE_ALIGPU)
#PH    add_subdirectory(test)
    set(SRCS ${SRCS} AliTPCSpaceCharge3DDriftLine.cxx)
    set(LIBDEPS ${LIBDEPS} AliTPCSpaceChargeBase)
endif()

# Module
set(MODULE TPCbase)
add_definitions(-D_MODULE_="${MODULE}")

# Module include folder
include_directories(${AliRoot_SOURCE_DIR}/TPC/${MODULE})

# Additional include folders in alphabetical order except ROOT
include_directories(SYSTEM ${ROOT_INCLUDE_DIR})
include_directories(${AliRoot_SOURCE_DIR}/RAW/RAWDatabase
                    ${AliRoot_SOURCE_DIR}/RAW/RAWDatarec
                    ${AliRoot_SOURCE_DIR}/RAW/dateStream
                    ${AliRoot_SOURCE_DIR}/STEER/CDB
                    ${AliRoot_SOURCE_DIR}/STEER/ESD
                    ${AliRoot_SOURCE_DIR}/STEER/STEER
                    ${AliRoot_SOURCE_DIR}/STAT
                    ${AliRoot_SOURCE_DIR}/STEER/STEERBase
                    ${CMAKE_BINARY_DIR}/version
                   )

# Sources in alphabetical order
set(SRCS ${SRCS}
    AliClusters.cxx
    AliComplexCluster.cxx
    AliDetectorParam.cxx
    AliDigitsArray.cxx
    AliDigits.cxx
    AliH2F.cxx
    AliSegmentArray.cxx
    AliSegmentID.cxx
    AliSimDigits.cxx
    AliTPCAltroMapping.cxx
    AliTPCBoundaryVoltError.cxx
    AliTPCCalibCE.cxx
    AliTPCcalibDB.cxx
    AliTPCcalibDButil.cxx
    AliTPCCalibGlobalMisalignment.cxx
    AliTPCCalibPedestal.cxx
    AliTPCCalibPulser.cxx
    AliTPCCalibRawBase.cxx
    AliTPCCalibRaw.cxx
    AliTPCCalibVdrift.cxx
    AliTPCCalibViewer.cxx
    AliTPCCalibViewerGUI.cxx
    AliTPCCalibViewerGUItime.cxx
    AliTPCCalPad.cxx
    AliTPCCalROC.cxx
    AliTPCChebCorr.cxx
    AliTPCChebDist.cxx
    AliTPCclusterInfo.cxx
    AliTPCclusterMI.cxx
    AliTPCClusterParam.cxx
    AliTPCClustersRow.cxx
    AliTPCComposedCorrection.cxx
    AliTPCConfigDA.cxx
    AliTPCConfigParser.cxx
    AliTPCCorrection.cxx
    AliTPCCorrectionDrift.cxx
    AliTPCCorrectionLookupTable.cxx
    AliTPCdataQA.cxx
    AliTPCDigitsArray.cxx
    AliTPCExBBShape.cxx
    AliTPCExB.cxx
    AliTPCExBEffective.cxx
    AliTPCExBEffectiveSector.cxx
    AliTPCExBExact.cxx
    AliTPCExBFirst.cxx
    AliTPCExBTwist.cxx
    AliTPCFCVoltError3D.cxx
    AliTPCGGVoltError.cxx
    AliTPCInverseCorrection.cxx
    AliTPCLaserTrack.cxx
    AliTPCLoader.cxx
    AliTPCmapper.cxx
    AliTPCParam.cxx
    AliTPCParamSR.cxx
    AliTPCPointCorrection.cxx
    AliTPCPreprocessor.cxx
    AliTPCPreprocessorOnline.cxx
    AliTPCPRF2D.cxx
    AliTPCQAChecker.cxx
    AliTPCQADataMakerRec.cxx
    AliTPCRawStreamV3.cxx
    AliTPCRecoParam.cxx
    AliTPCRF1D.cxx
    AliTPCROC.cxx
    AliTPCROCVoltError3D.cxx
    AliTPCSensorTempArray.cxx
    AliTPCSensorTemp.cxx
    AliTPCSpaceCharge3D.cxx
    AliTPCSpaceCharge2D2D.cxx
    AliTPCSpaceCharge.cxx
    AliTPCTempMap.cxx
    AliTPCTransform.cxx
    AliTransform.cxx
    AliXRDPROOFtoolkit.cxx
    AliTPCSAMPAEmulator.cxx
   )

# Headers from sources
string(REPLACE ".cxx" ".h" HDRS "${SRCS}")

# Generate the dictionary
# It will create G_ARG1.cxx and G_ARG1.h / ARG1 = function first argument
get_directory_property(incdirs INCLUDE_DIRECTORIES)
generate_dictionary("${MODULE}" "${MODULE}LinkDef.h" "${HDRS}" "${incdirs}")

set(ROOT_DEPENDENCIES Core EG GenVector Geom Gpad Graf Gui Hist MathCore Matrix Minuit Physics Proof RIO Spectrum Tree)
set(ALIROOT_DEPENDENCIES STEERBase STAT STEER CDB ESD RAWDatabase RAWDatarec)

# Generate the ROOT map
# Dependecies
set(LIBDEPS ${LIBDEPS} ${ALIROOT_DEPENDENCIES} ${ROOT_DEPENDENCIES})
generate_rootmap("${MODULE}" "${LIBDEPS}" "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}LinkDef.h")

# Generate a PARfile target for this library
add_target_parfile(${MODULE} "${SRCS}" "${HDRS}" "${MODULE}LinkDef.h" "${LIBDEPS}")

# Create an object to be reused in case of static libraries
# Otherwise the sources will be compiled twice
add_library(${MODULE}-object OBJECT ${SRCS} G__${MODULE}.cxx)
# Follow headers dependencies
add_dependencies(${MODULE}-object ${ALIROOT_DEPENDENCIES})
# Add a library to the project using the object
add_library(${MODULE} SHARED $<TARGET_OBJECTS:TPCbase-object>)

# Linking
target_link_libraries(${MODULE} ${LIBDEPS})

# Setting the correct headers for the object as gathered from the dependencies
target_include_directories(${MODULE}-object PUBLIC $<TARGET_PROPERTY:${MODULE},INCLUDE_DIRECTORIES>)
set_target_properties(${MODULE}-object PROPERTIES COMPILE_DEFINITIONS $<TARGET_PROPERTY:${MODULE},COMPILE_DEFINITIONS>)

# Public include folders that will be propagated to the dependecies
target_include_directories(${MODULE} PUBLIC ${incdirs})

# Additional compilation flags
set_target_properties(${MODULE}-object PROPERTIES COMPILE_FLAGS "")

# System dependent: Modify the way the library is build
if(${CMAKE_SYSTEM} MATCHES Darwin)
    set_target_properties(${MODULE} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif(${CMAKE_SYSTEM} MATCHES Darwin)

# Installation
install(TARGETS ${MODULE}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

install(FILES ${HDRS} DESTINATION include)

# Static version if DA enabled
if(ALIROOT_STATIC)
    add_library(${MODULE}-static STATIC $<TARGET_OBJECTS:TPCbase-object>)
    set_target_properties(${MODULE}-static PROPERTIES OUTPUT_NAME ${MODULE})

    # list of shared dependencies / the name of the variable containing the list of static ones
    generate_static_dependencies("${ALIROOT_DEPENDENCIES}" "STATIC_ALIROOT_DEPENDENCIES")
    target_link_libraries(${MODULE}-static ${STATIC_ALIROOT_DEPENDENCIES} Root RootExtra)

    # Public include folders that will be propagated to the dependecies
    target_include_directories(${MODULE}-static PUBLIC ${incdirs})

    set_target_properties(${MODULE}-static PROPERTIES LINK_FLAGS "-Wl,--whole-archive")

    # Installation
    install(TARGETS ${MODULE}-static
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib)
endif(ALIROOT_STATIC)
