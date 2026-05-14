/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#pragma once

#include "Module.h"

//#include <interfaces/IDeviceSettingsManager.h>
#include <interfaces/IDeviceSettingsAudio.h>
#include <interfaces/IDeviceSettingsCompositeIn.h>
#include <interfaces/IDeviceSettingsDisplay.h>
#include <interfaces/IDeviceSettingsFPD.h>
#include <interfaces/IDeviceSettingsHDMIIn.h>
#include <interfaces/IDeviceSettingsHost.h>
//#include <interfaces/IDeviceSettingsManager.h>
#include <interfaces/IDeviceSettingsVideoDevice.h>
#include <interfaces/IDeviceSettingsVideoPort.h>

#include "UtilsLogging.h"
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>

#include "DeviceSettingsTypes.h"


namespace WPEFramework {
namespace Plugin {

    class DeviceSettings : public PluginHost::IPlugin
    {
    private:
        class NotificationHandler : public RPC::IRemoteConnection::INotification
                                  , public PluginHost::IShell::ICOMLink::INotification
                                  , public DeviceSettingsCompositeIn::INotification
                                  , public DeviceSettingsAudio::INotification
                                  , public DeviceSettingsFPD::INotification
                                  , public DeviceSettingsDisplay::INotification
                                  , public DeviceSettingsHDMIIn::INotification
                                  , public DeviceSettingsHost::INotification
                                  , public DeviceSettingsVideoPort::INotification
                                  , public DeviceSettingsVideoDevice::INotification
            {
        private:
            NotificationHandler()                                      = delete;
            NotificationHandler(const NotificationHandler&)            = delete;
            NotificationHandler& operator=(const NotificationHandler&) = delete;

        public:
            explicit NotificationHandler(DeviceSettings* parent)
                : mParent(*parent)
            {
                ASSERT(parent != nullptr);
            }

            virtual ~NotificationHandler()
            {
            }

            template <typename T>
            T* baseInterface()
            {
                static_assert(std::is_base_of<T, NotificationHandler>(), "base type mismatch");
                return static_cast<T*>(this);
            }

            BEGIN_INTERFACE_MAP(NotificationHandler)
            INTERFACE_ENTRY(DeviceSettingsCompositeIn::INotification)
            INTERFACE_ENTRY(DeviceSettingsAudio::INotification)
            INTERFACE_ENTRY(DeviceSettingsFPD::INotification)
            INTERFACE_ENTRY(DeviceSettingsDisplay::INotification)
            INTERFACE_ENTRY(DeviceSettingsHDMIIn::INotification)
            INTERFACE_ENTRY(DeviceSettingsHost::INotification)
            INTERFACE_ENTRY(DeviceSettingsVideoPort::INotification)
            INTERFACE_ENTRY(DeviceSettingsVideoDevice::INotification)
            INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
            END_INTERFACE_MAP

            void Activated(RPC::IRemoteConnection*) override
            {
            }

            void Deactivated(RPC::IRemoteConnection* connection) override
            {
                mParent.Deactivated(connection);
            }

            void Dangling(const Core::IUnknown* remote, const uint32_t interfaceId) override
            {
                ASSERT(remote != nullptr);
                mParent.CallbackRevoked(remote, interfaceId);
            }

            void Revoked(const Core::IUnknown* remote, const uint32_t interfaceId) override
            {
                ASSERT(remote != nullptr);
                mParent.CallbackRevoked(remote, interfaceId);
            }

            void OnFPDTimeFormatChanged(const FPDTimeFormat timeFormat) override
            {
                LOGINFO("OnFPDTimeFormatChanged: timeFormat %d", timeFormat);
            }

            // Audio notification handlers
            void OnAssociatedAudioMixingChanged(bool mixing) override
            {
                LOGINFO("OnAssociatedAudioMixingChanged: mixing %d", mixing);
            }

            void OnAudioFaderControlChanged(int32_t mixerBalance) override
            {
                LOGINFO("OnAudioFaderControlChanged: mixerBalance %d", mixerBalance);
            }

            void OnAudioPrimaryLanguageChanged(const string& primaryLanguage) override
            {
                LOGINFO("OnAudioPrimaryLanguageChanged: primaryLanguage %s", primaryLanguage.c_str());
            }

            void OnAudioSecondaryLanguageChanged(const string& secondaryLanguage) override
            {
                LOGINFO("OnAudioSecondaryLanguageChanged: secondaryLanguage %s", secondaryLanguage.c_str());
            }

            void OnAudioOutHotPlug(AudioPortType portType, uint32_t uiPortNumber, bool isPortConnected) override
            {
                LOGINFO("OnAudioOutHotPlug: portType %d, port %d, connected %d", portType, uiPortNumber, isPortConnected);
            }

            void OnAudioFormatUpdate(AudioFormat audioFormat) override
            {
                LOGINFO("OnAudioFormatUpdate: audioFormat %d", audioFormat);
            }

            void OnDolbyAtmosCapabilitiesChanged(DolbyAtmosCapability atmosCapability, bool status) override
            {
                LOGINFO("OnDolbyAtmosCapabilitiesChanged: capability %d, status %d", atmosCapability, status);
            }

            void OnAudioPortStateChanged(AudioPortState audioPortState) override
            {
                LOGINFO("OnAudioPortStateChanged: state %d", audioPortState);
            }

            void OnAudioLevelChangedEvent(int32_t audioLevel) override
            {
                LOGINFO("OnAudioLevelChangedEvent: level %d", audioLevel);
            }

            void OnAudioModeEvent(AudioPortType audioPortType, AudioStereoMode audioMode) override
            {
                LOGINFO("OnAudioModeEvent: portType %d, mode %d", audioPortType, audioMode);
            }

            void OnHDMIInEventHotPlug(const HDMIInPort port, const bool isConnected) override 
            {
                LOGINFO("OnHDMIInEventHotPlug:");
            }

            void OnHDMIInEventSignalStatus(const HDMIInPort port, const HDMIInSignalStatus signalStatus) override
            {
                LOGINFO("OnHDMIInEventSignalStatus");
            }

            void OnHDMIInEventStatus(const HDMIInPort activePort, const bool isPresented) override
            {
                LOGINFO("OnHDMIInEventStatus");
            }

            void OnHDMIInVideoModeUpdate(const HDMIInPort port, const HDMIVideoPortResolution videoPortResolution) override
            {
                LOGINFO("OnHDMIInVideoModeUpdate");
            }

            void OnHDMIInAllmStatus(const HDMIInPort port, const bool allmStatus) override
            {
                LOGINFO("OnHDMIInAllmStatus");
            }

            void OnHDMIInAVIContentType(const HDMIInPort port, const HDMIInAviContentType aviContentType) override
            {
                LOGINFO("OnHDMIInAVIContentType");
            }

            void OnHDMIInAVLatency(const int32_t audioDelay, const int32_t videoDelay) override
            {
                LOGINFO("OnHDMIInAVLatency");
            }

            void OnHDMIInVRRStatus(const HDMIInPort port, const HDMIInVRRType vrrType) override
            {
                LOGINFO("OnHDMIInVRRStatus");
            }

            // VideoPort notification handlers matching WPE interface
            void OnResolutionPostChange(const ResolutionChange resolution) override
            {
                LOGINFO("OnResolutionPostChange");
            }

            void OnResolutionPreChange(const ResolutionChange resolution) override
            {
                LOGINFO("OnResolutionPreChange");
            }

            void OnHDCPStatusChange(const Exchange::IDeviceSettingsVideoPort::HDCPStatus hdcpStatus) override
            {
                LOGINFO("OnHDCPStatusChange: status=%d", (int)hdcpStatus);
            }

            void OnVideoFormatUpdate(const Exchange::IDeviceSettingsVideoPort::HDRStandard videoFormatHDR) override
            {
                LOGINFO("OnVideoFormatUpdate: hdrStandard=%d", (int)videoFormatHDR);
            }

            // CompositeIn notification handlers matching WPE interface
            void OnCompositeInHotPlug(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const bool isConnected) override
            {
                LOGINFO("OnCompositeInHotPlug: port=%d, isConnected=%s", (int)port, isConnected ? "true" : "false");
            }

            void OnCompositeInSignalStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus signalStatus) override
            {
                LOGINFO("OnCompositeInSignalStatus: port=%d, signalStatus=%d", (int)port, (int)signalStatus);
            }

