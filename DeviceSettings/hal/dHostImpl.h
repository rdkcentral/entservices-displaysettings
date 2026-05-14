// Out-of-line virtual destructor definition for RTTI/typeinfo
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
#include <unordered_map>
#include "dHost.h"
#include "dsHost.h"
#include "dsError.h"
#include "dsUtl.h"
#include "dsTypes.h"
#include "dsRpc.h"
#include "UtilsLogging.h"

#include <WPEFramework/interfaces/IDeviceSettingsHost.h>
#include "DeviceSettingsTypes.h"

#include "../helpers/UtilsSearchRDKProfile.h"

// Static global variables from dsHost.cpp conversion
static int host_isInitialized = 0;
static int host_isPlatInitialized = 0;
static dsSleepMode_t srv_SleepMode = dsHOST_SLEEP_MODE_LIGHT;

// MS12 Configuration constants
#ifndef MS12_CONFIG_BUF_SIZE
#define MS12_CONFIG_BUF_SIZE 256
#endif

// EDID constants
#ifndef EDID_MAX_DATA_SIZE
#define EDID_MAX_DATA_SIZE 1024
#endif

// HAL API version constants
#define DSHAL_API_VERSION_MAJOR_DEFAULT 1
#define DSHAL_API_VERSION_MINOR_DEFAULT 0

// Static global callback functions for Host events - following VideoPort/HDMIIn pattern
static std::function<void(const HostSleepMode)> g_HostSleepModeChangedCallback;

// DS HAL function type definitions
typedef dsError_t (*dsGetPreferredSleepModeFunc_t)(dsSleepMode_t *mode);
typedef dsError_t (*dsSetPreferredSleepModeFunc_t)(dsSleepMode_t mode);
typedef dsError_t (*dsGetCPUTemperatureFunc_t)(float *cpuTemperature);
typedef dsError_t (*dsGetVersionFunc_t)(uint32_t *versionNumber);
typedef dsError_t (*dsGetSocIDFromSDKFunc_t)(char* socID);
typedef dsError_t (*dsGetHostEDIDFunc_t)(unsigned char *edid, int *length);

class dHostImpl : public hal::dHost::IPlatform {

    // delete copy constructor and assignment operator
    dHostImpl(const dHostImpl&) = delete;
    dHostImpl& operator=(const dHostImpl&) = delete;

public:
    dHostImpl()
    {
        LOGINFO("dHostImpl Constructor");
        getInstance() = this; // Set static instance for callback access
        InitialiseHAL();
    }

    virtual ~dHostImpl()
    {
        LOGINFO("dHostImpl Destructor");
        DeInitialiseHAL();
        getInstance() = nullptr; // Clear static instance
    }

    // Singleton getInstance method - following VideoPort/HDMIIn pattern
    static dHostImpl*& getInstance()
    {
        static dHostImpl* instance = nullptr;
        return instance;
    }

    void InitialiseHAL()
    {
        LOGINFO("InitialiseHAL");
        // Note: host_isInitialized should only be set in setAllCallbacks after callback registration
        // Don't set it here as it prevents callback registration condition from working

        if (!host_isPlatInitialized) {
            LOGINFO("InitialiseHAL <dsHost>");
            dsError_t eError = dsHostInit();
            if (dsERR_NONE != eError) {
                LOGERR("InitialiseHAL: dsHostInit failed with error: %d", eError);
                return;
            }
            host_isPlatInitialized = 1;
            LOGINFO("InitialiseHAL: dsHost HAL initialized successfully");
            
            // Load persistence values - following dsHost.cpp dsHostMgr_init pattern
            getPersistenceValue();
        }
    }

    void DeInitialiseHAL()
    {
        LOGINFO("DeInitialiseHAL");
        if (host_isPlatInitialized) {
            dsError_t eError = dsHostTerm();
            if (dsERR_NONE != eError) {
                LOGERR("DeInitialiseHAL: dsHostTerm failed with error: %d", eError);
            }
            host_isPlatInitialized = 0;
            LOGINFO("DeInitialiseHAL: dsHost HAL de-initialized successfully");
        }
    }

