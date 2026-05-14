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
#include <interfaces/IDeviceSettingsVideoDevice.h>  // For IDeviceSettingsVideoDevice::INotification

#include "VideoDevice.h"

#include "list.hpp"
#include "DeviceSettingsTypes.h"

namespace WPEFramework {
namespace Plugin {
    class DeviceSettingsVideoDeviceImpl : public VideoDevice::INotification
    {
    public:
        // Note: No need to inherit from Exchange::IDeviceSettingsVideoDevice anymore
        // DeviceSettingsImp handles the WPEFramework interface contract
        // This class only needs VideoDevice::INotification for hardware callbacks
        
        DeviceSettingsVideoDeviceImpl();
        ~DeviceSettingsVideoDeviceImpl() override;

        static DeviceSettingsVideoDeviceImpl* Create()
        {
            return new DeviceSettingsVideoDeviceImpl();
        }

        // We do not allow this plugin to be copied !!
        DeviceSettingsVideoDeviceImpl(const DeviceSettingsVideoDeviceImpl&)            = delete;
        DeviceSettingsVideoDeviceImpl& operator=(const DeviceSettingsVideoDeviceImpl&) = delete;

        // INTERFACE_MAP not needed - this is an implementation class aggregated by DeviceSettingsImp
        // DeviceSettingsImp handles QueryInterface for all component interfaces

    public:

        // Template method for dispatching VideoDevice Events
        template<typename Func, typename... Args>
        void dispatchVideoDeviceEvent(Func notifyFunc, Args&&... args);

        // Template methods for notification management
        template <typename T>
        Core::hresult Register(std::list<T*>& list, T* notification);

        template <typename T>
        Core::hresult Unregister(std::list<T*>& list, const T* notification);

        // Public notification registration methods called by DeviceSettingsImp
        Core::hresult Register(Exchange::IDeviceSettingsVideoDevice::INotification* notification);
        Core::hresult Unregister(Exchange::IDeviceSettingsVideoDevice::INotification* notification);

        // Required VideoDevice::INotification interface implementations
        void OnZoomSettingsChanged(const VideoDeviceZoom zoomSetting) override;
        void OnDisplayFrameratePreChange(const string frameRate) override;
        void OnDisplayFrameratePostChange(const string frameRate) override;

        // VideoDevice interface method implementations called by DeviceSettingsImp 
        uint32_t GetVideoDeviceHandle(const int32_t index, int32_t &handle);
        uint32_t SetVideoDeviceDFC(const int32_t handle, const VideoDeviceZoom zoomSetting);
        uint32_t GetVideoDeviceDFC(const int32_t handle, VideoDeviceZoom &zoomSetting);
        uint32_t GetHDRCapabilities(const int32_t handle, int32_t &capabilities);
        uint32_t GetSupportedVideoCodingFormats(const int32_t handle, int32_t &supportedFormats);
        uint32_t GetCodecInfo(const int32_t handle, const VideoDeviceCodec videoCodec, IDeviceSettingsVideoCodecProfileSupportIterator*& codecInfo);
        uint32_t DisableHDR(const int32_t handle, const bool disable);
        uint32_t SetFRFMode(const int32_t handle, const int32_t frfmode);
        uint32_t GetFRFMode(const int32_t handle, int32_t &frfmode);
        uint32_t GetCurrentDisplayFrameRate(const int32_t handle, string &framerate);
        uint32_t SetDisplayFrameRate(const int32_t handle, const string framerate);

    private:
        std::list<Exchange::IDeviceSettingsVideoDevice::INotification*> _VideoDeviceNotifications;

        // Thread-safety locks
        mutable Core::CriticalSection _apiLock;
        mutable Core::CriticalSection _callbackLock;

        VideoDevice _videoDevice;
    };

} // namespace Plugin
} // namespace WPEFramework