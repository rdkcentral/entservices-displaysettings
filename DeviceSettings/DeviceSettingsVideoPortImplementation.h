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

#include "Module.h"

#include <memory>
#include <unordered_map>
#include <chrono>
#include <cstdint>

#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>

// Note: Need Exchange interface includes for notification interfaces  
#include <interfaces/IDeviceSettingsVideoPort.h>  // For IDeviceSettingsVideoPort::INotification

#include "VideoPort.h"

#include "list.hpp"
#include "DeviceSettingsTypes.h"

namespace WPEFramework {
namespace Plugin {
    class DeviceSettingsVideoPortImpl : public VideoPort::INotification
    {
    public:
        // Note: No need to inherit from Exchange::IDeviceSettingsVideoPort anymore
        // DeviceSettingsImp handles the WPEFramework interface contract
        // This class only needs VideoPort::INotification for hardware callbacks
        
        DeviceSettingsVideoPortImpl();
        ~DeviceSettingsVideoPortImpl() override;

        static DeviceSettingsVideoPortImpl* Create()
        {
            return new DeviceSettingsVideoPortImpl();
        }

        // We do not allow this plugin to be copied !!
        DeviceSettingsVideoPortImpl(const DeviceSettingsVideoPortImpl&)            = delete;
        DeviceSettingsVideoPortImpl& operator=(const DeviceSettingsVideoPortImpl&) = delete;

        // INTERFACE_MAP not needed - this is an implementation class aggregated by DeviceSettingsImp
        // DeviceSettingsImp handles QueryInterface for all component interfaces

    public:

        // Template method for dispatching VideoPort Events
        template<typename Func, typename... Args>
        void dispatchVideoPortEvent(Func notifyFunc, Args&&... args);

        // Template methods for notification management
        template <typename T>
        Core::hresult Register(std::list<T*>& list, T* notification);

        template <typename T>
        Core::hresult Unregister(std::list<T*>& list, const T* notification);

        // Public notification registration methods called by DeviceSettingsImp
        Core::hresult Register(Exchange::IDeviceSettingsVideoPort::INotification* notification);
        Core::hresult Unregister(Exchange::IDeviceSettingsVideoPort::INotification* notification);

        // Event notification methods removed - DS HAL callbacks now directly call dispatchVideoPortEvent

        // Required VideoPort::INotification interface implementations
        void OnResolutionPreChange(const ResolutionChange resolution) override;
        void OnResolutionPostChange(const ResolutionChange resolution) override;
        void OnHDCPStatusChange(const VideoPortHdcpStatus hdcpStatus) override;
        void OnVideoFormatUpdate(const HDRStandard videoFormatHDR) override;

        // VideoPort interface method implementations called by DeviceSettingsImp 
        uint32_t GetVideoPort(const VideoPortType videoPort, const int32_t index, int32_t &handle);
        uint32_t IsVideoPortEnabled(const int32_t handle, bool &enabled);
        uint32_t EnableVideoPort(const int32_t handle, const bool enabled);
        uint32_t IsVideoPortDisplayConnected(const int32_t handle, bool &connected);
        uint32_t IsVideoPortActive(const int32_t handle, bool &active);
        uint32_t GetVideoPortResolution(const int32_t handle, VideoPortResolution &resolution);
        uint32_t SetVideoPortResolution(const int32_t handle, const VideoPortResolution resolution, const bool persist, const bool forceCompatibility);
        uint32_t GetColorSpace(const int32_t handle, VideoPortColorSpace &colorSpace);
        uint32_t SetColorSpace(const int32_t handle, const VideoPortColorSpace colorSpace, const bool persist);
        uint32_t GetQuantizationRange(const int32_t handle, VideoPortQuantizationRange &quantizationRange);
        uint32_t SetQuantizationRange(const int32_t handle, const VideoPortQuantizationRange quantizationRange, const bool persist);
        uint32_t GetVideoPortHDCPStatus(const int32_t handle, VideoPortHdcpStatus &hdcpStatus);
        uint32_t GetHDCPProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion);
        uint32_t GetHDCPReceiverProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion);
        uint32_t GetVideoPortHDCPCurrentProtocol(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion);
        uint32_t GetHDCPCurrentProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion);
        uint32_t SetVideoPortHDCPProfile(const int32_t handle, const VideoPortHdcpProtocolVersion hdcpVersion, const bool persist);
        uint32_t EnableHDCPOnVideoPort(const int32_t handle, const bool hdcpEnable, const uint8_t* hdcpKey, const uint16_t hdcpKeySize);
        uint32_t IsHDCPEnabledOnVideoPort(const int32_t handle, bool &hdcpEnabled);
        uint32_t GetTVHDRCapabilities(const int32_t handle, int32_t &capabilities);
        uint32_t GetTVSupportedResolutions(const int32_t handle, int32_t &resolutions);
        uint32_t SetForceDisable4K(const int32_t handle, const bool disable);
        uint32_t GetForceDisable4K(const int32_t handle, bool &disabled);
        uint32_t IsVideoPortOutputHDR(const int32_t handle, bool &isHDR);
        uint32_t ResetVideoPortOutputToSDR();
        uint32_t GetHDMIPreference(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion);
        uint32_t SetHDMIPreference(const int32_t handle, const VideoPortHdcpProtocolVersion hdcpVersion);
        uint32_t GetVideoEOTF(const int32_t handle, HDRStandard &hdrStandard);
        uint32_t IsVideoPortDisplaySurround(const int32_t handle, bool &surround);
        uint32_t GetVideoPortDisplaySurroundMode(const int32_t handle, VideoPortSurroundMode &surroundMode);
        uint32_t GetColorDepth(const int32_t handle, uint32_t &colorDepth);

        // Additional VideoPort methods
        uint32_t GetMatrixCoefficients(const int32_t handle, DisplayMatrixCoefficients &matrixCoefficients);
        uint32_t GetCurrentOutputSettings(const int32_t handle, DSOutputSettings &outputSettings);
        uint32_t SetBackgroundColor(const int32_t handle, const VideoBackgroundColor backgroundColor);
        uint32_t SetForceHDRMode(const int32_t handle, const HDRStandard hdrMode);
        uint32_t GetColorDepthCapabilities(const int32_t handle, uint32_t &colorDepthCapabilities);
        uint32_t GetPreferredColorDepth(const int32_t handle, DisplayColorDepth &colorDepth, const bool persist);
        uint32_t SetPreferredColorDepth(const int32_t handle, const DisplayColorDepth colorDepth, const bool persist);

    private:
        std::list<Exchange::IDeviceSettingsVideoPort::INotification*> _VideoPortNotifications;

        // Thread-safety locks
        mutable Core::CriticalSection _apiLock;
        mutable Core::CriticalSection _callbackLock;

        VideoPort _videoPort;
    };

} // namespace Plugin
} // namespace WPEFramework