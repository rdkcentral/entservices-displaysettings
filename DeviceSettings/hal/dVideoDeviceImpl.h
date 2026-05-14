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
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <cstring>
#include <dlfcn.h>

#include "dVideoDevice.h"
#include "dsVideoDevice.h"
#include "dsError.h"
#include "dsUtl.h"
#include "dsTypes.h"
#include "dsRpc.h"
#include "dsHdmiIn.h"

#include "../helpers/UtilsSearchRDKProfile.h"

#include <WPEFramework/interfaces/IDeviceSettingsVideoDevice.h>
#include "DeviceSettingsTypes.h"

// Static global variables from dsVideoDevice.c conversion
static int videoDevice_isInitialized = 0;
static int videoDevice_isPlatInitialized = 0;
static dsVideoZoom_t srv_dfc = dsVIDEO_ZOOM_FULL;
static bool force_disable_hdr = true;

// Static global callbacks for VideoDevice events - following HdmiIn pattern
static std::function<void(const VideoDeviceZoom)> g_VideoDeviceZoomSettingsChangedCallback;
static std::function<void(const string)> g_VideoDeviceDisplayFrameratePreChangeCallback;
static std::function<void(const string)> g_VideoDeviceDisplayFrameratePostChangeCallback;

class dVideoDeviceImpl : public hal::dVideoDevice::IPlatform {

    // delete copy constructor and assignment operator
    dVideoDeviceImpl(const dVideoDeviceImpl&) = delete;
    dVideoDeviceImpl& operator=(const dVideoDeviceImpl&) = delete;

public:
    dVideoDeviceImpl()
    {
        LOGINFO("dVideoDeviceImpl Constructor");
        InitialiseHAL();
    }

    virtual ~dVideoDeviceImpl()
    {
        LOGERR("dVideoDeviceImpl Destructor");
        DeInitialiseHAL();
    }

    // Singleton getInstance method - following HdmiIn pattern
    static dVideoDeviceImpl*& getInstance()
    {
        static dVideoDeviceImpl* instance = new dVideoDeviceImpl();
        return instance;
    }

    void InitialiseHAL()
    {
        LOGINFO("InitialiseHAL");
        // Note: videoDevice_isInitialized should only be set in setAllCallbacks after callback registration
        // Don't set it here as it prevents callback registration condition from working

        if (!videoDevice_isPlatInitialized) {
            LOGINFO("InitialiseHAL <dsVideoDevice>");
            dsError_t eError = dsVideoDeviceInit();
            if (dsERR_NONE != eError) {
                LOGERR("InitialiseHAL: dsVideoDeviceInit failed with error: %d", eError);
                return;
            }
            LOGINFO("InitialiseHAL: dsVideoDeviceInit succeeded");
            
            // Load persistence values after successful initialization - following dsVideoDevice.c pattern
            getPersistenceValue();
            
            videoDevice_isPlatInitialized = 1;
            LOGINFO("InitialiseHAL completed: videoDevice_isPlatInitialized=%d, videoDevice_isInitialized=%d", 
                    videoDevice_isPlatInitialized, videoDevice_isInitialized);
        }
    }

    void DeInitialiseHAL()
    {
        LOGINFO("DeInitialiseHAL");
        if (videoDevice_isPlatInitialized)
        {
            dsVideoDeviceTerm();
            videoDevice_isPlatInitialized = 0;
        }
        videoDevice_isInitialized = 0;
    }

    static void* resolve(const std::string& libName, const std::string& symbolName) {
        void* handle = dlopen(libName.c_str(), RTLD_LAZY);
        if (!handle) {
            LOGERR("resolve: Failed to load library %s: %s", libName.c_str(), dlerror());
            return nullptr;
        }
        
        void* symbol = dlsym(handle, symbolName.c_str());
        if (!symbol) {
            LOGERR("resolve: Failed to find symbol %s in %s: %s", symbolName.c_str(), libName.c_str(), dlerror());
            dlclose(handle);
            return nullptr;
        }
        
        LOGINFO("resolve: Successfully resolved %s from %s", symbolName.c_str(), libName.c_str());
        dlclose(handle);
        return symbol;
    }

