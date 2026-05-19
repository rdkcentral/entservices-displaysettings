/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#ifndef USERPLUGIN_USERPLUGIN_H
#define USERPLUGIN_USERPLUGIN_H

#include "Module.h"

#include "tracing/Logging.h"
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include <mutex>

#include <interfaces/IUserPlugin.h>
#include <interfaces/json/JUserPlugin.h>
#include <interfaces/IDeviceSettings.h>
//#include <interfaces/IDeviceSettingsManager.h>
#include <interfaces/IDeviceSettingsAudio.h>
#include <interfaces/IDeviceSettingsCompositeIn.h>
#include <interfaces/IDeviceSettingsDisplay.h>
#include <interfaces/IDeviceSettingsFPD.h>
#include <interfaces/IDeviceSettingsHDMIIn.h>
#include <interfaces/IDeviceSettingsHost.h>
#include <interfaces/IDeviceSettingsVideoDevice.h>
#include <interfaces/IDeviceSettingsVideoPort.h>

//#include "PowerManagerInterface.h"
using namespace WPEFramework::Exchange;
// PowerManager is no longer part of DeviceSettings - remove PowerState typedef
using DeviceSettingsFPD            = WPEFramework::Exchange::IDeviceSettingsFPD;
using DeviceSettingsHDMIIn         = WPEFramework::Exchange::IDeviceSettingsHDMIIn;


namespace WPEFramework
{
    namespace Plugin
    {
        class UserPlugin : public PluginHost::IPlugin, public PluginHost::JSONRPC, public Exchange::IUserPlugin 
        {
        private:
            // PowerManager notification removed - no longer part of DeviceSettings
            
            class HDMIInNotification : public virtual DeviceSettingsHDMIIn::INotification {
            private:
                HDMIInNotification(const HDMIInNotification&) = delete;
                HDMIInNotification& operator=(const HDMIInNotification&) = delete;
            
            public:
                explicit HDMIInNotification(UserPlugin& parent)
                    : _parent(parent)
                {
                }

            public:
                void OnHDMIInEventHotPlug(const DeviceSettingsHDMIIn::HDMIInPort port, const bool isConnected) override
                {
                    _parent.OnHDMIInEventHotPlug(port, isConnected);
                }

                void OnHDMIInEventSignalStatus(const DeviceSettingsHDMIIn::HDMIInPort port, const DeviceSettingsHDMIIn::HDMIInSignalStatus signalStatus) override
                {
                    _parent.OnHDMIInEventSignalStatus(port, signalStatus);
                }

                void OnHDMIInEventStatus(const DeviceSettingsHDMIIn::HDMIInPort activePort, const bool isPresented) override
                {
                    _parent.OnHDMIInEventStatus(activePort, isPresented);
                }

                void OnHDMIInVideoModeUpdate(const DeviceSettingsHDMIIn::HDMIInPort port, const DeviceSettingsHDMIIn::HDMIVideoPortResolution videoPortResolution) override
                {
                    _parent.OnHDMIInVideoModeUpdate(port, videoPortResolution);
                }

                void OnHDMIInAllmStatus(const DeviceSettingsHDMIIn::HDMIInPort port, const bool allmStatus) override
                {
                    _parent.OnHDMIInAllmStatus(port, allmStatus);
                }

                void OnHDMIInAVIContentType(const DeviceSettingsHDMIIn::HDMIInPort port, const DeviceSettingsHDMIIn::HDMIInAviContentType aviContentType) override
                {
                    _parent.OnHDMIInAVIContentType(port, aviContentType);
                }

                void OnHDMIInAVLatency(const int32_t audioDelay, const int32_t videoDelay) override
                {
                    _parent.OnHDMIInAVLatency(audioDelay, videoDelay);
                }

                void OnHDMIInVRRStatus(const DeviceSettingsHDMIIn::HDMIInPort port, const DeviceSettingsHDMIIn::HDMIInVRRType vrrType) override
                {
                    _parent.OnHDMIInVRRStatus(port, vrrType);
                }

                template <typename T>
                T* baseInterface()
                {
                    static_assert(std::is_base_of<T, HDMIInNotification>(), "base type mismatch");
                    return static_cast<T*>(this);
                }

