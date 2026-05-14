/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

//#include <interfaces/IDeviceSettingsManager.h>
// Note: Need Exchange interface includes for notification interfaces
#include <interfaces/IDeviceSettingsFPD.h>  // For IDeviceSettingsFPD::INotification

#include "fpd.h"
//#include "HdmiIn.h"

#include "list.hpp"
#include "DeviceSettingsTypes.h"

namespace WPEFramework {
namespace Plugin {
    class DeviceSettingsFPDImpl : public FPD::INotification
    {
    public:
        // Note: No need to inherit from Exchange::IDeviceSettingsFPD anymore
        // DeviceSettingsImp handles the WPEFramework interface contract
        // This class only needs FPD::INotification for hardware callbacks
        
        DeviceSettingsFPDImpl();
        ~DeviceSettingsFPDImpl() override;

        static DeviceSettingsFPDImpl* Create()
        {
            return new DeviceSettingsFPDImpl();
        }

        // We do not allow this plugin to be copied !!
        DeviceSettingsFPDImpl(const DeviceSettingsFPDImpl&)            = delete;
        DeviceSettingsFPDImpl& operator=(const DeviceSettingsFPDImpl&) = delete;

        // INTERFACE_MAP not needed - this is an implementation class aggregated by DeviceSettingsImp
        // DeviceSettingsImp handles QueryInterface for all component interfaces

    public:
        class EXTERNAL LambdaJob : public Core::IDispatch {
        protected:
            LambdaJob(DeviceSettingsFPDImpl* impl, std::function<void()> lambda)
                : _impl(impl)
                , _lambda(std::move(lambda))
            {
            }

        public:
            LambdaJob()                            = delete;
            LambdaJob(const LambdaJob&)            = delete;
            LambdaJob& operator=(const LambdaJob&) = delete;
            ~LambdaJob() {}

            static Core::ProxyType<Core::IDispatch> Create(DeviceSettingsFPDImpl* impl, std::function<void()> lambda)
            {
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<LambdaJob>::Create(impl, std::move(lambda))));
            }

            virtual void Dispatch()
            {
                _lambda();
            }

        private:
            DeviceSettingsFPDImpl* _impl;
            std::function<void()> _lambda;
        };

    public:
        void InitializeIARM();

        // FPD implementation methods - no longer interface methods, just implementation
        // These are called by DeviceSettingsImp which implements the Exchange interface
        Core::hresult Register(Exchange::IDeviceSettingsFPD::INotification* notification);
        Core::hresult Unregister(Exchange::IDeviceSettingsFPD::INotification* notification);
        Core::hresult SetFPDTime(const FPDTimeFormat timeFormat, const uint32_t minutes, const uint32_t seconds);
        Core::hresult SetFPDScroll(const uint32_t scrollHoldDuration, const uint32_t nHorizontalScrollIterations, const uint32_t nVerticalScrollIterations);
        Core::hresult SetFPDBlink(const FPDIndicator indicator, const uint32_t blinkDuration, const uint32_t blinkIterations);
        Core::hresult SetFPDBrightness(const FPDIndicator indicator, const uint32_t brightNess, const bool persist);
        Core::hresult GetFPDBrightness(const FPDIndicator indicator, uint32_t &brightNess);
        Core::hresult SetFPDState(const FPDIndicator indicator, const FPDState state);
        Core::hresult GetFPDState(const FPDIndicator indicator, FPDState &state);
        Core::hresult GetFPDColor(const FPDIndicator indicator, uint32_t &color);
        Core::hresult SetFPDColor(const FPDIndicator indicator, const uint32_t color);
        Core::hresult SetFPDTextBrightness(const FPDTextDisplay textDisplay, const uint32_t brightNess);
        Core::hresult GetFPDTextBrightness(const FPDTextDisplay textDisplay, uint32_t &brightNess);
        Core::hresult EnableFPDClockDisplay(const bool enable);
        Core::hresult GetFPDTimeFormat(FPDTimeFormat &fpdTimeFormat);
        Core::hresult SetFPDTimeFormat(const FPDTimeFormat fpdTimeFormat);
        Core::hresult SetFPDMode(const FPDMode fpdMode);

        private:
        std::list<Exchange::IDeviceSettingsFPD::INotification*> _FPDNotifications;

        // lock to guard all apis of DeviceSettings
        mutable Core::CriticalSection _apiLock;
        // lock to guard all notification from DeviceSettings to clients and also their callback register & unregister
        mutable Core::CriticalSection _callbackLock;

        template <typename T>
        Core::hresult Register(std::list<T*>& list, T* notification);
        template <typename T>
        Core::hresult Unregister(std::list<T*>& list, const T* notification);

        template<typename Func, typename... Args>
        void dispatchFPDEvent(Func notifyFunc, Args&&... args);

        // FPD notification method
        virtual void OnFPDTimeFormatChanged(const FPDTimeFormat timeFormat) override;

        FPD _fpd;
    };
} // namespace Plugin
} // namespace WPEFramework