    // Implementation of all VideoDevice Platform interface methods
    uint32_t GetVideoDeviceHandle(const int32_t index, int32_t& handle) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetVideoDeviceHandle: index=%d", index);
        
        dsError_t eError = dsGetVideoDevice(index, &handle);
        if (eError == dsERR_NONE) {
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetVideoDeviceHandle: SUCCESS - handle=%d", handle);
        } else {
            LOGERR("GetVideoDeviceHandle: dsGetVideoDevice failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t SetVideoDeviceDFC(const int32_t handle, const VideoDeviceZoom zoomSetting) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetVideoDeviceDFC: handle=%d, zoomSetting=%d", handle, static_cast<int>(zoomSetting));
        
        // Convert VideoDeviceZoom to dsVideoZoom_t
        dsVideoZoom_t dsZoom = convertVideoDeviceZoom(zoomSetting);
        
        try {
            if (dsZoom == dsVIDEO_ZOOM_NONE) {
                LOGINFO("Call Zoom setting NONE");
                dsError_t eError = dsSetDFC(handle, dsZoom);
                if (eError == dsERR_NONE) {
                    srv_dfc = dsZoom;
                    retCode = WPEFramework::Core::ERROR_NONE;
                    device::HostPersistence::getInstance().persistHostProperty("VideoDevice.DFC", "None");
                    
                    // Trigger zoom settings changed callback
                    if (g_VideoDeviceZoomSettingsChangedCallback) {
                        g_VideoDeviceZoomSettingsChangedCallback(VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_NONE);
                    }
                    
                    LOGINFO("SetVideoDeviceDFC: SUCCESS (NONE)");
                } else {
                    LOGERR("SetVideoDeviceDFC: dsSetDFC failed with error: %d", eError);
                }
            } else if (dsZoom == dsVIDEO_ZOOM_FULL) {
                LOGINFO("Call Zoom setting FULL");
                dsError_t eError = dsSetDFC(handle, dsZoom);
                if (eError == dsERR_NONE) {
                    srv_dfc = dsZoom;
                    retCode = WPEFramework::Core::ERROR_NONE;
                    device::HostPersistence::getInstance().persistHostProperty("VideoDevice.DFC", "Full");
                    
                    // Trigger zoom settings changed callback
                    if (g_VideoDeviceZoomSettingsChangedCallback) {
                        g_VideoDeviceZoomSettingsChangedCallback(VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_FULL);
                    }
                    
                    LOGINFO("SetVideoDeviceDFC: SUCCESS (FULL)");
                } else {
                    LOGERR("SetVideoDeviceDFC: dsSetDFC failed with error: %d", eError);
                }
            } else if (dsZoom == dsVIDEO_ZOOM_16_9_ZOOM) {
                LOGINFO("Call Zoom setting dsVIDEO_ZOOM_16_9_ZOOM");
                dsError_t eError = dsSetDFC(handle, dsZoom);
                if (eError == dsERR_NONE) {
                    srv_dfc = dsZoom;
                    retCode = WPEFramework::Core::ERROR_NONE;
                    device::HostPersistence::getInstance().persistHostProperty("VideoDevice.DFC", "Full");
                    
                    // Trigger zoom settings changed callback
                    if (g_VideoDeviceZoomSettingsChangedCallback) {
                        g_VideoDeviceZoomSettingsChangedCallback(VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_16_9_ZOOM);
                    }
                    
                    LOGINFO("SetVideoDeviceDFC: SUCCESS (16_9_ZOOM)");
                } else {
                    LOGERR("SetVideoDeviceDFC: dsSetDFC failed with error: %d", eError);
                }
            } else {
                LOGERR("ERROR: unsupported Zoom setting %d", static_cast<int>(zoomSetting));
            }

            if (profileType == TV) {
                LOGINFO("TV Profile - setting HDMI In zoom mode");
                dsHdmiInSelectZoomMode(srv_dfc);
            }
        } catch (...) {
            LOGERR("Error in Setting the Video Device DFC");
        }
        
        return retCode;
    }

