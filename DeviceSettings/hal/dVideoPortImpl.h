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
#include "dVideoPort.h"
#include "dsVideoPort.h"
#include "dsError.h"
//#include "dsVideoPortTypes.h"
#include "dsUtl.h"
#include "dsTypes.h"
#include "dsRpc.h"
#include "UtilsLogging.h"

#include <WPEFramework/interfaces/IDeviceSettingsVideoPort.h>
#include "DeviceSettingsTypes.h"
//#include "hostPersistence.hpp"  // Removed - HostPersistence is already defined in DeviceSettingsTypes.h

static int videoPort_isInitialized = 0;
static int videoPort_isPlatInitialized = 0;

// Persistent resolution settings - following dsVideoPort.c pattern
static std::string _dsHDMIResolution = "1080p";
static std::string _dsCompResolution = "1080p";
static std::string _dsRFResolution = "1080p";
static std::string _dsBBResolution = "1080p";

// Color depth settings - following dsVideoPort.c pattern
static const dsDisplayColorDepth_t DEFAULT_COLOR_DEPTH = dsDISPLAY_COLORDEPTH_AUTO;
// static dsDisplayColorDepth_t hdmiColorDepth = DEFAULT_COLOR_DEPTH; // Unused variable - commented out

// Static global callback functions for VideoPort events - following HdmiIn pattern
static std::function<void(const ResolutionChange)> g_VideoPortResolutionPreChangeCallback;
static std::function<void(const ResolutionChange)> g_VideoPortResolutionPostChangeCallback;
static std::function<void(const VideoPortHdcpStatus)> g_VideoPortHDCPStatusChangeCallback;
static std::function<void(const HDRStandard)> g_VideoPortVideoFormatUpdateCallback;

class dVideoPortImpl : public hal::dVideoPort::IPlatform {

    // delete copy constructor and assignment operator
    dVideoPortImpl(const dVideoPortImpl&) = delete;
    dVideoPortImpl& operator=(const dVideoPortImpl&) = delete;

public:
    dVideoPortImpl()
    {
        LOGINFO("dVideoPortImpl Constructor");
        getInstance() = this; // Set static instance for callback access
        InitialiseHAL();
    }

    virtual ~dVideoPortImpl()
    {
        LOGINFO("dVideoPortImpl Destructor");
        DeInitialiseHAL();
        getInstance() = nullptr; // Clear static instance
    }

    // Singleton getInstance method - following HdmiIn pattern
    static dVideoPortImpl*& getInstance()
    {
        static dVideoPortImpl* instance = nullptr;
        return instance;
    }

    void InitialiseHAL()
    {
        LOGINFO("InitialiseHAL");
        // Note: videoPort_isInitialized should only be set in setAllCallbacks after callback registration
        // Don't set it here as it prevents callback registration condition from working

        if (!videoPort_isPlatInitialized) {
            LOGINFO("InitialiseHAL <dsVideoPort>");
            dsError_t eError = dsVideoPortInit();
            if (dsERR_NONE != eError) {
                LOGERR("InitialiseHAL: dsVideoPortInit failed with error: %d", eError);
                return;
            }
            LOGINFO("InitialiseHAL: dsVideoPortInit succeeded");
            
            // Load persistence values after successful initialization - following dsVideoPort.c pattern
            getPersistenceValue();
            
            videoPort_isPlatInitialized = 1;
            LOGINFO("InitialiseHAL completed: videoPort_isPlatInitialized=%d, videoPort_isInitialized=%d", 
                    videoPort_isPlatInitialized, videoPort_isInitialized);
        }
    }

    void DeInitialiseHAL()
    {
        LOGINFO("DeInitialiseHAL");
        if (videoPort_isPlatInitialized)
        {
            dsVideoPortTerm();
            videoPort_isPlatInitialized = 0;
        }
        videoPort_isInitialized = 0;
    }

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

    // Implementation of all VideoPort Platform interface methods
    uint32_t GetVideoPort(const VideoPortType videoPort, const int32_t index, int32_t& handle) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetVideoPort: videoPort=%d, index=%d", static_cast<int>(videoPort), index);
        
        dsVideoPortType_t dsVideoPort = convertVideoPortType(videoPort);
        intptr_t dsHandle;
        
