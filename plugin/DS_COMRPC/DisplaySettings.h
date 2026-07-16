/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
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

#include <map>
#include <mutex>
#include <condition_variable>
#include "Module.h"
#include "tptimer.h"
#include "rfcapi.h"
#include <interfaces/ISystemMode.h>
#include <interfaces/IDeviceOptimizeStateActivator.h>
#include <iostream>
#include <fstream>
#include <interfaces/IPowerManager.h>
#include "PowerManagerInterface.h"

// COM-RPC path: DeviceSettingsInterface.h brings in DSHelper
// (which inherits PluginSmartInterfaceType<IDeviceSettings>) plus all DS
// sub-interface headers.
#include "DeviceSettingsInterface.h"

using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;
namespace WPEFramework {

    namespace Plugin {
		// This is a server for a JSONRPC communication channel.
		// For a plugin to be capable to handle JSONRPC, inherit from PluginHost::JSONRPC.
		// By inheriting from this class, the plugin realizes the interface PluginHost::IDispatcher.
		// This realization of this interface implements, by default, the following methods on this plugin
		// - exists
		// - register
		// - unregister
		// Any other methood to be handled by this plugin  can be added can be added by using the
		// templated methods Register on the PluginHost::JSONRPC class.
		// As the registration/unregistration of notifications is realized by the class PluginHost::JSONRPC,
		// this class exposes a public method called, Notify(), using this methods, all subscribed clients
		// will receive a JSONRPC message as a notification, in case this method is called.
        class DisplaySettings : public PluginHost::IPlugin, public PluginHost::JSONRPC, Exchange::IDeviceOptimizeStateActivator
                                // COM-RPC: single root IDeviceSettings link; sub-interfaces via AcquireSubInterface<T>()
                                , public DSHelper
        {
        private:
            typedef Core::JSON::String JString;
            typedef Core::JSON::ArrayType<JString> JStringArray;
            typedef Core::JSON::Boolean JBool;
            class PowerManagerNotification : public Exchange::IPowerManager::IModeChangedNotification {
            private:
                PowerManagerNotification(const PowerManagerNotification&) = delete;
                PowerManagerNotification& operator=(const PowerManagerNotification&) = delete;
            
            public:
                explicit PowerManagerNotification(DisplaySettings& parent)
                    : _parent(parent)
                {
                }
                ~PowerManagerNotification() override = default;

            public:
                void OnPowerModeChanged(const PowerState currentState, const PowerState newState) override
                {
                    _parent.onPowerModeChanged(currentState, newState);
                }

                template <typename T>
                T* baseInterface()
                {
                    static_assert(std::is_base_of<T, PowerManagerNotification>(), "base type mismatch");
                    return static_cast<T*>(this);
                }

                BEGIN_INTERFACE_MAP(PowerManagerNotification)
                INTERFACE_ENTRY(Exchange::IPowerManager::IModeChangedNotification)
                END_INTERFACE_MAP
            
            private:
                DisplaySettings& _parent;
            };

            // We do not allow this plugin to be copied !!
            DisplaySettings(const DisplaySettings&) = delete;
            DisplaySettings& operator=(const DisplaySettings&) = delete;


            // ----------------------------------------------------------------
            // COM-RPC notification delegate: IDeviceSettingsVideoPort::INotification
            // Bridges COM-RPC resolution/HDCP/video-format events to DisplaySettings.
            // ----------------------------------------------------------------
            class DSVideoPortNotification
                : public Exchange::IDeviceSettingsVideoPort::INotification {
            public:
                explicit DSVideoPortNotification(DisplaySettings& parent) : _parent(parent) {}
                DSVideoPortNotification(const DSVideoPortNotification&) = delete;
                DSVideoPortNotification& operator=(const DSVideoPortNotification&) = delete;

                void OnResolutionPreChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange& /*res*/) override {
                    _parent.OnDSResolutionPreChange();
                }
                void OnResolutionPostChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange& res) override {
                    _parent.OnDSResolutionPostChange(res.width, res.height);
                }
                void OnHDCPStatusChange(const Exchange::IDeviceSettingsVideoPort::HDCPStatus /*hdcpStatus*/) override {}
                void OnVideoFormatUpdate(const Exchange::IDeviceSettingsVideoPort::HDRStandard videoFormatHDR) override {
                    _parent.OnDSVideoFormatUpdate(static_cast<uint32_t>(videoFormatHDR));
                }

