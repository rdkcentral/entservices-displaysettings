/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <iostream>
#include <functional>
#include <string>
#include "dDisplay.h"
#include "dsDisplay.h"
#include "dsError.h"
#include "dsUtl.h"
#include "dsTypes.h"
//#include "dsRpc.h"
#include "UtilsLogging.h"

#include <WPEFramework/interfaces/IDeviceSettingsDisplay.h>
#include "DeviceSettingsTypes.h"

#ifndef RDK_DSHAL_NAME
#warning   "RDK_DSHAL_NAME is not defined"
#define RDK_DSHAL_NAME "RDK_DSHAL_NAME is not defined"
#endif

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

static int display_isInitialized = 0;
static int display_isPlatInitialized = 0;
// Suppress unused variable warnings for compatibility
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
static bool isEdidCached __attribute__((unused)) = false;
static bool isEdidBytesCached __attribute__((unused)) = false;
#pragma GCC diagnostic pop
static pthread_mutex_t dsDisplayLock = PTHREAD_MUTEX_INITIALIZER;

// Static global callback functions for Display events
static std::function<void(const uint8_t, const bool)> g_DisplayRxSenseCallback;
static std::function<void(const uint8_t, const bool)> g_DisplayHDCPStatusCallback;
static std::function<void(const uint8_t, const bool)> g_DisplayHDMIHotPlugCallback;

class dDisplayImpl : public hal::dDisplay::IPlatform {

    // delete copy constructor and assignment operator
    dDisplayImpl(const dDisplayImpl&) = delete; 
    dDisplayImpl& operator=(const dDisplayImpl&) = delete;

public:
    dDisplayImpl()
    {
        LOGINFO("dDisplayImpl Constructor");
        getInstance() = this; // Set static instance for callback access
        InitialiseHAL();
    }

    virtual ~dDisplayImpl()
    {
        LOGINFO("dDisplayImpl Destructor");
        DeInitialiseHAL();
        getInstance() = nullptr; // Clear static instance
    }

    // Resolve method for dynamic library loading - following dHdmiInImpl.h pattern
    static void* resolve(const std::string& libName, const std::string& symbolName) {
        void* handle = dlopen(libName.c_str(), RTLD_LAZY);
        if (!handle) {
            LOGERR("dlopen failed for %s: %s", libName.c_str(), dlerror());
            return nullptr;
        }
        void* symbol = dlsym(handle, symbolName.c_str());
        if (!symbol) {
            LOGERR("dlsym failed for %s: %s", symbolName.c_str(), dlerror());
        }
        dlclose(handle);
        return symbol;
    }

    // Singleton getInstance method - following VideoPort pattern
    static dDisplayImpl*& getInstance()
    {
        static dDisplayImpl* instance = nullptr;
        return instance;
    }

    void InitialiseHAL()
    {
        LOGINFO("InitialiseHAL");
        
        if (!display_isPlatInitialized) {
            LOGINFO("InitialiseHAL <dsDisplay>");
            dsError_t eError = dsDisplayInit();
            if (dsERR_NONE != eError) {
                LOGERR("InitialiseHAL: dsDisplayInit failed with error: %d", eError);
                return;
            }
            LOGINFO("InitialiseHAL: dsDisplayInit succeeded");
            
            // Load persistence values after successful initialization
            getPersistenceValue();
            
            display_isPlatInitialized = 1;
            LOGINFO("InitialiseHAL completed: display_isPlatInitialized=%d, display_isInitialized=%d", 
                    display_isPlatInitialized, display_isInitialized);
        }
    }

    void DeInitialiseHAL()
    {
        LOGINFO("DeInitialiseHAL");
        if (display_isPlatInitialized)
        {
            dsDisplayTerm();
            display_isPlatInitialized = 0;
        }
        display_isInitialized = 0;
    }

    void setAllCallbacks(const CallbackBundle& bundle) override
    {
        LOGINFO("dDisplayImpl setAllCallbacks");
        
        if (!display_isInitialized) {
            // Set the global callback function pointers
            g_DisplayRxSenseCallback = bundle.DisplayRxSenseEventCallback;
            g_DisplayHDCPStatusCallback = bundle.DisplayHDCPStatusEventCallback;
            g_DisplayHDMIHotPlugCallback = bundle.DisplayHDMIHotPlugEventCallback;

            // Register HAL callbacks
            registerDisplayEventCallbacks();
            
            display_isInitialized = 1;
            LOGINFO("dDisplayImpl setAllCallbacks: Display callbacks registered successfully");
        } else {
            LOGINFO("dDisplayImpl setAllCallbacks: Display already initialized, skipping callback registration");
        }
    }