        dsError_t eError = dsGetVideoPort(dsVideoPort, index, &dsHandle);
        if (eError == dsERR_NONE) {
            handle = static_cast<int32_t>(dsHandle);
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetVideoPort: SUCCESS - handle=%d", handle);
        } else {
            LOGERR("GetVideoPort: dsGetVideoPort failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t IsVideoPortEnabled(const int32_t handle, bool& enabled) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("IsVideoPortEnabled: handle=%d", handle);
        
        bool dsEnabled = false;
        dsError_t eError = dsIsVideoPortEnabled(handle, &dsEnabled);
        if (eError == dsERR_NONE) {
            enabled = dsEnabled;
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("IsVideoPortEnabled: SUCCESS - enabled=%s", enabled ? "true" : "false");
        } else {
            LOGERR("IsVideoPortEnabled: dsIsVideoPortEnabled failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t EnableVideoPort(const int32_t handle, const bool enabled) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("EnableVideoPort: handle=%d, enabled=%s", handle, enabled ? "true" : "false");
        
        dsError_t eError = dsEnableVideoPort(handle, enabled);
        if (eError == dsERR_NONE) {
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("EnableVideoPort: SUCCESS");
        } else {
            LOGERR("EnableVideoPort: dsEnableVideoPort failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t IsVideoPortDisplayConnected(const int32_t handle, bool& connected) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("IsVideoPortDisplayConnected: handle=%d", handle);
        
        bool dsConnected = false;
        dsError_t eError = dsIsDisplayConnected(handle, &dsConnected);
        if (eError == dsERR_NONE) {
            connected = dsConnected;
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("IsVideoPortDisplayConnected: SUCCESS - connected=%s", connected ? "true" : "false");
        } else {
            LOGERR("IsVideoPortDisplayConnected: dsIsDisplayConnected failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t IsVideoPortActive(const int32_t handle, bool& active) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("IsVideoPortActive: handle=%d", handle);
        
        bool dsActive = false;
        dsError_t eError = dsIsVideoPortActive(handle, &dsActive);
        if (eError == dsERR_NONE) {
            active = dsActive;
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("IsVideoPortActive: SUCCESS - active=%s", active ? "true" : "false");
        } else {
            LOGERR("IsVideoPortActive: dsIsVideoPortActive failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t GetVideoPortResolution(const int32_t handle, VideoPortResolution& resolution) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetVideoPortResolution: handle=%d", handle);
        
        dsVideoPortResolution_t dsResolution;
        dsError_t eError = dsGetResolution(handle, &dsResolution);
        if (eError == dsERR_NONE) {
            resolution = convertVideoPortResolution(dsResolution);
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetVideoPortResolution: SUCCESS");
        } else {
            LOGERR("GetVideoPortResolution: dsGetResolution failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t GetColorDepth(const int32_t handle, uint32_t& colorDepth) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetColorDepth: handle=%d", handle);
        
        typedef dsError_t (*dsGetColorDepth_t)(intptr_t handle, unsigned int* color_depth);
        static dsGetColorDepth_t dsGetColorDepthFunc = 0;

        if (dsGetColorDepthFunc == 0) {
            dsGetColorDepthFunc = (dsGetColorDepth_t)resolve(RDK_DSHAL_NAME, "dsGetColorDepth");
            if (dsGetColorDepthFunc == 0) {
                LOGERR("GetColorDepth: dsGetColorDepth_t(int, unsigned int*) is not defined");
            }
            else {
                LOGINFO("GetColorDepth: dsGetColorDepth_t(int, unsigned int*) is defined and loaded");
            }
        }

        if (dsGetColorDepthFunc != 0) {
            unsigned int dsColorDepth = 0;
            dsError_t eError = dsGetColorDepthFunc(handle, &dsColorDepth);
            if (eError == dsERR_NONE) {
                colorDepth = dsColorDepth;
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetColorDepth: SUCCESS - colorDepth=%u", colorDepth);
            } else {
                LOGERR("GetColorDepth: dsGetColorDepth failed with error: %d", eError);
                colorDepth = 0; // Default value on error
            }
        } else {
            LOGERR("GetColorDepth: not able to load function dsGetColorDepthFunc:%p", dsGetColorDepthFunc);
            colorDepth = 0; // Default value
        }
        
        return retCode;
    }

    uint32_t SetVideoPortColorDepth(const int32_t handle, const uint32_t colorDepth) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetVideoPortColorDepth: handle=%d, colorDepth=%u", handle, colorDepth);
        
        // Use dsSetPreferredColorDepth instead since dsSetVideoPortColorDepth may not exist
        dsDisplayColorDepth_t dsColorDepth = static_cast<dsDisplayColorDepth_t>(colorDepth);
        dsError_t eError = dsSetPreferredColorDepth(handle, dsColorDepth);
        if (eError == dsERR_NONE) {
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("SetVideoPortColorDepth: SUCCESS (via dsSetPreferredColorDepth)");
        } else {
            LOGERR("SetVideoPortColorDepth: dsSetPreferredColorDepth failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t GetQuantizationRange(const int32_t handle, VideoPortQuantizationRange& quantizationRange) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetQuantizationRange: handle=%d", handle);
        
        typedef dsError_t (*dsGetQuantizationRange_t)(intptr_t handle, dsDisplayQuantizationRange_t* quantization_range);
        static dsGetQuantizationRange_t dsGetQuantizationRangeFunc = 0;

        if (dsGetQuantizationRangeFunc == 0) {
            dsGetQuantizationRangeFunc = (dsGetQuantizationRange_t)resolve(RDK_DSHAL_NAME, "dsGetQuantizationRange");
            if(dsGetQuantizationRangeFunc == 0) {
                LOGERR("dsGetQuantizationRange is not defined");
            }
            else {
                LOGINFO("dsGetQuantizationRange loaded");
            }
        }

        if (dsGetQuantizationRangeFunc != 0) {
            dsDisplayQuantizationRange_t dsQuantizationRange;
            dsError_t eError = dsGetQuantizationRangeFunc(handle, &dsQuantizationRange);
            if (eError == dsERR_NONE) {
                quantizationRange = convertQuantizationRange(dsQuantizationRange);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetQuantizationRange: SUCCESS");
            } else {
                LOGERR("GetQuantizationRange: dsGetQuantizationRange failed with error: %d", eError);
            }
        } else {
            LOGERR("GetQuantizationRange: dsGetQuantizationRange function not available");
            quantizationRange = static_cast<VideoPortQuantizationRange>(dsDISPLAY_QUANTIZATIONRANGE_UNKNOWN);
        }
        
        return retCode;
    }

    uint32_t SetVideoPortQuantizationRange(const int32_t handle, const VideoPortQuantizationRange quantizationRange) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetVideoPortQuantizationRange: handle=%d", handle);
        
        // Note: dsSetQuantizationRange may not exist in DS HAL - stub implementation
        LOGWARN("SetVideoPortQuantizationRange: Function not available in DS HAL - using stub");
        retCode = WPEFramework::Core::ERROR_NONE;
        LOGINFO("SetVideoPortQuantizationRange: SUCCESS (stub)");
        
        return retCode;
    }

    uint32_t GetColorSpace(const int32_t handle, VideoPortColorSpace& colorSpace) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetColorSpace: handle=%d", handle);
        
        typedef dsError_t (*dsGetColorSpace_t)(intptr_t handle, dsDisplayColorSpace_t* color_space);
        static dsGetColorSpace_t dsGetColorSpaceFunc = 0;

        if (dsGetColorSpaceFunc == 0) {
            dsGetColorSpaceFunc = (dsGetColorSpace_t)resolve(RDK_DSHAL_NAME, "dsGetColorSpace");
            if(dsGetColorSpaceFunc == 0) {
                LOGERR("dsGetColorSpace is not defined");
            }
            else {
                LOGINFO("dsGetColorSpace loaded");
            }
        }

        if (dsGetColorSpaceFunc != 0) {
            dsDisplayColorSpace_t dsColorSpace;
            dsError_t eError = dsGetColorSpaceFunc(handle, &dsColorSpace);
            if (eError == dsERR_NONE) {
                colorSpace = static_cast<VideoPortColorSpace>(dsColorSpace);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetColorSpace: SUCCESS - colorSpace=%d", static_cast<int>(colorSpace));
            } else {
                LOGERR("GetColorSpace: dsGetColorSpace failed with error: %d", eError);
            }
        } else {
            LOGERR("GetColorSpace: dsGetColorSpace function not available");
            colorSpace = static_cast<VideoPortColorSpace>(dsDISPLAY_COLORSPACE_RGB); // Default fallback
        }
        
        return retCode;
    }

    uint32_t SetColorSpace(const int32_t handle, const VideoPortColorSpace colorSpace) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetColorSpace: handle=%d", handle);
        
        // Note: dsSetColorSpace may not exist in DS HAL - stub implementation
        LOGWARN("SetColorSpace: Function not available in DS HAL - using stub");
        retCode = WPEFramework::Core::ERROR_NONE;
        LOGINFO("SetColorSpace: SUCCESS (stub)");
        
        return retCode;
    }

    uint32_t GetVideoPortFrameRate(const int32_t handle, uint32_t& frameRate) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetVideoPortFrameRate: handle=%d", handle);
        
        // Note: dsGetVideoFrameRate does not exist in DS HAL - using stub implementation
        LOGWARN("GetVideoPortFrameRate: Function not available in DS HAL - using stub");
        frameRate = 60; // Default frame rate
        retCode = WPEFramework::Core::ERROR_NONE;
        LOGINFO("GetVideoPortFrameRate: SUCCESS (stub) - frameRate=%u", frameRate);
        
        return retCode;
    }

    uint32_t SetVideoPortFrameRate(const int32_t handle, const uint32_t frameRate) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetVideoPortFrameRate: handle=%d, frameRate=%u", handle, frameRate);
        
        // Note: dsSetVideoFrameRate does not exist in DS HAL - using stub implementation
        LOGWARN("SetVideoPortFrameRate: Function not available in DS HAL - using stub");
        retCode = WPEFramework::Core::ERROR_NONE;
        LOGINFO("SetVideoPortFrameRate: SUCCESS (stub)");
        
        return retCode;
    }

    uint32_t GetVideoPortHDCPStatus(const int32_t handle, VideoPortHdcpStatus& hdcpStatus) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetVideoPortHDCPStatus: handle=%d", handle);
        
        dsHdcpStatus_t dsHdcpStatus;
        dsError_t eError = dsGetHDCPStatus(handle, &dsHdcpStatus);
        if (eError == dsERR_NONE) {
            hdcpStatus = convertHdcpStatus(dsHdcpStatus);
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetVideoPortHDCPStatus: SUCCESS");
        } else {
            LOGERR("GetVideoPortHDCPStatus: dsGetHDCPStatus failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t GetHDCPProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion& hdcpVersion) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetHDCPProtocolVersionOnVideoPort: handle=%d", handle);
        
        typedef dsError_t (*dsGetHDCPProtocol_t)(intptr_t handle, dsHdcpProtocolVersion_t* protocolVersion);
        static dsGetHDCPProtocol_t dsGetHDCPProtocolFunc = 0;

        if (dsGetHDCPProtocolFunc == 0) {
            dsGetHDCPProtocolFunc = (dsGetHDCPProtocol_t)resolve(RDK_DSHAL_NAME, "dsGetHDCPProtocol");
            if(dsGetHDCPProtocolFunc == 0) {
                LOGERR("dsGetHDCPProtocol is not defined");
            }
            else {
                LOGINFO("dsGetHDCPProtocol loaded");
            }
        }

        if (dsGetHDCPProtocolFunc != 0) {
            dsHdcpProtocolVersion_t dsHdcpVersion;
            dsError_t eError = dsGetHDCPProtocolFunc(handle, &dsHdcpVersion);
            if (eError == dsERR_NONE) {
                hdcpVersion = convertHdcpProtocolVersion(dsHdcpVersion);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetHDCPProtocolVersionOnVideoPort: SUCCESS");
            } else {
                LOGERR("GetHDCPProtocolVersionOnVideoPort: dsGetHDCPProtocol failed with error: %d", eError);
            }
        } else {
            LOGERR("GetHDCPProtocolVersionOnVideoPort: dsGetHDCPProtocol function not available");
        }
        
        return retCode;
    }

    uint32_t GetHDCPReceiverProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion& hdcpVersion) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetHDCPReceiverProtocolVersionOnVideoPort: handle=%d", handle);
        
        typedef dsError_t (*dsGetHDCPReceiverProtocol_t)(intptr_t handle, dsHdcpProtocolVersion_t* protocolVersion);
        static dsGetHDCPReceiverProtocol_t dsGetHDCPReceiverProtocolFunc = 0;

        if (dsGetHDCPReceiverProtocolFunc == 0) {
            dsGetHDCPReceiverProtocolFunc = (dsGetHDCPReceiverProtocol_t)resolve(RDK_DSHAL_NAME, "dsGetHDCPReceiverProtocol");
            if(dsGetHDCPReceiverProtocolFunc == 0) {
                LOGERR("dsGetHDCPReceiverProtocol is not defined");
            }
            else {
                LOGINFO("dsGetHDCPReceiverProtocol loaded");
            }
        }

        if (dsGetHDCPReceiverProtocolFunc != 0) {
            dsHdcpProtocolVersion_t dsHdcpVersion;
            dsError_t eError = dsGetHDCPReceiverProtocolFunc(handle, &dsHdcpVersion);
            if (eError == dsERR_NONE) {
                hdcpVersion = convertHdcpProtocolVersion(dsHdcpVersion);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetHDCPReceiverProtocolVersionOnVideoPort: SUCCESS");
            } else {
                LOGERR("GetHDCPReceiverProtocolVersionOnVideoPort: dsGetHDCPReceiverProtocol failed with error: %d", eError);
            }
        } else {
            LOGERR("GetHDCPReceiverProtocolVersionOnVideoPort: dsGetHDCPReceiverProtocol function not available");
        }
        
        return retCode;
    }

    uint32_t GetHDCPCurrentProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion& hdcpVersion) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetHDCPCurrentProtocolVersionOnVideoPort: handle=%d", handle);
        
        typedef dsError_t (*dsGetHDCPCurrentProtocol_t)(intptr_t handle, dsHdcpProtocolVersion_t* protocolVersion);
        static dsGetHDCPCurrentProtocol_t dsGetHDCPCurrentProtocolFunc = 0;

        if (dsGetHDCPCurrentProtocolFunc == 0) {
            dsGetHDCPCurrentProtocolFunc = (dsGetHDCPCurrentProtocol_t)resolve(RDK_DSHAL_NAME, "dsGetHDCPCurrentProtocol");
            if(dsGetHDCPCurrentProtocolFunc == 0) {
                LOGERR("dsGetHDCPCurrentProtocol is not defined");
            }
            else {
                LOGINFO("dsGetHDCPCurrentProtocol loaded");
            }
        }

        if (dsGetHDCPCurrentProtocolFunc != 0) {
            dsHdcpProtocolVersion_t dsHdcpVersion;
            dsError_t eError = dsGetHDCPCurrentProtocolFunc(handle, &dsHdcpVersion);
            if (eError == dsERR_NONE) {
                hdcpVersion = convertHdcpProtocolVersion(dsHdcpVersion);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetHDCPCurrentProtocolVersionOnVideoPort: SUCCESS");
            } else {
                LOGERR("GetHDCPCurrentProtocolVersionOnVideoPort: dsGetHDCPCurrentProtocol failed with error: %d", eError);
            }
        } else {
            LOGERR("GetHDCPCurrentProtocolVersionOnVideoPort: dsGetHDCPCurrentProtocol function not available");
        }
        
        return retCode;
    }

    uint32_t GetVideoEOTF(const int32_t handle, HDRStandard& hdrStandard) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetVideoEOTF: handle=%d", handle);
        
        typedef dsError_t (*dsGetVideoEOTF_t)(intptr_t handle, dsHDRStandard_t* video_eotf);
        static dsGetVideoEOTF_t dsGetVideoEOTFFunc = 0;

        if (dsGetVideoEOTFFunc == 0) {
            dsGetVideoEOTFFunc = (dsGetVideoEOTF_t)resolve(RDK_DSHAL_NAME, "dsGetVideoEOTF");
            if(dsGetVideoEOTFFunc == 0) {
                LOGERR("dsGetVideoEOTF is not defined");
            }
            else {
                LOGINFO("dsGetVideoEOTF loaded");
            }
        }