                BEGIN_INTERFACE_MAP(DSVideoPortNotification)
                    INTERFACE_ENTRY(Exchange::IDeviceSettingsVideoPort::INotification)
                END_INTERFACE_MAP
            private:
                DisplaySettings& _parent;
            };

            // ----------------------------------------------------------------
            // COM-RPC notification delegate: IDeviceSettingsAudio::INotification
            // Bridges COM-RPC audio events to DisplaySettings.
            // ----------------------------------------------------------------
            class DSAudioNotification
                : public Exchange::IDeviceSettingsAudio::INotification {
            public:
                explicit DSAudioNotification(DisplaySettings& parent) : _parent(parent) {}
                DSAudioNotification(const DSAudioNotification&) = delete;
                DSAudioNotification& operator=(const DSAudioNotification&) = delete;

                void OnAudioOutHotPlug(Exchange::IDeviceSettingsAudio::AudioPortType portType,
                                       uint32_t uiPortNumber, bool isPortConnected) override {
                    _parent.OnDSAudioOutHotPlug(static_cast<int>(portType), uiPortNumber, isPortConnected);
                }
                void OnAudioFormatUpdate(Exchange::IDeviceSettingsAudio::AudioFormat audioFormat) override {
                    _parent.OnDSAudioFormatUpdate(static_cast<uint32_t>(audioFormat));
                }
                void OnDolbyAtmosCapabilitiesChanged(Exchange::IDeviceSettingsAudio::DolbyAtmosCapability atmosCapability,
                                                     bool status) override {
                    _parent.OnDSDolbyAtmosCapabilitiesChanged(static_cast<uint32_t>(atmosCapability), status);
                }
                void OnAudioPortStateChanged(Exchange::IDeviceSettingsAudio::AudioPortState audioPortState) override {
                    _parent.OnDSAudioPortStateChanged(static_cast<uint32_t>(audioPortState));
                }
                void OnAssociatedAudioMixingChanged(bool mixing) override {
                    _parent.OnDSAssociatedAudioMixingChanged(mixing);
                }
                void OnAudioFaderControlChanged(int32_t mixerBalance) override {
                    _parent.OnDSAudioFaderControlChanged(mixerBalance);
                }
                void OnAudioPrimaryLanguageChanged(const string& primaryLanguage) override {
                    _parent.OnDSAudioPrimaryLanguageChanged(primaryLanguage);
                }
                void OnAudioSecondaryLanguageChanged(const string& secondaryLanguage) override {
                    _parent.OnDSAudioSecondaryLanguageChanged(secondaryLanguage);
                }

                BEGIN_INTERFACE_MAP(DSAudioNotification)
                    INTERFACE_ENTRY(Exchange::IDeviceSettingsAudio::INotification)
                END_INTERFACE_MAP
            private:
                DisplaySettings& _parent;
            };

            // ----------------------------------------------------------------
            // COM-RPC notification delegate: IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification
            // Bridges COM-RPC HDMI hot-plug events to DisplaySettings.
            // ----------------------------------------------------------------
            class DSDisplayHotPlugNotification
                : public Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification {
            public:
                explicit DSDisplayHotPlugNotification(DisplaySettings& parent) : _parent(parent) {}
                DSDisplayHotPlugNotification(const DSDisplayHotPlugNotification&) = delete;
                DSDisplayHotPlugNotification& operator=(const DSDisplayHotPlugNotification&) = delete;

                void OnDisplayHDMIHotPlug(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent) override {
                    _parent.OnDSDisplayHDMIHotPlug(static_cast<uint32_t>(displayEvent));
                }

                BEGIN_INTERFACE_MAP(DSDisplayHotPlugNotification)
                    INTERFACE_ENTRY(Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification)
                END_INTERFACE_MAP
            private:
                DisplaySettings& _parent;
            };

            // ----------------------------------------------------------------
            // COM-RPC notification delegate: IDeviceSettingsDisplay::INotification
            // Bridges COM-RPC RxSense / HDCP status events to DisplaySettings.
            // ----------------------------------------------------------------
            class DSDisplayNotification
                : public Exchange::IDeviceSettingsDisplay::INotification {
            public:
                explicit DSDisplayNotification(DisplaySettings& parent) : _parent(parent) {}
                DSDisplayNotification(const DSDisplayNotification&) = delete;
                DSDisplayNotification& operator=(const DSDisplayNotification&) = delete;

                void OnDisplayRxSense(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent) override {
                    _parent.OnDSDisplayRxSense(static_cast<uint32_t>(displayEvent));
                }

                BEGIN_INTERFACE_MAP(DSDisplayNotification)
                    INTERFACE_ENTRY(Exchange::IDeviceSettingsDisplay::INotification)
                END_INTERFACE_MAP
            private:
                DisplaySettings& _parent;
            };