            void OnCompositeInStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const bool isPresented) override
            {
                LOGINFO("OnCompositeInStatus: activePort=%d, isPresented=%s", (int)activePort, isPresented ? "true" : "false");
            }

            void OnCompositeInVideoModeUpdate(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const Exchange::IDeviceSettingsCompositeIn::DisplayVideoPortResolution videoResolution) override
            {
                LOGINFO("OnCompositeInVideoModeUpdate: activePort=%d, resolution=%s", (int)activePort, videoResolution.name.c_str());
            }

            // VideoDevice event handlers (matching actual IDeviceSettingsVideoDevice::INotification interface)
            void OnZoomSettingsChanged(const Exchange::IDeviceSettingsVideoDevice::VideoZoom zoomSetting) override
            {
                LOGINFO("OnZoomSettingsChanged: zoomSetting=%d", static_cast<int>(zoomSetting));
            }

            void OnDisplayFrameratePreChange(const string frameRate) override
            {
                LOGINFO("OnDisplayFrameratePreChange: frameRate=%s", frameRate.c_str());
            }

            void OnDisplayFrameratePostChange(const string frameRate) override
            {
                LOGINFO("OnDisplayFrameratePostChange: frameRate=%s", frameRate.c_str());
            }

            // Host notification handlers
            void OnSleepModeChanged(const Exchange::IDeviceSettingsHost::SleepMode sleepMode) override
            {
                LOGINFO("OnSleepModeChanged: sleepMode=%d", static_cast<int>(sleepMode));
            }