    void getPersistenceValue() override
    {
        LOGINFO("dDisplayImpl getPersistenceValue - Display persistence loading");
        // Load any display-specific persistence values here
        // This would be similar to VideoPort persistence loading but for Display settings
    }

    // Implementation of Display Platform interface methods
    uint32_t GetConnectedVideoDisplay(const int32_t videoPortHandle, bool& isConnected) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetConnectedVideoDisplay: videoPortHandle=%d", videoPortHandle);
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use dynamic library loading for dsIsDisplayConnected
        typedef dsError_t (*dsIsDisplayConnected_t)(intptr_t handle, bool *connected);
        static dsIsDisplayConnected_t func = 0;
        if (func == 0) {
            void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
            if (dllib) {
                func = (dsIsDisplayConnected_t) dlsym(dllib, "dsIsDisplayConnected");
                dlclose(dllib);
            }
        }
        
        if (func != 0) {
            dsError_t eError = func(static_cast<intptr_t>(videoPortHandle), &isConnected);
            if (eError == dsERR_NONE) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetConnectedVideoDisplay: SUCCESS - isConnected=%s", isConnected ? "true" : "false");
            } else {
                LOGERR("GetConnectedVideoDisplay: FAILED - dsIsDisplayConnected error=%d", eError);
            }
        } else {
            LOGERR("GetConnectedVideoDisplay: FAILED - dsIsDisplayConnected not available");
        }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

    uint32_t GetDisplaySurroundMode(const int32_t videoPortHandle, VideoPortSurroundMode& surroundMode) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetDisplaySurroundMode: videoPortHandle=%d", videoPortHandle);
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use dynamic library loading for dsGetDisplaySurroundMode
        typedef dsError_t (*dsGetDisplaySurroundMode_t)(intptr_t handle, int *surroundMode);
        static dsGetDisplaySurroundMode_t func = 0;
        if (func == 0) {
            void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
            if (dllib) {
                func = (dsGetDisplaySurroundMode_t) dlsym(dllib, "dsGetDisplaySurroundMode");
                dlclose(dllib);
            }
        }
        
        if (func != 0) {
            int dsSurroundMode = 0;
            dsError_t eError = func(static_cast<intptr_t>(videoPortHandle), &dsSurroundMode);
            if (eError == dsERR_NONE) {
                surroundMode = static_cast<VideoPortSurroundMode>(dsSurroundMode);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetDisplaySurroundMode: SUCCESS - surroundMode=%d", static_cast<int>(surroundMode));
            } else {
                LOGERR("GetDisplaySurroundMode: FAILED - dsGetDisplaySurroundMode error=%d", eError);
            }
        } else {
            LOGERR("GetDisplaySurroundMode: FAILED - dsGetDisplaySurroundMode not available");
        }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

    uint32_t GetDisplayEDID(const int32_t videoPortHandle, uint8_t edidBytes[], const uint16_t edidBytesLength) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetDisplayEDID: videoPortHandle=%d, edidBytesLength=%d", videoPortHandle, edidBytesLength);
        
        if (!edidBytes || edidBytesLength <= 0) {
            LOGERR("GetDisplayEDID: FAILED - Invalid parameters");
            return WPEFramework::Core::ERROR_BAD_REQUEST;
        }
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use dynamic library loading for dsGetEDIDBytes  
        typedef dsError_t (*dsGetEDIDBytes_t)(intptr_t handle, unsigned char *edid, int *length);
        static dsGetEDIDBytes_t func = 0;
        if (func == 0) {
            void *dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
            if (dllib) {
                func = (dsGetEDIDBytes_t) dlsym(dllib, "dsGetEDIDBytes");
                dlclose(dllib);
            }
        }
        
        if (func != 0) {
            int actualLength = edidBytesLength;
            dsError_t eError = func(static_cast<intptr_t>(videoPortHandle), edidBytes, &actualLength);
            if (eError == dsERR_NONE && actualLength <= edidBytesLength) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetDisplayEDID: SUCCESS - actualLength=%d", actualLength);
            } else {
                LOGERR("GetDisplayEDID: FAILED - dsGetEDIDBytes error=%d, actualLength=%d", eError, actualLength);
            }
        } else {
            LOGERR("GetDisplayEDID: FAILED - dsGetEDIDBytes not available");
        }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

    uint32_t GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetDisplayEdidBytes: handle=%d, edidLength=%d", handle, edidLength);
        
        if (edIdBytes == nullptr || edidLength == 0) {
            LOGERR("GetDisplayEdidBytes: FAILED - Invalid parameters");
            return retCode;
        }
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use resolve method for dsGetEDIDBytes (matches dsDisplay.c pattern)
        typedef dsError_t (*dsGetEDIDBytes_t)(intptr_t handle, uint8_t *edidBytes, int *actualLength);
        static dsGetEDIDBytes_t func = 0;
        if (func == 0) {
            func = (dsGetEDIDBytes_t) resolve(RDK_DSHAL_NAME, "dsGetEDIDBytes");
        }
        
        if (func != 0) {
            int actualLength = 0;
            dsError_t eError = func(handle, edIdBytes, &actualLength);
            if (eError == dsERR_NONE && actualLength <= edidLength) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetDisplayEdidBytes: SUCCESS - actualLength=%d", actualLength);
            } else {
                LOGERR("GetDisplayEdidBytes: FAILED - dsGetEDIDBytes error=%d, actualLength=%d", eError, actualLength);
            }
        } else {
            LOGERR("GetDisplayEdidBytes: FAILED - dsGetEDIDBytes not available");
        }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

    // New Display HAL methods implementation
    uint32_t GetDisplay(const int32_t type, const int32_t index, int32_t &handle) override 
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetDisplay: type=%d, index=%d", type, index);

        // Validate input parameters
        if (type < 0 || index < 0) {
            LOGERR("GetDisplay: FAILED - Invalid parameters, type=%d, index=%d", type, index);
            return WPEFramework::Core::ERROR_BAD_REQUEST;
        }

        // Initialize handle to safe value
        handle = -1;

        // Add safety check for mutex lock
        int lock_result = pthread_mutex_lock(&dsDisplayLock);
        if (lock_result != 0) {
            LOGERR("GetDisplay: FAILED - Could not acquire mutex lock, error=%d", lock_result);
            return WPEFramework::Core::ERROR_GENERAL;
        }

        // Use direct call for dsGetDisplay (matches dsDisplay.c _dsGetDisplay pattern)
        intptr_t halHandle = 0;
        LOGINFO("GetDisplay: Calling dsGetDisplay with type=%d, index=%d", type, index);
        
        dsError_t eError = dsGetDisplay(static_cast<dsVideoPortType_t>(type), index, &halHandle);
        
        if (eError == dsERR_NONE) {
            handle = static_cast<int32_t>(halHandle);
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetDisplay: SUCCESS - handle=%d", handle);
        } else {
            LOGERR("GetDisplay: FAILED - dsGetDisplay error=%d", eError);
            handle = -1;  // Ensure handle is set to safe value on error
        }
        
        int unlock_result = pthread_mutex_unlock(&dsDisplayLock);
        if (unlock_result != 0) {
            LOGERR("GetDisplay: WARNING - Could not release mutex lock, error=%d", unlock_result);
        }
        
        return retCode;
    }

    uint32_t GetDisplayAspectRatio(const int32_t handle, DisplayVideoAspectRatio &aspectRatio) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetDisplayAspectRatio: handle=%d", handle);
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use direct call for dsGetDisplayAspectRatio (matches dsDisplay.c _dsGetDisplayAspectRatio pattern)
        dsVideoAspectRatio_t halAspectRatio;
        dsError_t eError = dsGetDisplayAspectRatio(handle, &halAspectRatio);
        if (eError == dsERR_NONE) {
            // Convert DS HAL type to WPE Framework type
            aspectRatio = (halAspectRatio == dsVIDEO_ASPECT_RATIO_4x3) ? 
                         DisplayVideoAspectRatio::DS_DISPLAY_ASPECT_RATIO_4X3 : 
                         DisplayVideoAspectRatio::DS_DISPLAY_ASPECT_RATIO_16X9;
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetDisplayAspectRatio: SUCCESS - aspectRatio=%d", static_cast<int>(aspectRatio));
        } else {
            LOGERR("GetDisplayAspectRatio: FAILED - dsGetDisplayAspectRatio error=%d", eError);
        }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

    uint32_t GetDisplayEdid(const int32_t handle, WPEFramework::Exchange::IDeviceSettingsDisplay::DisplayEDID &edId) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetDisplayEdid: handle=%d", handle);
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use direct call for dsGetEDID (matches dsDisplay.c _dsGetEDID pattern)
        dsDisplayEDID_t halEdid;
        dsError_t eError = dsGetEDID(handle, &halEdid);
            if (eError == dsERR_NONE) {
                // Convert DS HAL type to WPE Framework type
                edId.productCode = halEdid.productCode;
                edId.serialNumber = halEdid.serialNumber;
                edId.manufactureYear = halEdid.manufactureYear;
                edId.manufactureWeek = halEdid.manufactureWeek;
                edId.hdmiDeviceType = halEdid.hdmiDeviceType;
                edId.isRepeater = halEdid.isRepeater;
                edId.physicalAddressA = halEdid.physicalAddressA;
                edId.physicalAddressB = halEdid.physicalAddressB;
                edId.physicalAddressC = halEdid.physicalAddressC;
                edId.physicalAddressD = halEdid.physicalAddressD;
                edId.numOfSupportedResolution = halEdid.numOfSupportedResolution;
                edId.monitorName = std::string(halEdid.monitorName);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetDisplayEdid: SUCCESS");
            } else {
                LOGERR("GetDisplayEdid: FAILED - dsGetEDID error=%d", eError);
            }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

    uint32_t SetAllmEnabled(const int32_t handle, const bool enabled) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetAllmEnabled: handle=%d, enabled=%s", handle, enabled ? "true" : "false");
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use resolve method for dsSetAllmEnabled (matches dsDisplay.c pattern)
        typedef dsError_t (*dsSetAllmEnabled_t)(intptr_t handle, bool enabled);
        typedef dsError_t (*dsGetAllmEnabled_t)(intptr_t handle, bool *enabled);
        static dsSetAllmEnabled_t func_dsSetAllmEnabled = 0;
        static dsGetAllmEnabled_t func_dsGetAllmEnabled = 0;
        if (func_dsGetAllmEnabled == 0 && func_dsSetAllmEnabled == 0) {
            func_dsGetAllmEnabled = (dsGetAllmEnabled_t) resolve(RDK_DSHAL_NAME, "dsGetAllmEnabled");
            func_dsSetAllmEnabled = (dsSetAllmEnabled_t) resolve(RDK_DSHAL_NAME, "dsSetAllmEnabled");
        }
        
        if (func_dsGetAllmEnabled != 0 && func_dsSetAllmEnabled != 0) {
            bool currentALLMState = false;
            dsError_t eError = func_dsGetAllmEnabled(handle, &currentALLMState);
            if (eError == dsERR_NONE) {
                if (currentALLMState == enabled) {
                    LOGINFO("SetAllmEnabled: ALLM mode already %s", enabled ? "Enabled" : "Disabled");
                    retCode = WPEFramework::Core::ERROR_NONE;
                } else {
                    LOGINFO("SetAllmEnabled: Current ALLM state %s, Requested to %s", 
                           currentALLMState ? "Enabled" : "Disabled", enabled ? "Enabled" : "Disabled");
                    eError = func_dsSetAllmEnabled(handle, enabled);
                    if (eError == dsERR_NONE) {
                        retCode = WPEFramework::Core::ERROR_NONE;
                        LOGINFO("SetAllmEnabled: SUCCESS");
                    } else {
                        LOGERR("SetAllmEnabled: FAILED - dsSetAllmEnabled error=%d", eError);
                    }
                }
            } else {
                LOGERR("SetAllmEnabled: FAILED - dsGetAllmEnabled error=%d", eError);
            }
        } else {
            LOGERR("SetAllmEnabled: FAILED - dsSetAllmEnabled/dsGetAllmEnabled not available");
        }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

    uint32_t SetAVIContentType(const int32_t handle, const int32_t contentType) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetAVIContentType: handle=%d, contentType=%d", handle, contentType);
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use resolve method for dsSetAVIContentType (matches dsDisplay.c pattern)
        typedef dsError_t (*dsSetAVIContentType_t)(intptr_t handle, dsAviContentType_t contentType);
        typedef dsError_t (*dsGetAVIContentType_t)(intptr_t handle, dsAviContentType_t* contentType);
        static dsSetAVIContentType_t func_dsSetAVIContentType = 0;
        static dsGetAVIContentType_t func_dsGetAVIContentType = 0;
        if (func_dsGetAVIContentType == 0 && func_dsSetAVIContentType == 0) {
            func_dsSetAVIContentType = (dsSetAVIContentType_t) resolve(RDK_DSHAL_NAME, "dsSetAVIContentType");
            func_dsGetAVIContentType = (dsGetAVIContentType_t) resolve(RDK_DSHAL_NAME, "dsGetAVIContentType");
        }
        
        if (func_dsGetAVIContentType != 0 && func_dsSetAVIContentType != 0) {
            dsAviContentType_t currentContentType = dsAVICONTENT_TYPE_NOT_SIGNALLED;
            dsError_t eError = func_dsGetAVIContentType(handle, &currentContentType);
            if (eError == dsERR_NONE) {
                if (currentContentType == static_cast<dsAviContentType_t>(contentType)) {
                    LOGINFO("SetAVIContentType: HDMI AVI content type already set to %d", contentType);
                    retCode = WPEFramework::Core::ERROR_NONE;
                } else {
                    LOGINFO("SetAVIContentType: Current AVI content type %d, requested content type %d", 
                           currentContentType, contentType);
                    eError = func_dsSetAVIContentType(handle, static_cast<dsAviContentType_t>(contentType));
                    if (eError == dsERR_NONE) {
                        retCode = WPEFramework::Core::ERROR_NONE;
                        LOGINFO("SetAVIContentType: SUCCESS");
                    } else {
                        LOGERR("SetAVIContentType: FAILED - dsSetAVIContentType error=%d", eError);
                    }
                }
            } else {
                LOGERR("SetAVIContentType: FAILED - dsGetAVIContentType error=%d", eError);
            }
        } else {
            LOGERR("SetAVIContentType: FAILED - dsSetAVIContentType/dsGetAVIContentType not available");
        }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

    uint32_t SetAVIScanInformation(const int32_t handle, const int32_t scanInfo) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetAVIScanInformation: handle=%d, scanInfo=%d", handle, scanInfo);
        
        pthread_mutex_lock(&dsDisplayLock);
        
        // Use resolve method for dsSetAVIScanInformation (matches dsDisplay.c pattern)
        typedef dsError_t (*dsSetAVIScanInfo_t)(intptr_t handle, dsAVIScanInformation_t scanInfo);
        typedef dsError_t (*dsGetAVIScanInfo_t)(intptr_t handle, dsAVIScanInformation_t* scanInfo);
        static dsSetAVIScanInfo_t func_dsSetAVIScanInfo = 0;
        static dsGetAVIScanInfo_t func_dsGetAVIScanInfo = 0;
        if (func_dsGetAVIScanInfo == 0 && func_dsSetAVIScanInfo == 0) {
            func_dsSetAVIScanInfo = (dsSetAVIScanInfo_t) resolve(RDK_DSHAL_NAME, "dsSetAVIScanInformation");
            func_dsGetAVIScanInfo = (dsGetAVIScanInfo_t) resolve(RDK_DSHAL_NAME, "dsGetAVIScanInformation");
        }
        
        if (func_dsGetAVIScanInfo != 0 && func_dsSetAVIScanInfo != 0) {
            dsAVIScanInformation_t currentScanInfo = dsAVI_SCAN_TYPE_NO_DATA;
            dsError_t eError = func_dsGetAVIScanInfo(handle, &currentScanInfo);
            if (eError == dsERR_NONE) {
                if (currentScanInfo == static_cast<dsAVIScanInformation_t>(scanInfo)) {
                    LOGINFO("SetAVIScanInformation: HDMI AVI scan Info already set to %d", scanInfo);
                    retCode = WPEFramework::Core::ERROR_NONE;
                } else {
                    LOGINFO("SetAVIScanInformation: Current AVI scan Info %d, requested scan Info %d", 
                           currentScanInfo, scanInfo);
                    eError = func_dsSetAVIScanInfo(handle, static_cast<dsAVIScanInformation_t>(scanInfo));
                    if (eError == dsERR_NONE) {
                        retCode = WPEFramework::Core::ERROR_NONE;
                        LOGINFO("SetAVIScanInformation: SUCCESS");
                    } else {
                        LOGERR("SetAVIScanInformation: FAILED - dsSetAVIScanInformation error=%d", eError);
                    }
                }
            } else {
                LOGERR("SetAVIScanInformation: FAILED - dsGetAVIScanInformation error=%d", eError);
            }
        } else {
            LOGERR("SetAVIScanInformation: FAILED - dsSetAVIScanInformation/dsGetAVIScanInformation not available");
        }
        
        pthread_mutex_unlock(&dsDisplayLock);
        return retCode;
    }