            // ----------------------------------------------------------------
            // COM-RPC notification delegate: IDeviceSettingsVideoDevice::INotification
            // Bridges COM-RPC video-device zoom events to DisplaySettings.
            // ----------------------------------------------------------------
            class DSVideoDeviceNotification
                : public Exchange::IDeviceSettingsVideoDevice::INotification {
            public:
                explicit DSVideoDeviceNotification(DisplaySettings& parent) : _parent(parent) {}
                DSVideoDeviceNotification(const DSVideoDeviceNotification&) = delete;
                DSVideoDeviceNotification& operator=(const DSVideoDeviceNotification&) = delete;

                void OnDisplayFrameratePreChange(const string& /*frameRate*/) override {}
                void OnDisplayFrameratePostChange(const string& /*frameRate*/) override {}
                void OnZoomSettingsChanged(const Exchange::IDeviceSettingsVideoDevice::VideoZoom zoomSetting) override {
                    _parent.OnDSZoomSettingChanged(static_cast<int32_t>(zoomSetting));
                }

                BEGIN_INTERFACE_MAP(DSVideoDeviceNotification)
                    INTERFACE_ENTRY(Exchange::IDeviceSettingsVideoDevice::INotification)
                END_INTERFACE_MAP
            private:
                DisplaySettings& _parent;
            };

            // ----------------------------------------------------------------
            // COM-RPC notification delegate: IDeviceSettingsHDMIIn::INotification
            // Bridges COM-RPC HDMI-In hotplug events to DisplaySettings.
            // DS_IARM equivalent: IHdmiInEvents::OnHdmiInEventHotPlug
            //   (registered for IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG in IarmImpl.cpp).
            // ----------------------------------------------------------------
            class DSHDMIInNotification
                : public Exchange::IDeviceSettingsHDMIIn::INotification {
            public:
                explicit DSHDMIInNotification(DisplaySettings& parent) : _parent(parent) {}
                DSHDMIInNotification(const DSHDMIInNotification&) = delete;
                DSHDMIInNotification& operator=(const DSHDMIInNotification&) = delete;

                void OnHDMIInEventHotPlug(const Exchange::IDeviceSettingsHDMIIn::HDMIInPort port,
                                          const bool isConnected) override {
                    _parent.OnDSHDMIInEventHotPlug(static_cast<int>(port), isConnected);
                }

                BEGIN_INTERFACE_MAP(DSHDMIInNotification)
                    INTERFACE_ENTRY(Exchange::IDeviceSettingsHDMIIn::INotification)
                END_INTERFACE_MAP
            private:
                DisplaySettings& _parent;
            };

            // COM-RPC: notification delegate instance members (initialized with *this in ctor)
            Core::Sink<DSVideoPortNotification>      _DSVideoPortNotification;
            Core::Sink<DSAudioNotification>          _DSAudioNotification;
            Core::Sink<DSDisplayHotPlugNotification> _DSDisplayHotPlugNotification;
            Core::Sink<DSDisplayNotification>        _DSDisplayNotification;
            Core::Sink<DSVideoDeviceNotification>    _DSVideoDeviceNotification;
            Core::Sink<DSHDMIInNotification>         _DSHDMIInNotification;

            void OnDeviceSettingsActivated() override;
            void OnDeviceSettingsDeactivated() override;

            // COM-RPC: private forwarders called from notification delegates
            void OnDSResolutionPreChange();
            void OnDSResolutionPostChange(uint32_t width, uint32_t height);
            void OnDSVideoFormatUpdate(uint32_t videoFormatHDR);
            void OnDSAudioOutHotPlug(int portType, uint32_t portNumber, bool isPortConnected);
            void OnDSAudioFormatUpdate(uint32_t audioFormat);
            void OnDSDolbyAtmosCapabilitiesChanged(uint32_t atmosCapability, bool status);
            void OnDSAudioPortStateChanged(uint32_t audioPortState);
            void OnDSAssociatedAudioMixingChanged(bool mixing);
            void OnDSAudioFaderControlChanged(int32_t mixerBalance);
            void OnDSAudioPrimaryLanguageChanged(const string& primaryLanguage);
            void OnDSAudioSecondaryLanguageChanged(const string& secondaryLanguage);
            void OnDSDisplayHDMIHotPlug(uint32_t displayEvent);
            void OnDSDisplayRxSense(uint32_t displayEvent);
            void OnDSZoomSettingChanged(int32_t zoomSetting);
            void OnDSHDMIInEventHotPlug(int port, bool isConnected);

