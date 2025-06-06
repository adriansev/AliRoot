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

#PH add_subdirectory (test)
# Module
set(MODULE Utilities)
add_definitions(-D_MODULE_="${MODULE}")
# Aditional include folders in alphabetical order except ROOT
include_directories(SYSTEM ${ROOT_INCLUDE_DIR})

# Create an object to be reused in case of static libraries
# Otherwise the sources will be compiled twice
# Add a library to the project using the object
add_executable(alihadd alihadd.cxx)
add_executable(alisync alisync.cxx)

target_link_libraries(alihadd RIO Hist Core Net)
target_link_libraries(alisync RIO Hist Core Net Tree)

# Installation
install(TARGETS alihadd RUNTIME DESTINATION bin)
install(TARGETS alisync RUNTIME DESTINATION bin)
install(PROGRAMS alien_rsync.sh DESTINATION bin)
install(PROGRAMS alilog4bash.sh DESTINATION libexec)
install(PROGRAMS utilities.sh   DESTINATION libexec)
install(PROGRAMS alihadd.sh     DESTINATION libexec)

# Tests
#PH add_executable(test_helpers test_helpers.cxx)
#PH add_executable(test_alisync01 test_alisync01.cxx)

#PH enable_testing()
#PH add_test(test_helpers ./test_helpers)
#PH add_test(test_alisync01 ./test_alisync01)