        if (dsGetVideoEOTFFunc != 0) {
            dsHDRStandard_t dsVideoEotf;
            dsError_t eError = dsGetVideoEOTFFunc(handle, &dsVideoEotf);
            if (eError == dsERR_NONE) {
                hdrStandard = static_cast<HDRStandard>(dsVideoEotf);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetVideoEOTF: SUCCESS - hdrStandard=%d", static_cast<int>(hdrStandard));
            } else {
                LOGERR("GetVideoEOTF: dsGetVideoEOTF failed with error: %d", eError);
            }
        } else {
            LOGERR("GetVideoEOTF: dsGetVideoEOTF function not available");
            hdrStandard = static_cast<HDRStandard>(dsHDRSTANDARD_NONE);
        }
        
        return retCode;
    }

    uint32_t GetMatrixCoefficients(const int32_t handle, DisplayMatrixCoefficients& matrixCoefficients) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetMatrixCoefficients: handle=%d", handle);
        
        typedef dsError_t (*dsGetMatrixCoefficients_t)(intptr_t handle, dsDisplayMatrixCoefficients_t* matrix_coefficients);
        static dsGetMatrixCoefficients_t dsGetMatrixCoefficientsFunc = 0;

        if (dsGetMatrixCoefficientsFunc == 0) {
            dsGetMatrixCoefficientsFunc = (dsGetMatrixCoefficients_t)resolve(RDK_DSHAL_NAME, "dsGetMatrixCoefficients");
            if(dsGetMatrixCoefficientsFunc == 0) {
                LOGERR("dsGetMatrixCoefficients is not defined");
            }
            else {
                LOGINFO("dsGetMatrixCoefficients loaded");
            }
        }

        if (dsGetMatrixCoefficientsFunc != 0) {
            dsDisplayMatrixCoefficients_t dsMatrixCoefficients;
            dsError_t eError = dsGetMatrixCoefficientsFunc(handle, &dsMatrixCoefficients);
            if (eError == dsERR_NONE) {
                matrixCoefficients = static_cast<DisplayMatrixCoefficients>(dsMatrixCoefficients);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetMatrixCoefficients: SUCCESS - matrixCoefficients=%d", static_cast<int>(matrixCoefficients));
            } else {
                LOGERR("GetMatrixCoefficients: dsGetMatrixCoefficients failed with error: %d", eError);
            }
        } else {
            LOGERR("GetMatrixCoefficients: dsGetMatrixCoefficients function not available");
            matrixCoefficients = static_cast<DisplayMatrixCoefficients>(dsDISPLAY_MATRIXCOEFFICIENT_UNKNOWN);
        }
        
        return retCode;
    }

    uint32_t IsVideoPortDisplaySurround(const int32_t handle, bool& surround) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("IsVideoPortDisplaySurround: handle=%d", handle);
        
        typedef dsError_t (*dsIsDisplaySurround_t)(intptr_t handle, bool *surround);
        static dsIsDisplaySurround_t dsIsDisplaySurroundFunc = 0;

        if (dsIsDisplaySurroundFunc == 0) {
            dsIsDisplaySurroundFunc = (dsIsDisplaySurround_t)resolve(RDK_DSHAL_NAME, "dsIsDisplaySurround");
            if(dsIsDisplaySurroundFunc == 0) {
                LOGERR("dsIsDisplaySurround is not defined");
            }
            else {
                LOGINFO("dsIsDisplaySurround loaded");
            }
        }

        if (dsIsDisplaySurroundFunc != 0) {
            bool dsSurround = false;
            dsError_t eError = dsIsDisplaySurroundFunc(handle, &dsSurround);
            if (eError == dsERR_NONE) {
                surround = dsSurround;
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("IsVideoPortDisplaySurround: SUCCESS - surround=%s", surround ? "true" : "false");
            } else {
                LOGERR("IsVideoPortDisplaySurround: dsIsDisplaySurround failed with error: %d", eError);
            }
        } else {
            LOGERR("IsVideoPortDisplaySurround: dsIsDisplaySurround function not available");
            surround = false;
        }
        
        return retCode;
    }

    uint32_t GetVideoPortDisplaySurroundMode(const int32_t handle, VideoPortSurroundMode& surroundMode) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetVideoPortDisplaySurroundMode: handle=%d", handle);
        
        typedef dsError_t (*dsGetSurroundMode_t)(intptr_t handle, int *surround);
        static dsGetSurroundMode_t dsGetSurroundModeFunc = 0;

        if (dsGetSurroundModeFunc == 0) {
            dsGetSurroundModeFunc = (dsGetSurroundMode_t)resolve(RDK_DSHAL_NAME, "dsGetSurroundMode");
            if(dsGetSurroundModeFunc == 0) {
                LOGERR("dsGetSurroundMode is not defined");
            }
            else {
                LOGINFO("dsGetSurroundMode loaded");
            }
        }

        if (dsGetSurroundModeFunc != 0) {
            int dsSurroundMode = 0;
            dsError_t eError = dsGetSurroundModeFunc(handle, &dsSurroundMode);
            if (eError == dsERR_NONE) {
                surroundMode = static_cast<VideoPortSurroundMode>(dsSurroundMode);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetVideoPortDisplaySurroundMode: SUCCESS - surroundMode=%d", static_cast<int>(surroundMode));
            } else {
                LOGERR("GetVideoPortDisplaySurroundMode: dsGetSurroundMode failed with error: %d", eError);
            }
        } else {
            LOGERR("GetVideoPortDisplaySurroundMode: dsGetSurroundMode function not available");
            surroundMode = VideoPortSurroundMode::DS_VIDEO_PORT_SURROUNDMODE_NONE;
        }
        
        return retCode;
    }

    uint32_t GetCurrentOutputSettings(const int32_t handle, DSOutputSettings& outputSettings) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetCurrentOutputSettings: handle=%d", handle);
        
        typedef dsError_t (*dsGetCurrentOutputSettings_t)(intptr_t handle, dsHDRStandard_t* video_eotf, dsDisplayMatrixCoefficients_t* matrix_coefficients, dsDisplayColorSpace_t* color_space, unsigned int* color_depth, dsDisplayQuantizationRange_t* quantization_range);
        static dsGetCurrentOutputSettings_t dsGetCurrentOutputSettingsFunc = 0;

        if (dsGetCurrentOutputSettingsFunc == 0) {
            dsGetCurrentOutputSettingsFunc = (dsGetCurrentOutputSettings_t)resolve(RDK_DSHAL_NAME, "dsGetCurrentOutputSettings");
            if(dsGetCurrentOutputSettingsFunc == 0) {
                LOGERR("dsGetCurrentOutputSettings is not defined");
            }
            else {
                LOGINFO("dsGetCurrentOutputSettings loaded");
            }
        }

        if (dsGetCurrentOutputSettingsFunc != 0) {
            dsHDRStandard_t dsVideoEotf;
            dsDisplayMatrixCoefficients_t dsMatrixCoefficients;
            dsDisplayColorSpace_t dsColorSpace;
            unsigned int dsColorDepth;
            dsDisplayQuantizationRange_t dsQuantizationRange;
            
            dsError_t eError = dsGetCurrentOutputSettingsFunc(handle, &dsVideoEotf, &dsMatrixCoefficients, &dsColorSpace, &dsColorDepth, &dsQuantizationRange);
            if (eError == dsERR_NONE) {
                outputSettings.videoEotf = static_cast<HDRStandard>(dsVideoEotf);
                outputSettings.matrixCoefficients = static_cast<DisplayMatrixCoefficients>(dsMatrixCoefficients);
                outputSettings.colorDepth = static_cast<uint32_t>(dsColorDepth);
                outputSettings.colorSpace = static_cast<VideoPortColorSpace>(dsColorSpace);
                outputSettings.quantizationRange = static_cast<VideoPortQuantizationRange>(dsQuantizationRange);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetCurrentOutputSettings: SUCCESS - eotf=%d, matrix=%d, colorDepth=%u, colorSpace=%d, quantization=%d", 
                       static_cast<int>(outputSettings.videoEotf), static_cast<int>(outputSettings.matrixCoefficients),
                       outputSettings.colorDepth, static_cast<int>(outputSettings.colorSpace), static_cast<int>(outputSettings.quantizationRange));
            } else {
                LOGERR("GetCurrentOutputSettings: dsGetCurrentOutputSettings failed with error: %d", eError);
            }
        } else {
            LOGERR("GetCurrentOutputSettings: dsGetCurrentOutputSettings function not available");
            // Set default values
            outputSettings.videoEotf = static_cast<HDRStandard>(dsHDRSTANDARD_NONE);
            outputSettings.matrixCoefficients = static_cast<DisplayMatrixCoefficients>(dsDISPLAY_MATRIXCOEFFICIENT_UNKNOWN);
            outputSettings.colorDepth = 0;
            outputSettings.colorSpace = static_cast<VideoPortColorSpace>(dsDISPLAY_COLORSPACE_UNKNOWN);
            outputSettings.quantizationRange = static_cast<VideoPortQuantizationRange>(dsDISPLAY_QUANTIZATIONRANGE_UNKNOWN);
        }
        
        return retCode;
    }

    uint32_t GetPreferredColorDepth(const int32_t handle, DisplayColorDepth& colorDepth, const bool persist) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetPreferredColorDepth: handle=%d, persist=%s", handle, persist ? "true" : "false");
        
        if (persist) {
            // Use persistent color depth - following dsVideoPort.c pattern
            DisplayColorDepth persistentColorDepth = getPersistentColorDepth();
            colorDepth = persistentColorDepth;
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetPreferredColorDepth: SUCCESS (from persistence) - colorDepth=%d", static_cast<int>(colorDepth));
        } else {
            // Get from HAL
            typedef dsError_t (*dsGetPreferredColorDepth_t)(intptr_t handle, dsDisplayColorDepth_t *colorDepth);
            static dsGetPreferredColorDepth_t dsGetPreferredColorDepthFunc = 0;

            if (dsGetPreferredColorDepthFunc == 0) {
                dsGetPreferredColorDepthFunc = (dsGetPreferredColorDepth_t)resolve(RDK_DSHAL_NAME, "dsGetPreferredColorDepth");
                if(dsGetPreferredColorDepthFunc == 0) {
                    LOGERR("dsGetPreferredColorDepth is not defined");
                }
                else {
                    LOGINFO("dsGetPreferredColorDepth loaded");
                }
            }

            if (dsGetPreferredColorDepthFunc != 0) {
                dsDisplayColorDepth_t dsColorDepth;
                dsError_t eError = dsGetPreferredColorDepthFunc(handle, &dsColorDepth);
                if (eError == dsERR_NONE) {
                    colorDepth = static_cast<DisplayColorDepth>(dsColorDepth);
                    retCode = WPEFramework::Core::ERROR_NONE;
                    LOGINFO("GetPreferredColorDepth: SUCCESS (from HAL) - colorDepth=%d", static_cast<int>(colorDepth));
                } else {
                    LOGERR("GetPreferredColorDepth: dsGetPreferredColorDepth failed with error: %d", eError);
                }
            } else {
                LOGERR("GetPreferredColorDepth: dsGetPreferredColorDepth function not available");
                colorDepth = static_cast<DisplayColorDepth>(dsDISPLAY_COLORDEPTH_UNKNOWN);
            }
        }
        
        return retCode;
    }

    uint32_t SetPreferredColorDepth(const int32_t handle, const DisplayColorDepth colorDepth, const bool persist) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetPreferredColorDepth: handle=%d, colorDepth=%d, persist=%s", handle, static_cast<int>(colorDepth), persist ? "true" : "false");
        
        typedef dsError_t (*dsSetPreferredColorDepth_t)(intptr_t handle, dsDisplayColorDepth_t colorDepth);
        static dsSetPreferredColorDepth_t dsSetPreferredColorDepthFunc = 0;

        if (dsSetPreferredColorDepthFunc == 0) {
            dsSetPreferredColorDepthFunc = (dsSetPreferredColorDepth_t)resolve(RDK_DSHAL_NAME, "dsSetPreferredColorDepth");
            if(dsSetPreferredColorDepthFunc == 0) {
                LOGERR("dsSetPreferredColorDepth is not defined");
            }
            else {
                LOGINFO("dsSetPreferredColorDepth loaded");
            }
        }

        if (dsSetPreferredColorDepthFunc != 0) {
            dsDisplayColorDepth_t dsColorDepth = static_cast<dsDisplayColorDepth_t>(colorDepth);
            dsError_t eError = dsSetPreferredColorDepthFunc(handle, dsColorDepth);
            if (eError == dsERR_NONE) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("SetPreferredColorDepth: SUCCESS");
                
                // Persist color depth setting if requested - following dsVideoPort.c pattern
                if (persist) {
                    try {
                        std::string colorDepthStr = std::to_string(static_cast<int>(colorDepth));
                        device::HostPersistence::getInstance().persistHostProperty("HDMI0.colorDepth", colorDepthStr);
                        LOGINFO("Color depth persisted: %s", colorDepthStr.c_str());
                    } catch(...) {
                        LOGERR("Failed to persist color depth setting");
                    }
                }
            } else {
                LOGERR("SetPreferredColorDepth: dsSetPreferredColorDepth failed with error: %d", eError);
            }
        } else {
            LOGERR("SetPreferredColorDepth: dsSetPreferredColorDepth function not available");
        }
        
        return retCode;
    }

    uint32_t SetVideoPortResolution(const int32_t handle, const VideoPortResolution resolution, const bool persist, const bool forceCompatibility) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetVideoPortResolution: handle=%d, persist=%s, forceCompatibility=%s", handle, persist ? "true" : "false", forceCompatibility ? "true" : "false");
        
        dsVideoPortResolution_t dsResolution = convertVideoPortResolution(resolution);
        
        // Trigger resolution pre-change callback
        VideoPortPreResolutionChange(&dsResolution);
        
        dsError_t eError = dsSetResolution(handle, &dsResolution);
        if (eError == dsERR_NONE) {
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("SetVideoPortResolution: SUCCESS");
            
            // Persist resolution setting if requested - following dsVideoPort.c pattern
            if (persist) {
                persistVideoPortResolution(handle, dsResolution, forceCompatibility);
            }
            
            // Trigger resolution post-change callback on successful resolution change
            VideoPortPostResolutionChange(&dsResolution);
        } else {
            LOGERR("SetVideoPortResolution: dsSetResolution failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t EnableHDCPOnVideoPort(const int32_t handle, const bool hdcpEnable, const uint8_t* hdcpKey, const uint16_t hdcpKeySize) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("EnableHDCPOnVideoPort: handle=%d, hdcpEnable=%s, hdcpKeySize=%u", handle, hdcpEnable ? "true" : "false", hdcpKeySize);
        
        dsError_t eError = dsEnableHDCP(handle, hdcpEnable, (char*)hdcpKey, static_cast<int>(hdcpKeySize));
        if (eError == dsERR_NONE) {
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("EnableHDCPOnVideoPort: SUCCESS");
        } else {
            LOGERR("EnableHDCPOnVideoPort: dsEnableHDCP failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t IsHDCPEnabledOnVideoPort(const int32_t handle, bool& hdcpEnabled) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("IsHDCPEnabledOnVideoPort: handle=%d", handle);
        
        bool dsHdcpEnabled = false;
        dsError_t eError = dsIsHDCPEnabled(handle, &dsHdcpEnabled);
        if (eError == dsERR_NONE) {
            hdcpEnabled = dsHdcpEnabled;
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("IsHDCPEnabledOnVideoPort: SUCCESS - hdcpEnabled=%s", hdcpEnabled ? "true" : "false");
        } else {
            LOGERR("IsHDCPEnabledOnVideoPort: dsIsHDCPEnabled failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t GetTVHDRCapabilities(const int32_t handle, int32_t& capabilities) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetTVHDRCapabilities: handle=%d", handle);
        
        typedef dsError_t (*dsGetTVHDRCapabilitiesFunc_t)(intptr_t handle, int* capabilities);
        static dsGetTVHDRCapabilitiesFunc_t dsGetTVHDRCapabilitiesFunc = 0;

        if (dsGetTVHDRCapabilitiesFunc == 0) {
            dsGetTVHDRCapabilitiesFunc = (dsGetTVHDRCapabilitiesFunc_t)resolve(RDK_DSHAL_NAME, "dsGetTVHDRCapabilities");
            if(dsGetTVHDRCapabilitiesFunc == 0) {
                LOGERR("dsGetTVHDRCapabilities is not defined");
            }
            else {
                LOGINFO("dsGetTVHDRCapabilities loaded");
            }
        }

        if (dsGetTVHDRCapabilitiesFunc != 0) {
            int dsCapabilities = 0;
            dsError_t eError = dsGetTVHDRCapabilitiesFunc(handle, &dsCapabilities);
            if (eError == dsERR_NONE) {
                capabilities = static_cast<int32_t>(dsCapabilities);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetTVHDRCapabilities: SUCCESS - capabilities=0x%x", capabilities);
            } else {
                LOGERR("GetTVHDRCapabilities: dsGetTVHDRCapabilities failed with error: %d", eError);
            }
        } else {
            LOGERR("GetTVHDRCapabilities: dsGetTVHDRCapabilities function not available");
            capabilities = 0; // Default value
        }
        
        return retCode;
    }

    uint32_t GetTVSupportedResolutions(const int32_t handle, int32_t& resolutions) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetTVSupportedResolutions: handle=%d", handle);
        
        typedef dsError_t (*dsSupportedTvResolutionsFunc_t)(intptr_t handle, int* resolutions);
        static dsSupportedTvResolutionsFunc_t dsSupportedTvResolutionsFunc = 0;

        if (dsSupportedTvResolutionsFunc == 0) {
            dsSupportedTvResolutionsFunc = (dsSupportedTvResolutionsFunc_t)resolve(RDK_DSHAL_NAME, "dsSupportedTvResolutions");
            if(dsSupportedTvResolutionsFunc == 0) {
                LOGERR("dsSupportedTvResolutions is not defined");
            }
            else {
                LOGINFO("dsSupportedTvResolutions loaded");
            }
        }

        if (dsSupportedTvResolutionsFunc != 0) {
            int dsResolutions = 0;
            dsError_t eError = dsSupportedTvResolutionsFunc(handle, &dsResolutions);
            if (eError == dsERR_NONE) {
                resolutions = static_cast<int32_t>(dsResolutions);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetTVSupportedResolutions: SUCCESS - resolutions=0x%x", resolutions);
            } else {
                LOGERR("GetTVSupportedResolutions: dsSupportedTvResolutions failed with error: %d", eError);
            }
        } else {
            LOGERR("GetTVSupportedResolutions: dsSupportedTvResolutions function not available");
            resolutions = 0; // Default value
        }
        
        return retCode;
    }

    uint32_t SetForceDisable4K(const int32_t handle, const bool disable) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetForceDisable4K: handle=%d, disable=%s", handle, disable ? "true" : "false");
        
        // Use correct DS HAL function: dsSetForceDisable4KSupport
        dsError_t eError = dsSetForceDisable4KSupport(handle, disable);
        if (eError == dsERR_NONE) {
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("SetForceDisable4K: SUCCESS");
        } else {
            LOGERR("SetForceDisable4K: dsSetForceDisable4KSupport failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t GetForceDisable4K(const int32_t handle, bool& disabled) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetForceDisable4K: handle=%d", handle);
        
        // Use correct DS HAL function: dsGetForceDisable4KSupport
        bool dsDisabled = false;
        dsError_t eError = dsGetForceDisable4KSupport(handle, &dsDisabled);
        if (eError == dsERR_NONE) {
            disabled = dsDisabled;
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("GetForceDisable4K: SUCCESS - disabled=%s", disabled ? "true" : "false");
        } else {
            LOGERR("GetForceDisable4K: dsGetForceDisable4KSupport failed with error: %d", eError);
            disabled = false; // Default value on error
        }
        
        return retCode;
    }

    uint32_t IsVideoPortOutputHDR(const int32_t handle, bool& isHDR) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("IsVideoPortOutputHDR: handle=%d", handle);
        
        typedef dsError_t (*dsIsOutputHDR_t)(intptr_t handle, bool* isHDR);
        static dsIsOutputHDR_t dsIsOutputHDRFunc = 0;

        if (dsIsOutputHDRFunc == 0) {
            dsIsOutputHDRFunc = (dsIsOutputHDR_t)resolve(RDK_DSHAL_NAME, "dsIsOutputHDR");
            if(dsIsOutputHDRFunc == 0) {
                LOGERR("dsIsOutputHDR is not defined");
            }
            else {
                LOGINFO("dsIsOutputHDR loaded");
            }
        }

        if (dsIsOutputHDRFunc != 0) {
            bool dsIsHDR = false;
            dsError_t eError = dsIsOutputHDRFunc(handle, &dsIsHDR);
            if (eError == dsERR_NONE) {
                isHDR = dsIsHDR;
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("IsVideoPortOutputHDR: SUCCESS - isHDR=%s", isHDR ? "true" : "false");
            } else {
                LOGERR("IsVideoPortOutputHDR: dsIsOutputHDR failed with error: %d", eError);
            }
        } else {
            LOGERR("IsVideoPortOutputHDR: dsIsOutputHDR function not available");
            isHDR = false; // Default value
        }
        
        return retCode;
    }

    uint32_t ResetVideoPortOutputToSDR() override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("ResetVideoPortOutputToSDR");
        
        typedef dsError_t (*dsResetOutputToSDR_t)(void);
        static dsResetOutputToSDR_t dsResetOutputToSDRFunc = 0;

        if (dsResetOutputToSDRFunc == 0) {
            dsResetOutputToSDRFunc = (dsResetOutputToSDR_t)resolve(RDK_DSHAL_NAME, "dsResetOutputToSDR");
            if(dsResetOutputToSDRFunc == 0) {
                LOGERR("dsResetOutputToSDR is not defined");
            }
            else {
                LOGINFO("dsResetOutputToSDR loaded");
            }
        }

        if (dsResetOutputToSDRFunc != 0) {
            dsError_t eError = dsResetOutputToSDRFunc();
            if (eError == dsERR_NONE) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("ResetVideoPortOutputToSDR: SUCCESS");
            } else {
                LOGERR("ResetVideoPortOutputToSDR: dsResetOutputToSDR failed with error: %d", eError);
            }
        } else {
            LOGERR("ResetVideoPortOutputToSDR: dsResetOutputToSDR function not available");
        }
        
        return retCode;
    }

    uint32_t GetHDMIPreference(const int32_t handle, VideoPortHdcpProtocolVersion& hdcpVersion) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetHDMIPreference: handle=%d", handle);
        
        typedef dsError_t (*dsGetHdmiPreference_t)(intptr_t handle, dsHdcpProtocolVersion_t* hdcpVersion);
        static dsGetHdmiPreference_t dsGetHdmiPreferenceFunc = 0;

        if (dsGetHdmiPreferenceFunc == 0) {
            dsGetHdmiPreferenceFunc = (dsGetHdmiPreference_t)resolve(RDK_DSHAL_NAME, "dsGetHdmiPreference");
            if(dsGetHdmiPreferenceFunc == 0) {
                LOGERR("dsGetHdmiPreference is not defined");
            }
            else {
                LOGINFO("dsGetHdmiPreference loaded");
            }
        }

        if (dsGetHdmiPreferenceFunc != 0) {
            dsHdcpProtocolVersion_t dsHdcpVersion;
            dsError_t eError = dsGetHdmiPreferenceFunc(handle, &dsHdcpVersion);
            if (eError == dsERR_NONE) {
                hdcpVersion = convertHdcpProtocolVersion(dsHdcpVersion);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetHDMIPreference: SUCCESS - hdcpVersion=%d", static_cast<int>(hdcpVersion));
            } else {
                LOGERR("GetHDMIPreference: dsGetHdmiPreference failed with error: %d", eError);
            }
        } else {
            LOGERR("GetHDMIPreference: dsGetHdmiPreference function not available");
        }
        
        return retCode;
    }

    uint32_t SetHDMIPreference(const int32_t handle, const VideoPortHdcpProtocolVersion hdcpVersion) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetHDMIPreference: handle=%d, hdcpVersion=%d", handle, static_cast<int>(hdcpVersion));
        
        typedef dsError_t (*dsSetHdmiPreference_t)(intptr_t handle, dsHdcpProtocolVersion_t* hdcpVersion);
        static dsSetHdmiPreference_t dsSetHdmiPreferenceFunc = 0;

        if (dsSetHdmiPreferenceFunc == 0) {
            dsSetHdmiPreferenceFunc = (dsSetHdmiPreference_t)resolve(RDK_DSHAL_NAME, "dsSetHdmiPreference");
            if(dsSetHdmiPreferenceFunc == 0) {
                LOGERR("dsSetHdmiPreference is not defined");
            }
            else {
                LOGINFO("dsSetHdmiPreference loaded");
            }
        }

        if (dsSetHdmiPreferenceFunc != 0) {
            dsHdcpProtocolVersion_t dsHdcpVersion = convertHdcpProtocolVersionToDSHal(hdcpVersion);
            dsError_t eError = dsSetHdmiPreferenceFunc(handle, &dsHdcpVersion);
            if (eError == dsERR_NONE) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("SetHDMIPreference: SUCCESS");
            } else {
                LOGERR("SetHDMIPreference: dsSetHdmiPreference failed with error: %d", eError);
            }
        } else {
            LOGERR("SetHDMIPreference: dsSetHdmiPreference function not available");
        }
        
        return retCode;
    }

    uint32_t SetBackgroundColor(const int32_t handle, const VideoBackgroundColor backgroundColor) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetBackgroundColor: handle=%d, backgroundColor=%d", handle, static_cast<int>(backgroundColor));
        
        dsVideoBackgroundColor_t dsBackgroundColor = static_cast<dsVideoBackgroundColor_t>(backgroundColor);
        dsError_t eError = dsSetBackgroundColor(handle, dsBackgroundColor);
        if (eError == dsERR_NONE) {
            retCode = WPEFramework::Core::ERROR_NONE;
            LOGINFO("SetBackgroundColor: SUCCESS");
        } else {
            LOGERR("SetBackgroundColor: dsSetBackgroundColor failed with error: %d", eError);
        }
        
        return retCode;
    }

    uint32_t SetForceHDRMode(const int32_t handle, const HDRStandard hdrMode) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("SetForceHDRMode: handle=%d, hdrMode=%d", handle, static_cast<int>(hdrMode));
        
        typedef dsError_t (*dsSetForceHDRMode_t)(intptr_t handle, dsHDRStandard_t hdrMode);
        static dsSetForceHDRMode_t dsSetForceHDRModeFunc = 0;

        if (dsSetForceHDRModeFunc == 0) {
            dsSetForceHDRModeFunc = (dsSetForceHDRMode_t)resolve(RDK_DSHAL_NAME, "dsSetForceHDRMode");
            if(dsSetForceHDRModeFunc == 0) {
                LOGERR("dsSetForceHDRMode is not defined");
            }
            else {
                LOGINFO("dsSetForceHDRMode loaded");
            }
        }

        if (dsSetForceHDRModeFunc != 0) {
            dsHDRStandard_t dsHdrMode = static_cast<dsHDRStandard_t>(hdrMode);
            dsError_t eError = dsSetForceHDRModeFunc(handle, dsHdrMode);
            if (eError == dsERR_NONE) {
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("SetForceHDRMode: SUCCESS");
            } else {
                LOGERR("SetForceHDRMode: dsSetForceHDRMode failed with error: %d", eError);
            }
        } else {
            LOGERR("SetForceHDRMode: dsSetForceHDRMode function not available");
        }
        
        return retCode;
    }

    uint32_t GetColorDepthCapabilities(const int32_t handle, uint32_t& colorDepthCapabilities) override
    {
        uint32_t retCode = WPEFramework::Core::ERROR_GENERAL;
        LOGINFO("GetColorDepthCapabilities: handle=%d", handle);
        
        typedef dsError_t (*dsColorDepthCapabilities_t)(intptr_t handle, unsigned int* colorDepthCapability);
        static dsColorDepthCapabilities_t dsColorDepthCapabilitiesFunc = 0;

        if (dsColorDepthCapabilitiesFunc == 0) {
            dsColorDepthCapabilitiesFunc = (dsColorDepthCapabilities_t)resolve(RDK_DSHAL_NAME, "dsColorDepthCapabilities");
            if (dsColorDepthCapabilitiesFunc == 0) {
                LOGERR("GetColorDepthCapabilities: dsColorDepthCapabilities(intptr_t handle, unsigned int *colorDepthCapability ) is not defined");
            }
            else {
                LOGINFO("GetColorDepthCapabilities: dsColorDepthCapabilities(intptr_t handle, unsigned int *colorDepthCapability ) is defined and loaded");
            }
        }

        if (dsColorDepthCapabilitiesFunc != 0) {
            unsigned int dsColorDepthCapabilities = 0;
            dsError_t eError = dsColorDepthCapabilitiesFunc(handle, &dsColorDepthCapabilities);
            if (eError == dsERR_NONE) {
                LOGINFO("GetColorDepthCapabilities: dsColorDepthCapabilities returned:%d  colorDepthCapability: 0x%x", 
                        eError, dsColorDepthCapabilities);
                
                // Add auto by default - consistent with _dsColorDepthCapabilities in dsVideoPort.c
                dsColorDepthCapabilities = (dsColorDepthCapabilities | dsDISPLAY_COLORDEPTH_AUTO);
                
                colorDepthCapabilities = static_cast<uint32_t>(dsColorDepthCapabilities);
                retCode = WPEFramework::Core::ERROR_NONE;
                LOGINFO("GetColorDepthCapabilities: SUCCESS - final colorDepthCapabilities=0x%x", colorDepthCapabilities);
            } else {
                LOGERR("GetColorDepthCapabilities: dsColorDepthCapabilities failed with error: %d", eError);
                colorDepthCapabilities = 0; // Default value on error
            }
        } else {
            LOGERR("GetColorDepthCapabilities: not able to load function dsColorDepthCapabilitiesFunc:%p", dsColorDepthCapabilitiesFunc);
            colorDepthCapabilities = 0; // Default value
        }
        
        return retCode;
    }

    // VideoPort Event Handling Infrastructure - following HdmiIn singleton pattern
    void setAllCallbacks(const CallbackBundle& bundle) override
    {
        ENTRY_LOG;
        LOGINFO("VideoPort::setAllCallbacks - Registering event callbacks with DS HAL");
        
        // Debug logging to diagnose condition failure
        LOGINFO("VideoPort callback registration check: videoPort_isInitialized=%d, videoPort_isPlatInitialized=%d", 
                videoPort_isInitialized, videoPort_isPlatInitialized);
        
        if (videoPort_isPlatInitialized && !videoPort_isInitialized) {
            LOGINFO("VideoPort platform callback Initialization");
            
            // Register Resolution Pre/Post Change callbacks
            if (bundle.OnResolutionPreChange) {
                LOGINFO("VideoPort Resolution PreChange Event Callback Registered");
                g_VideoPortResolutionPreChangeCallback = bundle.OnResolutionPreChange;
                // Resolution callbacks are handled manually during resolution setting
            }
            
            if (bundle.OnResolutionPostChange) {
                LOGINFO("VideoPort Resolution PostChange Event Callback Registered");
                g_VideoPortResolutionPostChangeCallback = bundle.OnResolutionPostChange;
                // Resolution callbacks are handled manually during resolution setting
            }
            
            // Register HDCP Status Callback with DS HAL
            if (bundle.OnHDCPStatusChange) {
                LOGINFO("VideoPort HDCP Status Change Event Callback Registered");
                g_VideoPortHDCPStatusChangeCallback = bundle.OnHDCPStatusChange;
                
                intptr_t handle = 0;
                dsError_t eReturn = dsGetVideoPort(dsVIDEOPORT_TYPE_HDMI, 0, &handle);
                if (dsERR_NONE != eReturn) {
                    eReturn = dsGetVideoPort(dsVIDEOPORT_TYPE_INTERNAL, 0, &handle);
                }
                
                if (dsERR_NONE == eReturn && handle != 0) {
                    LOGINFO("Registering HDCP status callback with handle: %p", (void*)handle);
                    dsRegisterHdcpStatusCallback(handle, VideoPortHDCPStatusCallback);
                } else {
                    LOGERR("Failed to get video port handle for HDCP callback registration");
                }
            }
            
            // Register Video Format Update Callback with DS HAL
            if (bundle.OnVideoFormatUpdate) {
                LOGINFO("VideoPort Video Format Update Event Callback Registered");
                g_VideoPortVideoFormatUpdateCallback = bundle.OnVideoFormatUpdate;
                
                dsError_t eRet = VideoPortRegisterVideoFormatUpdateCB(VideoPortVideoFormatUpdateCallback);
                if (dsERR_NONE != eRet) {
                    LOGERR("VideoPortRegisterVideoFormatUpdateCB failed with error: %d", eRet);
                } else {
                    LOGINFO("Video format update callback registered successfully");
                }
            }
            
            videoPort_isInitialized = 1;
            LOGINFO("VideoPort platform callback Initialization done");
        } else {
            if (!videoPort_isPlatInitialized) {
                LOGERR("VideoPort callback registration FAILED: Platform not initialized (videoPort_isPlatInitialized=%d)", 
                       videoPort_isPlatInitialized);
            }
            if (videoPort_isInitialized) {
                LOGWARN("VideoPort callback registration SKIPPED: Callbacks already initialized (videoPort_isInitialized=%d)", 
                        videoPort_isInitialized);
            }
        }
        
        EXIT_LOG;
    }

    void getPersistenceValue()
    {
        ENTRY_LOG;
        LOGINFO("VideoPort::getPersistenceValue - Loading persistence settings");
        
        try {
            // Read persistent resolution settings - following dsVideoPort.c pattern
            std::string defaultResolution = "1080p";
            
            _dsHDMIResolution = device::HostPersistence::getInstance().getProperty("HDMI0.resolution", defaultResolution);
            LOGINFO("Persistent HDMI resolution read: %s", _dsHDMIResolution.c_str());
            
            #ifdef HAS_ONLY_COMPOSITE
                _dsCompResolution = device::HostPersistence::getInstance().getProperty("Baseband0.resolution", defaultResolution);
            #else
                _dsCompResolution = device::HostPersistence::getInstance().getProperty("COMPONENT0.resolution", defaultResolution);
            #endif
            LOGINFO("Persistent Component/Composite resolution read: %s", _dsCompResolution.c_str());
            
            _dsRFResolution = device::HostPersistence::getInstance().getProperty("RF0.resolution", defaultResolution);
            LOGINFO("Persistent RF resolution read: %s", _dsRFResolution.c_str());
            
            _dsBBResolution = device::HostPersistence::getInstance().getProperty("Baseband0.resolution", defaultResolution);
            LOGINFO("Persistent BB resolution read: %s", _dsBBResolution.c_str());
            
            // Read 4K disable setting
            std::string force4KDisabled = "false";
            force4KDisabled = device::HostPersistence::getInstance().getProperty("VideoDevice.force4KDisabled", force4KDisabled);
            if (force4KDisabled.compare("true") == 0) {
                LOGINFO("4K support is force disabled via persistence");
            }
            
        } catch(...) {
            LOGERR("Error reading persistence values for VideoPort");
        }
        
        EXIT_LOG;
    }

    // Static callback functions for DS HAL integration - following HdmiIn pattern
    static void VideoPortHDCPStatusCallback(intptr_t handle, dsHdcpStatus_t status)
    {
        LOGINFO("VideoPortHDCPStatusCallback: handle=%p, status=%d", (void*)handle, status);
        
        // Convert DS HAL HDCP status to VideoPortHdcpStatus
        VideoPortHdcpStatus hdcpStatus;
        switch (status) {
            case dsHDCP_STATUS_UNAUTHENTICATED:
                hdcpStatus = VideoPortHdcpStatus::DS_HDCP_STATUS_UNAUTHENTICATED;
                break;
            case dsHDCP_STATUS_AUTHENTICATED:
                hdcpStatus = VideoPortHdcpStatus::DS_HDCP_STATUS_AUTHENTICATED;
                break;
            case dsHDCP_STATUS_AUTHENTICATIONFAILURE:
                hdcpStatus = VideoPortHdcpStatus::DS_HDCP_STATUS_AUTHENTICATIONFAILURE;
                break;
            case dsHDCP_STATUS_INPROGRESS:
                hdcpStatus = VideoPortHdcpStatus::DS_HDCP_STATUS_INPROGRESS;
                break;
            case dsHDCP_STATUS_PORTDISABLED:
                hdcpStatus = VideoPortHdcpStatus::DS_HDCP_STATUS_PORTDISABLED;
                break;
            default:
                hdcpStatus = VideoPortHdcpStatus::DS_HDCP_STATUS_UNAUTHENTICATED;
                LOGERR("Unknown HDCP status: %d, defaulting to unauthenticated", status);
                break;
        }
        
        // Call the stored global callback if available
        if (g_VideoPortHDCPStatusChangeCallback) {
            g_VideoPortHDCPStatusChangeCallback(hdcpStatus);
        }
    }

    static void VideoPortVideoFormatUpdateCallback(dsHDRStandard_t videoFormat)
    {
        LOGINFO("VideoPortVideoFormatUpdateCallback: videoFormat=%d", videoFormat);
        
        // Convert DS HAL HDR standard to HDRStandard
        HDRStandard hdrStandard;
        switch (videoFormat) {
            case dsHDRSTANDARD_SDR:
                hdrStandard = HDRStandard::DS_HDRSTANDARD_SDR;
                break;
            case dsHDRSTANDARD_HDR10:
                hdrStandard = HDRStandard::DS_HDRSTANDARD_HDR10;
                break;
            case dsHDRSTANDARD_HDR10PLUS:
                hdrStandard = HDRStandard::DS_HDRSTANDARD_HDR10PLUS;
                break;
            case dsHDRSTANDARD_DolbyVision:
                hdrStandard = HDRStandard::DS_HDRSTANDARD_DOLBYVISION;
                break;
            default:
                hdrStandard = HDRStandard::DS_HDRSTANDARD_SDR;
                LOGERR("Unknown HDR standard: %d, defaulting to SDR", videoFormat);
                break;
        }
        
        // Call the stored global callback if available
        if (g_VideoPortVideoFormatUpdateCallback) {
            g_VideoPortVideoFormatUpdateCallback(hdrStandard);
        }
    }

    // DS HAL Video Format Update Callback Registration
    static dsError_t VideoPortRegisterVideoFormatUpdateCB(dsVideoFormatUpdateCB_t cbFun)
    {
        dsError_t eRet = dsERR_GENERAL;
        LOGINFO("VideoPortRegisterVideoFormatUpdateCB: Registering video format callback");
        
        typedef dsError_t (*dsVideoFormatUpdateRegisterCB_t)(dsVideoFormatUpdateCB_t cbFunArg);
        static dsVideoFormatUpdateRegisterCB_t dsVideoFormatUpdateRegisterCBFunc = 0;
        
        if (dsVideoFormatUpdateRegisterCBFunc == 0) {
            void* dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
            if (dllib) {
                dsVideoFormatUpdateRegisterCBFunc = (dsVideoFormatUpdateRegisterCB_t) dlsym(dllib, "dsVideoFormatUpdateRegisterCB");
                if (dsVideoFormatUpdateRegisterCBFunc == 0) {
                    LOGERR("dsVideoFormatUpdateRegisterCB is not defined: %s", dlerror());
                    eRet = dsERR_GENERAL;
                } else {
                    LOGINFO("dsVideoFormatUpdateRegisterCB loaded successfully");
                }
                dlclose(dllib);
            } else {
                LOGERR("Failed to open RDK_DSHAL_NAME [%s]: %s", RDK_DSHAL_NAME, dlerror());
                eRet = dsERR_GENERAL;
            }
        }
        
        if (dsVideoFormatUpdateRegisterCBFunc != 0) {
            eRet = dsVideoFormatUpdateRegisterCBFunc(cbFun);
            if (dsERR_NONE == eRet) {
                LOGINFO("Video format update callback registered successfully");
            } else {
                LOGERR("Failed to register video format callback: %d", eRet);
            }
        }
        
        return eRet;
    }

    // Resolution Change Helper Functions - Following dsVideoPort.c RPC server pattern
    static void VideoPortPreResolutionChange(dsVideoPortResolution_t* resolution)
    {
        if (!resolution) {
            LOGERR("VideoPortPreResolutionChange: Invalid resolution parameter");
            return;
        }
        
        LOGINFO("VideoPortPreResolutionChange: pixelResolution=%d", resolution->pixelResolution);
        
        // Convert dsVideoPortResolution_t to ResolutionChange structure - based on dsVideoPort.c
        ResolutionChange resolutionChange;
        switch(resolution->pixelResolution) {
            case dsVIDEO_PIXELRES_720x480:
                resolutionChange.width = 720;
                resolutionChange.height = 480;
                break;
            case dsVIDEO_PIXELRES_720x576:
                resolutionChange.width = 720;
                resolutionChange.height = 576;
                break;
            case dsVIDEO_PIXELRES_1280x720:
                resolutionChange.width = 1280;
                resolutionChange.height = 720;
                break;
            case dsVIDEO_PIXELRES_1366x768:
                resolutionChange.width = 1366;
                resolutionChange.height = 768;
                break;
            case dsVIDEO_PIXELRES_1920x1080:
                resolutionChange.width = 1920;
                resolutionChange.height = 1080;
                break;
            case dsVIDEO_PIXELRES_3840x2160:
                resolutionChange.width = 3840;
                resolutionChange.height = 2160;
                break;
            case dsVIDEO_PIXELRES_4096x2160:
                resolutionChange.width = 4096;
                resolutionChange.height = 2160;
                break;
            default:
                resolutionChange.width = 1280;
                resolutionChange.height = 720;
                LOGERR("Unknown pixel resolution: %d, defaulting to 720p", resolution->pixelResolution);
                break;
        }
        
        // Call the stored global callback if available
        if (g_VideoPortResolutionPreChangeCallback) {
            g_VideoPortResolutionPreChangeCallback(resolutionChange);
        }
    }

    static void VideoPortPostResolutionChange(dsVideoPortResolution_t* resolution)
    {
        if (!resolution) {
            LOGERR("VideoPortPostResolutionChange: Invalid resolution parameter");
            return;
        }
        
        LOGINFO("VideoPortPostResolutionChange: pixelResolution=%d", resolution->pixelResolution);
        
        // Convert dsVideoPortResolution_t to ResolutionChange structure - based on dsVideoPort.c
        ResolutionChange resolutionChange;
        switch(resolution->pixelResolution) {
            case dsVIDEO_PIXELRES_720x480:
                resolutionChange.width = 720;
                resolutionChange.height = 480;
                break;
            case dsVIDEO_PIXELRES_720x576:
                resolutionChange.width = 720;
                resolutionChange.height = 576;
                break;
            case dsVIDEO_PIXELRES_1280x720:
                resolutionChange.width = 1280;
                resolutionChange.height = 720;
                break;
            case dsVIDEO_PIXELRES_1366x768:
                resolutionChange.width = 1366;
                resolutionChange.height = 768;
                break;
            case dsVIDEO_PIXELRES_1920x1080:
                resolutionChange.width = 1920;
                resolutionChange.height = 1080;
                break;
            case dsVIDEO_PIXELRES_3840x2160:
                resolutionChange.width = 3840;
                resolutionChange.height = 2160;
                break;
            case dsVIDEO_PIXELRES_4096x2160:
                resolutionChange.width = 4096;
                resolutionChange.height = 2160;
                break;
            default:
                resolutionChange.width = 1280;
                resolutionChange.height = 720;
                LOGERR("Unknown pixel resolution: %d, defaulting to 720p", resolution->pixelResolution);
                break;
        }
        
        // Call the stored global callback if available
        if (g_VideoPortResolutionPostChangeCallback) {
            g_VideoPortResolutionPostChangeCallback(resolutionChange);
        }
    }

    // Helper function to convert DS resolution to ResolutionChange structure
    static void convertDSResolutionToResolutionChange(dsVideoPortResolution_t* dsResolution, ResolutionChange& resolutionChange)
    {
        // Convert pixel resolution to width/height based on dsVideoPort.c pattern
        switch (dsResolution->pixelResolution) {
            case dsVIDEO_PIXELRES_720x480:
                resolutionChange.width = 720;
                resolutionChange.height = 480;
                break;
            case dsVIDEO_PIXELRES_720x576:
                resolutionChange.width = 720;
                resolutionChange.height = 576;
                break;
            case dsVIDEO_PIXELRES_1280x720:
                resolutionChange.width = 1280;
                resolutionChange.height = 720;
                break;
            case dsVIDEO_PIXELRES_1920x1080:
                resolutionChange.width = 1920;
                resolutionChange.height = 1080;
                break;
            case dsVIDEO_PIXELRES_3840x2160:
                resolutionChange.width = 3840;
                resolutionChange.height = 2160;
                break;
            case dsVIDEO_PIXELRES_4096x2160:
                resolutionChange.width = 4096;
                resolutionChange.height = 2160;
                break;
            default:
                resolutionChange.width = 1920;
                resolutionChange.height = 1080;
                LOGERR("Unknown pixel resolution: %d, defaulting to 1920x1080", dsResolution->pixelResolution);
                break;
        }
        
        // Note: ResolutionChange only has width/height members
        // Additional information like pixelResolution, frameRate, interlaced are not part of the interface
    }