            //Begin methods
            uint32_t getConnectedVideoDisplays(const JsonObject& parameters, JsonObject& response);
            uint32_t getConnectedAudioPorts(const JsonObject& parameters, JsonObject& response);
	    uint32_t setEnableAudioPort (const JsonObject& parameters, JsonObject& response);
            uint32_t getSupportedResolutions(const JsonObject& parameters, JsonObject& response);
            uint32_t getSupportedVideoDisplays(const JsonObject& parameters, JsonObject& response);
            uint32_t getSupportedTvResolutions(const JsonObject& parameters, JsonObject& response);
            uint32_t getSupportedSettopResolutions(const JsonObject& parameters, JsonObject& response);
            uint32_t getSupportedAudioPorts(const JsonObject& parameters, JsonObject& response);
            uint32_t getSupportedAudioModes(const JsonObject& parameters, JsonObject& response);
            uint32_t getZoomSetting(const JsonObject& parameters, JsonObject& response);
            uint32_t setZoomSetting(const JsonObject& parameters, JsonObject& response);
            uint32_t getCurrentResolution(const JsonObject& parameters, JsonObject& response);
            uint32_t setCurrentResolution(const JsonObject& parameters, JsonObject& response);
            uint32_t getSoundMode(const JsonObject& parameters, JsonObject& response);
            uint32_t setSoundMode(const JsonObject& parameters, JsonObject& response);
            uint32_t readEDID(const JsonObject& parameters, JsonObject& response);
            uint32_t readHostEDID(const JsonObject& parameters, JsonObject& response);
            uint32_t getActiveInput(const JsonObject& parameters, JsonObject& response);
            uint32_t getTvHDRSupport(const JsonObject& parameters, JsonObject& response);
            uint32_t getSettopHDRSupport(const JsonObject& parameters, JsonObject& response);
            uint32_t getCurrentOutputSettings(const JsonObject& parameters, JsonObject& response);
            uint32_t setForceHDRMode(const JsonObject& parameters, JsonObject& response);
            //End methods
            uint32_t setMS12AudioCompression(const JsonObject& parameters, JsonObject& response);
            uint32_t getMS12AudioCompression(const JsonObject& parameters, JsonObject& response);
            uint32_t setDolbyVolumeMode(const JsonObject& parameters, JsonObject& response);
            uint32_t getDolbyVolumeMode(const JsonObject& parameters, JsonObject& response);
            uint32_t setDialogEnhancement(const JsonObject& parameters, JsonObject& response);
            uint32_t getDialogEnhancement(const JsonObject& parameters, JsonObject& response);
            uint32_t setIntelligentEqualizerMode(const JsonObject& parameters, JsonObject& response);
            uint32_t getIntelligentEqualizerMode(const JsonObject& parameters, JsonObject& response);
            uint32_t setGraphicEqualizerMode(const JsonObject& parameters, JsonObject& response);
            uint32_t getGraphicEqualizerMode(const JsonObject& parameters, JsonObject& response);
	    uint32_t setMS12AudioProfile(const JsonObject& parameters, JsonObject& response);
	    uint32_t getMS12AudioProfile(const JsonObject& parameters, JsonObject& response);
	    uint32_t getSupportedMS12AudioProfiles(const JsonObject& parameters, JsonObject& response);
            uint32_t getAudioDelay(const JsonObject& parameters, JsonObject& response);
            uint32_t setAudioDelay(const JsonObject& parameters, JsonObject& response);
            uint32_t getSinkAtmosCapability(const JsonObject& parameters, JsonObject& response);
            uint32_t setAudioAtmosOutputMode(const JsonObject& parameters, JsonObject& response);
            uint32_t getTVHDRCapabilities(const JsonObject& parameters, JsonObject& response);
            uint32_t isConnectedDeviceRepeater(const JsonObject& parameters, JsonObject& response);
            uint32_t getDefaultResolution(const JsonObject& parameters, JsonObject& response);
            uint32_t setScartParameter(const JsonObject& parameters, JsonObject& response);
            uint32_t getVolumeLeveller(const JsonObject& parameters, JsonObject& response);
            uint32_t getBassEnhancer(const JsonObject& parameters, JsonObject& response);
            uint32_t isSurroundDecoderEnabled(const JsonObject& parameters, JsonObject& response);
            uint32_t getDRCMode(const JsonObject& parameters, JsonObject& response);
            uint32_t getSurroundVirtualizer(const JsonObject& parameters, JsonObject& response);
            uint32_t getMISteering(const JsonObject& parameters, JsonObject& response);
            uint32_t setVolumeLeveller(const JsonObject& parameters, JsonObject& response);
            uint32_t setBassEnhancer(const JsonObject& parameters, JsonObject& response);
            uint32_t enableSurroundDecoder(const JsonObject& parameters, JsonObject& response);
            uint32_t setSurroundVirtualizer(const JsonObject& parameters, JsonObject& response);
            uint32_t setMISteering(const JsonObject& parameters, JsonObject& response);
            uint32_t setGain(const JsonObject& parameters, JsonObject& response);
            uint32_t getGain(const JsonObject& parameters, JsonObject& response);
            uint32_t setMuted(const JsonObject& parameters, JsonObject& response);
            uint32_t getMuted(const JsonObject& parameters, JsonObject& response);
            uint32_t setVolumeLevel(const JsonObject& parameters, JsonObject& response);
            uint32_t getVolumeLevel(const JsonObject& parameters, JsonObject& response);
            uint32_t setDRCMode(const JsonObject& parameters, JsonObject& response);
            uint32_t getSettopMS12Capabilities(const JsonObject& parameters, JsonObject& response);
            uint32_t getSettopAudioCapabilities(const JsonObject& parameters, JsonObject& response);
            uint32_t getEnableAudioPort(const JsonObject& parameters, JsonObject& response);