    uint32_t GetPreferredSleepMode(HostSleepMode &mode) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetPreferredSleepMode");

        // Return the cached sleep mode
        mode = convertDSSleepMode(srv_SleepMode);
        retCode = WPEFramework::Core::ERROR_NONE;
        LOGINFO("GetPreferredSleepMode: SUCCESS - mode=%d (%s)", static_cast<int>(mode), enumToString(srv_SleepMode).c_str());

        return retCode;
    }

    uint32_t SetPreferredSleepMode(const HostSleepMode mode) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetPreferredSleepMode: mode=%d", static_cast<int>(mode));

        try {
            dsSleepMode_t dsMode = convertHostSleepModeToDS(mode);

            // Persist the sleep mode setting
            device::HostPersistence::getInstance().persistHostProperty("Power.Mode", enumToString(dsMode));
            srv_SleepMode = dsMode;

            // Trigger sleep mode changed callback
            if (g_HostSleepModeChangedCallback) {
                g_HostSleepModeChangedCallback(mode);
            }

            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("SetPreferredSleepMode: SUCCESS - mode set to %s", enumToString(dsMode).c_str());

        } catch (const std::exception& e) {
            LOGERR("SetPreferredSleepMode: Error in persisting the Power Mode: %s", e.what());
        } catch (...) {
            LOGERR("SetPreferredSleepMode: Unknown error in persisting the Power Mode");
        }

        return retCode;
    }

    uint32_t GetCPUTemperature(float &temperature) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetCPUTemperature");

        #ifdef HAS_THERMAL_API
        // Use resolve function like other methods for consistency
        typedef dsError_t (*dsGetCPUTemperatureFunc_t)(float *cpuTemperature);
        dsGetCPUTemperatureFunc_t func = (dsGetCPUTemperatureFunc_t)resolve(RDK_DSHAL_NAME, "dsGetCPUTemperature");

        if (func != nullptr) {
            float cpuTemp = 45.0f;
            dsError_t eError = func(&cpuTemp);
            if (eError == dsERR_NONE) {
                temperature = cpuTemp;
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetCPUTemperature: SUCCESS - temperature=%.2fC", temperature);
            } else {
                LOGERR("GetCPUTemperature: dsGetCPUTemperature failed with error: %d", eError);
            }
        } else {
            LOGERR("GetCPUTemperature: Function not available");
        }
        #else
        LOGINFO("GetCPUTemperature: Thermal API not compiled");
        #endif

        return retCode;
    }

    uint32_t GetHALVersion(uint32_t &versionNo) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_NONE;
        LOGINFO("GetHALVersion");

        // Following dsHost.cpp pattern - return static default version without calling HAL
        versionNo = dsHAL_APIVER(DSHAL_API_VERSION_MAJOR_DEFAULT, DSHAL_API_VERSION_MINOR_DEFAULT);
        LOGINFO("GetHALVersion: SUCCESS - version=0x%x (%d.%d)", versionNo, 
               dsHAL_APIVER_MAJOR(versionNo), dsHAL_APIVER_MINOR(versionNo));

        return retCode;
    }

    uint32_t GetSoCID(string &socID) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetSoCID");

        // Use resolve function following dHdmiInImpl.h pattern
        typedef dsError_t (*dsGetSocIDFromSDKFunc_t)(char* socID);
        dsGetSocIDFromSDKFunc_t func = (dsGetSocIDFromSDKFunc_t)resolve(RDK_DSHAL_NAME, "dsGetSocIDFromSDK");

        if (func != nullptr) {
            char dsSocID[256] = {0};
            dsError_t eError = func(dsSocID);
            if (eError == dsERR_NONE) {
                socID = string(dsSocID);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetSoCID: SUCCESS - socID='%s'", socID.c_str());
            } else {
                LOGERR("GetSoCID: dsGetSocIDFromSDK failed with error: %d", eError);
            }
        } else {
            LOGERR("GetSoCID: Function not available");
        }

        return retCode;
    }

    uint32_t GetEDID(uint8_t edId[], const uint16_t edIdLength) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetEDID: edIdLength=%u", edIdLength);

        // Use resolve function following dHdmiInImpl.h pattern
        typedef dsError_t (*dsGetHostEDIDFunc_t)(unsigned char *edid, int *length);
        dsGetHostEDIDFunc_t func = (dsGetHostEDIDFunc_t)resolve(RDK_DSHAL_NAME, "dsGetHostEDID");

        if (func != nullptr) {
            unsigned char edidBytes[EDID_MAX_DATA_SIZE];
            int length = 0;
            dsError_t eError = func(edidBytes, &length);
            if (eError == dsERR_NONE && length <= static_cast<int>(edIdLength)) {
                memcpy(edId, edidBytes, length);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetEDID: SUCCESS - copied %d bytes", length);
            } else if (eError == dsERR_NONE && length > static_cast<int>(edIdLength)) {
                LOGERR("GetEDID: Buffer too small - required %d bytes, provided %u", length, edIdLength);
                retCode = WPEFramework::Core::ERROR_BAD_REQUEST;
            } else {
                LOGERR("GetEDID: dsGetHostEDID failed with error: %d", eError);
            }
        } else {
            retCode = WPEFramework::Core::ERROR_UNAVAILABLE;
            LOGERR("GetEDID: Function not available");
        }

        return retCode;
    }

    uint32_t GetMS12ConfigType(string &ms12Config) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetMS12ConfigType");
        
        // Following dsHost.cpp pattern
        try {
            ms12Config = device::HostPersistence::getInstance().getDefaultProperty("MS12.Config.Type");
            LOGINFO("GetMS12ConfigType: SUCCESS - ms12Config='%s'", ms12Config.c_str());
            retCode = WPEFramework::Core::ERROR_NONE;
        } catch (const std::exception& e) {
            LOGWARN("GetMS12ConfigType: Failed to retrieve config from default persistence: %s", e.what());
            ms12Config = "CONFIG_NONE";
            retCode = WPEFramework::Core::ERROR_NONE;
        } catch (...) {
            LOGWARN("GetMS12ConfigType: Unknown error retrieving config from default persistence");
            ms12Config = "CONFIG_NONE";
            retCode = WPEFramework::Core::ERROR_NONE;
        }
        
        return retCode;
    }

    // Host Event Handling Infrastructure - following VideoDevice singleton pattern
    void setAllCallbacks(const CallbackBundle& bundle) override
    {
        ENTRY_LOG;
        LOGINFO("Host::setAllCallbacks - Registering event callbacks with DS HAL");
        
        // Debug logging to diagnose condition failure
        LOGINFO("Host callback registration check: host_isInitialized=%d, host_isPlatInitialized=%d",
                host_isInitialized, host_isPlatInitialized);
        
        if (host_isPlatInitialized && !host_isInitialized) {
            LOGINFO("Host platform callback Initialization");
            
            // Register Sleep Mode Changed Callback
            if (bundle.OnSleepModeChanged) {
                LOGINFO("Host Sleep Mode Changed Event Callback Registered");
                g_HostSleepModeChangedCallback = bundle.OnSleepModeChanged;
                // Sleep mode callbacks are triggered manually during sleep mode setting
            }
            
            host_isInitialized = 1;
            LOGINFO("Host platform callback Initialization done");
        } else {
            if (!host_isPlatInitialized) {
                LOGERR("Host callback registration FAILED: Platform not initialized (host_isPlatInitialized=%d)",
                       host_isPlatInitialized);
            }
            if (host_isInitialized) {
                LOGWARN("Host callback registration SKIPPED: Callbacks already initialized (host_isInitialized=%d)",
                        host_isInitialized);
            }
        }
        
        EXIT_LOG;
    }

    void getPersistenceValue() override
    {
        ENTRY_LOG;
        LOGINFO("Host::getPersistenceValue - Loading persistence settings");
        
        try {
            std::string _SleepModeSettings("LIGHT_SLEEP");
            /* Get the Sleep Mode from Persistence */
            _SleepModeSettings = device::HostPersistence::getInstance().getProperty("Power.Mode", _SleepModeSettings);
            LOGINFO("Sleep mode Persistent value is -> %s", _SleepModeSettings.c_str());
            
            srv_SleepMode = stringToEnum(std::move(_SleepModeSettings));
            LOGINFO("Sleep mode set from persistence: %s (%d)", enumToString(srv_SleepMode).c_str(), static_cast<int>(srv_SleepMode));
            
            /* Get force disable HDR from Persistence (for completeness) */
            std::string _HDRSettings("true");
            _HDRSettings = device::HostPersistence::getInstance().getProperty("Host.forceHDRDisabled", _HDRSettings);
            LOGINFO("Host HDR disabled settings: %s", _HDRSettings.c_str());
            
        } catch (const std::exception& e) {
            LOGERR("Host::getPersistenceValue - Error loading persistence settings: %s", e.what());
        } catch (...) {
            LOGERR("Host::getPersistenceValue - Unknown error loading persistence settings");
        }
        
        EXIT_LOG;
    }

