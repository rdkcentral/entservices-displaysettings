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

#include <functional>
#include <unistd.h>
#include <memory>

#include <core/IAction.h>
#include <core/Time.h>
#include <core/WorkerPool.h>

#include "UtilsLogging.h"
#include "secure_wrapper.h"
#include "VideoPort.h"
#include "hal/dVideoPortImpl.h"

VideoPort::VideoPort(INotification& parent, std::shared_ptr<IPlatform> platform)
    : _platform(std::move(platform))
    , _parent(parent)
{
    LOGINFO("VideoPort Constructor");
    Platform_init();
}

void VideoPort::Platform_init()
{
    LOGINFO("VideoPort Init - Setting up event callbacks");
    
    // Set up callback bundle for VideoPort events - using global CallbackBundle pattern
    CallbackBundle bundle;
    
    bundle.OnResolutionPreChange = [this](const ResolutionChange resolution) {
        this->OnResolutionPreChange(resolution);
    };
    bundle.OnResolutionPostChange = [this](const ResolutionChange resolution) {
        this->OnResolutionPostChange(resolution);
    };
    bundle.OnHDCPStatusChange = [this](const VideoPortHdcpStatus hdcpStatus) {
        this->OnHDCPStatusChange(hdcpStatus);
    };
    bundle.OnVideoFormatUpdate = [this](const HDRStandard videoFormatHDR) {
        this->OnVideoFormatUpdate(videoFormatHDR);
    };
    
    if (_platform) {
        // Use interface method directly - no casting needed
        this->platform().setAllCallbacks(bundle);
        this->platform().getPersistenceValue();
    }
    
}

