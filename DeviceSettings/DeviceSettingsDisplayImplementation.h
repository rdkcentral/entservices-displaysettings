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
#include <interfaces/IDeviceSettingsDisplay.h>  // For IDeviceSettingsDisplay::INotification

#include "Display.h"

#include "list.hpp"
#include "DeviceSettingsTypes.h"

namespace WPEFramework {
namespace Plugin {
    class DeviceSettingsDisplayImpl : public Display::INotification
    {
    public:
        // Note: No need to inherit from Exchange::IDeviceSettingsDisplay anymore
        // DeviceSettingsImp handles the WPEFramework interface contract
        // This class only needs Display::INotification for hardware callbacks
        
        DeviceSettingsDisplayImpl();
        ~DeviceSettingsDisplayImpl() override;

        static DeviceSettingsDisplayImpl* Create()
        {
            return new DeviceSettingsDisplayImpl();
        }

        // We do not allow this plugin to be copied !!
        DeviceSettingsDisplayImpl(const DeviceSettingsDisplayImpl&)            = delete;
        DeviceSettingsDisplayImpl& operator=(const DeviceSettingsDisplayImpl&) = delete;

        // INTERFACE_MAP not needed - this is an implementation class aggregated by DeviceSettingsImp
        // DeviceSettingsImp handles QueryInterface for all component interfaces

    public:

        // Template method for dispatching Display Events
        template<typename Func, typename... Args>
        void dispatchDisplayEvent(Func notifyFunc, Args&&... args);

        // Template methods for notification management
        template <typename T>
        Core::hresult Register(std::list<T*>& list, T* notification);

        template <typename T>
        Core::hresult Unregister(std::list<T*>& list, const T* notification);

        // Public notification registration methods called by DeviceSettingsImp
        Core::hresult Register(IDisplayNotification* notification);
        Core::hresult Unregister(IDisplayNotification* notification);
        Core::hresult Register(IDisplayHDMIHotPlugNotification* notification);
        Core::hresult Unregister(IDisplayHDMIHotPlugNotification* notification);

        // Required Display::INotification interface implementations
        void OnDisplayRxSense(const DisplayEvent displayEvent) override;
        void OnDisplayHDCPStatus() override;
        void OnDisplayHDMIHotPlug(const DisplayEvent displayEvent) override;

        // Display interface method implementations called by DeviceSettingsImp 
        uint32_t GetDisplayEdid(const int32_t handle, DisplayEDID &edId, IDSVideoPortResolutionIterator*& supportedResolutionList);
        uint32_t GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength);
        
        // New Display interface methods
        uint32_t GetDisplay(const DisplayPortType portType, const int32_t index, int32_t &handle);
        uint32_t GetDisplayAspectRatio(const int32_t handle, Exchange::IDeviceSettingsDisplay::DisplayVideoAspectRatio &aspectRatio);
        uint32_t SetAllmEnabled(const int32_t handle, const bool enabled);
        uint32_t SetAVIContentType(const int32_t handle, const DisplayAVIContentType contentType);
        uint32_t SetAVIScanInformation(const int32_t handle, const DisplayAVIScanInformation scanInfo);

        // Template method for event dispatch
        template<typename Func, typename... Args>
        void dispatchDisplayHDMIHotPlugEvent(Func notifyFunc, Args&&... args);

    private:
        std::list<IDisplayNotification*> _DisplayNotifications;
        std::list<IDisplayHDMIHotPlugNotification*> _DisplayHDMIHotPlugNotifications;

        // Thread-safety locks
        mutable Core::CriticalSection _apiLock;
        mutable Core::CriticalSection _callbackLock;

        Display _display;
    };

} // namespace Plugin
} // namespace WPEFramework