    uint32_t GetVideoDeviceDFC(const int32_t handle, VideoDeviceZoom& zoomSetting) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetVideoDeviceDFC: handle=%d", handle);
        
        // Return the cached zoom setting
        zoomSetting = convertDSVideoZoom(srv_dfc);
        retCode = WPEFramework::Core::ERROR_NONE;
        LOGINFO("GetVideoDeviceDFC: SUCCESS - zoomSetting=%d", static_cast<int>(zoomSetting));
        
        return retCode;
    }

    uint32_t GetHDRCapabilities(const int32_t handle, int32_t& capabilities) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetHDRCapabilities: handle=%d", handle);
        
        typedef dsError_t (*dsGetHDRCapabilitiesFunc_t)(intptr_t handle, int *capabilities);
        static dsGetHDRCapabilitiesFunc_t func = 0;
        if (func == 0) {
            func = (dsGetHDRCapabilitiesFunc_t)resolve(RDK_DSHAL_NAME, "dsGetHDRCapabilities");
            if (func) {
                LOGINFO("dsGetHDRCapabilities() is defined and loaded");
            } else {
                LOGINFO("dsGetHDRCapabilities() is not defined");
            }
        }
        
        if ((0 != func) && (false == force_disable_hdr)) {
            int dsCapabilities = 0;
            dsError_t eError = func(handle, &dsCapabilities);
            if (eError == dsERR_NONE) {
                capabilities = static_cast<int32_t>(dsCapabilities);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetHDRCapabilities: SUCCESS - capabilities=0x%x", capabilities);
            } else {
                LOGERR("GetHDRCapabilities: dsGetHDRCapabilities failed with error: %d", eError);
            }
        } else {
            capabilities = dsHDRSTANDARD_NONE;
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetHDRCapabilities: HDR disabled or function not available - capabilities=0x%x", capabilities);
        }
        
        return retCode;
    }

    uint32_t GetSupportedVideoCodingFormats(const int32_t handle, int32_t& supportedFormats) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetSupportedVideoCodingFormats: handle=%d", handle);
        
        typedef dsError_t (*dsGetSupportedVideoCodingFormatsFunc_t)(intptr_t handle, unsigned int *supported_formats);
        static dsGetSupportedVideoCodingFormatsFunc_t func = 0;
        if (func == 0) {
            func = (dsGetSupportedVideoCodingFormatsFunc_t)resolve(RDK_DSHAL_NAME, "dsGetSupportedVideoCodingFormats");
            if (func) {
                LOGINFO("dsGetSupportedVideoCodingFormats() is defined and loaded");
            } else {
                LOGINFO("dsGetSupportedVideoCodingFormats() is not defined");
            }
        }
        
        if (0 != func) {
            unsigned int dsFormats = 0;
            dsError_t eError = func(handle, &dsFormats);
            if (eError == dsERR_NONE) {
                supportedFormats = static_cast<int32_t>(dsFormats);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetSupportedVideoCodingFormats: SUCCESS - supportedFormats=0x%x", supportedFormats);
            } else {
                LOGERR("GetSupportedVideoCodingFormats: dsGetSupportedVideoCodingFormats failed with error: %d", eError);
            }
        } else {
            supportedFormats = 0x0; // Safe default: no formats supported
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetSupportedVideoCodingFormats: Function not available - supportedFormats=0x%x", supportedFormats);
        }
        
        return retCode;
    }

    uint32_t GetCodecInfo(const int32_t handle, const VideoDeviceCodec videoCodec, IDeviceSettingsVideoCodecProfileSupportIterator*& codecInfo) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetCodecInfo: handle=%d, videoCodec=%d", handle, static_cast<int>(videoCodec));
        
        typedef dsError_t (*dsGetVideoCodecInfoFunc_t)(intptr_t handle, dsVideoCodingFormat_t codec, dsVideoCodecInfo_t * info);
        static dsGetVideoCodecInfoFunc_t func = 0;
        if (func == 0) {
            func = (dsGetVideoCodecInfoFunc_t)resolve(RDK_DSHAL_NAME, "dsGetVideoCodecInfo");
            if (func) {
                LOGINFO("dsGetVideoCodecInfo() is defined and loaded");
            } else {
                LOGINFO("dsGetVideoCodecInfo() is not defined");
            }
        }
        
        if (0 != func) {
            dsVideoCodingFormat_t dsFormat = convertVideoCodecToDSFormat(videoCodec);
            dsVideoCodecInfo_t info;
            dsError_t eError = func(handle, dsFormat, &info);
            if (eError == dsERR_NONE) {
                // Convert dsVideoCodecInfo_t to iterator
                codecInfo = createCodecInfoIterator(info);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetCodecInfo: SUCCESS - codecInfo created");
            } else {
                LOGERR("GetCodecInfo: dsGetVideoCodecInfo failed with error: %d", eError);
            }
        } else {
            retCode = WPEFramework::Core::ERROR_UNAVAILABLE;
            LOGERR("GetCodecInfo: Function not available");
        }
        
        return retCode;
    }

    uint32_t DisableHDR(const int32_t handle, const bool disable) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("DisableHDR: handle=%d, disable=%s", handle, disable ? "true" : "false");
        
        typedef dsError_t (*dsDisableHDRSupportFunc_t)(intptr_t handle, bool enable);
        static dsDisableHDRSupportFunc_t func = 0;
        if (func == 0) {
            func = (dsDisableHDRSupportFunc_t)resolve(RDK_DSHAL_NAME, "dsForceDisableHDRSupport");
            if (func) {
                LOGINFO("dsForceDisableHDRSupport() is defined and loaded");
            } else {
                LOGINFO("dsForceDisableHDRSupport() is not defined");
            }
        }
        
        retCode = WPEFramework::Core::ERROR_NONE;
        
        if (0 != func) {
            dsError_t eError = func(handle, disable);
            if (eError != dsERR_NONE) {
                retCode = WPEFramework::Core::ERROR_GENERAL;
                LOGERR("DisableHDR: dsForceDisableHDRSupport failed with error: %d", eError);
            } else {
                LOGINFO("DisableHDR: dsForceDisableHDRSupport succeeded - disable=%s", disable ? "true" : "false");
            }
        }
        
        force_disable_hdr = disable;
        if (force_disable_hdr) {
            device::HostPersistence::getInstance().persistHostProperty("VideoDevice.forceHDRDisabled", "true");
        } else {
            device::HostPersistence::getInstance().persistHostProperty("VideoDevice.forceHDRDisabled", "false");
        }
        
        return retCode;
    }

    uint32_t SetFRFMode(const int32_t handle, const int32_t frfmode) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetFRFMode: handle=%d, frfmode=%d", handle, frfmode);
        
        typedef dsError_t (*dsSetFRFModeFunc_t)(intptr_t handle, int frfmode);
        static dsSetFRFModeFunc_t func = 0;
        if (func == 0) {
            func = (dsSetFRFModeFunc_t)resolve(RDK_DSHAL_NAME, "dsSetFRFMode");
            if (func) {
                LOGINFO("dsSetFRFMode is defined and loaded");
            } else {
                LOGINFO("dsSetFRFMode is not defined");
            }
        }
        
        if (0 != func) {
            dsError_t eError = func(handle, frfmode);
            if (eError == dsERR_NONE) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("SetFRFMode: SUCCESS");
            } else {
                LOGERR("SetFRFMode: dsSetFRFMode failed with error: %d", eError);
            }
        }
        
        return retCode;
    }

    uint32_t GetFRFMode(const int32_t handle, int32_t& frfmode) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetFRFMode: handle=%d", handle);
        
        typedef dsError_t (*dsGetFRFModeFunc_t)(intptr_t handle, int *frfmode);
        static dsGetFRFModeFunc_t func = 0;
        if (func == 0) {
            func = (dsGetFRFModeFunc_t)resolve(RDK_DSHAL_NAME, "dsGetFRFMode");
            if (func) {
                LOGINFO("dsGetFRFMode() is defined and loaded");
            } else {
                LOGINFO("dsGetFRFMode() is not defined");
            }
        }
        
        if (0 != func) {
            int dsFrfMode = 0;
            dsError_t eError = func(handle, &dsFrfMode);
            if (eError == dsERR_NONE) {
                frfmode = static_cast<int32_t>(dsFrfMode);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetFRFMode: SUCCESS - frfmode=%d", frfmode);
            } else {
                LOGERR("GetFRFMode: dsGetFRFMode failed with error: %d", eError);
            }
        }
        
        return retCode;
    }

    uint32_t GetCurrentDisplayFrameRate(const int32_t handle, string& framerate) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetCurrentDisplayFrameRate: handle=%d", handle);
        
        typedef dsError_t (*dsGetCurrentDisframerateFunc_t)(intptr_t handle, char *framerate);
        static dsGetCurrentDisframerateFunc_t func = 0;
        if (func == 0) {
            func = (dsGetCurrentDisframerateFunc_t)resolve(RDK_DSHAL_NAME, "dsGetCurrentDisplayframerate");
            if (func) {
                LOGINFO("dsGetCurrentDisframerate() is defined and loaded");
            } else {
                LOGINFO("dsGetCurrentDisframerate() is not defined");
            }
        }
        
        if (0 != func) {
            char dsFramerate[32] = "";
            dsError_t eError = func(handle, dsFramerate);
            if (eError == dsERR_NONE) {
                framerate = string(dsFramerate);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetCurrentDisplayFrameRate: SUCCESS - framerate=%s", framerate.c_str());
            } else {
                LOGERR("GetCurrentDisplayFrameRate: dsGetCurrentDisplayframerate failed with error: %d", eError);
            }
        }
        
        return retCode;
    }

    uint32_t SetDisplayFrameRate(const int32_t handle, const string framerate) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetDisplayFrameRate: handle=%d, framerate=%s", handle, framerate.c_str());
        
        typedef dsError_t (*dsSetDisplayframerateFunc_t)(intptr_t handle, char *frfmode);
        static dsSetDisplayframerateFunc_t func = 0;
        if (func == 0) {
            func = (dsSetDisplayframerateFunc_t)resolve(RDK_DSHAL_NAME, "dsSetDisplayframerate");
            if (func) {
                LOGINFO("dsSetDisplayframerate() is defined and loaded");
            } else {
                LOGINFO("dsSetDisplayframerate() is not defined");
            }
        }
        
        dsError_t result = dsERR_NONE;
        
        // Validate framerate parameter
        if (framerate.empty()) {
            result = dsERR_INVALID_PARAM;
            return WPEFramework::Core::ERROR_BAD_REQUEST;
        }
        
        // Send pre-change callback
        if (g_VideoDeviceDisplayFrameratePreChangeCallback) {
            g_VideoDeviceDisplayFrameratePreChangeCallback(framerate);
        }
        
        if (0 != func) {
            char dsFramerate[32];
            strncpy(dsFramerate, framerate.c_str(), sizeof(dsFramerate) - 1);
            dsFramerate[sizeof(dsFramerate) - 1] = '\0';
            
            result = func(handle, dsFramerate);
            if (result == dsERR_NONE) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("SetDisplayFrameRate: SUCCESS");
            } else {
                LOGERR("SetDisplayFrameRate: dsSetDisplayframerate failed with error: %d", result);
            }
        }
        
        // Send post-change callback
        if (g_VideoDeviceDisplayFrameratePostChangeCallback) {
            g_VideoDeviceDisplayFrameratePostChangeCallback(framerate);
        }
        
        return retCode;
    }

    // VideoDevice Event Handling Infrastructure - following HdmiIn singleton pattern
    void setAllCallbacks(const CallbackBundle& bundle) override
    {
        ENTRY_LOG;
        LOGINFO("VideoDevice::setAllCallbacks - Registering event callbacks with DS HAL");
        
        // Debug logging to diagnose condition failure
        LOGINFO("VideoDevice callback registration check: videoDevice_isInitialized=%d, videoDevice_isPlatInitialized=%d", 
                videoDevice_isInitialized, videoDevice_isPlatInitialized);
        
        if (videoDevice_isPlatInitialized && !videoDevice_isInitialized) {
            LOGINFO("VideoDevice platform callback Initialization");
            
            // Register Zoom Settings Changed Callback
            if (bundle.OnZoomSettingsChanged) {
                LOGINFO("VideoDevice Zoom Settings Changed Event Callback Registered");
                g_VideoDeviceZoomSettingsChangedCallback = bundle.OnZoomSettingsChanged;
                // Zoom callbacks are triggered manually during DFC setting
            }
            
            // Register Display Framerate Pre-Change Callback - following dsVideoDevice.c pattern
            if (bundle.OnDisplayFrameratePreChange) {
                LOGINFO("VideoDevice Display Framerate Pre-Change Event Callback Registered");
                g_VideoDeviceDisplayFrameratePreChangeCallback = bundle.OnDisplayFrameratePreChange;
                
                // Register framerate pre-change callback with DS HAL - exact pattern from dsVideoDevice.c
                dsError_t eRet = VideoDeviceRegisterFrameratePreChangeCB(VideoDeviceFramerateStatusPreChangeCB);
                if (dsERR_NONE != eRet) {
                    LOGERR("VideoDeviceRegisterFrameratePreChangeCB failed with error: %d", eRet);
                } else {
                    LOGINFO("Framerate pre-change callback registered successfully with DS HAL");
                }
            }
            
            // Register Display Framerate Post-Change Callback - following dsVideoDevice.c pattern
            if (bundle.OnDisplayFrameratePostChange) {
                LOGINFO("VideoDevice Display Framerate Post-Change Event Callback Registered");
                g_VideoDeviceDisplayFrameratePostChangeCallback = bundle.OnDisplayFrameratePostChange;
                
                // Register framerate post-change callback with DS HAL - exact pattern from dsVideoDevice.c
                dsError_t eRet = VideoDeviceRegisterFrameratePostChangeCB(VideoDeviceFramerateStatusPostChangeCB);
                if (dsERR_NONE != eRet) {
                    LOGERR("VideoDeviceRegisterFrameratePostChangeCB failed with error: %d", eRet);
                } else {
                    LOGINFO("Framerate post-change callback registered successfully with DS HAL");
                }
            }
            
            videoDevice_isInitialized = 1;
            LOGINFO("VideoDevice platform callback Initialization done");
        } else {
            if (!videoDevice_isPlatInitialized) {
                LOGERR("VideoDevice callback registration FAILED: Platform not initialized (videoDevice_isPlatInitialized=%d)", 
                       videoDevice_isPlatInitialized);
            }
            if (videoDevice_isInitialized) {
                LOGWARN("VideoDevice callback registration SKIPPED: Callbacks already initialized (videoDevice_isInitialized=%d)", 
                        videoDevice_isInitialized);
            }
        }
        
        EXIT_LOG;
    }

    void getPersistenceValue()
    {
        ENTRY_LOG;
        LOGINFO("VideoDevice::getPersistenceValue - Loading persistence settings");
        
        try {
            std::string _ZoomSettings("Full");
            /* Get the Zoom from Persistence */
            _ZoomSettings = device::HostPersistence::getInstance().getProperty("VideoDevice.DFC", _ZoomSettings);
            if (_ZoomSettings.compare("None") == 0) {
                srv_dfc = dsVIDEO_ZOOM_NONE;
            }
            LOGINFO("Persistent VideoDevice DFC read: %s", _ZoomSettings.c_str());
            
            if (profileType == TV) {
                LOGINFO("TV Profile - setting persistent zoom mode");
                dsHdmiInSelectZoomMode(srv_dfc);
            }
        } catch (...) {
            LOGINFO("Exception in Getting the Zoom settings on Startup");
        }
        
        try {
            std::string _hdr_setting("false");
            _hdr_setting = device::HostPersistence::getInstance().getProperty("VideoDevice.forceHDRDisabled", _hdr_setting);
            if (_hdr_setting.compare("false") == 0) {
                force_disable_hdr = false;
            } else {
                force_disable_hdr = true;
                LOGINFO("HDR support in disabled configuration");
            }
        } catch (...) {
            LOGINFO("Exception in getting force-disable-HDR setting at start up");
        }
        
        EXIT_LOG;
    }

    // Static callback functions for DS HAL integration - following HdmiIn pattern
    static void VideoDeviceFramerateStatusPreChangeCB(unsigned int inputStatus)
    {
        LOGINFO("VideoDeviceFramerateStatusPreChangeCB: inputStatus=%u", inputStatus);
        
        // Call the stored global callback if available
        if (g_VideoDeviceDisplayFrameratePreChangeCallback) {
            std::string framerate = std::to_string(inputStatus);
            g_VideoDeviceDisplayFrameratePreChangeCallback(framerate);
        }
    }

    static void VideoDeviceFramerateStatusPostChangeCB(unsigned int inputStatus)
    {
        LOGINFO("VideoDeviceFramerateStatusPostChangeCB: inputStatus=%u", inputStatus);
        
        // Call the stored global callback if available
        if (g_VideoDeviceDisplayFrameratePostChangeCallback) {
            std::string framerate = std::to_string(inputStatus);
            g_VideoDeviceDisplayFrameratePostChangeCallback(framerate);
        }
    }

    // DS HAL Callback Registration Functions - following exact pattern from dsVideoDevice.c
    static dsError_t VideoDeviceRegisterFrameratePreChangeCB(dsRegisterFrameratePreChangeCB_t cbFunc)
    {
        dsError_t eRet = dsERR_GENERAL;
        LOGINFO("VideoDeviceRegisterFrameratePreChangeCB: Registering framerate pre-change callback");
        
        typedef dsError_t (*_dsFramerateStatusPreChangeCB_t)(dsRegisterFrameratePreChangeCB_t CBFunc);
        static _dsFramerateStatusPreChangeCB_t frameratePreChangeCB = 0;
        
        if (frameratePreChangeCB == 0) {
            void* dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
            if (dllib) {
                frameratePreChangeCB = (_dsFramerateStatusPreChangeCB_t) dlsym(dllib, "dsRegisterFrameratePreChangeCB");
                if (frameratePreChangeCB == 0) {
                    LOGINFO("dsRegisterFrameratePreChangeCB is not defined");
                } else {
                    LOGINFO("dsRegisterFrameratePreChangeCB loaded");
                }
                dlclose(dllib);
            } else {
                LOGERR("Failed to open RDK_DSHAL_NAME [%s]: %s", RDK_DSHAL_NAME, dlerror());
                eRet = dsERR_GENERAL;
            }
        }
        
        if (frameratePreChangeCB) {
            eRet = frameratePreChangeCB(cbFunc);
            if (dsERR_NONE == eRet) {
                LOGINFO("Framerate pre-change callback registered successfully");
            } else {
                LOGERR("Failed to register framerate pre-change callback: %d", eRet);
            }
        }
        
        return eRet;
    }

    static dsError_t VideoDeviceRegisterFrameratePostChangeCB(dsRegisterFrameratePostChangeCB_t cbFunc)
    {
        dsError_t eRet = dsERR_GENERAL;
        LOGINFO("VideoDeviceRegisterFrameratePostChangeCB: Registering framerate post-change callback");
        
        typedef dsError_t (*_dsFramerateStatusPostChangeCB_t)(dsRegisterFrameratePostChangeCB_t CBFunc);
        static _dsFramerateStatusPostChangeCB_t frameratePostChangeCB = 0;
        
        if (frameratePostChangeCB == 0) {
            void* dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
            if (dllib) {
                frameratePostChangeCB = (_dsFramerateStatusPostChangeCB_t) dlsym(dllib, "dsRegisterFrameratePostChangeCB");
                if (frameratePostChangeCB == 0) {
                    LOGINFO("dsRegisterFrameratePostChangeCB is not defined");
                } else {
                    LOGINFO("dsRegisterFrameratePostChangeCB loaded");
                }
                dlclose(dllib);
            } else {
                LOGERR("Failed to open RDK_DSHAL_NAME [%s]: %s", RDK_DSHAL_NAME, dlerror());
                eRet = dsERR_GENERAL;
            }
        }
        
        if (frameratePostChangeCB) {
            eRet = frameratePostChangeCB(cbFunc);
            if (dsERR_NONE == eRet) {
                LOGINFO("Framerate post-change callback registered successfully");
            } else {
                LOGERR("Failed to register framerate post-change callback: %d", eRet);
            }
        }
        
        return eRet;
    }