	    uint32_t setAssociatedAudioMixing(const JsonObject& parameters, JsonObject& response);
            uint32_t getAssociatedAudioMixing(const JsonObject& parameters, JsonObject& response);
            uint32_t setFaderControl(const JsonObject& parameters, JsonObject& response);
            uint32_t getFaderControl(const JsonObject& parameters, JsonObject& response);
            uint32_t setPrimaryLanguage(const JsonObject& parameters, JsonObject& response);
            uint32_t getPrimaryLanguage(const JsonObject& parameters, JsonObject& response);
            uint32_t setSecondaryLanguage(const JsonObject& parameters, JsonObject& response);
            uint32_t getSecondaryLanguage(const JsonObject& parameters, JsonObject& response);

	    uint32_t getAudioFormat(const JsonObject& parameters, JsonObject& response);
	    uint32_t getVolumeLeveller2(const JsonObject& parameters, JsonObject& response);
	    uint32_t setVolumeLeveller2(const JsonObject& parameters, JsonObject& response);
	    uint32_t getSurroundVirtualizer2(const JsonObject& parameters, JsonObject& response);
	    uint32_t setSurroundVirtualizer2(const JsonObject& parameters, JsonObject& response);
            uint32_t resetDialogEnhancement(const JsonObject& parameters, JsonObject& response);
            uint32_t resetBassEnhancer(const JsonObject& parameters, JsonObject& response);
            uint32_t resetSurroundVirtualizer(const JsonObject& parameters, JsonObject& response);
            uint32_t resetVolumeLeveller(const JsonObject& parameters, JsonObject& response);
            uint32_t getVideoFormat(const JsonObject& parameters, JsonObject& response);
            uint32_t setMS12ProfileSettingsOverride(const JsonObject& parameters, JsonObject& response);

            uint32_t setPreferredColorDepth(const JsonObject& parameters, JsonObject& response);
            uint32_t getPreferredColorDepth(const JsonObject& parameters, JsonObject& response);
            uint32_t getColorDepthCapabilities(const JsonObject& parameters, JsonObject& response);
	    uint32_t getSupportedMS12Config(const JsonObject& parameters, JsonObject& response);