uint32_t VideoPort::GetVideoPort(const VideoPortType videoPort, const int32_t index, int32_t &handle) {
    LOGINFO("GetVideoPort: videoPort=%d, index=%d", static_cast<int>(videoPort), index);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetVideoPort(videoPort, index, handle);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetVideoPort: SUCCESS - platform call completed successfully, handle=%d", handle);
    } else {
        LOGERR("GetVideoPort: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::IsVideoPortEnabled(const int32_t handle, bool &enabled) {
    LOGINFO("IsVideoPortEnabled: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().IsVideoPortEnabled(handle, enabled);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("IsVideoPortEnabled: SUCCESS - enabled=%s", enabled ? "true" : "false");
    } else {
        LOGERR("IsVideoPortEnabled: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::EnableVideoPort(const int32_t handle, const bool enabled) {
    LOGINFO("EnableVideoPort: handle=%d, enabled=%s", handle, enabled ? "true" : "false");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().EnableVideoPort(handle, enabled);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("EnableVideoPort: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("EnableVideoPort: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::IsVideoPortDisplayConnected(const int32_t handle, bool &connected) {
    LOGINFO("IsVideoPortDisplayConnected: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().IsVideoPortDisplayConnected(handle, connected);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("IsVideoPortDisplayConnected: SUCCESS - connected=%s", connected ? "true" : "false");
    } else {
        LOGERR("IsVideoPortDisplayConnected: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::IsVideoPortActive(const int32_t handle, bool &active) {
    LOGINFO("IsVideoPortActive: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().IsVideoPortActive(handle, active);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("IsVideoPortActive: SUCCESS - active=%s", active ? "true" : "false");
    } else {
        LOGERR("IsVideoPortActive: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetVideoPortResolution(const int32_t handle, VideoPortResolution &resolution) {
    LOGINFO("GetVideoPortResolution: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetVideoPortResolution(handle, resolution);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetVideoPortResolution: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetVideoPortResolution: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetColorDepth(const int32_t handle, uint32_t &colorDepth) {
    LOGINFO("GetColorDepth: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetColorDepth(handle, colorDepth);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetColorDepth: SUCCESS - colorDepth=%u", colorDepth);
    } else {
        LOGERR("GetColorDepth: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetVideoPortColorDepth(const int32_t handle, const uint32_t colorDepth) {
    LOGINFO("SetVideoPortColorDepth: handle=%d, colorDepth=%u", handle, colorDepth);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetVideoPortColorDepth(handle, colorDepth);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetVideoPortColorDepth: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetVideoPortColorDepth: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetQuantizationRange(const int32_t handle, VideoPortQuantizationRange &quantizationRange) {
    LOGINFO("GetQuantizationRange: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetQuantizationRange(handle, quantizationRange);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetQuantizationRange: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetQuantizationRange: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetVideoPortQuantizationRange(const int32_t handle, const VideoPortQuantizationRange quantizationRange) {
    LOGINFO("SetVideoPortQuantizationRange: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetVideoPortQuantizationRange(handle, quantizationRange);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetVideoPortQuantizationRange: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetVideoPortQuantizationRange: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetColorSpace(const int32_t handle, VideoPortColorSpace &colorSpace) {
    LOGINFO("GetColorSpace: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetColorSpace(handle, colorSpace);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetColorSpace: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetColorSpace: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetColorSpace(const int32_t handle, const VideoPortColorSpace colorSpace) {
    LOGINFO("SetColorSpace: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetColorSpace(handle, colorSpace);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetColorSpace: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetColorSpace: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetVideoPortFrameRate(const int32_t handle, uint32_t &frameRate) {
    LOGINFO("GetVideoPortFrameRate: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetVideoPortFrameRate(handle, frameRate);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetVideoPortFrameRate: SUCCESS - frameRate=%u", frameRate);
    } else {
        LOGERR("GetVideoPortFrameRate: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetVideoPortFrameRate(const int32_t handle, const uint32_t frameRate) {
    LOGINFO("SetVideoPortFrameRate: handle=%d, frameRate=%u", handle, frameRate);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetVideoPortFrameRate(handle, frameRate);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetVideoPortFrameRate: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetVideoPortFrameRate: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetVideoPortHDCPStatus(const int32_t handle, VideoPortHdcpStatus &hdcpStatus) {
    LOGINFO("GetVideoPortHDCPStatus: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetVideoPortHDCPStatus(handle, hdcpStatus);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetVideoPortHDCPStatus: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetVideoPortHDCPStatus: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetHDCPProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion) {
    LOGINFO("GetHDCPProtocolVersionOnVideoPort: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetHDCPProtocolVersionOnVideoPort(handle, hdcpVersion);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetHDCPProtocolVersionOnVideoPort: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetHDCPProtocolVersionOnVideoPort: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetHDCPReceiverProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion) {
    LOGINFO("GetHDCPReceiverProtocolVersionOnVideoPort: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetHDCPReceiverProtocolVersionOnVideoPort(handle, hdcpVersion);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetHDCPReceiverProtocolVersionOnVideoPort: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetHDCPReceiverProtocolVersionOnVideoPort: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetHDCPCurrentProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion) {
    LOGINFO("GetHDCPCurrentProtocolVersionOnVideoPort: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetHDCPCurrentProtocolVersionOnVideoPort(handle, hdcpVersion);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetHDCPCurrentProtocolVersionOnVideoPort: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetHDCPCurrentProtocolVersionOnVideoPort: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetVideoPortResolution(const int32_t handle, const VideoPortResolution& resolution, const bool persist, const bool forceCompatibility) {
    LOGINFO("SetVideoPortResolution: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetVideoPortResolution(handle, resolution, persist, forceCompatibility);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetVideoPortResolution: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetVideoPortResolution: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::EnableHDCPOnVideoPort(const int32_t handle, const bool hdcpEnable, const uint8_t* hdcpKey, const uint16_t hdcpKeySize) {
    LOGINFO("EnableHDCPOnVideoPort: handle=%d, hdcpEnable=%s", handle, hdcpEnable ? "true" : "false");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().EnableHDCPOnVideoPort(handle, hdcpEnable, hdcpKey, hdcpKeySize);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("EnableHDCPOnVideoPort: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("EnableHDCPOnVideoPort: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::IsHDCPEnabledOnVideoPort(const int32_t handle, bool &hdcpEnabled) {
    LOGINFO("IsHDCPEnabledOnVideoPort: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().IsHDCPEnabledOnVideoPort(handle, hdcpEnabled);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("IsHDCPEnabledOnVideoPort: SUCCESS - hdcpEnabled=%s", hdcpEnabled ? "true" : "false");
    } else {
        LOGERR("IsHDCPEnabledOnVideoPort: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetTVHDRCapabilities(const int32_t handle, int32_t &capabilities) {
    LOGINFO("GetTVHDRCapabilities: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetTVHDRCapabilities(handle, capabilities);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetTVHDRCapabilities: SUCCESS - capabilities=0x%x", capabilities);
    } else {
        LOGERR("GetTVHDRCapabilities: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetTVSupportedResolutions(const int32_t handle, int32_t &resolutions) {
    LOGINFO("GetTVSupportedResolutions: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetTVSupportedResolutions(handle, resolutions);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetTVSupportedResolutions: SUCCESS - resolutions=0x%x", resolutions);
    } else {
        LOGERR("GetTVSupportedResolutions: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetForceDisable4K(const int32_t handle, const bool disable) {
    LOGINFO("SetForceDisable4K: handle=%d, disable=%s", handle, disable ? "true" : "false");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetForceDisable4K(handle, disable);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetForceDisable4K: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetForceDisable4K: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetForceDisable4K(const int32_t handle, bool &disabled) {
    LOGINFO("GetForceDisable4K: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetForceDisable4K(handle, disabled);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetForceDisable4K: SUCCESS - disabled=%s", disabled ? "true" : "false");
    } else {
        LOGERR("GetForceDisable4K: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::IsVideoPortOutputHDR(const int32_t handle, bool &isHDR) {
    LOGINFO("IsVideoPortOutputHDR: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().IsVideoPortOutputHDR(handle, isHDR);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("IsVideoPortOutputHDR: SUCCESS - isHDR=%s", isHDR ? "true" : "false");
    } else {
        LOGERR("IsVideoPortOutputHDR: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::ResetVideoPortOutputToSDR() {
    LOGINFO("ResetVideoPortOutputToSDR");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().ResetVideoPortOutputToSDR();
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("ResetVideoPortOutputToSDR: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("ResetVideoPortOutputToSDR: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetHDMIPreference(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion) {
    LOGINFO("GetHDMIPreference: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetHDMIPreference(handle, hdcpVersion);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetHDMIPreference: SUCCESS - hdcpVersion=%d", static_cast<int>(hdcpVersion));
    } else {
        LOGERR("GetHDMIPreference: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetHDMIPreference(const int32_t handle, const VideoPortHdcpProtocolVersion hdcpVersion) {
    LOGINFO("SetHDMIPreference: handle=%d, hdcpVersion=%d", handle, static_cast<int>(hdcpVersion));
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetHDMIPreference(handle, hdcpVersion);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetHDMIPreference: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetHDMIPreference: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetVideoEOTF(const int32_t handle, HDRStandard &hdrStandard) {
    LOGINFO("GetVideoEOTF: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetVideoEOTF(handle, hdrStandard);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetVideoEOTF: SUCCESS - hdrStandard=%d", static_cast<int>(hdrStandard));
    } else {
        LOGERR("GetVideoEOTF: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetMatrixCoefficients(const int32_t handle, DisplayMatrixCoefficients &matrixCoefficients) {
    LOGINFO("GetMatrixCoefficients: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetMatrixCoefficients(handle, matrixCoefficients);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetMatrixCoefficients: SUCCESS - matrixCoefficients=%d", static_cast<int>(matrixCoefficients));
    } else {
        LOGERR("GetMatrixCoefficients: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::IsVideoPortDisplaySurround(const int32_t handle, bool &surround) {
    LOGINFO("IsVideoPortDisplaySurround: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().IsVideoPortDisplaySurround(handle, surround);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("IsVideoPortDisplaySurround: SUCCESS - surround=%s", surround ? "true" : "false");
    } else {
        LOGERR("IsVideoPortDisplaySurround: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetVideoPortDisplaySurroundMode(const int32_t handle, VideoPortSurroundMode &surroundMode) {
    LOGINFO("GetVideoPortDisplaySurroundMode: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetVideoPortDisplaySurroundMode(handle, surroundMode);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetVideoPortDisplaySurroundMode: SUCCESS - surroundMode=%d", static_cast<int>(surroundMode));
    } else {
        LOGERR("GetVideoPortDisplaySurroundMode: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetCurrentOutputSettings(const int32_t handle, DSOutputSettings &outputSettings) {
    LOGINFO("GetCurrentOutputSettings: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetCurrentOutputSettings(handle, outputSettings);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetCurrentOutputSettings: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetCurrentOutputSettings: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetBackgroundColor(const int32_t handle, const VideoBackgroundColor backgroundColor) {
    LOGINFO("SetBackgroundColor: handle=%d, backgroundColor=%d", handle, static_cast<int>(backgroundColor));
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetBackgroundColor(handle, backgroundColor);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetBackgroundColor: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetBackgroundColor: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetForceHDRMode(const int32_t handle, const HDRStandard hdrMode) {
    LOGINFO("SetForceHDRMode: handle=%d, hdrMode=%d", handle, static_cast<int>(hdrMode));
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetForceHDRMode(handle, hdrMode);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetForceHDRMode: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetForceHDRMode: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetColorDepthCapabilities(const int32_t handle, uint32_t &colorDepthCapabilities) {
    LOGINFO("GetColorDepthCapabilities: handle=%d", handle);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetColorDepthCapabilities(handle, colorDepthCapabilities);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetColorDepthCapabilities: SUCCESS - colorDepthCapabilities=0x%x", colorDepthCapabilities);
    } else {
        LOGERR("GetColorDepthCapabilities: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::GetPreferredColorDepth(const int32_t handle, DisplayColorDepth &colorDepth, const bool persist) {
    LOGINFO("GetPreferredColorDepth: handle=%d, persist=%s", handle, persist ? "true" : "false");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetPreferredColorDepth(handle, colorDepth, persist);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetPreferredColorDepth: SUCCESS - colorDepth=%d", static_cast<int>(colorDepth));
    } else {
        LOGERR("GetPreferredColorDepth: FAILED - result=%u", result);
    }
    return result;
}

uint32_t VideoPort::SetPreferredColorDepth(const int32_t handle, const DisplayColorDepth colorDepth, const bool persist) {
    LOGINFO("SetPreferredColorDepth: handle=%d, colorDepth=%d, persist=%s", handle, static_cast<int>(colorDepth), persist ? "true" : "false");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetPreferredColorDepth(handle, colorDepth, persist);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetPreferredColorDepth: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetPreferredColorDepth: FAILED - result=%u", result);
    }
    return result;
}

// VideoPort event handling methods - Forward DS HAL events to parent notification system
void VideoPort::OnResolutionPreChange(const ResolutionChange resolution)
{
    LOGINFO("VideoPort::OnResolutionPreChange: forwarding to parent");
    _parent.OnResolutionPreChange(resolution);
}

void VideoPort::OnResolutionPostChange(const ResolutionChange resolution)
{
    LOGINFO("VideoPort::OnResolutionPostChange: forwarding to parent");
    _parent.OnResolutionPostChange(resolution);
}

void VideoPort::OnHDCPStatusChange(const VideoPortHdcpStatus hdcpStatus)
{
    LOGINFO("VideoPort::OnHDCPStatusChange: forwarding to parent");
    _parent.OnHDCPStatusChange(hdcpStatus);
}

void VideoPort::OnVideoFormatUpdate(const HDRStandard videoFormatHDR)
{
    LOGINFO("VideoPort::OnVideoFormatUpdate: forwarding to parent");
    _parent.OnVideoFormatUpdate(videoFormatHDR);
}