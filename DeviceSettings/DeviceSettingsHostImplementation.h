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
#include <chrono>
#include <cstdint>

#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>

// Note: Need Exchange interface includes for notification interfaces  
#include <interfaces/IDeviceSettingsHost.h>  // For IDeviceSettingsHost::INotification

#include "Host.h"

#include "list.hpp"
#include "DeviceSettingsTypes.h"

namespace WPEFramework {
namespace Plugin {

    class DeviceSettingsHostImpl : public Host::INotification {
        // Note: No need to inherit from Exchange::IDeviceSettingsHost anymore
        // DeviceSettingsImp handles the WPEFramework interface contract
        // This class only needs Host::INotification for hardware callbacks

    private:
        DeviceSettingsHostImpl(const DeviceSettingsHostImpl&) = delete;
        DeviceSettingsHostImpl& operator=(const DeviceSettingsHostImpl&) = delete;

    public:
        DeviceSettingsHostImpl();
        virtual ~DeviceSettingsHostImpl();

        static DeviceSettingsHostImpl* Create() {
            return new DeviceSettingsHostImpl();
        }

        // INTERFACE_MAP not needed - this is an implementation class aggregated by DeviceSettingsImp
        // DeviceSettingsImp handles QueryInterface for all component interfaces

    public:

        // Template method for dispatching Host Events
        template<typename Func, typename... Args>
        void dispatchHostEvent(Func notifyFunc, Args&&... args);

        // Template methods for notification management
        template <typename T>
        Core::hresult Register(std::list<T*>& list, T* notification);

        template <typename T>
        Core::hresult Unregister(std::list<T*>& list, const T* notification);

        // Public notification registration methods called by DeviceSettingsImp
        Core::hresult Register(Exchange::IDeviceSettingsHost::INotification* notification);
        Core::hresult Unregister(Exchange::IDeviceSettingsHost::INotification* notification);

        // Required Host::INotification interface implementation (called by DS HAL)
        void OnSleepModeChanged(const HostSleepMode sleepMode) override;

        // Host interface method implementations called by DeviceSettingsImp 
        Core::hresult GetPreferredSleepMode(HostSleepMode &mode);
        Core::hresult SetPreferredSleepMode(const HostSleepMode mode);
        Core::hresult GetCPUTemperature(float &temperature);
        Core::hresult GetHALVersion(uint32_t &versionNo);
        Core::hresult GetSoCID(string &socID);
        Core::hresult GetEDID(uint8_t edId[], const uint16_t edIdLength);
        Core::hresult GetMS12ConfigType(string &ms12Config);

    private:
        std::list<Exchange::IDeviceSettingsHost::INotification*> _HostNotifications;

        // Thread-safety locks
        mutable Core::CriticalSection _apiLock;
        mutable Core::CriticalSection _callbackLock;

        Host _host;
    };

} // namespace Plugin
} // namespace WPEFramework