private:
    void registerDisplayEventCallbacks()
    {
        LOGINFO("registerDisplayEventCallbacks");
        
        // Use direct calls matching dsDisplay.c _dsDisplayInit pattern
        intptr_t handle = 0;
        dsError_t eReturn = dsGetDisplay(dsVIDEOPORT_TYPE_HDMI, 0, &handle);
        if (dsERR_NONE != eReturn) {
            LOGINFO("registerDisplayEventCallbacks: dsGetDisplay for HDMI failed, trying INTERNAL");
            eReturn = dsGetDisplay(dsVIDEOPORT_TYPE_INTERNAL, 0, &handle);
            if (dsERR_NONE != eReturn) {
                LOGERR("registerDisplayEventCallbacks: FAILED - dsGetDisplay for INTERNAL also failed, error=%d", eReturn);
                return;
            }
        }
        
        // Register display event callback directly
        dsError_t eError = dsRegisterDisplayEventCallback(handle, dsDisplayEventCallback);
        if (eError == dsERR_NONE) {
            LOGINFO("registerDisplayEventCallbacks: SUCCESS - registered with handle=%d", static_cast<int>(handle));
        } else {
            LOGERR("registerDisplayEventCallbacks: FAILED - error=%d", eError);
        }
    }

    // Static callback function to handle display events from HAL
    static void dsDisplayEventCallback(intptr_t handle, dsDisplayEvent_t dsDisplayEvent, void* eventData)
    {
        LOGINFO("dsDisplayEventCallback: handle=%d, event=%d", static_cast<int>(handle), static_cast<int>(dsDisplayEvent));
        
        dDisplayImpl* instance = getInstance();
        if (!instance) {
            LOGERR("dsDisplayEventCallback: No Display instance available");
            return;
        }

        uint8_t port = static_cast<uint8_t>(handle & 0xFF); // Extract port from handle

        switch (dsDisplayEvent) {
            case dsDISPLAY_RXSENSE_ON: // DS_DISPLAY_RXSENSE_ON equivalent
                if (g_DisplayRxSenseCallback) {
                    g_DisplayRxSenseCallback(port, true);
                }
                break;
                
            case dsDISPLAY_RXSENSE_OFF: // DS_DISPLAY_RXSENSE_OFF equivalent  
                if (g_DisplayRxSenseCallback) {
                    g_DisplayRxSenseCallback(port, false);
                }
                break;
                
            case dsDISPLAY_HDCPPROTOCOL_CHANGE: // DS_DISPLAY_HDCPPROTOCOL_CHANGE equivalent
                if (g_DisplayHDCPStatusCallback && eventData) {
                    bool isAuthenticated = *static_cast<bool*>(eventData);
                    g_DisplayHDCPStatusCallback(port, isAuthenticated);
                }
                break;
                
            case dsDISPLAY_EVENT_CONNECTED: // DS_DISPLAY_EVENT_CONNECTED equivalent
                if (g_DisplayHDMIHotPlugCallback) {
                    g_DisplayHDMIHotPlugCallback(port, true);
                }
                break;
                
            case dsDISPLAY_EVENT_DISCONNECTED: // DS_DISPLAY_EVENT_DISCONNECTED equivalent
                if (g_DisplayHDMIHotPlugCallback) {
                    g_DisplayHDMIHotPlugCallback(port, false);
                }
                break;
                
            default:
                LOGWARN("dsDisplayEventCallback: Unknown event=%d", static_cast<int>(dsDisplayEvent));
                break;
        }
    }
};