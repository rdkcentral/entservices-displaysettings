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
#include <interfaces/IDeviceSettingsCompositeIn.h>  // For IDeviceSettingsCompositeIn::INotification

#include "CompositeIn.h"

#include "list.hpp"
#include "DeviceSettingsTypes.h"

namespace WPEFramework {
namespace Plugin {
    class DeviceSettingsCompositeInImpl : public CompositeIn::INotification
    {
    public:
        // Note: No need to inherit from Exchange::IDeviceSettingsCompositeIn anymore
        // DeviceSettingsImp handles the WPEFramework interface contract
        // This class only needs CompositeIn::INotification for hardware callbacks
        
        DeviceSettingsCompositeInImpl();
        ~DeviceSettingsCompositeInImpl() override;

        static DeviceSettingsCompositeInImpl* Create()
        {
            return new DeviceSettingsCompositeInImpl();
        }

        // We do not allow this plugin to be copied !!
        DeviceSettingsCompositeInImpl(const DeviceSettingsCompositeInImpl&)            = delete;
        DeviceSettingsCompositeInImpl& operator=(const DeviceSettingsCompositeInImpl&) = delete;

        // INTERFACE_MAP not needed - this is an implementation class aggregated by DeviceSettingsImp
        // DeviceSettingsImp handles QueryInterface for all component interfaces

    public:

        // Template method for dispatching CompositeIn Events
        template<typename Func, typename... Args>
        void dispatchCompositeInEvent(Func notifyFunc, Args&&... args);

        // Template methods for notification management
        template <typename T>
        Core::hresult Register(std::list<T*>& list, T* notification);

        template <typename T>
        Core::hresult Unregister(std::list<T*>& list, const T* notification);

        // Public notification registration methods called by DeviceSettingsImp
        Core::hresult Register(Exchange::IDeviceSettingsCompositeIn::INotification* notification);
        Core::hresult Unregister(Exchange::IDeviceSettingsCompositeIn::INotification* notification);

        // Required CompositeIn::INotification interface implementations - receive WPE Framework types from HAL
        void OnCompositeInHotPlug(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const bool isConnected) override;
        void OnCompositeInSignalStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus signalStatus) override;
        void OnCompositeInStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const bool isPresented) override;
        void OnCompositeInVideoModeUpdate(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const Exchange::IDeviceSettingsCompositeIn::DisplayVideoPortResolution videoResolution) override;

        // CompositeIn interface method implementations called by DeviceSettingsImp 
        uint32_t GetNrOfCompositeInputs(int32_t &nrCompositeInputs);
        uint32_t GetCompositeInStatus(CompositeInStatus &status);
        uint32_t SelectCompositeInPort(const CompositeInPort port);
        uint32_t ScaleCompositeInVideo(const CompositeInVideoRectangle videoRect);

    private:
        std::list<Exchange::IDeviceSettingsCompositeIn::INotification*> _CompositeInNotifications;

        // Thread-safety locks
        mutable Core::CriticalSection _apiLock;
        mutable Core::CriticalSection _callbackLock;

        CompositeIn _compositeIn;
    };

} // namespace Plugin
} // namespace WPEFramework