            uint32_t setAudioDucking(const JsonObject& parameters, JsonObject& response);
            uint32_t setEnableVideoPort(const JsonObject& parameters, JsonObject& response);
            uint32_t getEnableVideoPort(const JsonObject& parameters, JsonObject& response);
            uint32_t getSupportedVideoCodingFormats(const JsonObject& parameters, JsonObject& response);
            uint32_t getVideoCodecInfo(const JsonObject& parameters, JsonObject& response);
            uint32_t getAudioEncoding(const JsonObject& parameters, JsonObject& response);
            uint32_t setAudioEncoding(const JsonObject& parameters, JsonObject& response);
            uint32_t getDisplayAspectRatio(const JsonObject& parameters, JsonObject& response);

            void InitAudioPorts();
            void AudioPortsReInitialize();
            static void initAudioPortsWorker(void);
            //End methods

            //Begin events
            void resolutionPreChange();
            void resolutionChanged(int width, int height);
            void zoomSettingUpdated(const string& zoomSetting);
            void activeInputChanged(bool activeInput);
            void connectedVideoDisplaysUpdated(int hdmiHotPlugEvent);
            void connectedAudioPortUpdated (int iAudioPortType, bool isPortConnected);
	    void notifyAudioFormatChange(
                uint32_t audioFormat
                );
		void notifyAtmosCapabilityChange(
                uint32_t atmoCaps
                );
            void notifyAssociatedAudioMixingChange(bool mixing);
            void notifyFaderControlChange(bool mixerbalance);
            void notifyPrimaryLanguageChange(std::string pLang);
            void notifySecondaryLanguageChange(std::string sLang);
	    void notifyVideoFormatChange(
                uint32_t videoFormat
                );
	    void onARCInitiationEventHandler(const JsonObject& parameters);
            void onARCTerminationEventHandler(const JsonObject& parameters);
	    void onShortAudioDescriptorEventHandler(const JsonObject& parameters);
	    void onSystemAudioModeEventHandler(const JsonObject& parameters);
            void onArcAudioStatusEventHandler(const JsonObject& parameters);
	    void onAudioDeviceConnectedStatusEventHandler(const JsonObject& parameters);
	    void onCecEnabledEventHandler(const JsonObject& parameters);
            void onAudioDevicePowerStatusEventHandler(const JsonObject& parameters);
	    bool isDisplayConnected (std::string port);
            //End events
        public:
            DisplaySettings();
            virtual ~DisplaySettings();
            //IPlugin methods
            virtual const string Initialize(PluginHost::IShell* service) override;
            virtual void Deinitialize(PluginHost::IShell* service) override;
            virtual string Information() const override { return {}; }
            void onPowerModeChanged(const PowerState currentState, const PowerState newState);
            void registerEventHandlers();
            BEGIN_INTERFACE_MAP(DisplaySettings)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
	    INTERFACE_ENTRY(Exchange::IDeviceOptimizeStateActivator)
            END_INTERFACE_MAP

	    Core::hresult Request(const string& newState);

        private:
            void getConnectedVideoDisplaysHelper(std::vector<string>& connectedDisplays);
            void audioFormatToString(uint32_t audioFormat, JsonObject &response);
            const char *getVideoFormatTypeToString(uint32_t format);
            uint32_t getVideoFormatTypeFromString(const char *mode);
            JsonArray getSupportedVideoFormats();
            bool checkPortName(std::string& name) const;
            PowerState getSystemPowerState();

	    void getHdmiCecSinkPlugin();
	    WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>* m_client;
	    std::vector<std::string> m_clientRegisteredEventNames;
	    uint32_t subscribeForHdmiCecSinkEvent(const char* eventName);
	    bool setUpHdmiCecSinkArcRouting (bool arcEnable);
	    bool requestShortAudioDescriptor();
            bool requestAudioDevicePowerStatus();
            bool requestDeviceAudioStatus();
	    bool sendUserControlPressCommand(int keyCode);
	    bool sendHdmiCecSinkAudioDevicePowerOn();
	    bool getHdmiCecSinkCecEnableStatus();
	    bool getHdmiCecSinkAudioDeviceConnectedStatus();
        int getAudioDeviceSADState(void);
        void setAudioDeviceSADState(int newState);
        int getCurrentArcRoutingState(void);

	    void onTimer();
	    void stopCecTimeAndUnsubscribeEvent();
            void checkAudioDeviceDetectionTimer();
	    void checkArcDeviceConnected();
	    void checkSADUpdate();
	    void checkAudioDevicePowerStatusTimer();