private:

    // Helper methods for DS Host HAL conversion  
    HostSleepMode convertDSSleepMode(dsSleepMode_t dsMode) {
        switch (dsMode) {
            case dsHOST_SLEEP_MODE_LIGHT: return HostSleepMode::DS_HOST_SLEEPMODE_LIGHT;
            case dsHOST_SLEEP_MODE_DEEP: return HostSleepMode::DS_HOST_SLEEPMODE_DEEP;
            default: return HostSleepMode::DS_HOST_SLEEPMODE_LIGHT;
        }
    }
    
    dsSleepMode_t convertHostSleepModeToDS(HostSleepMode mode) {
        switch (mode) {
            case HostSleepMode::DS_HOST_SLEEPMODE_LIGHT: return dsHOST_SLEEP_MODE_LIGHT;
            case HostSleepMode::DS_HOST_SLEEPMODE_DEEP: return dsHOST_SLEEP_MODE_DEEP;
            default: return dsHOST_SLEEP_MODE_LIGHT;
        }
    }
    
    // Helper functions for string conversion
    string enumToString(dsSleepMode_t mode) {
        string ret;
        switch (mode) {
            case dsHOST_SLEEP_MODE_LIGHT:
                ret = "LIGHT_SLEEP";
                break;
            case dsHOST_SLEEP_MODE_DEEP:
                ret = "DEEP_SLEEP";
                break;
            default:
                ret = "LIGHT_SLEEP";
        }
        return ret;
    }
    
    dsSleepMode_t stringToEnum(string mode) {
        if (mode == "LIGHT_SLEEP") {
            return dsHOST_SLEEP_MODE_LIGHT;
        } else if (mode == "DEEP_SLEEP") {
            return dsHOST_SLEEP_MODE_DEEP;
        }
        return dsHOST_SLEEP_MODE_LIGHT;
    }
    
    // Dynamic loading helper - following dHdmiInImpl.h pattern
    static void* resolve(const std::string& libName, const std::string& symbolName) {
        void* handle = dlopen(libName.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "dlopen failed for " << libName << ": " << dlerror() << std::endl;
            return nullptr;
        }
        void* symbol = dlsym(handle, symbolName.c_str());
        if (!symbol) {
            std::cerr << "dlsym failed for " << symbolName << ": " << dlerror() << std::endl;
        }
        dlclose(handle);
        return symbol;
    }
};