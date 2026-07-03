# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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

set(PLUGIN_NAME DisplaySettings)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_DISPLAYSETTINGS_AUTOSTART "false" CACHE STRING "Automatically start DisplaySettings plugin")
set(PLUGIN_DISPLAYSETTINGS_STARTUPORDER "" CACHE STRING "To configure startup order of DisplaySettings plugin")

# -----------------------------------------------------------------------
# DS_COMRPC: build against the DeviceSettings Thunder plugin via COM-RPC
#            (sources in DS_COMRPC/, no libds / IARM dependency)
# DS_IARM:   build against libds / IARM directly
#            (sources in DS_IARM/)
#
# Select at configure time:
#   cmake -DDS_COMRPC=ON    (default: OFF -> DS_IARM is used)
# In Yocto:
#   EXTRA_OECMAKE += "-DDS_COMRPC=ON"
# -----------------------------------------------------------------------
option(DS_COMRPC
    "Build DisplaySettings using the DeviceSettings COM-RPC plugin (DS_COMRPC/).  OFF = build using libds/IARM (DS_IARM/)."
    OFF)

set(PLUGIN_DISPLAYSETTINGS_DEVICESETTINGS_CALLSIGN "org.rdk.DeviceSettings"
    CACHE STRING "Callsign of the DeviceSettings plugin (used by DS_COMRPC build)")

find_package(${NAMESPACE}Plugins REQUIRED)

# ---------------------------------------------------------------------------
# Source selection: no USE_DEVICESETTING_PLUGIN compile-time macro needed.
# Each subdirectory contains a self-contained, guard-free implementation.
# ---------------------------------------------------------------------------
if (DS_COMRPC)
    message(STATUS "DisplaySettings: DS_COMRPC=ON -- building from DS_COMRPC/ (callsign=${PLUGIN_DISPLAYSETTINGS_DEVICESETTINGS_CALLSIGN})")
    set(DS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/DS_COMRPC)
else()
    message(STATUS "DisplaySettings: DS_COMRPC=OFF -- building from DS_IARM/ (libds / IARM)")
    set(DS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/DS_IARM)
endif()

add_library(${MODULE_NAME} SHARED
	${DS_SOURCE_DIR}/DisplaySettings.cpp
	Module.cpp)

target_include_directories(${MODULE_NAME} PRIVATE ${DS_SOURCE_DIR})

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES)

target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

if (RDK_SERVICE_L2_TEST)
    find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
    if (TESTMOCKLIB_LIBRARIES)
        message("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
        target_link_libraries(${MODULE_NAME} PRIVATE ${TESTMOCKLIB_LIBRARIES})
    else()
        message("Require ${TESTMOCKLIB_LIBRARIES} library")
    endif()
endif()

# ---------------------------------------------------------------------------
# Link dependencies depend on which implementation is selected
# ---------------------------------------------------------------------------
if (DS_COMRPC)
    # COM-RPC path: entservices-helpers provides DeviceSettingsClientHelper.h
    add_definitions(-DPLUGIN_DISPLAYSETTINGS_DEVICESETTINGS_CALLSIGN="${PLUGIN_DISPLAYSETTINGS_DEVICESETTINGS_CALLSIGN}")
    find_package(${NAMESPACE}Helpers REQUIRED)
    target_link_libraries(${MODULE_NAME}
        PRIVATE
            ${NAMESPACE}Plugins::${NAMESPACE}Plugins
            ${NAMESPACE}::${NAMESPACE}Helpers
            "-ltr181api")
else()
    # libds / IARM path
    find_package(DS)
    if (DS_FOUND)
        find_package(IARMBus)
        add_definitions(-DDS_FOUND)
        target_include_directories(${MODULE_NAME} PRIVATE ${IARMBUS_INCLUDE_DIRS})
        target_include_directories(${MODULE_NAME} PRIVATE ${DS_INCLUDE_DIRS})
        if (RDK_SERVICE_L2_TEST)
            target_link_libraries(${MODULE_NAME} PRIVATE
                ${NAMESPACE}Plugins::${NAMESPACE}Plugins
                ${IARMBUS_LIBRARIES}
                ${DS_LIBRARIES}
                ${NAMESPACE}::${NAMESPACE}Helpers)
        else()
            target_link_libraries(${MODULE_NAME} PRIVATE
                ${NAMESPACE}Plugins::${NAMESPACE}Plugins
                ${IARMBUS_LIBRARIES}
                ${DS_LIBRARIES}
                "-ltr181api"
                ${NAMESPACE}::${NAMESPACE}Helpers)
        endif()
    else()
        target_link_libraries(${MODULE_NAME} PRIVATE
            ${NAMESPACE}Plugins::${NAMESPACE}Plugins
            ${NAMESPACE}::${NAMESPACE}Helpers)
    endif()
endif()

install(TARGETS ${MODULE_NAME}
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
