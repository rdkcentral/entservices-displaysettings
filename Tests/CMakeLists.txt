# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "ENTSRVICECOMRPCTestClients")
set(DESCRIPTION "EntServices COMRPC Client to test the COMRPC implementation.")
set(DEFAULT_BUILD_TYPE "Release")

project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(TESTBINPREFIX "ComRPCPlugin")
set(MODULE_NAME ${TESTBINPREFIX})

# Include common directories and libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(WPEFRAMEWORKCORE REQUIRED WPEFrameworkCore)
pkg_check_modules(WPEFRAMEWORKPLUGINS REQUIRED WPEFrameworkPlugins)
pkg_check_modules(WPEFRAMEWORKWEBSOCKET REQUIRED WPEFrameworkWebSocket)
pkg_check_modules(WPEFRAMEWORKDEFINITIONS REQUIRED WPEFrameworkDefinitions)
pkg_check_modules(WPEFRAMEWORKPLUGINS REQUIRED WPEFrameworkPlugins)

set(COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WPEFRAMEWORKCORE_INCLUDE_DIRS}
    ${WPEFRAMEWORKPLUGINS_INCLUDE_DIRS}
    ${WPEFRAMEWORKWEBSOCKET_INCLUDE_DIRS}
    ${WPEFRAMEWORKDEFINITIONS_INCLUDE_DIRS}
    ${WPEFRAMEWORKPLUGINS_INCLUDE_DIRS}
    )

set(COMMON_LIBRARIES
    ${WPEFRAMEWORKCORE_LIBRARIES}
    ${WPEFRAMEWORKPLUGINS_LIBRARIES}
    ${WPEFRAMEWORKWEBSOCKET_LIBRARIES}
    ${WPEFRAMEWORKDEFINITIONS_LIBRARIES}
    ${WPEFRAMEWORKPLUGINS_LIBRARIES}
    )

message(STATUS "Common include directories: ${COMMON_INCLUDE_DIRS}")
message(STATUS "Common libraries: ${COMMON_LIBRARIES}")

include(GNUInstallDirs)

# List to hold dynamically added targets
set(TEST_TARGETS)

if (FRAMERATETESTAPP)
    message(STATUS "Framerate test application is enabled.")
    set(FRAMERATE_EXECUTABLE_NAME "${TESTBINPREFIX}FrameRateTest")
    add_executable(${FRAMERATE_EXECUTABLE_NAME} entServicesCOMRPC-FrameRateTest.cpp)
    target_compile_definitions(${FRAMERATE_EXECUTABLE_NAME} PUBLIC MODULE_NAME=${FRAMERATE_EXECUTABLE_NAME})
    target_include_directories(${FRAMERATE_EXECUTABLE_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_link_libraries(${FRAMERATE_EXECUTABLE_NAME} PRIVATE ${COMMON_LIBRARIES})
    list(APPEND TEST_TARGETS ${FRAMERATE_EXECUTABLE_NAME})
else()
    message(STATUS "Framerate test application is disabled.")
endif()

# Install dynamically added targets
if (TEST_TARGETS)
    install(TARGETS ${TEST_TARGETS} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    message(STATUS "No test applications enabled for installation.")
endif()