private:
    
    // Helper methods for DS VideoDevice HAL conversion
    dsVideoZoom_t convertVideoDeviceZoom(const VideoDeviceZoom zoom)
    {
        switch (zoom) {
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_NONE:
                return dsVIDEO_ZOOM_NONE;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_FULL:
                return dsVIDEO_ZOOM_FULL;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_LB_16_9:
                return dsVIDEO_ZOOM_LB_16_9;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_LB_14_9:
                return dsVIDEO_ZOOM_LB_14_9;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_CCO:
                return dsVIDEO_ZOOM_CCO;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_PAN_SCAN:
                return dsVIDEO_ZOOM_PAN_SCAN;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_LB_2_21_1_ON_4_3:
                return dsVIDEO_ZOOM_LB_2_21_1_ON_4_3;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_LB_2_21_1_ON_16_9:
                return dsVIDEO_ZOOM_LB_2_21_1_ON_16_9;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_PLATFORM:
                return dsVIDEO_ZOOM_PLATFORM;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_16_9_ZOOM:
                return dsVIDEO_ZOOM_16_9_ZOOM;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_PILLARBOX_4_3:
                return dsVIDEO_ZOOM_PILLARBOX_4_3;
            case VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_WIDE_4_3:
                return dsVIDEO_ZOOM_WIDE_4_3;
            default:
                return dsVIDEO_ZOOM_FULL;
        }
    }

    VideoDeviceZoom convertDSVideoZoom(const dsVideoZoom_t dsZoom)
    {
        switch (dsZoom) {
            case dsVIDEO_ZOOM_NONE:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_NONE;
            case dsVIDEO_ZOOM_FULL:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_FULL;
            case dsVIDEO_ZOOM_LB_16_9:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_LB_16_9;
            case dsVIDEO_ZOOM_LB_14_9:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_LB_14_9;
            case dsVIDEO_ZOOM_CCO:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_CCO;
            case dsVIDEO_ZOOM_PAN_SCAN:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_PAN_SCAN;
            case dsVIDEO_ZOOM_LB_2_21_1_ON_4_3:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_LB_2_21_1_ON_4_3;
            case dsVIDEO_ZOOM_LB_2_21_1_ON_16_9:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_LB_2_21_1_ON_16_9;
            case dsVIDEO_ZOOM_PLATFORM:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_PLATFORM;
            case dsVIDEO_ZOOM_16_9_ZOOM:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_16_9_ZOOM;
            case dsVIDEO_ZOOM_PILLARBOX_4_3:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_PILLARBOX_4_3;
            case dsVIDEO_ZOOM_WIDE_4_3:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_WIDE_4_3;
            default:
                return VideoDeviceZoom::DS_VIDEO_DEVICE_ZOOM_FULL;
        }
    }

    dsVideoCodingFormat_t convertVideoCodecToDSFormat(const VideoDeviceCodec codec)
    {
        switch (codec) {
            case VideoDeviceCodec::DS_VIDEO_CODEC_MPEGHPART2:
                return dsVIDEO_CODEC_MPEGHPART2;
            case VideoDeviceCodec::DS_VIDEO_CODEC_MPEG4PART10:
                return dsVIDEO_CODEC_MPEG4PART10;
            case VideoDeviceCodec::DS_VIDEO_CODEC_MPEG2:
                return dsVIDEO_CODEC_MPEG2;
            default:
                return dsVIDEO_CODEC_MPEGHPART2;
        }
    }

    IDeviceSettingsVideoCodecProfileSupportIterator* createCodecInfoIterator(const dsVideoCodecInfo_t& info)
    {
        // This is a placeholder implementation
        // In a real implementation, this would create an iterator from the codec info
        LOGWARN("createCodecInfoIterator: Not implemented - returning nullptr");
        return nullptr;
    }
};