                BEGIN_INTERFACE_MAP(HDMIInNotification)
                INTERFACE_ENTRY(DeviceSettingsHDMIIn::INotification)
                END_INTERFACE_MAP
            
            private:
                UserPlugin& _parent;
            };

            class CompositeInNotification : public virtual Exchange::IDeviceSettingsCompositeIn::INotification {
            private:
                CompositeInNotification(const CompositeInNotification&) = delete;
                CompositeInNotification& operator=(const CompositeInNotification&) = delete;
            
            public:
                explicit CompositeInNotification(UserPlugin& parent)
                    : _parent(parent)
                {
                }

            public:
                void OnCompositeInHotPlug(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const bool isConnected) override
                {
                    _parent.OnCompositeInHotPlug(port, isConnected);
                }

                void OnCompositeInSignalStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus signalStatus) override
                {
                    _parent.OnCompositeInSignalStatus(port, signalStatus);
                }

                void OnCompositeInStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const bool isPresented) override
                {
                    _parent.OnCompositeInStatus(activePort, isPresented);
                }

                void OnCompositeInVideoModeUpdate(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const Exchange::IDeviceSettingsCompositeIn::DisplayVideoPortResolution videoResolution) override
                {
                    _parent.OnCompositeInVideoModeUpdate(activePort, videoResolution);
                }

                template <typename T>
                T* baseInterface()
                {
                    static_assert(std::is_base_of<T, CompositeInNotification>(), "base type mismatch");
                    return static_cast<T*>(this);
                }

                BEGIN_INTERFACE_MAP(CompositeInNotification)
                INTERFACE_ENTRY(Exchange::IDeviceSettingsCompositeIn::INotification)
                END_INTERFACE_MAP
            
            private:
                UserPlugin& _parent;
            };

            class VideoDeviceNotification : public virtual Exchange::IDeviceSettingsVideoDevice::INotification {
            private:
                VideoDeviceNotification(const VideoDeviceNotification&) = delete;
                VideoDeviceNotification& operator=(const VideoDeviceNotification&) = delete;
            
            public:
                explicit VideoDeviceNotification(UserPlugin& parent)
                    : _parent(parent)
                {
                }

            public:
                void OnZoomSettingsChanged(const Exchange::IDeviceSettingsVideoDevice::VideoZoom zoomSetting) override
                {
                    _parent.OnZoomSettingsChanged(zoomSetting);
                }

                void OnDisplayFrameratePreChange(const string frameRate) override
                {
                    _parent.OnDisplayFrameratePreChange(frameRate);
                }

                void OnDisplayFrameratePostChange(const string frameRate) override
                {
                    _parent.OnDisplayFrameratePostChange(frameRate);
                }

                BEGIN_INTERFACE_MAP(VideoDeviceNotification)
                INTERFACE_ENTRY(Exchange::IDeviceSettingsVideoDevice::INotification)
                END_INTERFACE_MAP

            private:
                UserPlugin& _parent;
            };

            class HostNotification : public virtual Exchange::IDeviceSettingsHost::INotification {
            private:
                HostNotification(const HostNotification&) = delete;
                HostNotification& operator=(const HostNotification&) = delete;
            
            public:
                explicit HostNotification(UserPlugin& parent)
                    : _parent(parent)
                {
                }

            public:
                void OnSleepModeChanged(const Exchange::IDeviceSettingsHost::SleepMode sleepMode) override
                {
                    _parent.OnSleepModeChanged(sleepMode);
                }

                BEGIN_INTERFACE_MAP(HostNotification)
                INTERFACE_ENTRY(Exchange::IDeviceSettingsHost::INotification)
                END_INTERFACE_MAP
            
            private:
                UserPlugin& _parent;
            };

            class DisplayNotification : public virtual Exchange::IDeviceSettingsDisplay::INotification, public virtual Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification {
            private:
                DisplayNotification(const DisplayNotification&) = delete;
                DisplayNotification& operator=(const DisplayNotification&) = delete;
            
            public:
                explicit DisplayNotification(UserPlugin& parent)
                    : _parent(parent)
                {
                }

            public:
                void OnDisplayRxSense(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent) override
                {
                    _parent.OnDisplayRxSense(displayEvent);
                }