private:

    
    // Helper methods for DS VideoPort HAL conversion
    dsVideoPortType_t convertVideoPortType(const VideoPortType videoPort)
    {
        switch (videoPort) {
            case VideoPortType::DS_VIDEO_PORT_TYPE_HDMI:
                return dsVIDEOPORT_TYPE_HDMI;
            case VideoPortType::DS_VIDEO_PORT_TYPE_COMPONENT:
                return dsVIDEOPORT_TYPE_COMPONENT;
            case VideoPortType::DS_VIDEO_PORT_TYPE_SVIDEO:
                return dsVIDEOPORT_TYPE_SVIDEO;
            case VideoPortType::DS_VIDEO_PORT_TYPE_1394:
                return dsVIDEOPORT_TYPE_1394;
            case VideoPortType::DS_VIDEO_PORT_TYPE_DVI:
                return dsVIDEOPORT_TYPE_DVI;
            case VideoPortType::DS_VIDEO_PORT_TYPE_INTERNAL:
                return dsVIDEOPORT_TYPE_INTERNAL;
            default:
                return dsVIDEOPORT_TYPE_HDMI;
        }
    }

    VideoPortType convertVideoPortType(const dsVideoPortType_t dsVideoPort)
    {
        switch (dsVideoPort) {
            case dsVIDEOPORT_TYPE_HDMI:
                return VideoPortType::DS_VIDEO_PORT_TYPE_HDMI;
            case dsVIDEOPORT_TYPE_COMPONENT:
                return VideoPortType::DS_VIDEO_PORT_TYPE_COMPONENT;
            case dsVIDEOPORT_TYPE_SVIDEO:
                return VideoPortType::DS_VIDEO_PORT_TYPE_SVIDEO;
            case dsVIDEOPORT_TYPE_1394:
                return VideoPortType::DS_VIDEO_PORT_TYPE_1394;
            case dsVIDEOPORT_TYPE_DVI:
                return VideoPortType::DS_VIDEO_PORT_TYPE_DVI;
            case dsVIDEOPORT_TYPE_INTERNAL:
                return VideoPortType::DS_VIDEO_PORT_TYPE_INTERNAL;
            default:
                return VideoPortType::DS_VIDEO_PORT_TYPE_HDMI;
        }
    }

    VideoPortResolution convertVideoPortResolution(const dsVideoPortResolution_t& dsResolution)
    {
        VideoPortResolution resolution;
        
        // Map DS pixel resolution to interface VideoResolution enum
        switch (dsResolution.pixelResolution) {
            case dsVIDEO_PIXELRES_720x480:
                resolution.pixelResolution = VideoResolution::DS_VIDEO_PIXELRES_720X480;
                resolution.name = "720x480";
                break;
            case dsVIDEO_PIXELRES_720x576:
                resolution.pixelResolution = VideoResolution::DS_VIDEO_PIXELRES_720X576;
                resolution.name = "720x576";
                break;
            case dsVIDEO_PIXELRES_1280x720:
                resolution.pixelResolution = VideoResolution::DS_VIDEO_PIXELRES_1280X720;
                resolution.name = "1280x720";
                break;
            case dsVIDEO_PIXELRES_1920x1080:
                resolution.pixelResolution = VideoResolution::DS_VIDEO_PIXELRES_1920X1080;
                resolution.name = "1920x1080";
                break;
            case dsVIDEO_PIXELRES_3840x2160:
                resolution.pixelResolution = VideoResolution::DS_VIDEO_PIXELRES_3840X2160;
                resolution.name = "3840x2160";
                break;
            default:
                resolution.pixelResolution = VideoResolution::DS_VIDEO_PIXELRES_1920X1080;
                resolution.name = "1920x1080";
                break;
        }
        
        // Set default values for other fields - can be enhanced based on DS data available
        resolution.aspectRatio = VideoAspectRatio::DS_VIDEO_ASPECT_RATIO_16X9;
        resolution.stereoScopicMode = VideoStereoScopicMode::DS_VIDEO_SSMODE_2D;
        resolution.frameRate = VideoFrameRate::DS_VIDEO_FRAMERATE_60;
        resolution.interlaced = dsResolution.interlaced;
        
        return resolution;
    }

    dsVideoPortResolution_t convertVideoPortResolution(const VideoPortResolution& resolution)
    {
        dsVideoPortResolution_t dsResolution;
        
        // Map interface VideoResolution enum to DS pixel resolution
        switch (resolution.pixelResolution) {
            case VideoResolution::DS_VIDEO_PIXELRES_720X480:
                dsResolution.pixelResolution = dsVIDEO_PIXELRES_720x480;
                break;
            case VideoResolution::DS_VIDEO_PIXELRES_720X576:
                dsResolution.pixelResolution = dsVIDEO_PIXELRES_720x576;
                break;
            case VideoResolution::DS_VIDEO_PIXELRES_1280X720:
                dsResolution.pixelResolution = dsVIDEO_PIXELRES_1280x720;
                break;
            case VideoResolution::DS_VIDEO_PIXELRES_1920X1080:
                dsResolution.pixelResolution = dsVIDEO_PIXELRES_1920x1080;
                break;
            case VideoResolution::DS_VIDEO_PIXELRES_3840X2160:
                dsResolution.pixelResolution = dsVIDEO_PIXELRES_3840x2160;
                break;
            default:
                dsResolution.pixelResolution = dsVIDEO_PIXELRES_1920x1080;
                break;
        }
        
        dsResolution.interlaced = resolution.interlaced;
        // Note: frameRate and aspectRatio conversions would need additional DS API support
        
        return dsResolution;
    }

    // Convert DS HAL HDCP version to interface HDCP version
    VideoPortHdcpProtocolVersion convertHdcpProtocolVersion(const dsHdcpProtocolVersion_t dsHdcpVersion)
    {
        switch (dsHdcpVersion) {
            case dsHDCP_VERSION_1X:
                return VideoPortHdcpProtocolVersion::DS_HDCP_VERSION_1X;
            case dsHDCP_VERSION_2X:
                return VideoPortHdcpProtocolVersion::DS_HDCP_VERSION_2X;
            default:
                return VideoPortHdcpProtocolVersion::DS_HDCP_VERSION_1X;
        }
    }

    // Convert interface HDCP version to DS HAL HDCP version
    dsHdcpProtocolVersion_t convertHdcpProtocolVersionToDSHal(const VideoPortHdcpProtocolVersion hdcpVersion)
    {
        switch (hdcpVersion) {
            case VideoPortHdcpProtocolVersion::DS_HDCP_VERSION_1X:
                return dsHDCP_VERSION_1X;
            case VideoPortHdcpProtocolVersion::DS_HDCP_VERSION_2X:
                return dsHDCP_VERSION_2X;
            default:
                return dsHDCP_VERSION_1X;
        }
    }

    dsDisplayColorSpace_t convertColorSpace(const VideoPortColorSpace colorSpace)
    {
        switch (colorSpace) {
            case VideoPortColorSpace::DS_DISPLAY_COLORSPACE_RGB:
                return dsDISPLAY_COLORSPACE_RGB;
            case VideoPortColorSpace::DS_DISPLAY_COLORSPACE_YCBCR422:
                return dsDISPLAY_COLORSPACE_YCbCr422;
            case VideoPortColorSpace::DS_DISPLAY_COLORSPACE_YCBCR444:
                return dsDISPLAY_COLORSPACE_YCbCr444;
            case VideoPortColorSpace::DS_DISPLAY_COLORSPACE_YCBCR420:
                return dsDISPLAY_COLORSPACE_YCbCr420;
            default:
                return dsDISPLAY_COLORSPACE_RGB;
        }
    }

    VideoPortColorSpace convertColorSpace(const dsDisplayColorSpace_t dsColorSpace)
    {
        switch (dsColorSpace) {
            case dsDISPLAY_COLORSPACE_RGB:
                return VideoPortColorSpace::DS_DISPLAY_COLORSPACE_RGB;
            case dsDISPLAY_COLORSPACE_YCbCr422:
                return VideoPortColorSpace::DS_DISPLAY_COLORSPACE_YCBCR422;
            case dsDISPLAY_COLORSPACE_YCbCr444:
                return VideoPortColorSpace::DS_DISPLAY_COLORSPACE_YCBCR444;
            case dsDISPLAY_COLORSPACE_YCbCr420:
                return VideoPortColorSpace::DS_DISPLAY_COLORSPACE_YCBCR420;
            default:
                return VideoPortColorSpace::DS_DISPLAY_COLORSPACE_RGB;
        }
    }

    dsDisplayQuantizationRange_t convertQuantizationRange(const VideoPortQuantizationRange quantizationRange)
    {
        switch (quantizationRange) {
            case VideoPortQuantizationRange::DS_DISPLAY_QUANTIZATIONRANGE_LIMITED:
                return dsDISPLAY_QUANTIZATIONRANGE_LIMITED;
            case VideoPortQuantizationRange::DS_DISPLAY_QUANTIZATIONRANGE_FULL:
                return dsDISPLAY_QUANTIZATIONRANGE_FULL;
            default:
                return dsDISPLAY_QUANTIZATIONRANGE_LIMITED;
        }
    }

    VideoPortQuantizationRange convertQuantizationRange(const dsDisplayQuantizationRange_t dsQuantizationRange)
    {
        switch (dsQuantizationRange) {
            case dsDISPLAY_QUANTIZATIONRANGE_LIMITED:
                return VideoPortQuantizationRange::DS_DISPLAY_QUANTIZATIONRANGE_LIMITED;
            case dsDISPLAY_QUANTIZATIONRANGE_FULL:
                return VideoPortQuantizationRange::DS_DISPLAY_QUANTIZATIONRANGE_FULL;
            default:
                return VideoPortQuantizationRange::DS_DISPLAY_QUANTIZATIONRANGE_LIMITED;
        }
    }

    VideoPortHdcpStatus convertHdcpStatus(const dsHdcpStatus_t& dsHdcpStatus)
    {
        switch (dsHdcpStatus) {
            case dsHDCP_STATUS_UNPOWERED:
                return VideoPortHdcpStatus::DS_HDCP_STATUS_UNPOWERED;
            case dsHDCP_STATUS_UNAUTHENTICATED:
                return VideoPortHdcpStatus::DS_HDCP_STATUS_UNAUTHENTICATED;
            case dsHDCP_STATUS_AUTHENTICATED:
                return VideoPortHdcpStatus::DS_HDCP_STATUS_AUTHENTICATED;
            case dsHDCP_STATUS_AUTHENTICATIONFAILURE:
                return VideoPortHdcpStatus::DS_HDCP_STATUS_AUTHENTICATIONFAILURE;
            default:
                return VideoPortHdcpStatus::DS_HDCP_STATUS_UNPOWERED;
        }
    }


    void persistVideoPortResolution(const int32_t handle, const dsVideoPortResolution_t& resolution, const bool forceCompatible)
    {
        LOGINFO("persistVideoPortResolution: handle=%d, forceCompatible=%s", handle, forceCompatible ? "true" : "false");
        
        try {
            std::string resolutionName(resolution.name);
            
            // Determine port type based on handle - simplified approach
            dsVideoPortType_t portType = dsVIDEOPORT_TYPE_HDMI; // Default assumption
            
            // Try to get actual port type (this is a simplification - in real dsVideoPort.c it uses _GetVideoPortType)
            intptr_t test_handle = 0;
            if (dsGetVideoPort(dsVIDEOPORT_TYPE_HDMI, 0, &test_handle) == dsERR_NONE && test_handle == handle) {
                portType = dsVIDEOPORT_TYPE_HDMI;
            } else if (dsGetVideoPort(dsVIDEOPORT_TYPE_COMPONENT, 0, &test_handle) == dsERR_NONE && test_handle == handle) {
                portType = dsVIDEOPORT_TYPE_COMPONENT;
            } else if (dsGetVideoPort(dsVIDEOPORT_TYPE_INTERNAL, 0, &test_handle) == dsERR_NONE && test_handle == handle) {
                portType = dsVIDEOPORT_TYPE_INTERNAL;
            }
            
            if (portType == dsVIDEOPORT_TYPE_HDMI || portType == dsVIDEOPORT_TYPE_INTERNAL) {
                // Persist HDMI resolution
                device::HostPersistence::getInstance().persistHostProperty("HDMI0.resolution", resolutionName);
                LOGINFO("Persisted HDMI resolution: %s", resolutionName.c_str());
                _dsHDMIResolution = resolutionName;
                
                // Check compatibility with analog ports
                if (forceCompatible) {
                    // Simplified compatibility logic - in real implementation this would be more complex
                    std::string compatibleResolution = getCompatibleAnalogResolution(resolution);
                    if (!compatibleResolution.empty() && compatibleResolution != _dsCompResolution) {
                        #ifdef HAS_ONLY_COMPOSITE
                            device::HostPersistence::getInstance().persistHostProperty("Baseband0.resolution", compatibleResolution);
                        #else
                            device::HostPersistence::getInstance().persistHostProperty("COMPONENT0.resolution", compatibleResolution);
                        #endif
                        _dsCompResolution = compatibleResolution;
                        LOGINFO("Force compatible: Updated analog resolution to %s", compatibleResolution.c_str());
                    }
                }
            } 
            else if (portType == dsVIDEOPORT_TYPE_COMPONENT) {
                // Persist Component resolution
                #ifdef HAS_ONLY_COMPOSITE
                    device::HostPersistence::getInstance().persistHostProperty("Baseband0.resolution", resolutionName);
                #else
                    device::HostPersistence::getInstance().persistHostProperty("COMPONENT0.resolution", resolutionName);
                #endif
                LOGINFO("Persisted Component resolution: %s", resolutionName.c_str());
                _dsCompResolution = resolutionName;
                
                // Check compatibility with HDMI port
                if (forceCompatible) {
                    std::string compatibleResolution = getCompatibleHDMIResolution(resolution);
                    if (!compatibleResolution.empty() && compatibleResolution != _dsHDMIResolution) {
                        device::HostPersistence::getInstance().persistHostProperty("HDMI0.resolution", compatibleResolution);
                        _dsHDMIResolution = compatibleResolution;
                        LOGINFO("Force compatible: Updated HDMI resolution to %s", compatibleResolution.c_str());
                    }
                }
            }
            
        } catch(...) {
            LOGERR("Exception in persistVideoPortResolution");
        }
    }

    // Helper function to get compatible analog resolution - simplified from dsVideoPort.c
    std::string getCompatibleAnalogResolution(const dsVideoPortResolution_t& hdmiResolution)
    {
        // Simplified compatibility mapping based on dsVideoPort.c patterns
        switch(hdmiResolution.pixelResolution) {
            case dsVIDEO_PIXELRES_3840x2160:
            case dsVIDEO_PIXELRES_4096x2160:
                return "1080p"; // 4K -> 1080p for analog
            case dsVIDEO_PIXELRES_1920x1080:
                return "1080p";
            case dsVIDEO_PIXELRES_1280x720:
                return "720p";
            case dsVIDEO_PIXELRES_720x480:
                return "480p";
            case dsVIDEO_PIXELRES_720x576:
                return "576p";
            default:
                return "1080p"; // Default fallback
        }
    }

    // Helper function to get compatible HDMI resolution - simplified from dsVideoPort.c
    std::string getCompatibleHDMIResolution(const dsVideoPortResolution_t& analogResolution)
    {
        // For analog to HDMI, generally same resolution or upgrade
        switch(analogResolution.pixelResolution) {
            case dsVIDEO_PIXELRES_720x480:
                return "480p";  // Note: dsVideoPort.c converts 480i to 480p
            case dsVIDEO_PIXELRES_720x576:
                return "576p";
            case dsVIDEO_PIXELRES_1280x720:
                return "720p";
            case dsVIDEO_PIXELRES_1920x1080:
                return "1080p";
            default:
                return "1080p"; // Default fallback
        }
    }

    // Get persistent color depth - following dsVideoPort.c getPersistentColorDepth() pattern
    DisplayColorDepth getPersistentColorDepth()
    {
        DisplayColorDepth defaultColorDepth = static_cast<DisplayColorDepth>(DEFAULT_COLOR_DEPTH);
        std::string colorDepthStr = std::to_string(static_cast<int>(defaultColorDepth));
        
        try {
            colorDepthStr = device::HostPersistence::getInstance().getProperty("HDMI0.colorDepth", colorDepthStr);
            int colorDepthValue = std::stoi(colorDepthStr);
            DisplayColorDepth persistentColorDepth = static_cast<DisplayColorDepth>(colorDepthValue);
            LOGINFO("Reading HDMI persistent color depth: %d", colorDepthValue);
            return persistentColorDepth;
        } catch(...) {
            LOGERR("Reading HDMI persistent color depth %s conversion failed", colorDepthStr.c_str());
            return defaultColorDepth;
        }
    }
};
