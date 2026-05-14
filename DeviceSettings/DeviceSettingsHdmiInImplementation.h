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
#include <interfaces/IDeviceSettingsHDMIIn.h>  // For IDeviceSettingsHDMIIn::INotification

#include "fpd.h"
#include "HdmiIn.h"

#include "list.hpp"
#include "DeviceSettingsTypes.h"

namespace WPEFramework {
namespace Plugin {
    class DeviceSettingsHdmiInImp : public HdmiIn::INotification
    {
    public:
        // Note: No need to inherit from Exchange::IDeviceSettingsHDMIIn anymore
        // DeviceSettingsImp handles the WPEFramework interface contract
        // This class only needs HdmiIn::INotification for hardware callbacks
        
        DeviceSettingsHdmiInImp();
        ~DeviceSettingsHdmiInImp() override;

        static DeviceSettingsHdmiInImp* Create()
        {
            return new DeviceSettingsHdmiInImp();
        }

        // We do not allow this plugin to be copied !!
        DeviceSettingsHdmiInImp(const DeviceSettingsHdmiInImp&)            = delete;
        DeviceSettingsHdmiInImp& operator=(const DeviceSettingsHdmiInImp&) = delete;

        // INTERFACE_MAP not needed - this is an implementation class aggregated by DeviceSettingsImp
        // DeviceSettingsImp handles QueryInterface for all component interfaces

    public: 
        class EXTERNAL LambdaJob : public Core::IDispatch {
        protected:
            LambdaJob(DeviceSettingsHdmiInImp* impl, std::function<void()> lambda)
                : _impl(impl)
                , _lambda(std::move(lambda))
            {
            }

        public:
            LambdaJob()                            = delete;
            LambdaJob(const LambdaJob&)            = delete;
            LambdaJob& operator=(const LambdaJob&) = delete;
            ~LambdaJob() {}

            static Core::ProxyType<Core::IDispatch> Create(DeviceSettingsHdmiInImp* impl, std::function<void()> lambda)
            {
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<LambdaJob>::Create(impl, std::move(lambda))));
            }

            virtual void Dispatch()
            {
                _lambda();
            }

        private:
            DeviceSettingsHdmiInImp* _impl;
            std::function<void()> _lambda;
        };

    public:
        void InitializeIARM();

        // HDMIIn implementation methods - no longer interface methods, just implementation
        // These are called by DeviceSettingsImp which implements the Exchange interface
        Core::hresult Register(Exchange::IDeviceSettingsHDMIIn::INotification* notification);
        Core::hresult Unregister(Exchange::IDeviceSettingsHDMIIn::INotification* notification);
        Core::hresult GetHDMIInNumbefOfInputs(int32_t &count);
        Core::hresult GetHDMIInStatus(HDMIInStatus &hdmiStatus, IHDMIInPortConnectionStatusIterator*& portConnectionStatus);
        Core::hresult SelectHDMIInPort(const HDMIInPort port, const bool requestAudioMix, const bool topMostPlane, const HDMIVideoPlaneType videoPlaneType);
        Core::hresult ScaleHDMIInVideo(const HDMIInVideoRectangle videoPosition);
        Core::hresult SelectHDMIZoomMode(const HDMIInVideoZoom zoomMode);
        Core::hresult GetSupportedGameFeaturesList(IHDMIInGameFeatureListIterator *& gameFeatureList);
        Core::hresult GetHDMIInAVLatency(uint32_t &videoLatency, uint32_t &audioLatency);
        Core::hresult GetHDMIInAllmStatus(const HDMIInPort port, bool &allmStatus);
        Core::hresult GetHDMIInEdid2AllmSupport(const HDMIInPort port, bool &allmSupport);
        Core::hresult SetHDMIInEdid2AllmSupport(const HDMIInPort port, bool allmSupport);
        Core::hresult GetEdidBytes(const HDMIInPort port, const uint16_t edidBytesLength, uint8_t edidBytes[]);
        Core::hresult GetHDMISPDInformation(const HDMIInPort port, const uint16_t spdBytesLength, uint8_t spdBytes[]);
        Core::hresult GetHDMIEdidVersion(const HDMIInPort port, HDMIInEdidVersion &edidVersion);
        Core::hresult SetHDMIEdidVersion(const HDMIInPort port, const HDMIInEdidVersion edidVersion);
        Core::hresult GetHDMIVideoMode(HDMIVideoPortResolution &videoPortResolution);
        Core::hresult GetHDMIVersion(const HDMIInPort port, HDMIInCapabilityVersion &capabilityVersion);
        Core::hresult SetVRRSupport(const HDMIInPort port, const bool vrrSupport);
        Core::hresult GetVRRSupport(const HDMIInPort port, bool &vrrSupport);
        Core::hresult GetVRRStatus(const HDMIInPort port, HDMIInVRRStatus &vrrStatus);

        private:
        std::list<Exchange::IDeviceSettingsHDMIIn::INotification*> _HDMIInNotifications;

        // lock to guard all apis of DeviceSettings
        mutable Core::CriticalSection _apiLock;
        // lock to guard all notification from DeviceSettings to clients and also their callback register & unregister
        mutable Core::CriticalSection _callbackLock;

        template <typename T>
        Core::hresult Register(std::list<T*>& list, T* notification);
        template <typename T>
        Core::hresult Unregister(std::list<T*>& list, const T* notification);

        template<typename Func, typename... Args>
        void dispatchHDMIInEvent(Func notifyFunc, Args&&... args);

        virtual void OnHDMIInEventHotPlugNotification(const HDMIInPort port, const bool isConnected) override;
        virtual void OnHDMIInEventSignalStatusNotification(const HDMIInPort port, const HDMIInSignalStatus signalStatus) override;
        virtual void OnHDMIInEventStatusNotification(const HDMIInPort activePort, const bool isPresented) override;
        virtual void OnHDMIInVideoModeUpdateNotification(const HDMIInPort port, const HDMIVideoPortResolution videoPortResolution) override;
        virtual void OnHDMIInAllmStatusNotification(const HDMIInPort port, const bool allmStatus) override;
        virtual void OnHDMIInAVIContentTypeNotification(const HDMIInPort port, const HDMIInAviContentType aviContentType) override;
        virtual void OnHDMIInAVLatencyNotification(const int32_t audioDelay, const int32_t videoDelay) override;
        virtual void OnHDMIInVRRStatusNotification(const HDMIInPort port, const HDMIInVRRType vrrType) override;

        HdmiIn _hdmiIn;
    };
} // namespace Plugin
} // namespace WPEFramework