                void OnDisplayHDCPStatus() override
                {
                    _parent.OnDisplayHDCPStatus();
                }

                void OnDisplayHDMIHotPlug(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent) override
                {
                    _parent.OnDisplayHDMIHotPlug(displayEvent);
                }

                template <typename T>
                T* baseInterface()
                {
                    static_assert(std::is_base_of<T, DisplayNotification>(), "base type mismatch");
                    return static_cast<T*>(this);
                }

                BEGIN_INTERFACE_MAP(DisplayNotification)
                INTERFACE_ENTRY(Exchange::IDeviceSettingsDisplay::INotification)
                INTERFACE_ENTRY(Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification)
                END_INTERFACE_MAP

            private:
                UserPlugin& _parent;
            };

            class VideoPortNotification : public virtual Exchange::IDeviceSettingsVideoPort::INotification {
            private:
                VideoPortNotification(const VideoPortNotification&) = delete;
                VideoPortNotification& operator=(const VideoPortNotification&) = delete;
            
            public:
                explicit VideoPortNotification(UserPlugin& parent)
                    : _parent(parent)
                {
                }

            public:
                void OnResolutionPostChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange resolution) override
                {
                    _parent.OnResolutionPostChange(resolution);
                }

                void OnResolutionPreChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange resolution) override
                {
                    _parent.OnResolutionPreChange(resolution);
                }

                void OnHDCPStatusChange(const Exchange::IDeviceSettingsVideoPort::HDCPStatus hdcpStatus) override
                {
                    _parent.OnHDCPStatusChange(hdcpStatus);
                }

                void OnVideoFormatUpdate(const Exchange::IDeviceSettingsVideoPort::HDRStandard videoFormatHDR) override
                {
                    _parent.OnVideoFormatUpdate(videoFormatHDR);
                }

                BEGIN_INTERFACE_MAP(VideoPortNotification)
                INTERFACE_ENTRY(Exchange::IDeviceSettingsVideoPort::INotification)
                END_INTERFACE_MAP

            private:
                UserPlugin& _parent;
            };

            class AudioNotification : public virtual Exchange::IDeviceSettingsAudio::INotification {
            private:
                AudioNotification(const AudioNotification&) = delete;
                AudioNotification& operator=(const AudioNotification&) = delete;
            
            public:
                explicit AudioNotification(UserPlugin& parent)
                    : _parent(parent)
                {
                }

            public:
                void OnAssociatedAudioMixingChanged(bool mixing) override
                {
                    _parent.OnAssociatedAudioMixingChanged(mixing);
                }

                void OnAudioFaderControlChanged(int32_t mixerBalance) override
                {
                    _parent.OnAudioFaderControlChanged(mixerBalance);
                }

                void OnAudioPrimaryLanguageChanged(const string& primaryLanguage) override
                {
                    _parent.OnAudioPrimaryLanguageChanged(primaryLanguage);
                }

                void OnAudioSecondaryLanguageChanged(const string& secondaryLanguage) override
                {
                    _parent.OnAudioSecondaryLanguageChanged(secondaryLanguage);
                }

                void OnAudioOutHotPlug(Exchange::IDeviceSettingsAudio::AudioPortType portType, uint32_t uiPortNumber, bool isPortConnected) override
                {
                    _parent.OnAudioOutHotPlug(portType, uiPortNumber, isPortConnected);
                }

                void OnAudioFormatUpdate(Exchange::IDeviceSettingsAudio::AudioFormat audioFormat) override
                {
                    _parent.OnAudioFormatUpdate(audioFormat);
                }

                void OnDolbyAtmosCapabilitiesChanged(Exchange::IDeviceSettingsAudio::DolbyAtmosCapability atmosCapability, bool status) override
                {
                    _parent.OnDolbyAtmosCapabilitiesChanged(atmosCapability, status);
                }

                void OnAudioPortStateChanged(Exchange::IDeviceSettingsAudio::AudioPortState audioPortState) override
                {
                    _parent.OnAudioPortStateChanged(audioPortState);
                }

                void OnAudioModeEvent(Exchange::IDeviceSettingsAudio::AudioPortType audioPortType, Exchange::IDeviceSettingsAudio::StereoMode audioMode) override
                {
                    _parent.OnAudioModeEvent(audioPortType, audioMode);
                }

                void OnAudioLevelChangedEvent(int32_t audioLevel) override
                {
                    _parent.OnAudioLevelChangedEvent(audioLevel);
                }

                template <typename T>
                T* baseInterface()
                {
                    static_assert(std::is_base_of<T, AudioNotification>(), "base type mismatch");
                    return static_cast<T*>(this);
                }

                BEGIN_INTERFACE_MAP(AudioNotification)
                INTERFACE_ENTRY(Exchange::IDeviceSettingsAudio::INotification)
                END_INTERFACE_MAP
            
            private:
                UserPlugin& _parent;
            };

        public:
            static UserPlugin* _instance;
            // We do not allow this plugin to be copied !!
            UserPlugin(const UserPlugin &) = delete;
            UserPlugin &operator=(const UserPlugin &) = delete;

            UserPlugin();
            ~UserPlugin() override;

            BEGIN_INTERFACE_MAP(UserPlugin)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_ENTRY(Exchange::IUserPlugin)
            END_INTERFACE_MAP

            // PowerManager methods removed as it's no longer part of DeviceSettings
            Core::hresult GetDevicePowerState(std::string& powerState) const;
            Core::hresult GetVolumeLevel (const string& port, string& level) const;
            void onPowerStateChanged(string currentPowerState, string powerState);
            void TestSimplifiedHDMIInAPIs();
            void TestSimplifiedFPDAPIs();
            void TestSelectHDMIInPortAPI();
            void TestCompositeInAPIs();
            void TestAudioAPIs();
            void TestVideoPortAPIs();
            void TestVideoDeviceAPIs();
            
            // HDMI In Event Handlers
            void OnHDMIInEventHotPlug(const DeviceSettingsHDMIIn::HDMIInPort port, const bool isConnected);
            void OnHDMIInEventSignalStatus(const DeviceSettingsHDMIIn::HDMIInPort port, const DeviceSettingsHDMIIn::HDMIInSignalStatus signalStatus);
            void OnHDMIInEventStatus(const DeviceSettingsHDMIIn::HDMIInPort activePort, const bool isPresented);
            void OnHDMIInVideoModeUpdate(const DeviceSettingsHDMIIn::HDMIInPort port, const DeviceSettingsHDMIIn::HDMIVideoPortResolution videoPortResolution);
            void OnHDMIInAllmStatus(const DeviceSettingsHDMIIn::HDMIInPort port, const bool allmStatus);
            void OnHDMIInAVIContentType(const DeviceSettingsHDMIIn::HDMIInPort port, const DeviceSettingsHDMIIn::HDMIInAviContentType aviContentType);
            void OnHDMIInAVLatency(const int32_t audioDelay, const int32_t videoDelay);
            void OnHDMIInVRRStatus(const DeviceSettingsHDMIIn::HDMIInPort port, const DeviceSettingsHDMIIn::HDMIInVRRType vrrType);

            // CompositeIn Event Handlers
            void OnCompositeInHotPlug(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const bool isConnected);
            void OnCompositeInSignalStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus signalStatus);
            void OnCompositeInStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const bool isPresented);
            void OnCompositeInVideoModeUpdate(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const Exchange::IDeviceSettingsCompositeIn::DisplayVideoPortResolution videoResolution);

            // Audio Event Handlers
            void OnAssociatedAudioMixingChanged(bool mixing);
            void OnAudioFaderControlChanged(int32_t mixerBalance);
            void OnAudioPrimaryLanguageChanged(const string& primaryLanguage);
            void OnAudioSecondaryLanguageChanged(const string& secondaryLanguage);
            void OnAudioOutHotPlug(Exchange::IDeviceSettingsAudio::AudioPortType portType, uint32_t uiPortNumber, bool isPortConnected);
            void OnAudioFormatUpdate(Exchange::IDeviceSettingsAudio::AudioFormat audioFormat);
            void OnDolbyAtmosCapabilitiesChanged(Exchange::IDeviceSettingsAudio::DolbyAtmosCapability atmosCapability, bool status);
            void OnAudioPortStateChanged(Exchange::IDeviceSettingsAudio::AudioPortState audioPortState);
            void OnAudioModeEvent(Exchange::IDeviceSettingsAudio::AudioPortType audioPortType, Exchange::IDeviceSettingsAudio::StereoMode audioMode);
            void OnAudioLevelChangedEvent(int32_t audioLevel);

            // VideoPort Event Handlers
            void OnResolutionPostChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange resolution);
            void OnResolutionPreChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange resolution);
            void OnHDCPStatusChange(const Exchange::IDeviceSettingsVideoPort::HDCPStatus hdcpStatus);
            void OnVideoFormatUpdate(const Exchange::IDeviceSettingsVideoPort::HDRStandard videoFormatHDR);

            // VideoDevice Event Handlers
            void OnZoomSettingsChanged(const Exchange::IDeviceSettingsVideoDevice::VideoZoom zoomSetting);
            void OnDisplayFrameratePreChange(const string frameRate);
            void OnDisplayFrameratePostChange(const string frameRate);

            // Display Event Handlers
            void OnDisplayRxSense(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent);
            void OnDisplayHDCPStatus();
            void OnDisplayHDMIHotPlug(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent);

            // Host Event Handlers
            void OnSleepModeChanged(const Exchange::IDeviceSettingsHost::SleepMode sleepMode);

            // Test cases for Host APIs
            void TestHostAPIs();

            // Test cases for Display APIs
            void TestDisplayAPIs();
            
            // DS HAL Readiness Diagnostic Function  
            bool CheckDSHALReadiness();

            // IARM API methods for direct DsMgr daemon communication
            Core::hresult TestIARMHdmiInSelectPort(const int port, const bool requestAudioMix, const bool topMostPlane, const int videoPlaneType);
            Core::hresult TestIARMHdmiInScaleVideo(const int x, const int y, const int width, const int height);
            Core::hresult TestIARMGetAVLatency();
            Core::hresult TestIARMGetAllmStatus(const int port);
            Core::hresult TestIARMSetEdid2AllmSupport(const int port, const bool allmSupport);
            Core::hresult TestIARMSetEdidVersion(const int port, const int edidVersion);
            Core::hresult TestIARMHdmiInGetCurrentVideoMode();
            Core::hresult TestIARMSetVRRSupport(const int port, const bool vrrSupport);
            Core::hresult TestIARMGetVRRSupport(const int port);
            void TestIARMHdmiInAPIs();
            void TestIARMAdditionalAPIs();

            //  IPlugin methods
            // -------------------------------------------------------------------------------------------------------
            virtual const string Initialize(PluginHost::IShell *service) override;
            virtual void Deinitialize(PluginHost::IShell *service) override;
            virtual string Information() const override;

        private:
            PluginHost::IShell* _service{};
            uint32_t _connectionId{};
            // PowerManager removed - no longer part of DeviceSettings architecture
            Exchange::IDeviceSettings* _deviceSettings;
            Exchange::IDeviceSettingsFPD* _fpdManager;
            Exchange::IDeviceSettingsHDMIIn* _hdmiInManager;
            Exchange::IDeviceSettingsCompositeIn* _compositeInManager;
            Exchange::IDeviceSettingsAudio* _audioManager;
            Exchange::IDeviceSettingsVideoPort* _videoPortManager;
            Exchange::IDeviceSettingsVideoDevice* _videoDeviceManager;
            Exchange::IDeviceSettingsDisplay* _displayManager;
            Exchange::IDeviceSettingsHost* _hostManager;
            //PowerManagerInterfaceRef _powerManager{};
            // PowerManager notification removed
            Core::Sink<HDMIInNotification> _hdmiInNotification;
            Core::Sink<CompositeInNotification> _compositeInNotification;
            Core::Sink<AudioNotification> _audioNotification;
            Core::Sink<VideoPortNotification> _videoPortNotification;
            Core::Sink<VideoDeviceNotification> _videoDeviceNotification;
            Core::Sink<DisplayNotification> _displayNotification;
            Core::Sink<HostNotification> _hostNotification;
            
            // Mutex for thread-safe Display API testing
            mutable std::mutex _displayTestMutex;

	    void Deactivated(RPC::IRemoteConnection *connection);
        };

    } // namespace Plugin
} // namespace WPEFramework

#endif // USERPLUGIN_USERPLUGIN_H