        private:
            DeviceSettings& mParent;
        };
    public:
        DeviceSettings(const DeviceSettings&) = delete;
        DeviceSettings(DeviceSettings&&) = delete;
        DeviceSettings& operator=(const DeviceSettings&) = delete;
        DeviceSettings& operator=(DeviceSettings&) = delete;

        DeviceSettings();
        virtual ~DeviceSettings();

        // Build QueryInterface implementation, specifying all possible interfaces to be returned.
        BEGIN_INTERFACE_MAP(DeviceSettings)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettings, _mDeviceSettings)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettingsCompositeIn, _mDeviceSettingsCompositeIn)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettingsAudio, _mDeviceSettingsAudio)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettingsFPD, _mDeviceSettingsFPD)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettingsDisplay, _mDeviceSettingsDisplay)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettingsHDMIIn, _mDeviceSettingsHDMIIn)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettingsHost, _mDeviceSettingsHost)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettingsVideoPort, _mDeviceSettingsVideoPort)
            INTERFACE_AGGREGATE(Exchange::IDeviceSettingsVideoDevice, _mDeviceSettingsVideoDevice)
        END_INTERFACE_MAP

    public:

        //   IPlugin methods
        // -------------------------------------------------------------------------------------------------------
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;

    private:
        void Deactivated(RPC::IRemoteConnection* connection);
        void CallbackRevoked(const Core::IUnknown* remote, const uint32_t interfaceId);

    private:
        uint32_t mConnectionId;
        PluginHost::IShell* mService;
        Exchange::IDeviceSettings* _mDeviceSettings;
        Exchange::IDeviceSettingsCompositeIn* _mDeviceSettingsCompositeIn;
        DeviceSettingsAudio* _mDeviceSettingsAudio;
        Exchange::IDeviceSettingsFPD* _mDeviceSettingsFPD;
        Exchange::IDeviceSettingsDisplay* _mDeviceSettingsDisplay;
        Exchange::IDeviceSettingsHDMIIn* _mDeviceSettingsHDMIIn;
        Exchange::IDeviceSettingsHost* _mDeviceSettingsHost;
        Exchange::IDeviceSettingsVideoPort* _mDeviceSettingsVideoPort;
        Exchange::IDeviceSettingsVideoDevice* _mDeviceSettingsVideoDevice;
        Core::Sink<NotificationHandler> mNotificationSink;

    };

} // namespace Plugin
} // namespace WPEFramework