	    TpTimer m_timer;
            TpTimer m_AudioDeviceDetectTimer;
	    TpTimer m_SADDetectionTimer;
	    TpTimer m_ArcDetectionTimer;
	    TpTimer m_AudioDevicePowerOnStatusTimer;
            bool m_subscribed;
            std::mutex m_callMutex;
            std::mutex m_SadMutex;
	    std::thread m_arcRoutingThread;
	    std::mutex m_AudioDeviceStatesUpdateMutex;
	    bool m_cecArcRoutingThreadRun; 
	    std::condition_variable arcRoutingCV;
	    bool m_hdmiInAudioDeviceConnected;
            bool m_arcEarcAudioEnabled;
	    bool m_arcEarcConnectionNotifiedToUI;
            bool m_arcPendingSADRequest;   
	    bool m_hdmiCecAudioDeviceDetected;
	    bool m_systemAudioMode_Power_RequestedAndReceived;
            int32_t m_hdmiInAudioDeviceType { 0 };  ///< maps to dsAudioARCTypes_t, 0 = NONE
	    JsonObject m_audioOutputPortConfig;
        PowerManagerInterfaceRef _powerManagerPlugin;
        Core::Sink<PowerManagerNotification> _pwrMgrNotification;
        bool _registeredEventHandlers;
        void InitializePowerManager();
            JsonObject getAudioOutputPortConfig() { return m_audioOutputPortConfig; }
            static PowerState m_powerState;

    private:
        bool _registeredDsEventHandlers;

    public:
        // COM-RPC path: DS events arrive via delegates above; register them in OnDeviceSettingsActivated()
        void registerDsEventHandlers();  // no-op stub — actual registration done in OnDeviceSettingsActivated()

            enum {
                ARC_STATE_REQUEST_ARC_INITIATION,
                ARC_STATE_ARC_INITIATED,
                ARC_STATE_REQUEST_ARC_TERMINATION,
                ARC_STATE_ARC_TERMINATED,
                ARC_STATE_ARC_EXIT
            };

            enum {
                AUDIO_DEVICE_POWER_STATE_UNKNOWN,
                AUDIO_DEVICE_POWER_STATE_REQUEST,
                AUDIO_DEVICE_POWER_STATE_STANDBY,
                AUDIO_DEVICE_POWER_STATE_ON,
            };

	    enum {
		AUDIO_DEVICE_SAD_UNKNOWN,
		AUDIO_DEVICE_SAD_REQUESTED,
		AUDIO_DEVICE_SAD_RECEIVED,
		AUDIO_DEVICE_SAD_UPDATED,
		AUDIO_DEVICE_SAD_CLEARED
	    };

	    enum {
		AVR_POWER_STATE_ON,
		AVR_POWER_STATE_STANDBY,
		AVR_POWER_STATE_STANDBY_TO_ON_TRANSITION
	    };

	    enum {
		ARC_EARC_DISCONNECTED,
		ARC_EARC_CONNECTED,
	    };

           typedef enum {
		SEND_AUDIO_DEVICE_POWERON_MSG = 1,
		REQUEST_SHORT_AUDIO_DESCRIPTOR,
		REQUEST_AUDIO_DEVICE_POWER_STATUS,
		SEND_DEVICE_AUDIO_STATUS,
		SEND_MUTE_KEY_EVENT,
		SEND_REQUEST_ARC_INITIATION,
		SEND_REQUEST_ARC_TERMINATION,
		} msg_t;

	   typedef struct sendMsgInfo {
                   int msg;
                   void *param;
                } SendMsgInfo;

	    void sendMsgToQueue(msg_t msg, void *param);
            bool m_sendMsgThreadExit;
            bool m_sendMsgThreadRun;

	    static void  sendMsgThread();
            std::thread m_sendMsgThread;
            std::mutex m_sendMsgMutex;
	    std::queue<SendMsgInfo> m_sendMsgQueue;
            std::condition_variable m_sendMsgCV;

            int m_hdmiInAudioDevicePowerState;
            int m_currentArcRoutingState;
            int m_AudioDeviceSADState;
	    bool m_requestSad;
	    bool m_requestSadRetrigger;
            PluginHost::IShell* m_service = nullptr;

        public:
            static DisplaySettings* _instance;

	private: 
	    mutable Core::CriticalSection _adminLock;

        };
	} // namespace Plugin
} // namespace WPEFramework

