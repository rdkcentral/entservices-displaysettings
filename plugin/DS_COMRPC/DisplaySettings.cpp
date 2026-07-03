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

//I have put several "TODO(MROLLINS)" in the code below to mark areas of concern I encountered
//  when refactoring the servicemanager's version of displaysettings into this new thunder plugin format

#include "DisplaySettings.h"
#include <algorithm>
#include <cmath>
#include "UtilsSynchro.hpp"
#include <map>
#include "tr181api.h"

#include "tracing/Logging.h"
#include <syscall.h>
#include "UtilsCStr.h"
#include "UtilsJsonRpc.h"
#include "UtilsString.h"
#include "UtilsisValidInt.h"


using namespace std;

#define HDMI_HOT_PLUG_EVENT_CONNECTED 0

#define HDMICECSINK_CALLSIGN "org.rdk.HdmiCecSink"
#define HDMICECSINK_CALLSIGN_VER HDMICECSINK_CALLSIGN".1"
#define HDMICECSINK_ARC_INITIATION_EVENT "arcInitiationEvent"
#define HDMICECSINK_ARC_TERMINATION_EVENT "arcTerminationEvent"
#define HDMICECSINK_ARC_AUDIO_STATUS_EVENT "reportAudioStatusEvent"
#define HDMICECSINK_SHORT_AUDIO_DESCRIPTOR_EVENT "shortAudiodescriptorEvent"
#define HDMICECSINK_SYSTEM_AUDIO_MODE_EVENT "setSystemAudioModeEvent"
#define HDMICECSINK_AUDIO_DEVICE_CONNECTED_STATUS_EVENT "reportAudioDeviceConnectedStatus"
#define HDMICECSINK_CEC_ENABLED_EVENT "reportCecEnabledEvent"
#define HDMICECSINK_AUDIO_DEVICE_POWER_STATUS_EVENT "reportAudioDevicePowerStatus"
#define SERVER_DETAILS  "127.0.0.1:9998"
#define WARMING_UP_TIME_IN_SECONDS 5
#define HDMICECSINK_PLUGIN_ACTIVATION_TIME 2
#define RECONNECTION_TIME_IN_MILLISECONDS 5500
#define AUDIO_DEVICE_CONNECTION_CHECK_TIME_IN_MILLISECONDS 3000
#define SAD_UPDATE_CHECK_TIME_IN_MILLISECONDS 3000
#define ARC_DETECTION_CHECK_TIME_IN_MILLISECONDS 1000
#define AUDIO_DEVICE_POWER_TRANSITION_TIME_IN_MILLISECONDS 1000

#define RFC_PWRMGR2 "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Power.PwrMgr2.Enable"

#define ZOOM_SETTINGS_FILE      "/opt/persistent/rdkservices/zoomSettings.json"
#define ZOOM_SETTINGS_DIRECTORY "/opt/persistent/rdkservices"

#define API_VERSION_NUMBER_MAJOR 2
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 5

static bool isCecEnabled = false;
static bool isResCacheUpdated = false;
static std::string currentResolutionCache;
static bool isDisplayConnectedCacheUpdated = false;
static bool isHdmiDisplayConnected = false;
int stbHDRcapabilitiesCache = 0;
bool isStbHDRcapabilitiesCache = false;
static int  hdmiArcPortId = -1;
static int retryPowerRequestCount = 0;
static int  hdmiArcVolumeLevel = 0;
static bool hdmiArcMuteStatus = false;
bool audioPortInitActive = false;
std::vector<int> sad_list;

static std::map<std::string, bool> audioPortEnableStatusMap;

using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;

// TODO: remove this
#define registerMethod(...) for (uint8_t i = 1; GetHandler(i); i++) GetHandler(i)->Register<JsonObject, JsonObject>(__VA_ARGS__)
#define registerMethodLockedApi(...) for (uint8_t i = 1; GetHandler(i); i++) Utils::Synchro::RegisterLockedApiForHandler(GetHandler(i), __VA_ARGS__)

namespace WPEFramework {

    namespace {

        static Plugin::Metadata<Plugin::DisplaySettings> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin {

        namespace {
            // Display Settings should use inter faces
#ifndef USE_THUNDER_R4
            class Job : public Core::IDispatchType<void> {
#else
            class Job : public Core::IDispatch {
#endif /* USE_THUNDER_R4 */
            public:
                Job(std::function<void()> work)
                    : _work(work)
                {
                }
                void Dispatch() override
                {
                    _work();
                }

            private:
                std::function<void()> _work;
            };
            uint32_t getServiceState(PluginHost::IShell* shell, const string& callsign, PluginHost::IShell::state& state)
            {
                uint32_t result;
                auto interface = shell->QueryInterfaceByCallsign<PluginHost::IShell>(callsign);
                if (interface == nullptr) {
                    result = Core::ERROR_UNAVAILABLE;
                    std::cout << "no IShell for " << callsign << std::endl;
                } else {
                    result = Core::ERROR_NONE;
                    state = interface->State();
                    std::cout << "IShell state " << state << " for " << callsign << std::endl;
                    interface->Release();
                }
                return result;
            }

            bool TryParseIntInRange(const string& value, const int minValue, const int maxValue, int& parsedValue)
            {
                try {
                    if (value.empty()) {
                        return false;
                    }

                    char* endPtr = nullptr;
                    errno = 0;
                    const long longValue = strtol(value.c_str(), &endPtr, 10);
                    if ((errno == ERANGE) || (endPtr == value.c_str()) || (*endPtr != '\0')) {
                        return false;
                    }
                    if ((longValue < minValue) || (longValue > maxValue)) {
                        return false;
                    }

                    parsedValue = static_cast<int>(longValue);
                    return true;
                } catch (const std::exception& err) {
                    LOGERR("Exception in TryParseIntInRange: %s", err.what());
                    return false;
                } catch (...) {
                    LOGERR("Unknown exception in TryParseIntInRange");
                    return false;
                }
            }
        }

        SERVICE_REGISTRATION(DisplaySettings, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        DisplaySettings* DisplaySettings::_instance = nullptr;
        WPEFramework::Exchange::IPowerManager::PowerState DisplaySettings::m_powerState = WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY;

        DisplaySettings::DisplaySettings()
            : PluginHost::JSONRPC()
            , _pwrMgrNotification(*this)
            , _DSVideoPortNotification(*this)
            , _DSAudioNotification(*this)
            , _DSDisplayHotPlugNotification(*this)
            , _DSDisplayNotification(*this)
            , _DSVideoDeviceNotification(*this)
            , _registeredEventHandlers(false)
            , _registeredDsEventHandlers(false)
        {
            LOGINFO("constructor");
            DisplaySettings::_instance = this;
            m_client = nullptr;

            CreateHandler({ 2 });

            registerMethodLockedApi("getConnectedVideoDisplays", &DisplaySettings::getConnectedVideoDisplays, this);
            registerMethodLockedApi("getConnectedAudioPorts", &DisplaySettings::getConnectedAudioPorts, this);
            registerMethodLockedApi("setEnableAudioPort", &DisplaySettings::setEnableAudioPort, this);
            registerMethodLockedApi("getEnableAudioPort", &DisplaySettings::getEnableAudioPort, this);
            registerMethodLockedApi("getSupportedResolutions", &DisplaySettings::getSupportedResolutions, this);
            registerMethodLockedApi("getSupportedVideoDisplays", &DisplaySettings::getSupportedVideoDisplays, this);
            registerMethodLockedApi("getSupportedTvResolutions", &DisplaySettings::getSupportedTvResolutions, this);
            registerMethodLockedApi("getSupportedSettopResolutions", &DisplaySettings::getSupportedSettopResolutions, this);
            registerMethodLockedApi("getSupportedAudioPorts", &DisplaySettings::getSupportedAudioPorts, this);
            registerMethodLockedApi("getSupportedAudioModes", &DisplaySettings::getSupportedAudioModes, this);
            registerMethodLockedApi("getAudioFormat", &DisplaySettings::getAudioFormat, this);
            registerMethodLockedApi("getZoomSetting", &DisplaySettings::getZoomSetting, this);
            registerMethodLockedApi("setZoomSetting", &DisplaySettings::setZoomSetting, this);
            registerMethodLockedApi("getCurrentResolution", &DisplaySettings::getCurrentResolution, this);
            registerMethodLockedApi("setCurrentResolution", &DisplaySettings::setCurrentResolution, this);
            registerMethodLockedApi("getSoundMode", &DisplaySettings::getSoundMode, this);
            registerMethodLockedApi("setSoundMode", &DisplaySettings::setSoundMode, this);
            registerMethodLockedApi("readEDID", &DisplaySettings::readEDID, this);
            registerMethodLockedApi("readHostEDID", &DisplaySettings::readHostEDID, this);
            registerMethodLockedApi("getActiveInput", &DisplaySettings::getActiveInput, this);
            registerMethodLockedApi("getTvHDRSupport", &DisplaySettings::getTvHDRSupport, this);
            registerMethodLockedApi("getSettopHDRSupport", &DisplaySettings::getSettopHDRSupport, this);
            registerMethodLockedApi("getCurrentOutputSettings", &DisplaySettings::getCurrentOutputSettings, this);

            Utils::Synchro::RegisterLockedApi("getVolumeLeveller", &DisplaySettings::getVolumeLeveller, this);
            registerMethodLockedApi("getBassEnhancer", &DisplaySettings::getBassEnhancer, this);
            registerMethodLockedApi("isSurroundDecoderEnabled", &DisplaySettings::isSurroundDecoderEnabled, this);
            registerMethodLockedApi("getDRCMode", &DisplaySettings::getDRCMode, this);
            Utils::Synchro::RegisterLockedApi("getSurroundVirtualizer", &DisplaySettings::getSurroundVirtualizer, this);
            Utils::Synchro::RegisterLockedApi("setVolumeLeveller", &DisplaySettings::setVolumeLeveller, this);
            registerMethodLockedApi("setBassEnhancer", &DisplaySettings::setBassEnhancer, this);
            registerMethodLockedApi("enableSurroundDecoder", &DisplaySettings::enableSurroundDecoder, this);
            Utils::Synchro::RegisterLockedApi("setSurroundVirtualizer", &DisplaySettings::setSurroundVirtualizer, this);
            registerMethodLockedApi("setMISteering", &DisplaySettings::setMISteering, this);
            registerMethodLockedApi("setGain", &DisplaySettings::setGain, this);
            registerMethodLockedApi("getGain", &DisplaySettings::getGain, this);
            registerMethodLockedApi("setMuted", &DisplaySettings::setMuted, this);
            registerMethodLockedApi("getMuted", &DisplaySettings::getMuted, this);
            registerMethodLockedApi("setVolumeLevel", &DisplaySettings::setVolumeLevel, this);
            registerMethodLockedApi("getVolumeLevel", &DisplaySettings::getVolumeLevel, this);
            registerMethodLockedApi("setDRCMode", &DisplaySettings::setDRCMode, this);
            registerMethodLockedApi("getMISteering", &DisplaySettings::getMISteering, this);
            registerMethodLockedApi("setMS12AudioCompression", &DisplaySettings::setMS12AudioCompression, this);
            registerMethodLockedApi("getMS12AudioCompression", &DisplaySettings::getMS12AudioCompression, this);
            registerMethodLockedApi("setDolbyVolumeMode", &DisplaySettings::setDolbyVolumeMode, this);
            registerMethodLockedApi("getDolbyVolumeMode", &DisplaySettings::getDolbyVolumeMode, this);
            registerMethodLockedApi("setDialogEnhancement", &DisplaySettings::setDialogEnhancement, this);
            registerMethodLockedApi("getDialogEnhancement", &DisplaySettings::getDialogEnhancement, this);
            registerMethodLockedApi("setIntelligentEqualizerMode", &DisplaySettings::setIntelligentEqualizerMode, this);
            registerMethodLockedApi("getIntelligentEqualizerMode", &DisplaySettings::getIntelligentEqualizerMode, this);
            registerMethodLockedApi("setGraphicEqualizerMode", &DisplaySettings::setGraphicEqualizerMode, this);
            registerMethodLockedApi("getGraphicEqualizerMode", &DisplaySettings::getGraphicEqualizerMode, this);
            registerMethodLockedApi("setMS12AudioProfile", &DisplaySettings::setMS12AudioProfile, this);
            registerMethodLockedApi("getMS12AudioProfile", &DisplaySettings::getMS12AudioProfile, this);
            registerMethodLockedApi("getSupportedMS12AudioProfiles", &DisplaySettings::getSupportedMS12AudioProfiles, this);
            registerMethodLockedApi("resetDialogEnhancement", &DisplaySettings::resetDialogEnhancement, this);
            registerMethodLockedApi("resetBassEnhancer", &DisplaySettings::resetBassEnhancer, this);
            registerMethodLockedApi("resetSurroundVirtualizer", &DisplaySettings::resetSurroundVirtualizer, this);
            registerMethodLockedApi("resetVolumeLeveller", &DisplaySettings::resetVolumeLeveller, this);

            registerMethodLockedApi("setAssociatedAudioMixing", &DisplaySettings::setAssociatedAudioMixing, this);
            registerMethodLockedApi("getAssociatedAudioMixing", &DisplaySettings::getAssociatedAudioMixing, this);
            registerMethodLockedApi("setFaderControl", &DisplaySettings::setFaderControl, this);
            registerMethodLockedApi("getFaderControl", &DisplaySettings::getFaderControl, this);
            registerMethodLockedApi("setPrimaryLanguage", &DisplaySettings::setPrimaryLanguage, this);
            registerMethodLockedApi("getPrimaryLanguage", &DisplaySettings::getPrimaryLanguage, this);
            registerMethodLockedApi("setSecondaryLanguage", &DisplaySettings::setSecondaryLanguage, this);
            registerMethodLockedApi("getSecondaryLanguage", &DisplaySettings::getSecondaryLanguage, this);

            registerMethodLockedApi("getAudioDelay", &DisplaySettings::getAudioDelay, this);
            registerMethodLockedApi("setAudioDelay", &DisplaySettings::setAudioDelay, this);
            registerMethodLockedApi("getSinkAtmosCapability", &DisplaySettings::getSinkAtmosCapability, this);
            registerMethodLockedApi("setAudioAtmosOutputMode", &DisplaySettings::setAudioAtmosOutputMode, this);
            registerMethodLockedApi("setForceHDRMode", &DisplaySettings::setForceHDRMode, this);
            registerMethodLockedApi("getTVHDRCapabilities", &DisplaySettings::getTVHDRCapabilities, this);
            registerMethodLockedApi("isConnectedDeviceRepeater", &DisplaySettings::isConnectedDeviceRepeater, this);
            registerMethodLockedApi("getDefaultResolution", &DisplaySettings::getDefaultResolution, this);
            registerMethodLockedApi("setScartParameter", &DisplaySettings::setScartParameter, this);
            registerMethodLockedApi("getSettopMS12Capabilities", &DisplaySettings::getSettopMS12Capabilities, this);
            registerMethodLockedApi("getSettopAudioCapabilities", &DisplaySettings::getSettopAudioCapabilities, this);
            registerMethodLockedApi("setMS12ProfileSettingsOverride", &DisplaySettings::setMS12ProfileSettingsOverride,this);

            Utils::Synchro::RegisterLockedApiForHandler(GetHandler(2), "getVolumeLeveller", &DisplaySettings::getVolumeLeveller2, this);
            Utils::Synchro::RegisterLockedApiForHandler(GetHandler(2), "setVolumeLeveller", &DisplaySettings::setVolumeLeveller2, this);
            Utils::Synchro::RegisterLockedApiForHandler(GetHandler(2), "getSurroundVirtualizer", &DisplaySettings::getSurroundVirtualizer2, this);
            Utils::Synchro::RegisterLockedApiForHandler(GetHandler(2), "setSurroundVirtualizer", &DisplaySettings::setSurroundVirtualizer2, this);

            registerMethodLockedApi("getVideoFormat", &DisplaySettings::getVideoFormat, this);

            registerMethodLockedApi("setPreferredColorDepth", &DisplaySettings::setPreferredColorDepth, this);
            registerMethodLockedApi("getPreferredColorDepth", &DisplaySettings::getPreferredColorDepth, this);
            registerMethodLockedApi("getColorDepthCapabilities", &DisplaySettings::getColorDepthCapabilities, this);
            registerMethodLockedApi("getSupportedMS12Config", &DisplaySettings::getSupportedMS12Config, this);

            registerMethodLockedApi("setAudioDucking", &DisplaySettings::setAudioDucking, this);
            registerMethodLockedApi("setEnableVideoPort", &DisplaySettings::setEnableVideoPort, this);
            registerMethodLockedApi("getEnableVideoPort", &DisplaySettings::getEnableVideoPort, this);
            registerMethodLockedApi("getSupportedVideoCodingFormats", &DisplaySettings::getSupportedVideoCodingFormats, this);
            registerMethodLockedApi("getVideoCodecInfo", &DisplaySettings::getVideoCodecInfo, this);
            registerMethodLockedApi("getAudioEncoding", &DisplaySettings::getAudioEncoding, this);
            registerMethodLockedApi("setAudioEncoding", &DisplaySettings::setAudioEncoding, this);
            registerMethodLockedApi("getDisplayAspectRatio", &DisplaySettings::getDisplayAspectRatio, this);
           

	    m_subscribed = false; //HdmiCecSink event subscription
	    m_hdmiInAudioDeviceConnected = false;// Tells about the device connection state, for eArc will be updated on audio device power status event handler after tinymix command and incase of ARC will be true after ARC Initiation
	    m_arcEarcAudioEnabled = false; // Arc routing enabled/disabled
	    m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED; // Arc connection/disconnection UI notified flag
	    m_hdmiCecAudioDeviceDetected = false;// Audio device detected through cec ping
            m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_UNKNOWN;// Power state of AVR
	    m_currentArcRoutingState = ARC_STATE_ARC_TERMINATED; // Maintains the ARC state
	    m_requestSadRetrigger = false;
	    m_hdmiInAudioDeviceType = 0; // dsAUDIOARCSUPPORT_NONE// Maintains the Audio device type whether Arc/eArc ocnnected
	    m_AudioDeviceSADState = AUDIO_DEVICE_SAD_UNKNOWN;// maintains the SAD state
	    m_sendMsgThreadExit = false;
	    isResCacheUpdated = false;
            isDisplayConnectedCacheUpdated = false;
            isStbHDRcapabilitiesCache = false;
	    audioPortEnableStatusMap["IDLR0"] = false;
	    audioPortEnableStatusMap["HDMI0"] = false;
	    audioPortEnableStatusMap["SPDIF0"] = false;
	    audioPortEnableStatusMap["SPEAKER0"] = false;
	    audioPortEnableStatusMap["HDMI_ARC0"] = false;
	    audioPortEnableStatusMap["HEADPHONE0"] = false;

	   // m_AudioSentPoweronmsg = false;
        }

        DisplaySettings::~DisplaySettings()
        {
            LOGINFO ("dtor");
            isResCacheUpdated = false;
            isDisplayConnectedCacheUpdated = false;
            isStbHDRcapabilitiesCache = false;
	    audioPortEnableStatusMap.clear();
        }

        void DisplaySettings::AudioPortsReInitialize()
        {
            LOGINFO("Entering DisplaySettings::AudioPortsReInitialize");
            // COM-RPC path: re-acquire audio port handles
            _audioPortHandles.clear();
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    // Re-use the cached config store — no reload needed
                    std::vector<AudioPortEntry> entries;
                    if (_audioConfigStore.BuildAudioPortEntries(entries)) {
                        for (const AudioPortEntry& e : entries) {
                            int32_t handle = -1;
                            if (audio->GetAudioPort(e.type, e.index, handle) == Core::ERROR_NONE) {
                                _audioPortHandles[e.name] = handle;
                            }
                        }
                    }
                    audio->Release();
                }
            }
        }
     
        void DisplaySettings::InitAudioPorts() 
        {   //sample servicemanager response: {"success":true,"supportedAudioPorts":["HDMI0"]}
            //LOGINFOMETHOD();
            LOGINFO("Entering DisplaySettings::InitAudioPorts");
	    m_systemAudioMode_Power_RequestedAndReceived = true; //resetting this variable for bootup for AVR case
            // COM-RPC path: restore persisted audio port enable state via DeviceSettings plugin.
            // HDMI CEC / ARC timer setup is not performed via DS plugin path.
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    for (const auto& kv : _audioPortHandles) {
                        bool enabled = false;
                        std::string portName;
                        if (audio->GetAudioEnablePersist(kv.second, enabled, portName) == Core::ERROR_NONE) {
                            LOGWARN("Audio Port (COM-RPC): [%s] persist=%d\n", kv.first.c_str(), enabled);
                            if (enabled) {
                                if (audio->EnableAudioPort(kv.second, true) != Core::ERROR_NONE) {
                                    LOGWARN("Audio Port (COM-RPC): [%s] EnableAudioPort failed\n", kv.first.c_str());
                                }
                            }
                        }
                    }
                    audio->Release();
                }
            }
            return;
        }

        const string DisplaySettings::Initialize(PluginHost::IShell* service)
        {
	    Exchange::ISystemMode* _remotStoreObject = nullptr;
            ASSERT(service != nullptr);
            ASSERT(m_service == nullptr);

            m_service = service;
            m_service->AddRef();

	    try {
            m_sendMsgThread = std::thread(sendMsgThread);
        } catch (const std::system_error& e) {
            LOGERR("Failed to start m_sendMsgThread: %s", e.what());
        }
	    m_timer.connect(std::bind(&DisplaySettings::onTimer, this));
            m_AudioDeviceDetectTimer.connect(std::bind(&DisplaySettings::checkAudioDeviceDetectionTimer, this));
            m_ArcDetectionTimer.connect(std::bind(&DisplaySettings::checkArcDeviceConnected, this));
            m_SADDetectionTimer.connect(std::bind(&DisplaySettings::checkSADUpdate, this));
	    m_AudioDevicePowerOnStatusTimer.connect(std::bind(&DisplaySettings::checkAudioDevicePowerStatusTimer, this));

            InitializePowerManager();
            // COM-RPC path: open the DeviceSettings plugin COM-RPC link.
            // Sub-interface acquisition and notification registration happen in
            // OnDeviceSettingsActivated() when the DeviceSettings plugin is ready.
            DeviceSettingsClientHelper::Open(service);
            LOGINFO("DisplaySettings: DeviceSettingsClientHelper::Open() called — awaiting OnDeviceSettingsActivated()");

            if (WPEFramework::Exchange::IPowerManager::POWER_STATE_ON == getSystemPowerState())
            {
                InitAudioPorts();
            }
            else
            {
                LOGWARN("Current power state %d", m_powerState);
            }
            LOGWARN ("DisplaySettings::Initialize completes line:%d", __LINE__);
             _remotStoreObject = service->QueryInterfaceByCallsign<Exchange::ISystemMode>("org.rdk.SystemMode");

	    ASSERT (nullptr != _remotStoreObject);


	    if(_remotStoreObject)
	    {
               const string& callsign = "org.rdk.DisplaySettings";
		    const string& systemMode = "DEVICE_OPTIMIZE";
	            _remotStoreObject->ClientActivated(callsign,systemMode);
                    _remotStoreObject->Release();
                    _remotStoreObject = nullptr;		    
	    }
            else
            {
                    Utils::String::updateSystemModeFile( "DEVICE_OPTIMIZE", "callsign", "org.rdk.DisplaySettings","add") ;
            }

            // On success return empty, to indicate there is no error text.
            return (string());
        }

        void DisplaySettings::Deinitialize(PluginHost::IShell* service)
        {
            Exchange::ISystemMode* _remotStoreObject1 = nullptr;
            LOGINFO("Enetering DisplaySettings::Deinitialize");
            if (_powerManagerPlugin) {
		// Unregister from PowerManagerPlugin Notification
		_powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
                _powerManagerPlugin.Reset();
            }

            _registeredEventHandlers = false;
            //During DisplaySettings plugin  activation the SystemMode may not be added .But it will be added /tmp/SystemMode.txt . If after 5 min SystemMode got activated then SystemMode fill the client map from /tmp/SystemMode.txt. In this case if we deactivate DisplaySettings then _remotStoreObject will be null here . So we try to QueryInterface the ISystemMode one more time 
		if(_remotStoreObject1 == nullptr)
		{
				_remotStoreObject1 = service->QueryInterfaceByCallsign<Exchange::ISystemMode>("org.rdk.SystemMode");

		}

		ASSERT (nullptr != _remotStoreObject1);

		if(_remotStoreObject1)
		{
			const string& callsign = "org.rdk.DisplaySettings";
			const string& systemMode = "DEVICE_OPTIMIZE";
			_remotStoreObject1->ClientDeactivated(callsign,systemMode);
	                _remotStoreObject1->Release();
	                _remotStoreObject1 = nullptr;		
		}
		else
		{
			Utils::String::updateSystemModeFile( "DEVICE_OPTIMIZE", "callsign", "org.rdk.DisplaySettings","delete") ;
		}

            {

            std::unique_lock<std::mutex> lock(DisplaySettings::_instance->m_sendMsgMutex);
            DisplaySettings::_instance->m_sendMsgThreadExit = true;
                    DisplaySettings::_instance->m_sendMsgThreadRun = true;
                    DisplaySettings::_instance->m_sendMsgCV.notify_one();
            }
            int count = 0;
            while(audioPortInitActive && count < 20){
                sleep(100);
                count++;
            }
            try
            {
            if (m_sendMsgThread.joinable())
            	m_sendMsgThread.join();
            }
            catch(const std::system_error& e)
               {
            LOGERR("system_error exception in thread join %s", e.what());
            }
            catch(const std::exception& e)
            {
            LOGERR("exception in thread join %s", e.what());
        }

            stopCecTimeAndUnsubscribeEvent();

            // COM-RPC path: unregister notifications from all DS sub-interfaces then close the link.
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    vp->Unregister(&_DSVideoPortNotification);
                    vp->Release();
                }
            }
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    audio->Unregister(&_DSAudioNotification);
                    audio->Release();
                }
            }
            {
                auto* disp = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
                if (disp != nullptr) {
                    disp->Unregister(&_DSDisplayHotPlugNotification);
                    disp->Unregister(&_DSDisplayNotification);
                    disp->Release();
                }
            }
            {
                auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                if (vd != nullptr) {
                    vd->Unregister(&_DSVideoDeviceNotification);
                    vd->Release();
                }
            }
            DeviceSettingsClientHelper::Close();
            _videoPortHandles.clear();
            _audioPortHandles.clear();
            _displayHandles.clear();
            _videoDeviceHandle = -1;
            _registeredDsEventHandlers = false;

            DisplaySettings::_instance = nullptr;

            ASSERT(service == m_service);

            m_service->Release();
            m_service = nullptr;
        }

        void DisplaySettings::InitializePowerManager()
        {
            LOGINFO("Connect the COM-RPC socket\n");
            PowerState pwrStateCur = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
            PowerState pwrStatePrev = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
            Core::hresult retStatus = Core::ERROR_GENERAL;
            _powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
                .withIShell(m_service)
                .withRetryIntervalMS(200)
                .withRetryCount(25)
                .createInterface();

            registerEventHandlers();

                ASSERT (_powerManagerPlugin);
                if (_powerManagerPlugin){
                    retStatus = _powerManagerPlugin->GetPowerState(pwrStateCur, pwrStatePrev);
                }
                if (Core::ERROR_NONE == retStatus)
                {
                    m_powerState = pwrStateCur;
                    LOGINFO("DisplaySettings::m_powerState:%d", m_powerState);
                }
            }

        void DisplaySettings::registerEventHandlers()
        {
            ASSERT (nullptr != _powerManagerPlugin);

            if(!_registeredEventHandlers && _powerManagerPlugin) {
                _registeredEventHandlers = true;
                _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
            }
        }
        int DisplaySettings::getAudioDeviceSADState(void) {
            //function used to read the current SAD state with lock
            std::lock_guard<std::mutex> lock(m_SadMutex);
            return m_AudioDeviceSADState;
        }

        void DisplaySettings::setAudioDeviceSADState(int newState) {
            // function used to set the required SAD state with lock
            std::lock_guard<std::mutex> lock(m_SadMutex);
            LOGINFO("Updating m_AudioDeviceSADState : %d", newState);
            m_AudioDeviceSADState = newState;
        }

        int DisplaySettings::getCurrentArcRoutingState(void) {
            std::lock_guard<std::mutex> lock(m_AudioDeviceStatesUpdateMutex);
            return m_currentArcRoutingState;
        }
        

        bool DisplaySettings::isDisplayConnected (std::string port){
            bool isConnected = isHdmiDisplayConnected;
            if (!isDisplayConnectedCacheUpdated || !(Utils::String::stringContains(port, "HDMI0"))) {
                const auto it = _videoPortHandles.find(port);
                if (it != _videoPortHandles.end()) {
                    auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                    if (vp != nullptr) {
                        bool connected = false;
                        vp->IsVideoPortDisplayConnected(it->second, connected);
                        vp->Release();
                        isHdmiDisplayConnected = connected;
                        isConnected = connected;
                        isDisplayConnectedCacheUpdated = true;
                    }
                }
            } else {
                LOGINFO("Using isDisplayConnected cache \n");
            }
            return isConnected;
	}

        // ====================================================================
        // COM-RPC: DeviceSettingsClientHelper overrides
        // Called by the framework when the DeviceSettings plugin activates or
        // deactivates (including after a restart).
        // ====================================================================

        void DisplaySettings::OnDeviceSettingsActivated()
        {
            LOGINFO("DisplaySettings: OnDeviceSettingsActivated — acquiring DS sub-interface handles");

            // --- VideoPort sub-interface ---
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    // Load port config once into cached member store (1-arg member fn)
                    LoadVideoPortConfig(_vpConfigStore);

                    _videoPortHandles.clear();
                    std::vector<VideoPortEntry> entries;
                    if (_vpConfigStore.BuildVideoPortEntries(entries)) {
                        for (const VideoPortEntry& e : entries) {
                            int32_t handle = -1;
                            Core::hresult rc = vp->GetVideoPort(e.type, e.index, handle);
                            if (rc == Core::ERROR_NONE) {
                                _videoPortHandles[e.name] = handle;
                                LOGINFO("VideoPort '%s' → handle=%d", e.name.c_str(), handle);
                            }
                        }
                    }
                    vp->Register(&_DSVideoPortNotification);
                    vp->Release();
                }
            }

            // --- Audio sub-interface ---
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    // Load audio config into cached member store (1-arg member fn)
                    LoadAudioConfig(_audioConfigStore);

                    _audioPortHandles.clear();
                    std::vector<AudioPortEntry> entries;
                    if (_audioConfigStore.BuildAudioPortEntries(entries)) {
                        for (const AudioPortEntry& e : entries) {
                            int32_t handle = -1;
                            Core::hresult rc = audio->GetAudioPort(e.type, e.index, handle);
                            if (rc == Core::ERROR_NONE) {
                                _audioPortHandles[e.name] = handle;
                                LOGINFO("AudioPort '%s' → handle=%d", e.name.c_str(), handle);
                            }
                        }
                    }
                    audio->Register(&_DSAudioNotification);
                    audio->Release();
                }
            }

            // --- Display sub-interface ---
            {
                auto* disp = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
                if (disp != nullptr) {
                    disp->Register(&_DSDisplayHotPlugNotification);
                    disp->Register(&_DSDisplayNotification);
                    disp->Release();
                }
            }

            // --- VideoDevice sub-interface ---
            {
                auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                if (vd != nullptr) {
                    vd->GetVideoDeviceHandle(0, _videoDeviceHandle);
                    vd->Register(&_DSVideoDeviceNotification);
                    vd->Release();
                }
            }

            _registeredDsEventHandlers = true;
            isDisplayConnectedCacheUpdated = false;
            isResCacheUpdated = false;
            isStbHDRcapabilitiesCache = false;

            // Trigger audio port initialization on (re-)activation
            if (WPEFramework::Exchange::IPowerManager::POWER_STATE_ON == getSystemPowerState()) {
                InitAudioPorts();
            }
        }

        void DisplaySettings::OnDeviceSettingsDeactivated()
        {
            LOGINFO("DisplaySettings: OnDeviceSettingsDeactivated — invalidating cached handles");
            _videoPortHandles.clear();
            _audioPortHandles.clear();
            _displayHandles.clear();
            _videoDeviceHandle = -1;
            _vpConfigStore.Clear();
            _audioConfigStore.Clear();
            _registeredDsEventHandlers = false;
            isDisplayConnectedCacheUpdated = false;
            isResCacheUpdated = false;
            isStbHDRcapabilitiesCache = false;
        }

        // ====================================================================
        // COM-RPC: private event forwarders called from notification delegates
        // These bridge COM-RPC notification calls to the existing notification
        // dispatchers (resolutionPreChange, notifyAudioFormatChange, etc.)
        // ====================================================================

        void DisplaySettings::OnDSResolutionPreChange()
        {
            LOGINFO("Received COM-RPC OnResolutionPreChange");
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->resolutionPreChange();
            }
            isResCacheUpdated = false;
        }

        void DisplaySettings::OnDSResolutionPostChange(uint32_t width, uint32_t height)
        {
            LOGINFO("Received COM-RPC OnResolutionPostChange %ux%u", width, height);
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->resolutionChanged(static_cast<int>(width), static_cast<int>(height));
            }
        }

        void DisplaySettings::OnDSVideoFormatUpdate(uint32_t videoFormatHDR)
        {
            LOGINFO("Received COM-RPC OnVideoFormatUpdate hdr=%u", videoFormatHDR);
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->notifyVideoFormatChange(videoFormatHDR);
            }
        }

        void DisplaySettings::OnDSAudioOutHotPlug(int portType, uint32_t portNumber, bool isPortConnected)
        {
            LOGINFO("Received COM-RPC OnAudioOutHotPlug portType=%d connected=%d", portType, isPortConnected);
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->connectedAudioPortUpdated(portType, isPortConnected);
                if (portType == static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC)) {
                    if (isPortConnected) {
                        DisplaySettings::_instance->m_arcEarcConnectionNotifiedToUI = ARC_EARC_CONNECTED;
                    } else {
                        DisplaySettings::_instance->m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED;
                    }
                }
            }
        }

        void DisplaySettings::OnDSAudioFormatUpdate(uint32_t audioFormat)
        {
            LOGINFO("Received COM-RPC OnAudioFormatUpdate format=%u", audioFormat);
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->notifyAudioFormatChange(audioFormat);
            }
        }

        void DisplaySettings::OnDSDolbyAtmosCapabilitiesChanged(uint32_t atmosCapability, bool status)
        {
            LOGINFO("Received COM-RPC OnDolbyAtmosCapabilitiesChanged cap=%u status=%d", atmosCapability, status);
            if (DisplaySettings::_instance && status) {
                DisplaySettings::_instance->notifyAtmosCapabilityChange(atmosCapability);
            }
        }

        void DisplaySettings::OnDSAudioPortStateChanged(uint32_t audioPortState)
        {
            LOGINFO("Received COM-RPC OnAudioPortStateChanged state=%u", audioPortState);
            if (audioPortState == static_cast<uint32_t>(Exchange::IDeviceSettingsAudio::AudioPortState::AUDIO_PORT_STATE_INITIALIZED)) {
                if (DisplaySettings::_instance) {
                    DisplaySettings::_instance->AudioPortsReInitialize();
                    DisplaySettings::_instance->InitAudioPorts();
                }
            }
        }

        void DisplaySettings::OnDSAssociatedAudioMixingChanged(bool mixing)
        {
            LOGINFO("Received COM-RPC OnAssociatedAudioMixingChanged mixing=%d", mixing);
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->notifyAssociatedAudioMixingChange(mixing);
            }
        }

        void DisplaySettings::OnDSAudioFaderControlChanged(int32_t mixerBalance)
        {
            LOGINFO("Received COM-RPC OnAudioFaderControlChanged balance=%d", mixerBalance);
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->notifyFaderControlChange(static_cast<bool>(mixerBalance));
            }
        }

        void DisplaySettings::OnDSAudioPrimaryLanguageChanged(const string& primaryLanguage)
        {
            LOGINFO("Received COM-RPC OnAudioPrimaryLanguageChanged lang=%s", primaryLanguage.c_str());
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->notifyPrimaryLanguageChange(primaryLanguage);
            }
        }

        void DisplaySettings::OnDSAudioSecondaryLanguageChanged(const string& secondaryLanguage)
        {
            LOGINFO("Received COM-RPC OnAudioSecondaryLanguageChanged lang=%s", secondaryLanguage.c_str());
            if (DisplaySettings::_instance) {
                DisplaySettings::_instance->notifySecondaryLanguageChange(secondaryLanguage);
            }
        }

        void DisplaySettings::OnDSDisplayHDMIHotPlug(uint32_t displayEvent)
        {
            LOGINFO("Received COM-RPC OnDisplayHDMIHotPlug event=%u", displayEvent);
            isResCacheUpdated = false;
            isDisplayConnectedCacheUpdated = false;
            isStbHDRcapabilitiesCache = false;
            if (DisplaySettings::_instance) {
                // DS_DISPLAY_EVENT_CONNECTED = 0 maps to HDMI_HOT_PLUG_EVENT_CONNECTED (0)
                DisplaySettings::_instance->connectedVideoDisplaysUpdated(static_cast<int>(displayEvent));
            }
        }

        void DisplaySettings::OnDSDisplayRxSense(uint32_t displayEvent)
        {
            LOGINFO("Received COM-RPC OnDisplayRxSense event=%u", displayEvent);
            if (DisplaySettings::_instance) {
                if (displayEvent == static_cast<uint32_t>(Exchange::IDeviceSettingsDisplay::DisplayEvent::DS_DISPLAY_RXSENSE_ON)) {
                    DisplaySettings::_instance->activeInputChanged(true);
                } else if (displayEvent == static_cast<uint32_t>(Exchange::IDeviceSettingsDisplay::DisplayEvent::DS_DISPLAY_RXSENSE_OFF)) {
                    DisplaySettings::_instance->activeInputChanged(false);
                }
            }
        }

        void DisplaySettings::OnDSZoomSettingChanged(int32_t zoomSetting)
        {
            LOGINFO("Received COM-RPC OnZoomSettingChanged zoom=%d", zoomSetting);
            // Map zoom integer to named string (0=NONE, 1=FULL per dsVideoZoom_t equivalent)
            if (DisplaySettings::_instance) {
                if (zoomSetting == 0) {
                    DisplaySettings::_instance->zoomSettingUpdated("NONE");
                } else if (zoomSetting == 1) {
                    DisplaySettings::_instance->zoomSettingUpdated("FULL");
                }
            }
        }

        // COM-RPC stub for registerDsEventHandlers() — actual registration is in OnDeviceSettingsActivated()
        void DisplaySettings::registerDsEventHandlers()
        {
            LOGINFO("DisplaySettings (COM-RPC): DS notifications registered via OnDeviceSettingsActivated()");
        }

        void setResponseArray(JsonObject& response, const char* key, const vector<string>& items)
        {
            JsonArray arr;
            for(auto& i : items) arr.Add(JsonValue(i));

            response[key] = arr;

            string json;
            response.ToString(json);
        }

        //Begin methods
        uint32_t DisplaySettings::getConnectedVideoDisplays(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"connectedVideoDisplays":["HDMI0"],"success":true}
            //this                          : {"connectedVideoDisplays":["HDMI0"]}
            LOGINFOMETHOD();

            vector<string> connectedVideoDisplays;
            getConnectedVideoDisplaysHelper(connectedVideoDisplays);
            setResponseArray(response, "connectedVideoDisplays", connectedVideoDisplays);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getConnectedAudioPorts(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"success":true,"connectedAudioPorts":["HDMI0"]}
            LOGINFOMETHOD();
            vector<string> connectedAudioPorts;
            // COM-RPC path: iterate cached audio port handles
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    for (const auto& kv : _audioPortHandles) {
                        bool enabled = false;
                        if (audio->IsAudioPortEnabled(kv.second, enabled) == Core::ERROR_NONE && enabled) {
                            const std::string& portName = kv.first;
                            if ((portName == "HDMI_ARC0") && (m_hdmiInAudioDeviceConnected != true)) {
                                continue;
                            }
                            vectorSet(connectedAudioPorts, portName);
                        }
                    }
                    audio->Release();
                }
            }
            setResponseArray(response, "connectedAudioPorts", connectedAudioPorts);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedResolutions(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"supportedResolutions":["720p","1080i","1080p60"]}
            LOGINFOMETHOD();
            vector<string> supportedResolutions;
            {
                // Use cached config store — no COM-RPC config reload per request
                const std::string defaultPort = _vpConfigStore.GetDefaultVideoPortName();
                string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : defaultPort;
                VideoPortEntry entry;
                if (_vpConfigStore.ResolveByName(videoDisplay, entry)) {
                    const auto hIt = _videoPortHandles.find(entry.name);
                    if (hIt != _videoPortHandles.end()) {
                        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                        if (vp != nullptr) {
                            bool connected = false;
                            vp->IsVideoPortDisplayConnected(hIt->second, connected);
                            if (connected) {
                                std::vector<VideoPortResolution> resolutions;
                                if (_vpConfigStore.GetResolutionsForType(entry.type, resolutions)) {
                                    for (const auto& res : resolutions) {
                                        vectorSet(supportedResolutions, res.name);
                                    }
                                }
                            }
                            vp->Release();
                        }
                    }
                }
            }
            setResponseArray(response, "supportedResolutions", supportedResolutions);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedVideoDisplays(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"supportedVideoDisplays":["HDMI0"],"success":true}
            LOGINFOMETHOD();
            vector<string> supportedVideoDisplays;
            {
                // Use cached config store — no COM-RPC config reload per request
                std::vector<VideoPortEntry> entries;
                if (_vpConfigStore.BuildVideoPortEntries(entries)) {
                    for (const VideoPortEntry& e : entries) {
                        vectorSet(supportedVideoDisplays, e.name);
                    }
                }
            }
            setResponseArray(response, "supportedVideoDisplays", supportedVideoDisplays);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedTvResolutions(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"supportedTvResolutions":["480i","480p","576i","720p","1080i","1080p"]}
            LOGINFOMETHOD();
            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            vector<string> supportedTvResolutions;
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        int32_t tvResolutions = 0;
                        if (vp->GetTVSupportedResolutions(it->second, tvResolutions) == Core::ERROR_NONE) {
                            if (!tvResolutions) supportedTvResolutions.emplace_back("none");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_480I))  { supportedTvResolutions.emplace_back("480i"); supportedTvResolutions.emplace_back("480i60"); }
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_480P))  { supportedTvResolutions.emplace_back("480p"); supportedTvResolutions.emplace_back("480p60"); }
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_576I))  supportedTvResolutions.emplace_back("576i50");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_576P))  supportedTvResolutions.emplace_back("576p50");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_720P50)) supportedTvResolutions.emplace_back("720p50");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_720P))  { supportedTvResolutions.emplace_back("720p"); supportedTvResolutions.emplace_back("720p60"); }
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_1080P24)) supportedTvResolutions.emplace_back("1080p24");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_1080I25)) supportedTvResolutions.emplace_back("1080p25");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_1080P30)) supportedTvResolutions.emplace_back("1080p30");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_1080I50)) supportedTvResolutions.emplace_back("1080i50");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_1080P50)) supportedTvResolutions.emplace_back("1080p50");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_1080I))  { supportedTvResolutions.emplace_back("1080i"); supportedTvResolutions.emplace_back("1080i60"); }
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_1080P))  { supportedTvResolutions.emplace_back("1080p"); supportedTvResolutions.emplace_back("1080p60"); }
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_1080P60)) supportedTvResolutions.emplace_back("1080p60");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_2160P24)) supportedTvResolutions.emplace_back("2160p24");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_2160P25)) supportedTvResolutions.emplace_back("2160p25");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_2160P30)) supportedTvResolutions.emplace_back("2160p30");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_2160P50)) supportedTvResolutions.emplace_back("2160p50");
                            if (tvResolutions & static_cast<int32_t>(TVResolution::DS_TV_RESOLUTION_2160P60)) supportedTvResolutions.emplace_back("2160p60");
                        }
                    }
                    vp->Release();
                }
            }
            setResponseArray(response, "supportedTvResolutions", supportedTvResolutions);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedSettopResolutions(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"supportedSettopResolutions":["720p","1080i","1080p60"]}
            LOGINFOMETHOD();
            vector<string> supportedSettopResolutions;
            {
                auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                if (vd != nullptr) {
                    Exchange::IDeviceSettingsVideoDevice::IVideoDeviceConfigIterator* cfgIt = nullptr;
                    if (vd->GetVideoDeviceConfig(cfgIt) == Core::ERROR_NONE && cfgIt != nullptr) {
                        Exchange::IDeviceSettingsVideoDevice::VideoDeviceConfigInfo cfg;
                        if (cfgIt->Next(cfg)) {
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_NONE))              vectorSet(supportedSettopResolutions, "NONE");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_FULL))              vectorSet(supportedSettopResolutions, "FULL");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_16_9))           vectorSet(supportedSettopResolutions, "LB_16_9");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_14_9))           vectorSet(supportedSettopResolutions, "LB_14_9");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_CCO))               vectorSet(supportedSettopResolutions, "CCO");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_PAN_SCAN))          vectorSet(supportedSettopResolutions, "PAN_SCAN");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_2_21_1_ON_4_3))  vectorSet(supportedSettopResolutions, "LB_2_21_1_ON_4_3");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_2_21_1_ON_16_9)) vectorSet(supportedSettopResolutions, "LB_2_21_1_ON_16_9");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_PLATFORM))          vectorSet(supportedSettopResolutions, "PLATFORM");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_16_9_ZOOM))         vectorSet(supportedSettopResolutions, "16_9_ZOOM");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_PILLARBOX_4_3))     vectorSet(supportedSettopResolutions, "PILLARBOX_4_3");
                            if (cfg.supportedDFCsMask & static_cast<uint32_t>(VideoZoom::DS_VIDEO_DEVICE_ZOOM_WIDE_4_3))          vectorSet(supportedSettopResolutions, "WIDE_4_3");
                        }
                        cfgIt->Release();
                    }
                    vd->Release();
                }
            }
            setResponseArray(response, "supportedSettopResolutions", supportedSettopResolutions);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedAudioPorts(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"success":true,"supportedAudioPorts":["HDMI0"]}
            LOGINFOMETHOD();
            vector<string> supportedAudioPorts;
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    for (const auto& kv : _audioPortHandles) {
                        vectorSet(supportedAudioPorts, kv.first);
                    }
                    audio->Release();
                }
            }
            setResponseArray(response, "supportedAudioPorts", supportedAudioPorts);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedAudioModes(const JsonObject& parameters, JsonObject& response)
        {   //sample response: {"success":true,"supportedAudioModes":["STEREO","PASSTHRU","AUTO (Dolby Digital 5.1)"]}
            LOGINFOMETHOD();
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "";
            vector<string> supportedAudioModes;
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    Exchange::IDeviceSettingsAudio::IAudioTypeConfigIterator* typeIt = nullptr;
                    Exchange::IDeviceSettingsAudio::IAudioPortConfigIterator* portIt = nullptr;
                    if (audio->GetAudioConfig(typeIt, portIt) == Core::ERROR_NONE) {
                        if (typeIt != nullptr) {
                            Exchange::IDeviceSettingsAudio::AudioTypeConfigInfo typeInfo;
                            while (typeIt->Next(typeInfo)) {
                                if (audioPort.empty() || Utils::String::stringContains(typeInfo.name, audioPort)) {
                                    uint32_t modeMask = typeInfo.supportedStereoModeMask;
                                    if (modeMask & (1u << static_cast<uint32_t>(StereoMode::AUDIO_STEREO_MONO)))        vectorSet(supportedAudioModes, "MONO");
                                    if (modeMask & (1u << static_cast<uint32_t>(StereoMode::AUDIO_STEREO_STEREO)))      vectorSet(supportedAudioModes, "STEREO");
                                    if (modeMask & (1u << static_cast<uint32_t>(StereoMode::AUDIO_STEREO_PASSTHROUGH))) vectorSet(supportedAudioModes, "PASSTHRU");
                                    if (modeMask & (1u << static_cast<uint32_t>(StereoMode::AUDIO_STEREO_DD)))          vectorSet(supportedAudioModes, "DOLBYDIGITAL");
                                    if (modeMask & (1u << static_cast<uint32_t>(StereoMode::AUDIO_STEREO_DDPLUS)))      vectorSet(supportedAudioModes, "DOLBYDIGITALPLUS");
                                }
                            }
                            typeIt->Release();
                        }
                        if (portIt != nullptr) portIt->Release();
                    }
                    audio->Release();
                }
            }
            setResponseArray(response, "supportedAudioModes", supportedAudioModes);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getZoomSetting(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            string zoomSetting = "unknown";

            bool success = true;
            {
                auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                if (vd != nullptr) {
                    Exchange::IDeviceSettingsVideoDevice::VideoZoom dfcZoom = Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_UNKNOWN;
                    if (vd->GetVideoDeviceDFC(_videoDeviceHandle, dfcZoom) == Core::ERROR_NONE) {
                        switch (dfcZoom) {
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_NONE:             zoomSetting = "NONE"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_FULL:             zoomSetting = "FULL"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_16_9:          zoomSetting = "LB_16_9"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_14_9:          zoomSetting = "LB_14_9"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_CCO:              zoomSetting = "CCO"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_PAN_SCAN:         zoomSetting = "PAN_SCAN"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_PLATFORM:         zoomSetting = "PLATFORM"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_16_9_ZOOM:        zoomSetting = "16_9_ZOOM"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_PILLARBOX_4_3:    zoomSetting = "PILLARBOX_4_3"; break;
                            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_WIDE_4_3:         zoomSetting = "WIDE_4_3"; break;
                            default: success = false; break;
                        }
                    } else {
                        success = false;
                    }
                    vd->Release();
                } else {
                    success = false;
                }
            }
#ifdef USE_IARM
            zoomSetting = iarm2svc(zoomSetting);
#endif
            response["zoomSetting"] = zoomSetting;
            returnResponse(success);
        }

        uint32_t DisplaySettings::setZoomSetting(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();

            returnIfParamNotFound(parameters, "zoomSetting");
            string zoomSetting = parameters["zoomSetting"].String();

            bool success = true;
            {
                auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                if (vd != nullptr) {
                    VideoZoom dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_UNKNOWN;
                    string zs = zoomSetting;
                    std::transform(zs.begin(), zs.end(), zs.begin(), ::toupper);
                    if (zs == "NONE")               dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_NONE;
                    else if (zs == "FULL")          dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_FULL;
                    else if (zs == "LB_16_9")       dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_16_9;
                    else if (zs == "LB_14_9")       dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_14_9;
                    else if (zs == "CCO")           dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_CCO;
                    else if (zs == "PAN_SCAN")      dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_PAN_SCAN;
                    else if (zs == "PLATFORM")      dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_PLATFORM;
                    else if (zs == "16_9_ZOOM")     dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_16_9_ZOOM;
                    else if (zs == "PILLARBOX_4_3") dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_PILLARBOX_4_3;
                    else if (zs == "WIDE_4_3")      dfcZoom = VideoZoom::DS_VIDEO_DEVICE_ZOOM_WIDE_4_3;
                    else { LOGERR("Unknown zoom setting: %s", zoomSetting.c_str()); success = false; }
                    if (success && vd->SetVideoDeviceDFC(_videoDeviceHandle, dfcZoom) != Core::ERROR_NONE) {
                        success = false;
                    }
                    vd->Release();
                } else {
                    success = false;
                }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getCurrentResolution(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"resolution":"720p"}
            LOGINFOMETHOD();
            bool success = true;
            {
                // Use cached config store and handles — no COM-RPC config reload per request
                const std::string defaultPort = _vpConfigStore.GetDefaultVideoPortName();
                string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : defaultPort;
                VideoPortEntry entry;
                if (_vpConfigStore.ResolveByName(videoDisplay, entry)) {
                    const auto hIt = _videoPortHandles.find(entry.name);
                    if (hIt != _videoPortHandles.end()) {
                        if (!isResCacheUpdated) {
                            auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                            if (vp != nullptr) {
                                Exchange::IDeviceSettingsVideoPort::VideoPortResolution vpRes;
                                if (vp->GetVideoPortResolution(hIt->second, vpRes) == Core::ERROR_NONE) {
                                    currentResolutionCache = vpRes.name;
                                    isResCacheUpdated = true;
                                } else {
                                    success = false;
                                }
                                vp->Release();
                            } else {
                                success = false;
                            }
                        }
                    } else {
                        success = false;
                    }
                } else {
                    success = false;
                }
            }
            if (success) {
                const string& res = currentResolutionCache;
                int width = 0, height = 0;
                bool progressive = false;
                if(res.rfind("480", 0) == 0) { width = 720; height = 480; }
                else if(res.rfind("576", 0) == 0) { width = 720; height = 576; }
                else if(res.rfind("720", 0) == 0) { width = 1280; height = 720; }
                else if(res.rfind("768", 0) == 0) { width = 1366; height = 768; }
                else if(res.rfind("1080", 0) == 0) { width = 1920; height = 1080; }
                else if(res.rfind("2160", 0) == 0) { width = 3840; height = 2160; }
                else if(res.rfind("4096x2160", 0) == 0) { width = 4096; height = 2160; }
                else { width = 1280; height = 720; }
                if(res.find('p') != std::string::npos) { progressive = true; }
                response["resolution"] = res;
                response["w"] = width;
                response["h"] = height;
                response["progressive"] = progressive;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setCurrentResolution(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "videoDisplay");
            returnIfParamNotFound(parameters, "resolution");

            string videoDisplay = parameters["videoDisplay"].String();
            string resolution = parameters["resolution"].String();

            bool hasPersist = parameters.HasLabel("persist");
            bool persist = hasPersist ? parameters["persist"].Boolean() : true;
            if (!hasPersist) LOGINFO("persist: true");
 
            bool isIgnoreEdidArg = parameters.HasLabel("ignoreEdid");
            bool isIgnoreEdid = isIgnoreEdidArg ? parameters["ignoreEdid"].Boolean() : false;
            if (!isIgnoreEdidArg) LOGINFO("isIgnoreEdid: false"); else LOGINFO("isIgnoreEdid: %d", isIgnoreEdid);

            bool success = true;
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        Exchange::IDeviceSettingsVideoPort::VideoPortResolution vpRes;
                        vpRes.name = resolution;
                        // pixelResolution/aspectRatio/etc. will be resolved by DeviceSettings implementation
                        Core::hresult rc = vp->SetVideoPortResolution(it->second, vpRes, persist, isIgnoreEdid);
                        if (rc != Core::ERROR_NONE) {
                            LOGERR("SetVideoPortResolution failed for '%s': %u", videoDisplay.c_str(), rc);
                            success = false;
                        } else {
                            isResCacheUpdated = false; // invalidate cache
                        }
                    } else {
                        LOGERR("No handle found for videoDisplay '%s'", videoDisplay.c_str());
                        success = false;
                    }
                    vp->Release();
                } else {
                    success = false;
                }
            }
            returnResponse(success);
        }
        uint32_t DisplaySettings::getSoundMode(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"soundMode":"AUTO (Dolby Digital 5.1)"}
            LOGINFOMETHOD();
            string audioPort = parameters["audioPort"].String();//empty value will browse all ports

            if (!checkPortName(audioPort))
                audioPort = "HDMI0";

            bool success = true;
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        Exchange::IDeviceSettingsAudio::StereoMode stereoMode;
                        if (audio->GetStereoMode(it->second, stereoMode) == Core::ERROR_NONE) {
                            switch (stereoMode) {
                                case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_STEREO:      response["soundMode"] = "STEREO"; break;
                                case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_MONO:        response["soundMode"] = "MONO"; break;
                                case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_SURROUND:    response["soundMode"] = "SURROUND"; break;
                                case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_PASSTHROUGH: response["soundMode"] = "PASSTHRU"; break;
                                default: response["soundMode"] = "AUTO"; break;
                            }
                        } else {
                            success = false;
                        }
                    } else {
                        success = false;
                    }
                    audio->Release();
                } else {
                    success = false;
                }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setSoundMode(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            string audioPort = parameters["audioPort"].String();//missing or empty string and we will set all ports

            returnIfParamNotFound(parameters, "soundMode");
            string soundMode = parameters["soundMode"].String();
            Utils::String::toLower(soundMode);

            bool hasPersist = parameters.HasLabel("persist");
            bool persist = hasPersist ? parameters["persist"].Boolean() : true;
            if (!hasPersist) LOGINFO("persist: true");

            bool success = true;

            LOGWARN("display = %s, mode = %s!", audioPort.c_str(), soundMode.c_str());

            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    if (!audioPort.empty()) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            StereoMode comMode = StereoMode::AUDIO_STEREO_STEREO;
                            if (soundMode == "mono")                  comMode = StereoMode::AUDIO_STEREO_MONO;
                            else if (soundMode == "stereo")           comMode = StereoMode::AUDIO_STEREO_STEREO;
                            else if (soundMode == "surround")         comMode = StereoMode::AUDIO_STEREO_SURROUND;
                            else if (soundMode.substr(0, 4) == "auto") comMode = StereoMode::AUDIO_STEREO_SURROUND;
                            else if (soundMode == "passthru")         comMode = StereoMode::AUDIO_STEREO_PASSTHROUGH;
                            else if (soundMode == "dolbydigital")     comMode = StereoMode::AUDIO_STEREO_DD;
                            else if (soundMode == "dolbydigitalplus")  comMode = StereoMode::AUDIO_STEREO_DDPLUS;
                            else if (soundMode == "dolby digital 5.1") comMode = StereoMode::AUDIO_STEREO_SURROUND;
                            if (audio->SetStereoMode(it->second, comMode, persist) != Core::ERROR_NONE) {
                                success = false;
                            }
                        } else {
                            success = false;
                        }
                    } else {
                        /* No audioPort specified, set mode to all connected ports */
                        JsonObject params;
                        params["videoDisplay"] = "HDMI0";
                        params["soundMode"] = soundMode;
                        JsonObject unusedResponse;
                        setSoundMode(params, response);
                        params["videoDisplay"] = "SPDIF0";
                        setSoundMode(params, unusedResponse);
                    }
                    audio->Release();
                } else {
                    success = false;
                }
            }
            //TODO(MROLLINS) -- so this is interesting.  ServiceManager had a settingChanged event that I guess handled settings from many services.
            //Does that mean we need to save our setting back to another plugin that would own settings (and this settingsChanged event) ?
            //ServiceManager::getInstance()->saveSetting(this, SETTING_DISPLAY_SERVICE_SOUND_MODE, soundMode);

            returnResponse(success);
        }

        uint32_t DisplaySettings::readEDID(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"EDID":"AP///////wBSYgYCAQEBAQEXAQOAoFp4CvCdo1VJmyYPR0ovzgCBgIvAAQEBAQEBAQEBAQEBAjqAGHE4LUBYLEUAQIRjAAAeZiFQsFEAGzBAcDYAQIRjAAAeAAAA/ABUT1NISUJBLVRWCiAgAAAA/QAXSw9EDwAKICAgICAgAbECAytxSpABAgMEBQYHICImCQcHEQcYgwEAAGwDDAAQADgtwBUVHx/jBQMBAR2AGHEcFiBYLCUAQIRjAACeAR0AclHQHiBuKFUAQIRjAAAejArQiiDgLRAQPpYAsIRDAAAYjAqgFFHwFgAmfEMAsIRDAACYAAAAAAAAAAAAAAAA9w=="
            //sample this thunder plugin    : {"EDID":"AP///////wBSYgYCAQEBAQEXAQOAoFp4CvCdo1VJmyYPR0ovzgCBgIvAAQEBAQEBAQEBAQEBAjqAGHE4LUBYLEUAQIRjAAAeZiFQsFEAGzBAcDYAQIRjAAAeAAAA/ABUT1NISUJBLVRWCiAgAAAA/QAXSw9EDwAKICAgICAgAbECAytxSpABAgMEBQYHICImCQcHEQcYgwEAAGwDDAAQADgtwBUVHx/jBQMBAR2AGHEcFiBYLCUAQIRjAACeAR0AclHQHiBuKFUAQIRjAAAejArQiiDgLRAQPpYAsIRDAAAYjAqgFFHwFgAmfEMAsIRDAACYAAAAAAAAAAAAAAAA9w"}
            LOGINFOMETHOD();

            string edidbase64 = "";
            {
                auto* disp = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
                if (disp != nullptr) {
                    int32_t displayHandle = -1;
                    if (disp->GetDisplay(Exchange::IDeviceSettingsDisplay::DS_DISPLAY_PORT_TYPE_HDMI, 0, displayHandle) == Core::ERROR_NONE && displayHandle >= 0) {
                        constexpr uint16_t kEdidMaxLen = 256;
                        uint8_t edidBuf[kEdidMaxLen] = {};
                        if (disp->GetDisplayEdidBytes(displayHandle, edidBuf, kEdidMaxLen) == Core::ERROR_NONE) {
                            Core::ToString(edidBuf, kEdidMaxLen, true, edidbase64);
                        }
                    }
                    disp->Release();
                }
            }
            response["EDID"] = edidbase64;
            returnResponse(true);
        }

        uint32_t DisplaySettings::readHostEDID(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();

            string base64String;
            {
                auto* host = AcquireSubInterface<Exchange::IDeviceSettingsHost>();
                if (host != nullptr) {
                    constexpr uint16_t kEdidMaxLen = 256;
                    uint8_t edidBuf[kEdidMaxLen] = {};
                    if (host->GetEDID(edidBuf, kEdidMaxLen) == Core::ERROR_NONE) {
                        Core::ToString(edidBuf, kEdidMaxLen, true, base64String);
                    }
                    host->Release();
                }
            }
            response["EDID"] = base64String;
            returnResponse(true);
        }

        uint32_t DisplaySettings::getActiveInput(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();

            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            bool active = true;
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        bool portActive = false;
                        if (vp->IsVideoPortActive(it->second, portActive) == Core::ERROR_NONE) {
                            active = portActive;
                        }
                    }
                    vp->Release();
                }
            }
            response["activeInput"] = JsonValue(active);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getTvHDRSupport(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"standards":["none"],"supportsHDR":false}
            LOGINFOMETHOD();

            JsonArray hdrCapabilities;
            int capabilities = 0;

            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        int32_t tvCapabilities = 0;
                        if (vp->GetTVHDRCapabilities(it->second, tvCapabilities) == Core::ERROR_NONE) {
                            capabilities = tvCapabilities;
                        }
                    }
                    vp->Release();
                }
            }

            if(!capabilities)hdrCapabilities.Add("none");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_HDR10))         hdrCapabilities.Add("HDR10");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_HDR10PLUS))     hdrCapabilities.Add("HDR10PLUS");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_HLG))           hdrCapabilities.Add("HLG");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_DOLBYVISION))   hdrCapabilities.Add("Dolby Vision");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_TECHNICOLORPRIME)) hdrCapabilities.Add("Technicolor Prime");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_SDR))           hdrCapabilities.Add("SDR");

            if(capabilities)
            {
                response["supportsHDR"] = true;
            }
            else
            {
                response["supportsHDR"] = false;
            }
            response["standards"] = hdrCapabilities;
            for (uint32_t i = 0; i < hdrCapabilities.Length(); i++)
            {
               LOGINFO("capabilities: %s", hdrCapabilities[i].String().c_str());
            }
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSettopHDRSupport(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"standards":["HDR10"],"supportsHDR":true}
            LOGINFOMETHOD();

            JsonArray hdrCapabilities;
	    int capabilities = stbHDRcapabilitiesCache;
            if (!isStbHDRcapabilitiesCache) {
                capabilities = 0; // dsHDRSTANDARD_NONE = 0
                {
                    auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                    if (vd != nullptr) {
                        int32_t caps = 0;
                        if (vd->GetHDRCapabilities(_videoDeviceHandle, caps) == Core::ERROR_NONE) {
                            capabilities = caps;
                        }
                        vd->Release();
                    }
                }
		stbHDRcapabilitiesCache = capabilities;
		isStbHDRcapabilitiesCache = true;
            } else {
                LOGINFO("Using getSettopHDRSupport cache \n");
            }


            if(!capabilities)hdrCapabilities.Add("none");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_HDR10))         hdrCapabilities.Add("HDR10");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_HDR10PLUS))     hdrCapabilities.Add("HDR10PLUS");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_HLG))           hdrCapabilities.Add("HLG");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_DOLBYVISION))   hdrCapabilities.Add("Dolby Vision");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_TECHNICOLORPRIME)) hdrCapabilities.Add("Technicolor Prime");
            if(capabilities & static_cast<int32_t>(HDRStandard::DS_HDRSTANDARD_SDR))           hdrCapabilities.Add("SDR");

            if(capabilities)
            {
                response["supportsHDR"] = true;
            }
            else
            {
                response["supportsHDR"] = false;
            }
            response["standards"] = hdrCapabilities;
            for (uint32_t i = 0; i < hdrCapabilities.Length(); i++)
            {
               LOGINFO("capabilities: %s", hdrCapabilities[i].String().c_str());
            }
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSettopAudioCapabilities(const JsonObject& parameters, JsonObject& response)
        {
            JsonArray audioCapabilities;
            int capabilities = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        int32_t caps = 0;
                        if (audio->GetAudioCapabilities(it->second, caps) == Core::ERROR_NONE) {
                            capabilities = caps;
                        }
                    }
                    audio->Release();
                }
            }

            if(!capabilities)audioCapabilities.Add("none");
            if(capabilities & static_cast<int32_t>(AudioCapabilities::AUDIO_CAPS_ATMOS))                   audioCapabilities.Add("ATMOS");
            if(capabilities & static_cast<int32_t>(AudioCapabilities::AUDIO_CAPS_DOLBY_DIGITAL))           audioCapabilities.Add("DOLBY DIGITAL");
            if(capabilities & static_cast<int32_t>(AudioCapabilities::AUDIO_CAPS_DOLBY_DIGITAL_PLUS))      audioCapabilities.Add("DOLBY DIGITAL PLUS");
            if(capabilities & static_cast<int32_t>(AudioCapabilities::AUDIO_CAPS_DIGITAL_AUDIO_DELIVERY))  audioCapabilities.Add("Dual Audio Decode");
            if(capabilities & static_cast<int32_t>(AudioCapabilities::AUDIO_CAPS_DIGITAL_AUDIO_PROCESS_V2)) audioCapabilities.Add("DAPv2");
            if(capabilities & static_cast<int32_t>(AudioCapabilities::AUDIO_CAPS_MS12))                    audioCapabilities.Add("MS12");

            response["AudioCapabilities"] = audioCapabilities;
            for (uint32_t i = 0; i < audioCapabilities.Length(); i++)
            {
               LOGINFO("capabilities: %s", audioCapabilities[i].String().c_str());
            }
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSettopMS12Capabilities(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"MS12Capabilities":["Dolby Volume","Inteligent Equalizer","Dialogue Enhancer"]}
            LOGINFOMETHOD();

            JsonArray ms12Capabilities;
            int capabilities = 0;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        int32_t caps = 0;
                        if (audio->GetAudioMS12Capabilities(it->second, caps) == Core::ERROR_NONE) {
                            capabilities = caps;
                        }
                    }
                    audio->Release();
                }
            }

            if(!capabilities)ms12Capabilities.Add("none");
            if(capabilities & dsMS12SUPPORT_DolbyVolume)ms12Capabilities.Add("Dolby Volume");
            if(capabilities & dsMS12SUPPORT_InteligentEqualizer)ms12Capabilities.Add("Inteligent Equalizer");
            if(capabilities & dsMS12SUPPORT_DialogueEnhancer)ms12Capabilities.Add("Dialogue Enhancer");

            response["MS12Capabilities"] = ms12Capabilities;
            for (uint32_t i = 0; i < ms12Capabilities.Length(); i++)
            {
               LOGINFO("capabilities: %s", ms12Capabilities[i].String().c_str());
            }
            returnResponse(true);
        }

        uint32_t DisplaySettings::getCurrentOutputSettings(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            bool success = true;
            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        Exchange::IDeviceSettingsVideoPort::DSOutputSettings settings;
                        if (vp->GetCurrentOutputSettings(it->second, settings) == Core::ERROR_NONE) {
                            response["video_eotf"] = static_cast<uint32_t>(settings.videoEotf);
                            response["matrix_coefficients"] = static_cast<uint32_t>(settings.matrixCoefficients);
                            response["color_depth"] = static_cast<uint32_t>(settings.colorDepth);
                            response["color_space"] = static_cast<uint32_t>(settings.colorSpace);
                            response["quantization_range"] = static_cast<uint32_t>(settings.quantizationRange);
                        } else {
                            success = false;
                        }
                    } else {
                        success = false;
                    }
                    vp->Release();
                } else {
                    success = false;
                }
            }

            LOGINFO("Leaving_ DisplaySettings::%s\n", __FUNCTION__);
            returnResponse(success);
        }

        uint32_t DisplaySettings::getVolumeLeveller(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();

                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            Exchange::IDeviceSettingsAudio::VolumeLeveller leveller{0,0};
                            if (audio->GetAudioVolumeLeveller(it->second, leveller) == Core::ERROR_NONE) {
                                response["enable"] = (leveller.mode ? true : false);
                                response["level"] = leveller.level;
                            } else {
                                success = false;
                                response["enable"] = false;
                                response["level"] = 0;
                            }
                        } else {
                            success = false;
                        }
                        audio->Release();
                    } else {
                        success = false;
                    }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::getVolumeLeveller2(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();

                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            Exchange::IDeviceSettingsAudio::VolumeLeveller leveller{0,0};
                            if (audio->GetAudioVolumeLeveller(it->second, leveller) == Core::ERROR_NONE) {
                                response["mode"] = leveller.mode;
                                response["level"] = leveller.level;
                            } else {
                                success = false;
                                response["mode"] = 0;
                                response["level"] = 0;
                            }
                        } else {
                            success = false;
                        }
                        audio->Release();
                    } else {
                        success = false;
                    }
                }
                returnResponse(success);
        }

        void DisplaySettings::audioFormatToString(
            uint32_t audioFormat,
            JsonObject & response)
        {
            std::vector<string> supportedAudioFormat = {"NONE", "PCM", "AAC","VORBIS","WMA", "DOLBY AC3", "DOLBY EAC3",
                                                         "DOLBY AC4", "DOLBY MAT", "DOLBY TRUEHD",
                                                         "DOLBY EAC3 ATMOS","DOLBY TRUEHD ATMOS",
                                                         "DOLBY MAT ATMOS","DOLBY AC4 ATMOS","UNKNOWN"};
            // COM-RPC path: Exchange::IDeviceSettingsAudio::AudioFormat enum values
            switch (static_cast<AudioFormat>(audioFormat)) {
                case AudioFormat::AUDIO_FORMAT_NONE:           response["currentAudioFormat"] = "NONE"; break;
                case AudioFormat::AUDIO_FORMAT_PCM:            response["currentAudioFormat"] = "PCM"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_AC3:      response["currentAudioFormat"] = "DOLBY AC3"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_EAC3:     response["currentAudioFormat"] = "DOLBY EAC3"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_AC4:      response["currentAudioFormat"] = "DOLBY AC4"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_MAT:      response["currentAudioFormat"] = "DOLBY MAT"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_TRUEHD:   response["currentAudioFormat"] = "DOLBY TRUEHD"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_EAC3_ATMOS:   response["currentAudioFormat"] = "DOLBY EAC3 ATMOS"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_TRUEHD_ATMOS: response["currentAudioFormat"] = "DOLBY TRUEHD ATMOS"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_MAT_ATMOS:    response["currentAudioFormat"] = "DOLBY MAT ATMOS"; break;
                case AudioFormat::AUDIO_FORMAT_DOLBY_AC4_ATMOS:    response["currentAudioFormat"] = "DOLBY AC4 ATMOS"; break;
                case AudioFormat::AUDIO_FORMAT_AAC:            response["currentAudioFormat"] = "AAC"; break;
                case AudioFormat::AUDIO_FORMAT_VORBIS:         response["currentAudioFormat"] = "VORBIS"; break;
                case AudioFormat::AUDIO_FORMAT_WMA:            response["currentAudioFormat"] = "WMA"; break;
                default:                              response["currentAudioFormat"] = "UNKNOWN"; break;
            }
            setResponseArray(response, "supportedAudioFormat", supportedAudioFormat);
	}

        uint32_t DisplaySettings::getAudioFormat(const JsonObject& parameters, JsonObject& response)
        {
             LOGINFOMETHOD();
	     bool success = true;
             {
                 // COM-RPC path: get audio format from first available audio port handle
                 string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                 auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                 if (audio != nullptr) {
                     const auto it = _audioPortHandles.find(audioPort);
                     if (it != _audioPortHandles.end()) {
                         Exchange::IDeviceSettingsAudio::AudioFormat fmt = Exchange::IDeviceSettingsAudio::AudioFormat::AUDIO_FORMAT_NONE;
                         if (audio->GetAudioFormat(it->second, fmt) == Core::ERROR_NONE) {
                             audioFormatToString(static_cast<uint32_t>(fmt), response);
                         } else {
                             audioFormatToString(static_cast<uint32_t>(Exchange::IDeviceSettingsAudio::AudioFormat::AUDIO_FORMAT_NONE), response);
                             success = false;
                         }
                     } else {
                         audioFormatToString(static_cast<uint32_t>(Exchange::IDeviceSettingsAudio::AudioFormat::AUDIO_FORMAT_NONE), response);
                         success = false;
                     }
                     audio->Release();
                 } else {
                     audioFormatToString(static_cast<uint32_t>(Exchange::IDeviceSettingsAudio::AudioFormat::AUDIO_FORMAT_NONE), response);
                     success = false;
                 }
             }
	     returnResponse(success);
        }

	void DisplaySettings::notifyAudioFormatChange(
            uint32_t audioFormat
            )
	{
	     JsonObject params;
	     audioFormatToString(audioFormat, params);
             sendNotify("audioFormatChanged", params);
	}

    void DisplaySettings::notifyAtmosCapabilityChange(
            uint32_t atmosCaps
            )
    {
         JsonObject params;
         switch (static_cast<Exchange::IDeviceSettingsAudio::DolbyAtmosCapability>(atmosCaps)) {
        case Exchange::IDeviceSettingsAudio::DolbyAtmosCapability::AUDIO_DOLBY_ATMOS_METADATA:
            params["currentAtmosCapability"] = "ATMOS_SUPPORTED";
            break;
        case Exchange::IDeviceSettingsAudio::DolbyAtmosCapability::AUDIO_DOLBY_ATMOS_NOT_SUPPORTED:
            params["currentAtmosCapability"] = "ATMOS_NOT_SUPPORTED";
            break;
        default:
            LOGINFO("Atmos capability unknown, not notifying");
            break;
         }
             sendNotify("AtmosCapabilityChanged", params);
    }
	void DisplaySettings::notifyVideoFormatChange(
            uint32_t videoFormat
            )
	{
            JsonObject params;
            params["currentVideoFormat"] = getVideoFormatTypeToString(videoFormat);
            params["supportedVideoFormat"] = getSupportedVideoFormats();
            sendNotify("videoFormatChanged", params);
        }

        void DisplaySettings::notifyAssociatedAudioMixingChange(bool mixing)
        {
             JsonObject params;
             params["mixing"] = mixing;
             sendNotify("associatedAudioMixingChanged", params);
        }

        void DisplaySettings::notifyFaderControlChange(bool mixerbalance)
        {
             JsonObject params;
             params["mixerBalance"] = mixerbalance;
             sendNotify("faderControlChanged", params);
        }

        void DisplaySettings::notifyPrimaryLanguageChange(std::string pLang)
        {
             JsonObject params;
             params["primaryLanguage"] = pLang;
             sendNotify("primaryLanguageChanged", params);
        }

        void DisplaySettings::notifySecondaryLanguageChange(std::string sLang)
        {
             JsonObject params;
             params["secondaryLanguage"] = sLang;
             sendNotify("secondaryLanguageChanged", params);
        }

        uint32_t DisplaySettings::getBassEnhancer(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                int boost = 0;
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            int32_t bassBoost = 0;
                            if (audio->GetAudioBassEnhancer(it->second, bassBoost) == Core::ERROR_NONE) {
                                boost = bassBoost;
                                response["enable"] = boost ? true : false;
                                response["bassBoost"] = boost;
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::isSurroundDecoderEnabled(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                bool surroundDecoderEnable = false;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->IsAudioSurroundDecoderEnabled(it->second, surroundDecoderEnable) == Core::ERROR_NONE) {
                                response["surroundDecoderEnable"] = surroundDecoderEnable;
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::getGain (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            float gain = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->GetAudioGain(it->second, gain) == Core::ERROR_NONE) {
                            response["gain"] = to_string(gain);
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getMuted (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            bool muted = false;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->IsAudioMuted(it->second, muted) == Core::ERROR_NONE) {
                            response["muted"] = muted;
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getVolumeLevel (const JsonObject& parameters, JsonObject& response)
        {
            //LOGINFOMETHOD();
            bool success = true;
            float level = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        float vol = 0;
                        if (audio->GetAudioLevel(it->second, vol) == Core::ERROR_NONE) {
                            level = vol;
                            response["volumeLevel"] = to_string(level);
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getDRCMode(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                int mode = 0;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            int32_t drcMode = 0;
                            if (audio->GetAudioDRCMode(it->second, drcMode) == Core::ERROR_NONE) {
                                mode = drcMode;
                                response["DRCMode"] = mode ? "RF" : "line";
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::getSurroundVirtualizer(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            Exchange::IDeviceSettingsAudio::SurroundVirtualizer sv{0,0};
                            if (audio->GetAudioSurroundVirtualizer(it->second, sv) == Core::ERROR_NONE) {
                                response["enable"] = sv.mode ? true : false;
                                response["boost"] = sv.boost;
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::getSurroundVirtualizer2(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            Exchange::IDeviceSettingsAudio::SurroundVirtualizer sv{0,0};
                            if (audio->GetAudioSurroundVirtualizer(it->second, sv) == Core::ERROR_NONE) {
                                response["mode"] = sv.mode;
                                response["boost"] = sv.boost;
                            } else {
                                success = false;
                                response["mode"] = 0;
                                response["boost"] = 0;
                            }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }
	
        uint32_t DisplaySettings::getMISteering(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                bool enable = false;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->GetAudioMISteering(it->second, enable) == Core::ERROR_NONE) {
                                response["MISteeringEnable"] = enable;
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setVolumeLeveller(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "level");
                string sVolumeLeveller = parameters["level"].String();
                dsVolumeLeveller_t VolumeLeveller;
                bool isIntiger = Utils::isValidUnsignedInt ((char*)sVolumeLeveller.c_str());
                if (false == isIntiger) {
                    LOGWARN("level should be an unsigned integer");
                    returnResponse(false);
                }

                try {
                    VolumeLeveller.level = stoi(sVolumeLeveller);
                    if(VolumeLeveller.level == 0) {
                        VolumeLeveller.mode = 0; //Off
                    }
                    else {
                        VolumeLeveller.mode = 1; //On
                    }
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sVolumeLeveller);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            Exchange::IDeviceSettingsAudio::VolumeLeveller vl;
                            vl.level = static_cast<uint8_t>(VolumeLeveller.level);
                            vl.mode  = static_cast<uint8_t>(VolumeLeveller.mode);
                            if (audio->SetAudioVolumeLeveller(it->second, vl) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setVolumeLeveller2(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "mode");
		string sMode = parameters["mode"].String();
                string sLevel = parameters["level"].String();
                dsVolumeLeveller_t volumeLeveller;
                if ((Utils::isValidUnsignedInt ((char*)sMode.c_str()) == false) || (Utils::isValidUnsignedInt ((char*)sMode.c_str()) == false)) {
                    LOGWARN("mode and level should be an unsigned integer");
                    returnResponse(false);
                }

                try {
                        int mode = stoi(sMode);
                        if (mode == 0) {
                                volumeLeveller.mode = 0; //Off
				volumeLeveller.level = 0;
                        }
                        else if (mode == 1){
                                volumeLeveller.mode = 1; //On
				volumeLeveller.level = stoi(sLevel);
                        }
			else if (mode == 2) {
				volumeLeveller.mode = 2; //Auto
				volumeLeveller.level = 0;
			}
			else {
				LOGERR("Invalid volume leveller mode \n");
				returnResponse(false);
			}
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sMode);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            Exchange::IDeviceSettingsAudio::VolumeLeveller vl;
                            vl.level = static_cast<uint8_t>(volumeLeveller.level);
                            vl.mode  = static_cast<uint8_t>(volumeLeveller.mode);
                            if (audio->SetAudioVolumeLeveller(it->second, vl) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }


        uint32_t DisplaySettings::enableSurroundDecoder(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "surroundDecoderEnable");
                string sEnableSurroundDecoder = parameters["surroundDecoderEnable"].String();
                bool enableSurroundDecoder = false;
                try {
                        enableSurroundDecoder= parameters["surroundDecoderEnable"].Boolean();
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sEnableSurroundDecoder);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->EnableAudioSurroundDecoder(it->second, enableSurroundDecoder) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setBassEnhancer(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "bassBoost");
                string sBassBoost = parameters["bassBoost"].String();
                int bassBoost = 0;
                bool isIntiger = Utils::isValidUnsignedInt ((char*)sBassBoost.c_str());
                if (false == isIntiger) {
                    LOGWARN("bassBoost should be an unsigned integer");
                    returnResponse(false);
                }
                try {
                        bassBoost = stoi(sBassBoost);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sBassBoost);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->SetAudioBassEnhancer(it->second, bassBoost) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setSurroundVirtualizer(const JsonObject& parameters, JsonObject& response)
        {
               LOGINFOMETHOD();
               returnIfParamNotFound(parameters, "boost");
               string sSurroundVirtualizer = parameters["boost"].String();
               dsSurroundVirtualizer_t surroundVirtualizer;
               bool isIntiger = Utils::isValidUnsignedInt ((char*)sSurroundVirtualizer.c_str());
               if (false == isIntiger) {
                   LOGWARN("boost should be an unsigned integer");
                   returnResponse(false);
               }

               try {
                  surroundVirtualizer.boost = stoi(sSurroundVirtualizer);
		  if(surroundVirtualizer.boost == 0) {
			  surroundVirtualizer.mode = 0; //Off
		  }
		  else {
			  surroundVirtualizer.mode = 1; //On
		  }
               }catch (const device::Exception& err) {
                  LOG_DEVICE_EXCEPTION1(sSurroundVirtualizer);
                              returnResponse(false);
               }
               bool success = true;
               string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
               {
                   auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                   if (audio != nullptr) {
                       const auto it = _audioPortHandles.find(audioPort);
                       if (it != _audioPortHandles.end()) {
                           Exchange::IDeviceSettingsAudio::SurroundVirtualizer sv;
                           sv.mode = static_cast<uint8_t>(surroundVirtualizer.mode);
                           sv.boost = static_cast<uint8_t>(surroundVirtualizer.boost);
                           if (audio->SetAudioSurroundVirtualizer(it->second, sv) != Core::ERROR_NONE) { success = false; }
                       } else { success = false; }
                       audio->Release();
                   } else { success = false; }
               }
               returnResponse(success);
        }

        uint32_t DisplaySettings::setSurroundVirtualizer2(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "mode");
                string sMode = parameters["mode"].String();
                string sBoost = parameters["boost"].String();
                dsSurroundVirtualizer_t surroundVirtualizer;

                if ((Utils::isValidUnsignedInt ((char*)sMode.c_str()) == false) || (Utils::isValidUnsignedInt ((char*)sBoost.c_str()) == false)) {
                    LOGWARN("mode and boost value should be an unsigned integer");
                    returnResponse(false);
                }

                try {
                        int mode = stoi(sMode);
                        if (mode == 0) {
                                surroundVirtualizer.mode = 0; //Off
                                surroundVirtualizer.boost = 0;
                        }
                        else if (mode == 1){
                                surroundVirtualizer.mode = 1; //On
                                surroundVirtualizer.boost = stoi(sBoost);
                        }
                        else if (mode == 2) {
                                surroundVirtualizer.mode = 2; //Auto
                                surroundVirtualizer.boost = 0;
                        }
                        else {
                                LOGERR("Invalid surround virtualizer mode \n");
                                returnResponse(false);
                        }
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sMode);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            Exchange::IDeviceSettingsAudio::SurroundVirtualizer sv;
                            sv.mode = static_cast<uint8_t>(surroundVirtualizer.mode);
                            sv.boost = static_cast<uint8_t>(surroundVirtualizer.boost);
                            if (audio->SetAudioSurroundVirtualizer(it->second, sv) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setMISteering(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "MISteeringEnable");
                string sMISteering = parameters["MISteeringEbnable"].String();
                bool MISteering = false;
                try {
                        MISteering = parameters["MISteeringEnable"].Boolean();
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sMISteering);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->SetAudioMISteering(it->second, MISteering) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success)
        }

        uint32_t DisplaySettings::setGain(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "gain");
                string sGain = parameters["gain"].String();
                float newGain = 0;
                try {
                        newGain = stof(sGain);
                        if ((newGain < -2080) || (newGain > 480)) {
                            LOGERR("Gain value being set to an invalid value newGain: %f \n",newGain);
                            returnResponse(false);
                        }
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sGain);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->SetAudioGain(it->second, newGain) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setMuted (const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "muted");
                string sMuted = parameters["muted"].String();
                bool muted = false;
                static bool cache_muted = false;
                try {
                        muted = parameters["muted"].Boolean();
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sMuted);
                        returnResponse(false);
                }

                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                LOGWARN("DisplaySettings::setMuted called Audio Port :%s muted:%d\n", audioPort.c_str(), muted);
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->SetAudioMute(it->second, muted) == Core::ERROR_NONE) {
                                if (cache_muted != muted) {
                                    cache_muted = muted;
                                    JsonObject params;
                                    params["muted"] = muted;
                                    sendNotify("muteStatusChanged", params);
                                }
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setVolumeLevel(const JsonObject& parameters, JsonObject& response)
        {
                //LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "volumeLevel");
                string sLevel = parameters["volumeLevel"].String();
                float level = 0;
                try {
                        level = stof(sLevel);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sLevel);
                        returnResponse(false);
                }

                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->SetAudioLevel(it->second, level) == Core::ERROR_NONE) {
                                static float cache_level = -1;
                                if (static_cast<int>(cache_level) != static_cast<int>(level)) {
                                    cache_level = level;
                                    JsonObject params;
                                    params["volumeLevel"] = static_cast<int>(level);
                                    sendNotify("volumeLevelChanged", params);
                                }
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setDRCMode(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "DRCMode");
                string sDRCMode = parameters["DRCMode"].String();
                int DRCMode = 0;
                bool isIntiger = Utils::isValidUnsignedInt ((char*)sDRCMode.c_str());
                if (false == isIntiger) {
                    LOGWARN("DRCMode should be an unsigned integer");
                    returnResponse(false);
                }
                try {
                        DRCMode = stoi(sDRCMode);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sDRCMode);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->SetAudioDRCMode(it->second, DRCMode) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setMS12AudioCompression (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "compresionLevel");

            string sCompresionLevel = parameters["compresionLevel"].String();
                       int compresionLevel = 0;
            try {
                compresionLevel = stoi(sCompresionLevel);
            }catch (const std::exception &err) {
               LOGERR("Failed to parse compresionLevel '%s'", sCompresionLevel.c_str());
                          returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioCompression(it->second, compresionLevel) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getMS12AudioCompression (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;
                       int compressionlevel = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        int32_t level = 0;
                        if (audio->GetAudioCompression(it->second, level) == Core::ERROR_NONE) {
                            compressionlevel = level;
                            response["compressionlevel"] = compressionlevel;
                            response["enable"] = (compressionlevel ? true : false);
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setDolbyVolumeMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "dolbyVolumeMode");

            string sDolbyVolumeMode = parameters["dolbyVolumeMode"].String();
            bool dolbyVolumeMode = false;

            try
            {
                dolbyVolumeMode = parameters["dolbyVolumeMode"].Boolean();
            }
            catch (const std::exception &err)
            {
               LOGERR("Failed to parse dolbyVolumeMode '%s'", sDolbyVolumeMode.c_str());
               returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioDolbyVolumeMode(it->second, dolbyVolumeMode) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getDolbyVolumeMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        bool enabled = false;
                        if (audio->GetAudioDolbyVolumeMode(it->second, enabled) == Core::ERROR_NONE) {
                            response["dolbyVolumeMode"] = enabled;
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setDialogEnhancement (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "enhancerlevel");

            string sEnhancerlevel = parameters["enhancerlevel"].String();
                       int enhancerlevel = 0;
            try {
                enhancerlevel = stoi(sEnhancerlevel);
            }catch (const std::exception &err) {
               LOGERR("Failed to parse enhancerlevel '%s'", sEnhancerlevel.c_str());
                          returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioDialogEnhancement(it->second, enhancerlevel) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getDialogEnhancement (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;
                       int enhancerlevel = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        int32_t level = 0;
                        if (audio->GetAudioDialogEnhancement(it->second, level) == Core::ERROR_NONE) {
                            enhancerlevel = level;
                            response["enable"] = (enhancerlevel ? true : false);
                            response["enhancerlevel"] = enhancerlevel;
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setIntelligentEqualizerMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "intelligentEqualizerMode");

            string sIntelligentEqualizerMode = parameters["intelligentEqualizerMode"].String();
                       int intelligentEqualizerMode = 0;
            try {
                intelligentEqualizerMode = stoi(sIntelligentEqualizerMode);
            }catch (const std::exception &err) {
               LOGERR("Failed to parse intelligentEqualizerMode '%s'", sIntelligentEqualizerMode.c_str());
                          returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioIntelligentEqualizerMode(it->second, intelligentEqualizerMode) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getIntelligentEqualizerMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;
                       int intelligentEqualizerMode = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        int32_t mode = 0;
                        if (audio->GetAudioIntelligentEqualizerMode(it->second, mode) == Core::ERROR_NONE) {
                            intelligentEqualizerMode = mode;
                            response["enable"] = (intelligentEqualizerMode ? true : false);
                            response["mode"] = intelligentEqualizerMode;
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }


        uint32_t DisplaySettings::setGraphicEqualizerMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "graphicEqualizerMode");

            string sGraphicEqualizerMode = parameters["graphicEqualizerMode"].String();
                       int graphicEqualizerMode = 0;
            try {
                graphicEqualizerMode = stoi(sGraphicEqualizerMode);
            }catch (const std::exception &err) {
               LOGERR("Failed to parse graphicEqualizerMode '%s'", sGraphicEqualizerMode.c_str());
                          returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioGraphicEqualizerMode(it->second, graphicEqualizerMode) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getGraphicEqualizerMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;
                       int graphicEqualizerMode = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        int32_t mode = 0;
                        if (audio->GetAudioGraphicEqualizerMode(it->second, mode) == Core::ERROR_NONE) {
                            graphicEqualizerMode = mode;
                            response["enable"] = (graphicEqualizerMode ? true : false);
                            response["mode"] = graphicEqualizerMode;
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }


        uint32_t DisplaySettings::setMS12AudioProfile (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            bool success = true;

            returnIfParamNotFound(parameters, "ms12AudioProfile");
            string audioProfileName = parameters["ms12AudioProfile"].String();

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioMS12Profile(it->second, audioProfileName) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }

	    returnResponse(success);
        }

        uint32_t DisplaySettings::setMS12ProfileSettingsOverride(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;

            returnIfParamNotFound(parameters, "operation");
            string audioProfileState = parameters["operation"].String();

            returnIfParamNotFound(parameters, "profileName");
            string audioProfileName = parameters["profileName"].String();

            returnIfParamNotFound(parameters, "ms12SettingsName");
            string audioProfileSettingsName = parameters["ms12SettingsName"].String();

            returnIfParamNotFound(parameters, "ms12SettingsValue");
            string audioProfileSettingValue = parameters["ms12SettingsValue"].String();


            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                // Map "operation" string (ADD/REMOVE) to MS12ProfileState enum
                Exchange::IDeviceSettingsAudio::MS12ProfileState profileState =
                    Exchange::IDeviceSettingsAudio::MS12ProfileState::AUDIO_MS12_PROFILE_STATE_ADD;
                if (audioProfileState == "REMOVE" || audioProfileState == "remove") {
                    profileState = Exchange::IDeviceSettingsAudio::MS12ProfileState::AUDIO_MS12_PROFILE_STATE_REMOVE;
                }
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioMS12SettingsOverride(it->second, audioProfileName, audioProfileSettingsName,
                                                               audioProfileSettingValue, profileState) != Core::ERROR_NONE) {
                            success = false;
                        }
                    } else {
                        success = false;
                    }
                    audio->Release();
                } else {
                    success = false;
                }
            }
            returnResponse(success);
        }


        uint32_t DisplaySettings::getMS12AudioProfile (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;

	    string audioProfileName;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->GetAudioMS12Profile(it->second, audioProfileName) == Core::ERROR_NONE) {
                            response["ms12AudioProfile"] = audioProfileName;
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }


        uint32_t DisplaySettings::getSupportedMS12AudioProfiles(const JsonObject& parameters, JsonObject& response)
        {   //sample response: {"success":true,"supportedMS12AudioProfiles":["Off","Music","Movie","Game","Voice","Night","User"]}
            LOGINFOMETHOD();
            vector<string> supportedProfiles;
	    string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        Exchange::IDeviceSettingsAudio::IDeviceSettingsAudioMS12AudioProfileIterator* it2 = nullptr;
                        if (audio->GetAudioMS12ProfileList(it->second, it2) == Core::ERROR_NONE && it2 != nullptr) {
                            Exchange::IDeviceSettingsAudio::MS12AudioProfile profile;
                            while (it2->Next(profile)) {
                                supportedProfiles.push_back(profile.audioProfile);
                            }
                            it2->Release();
                        }
                    }
                    audio->Release();
                }
            }
            setResponseArray(response, "supportedMS12AudioProfiles", supportedProfiles);
            returnResponse(true);
        }


        uint32_t DisplaySettings::setAssociatedAudioMixing(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "mixing");
                string sMixing = parameters["mixing"].String();
                bool mixing = false;
                try {
                        mixing = parameters["mixing"].Boolean();
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sMixing);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->SetAssociatedAudioMixing(it->second, mixing) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success)
        }



        uint32_t DisplaySettings::getAssociatedAudioMixing(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                bool mixing = false;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->GetAssociatedAudioMixing(it->second, mixing) == Core::ERROR_NONE) {
                                response["mixing"] = mixing;
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setFaderControl(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "mixerBalance");
                string sMixerBalance = parameters["mixerBalance"].String();
                int mixerBalance = 0;
                bool isIntiger = Utils::isValidInt ((char*)sMixerBalance.c_str());
                if (false == isIntiger) {
                    LOGWARN("mixerBalance should be an integer");
                    returnResponse(false);
                }
                try {
                        mixerBalance = stoi(sMixerBalance);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sMixerBalance);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            if (audio->SetAudioFaderControl(it->second, mixerBalance) != Core::ERROR_NONE) { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }


        uint32_t DisplaySettings::getFaderControl(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                int mixerBalance = 0;
                {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        const auto it = _audioPortHandles.find(audioPort);
                        if (it != _audioPortHandles.end()) {
                            int32_t balance = 0;
                            if (audio->GetAudioFaderControl(it->second, balance) == Core::ERROR_NONE) {
                                mixerBalance = balance;
                                response["mixerBalance"] = mixerBalance;
                            } else { success = false; }
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setPrimaryLanguage (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            bool success = true;

            returnIfParamNotFound(parameters, "lang");
            string primaryLanguage = parameters["lang"].String();

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioPrimaryLanguage(it->second, primaryLanguage) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }

            returnResponse(success);
        }


        uint32_t DisplaySettings::getPrimaryLanguage (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;

            string primaryLanguage;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->GetAudioPrimaryLanguage(it->second, primaryLanguage) == Core::ERROR_NONE) {
                            response["lang"] = primaryLanguage;
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setSecondaryLanguage (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            bool success = true;

            returnIfParamNotFound(parameters, "lang");
            string secondaryLanguage = parameters["lang"].String();

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioSecondaryLanguage(it->second, secondaryLanguage) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }

            returnResponse(success);
        }


        uint32_t DisplaySettings::getSecondaryLanguage (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;

            string secondaryLanguage;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->GetAudioSecondaryLanguage(it->second, secondaryLanguage) == Core::ERROR_NONE) {
                            response["lang"] = secondaryLanguage;
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }


        uint32_t DisplaySettings::getAudioDelay (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            bool success = true;
            string audioPort = parameters["audioPort"].String();//empty value will browse all ports

            if (!checkPortName(audioPort))
                audioPort = "HDMI0";

            uint32_t audioDelayMs = 0;
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->GetAudioDelay(it->second, audioDelayMs) == Core::ERROR_NONE) {
                            response["audioDelay"] = std::to_string(audioDelayMs);
                        } else { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }

            returnResponse(success);
        }

        uint32_t DisplaySettings::setAudioDelay (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();

			returnIfParamNotFound(parameters, "audioDelay");

            string sAudioDelayMs = parameters["audioDelay"].String();
			int audioDelayMs = 0;
            try {
                audioDelayMs = stoi(sAudioDelayMs);
            } catch (const std::exception &err) {
                LOGERR("Failed to parse audioDelay '%s'", sAudioDelayMs.c_str());
                returnResponse(false);
            }
            
            if ( audioDelayMs < 0 )
            {
                LOGERR("audioDelay '%s', Should be a postiive value", sAudioDelayMs.c_str());
                returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters["audioPort"].String();//empty value will browse all ports

            if (!checkPortName(audioPort))
                audioPort = "HDMI0";

            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioDelay(it->second, static_cast<uint32_t>(audioDelayMs)) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getSinkAtmosCapability (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "NULL";
            {
                const string usePort = (audioPort != "NULL") ? audioPort : "HDMI0";
                const auto it = _audioPortHandles.find(usePort);
                if (it != _audioPortHandles.end()) {
                    auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                    if (audio != nullptr) {
                        Exchange::IDeviceSettingsAudio::DolbyAtmosCapability caps = Exchange::IDeviceSettingsAudio::DolbyAtmosCapability::AUDIO_DOLBY_ATMOS_NOT_SUPPORTED;
                        if (audio->GetAudioSinkDeviceAtmosCapability(it->second, caps) == Core::ERROR_NONE) {
                            response["atmos_capability"] = static_cast<int>(caps);
                        } else { success = false; }
                        audio->Release();
                    } else { success = false; }
                } else {
                    LOGERR("getSinkAtmosCapability: port %s not found", usePort.c_str());
                    success = false;
                }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setAudioAtmosOutputMode (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "enable");

            string sEnable = parameters["enable"].String();
            int enable = parameters["enable"].Boolean();

            bool success = true;
            {
                const string audioPort2 = "HDMI0";
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort2);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioAtmosOutputMode(it->second, static_cast<bool>(enable)) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

	uint32_t DisplaySettings::setForceHDRMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "hdr_mode");

            string sMode = parameters["hdr_mode"].String();
            bool success = false;
            {
                uint32_t mode = getVideoFormatTypeFromString(sMode.c_str());
                string videoPort = parameters.HasLabel("videoPort") ? parameters["videoPort"].String() : "HDMI0";
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoPort);
                    if (it != _videoPortHandles.end()) {
                        if (vp->SetForceHDRMode(it->second, static_cast<Exchange::IDeviceSettingsVideoPort::HDRStandard>(mode)) == Core::ERROR_NONE) {
                            success = true;
                            LOGINFO("setForceHDRMode set successfully \n");
                        }
                    } else {
                        LOGERR("setForceHDRMode failure: port %s not found!\n", videoPort.c_str());
                    }
                    vp->Release();
                }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getPreferredColorDepth(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"colorDepth":"10 Bit","success":true}
            LOGINFOMETHOD();
            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            bool persist = parameters.HasLabel("persist") ? parameters["persist"].Boolean() : true;
            bool success = true;
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        DisplayColorDepth colorDepth = DisplayColorDepth::DS_DISPLAY_COLORDEPTH_UNKNOWN;
                        if (vp->GetPreferredColorDepth(it->second, colorDepth, persist) == Core::ERROR_NONE) {
                            switch (colorDepth) {
                                case DisplayColorDepth::DS_DISPLAY_COLORDEPTH_8BIT:  response["colorDepth"] = "8 Bit"; break;
                                case DisplayColorDepth::DS_DISPLAY_COLORDEPTH_10BIT: response["colorDepth"] = "10 Bit"; break;
                                case DisplayColorDepth::DS_DISPLAY_COLORDEPTH_12BIT: response["colorDepth"] = "12 Bit"; break;
                                case DisplayColorDepth::DS_DISPLAY_COLORDEPTH_AUTO:  response["colorDepth"] = "Auto"; break;
                                default: success = false; break;
                            }
                        } else { success = false; }
                    } else { success = false; }
                    vp->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setPreferredColorDepth(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"success":true}
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "videoDisplay");
            returnIfParamNotFound(parameters, "colorDepth");

            string videoDisplay = parameters["videoDisplay"].String();
            string strColorDepth = parameters["colorDepth"].String();

            bool persist = parameters.HasLabel("persist") ? parameters["persist"].Boolean() : true;

            bool success = true;
            try
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        Exchange::IDeviceSettingsVideoPort::DisplayColorDepth cd = Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_UNKNOWN;
                        if (strColorDepth == "8 Bit") cd = Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_8BIT;
                        else if (strColorDepth == "10 Bit") cd = Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_10BIT;
                        else if (strColorDepth == "12 Bit") cd = Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_12BIT;
                        else if (strColorDepth == "Auto") cd = Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_AUTO;
                        if (cd != Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_UNKNOWN) {
                            if (vp->SetPreferredColorDepth(it->second, cd, persist) != Core::ERROR_NONE) { success = false; }
                        } else {
                            LOGERR("UNKNOWN color depth: %s", strColorDepth.c_str());
                            success = false;
                        }
                    } else { success = false; }
                    vp->Release();
                } else { success = false; }
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(videoDisplay, strColorDepth);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setAudioDucking(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            returnIfStringParamNotFound(parameters, "mode"); // "mute" | "attenuate" | "raw"

            std::string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            std::string mode = parameters["mode"].String();

            uint32_t action = 0; // STOP
            uint32_t type = 0;   // ABSOLUTE
            uint8_t level = 100;

            if (mode == "mute")
            {
                returnIfBooleanParamNotFound(parameters, "mute");
                bool mute = parameters["mute"].Boolean();

                action = mute ? 1 : 0; // START : STOP
                type = 0;              // ABSOLUTE
                level = mute ? 0 : 100;
            }
            else if (mode == "attenuate")
            {
                returnIfBooleanParamNotFound(parameters, "enable");
                returnIfBooleanParamNotFound(parameters, "relative");
                returnIfNumberParamNotFound(parameters, "volume"); // 0.0..1.0

                bool enable = parameters["enable"].Boolean();
                bool relative = parameters["relative"].Boolean();
                double volume = parameters["volume"].Number();

                if (volume < 0.0 || volume > 1.0)
                {
                    LOGERR("Invalid volume %f", volume);
                    returnResponse(false);
                }

                action = enable ? 1 : 0; // START : STOP
                type = relative ? 1 : 0; // RELATIVE : ABSOLUTE
                level = static_cast<uint8_t>((volume * 100.0) + 0.5);
            }
            else if (mode == "raw")
            {
                returnIfStringParamNotFound(parameters, "action");      // "start" | "stop"
                returnIfStringParamNotFound(parameters, "duckingType"); // "absolute" | "relative"
                returnIfNumberParamNotFound(parameters, "level");       // 0..100

                std::string actionStr = parameters["action"].String();
                std::string typeStr = parameters["duckingType"].String();
                
                std::string levelStr = parameters["level"].String();
                LOGINFO("setAudioDucking raw: level Content-type=%d String='%s' Number=%s",
                        (int)parameters["level"].Content(),
                        levelStr.c_str(),
                        std::to_string(parameters["level"].Number()).c_str());

                // String() preserves the original token text in this Thunder version.
                // Validate it is actually an unsigned integer before using Number().
                if (!Utils::isValidUnsignedInt((char*)levelStr.c_str()))
                {
                    LOGERR("Invalid level value '%s': must be a non-negative integer 0..100", levelStr.c_str());
                    returnResponse(false);
                }

                int reqLevel = static_cast<int>(std::round(parameters["level"].Number()));

                if (reqLevel < 0 || reqLevel > 100)
                {
                    LOGERR("Invalid level %d", reqLevel);
                    returnResponse(false);
                }

                if (actionStr == "start") {
                    action = 1; // START
                } else if (actionStr == "stop") {
                    action = 0; // STOP
                } else {
                    LOGERR("Invalid action %s", actionStr.c_str());
                    returnResponse(false);
                }

                if (typeStr == "absolute") {
                    type = 0; // ABSOLUTE
                } else if (typeStr == "relative") {
                    type = 1; // RELATIVE
                } else {
                    LOGERR("Invalid duckingType %s", typeStr.c_str());
                    returnResponse(false);
                }

                level = static_cast<uint8_t>(reqLevel);
            }
            else
            {
                LOGERR("Invalid mode %s", mode.c_str());
                returnResponse(false);
            }

            bool success = true;
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioDucking(it->second,
                                static_cast<AudioDuckingType>(type),
                                static_cast<AudioDuckingAction>(action),
                                level) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setEnableVideoPort(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            returnIfParamNotFound(parameters, "videoDisplay");        // e.g. "HDMI0"
			returnIfBooleanParamNotFound(parameters, "enable");       // true | false

            string videoDisplay = parameters["videoDisplay"].String();
            bool enable = parameters["enable"].Boolean();

            bool success = false;
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        if (vp->EnableVideoPort(it->second, enable) == Core::ERROR_NONE) { success = true; }
                    }
                    vp->Release();
                }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getEnableVideoPort(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            returnIfStringParamNotFound(parameters, "videoDisplay");

            string videoDisplay = parameters["videoDisplay"].String();
            bool success = false;
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        bool enabled = false;
                        if (vp->IsVideoPortEnabled(it->second, enabled) == Core::ERROR_NONE) {
                            response["enable"] = enabled;
                            success = true;
                        }
                    }
                    vp->Release();
                }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getSupportedVideoCodingFormats(const JsonObject& parameters, JsonObject& response)
        {   //sample response: {"supportedFormats":["HEVC","H264","MPEG2"],"success":true}
            LOGINFOMETHOD();
            JsonArray supportedFormats;
            bool success = false;
            {
                auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                if (vd != nullptr) {
                    int32_t formats = 0;
                    if (vd->GetSupportedVideoCodingFormats(_videoDeviceHandle, formats) == Core::ERROR_NONE) {
                        if (formats & static_cast<int32_t>(VideoCodec::DS_VIDEO_CODEC_MPEGHPART2)) supportedFormats.Add("HEVC");
                        if (formats & static_cast<int32_t>(VideoCodec::DS_VIDEO_CODEC_MPEG4PART10)) supportedFormats.Add("H264");
                        if (formats & static_cast<int32_t>(VideoCodec::DS_VIDEO_CODEC_MPEG2)) supportedFormats.Add("MPEG2");
                        success = true;
                    }
                    vd->Release();
                }
            }
            response["supportedFormats"] = supportedFormats;
            returnResponse(success);
        }


        uint32_t DisplaySettings::getVideoCodecInfo(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            string codec = "MPEGH-Part2"; // default keeps TR-069 behavior
            if (parameters.HasLabel("codec")) {
                codec = parameters["codec"].String();
            }

            bool success = false;
            {
                Exchange::IDeviceSettingsVideoDevice::VideoCodec vc =
                    Exchange::IDeviceSettingsVideoDevice::VideoCodec::DS_VIDEO_CODEC_MPEGHPART2;
                if (codec == "MPEG4-Part10" || codec == "H264")
                    vc = Exchange::IDeviceSettingsVideoDevice::VideoCodec::DS_VIDEO_CODEC_MPEG4PART10;
                else if (codec == "MPEG2")
                    vc = Exchange::IDeviceSettingsVideoDevice::VideoCodec::DS_VIDEO_CODEC_MPEG2;
                auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                if (vd != nullptr) {
                    Exchange::IDeviceSettingsVideoDevice::IDeviceSettingsVideoCodecProfileSupportIterator* iter = nullptr;
                    if (vd->GetCodecInfo(_videoDeviceHandle, vc, iter) == Core::ERROR_NONE && iter != nullptr) {
                        JsonArray entries;
                        Exchange::IDeviceSettingsVideoDevice::VideoCodecProfileSupport ps{};
                        while (iter->Next(ps)) {
                            JsonObject item;
                            item["profile"] = static_cast<int>(ps.profile);
                            item["level"]   = ps.level;
                            entries.Add(item);
                        }
                        response["numberOfEntries"] = static_cast<int>(entries.Length());
                        response["entries"] = entries;
                        iter->Release();
                        success = true;
                    }
                    vd->Release();
                }
            }
            returnResponse(success);
        }

        static const char* encodingToString(int enc)
        {
            switch (enc) {
                case 0: return "NONE";
                case 1: return "DISPLAY";
                case 2: return "PCM";
                case 3: return "AC3";
                case 4: return "EAC3";
                default: return "UNKNOWN";
            }
        }


        uint32_t DisplaySettings::getAudioEncoding(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            string audioPort;
            audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            if (audioPort.empty()) {
                LOGERR("Invalid audioPort");
                returnResponse(false);
            }

            bool success = false;
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        Exchange::IDeviceSettingsAudio::AudioEncoding enc = Exchange::IDeviceSettingsAudio::AudioEncoding::AUDIO_ENCODING_NONE;
                        if (audio->GetAudioEncoding(it->second, enc) == Core::ERROR_NONE) {
                            response["audioPort"] = audioPort;
                            response["encoding"] = encodingToString(static_cast<int>(enc));
                            response["encodingId"] = static_cast<int>(enc);
                            success = true;
                        }
                    }
                    audio->Release();
                }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setAudioEncoding(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            returnIfStringParamNotFound(parameters, "encoding");
            string encoding = parameters["encoding"].String();

            bool success = false;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            if (audioPort.empty()) {
                returnResponse(success);
            }

            {
                // SetAudioEncoding not available in COM-RPC interface
                LOGWARN("setAudioEncoding: COM-RPC SetAudioEncoding not supported");
                (void)encoding; (void)audioPort;
            }

            returnResponse(success);
        }

        uint32_t DisplaySettings::getDisplayAspectRatio(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            bool success = false;
            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            {
                auto* disp = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
                if (disp != nullptr) {
                    const auto it = _displayHandles.find(videoDisplay);
                    if (it != _displayHandles.end()) {
                        Exchange::IDeviceSettingsDisplay::DisplayVideoAspectRatio ar =
                            Exchange::IDeviceSettingsDisplay::DisplayVideoAspectRatio::DS_DISPLAY_ASPECT_RATIO_16X9;
                        if (disp->GetDisplayAspectRatio(it->second, ar) == Core::ERROR_NONE) {
                            string aspectRatioName;
                            switch (ar) {
                                case Exchange::IDeviceSettingsDisplay::DisplayVideoAspectRatio::DS_DISPLAY_ASPECT_RATIO_4X3:
                                    aspectRatioName = "4x3"; break;
                                case Exchange::IDeviceSettingsDisplay::DisplayVideoAspectRatio::DS_DISPLAY_ASPECT_RATIO_16X9:
                                    aspectRatioName = "16x9"; break;
                                default:
                                    aspectRatioName = "Unknown"; break;
                            }
                            response["aspectRatio"] = aspectRatioName;
                            response["aspectRatioValue"] = static_cast<int>(ar);
                            success = true;
                        }
                    }
                    disp->Release();
                }
            }

            returnResponse(success);
        }

        uint32_t DisplaySettings::getColorDepthCapabilities(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"capabilities":["8 Bit","10 Bit","12 Bit","Auto"]}
            LOGINFOMETHOD();
            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            vector<string> colorDepthCapabilities;
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoDisplay);
                    if (it != _videoPortHandles.end()) {
                        uint32_t capabilities = 0;
                        if (vp->GetColorDepthCapabilities(it->second, capabilities) == Core::ERROR_NONE) {
                            if (!capabilities) colorDepthCapabilities.emplace_back("none");
                            if (capabilities & static_cast<uint32_t>(DisplayColorDepth::DS_DISPLAY_COLORDEPTH_8BIT))  colorDepthCapabilities.emplace_back("8 Bit");
                            if (capabilities & static_cast<uint32_t>(DisplayColorDepth::DS_DISPLAY_COLORDEPTH_10BIT)) colorDepthCapabilities.emplace_back("10 Bit");
                            if (capabilities & static_cast<uint32_t>(DisplayColorDepth::DS_DISPLAY_COLORDEPTH_12BIT)) colorDepthCapabilities.emplace_back("12 Bit");
                            if (capabilities & static_cast<uint32_t>(DisplayColorDepth::DS_DISPLAY_COLORDEPTH_AUTO))  colorDepthCapabilities.emplace_back("Auto");
                        }
                    }
                    vp->Release();
                }
            }
            setResponseArray(response, "capabilities", colorDepthCapabilities);
            returnResponse(true);
        }

	uint32_t DisplaySettings::getSupportedMS12Config(const JsonObject& parameters, JsonObject& response)
       {
           LOGINFOMETHOD();
           bool success = true;
           std::string type;
           {
               auto* host = AcquireSubInterface<Exchange::IDeviceSettingsHost>();
               if (host != nullptr) {
                   if (host->GetMS12ConfigType(type) == Core::ERROR_NONE) {
                       LOGINFO("Platform supports MS12 Config Z\n");
                       response["ms12config"] = type;
                   } else { success = false; }
                   host->Release();
               } else { success = false; }
           }
           returnResponse(success);
       }

        bool DisplaySettings::setUpHdmiCecSinkArcRouting (bool arcEnable)
        {
            bool success = true;

            PluginHost::IShell::state state;
            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);

                getHdmiCecSinkPlugin();
                if (!m_client) {
                    LOGERR("HdmiCecSink Initialisation failed\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;

                    if(arcEnable) {
                        param["enabled"] = true;
                    }else {
                        param["enabled"] = false;
                    }

                    LOGINFO("ARC Routing - %d \n", arcEnable);
                    {
                        Utils::Synchro::UnlockApiGuard<DisplaySettings> unlockApi;
                        m_client->Invoke<JsonObject, JsonObject>(2000, "setupARCRouting", param, hdmiCecSinkResult);
                    }
                    if (!hdmiCecSinkResult["success"].Boolean()) {
			success = false;
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
	    else {
		success = false;
                LOGERR("HdmiCecSink plugin not ready\n");
            }

            return success;
	}
	
	bool DisplaySettings::getHdmiCecSinkCecEnableStatus ()
        {
            bool cecEnable = false;

            PluginHost::IShell::state state;
            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);

                getHdmiCecSinkPlugin();
                if (!m_client) {
                    LOGERR("HdmiCecSink Initialisation failed\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;
                    {
                        Utils::Synchro::UnlockApiGuard<DisplaySettings> unlockApi;
                        m_client->Invoke<JsonObject, JsonObject>(2000, "getEnabled", param, hdmiCecSinkResult);
                    }

		    cecEnable = hdmiCecSinkResult["enabled"].Boolean();
		    LOGINFO("get-cecEnabled [%d]\n",cecEnable);

                    if (!hdmiCecSinkResult["success"].Boolean()) {
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
            else {
                LOGERR("HdmiCecSink plugin not ready\n");
            }
            return cecEnable;
        }

	bool DisplaySettings::getHdmiCecSinkAudioDeviceConnectedStatus ()
        {
            bool hdmiAudioDeviceDetected = false;

            PluginHost::IShell::state state;
            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);

                getHdmiCecSinkPlugin();
                if (!m_client) {
                    LOGERR("HdmiCecSink Initialisation failed\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;

                    {
                        Utils::Synchro::UnlockApiGuard<DisplaySettings> unlockApi;
                        m_client->Invoke<JsonObject, JsonObject>(2000, "getAudioDeviceConnectedStatus", param, hdmiCecSinkResult);
                    }

                    hdmiAudioDeviceDetected = hdmiCecSinkResult["connected"].Boolean();
                    LOGINFO("getAudioDeviceConnectedStatus [%d]\n",hdmiAudioDeviceDetected);

                    if (!hdmiCecSinkResult["success"].Boolean()) {
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
            else {
                LOGERR("HdmiCecSink plugin not ready\n");
            }
            return hdmiAudioDeviceDetected;
        }

        bool DisplaySettings::sendHdmiCecSinkAudioDevicePowerOn ()
        {
            bool success = true;

            PluginHost::IShell::state state;
            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);

                getHdmiCecSinkPlugin();
                if (!m_client) {
                    LOGERR("HdmiCecSink Initialisation failed\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;

                    LOGINFO("Send Audio Device Power On !!!\n");
                    {
                        Utils::Synchro::UnlockApiGuard<DisplaySettings> unlockApi;
                        m_client->Invoke<JsonObject, JsonObject>(2000, "sendAudioDevicePowerOnMessage", param, hdmiCecSinkResult);
                    }
                    if (!hdmiCecSinkResult["success"].Boolean()) {
                        success = false;
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
            else {
                success = false;
                LOGERR("HdmiCecSink plugin not ready\n");
            }

            return success;
        }

        bool DisplaySettings::requestShortAudioDescriptor()
        {
            bool success = true;

            PluginHost::IShell::state state;
            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);

                getHdmiCecSinkPlugin();
                if (!m_client) {
                    LOGERR("HdmiCecSink plugin not accessible\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;

                    LOGINFO("Requesting Short Audio Descriptor \n");
                    {
                        Utils::Synchro::UnlockApiGuard<DisplaySettings> unlockApi;
                        m_client->Invoke<JsonObject, JsonObject>(2000, "requestShortAudioDescriptor", param, hdmiCecSinkResult);
                    }
                    if (!hdmiCecSinkResult["success"].Boolean()) {
                        success = false;
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
            else {
                success = false;
                LOGERR("HdmiCecSink plugin not ready\n");
            }

            return success;
        }

        bool DisplaySettings::requestAudioDevicePowerStatus()
        {
            bool success = true;

            PluginHost::IShell::state state;
            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);

                getHdmiCecSinkPlugin();
                if (!m_client) {
                    LOGERR("HdmiCecSink plugin not accessible\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;

                    LOGINFO("Requesting Audio Device power Status \n");
                    {
                        Utils::Synchro::UnlockApiGuard<DisplaySettings> unlockApi;
                        m_client->Invoke<JsonObject, JsonObject>(2000, "requestAudioDevicePowerStatus", param, hdmiCecSinkResult);
                    }
                    if (!hdmiCecSinkResult["success"].Boolean()) {
                        success = false;
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
            else {
                success = false;
                LOGERR("HdmiCecSink plugin not ready\n");
            }

            return success;
        }
        
	bool DisplaySettings::requestDeviceAudioStatus()
        {
            bool success = true;

            PluginHost::IShell::state state;
            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);

                getHdmiCecSinkPlugin();
                if (!m_client) {
                    LOGERR("HdmiCecSink plugin not accessible\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;

                    LOGINFO("Requesting Audio Status \n");
                    {
                        Utils::Synchro::UnlockApiGuard<DisplaySettings> unlockApi;
                        m_client->Invoke<JsonObject, JsonObject>(2000, "sendGetAudioStatusMessage", param, hdmiCecSinkResult);
                    }
                    if (!hdmiCecSinkResult["success"].Boolean()) {
                        success = false;
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
            else {
                success = false;
                LOGERR("HdmiCecSink plugin not ready\n");
            }

            return success;
        }

        bool DisplaySettings::sendUserControlPressCommand(int keyCode)
        {
            bool success = true;

            PluginHost::IShell::state state;
            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);

                getHdmiCecSinkPlugin();
                if (!m_client) {
                    LOGERR("HdmiCecSink plugin not accessible\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;
                    param["logicalAddress"] = 5;
                    param["keyCode"] = keyCode;

                    LOGINFO(" Send mute key code \n");
                    {
                        Utils::Synchro::UnlockApiGuard<DisplaySettings> unlockApi;
                        m_client->Invoke<JsonObject, JsonObject>(2000, "sendUserControlPressed", param, hdmiCecSinkResult);
                    }
                    if (!hdmiCecSinkResult["success"].Boolean()) {
                        success = false;
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
            else {
                success = false;
                LOGERR("HdmiCecSink plugin not ready\n");
            }

            return success;
        }

        uint32_t DisplaySettings::setEnableAudioPort (const JsonObject& parameters, JsonObject& response)
        {   //TODO: Handle other audio ports. Currently only supports HDMI ARC/eARC
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "audioPort");

            bool success = true;
            string audioPort = parameters["audioPort"].String();

            returnIfParamNotFound(parameters, "enable");
            string spEnable = parameters["enable"].String();
            bool pEnable = false;
            try {
                    pEnable = parameters["enable"].Boolean();
            }catch (const std::exception& err) {
                    LOGERR("Exception parsing enable parameter: %s", err.what());
                    returnResponse(false);
            }

            if (true == pEnable && WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY == getSystemPowerState()) {
                LOGWARN("Ignoring the setEnableAudioPort(true) request based on the power state");
                returnResponse(false);
            }

            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->SetAudioEnablePersist(it->second, pEnable, audioPort) != Core::ERROR_NONE) { success = false; }
                        if (success && audio->EnableAudioPort(it->second, pEnable) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            if(audioPortEnableStatusMap[audioPort] != pEnable)
            {
                JsonObject params;
                audioPortEnableStatusMap[audioPort] = pEnable;
                params["audioPort"] = audioPort;
                params["enable"] = pEnable;
                sendNotify("audioPortEnableStatusChanged", params);
            }
            returnResponse(success);
        }

	void  DisplaySettings::checkSADUpdate() {
		// COM-RPC path: replaces libds calls with IDeviceSettingsAudio COM-RPC methods
		LOGINFO("Inside checkSADUpdate (COM-RPC)\n");
		std::lock_guard<std::mutex> lock(m_SadMutex);
		LOGINFO("m_AudioDeviceSADState = %d, m_arcEarcAudioEnabled = %d, m_hdmiInAudioDeviceConnected = %d\n",
		        m_AudioDeviceSADState, m_arcEarcAudioEnabled, m_hdmiInAudioDeviceConnected);
		if (m_SADDetectionTimer.isActive()) {
			m_SADDetectionTimer.stop();
		}
		if (m_arcEarcAudioEnabled == false && m_hdmiInAudioDeviceConnected == true) {
			if (m_AudioDeviceSADState == AUDIO_DEVICE_SAD_RECEIVED) {
				m_AudioDeviceSADState = AUDIO_DEVICE_SAD_UPDATED;
				auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
				if (audio != nullptr) {
					const auto it = _audioPortHandles.find("HDMI_ARC0");
					if (it != _audioPortHandles.end()) {
						// Build flat SAD array from sad_list
						std::vector<uint8_t> sadArray(sad_list.begin(), sad_list.end());
						audio->SetSAD(it->second,
						              sadArray.data(),
						              static_cast<uint8_t>(sadArray.size()));

						int32_t stereoAutoMode = 0;
						if (audio->GetStereoAuto(it->second, stereoAutoMode) == Core::ERROR_NONE
						    && stereoAutoMode != 0) {
							audio->SetStereoAuto(it->second, 1, true);
						} else {
							Exchange::IDeviceSettingsAudio::StereoMode mode =
							        Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_STEREO;
							audio->GetStereoMode(it->second, mode);
							audio->SetStereoMode(it->second, mode, true);
						}
					}
					audio->Release();
				}
				LOGINFO("SAD is updated m_AudioDeviceSADState = %d\n", m_AudioDeviceSADState);
			} else {
				if (m_requestSadRetrigger == false) {
					LOGINFO("Not recieved SAD update after 3sec timeout, retriggering the SAD request and starting the timer for 3 seconds\n");
					m_requestSadRetrigger = true;
					sendMsgToQueue(REQUEST_SHORT_AUDIO_DESCRIPTOR, NULL);
					m_AudioDeviceSADState = AUDIO_DEVICE_SAD_REQUESTED;
					m_SADDetectionTimer.start(SAD_UPDATE_CHECK_TIME_IN_MILLISECONDS);
				} else {
					LOGINFO("Not recieved SAD update even after retriggering the SAD request, proceeding with default SAD\n");
					m_requestSadRetrigger = false;
				}
			}
			if (!m_requestSadRetrigger) {
				LOGINFO("%s: Enable ARC (COM-RPC)... \n", __FUNCTION__);
				auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
				if (audio != nullptr) {
					const auto it = _audioPortHandles.find("HDMI_ARC0");
					if (it != _audioPortHandles.end()) {
						Exchange::IDeviceSettingsAudio::AudioARCStatus arcStatus{};
						arcStatus.arcType = Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_ARC;
						arcStatus.status  = true;
						audio->EnableARC(it->second, arcStatus);
					}
					audio->Release();
				}
				m_arcEarcAudioEnabled = true;
			}
		}
	}

        // Event management end

        // Thunder plugins communication end


        uint32_t DisplaySettings::getTVHDRCapabilities (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
			bool success = true;
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find("HDMI0");
                    if (it != _videoPortHandles.end()) {
                        int32_t capabilities = 0;
                        if (vp->GetTVHDRCapabilities(it->second, capabilities) == Core::ERROR_NONE) {
                            response["capabilities"] = capabilities;
                        } else { success = false; }
                    } else { success = false; }
                    vp->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::isConnectedDeviceRepeater (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            bool success = true;
            bool isConnectedDeviceRepeater = false;
            {
                const std::string strVideoPort = _vpConfigStore.GetDefaultVideoPortName();
                if (!isDisplayConnected(strVideoPort)) {
                    LOGERR("isConnectedDeviceRepeater failure: display not connected on %s\n", strVideoPort.c_str());
                    success = false;
                } else {
                    auto* disp = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
                    if (disp != nullptr) {
                        int32_t displayHandle = -1;
                        if (disp->GetDisplay(Exchange::IDeviceSettingsDisplay::DS_DISPLAY_PORT_TYPE_HDMI, 0, displayHandle) == Core::ERROR_NONE && displayHandle >= 0) {
                            Exchange::IDeviceSettingsDisplay::DisplayEDID edId{};
                            Exchange::IDeviceSettingsDisplay::IDSVideoPortResolutionIterator* resList = nullptr;
                            if (disp->GetDisplayEdid(displayHandle, edId, resList) == Core::ERROR_NONE) {
                                isConnectedDeviceRepeater = edId.isRepeater;
                                if (resList != nullptr) {
                                    resList->Release();
                                    resList = nullptr;
                                }
                            } else {
                                success = false;
                            }
                        } else {
                            success = false;
                        }
                        disp->Release();
                    } else {
                        success = false;
                    }
                }
            }
            response["HdcpRepeater"] = isConnectedDeviceRepeater;
            returnResponse(success);
        }

        uint32_t DisplaySettings::getDefaultResolution (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            bool success = true;
            {
                const std::string strVideoPort = _vpConfigStore.GetDefaultVideoPortName();
                if (!isDisplayConnected(strVideoPort)) {
                    LOGERR("getDefaultResolution failure: display not connected on %s\n", strVideoPort.c_str());
                    success = false;
                } else {
                    const std::string defaultRes = _vpConfigStore.GetDefaultResolution(strVideoPort);
                    if (!defaultRes.empty()) {
                        response["defaultResolution"] = defaultRes;
                    } else {
                        success = false;
                    }
                }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setScartParameter (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "scartParameter");
            returnIfParamNotFound(parameters, "scartParameterData");

            string sScartParameter = parameters["scartParameter"].String();
            string sScartParameterData = parameters["scartParameterData"].String();

            bool success = true;
            // SCART is a legacy interface not supported via COM-RPC. Skip.
            LOGWARN("setScartParameter: not supported in COM-RPC mode");
            success = false;
            returnResponse(success);
        }
        //End methods

        //Begin events
        void DisplaySettings::resolutionPreChange()
        {
            sendNotify("resolutionPreChange", JsonObject());
        }

        void DisplaySettings::resolutionChanged(int width, int height)
        {
            vector<string> connectedDisplays;
            getConnectedVideoDisplaysHelper(connectedDisplays);

            string firstDisplay = "";
            string firstResolution = "";
            bool firstResolutionSet = false;
            for (int i = 0; i < (int)connectedDisplays.size(); i++)
            {
                string resolution;
                string display = connectedDisplays.at(i);
                {
                    const auto it = _videoPortHandles.find(display);
                    if (it != _videoPortHandles.end()) {
                        auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                        if (vp != nullptr) {
                            Exchange::IDeviceSettingsVideoPort::VideoPortResolution vpRes;
                            if (vp->GetVideoPortResolution(it->second, vpRes) == Core::ERROR_NONE) {
                                resolution = vpRes.name;
                            }
                            vp->Release();
                        }
                    }
                }
                const std::string defaultPort = _videoPortHandles.empty() ? std::string("HDMI0") : _videoPortHandles.begin()->first;
                std::string videoPortName = defaultPort.substr(0, defaultPort.size() - 1);
                if (!resolution.empty())
                {
                    if (Utils::String::stringContains(display, videoPortName.c_str()))
                    {
                        // only report first HDMI connected device is HDMI is connected
                        JsonObject params;
                        params["width"] = width;
                        params["height"] = height;
                        params["videoDisplayType"] = display;
                        params["resolution"] = resolution;
                        sendNotify("resolutionChanged", params);
                        return;
                    }
                    else if (!firstResolutionSet)
                    {
                        firstDisplay = display;
                        firstResolution = resolution;
                        firstResolutionSet = true;
                    }
                }
            }
            if (firstResolutionSet)
            {
                //if HDMI is not connected then notify the server of first connected device
                JsonObject params;
                params["width"] = width;
                params["height"] = height;
                params["videoDisplayType"] = firstDisplay;
                params["resolution"] = firstResolution;
                sendNotify("resolutionChanged", params);
            }
        }

        void DisplaySettings::zoomSettingUpdated(const string& zoomSetting)
        {//servicemanager sample: {"name":"zoomSettingUpdated","params":{"zoomSetting":"None","success":true,"videoDisplayType":"all"}
         //servicemanager sample: {"name":"zoomSettingUpdated","params":{"zoomSetting":"Full","success":true,"videoDisplayType":"all"}
            JsonObject params;
            params["zoomSetting"] = zoomSetting;
            params["videoDisplayType"] = "all";
            sendNotify("zoomSettingUpdated", params);
        }

        void DisplaySettings::activeInputChanged(bool activeInput)
        {
            JsonObject params;
            params["activeInput"] = activeInput;
            sendNotify("activeInputChanged", params);
        }

        void DisplaySettings::connectedVideoDisplaysUpdated(int hdmiHotPlugEvent)
        {
            static int previousStatus = HDMI_HOT_PLUG_EVENT_CONNECTED;
            static int firstTime = 1;

            if (firstTime || previousStatus != hdmiHotPlugEvent)
            {
                firstTime = 0;
                JsonArray connectedDisplays;
                if (HDMI_HOT_PLUG_EVENT_CONNECTED == hdmiHotPlugEvent)
                {
                    connectedDisplays.Add("HDMI0");
                }
                else
                {
                    /* notify Empty list on HDMI-output-disconnect hotplug */
                }

                JsonObject params;
                params["connectedVideoDisplays"] = connectedDisplays;
                sendNotify("connectedVideoDisplaysUpdated", params);

            }
            previousStatus = hdmiHotPlugEvent;
            
	    //If HDMI hotplug event occurs, DisplaySettings  re-evaluate whether it should be signalling ALLM output of the HDMI port
	    std::string currentAllmState = "";
            Utils::String::getSystemModePropertyValue("DEVICE_OPTIMIZE" ,"currentstate" , currentAllmState);
            if(currentAllmState == "VIDEO" || currentAllmState == "GAME")
            {
                    Request(currentAllmState);
            }
        }

        void DisplaySettings::connectedAudioPortUpdated (int iAudioPortType, bool isPortConnected)
        {
            JsonObject params;
            string sPortName;
            string sPortStatus;
            // COM-RPC path: portType maps to Exchange::IDeviceSettingsAudio::AudioPortType
            if (iAudioPortType == static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC)) {
                params["HotpluggedAudioPort"] = "HDMI_ARC0";
                sPortName.assign("HDMI_ARC0");
            } else if (iAudioPortType == static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HEADPHONE)) {
                params["HotpluggedAudioPort"] = "HEADPHONE0";
                sPortName.assign("HEADPHONE0");
            }

            if (1 == isPortConnected)
            {
                params["isConnected"] = "connected";
                sPortStatus.assign ("connected");
            }
            else
            {
                params["isConnected"] = "disconnected";
                sPortStatus.assign ("disconnected");
            }
            LOGWARN ("Thunder sends notification %s audio port hotplug status %s", sPortName.c_str(), sPortStatus.c_str());
            sendNotify("connectedAudioPortUpdated", params);
        }

        //End events

        void DisplaySettings::getConnectedVideoDisplaysHelper(vector<string>& connectedDisplays)
        {
            // COM-RPC path: iterate over cached video port handles and check connectivity
            auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
            if (vp != nullptr) {
                for (const auto& kv : _videoPortHandles) {
                    bool connected = false;
                    if (vp->IsVideoPortDisplayConnected(kv.second, connected) == Core::ERROR_NONE && connected) {
                        const std::string& portName = kv.first;
                        if (Utils::String::stringContains(portName, "HDMI") &&
                            !Utils::String::stringContains(portName, "HDMI_ARC")) {
                            connectedDisplays.clear();
                            connectedDisplays.emplace_back(portName);
                            break;
                        } else {
                            vectorSet(connectedDisplays, portName);
                        }
                    }
                }
                vp->Release();
            }
        }

        bool DisplaySettings::checkPortName(std::string& name) const
        {
            if (Utils::String::stringContains(name,"HDMI")) {
		if(Utils::String::stringContains(name,"HDMI_ARC"))
                    name = "HDMI_ARC0";
		else
		    name = "HDMI0";
            }
            else if (Utils::String::stringContains(name,"SPDIF"))
                name = "SPDIF0";
            else if (Utils::String::stringContains(name,"IDLR"))
                name = "IDLR0";
            else if (Utils::String::stringContains(name,"SPEAKER"))
                name = "SPEAKER0";
            else if (Utils::String::stringContains(name,"HEADPHONE"))
                name = "HEADPHONE0";
            else if (!name.empty()) // Empty is allowed
                return false;

            return true;
        }

        uint32_t DisplaySettings::resetDialogEnhancement(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->ResetAudioDialogEnhancement(it->second) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::resetBassEnhancer(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->ResetAudioBassEnhancer(it->second) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::resetSurroundVirtualizer(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->ResetAudioSurroundVirtualizer(it->second) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::resetVolumeLeveller(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        if (audio->ResetAudioVolumeLeveller(it->second) != Core::ERROR_NONE) { success = false; }
                    } else { success = false; }
                    audio->Release();
                } else { success = false; }
            }
            returnResponse(success);
        }
        uint32_t DisplaySettings::getVideoFormat(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"currentVideoFormat":"SDR","supportedVideoFormat":["SDR","HDR10","HLG","DV","Technicolor Prime"],"success":true}
            LOGINFOMETHOD();
            {
                string videoPort = parameters.HasLabel("videoPort") ? parameters["videoPort"].String() : "HDMI0";
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    const auto it = _videoPortHandles.find(videoPort);
                    if (it != _videoPortHandles.end()) {
                        Exchange::IDeviceSettingsVideoPort::HDRStandard hdrStd = Exchange::IDeviceSettingsVideoPort::HDRStandard::DS_HDRSTANDARD_NONE;
                        if (vp->GetVideoEOTF(it->second, hdrStd) == Core::ERROR_NONE) {
                            response["currentVideoFormat"] = getVideoFormatTypeToString(static_cast<uint32_t>(hdrStd));
                        } else {
                            response["currentVideoFormat"] = "NONE";
                        }
                    } else {
                        response["currentVideoFormat"] = "NONE";
                    }
                    vp->Release();
                } else {
                    response["currentVideoFormat"] = "NONE";
                }
            }
            response["supportedVideoFormat"] = getSupportedVideoFormats();
            returnResponse(true);
        }

        JsonArray DisplaySettings::getSupportedVideoFormats()
        {
            JsonArray videoFormats;
            {
                auto* vd = AcquireSubInterface<Exchange::IDeviceSettingsVideoDevice>();
                if (vd != nullptr) {
                    int32_t capabilities = 0;
                    if (vd->GetHDRCapabilities(_videoDeviceHandle, capabilities) == Core::ERROR_NONE) {
                        using HS = Exchange::IDeviceSettingsVideoPort::HDRStandard;
                        if (capabilities & static_cast<int32_t>(HS::DS_HDRSTANDARD_HDR10))         videoFormats.Add("HDR10");
                        if (capabilities & static_cast<int32_t>(HS::DS_HDRSTANDARD_HLG))           videoFormats.Add("HLG");
                        if (capabilities & static_cast<int32_t>(HS::DS_HDRSTANDARD_DOLBYVISION))   videoFormats.Add("DV");
                        if (capabilities & static_cast<int32_t>(HS::DS_HDRSTANDARD_TECHNICOLORPRIME)) videoFormats.Add("Technicolor Prime");
                        if (capabilities & static_cast<int32_t>(HS::DS_HDRSTANDARD_HDR10PLUS))     videoFormats.Add("HDR10PLUS");
                        if (capabilities & static_cast<int32_t>(HS::DS_HDRSTANDARD_SDR))           videoFormats.Add("SDR");
                    }
                    vd->Release();
                }
            }
            return videoFormats;
        }

        const char *DisplaySettings::getVideoFormatTypeToString(
            uint32_t format
            )
        {
            const char *strValue = "NONE";
            using HS = Exchange::IDeviceSettingsVideoPort::HDRStandard;
            switch (static_cast<HS>(format)) {
                case HS::DS_HDRSTANDARD_SDR:             strValue = "SDR"; break;
                case HS::DS_HDRSTANDARD_HDR10:           strValue = "HDR10"; break;
                case HS::DS_HDRSTANDARD_HDR10PLUS:       strValue = "HDR10PLUS"; break;
                case HS::DS_HDRSTANDARD_HLG:             strValue = "HLG"; break;
                case HS::DS_HDRSTANDARD_DOLBYVISION:     strValue = "DV"; break;
                case HS::DS_HDRSTANDARD_TECHNICOLORPRIME: strValue = "TechnicolorPrime"; break;
                default:                                  strValue = "NONE"; break;
            }
            return strValue;
        }

	uint32_t DisplaySettings::getVideoFormatTypeFromString(const char *strFormat)
        {
            if(strcmp(strFormat,"SDR")== 0 )       return static_cast<uint32_t>(HDRStandard::DS_HDRSTANDARD_SDR);
            if(strcmp(strFormat,"HDR10")== 0)      return static_cast<uint32_t>(HDRStandard::DS_HDRSTANDARD_HDR10);
            if(strcmp(strFormat,"HDR10PLUS")== 0)  return static_cast<uint32_t>(HDRStandard::DS_HDRSTANDARD_HDR10PLUS);
            if(strcmp(strFormat,"DV")== 0)         return static_cast<uint32_t>(HDRStandard::DS_HDRSTANDARD_DOLBYVISION);
            if(strcmp(strFormat,"HLG")== 0)        return static_cast<uint32_t>(HDRStandard::DS_HDRSTANDARD_HLG);
            if(strcmp(strFormat,"TechnicolorPrime")== 0) return static_cast<uint32_t>(HDRStandard::DS_HDRSTANDARD_TECHNICOLORPRIME);
            return static_cast<uint32_t>(HDRStandard::DS_HDRSTANDARD_NONE);
        }
    Core::hresult DisplaySettings::Request(const string& newState)
	{
		vector<string> connectedDisplays;
		getConnectedVideoDisplaysHelper(connectedDisplays);
		for (int i = 0; i < (int)connectedDisplays.size(); i++)
		{
			const std::string& strVideoPort = connectedDisplays.at(i);
			bool enable = (newState == "GAME") ? true : false;
			auto* disp = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
			if (disp != nullptr) {
			    const auto it = _displayHandles.find(strVideoPort);
			    if (it != _displayHandles.end()) {
			        if (enable) {
			            disp->SetAVIContentType(it->second, DisplayAVIContentType::DS_DISPLAY_AVI_CONTENT_GAME);
			            disp->SetAVIScanInformation(it->second, DisplayAVIScanInformation::DS_DISPLAY_AVI_SCAN_UNDERSCAN);
			        } else {
			            disp->SetAVIContentType(it->second, DisplayAVIContentType::DS_DISPLAY_AVI_CONTENT_NOT_SIGNALLED);
			            disp->SetAVIScanInformation(it->second, DisplayAVIScanInformation::DS_DISPLAY_AVI_SCAN_NO_DATA);
			        }
			        disp->SetAllmEnabled(it->second, enable);
			    }
			    disp->Release();
			}
		}
		if( 0 == (int)connectedDisplays.size())
		{
			LOGWARN("No display connected to device (or)device's powerstate is not ON");
            return Core::ERROR_GENERAL;
		}
        return Core::ERROR_NONE;
	}

        void DisplaySettings::OnAudioOutHotPlug(dsAudioPortType_t portType, uint32_t uiPortNumber, bool isPortConnected)
        {
            LOGINFO("Received OnAudioOutHotPlug callback");
            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->connectedAudioPortUpdated((int)portType, isPortConnected);
		if (portType == dsAUDIOPORT_TYPE_HDMI_ARC) {
		   if(isPortConnected) {
			DisplaySettings::_instance->m_arcEarcConnectionNotifiedToUI = ARC_EARC_CONNECTED;
		   } else {
			DisplaySettings::_instance->m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED;
		   }
		}
            }
            else
            {
                LOGERR("DisplaySettings::dsHdmiEventHandler DisplaySettings::_instance is NULL\n");
            }
        }

        void DisplaySettings::OnAudioFormatUpdate(dsAudioFormat_t audioFormat)
        {
            LOGINFO("Received OnAudioFormatUpdate callback");
            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->notifyAudioFormatChange(audioFormat);
            }
        }

        void DisplaySettings::OnDolbyAtmosCapabilitiesChanged(dsATMOSCapability_t atmosCapability, bool status)
        {
            LOGINFO("Received OnDolbyAtmosCapabilitiesChanged callback:atmosCaps[%d]", atmosCapability);
            if(DisplaySettings::_instance && status)
            {
                DisplaySettings::_instance->notifyAtmosCapabilityChange(atmosCapability);
            }
        }

        void DisplaySettings::OnAudioPortStateChanged(dsAudioPortState_t audioPortState)
        {
            LOGINFO("Received OnAudioPortStateChanged callback. Audio Port Init State: %d", audioPortState);
            try
            {   if( audioPortState == dsAUDIOPORT_STATE_INITIALIZED)
                {
                    DisplaySettings::_instance->AudioPortsReInitialize();
                    DisplaySettings::_instance->InitAudioPorts();
                }
           }
           catch(const device::Exception& err)
           {
              LOG_DEVICE_EXCEPTION0();
           }
        }

        void DisplaySettings::OnAssociatedAudioMixingChanged(bool mixing)
        {
            LOGINFO("Received OnAssociatedAudioMixingChanged callback. Associated Audio Mixing: %d", mixing);
            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->notifyAssociatedAudioMixingChange(mixing);
            }
        }

        void DisplaySettings::OnAudioFaderControlChanged(int mixerBalance)
        {
            LOGINFO("Received OnAudioFaderControlChanged. Fader Control: %d", mixerBalance);
            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->notifyFaderControlChange(mixerBalance);
            }
        }

        void DisplaySettings::OnAudioPrimaryLanguageChanged(const std::string& primaryLanguage)
        {
            LOGINFO("Received OnAudioPrimaryLanguageChangedcallback. Primary Language: %s", primaryLanguage.c_str());
            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->notifyPrimaryLanguageChange(primaryLanguage);
            }
        }

        void DisplaySettings::OnAudioSecondaryLanguageChanged(const std::string& secondaryLanguage)
        {
            LOGINFO("Received OnAudioSecondaryLanguageChanged callback. Secondary Language: %s", secondaryLanguage.c_str());
            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->notifySecondaryLanguageChange(secondaryLanguage);
            }
        }

        void DisplaySettings::OnDisplayHDMIHotPlug(dsDisplayEvent_t displayEvent)
        {
            LOGINFO("Received OnDisplayHDMIHotPlug callback. event data:%d ", displayEvent);
            isResCacheUpdated = false;
            isDisplayConnectedCacheUpdated = false;
            isStbHDRcapabilitiesCache = false;

            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->connectedVideoDisplaysUpdated((int)displayEvent);
            }
        }

        void DisplaySettings::OnHdmiInEventHotPlug(dsHdmiInPort_t port, bool isConnected)
        {
           int hdmiin_hotplug_port = port;
           bool hdmiin_hotplug_conn = isConnected;
           LOGINFO("Received OnHDMIInEventHotPlug Port:%d, connected:%d", hdmiin_hotplug_port, hdmiin_hotplug_conn);

           if(!DisplaySettings::_instance) {
               LOGERR("DisplaySettings::OnHdmiInEventHotPlug DisplaySettings::_instance is NULL\n");
               return;
           }
           if(hdmiin_hotplug_port == hdmiArcPortId) //HDMI ARC/eARC Port Handling
           {
               try
               {
                    LOGINFO("Received OnHDMIInEventHotPlug  HDMI_ARC Port, connected status[%d]",  hdmiin_hotplug_conn);
                    if(!hdmiin_hotplug_conn)
                    {
                        LOGINFO("Current Arc/eArc states m_currentArcRoutingState = %d, m_hdmiInAudioDeviceConnected =%d, m_arcEarcAudioEnabled =%d, m_hdmiInAudioDeviceType = %d", DisplaySettings::_instance->m_currentArcRoutingState, DisplaySettings::_instance->m_hdmiInAudioDeviceConnected, \
                                     DisplaySettings::_instance->m_arcEarcAudioEnabled, DisplaySettings::_instance->m_hdmiInAudioDeviceType);
                        std::lock_guard<std::mutex> lock(DisplaySettings::_instance->m_AudioDeviceStatesUpdateMutex);
                        if (DisplaySettings::_instance->m_hdmiInAudioDeviceConnected == true)
                        {
                            DisplaySettings::_instance->m_hdmiInAudioDeviceConnected =  false;
                            DisplaySettings::_instance->m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_UNKNOWN;
                            //if(DisplaySettings::_instance->m_arcEarcAudioEnabled == true) // commenting out for the AVR HPD 0 and 1 events instantly for TV standby in/out case
                            {
                                DisplaySettings::_instance->connectedAudioPortUpdated(dsAUDIOPORT_TYPE_HDMI_ARC, hdmiin_hotplug_conn);
				DisplaySettings::_instance->m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED;
                                LOGINFO("Received OnHdmiInEventHotPlug  HDMI_ARC Port disconnected. Notify UI !!! ");
                            }
                        }
                        DisplaySettings::_instance->m_currentArcRoutingState = ARC_STATE_ARC_TERMINATED;
                        DisplaySettings::_instance->m_requestSadRetrigger = false;

                        if (DisplaySettings::_instance->m_AudioDeviceSADState != AUDIO_DEVICE_SAD_CLEARED)
                        {
                            DisplaySettings::_instance->m_AudioDeviceSADState = AUDIO_DEVICE_SAD_CLEARED;
                            LOGINFO("%s: Clearing Audio device SAD", __FUNCTION__);
                            sad_list.clear();
                        }
                        else
                        {
                            LOGINFO("SAD already cleared");
                        }
                    } // Release Mutex m_AudioDeviceStatesUpdateMutex
                }
                catch (const device::Exception& err)
                {
                     LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                }
            }// HDMI_IN_ARC_PORT_ID
        }

        void DisplaySettings::OnZoomSettingsChanged(dsVideoZoom_t zoomSetting)
        {
            LOGINFO("Received OnZoomSettingsChanged callback");
            if(zoomSetting == dsVIDEO_ZOOM_NONE)
            {
                LOGINFO("dsVIDEO_ZOOM_NONE Settings");
                if(DisplaySettings::_instance)
                    DisplaySettings::_instance->zoomSettingUpdated("NONE");
            }
            else if(zoomSetting == dsVIDEO_ZOOM_FULL)
            {
                LOGINFO("dsVIDEO_ZOOM_FULL Settings");
                if(DisplaySettings::_instance)
                    DisplaySettings::_instance->zoomSettingUpdated("FULL");
            }
        }

        void DisplaySettings::OnResolutionPreChange(const int width, const int height)
        {
            LOGINFO("Received OnResolutionPreChange callback");
            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->resolutionPreChange();
            }
            isResCacheUpdated = false;
        }

        void DisplaySettings::OnResolutionPostChange(const int width, const int height)
        {
            LOGINFO("Received OnResolutionPostChange callback");
            if(DisplaySettings::_instance) {
                DisplaySettings::_instance->resolutionChanged(width, height);
            }
        }

        void DisplaySettings::OnVideoFormatUpdate(dsHDRStandard_t videoFormatHDR)
        {
            LOGINFO("Received OnVideoFormatUpdate callback. Video format: %d", videoFormatHDR);
            if(DisplaySettings::_instance) {
                DisplaySettings::_instance->notifyVideoFormatChange(videoFormatHDR);
            }

        }




        // ================================================================
        // Methods ported from DS_IARM (JSONRPC/CEC/power — no libds)
        // ================================================================

        // --- getSystemPowerState ---
        PowerState DisplaySettings::getSystemPowerState()
        {
            PowerState pwrStateCur = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
            PowerState pwrStatePrev = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
            Core::hresult retStatus = Core::ERROR_GENERAL;

            ASSERT (_powerManagerPlugin);
            if (_powerManagerPlugin){
                retStatus = _powerManagerPlugin->GetPowerState(pwrStateCur, pwrStatePrev);
            }
            if (Core::ERROR_NONE == retStatus)
            {
                m_powerState = pwrStateCur;
                LOGWARN("DisplaySettings::m_powerState: %d", m_powerState);
            }

            else
            {
                LOGWARN("GetPowerState failed");
            }

            return m_powerState;
        }

        // --- initAudioPortsWorker ---
        void DisplaySettings::initAudioPortsWorker(void)
        {
            audioPortInitActive = true;
            DisplaySettings::_instance->InitAudioPorts();
            audioPortInitActive = false;
        }

        // --- sendMsgToQueue ---
	void DisplaySettings::sendMsgToQueue(msg_t msg, void *param )
	{
		SendMsgInfo msgInfo;

                msgInfo.msg = msg;
		msgInfo.param = param;
		std::unique_lock<std::mutex> lock(DisplaySettings::_instance->m_sendMsgMutex);
        	DisplaySettings::_instance->m_sendMsgQueue.push(msgInfo);
        	DisplaySettings::_instance->m_sendMsgThreadRun = true;
        	DisplaySettings::_instance->m_sendMsgCV.notify_one();
	}

        // --- sendMsgThread ---
void DisplaySettings::sendMsgThread()
{
	LOGINFO("%s: message Thread Start\n",__FUNCTION__);
	bool result = false;
        SendMsgInfo msgInfo;
	
	if(!DisplaySettings::_instance)
                 return;

	while(!_instance->m_sendMsgThreadExit) 
	{
		msgInfo.msg = -1;
        	msgInfo.param = NULL;
		{

                       LOGINFO("%s: Debug: Wait for message \n",__FUNCTION__);
		       std::unique_lock<std::mutex> lock(DisplaySettings::_instance->m_sendMsgMutex);
		       _instance->m_sendMsgCV.wait(lock, []{return (_instance->m_sendMsgThreadRun == true);});
		
		}

		if (_instance->m_sendMsgThreadExit == true)
        	{
            		LOGINFO(" sendCecMessageThread Exiting");
            		_instance->m_sendMsgThreadRun = false;
            		break;
        	}

        	if (_instance->m_sendMsgQueue.empty()) {
            		_instance->m_sendMsgThreadRun = false;
            		continue;
        	}
		
		msgInfo = DisplaySettings::_instance->m_sendMsgQueue.front();
		
			switch(msgInfo.msg)
			{
				case SEND_AUDIO_DEVICE_POWERON_MSG:
				{
					LOGINFO(" sendHdmiCecSinkAudioDevicePowerOn");
					result = DisplaySettings::_instance->sendHdmiCecSinkAudioDevicePowerOn();
				}
				break;
				
				case REQUEST_SHORT_AUDIO_DESCRIPTOR:
				{
					LOGINFO(" Request Short Audio descriptor");
					result = DisplaySettings::_instance->requestShortAudioDescriptor();
				}
				break;
				
				case REQUEST_AUDIO_DEVICE_POWER_STATUS:
				{
					LOGINFO(" Request Audio Device Power Status");
					result = DisplaySettings::_instance->requestAudioDevicePowerStatus();
				}
				break;

				case SEND_MUTE_KEY_EVENT:
                                {
                                        LOGINFO(" Send Mute code ");
                                        result = DisplaySettings::_instance->sendUserControlPressCommand(67);
                                }
                                break;
				case SEND_DEVICE_AUDIO_STATUS:
                                {
                                        LOGINFO(" Send Device Audio Status message");
                                        result = DisplaySettings::_instance->requestDeviceAudioStatus();
                                }
                                break;

				case SEND_REQUEST_ARC_INITIATION: // spearte initiation and termination cases
				{
					LOGINFO(" Send request for ARC INITIATION");
					result = DisplaySettings::_instance->setUpHdmiCecSinkArcRouting(true);
				}
				break;
				
				case SEND_REQUEST_ARC_TERMINATION:
				{
					LOGINFO(" Send request for ARC TERMINATION");
					result = DisplaySettings::_instance->setUpHdmiCecSinkArcRouting(false);
				}
                break;
		
				default:
				{
					LOGINFO(" Requested invalid message");
				}
				break;
				
			}
			
			if (result == true) {
			    LOGINFO(" send cec msg [%d] success \n",msgInfo.msg);
			}else{
			    LOGERR(" send cec msg [%d] failed \n",msgInfo.msg);
			}
						
		DisplaySettings::_instance->m_sendMsgQueue.pop();
	}
}

        // --- getHdmiCecSinkPlugin ---
        void DisplaySettings::getHdmiCecSinkPlugin()
        {
            if(m_client == nullptr)
            {
                string token;

                // TODO: use interfaces and remove token
                auto security = m_service->QueryInterfaceByCallsign<PluginHost::IAuthenticate>("SecurityAgent");
                if (security != nullptr) {
                    string payload = "http://localhost";
                    if (security->CreateToken(
                            static_cast<uint16_t>(payload.length()),
                            reinterpret_cast<const uint8_t*>(payload.c_str()),
                            token)
                        == Core::ERROR_NONE) {
                        std::cout << "DisplaySettings got security token" << std::endl;
                    } else {
                        std::cout << "DisplaySettings failed to get security token" << std::endl;
                    }
                    security->Release();
                } else {
                    std::cout << "No security agent" << std::endl;
                }

                string query = "token=" + token;
                Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T("127.0.0.1:9998")));
                m_client = new WPEFramework::JSONRPC::LinkType<Core::JSON::IElement>(_T(HDMICECSINK_CALLSIGN_VER), (_T(HDMICECSINK_CALLSIGN_VER)), false, query);
                LOGINFO("DisplaySettings getHdmiCecSinkPlugin init m_client\n");
            }
        }

        // --- subscribeForHdmiCecSinkEvent ---
        uint32_t DisplaySettings::subscribeForHdmiCecSinkEvent(const char* eventName)
        {
            uint32_t err = Core::ERROR_NONE;
            LOGINFO("Attempting to subscribe for event: %s\n", eventName);
            Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T(SERVER_DETAILS)));
            if (nullptr == m_client) {
                getHdmiCecSinkPlugin();
                if (nullptr == m_client) {
                    LOGERR("JSONRPC: %s: client initialization failed", HDMICECSINK_CALLSIGN_VER);
                    err = Core::ERROR_UNAVAILABLE;
                } 
            }

	    if(err == Core::ERROR_NONE) {
                /* Register handlers for Event reception. */
                if(strcmp(eventName, HDMICECSINK_ARC_INITIATION_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onARCInitiationEventHandler, this);
                    m_clientRegisteredEventNames.push_back(eventName);
                } else if(strcmp(eventName, HDMICECSINK_ARC_TERMINATION_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onARCTerminationEventHandler, this);
                    m_clientRegisteredEventNames.push_back(eventName);
                } else if(strcmp(eventName, HDMICECSINK_SHORT_AUDIO_DESCRIPTOR_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onShortAudioDescriptorEventHandler, this);
                    m_clientRegisteredEventNames.push_back(eventName);
                } else if(strcmp(eventName, HDMICECSINK_SYSTEM_AUDIO_MODE_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onSystemAudioModeEventHandler, this);
                    m_clientRegisteredEventNames.push_back(eventName);
                } else if(strcmp(eventName, HDMICECSINK_ARC_AUDIO_STATUS_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onArcAudioStatusEventHandler, this);
                    m_clientRegisteredEventNames.push_back(eventName);
                } else if(strcmp(eventName, HDMICECSINK_AUDIO_DEVICE_CONNECTED_STATUS_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onAudioDeviceConnectedStatusEventHandler, this);
                    m_clientRegisteredEventNames.push_back(eventName);
                } else if(strcmp(eventName, HDMICECSINK_CEC_ENABLED_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onCecEnabledEventHandler, this);
                    m_clientRegisteredEventNames.push_back(eventName);
                } else if(strcmp(eventName, HDMICECSINK_AUDIO_DEVICE_POWER_STATUS_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onAudioDevicePowerStatusEventHandler, this);
                    m_clientRegisteredEventNames.push_back(eventName);
		} else {
                     err = Core::ERROR_UNAVAILABLE;
                     LOGERR("Unsupported Event: %s ", eventName);
                }
                if ( err  == Core::ERROR_NONE) {
                    LOGINFO("Subscribed for %s", eventName);
                } else {
                    LOGERR("Failed to subscribe for %s with code %d", eventName, err);
                }
            }
            return err;
        }

        // --- stopCecTimeAndUnsubscribeEvent ---
        void DisplaySettings::stopCecTimeAndUnsubscribeEvent() {
            LOGINFO ("de-init cec timer and subscribbed event \n");
            {
                lock_guard<mutex> lck(m_callMutex);
                if ( m_timer.isActive()) {
                    m_timer.stop();
                }

                if ( m_AudioDeviceDetectTimer.isActive()) {
                    m_AudioDeviceDetectTimer.stop();
                }
		if ( m_SADDetectionTimer.isActive()) {
                        m_SADDetectionTimer.stop();
                }
                if ( m_ArcDetectionTimer.isActive()) {
                    m_ArcDetectionTimer.stop();
                }
                if ( m_AudioDevicePowerOnStatusTimer.isActive()) {
                    m_AudioDevicePowerOnStatusTimer.stop();
                }

                if (nullptr != m_client) {
                    for (std::string eventName : m_clientRegisteredEventNames) {
                        m_client->Unsubscribe(1000, _T(eventName));
                        LOGINFO ("Unsubscribing event %s\n", eventName.c_str());
                    }
                    m_clientRegisteredEventNames.clear();

                    LOGINFO ("deleting m_client \n");
                    delete m_client; m_client = nullptr;
                }
            }
        }

        // --- onTimer ---
        void DisplaySettings::onTimer()
        {
            // lock to prevent: parallel onTimer runs, destruction during onTimer
            lock_guard<mutex> lck(m_callMutex);

            PluginHost::IShell::state state;
            bool pluginActivated = false;

            if ((getServiceState(m_service, HDMICECSINK_CALLSIGN, state) == Core::ERROR_NONE) && (state == PluginHost::IShell::state::ACTIVATED)) {
                LOGINFO("%s is active", HDMICECSINK_CALLSIGN);
                pluginActivated = true;
            }

            LOGWARN ("DisplaySettings::onTimer pluginActivated:%d line:%d", pluginActivated, __LINE__);
            if(!m_subscribed) {
                if (pluginActivated && (subscribeForHdmiCecSinkEvent(HDMICECSINK_ARC_INITIATION_EVENT) == Core::ERROR_NONE) && (subscribeForHdmiCecSinkEvent(HDMICECSINK_ARC_TERMINATION_EVENT) == Core::ERROR_NONE) && (subscribeForHdmiCecSinkEvent(HDMICECSINK_SHORT_AUDIO_DESCRIPTOR_EVENT)== Core::ERROR_NONE) && (subscribeForHdmiCecSinkEvent(HDMICECSINK_SYSTEM_AUDIO_MODE_EVENT) == Core::ERROR_NONE) && (subscribeForHdmiCecSinkEvent(HDMICECSINK_AUDIO_DEVICE_CONNECTED_STATUS_EVENT) == Core::ERROR_NONE) && (subscribeForHdmiCecSinkEvent(HDMICECSINK_CEC_ENABLED_EVENT) == Core::ERROR_NONE) && (subscribeForHdmiCecSinkEvent(HDMICECSINK_AUDIO_DEVICE_POWER_STATUS_EVENT) == Core::ERROR_NONE)&& (subscribeForHdmiCecSinkEvent(HDMICECSINK_ARC_AUDIO_STATUS_EVENT) == Core::ERROR_NONE))
                {
                    m_subscribed = true;
                    if (m_timer.isActive()) {
                        m_timer.stop();
                        LOGINFO("Timer stopped.");
                    }
                    LOGINFO("Subscription completed.");
		    sleep(WARMING_UP_TIME_IN_SECONDS);

                } else {
                    LOGERR("Could not subscribe this time, one more attempt in %d msec. Plugin is %s", RECONNECTION_TIME_IN_MILLISECONDS, pluginActivated ? "ACTIVE" : "BLOCKED");
                }
            } else {
                //Standby ON transitions case
                LOGINFO("Already subscribed. Stopping the timer.");
                if (m_timer.isActive()) {
                    m_timer.stop();
                }
            }
	    
	    if(!isCecEnabled){
		try {
		    isCecEnabled = getHdmiCecSinkCecEnableStatus();
		}
		catch (const std::exception& err){
		    LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
		}
	    }

            if(m_subscribed) {
         	//Need to send power on request as this timer might have started based on standby out or boot up scenario
                LOGINFO("%s: Audio Port : [HDMI_ARC0] sendHdmiCecSinkAudioDevicePowerOn !!! \n", __FUNCTION__);
                sendMsgToQueue(SEND_AUDIO_DEVICE_POWERON_MSG, NULL);
		// Some AVR's and SB are not sending response for power on message even though it is in ON state
                // Send power request immediately to query power status of the AVR
                m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_REQUEST;
                sendMsgToQueue(REQUEST_AUDIO_DEVICE_POWER_STATUS, NULL);
                LOGINFO("[HDMI_ARC0] sendAudioDevicePowerStatusRequestMsg!!!\n");
            }
        }

        // --- checkAudioDeviceDetectionTimer ---
        void DisplaySettings::checkAudioDeviceDetectionTimer()
        {
            // lock to prevent: parallel onTimer runs, destruction during onTimer
            lock_guard<mutex> lck(m_callMutex);
            if (m_subscribed && m_hdmiCecAudioDeviceDetected)
            {
               //Connected Audio Ports status update is necessary on bootup / power state transitions
	       m_systemAudioMode_Power_RequestedAndReceived = false;
               LOGINFO("%s: Audio Port : [HDMI_ARC0] sendHdmiCecSinkAudioDevicePowerOn !!! \n", __FUNCTION__);
               sendMsgToQueue(SEND_AUDIO_DEVICE_POWERON_MSG, NULL);
	       LOGINFO("[HDMI_ARC0] Starting the timer to check audio device power status after power on msg!!!\n");
	       m_AudioDevicePowerOnStatusTimer.start(AUDIO_DEVICE_POWER_TRANSITION_TIME_IN_MILLISECONDS);
            } else {
		    LOGINFO("%s: No Audio device detected even after timeout\n", __FUNCTION__);
	    }

            if (m_AudioDeviceDetectTimer.isActive()) {
               m_AudioDeviceDetectTimer.stop();
            }
        }

        // --- onShortAudioDescriptorEventHandler ---
        void DisplaySettings::onShortAudioDescriptorEventHandler(const JsonObject& parameters) {
            string message;

            parameters.ToString(message);
	    JsonArray shortAudioDescriptorList;
            LOGINFO("[Short Audio Descriptor Event], %s : %s", __FUNCTION__, C_STR(message));

            if (parameters.HasLabel("shortAudioDescriptor")) {
                shortAudioDescriptorList = parameters["shortAudioDescriptor"].Array();
                int currentSADState = getAudioDeviceSADState();
		if (currentSADState == AUDIO_DEVICE_SAD_REQUESTED) {
                    try
                    {
            setAudioDeviceSADState(AUDIO_DEVICE_SAD_RECEIVED);
			m_requestSadRetrigger = false;
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI_ARC0");
			LOGINFO("Total Short Audio Descriptors received from connected ARC device: %d\n",shortAudioDescriptorList.Length());
			if(shortAudioDescriptorList.Length() <= 0) {
			    LOGERR("Not setting SAD. No SAD returned by connected ARC device\n");
			    return;
			}

			for (int i=0; i<shortAudioDescriptorList.Length(); i++) {
                            LOGINFO("Short Audio Descriptor[%d]: %lld \n",i, shortAudioDescriptorList[i].Number());
                            sad_list.push_back(shortAudioDescriptorList[i].Number());
                        }

			bool wasSADTimerActive = false;

			if (m_currentArcRoutingState == ARC_STATE_ARC_INITIATED) {
			    if (m_SADDetectionTimer.isActive()) {
			        //Timer is active, so stop the timer and if audio is not routed set SAD and route the audio
			        LOGINFO("%s: Stopping the SAD timer\n", __FUNCTION__);
			        m_SADDetectionTimer.stop();
				
				wasSADTimerActive = true;
			    }

			    if (wasSADTimerActive == true && m_arcEarcAudioEnabled == false ) { /*setEnableAudioPort is called, Timer has started, got SAD before Timer Expiry*/
			        LOGINFO("%s: Updating SAD \n", __FUNCTION__);
                                setAudioDeviceSADState(AUDIO_DEVICE_SAD_UPDATED);
                                aPort.setSAD(sad_list);
                                if(aPort.getStereoAuto() == true) {
                                    aPort.setStereoAuto(true,true);
                                }
                                else{
                                    device::AudioStereoMode mode = device::AudioStereoMode::kStereo;  //default to stereo
                                    mode = aPort.getStereoMode(); //get Last User set stereo mode and set
                                    aPort.setStereoMode(mode.toString(), true);
                                }
				LOGINFO("%s: Routing the audio since m_arcEarcAudioEnabled = %d\n", __FUNCTION__, m_arcEarcAudioEnabled);
				LOGINFO("%s: Enable ARC... \n",__FUNCTION__);
				aPort.enableARC(static_cast<int32_t>(Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_ARC), true);
				m_arcEarcAudioEnabled = true;
			    } else if (m_arcEarcAudioEnabled == true) { /*setEnableAudioPort is called,Timer started and Expired, arc is routed -- or for both wasSADTimerActive == true/false*/
				LOGINFO("%s: Updating SAD since audio is already routed and ARC is initiated\n", __FUNCTION__);
				setAudioDeviceSADState(AUDIO_DEVICE_SAD_UPDATED);
				    aPort.setSAD(sad_list);
                        	    if(aPort.getStereoAuto() == true) {
                    	            	aPort.setStereoAuto(true,true);
                            	    }
                            	    else{
                                	device::AudioStereoMode mode = device::AudioStereoMode::kStereo;  //default to stereo
                                	mode = aPort.getStereoMode(); //get Last User set stereo mode and set
                                	aPort.setStereoMode(mode.toString(), true);
                            	    }
			      } else { // SAD received before setEnableAudioPort
			            LOGINFO("%s: Not updating SAD now since arc routing has not yet happened and SAD timer is not active -> Routing and SAD is updated when setEnableAudioPort is called \n", __FUNCTION__);
			      }
			}else {
				LOGINFO("%s: m_currentArcRoutingState = %d, m_arcEarcAudioEnabled = %d", __FUNCTION__, m_currentArcRoutingState, m_arcEarcAudioEnabled);
			}/*End of m_currentArcRoutingState check */
                    }
                    catch (const std::exception& err)
                    {
                        LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                    }
            }  else {
                LOGERR("Invalid SAD state m_AudioDeviceSADState =%d", m_AudioDeviceSADState);
               }/*End of (m_AudioDeviceSADState == AUDIO_DEVICE_SAD_REQUESTED) */
			}
		    else {
                LOGERR("Field 'ShortAudioDescriptor' could not be found in the event's payload.");
            }/*End of (m_AudioDeviceSADState == AUDIO_DEVICE_SAD_REQUESTED) */
        }

        // --- onSystemAudioModeEventHandler ---
        void DisplaySettings::onSystemAudioModeEventHandler(const JsonObject& parameters) {
            string message;
            string value;

            parameters.ToString(message);
            LOGINFO("[System Audio Mode Event], %s : %s", __FUNCTION__, C_STR(message));

            if (parameters.HasLabel("audioMode")) {
                value = parameters["audioMode"].String();
                if(!value.compare("On")) {
	                m_systemAudioMode_Power_RequestedAndReceived = true; // system audio mode ON is received
			LOGINFO("Requesting power status of AVR as system audio mode is %s\n", C_STR(message));
	                m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_REQUEST;//Should we send power request irrespective of System audio mode status
                        sendMsgToQueue(REQUEST_AUDIO_DEVICE_POWER_STATUS, NULL);
                }
		else if(!value.compare("Off")) {
                    LOGINFO("%s :  audioMode OFF !!!\n", __FUNCTION__);
		    try {
			std::lock_guard<std::mutex> lock(m_AudioDeviceStatesUpdateMutex);
                        m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_UNKNOWN;

   		        if(m_hdmiInAudioDeviceConnected == true) {
			    LOGINFO("SystemAudio mode off disable Arc\n");
			    m_hdmiInAudioDeviceConnected = false;
			    if (m_arcEarcConnectionNotifiedToUI == ARC_EARC_CONNECTED) {
				LOGINFO("System Audio mode is off and m_arcEarcConnectionNotifiedToUI %d, Notify UI to disbale Arc", m_arcEarcConnectionNotifiedToUI);
		            	connectedAudioPortUpdated(static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC), false);
				m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED;
			    } else {
				LOGINFO("Not notifying UI since m_arcEarcConnectionNotifiedToUI =%d\n", m_arcEarcConnectionNotifiedToUI);
			    }
                            {
			      // Arc termination happens from HdmiCecSink plugin so just update the state here
                              m_currentArcRoutingState = ARC_STATE_ARC_TERMINATED;
			      m_requestSadRetrigger = false;
			      LOGINFO("Updating ARC routing state to ARC terminated\n");
                            }

		        }
                        else {
                            LOGINFO("onSystemAudioModeEventHandler: Skip Disable ARC and not notifying the UI as  m_hdmiInAudioDeviceConnected = false\n");
                        }
		    }//Release mutex m_AudioDeviceStatesUpdateMutex
		    catch (const std::exception& err)
                    {
		        LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                    }
                }
                else{
                    LOGERR("%s: Invalid audio mode sent by HdmiCecSink !!!\n",__FUNCTION__);
                }
            } else {
                LOGERR("Field 'audioMode' could not be found in the event's payload.");
            }
        }

        // --- onArcAudioStatusEventHandler ---
        void DisplaySettings::onArcAudioStatusEventHandler(const JsonObject& parameters) {
            string message;
            parameters.ToString(message);
            LOGINFO("[ARC Audio Status Event], %s : %s", __FUNCTION__, C_STR(message));

            if (parameters.HasLabel("muteStatus") && parameters.HasLabel("volumeLevel")) {
                int iArcVolumeLevel = 0;
                const string volumeLevelStr = parameters["volumeLevel"].String();
                if (!TryParseIntInRange(volumeLevelStr, 0, 100, iArcVolumeLevel)) {
                    LOGWARN("Invalid volumeLevel in ARC Audio Status Event");
                    return;
                }
                if(iArcVolumeLevel != hdmiArcVolumeLevel)
		{
		    hdmiArcVolumeLevel = iArcVolumeLevel;
                    JsonObject volParams;
                    volParams["volumeLevel"] = (int)hdmiArcVolumeLevel;
                    sendNotify("volumeLevelChanged", volParams);
		}

		int muteStatusInt = 0;
                const string muteStatusStr = parameters["muteStatus"].String();
                if (!TryParseIntInRange(muteStatusStr, 0, 1, muteStatusInt)) {
                    LOGWARN("Invalid muteStatus in ARC Audio Status Event");
                    return;
                }
		bool bMuteStatus = (muteStatusInt != 0);
                if( bMuteStatus != hdmiArcMuteStatus )
		{
                    hdmiArcMuteStatus = bMuteStatus;
		    JsonObject params;
                    params["muted"] = hdmiArcMuteStatus;
                    sendNotify("muteStatusChanged", params);
		}
            } else {
                LOGERR("Field 'muteStatus' and 'volumeLevel' could not be found in the event's payload.");
            }
        }

        // --- onCecEnabledEventHandler ---
	void DisplaySettings::onCecEnabledEventHandler(const JsonObject& parameters)
	{
             string value;

             LOGINFO(" CEC Enable-Disable Event... \n");
	     if (parameters.HasLabel("cecEnable"))
                 value = parameters["cecEnable"].String();

	     if(!value.compare("true")) {
		isCecEnabled = true;
	      } else{
		isCecEnabled = false;
		try
                    {
                        //if m_arcEarcAudioEnabled == true(case where arc/earc is already routed) we will not reset device type because it will be done from setEnableAudioPort during disable from the connectedAudioPort update
                        if (m_arcEarcAudioEnabled == false && m_hdmiInAudioDeviceType != 0) {
                           LOGINFO("Reset m_hdmiInAudioDeviceType since m_arcEarcAudioEnabled = %d", m_arcEarcAudioEnabled);
                           m_hdmiInAudioDeviceType = 0;
                        }
                        if(m_hdmiInAudioDeviceConnected ==  true) {
                            m_hdmiInAudioDeviceConnected = false;
                            connectedAudioPortUpdated(static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC), false);
			    m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED;
                            m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_UNKNOWN;
                        }
                        else {
                            LOGINFO("Skip Disable ARC and not notifying the UI as  m_hdmiInAudioDeviceConnected = false\n");
                        }
                    }
                    catch (const std::exception& err)
                    {
                        LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                    }
	      }

              LOGINFO("updated isCecEnabled [%d] ... \n", isCecEnabled);
	}

        // --- getEnableAudioPort ---
        uint32_t DisplaySettings::getEnableAudioPort (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            if (!audioPort.compare("HDMI_ARC0")) {
                JsonObject aPortConfig = getAudioOutputPortConfig();
                response["enable"] = aPortConfig["HDMI_ARC"].Boolean();
            } else {
                auto* audio = AcquireSubInterface<Exchange::IDeviceSettingsAudio>();
                if (audio != nullptr) {
                    const auto it = _audioPortHandles.find(audioPort);
                    if (it != _audioPortHandles.end()) {
                        bool enabled = false; string portName;
                        if (audio->GetAudioEnablePersist(it->second, enabled, portName) == Core::ERROR_NONE)
                            response["enable"] = enabled;
                        else success = false;
                    } else success = false;
                    audio->Release();
                } else success = false;
            }
            LOGWARN("getEnableAudioPort: audioPort=%s enable=%s", audioPort.c_str(),
                     response["enable"].Boolean() ? "true" : "false");
            returnResponse(success);
        }

        // --- onPowerModeChanged ---
        void DisplaySettings::onPowerModeChanged(const PowerState currentState, const PowerState newState)
        {
            LOGWARN("onPowerModeChanged: State Changed %d --> %d\r",
                         currentState, newState);
            m_powerState = newState;
            if (newState == WPEFramework::Exchange::IPowerManager::POWER_STATE_ON){
                isResCacheUpdated = false;
                isDisplayConnectedCacheUpdated = false;
                isStbHDRcapabilitiesCache = false;
            try
                {
            LOGWARN("creating worker thread for initAudioPortsWorker ");
            std::thread audioPortInitThread = std::thread(initAudioPortsWorker);
        audioPortInitThread.detach();
                }
                catch(const std::system_error& e)
                {
                    LOGERR("system_error exception in thread creation: %s", e.what());
                }
                catch(const std::exception& e)
                {
                    LOGERR("exception in thread creation : %s", e.what());
                }
            }

        else {
            LOGINFO("%s: Current Power state: %d\n",__FUNCTION__,newState);
            try
            {
                bool hdmi_arc_supported = (_audioPortHandles.count("HDMI_ARC0") > 0);

                if(hdmi_arc_supported) {
          LOGINFO("Current Arc/eArc states m_currentArcRoutingState = %d, m_hdmiInAudioDeviceConnected =%d, m_arcEarcAudioEnabled =%d, m_hdmiInAudioDeviceType = %d\n", DisplaySettings::_instance->m_currentArcRoutingState, DisplaySettings::_instance->m_hdmiInAudioDeviceConnected, \
                  DisplaySettings::_instance->m_arcEarcAudioEnabled, DisplaySettings::_instance->m_hdmiInAudioDeviceType);
                  {
                std::lock_guard<std::mutex> lock(DisplaySettings::_instance->m_AudioDeviceStatesUpdateMutex);
                        LOGINFO("%s: Cleanup ARC/eARC state\n",__FUNCTION__);
                        if(DisplaySettings::_instance->m_currentArcRoutingState != ARC_STATE_ARC_TERMINATED)
                            DisplaySettings::_instance->m_currentArcRoutingState = ARC_STATE_ARC_TERMINATED;
            DisplaySettings::_instance->m_requestSadRetrigger = false;
              {
                        if(DisplaySettings::_instance->m_hdmiInAudioDeviceConnected !=  false) {
                            DisplaySettings::_instance->m_hdmiInAudioDeviceConnected =  false;
                DisplaySettings::_instance->connectedAudioPortUpdated(static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC), false);
                DisplaySettings::_instance->m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED;
                DisplaySettings::_instance->m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_UNKNOWN;
             }
                    
                if(DisplaySettings::_instance->m_arcEarcAudioEnabled == true) {
                            device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI_ARC0");
                            LOGINFO("%s: Disable ARC/eARC Audio\n",__FUNCTION__);
                            aPort.enableARC(static_cast<int32_t>(Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_ARC), false);
                            DisplaySettings::_instance->m_arcEarcAudioEnabled = false;
                        }
            if((DisplaySettings::_instance->m_hdmiInAudioDeviceType != 0))
                DisplaySettings::_instance->m_hdmiInAudioDeviceType = 0;

              }
                  }//Release Mutex m_AudioDeviceStatesUpdateMutex

          {
                    std::lock_guard<mutex> lck(DisplaySettings::_instance->m_callMutex);
                    if ( DisplaySettings::_instance->m_timer.isActive()) {
                        DisplaySettings::_instance->m_timer.stop();
                    }

                    if ( DisplaySettings::_instance->m_AudioDeviceDetectTimer.isActive()) {
                        DisplaySettings::_instance->m_AudioDeviceDetectTimer.stop();
                    }
                    if ( DisplaySettings::_instance->m_SADDetectionTimer.isActive()) {
                        DisplaySettings::_instance->m_SADDetectionTimer.stop();
                    }
                    if ( DisplaySettings::_instance->m_ArcDetectionTimer.isActive()) {
                        DisplaySettings::_instance->m_ArcDetectionTimer.stop();
                    }
                    if ( DisplaySettings::_instance->m_AudioDevicePowerOnStatusTimer.isActive()) {
                        DisplaySettings::_instance->m_AudioDevicePowerOnStatusTimer.stop();
                    }
                  }

                }
              }
              catch (const std::exception& err)
              {
                LOG_DEVICE_EXCEPTION0();
              }
        }


        }

        // --- onARCInitiationEventHandler ---
        void DisplaySettings::onARCInitiationEventHandler(const JsonObject& parameters) {
            string message;
	    string value;

            parameters.ToString(message);
            LOGINFO("[ARC Initiation Event], %s : %s", __FUNCTION__, C_STR(message));

            if (!parameters.HasLabel("status")) {
                LOGERR("Field 'status' could not be found in the event's payload.");
                return;
            }
            int currentrcRoutingState = getCurrentArcRoutingState();
	    LOGINFO("ARC routing state before update m_currentArcRoutingState=%d\n ", currentrcRoutingState);
	    // AVR power status is not checked here assuming that ARC init request will happen only when AVR is in ON state
            if ((currentrcRoutingState != ARC_STATE_ARC_INITIATED) && (m_systemAudioMode_Power_RequestedAndReceived == true)) {
                value = parameters["status"].String();

		if( !value.compare("success") ) {
		    //Update Arc state
                    std::lock_guard<std::mutex> lock(m_AudioDeviceStatesUpdateMutex);
                    m_currentArcRoutingState = ARC_STATE_ARC_INITIATED;
		    //Request SAD
		    // We will get Arc initiation request only if port is connected and Audio device is detected
		    // So no need to explicitly check for that
	            LOGINFO("ARC routing state after update m_currentArcRoutingState=%d\n ", m_currentArcRoutingState);
		    device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI_ARC0");
		    device::AudioStereoMode mode = device::AudioStereoMode::kStereo;  //default to stereo
                    mode = aPort.getStereoMode(); //get Last User set stereo mode and set
		    if ((m_AudioDeviceSADState == AUDIO_DEVICE_SAD_CLEARED || m_AudioDeviceSADState == AUDIO_DEVICE_SAD_UNKNOWN) && \
				    ((mode == Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_PASSTHROUGH) || aPort.getStereoAuto() == true)) {
			   LOGINFO("Initiate SAD request\n");
			   m_AudioDeviceSADState = AUDIO_DEVICE_SAD_REQUESTED;
			   sendMsgToQueue(REQUEST_SHORT_AUDIO_DESCRIPTOR, NULL);
		    } else {
			    LOGINFO("SAD not requested m_AudioDeviceSADState =%d, soundmode = %s", m_AudioDeviceSADState, mode.toString().c_str());
		    }
		    //update device type in case we receive ARC init before power ON request
		    if (m_hdmiInAudioDeviceType == 0) {
			    LOGINFO("Updating Audio device type to Arc\n");
			    m_hdmiInAudioDeviceType = static_cast<int32_t>(Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_ARC);
		    } else {
			    LOGINFO("m_hdmiInAudioDeviceType is already updated %d\n", m_hdmiInAudioDeviceType);
		    }
                    try
                    {
			if(m_hdmiInAudioDeviceConnected ==  false) {
                            m_hdmiInAudioDeviceConnected = true;
			    if (m_arcEarcConnectionNotifiedToUI == ARC_EARC_DISCONNECTED) {
				LOGINFO("Arc Initiation sucess, Notify UI\n");
			        connectedAudioPortUpdated(static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC), true);
			        m_arcEarcConnectionNotifiedToUI = ARC_EARC_CONNECTED;
			    } else {
				    LOGINFO("not notified to UI since m_arcEarcConnectionNotifiedToUI =%d\n", m_arcEarcConnectionNotifiedToUI);
			    }
			}
			else {
                            LOGINFO("onARCInitiationEventHandler: not notifying the UI as m_hdmiInAudioDeviceConnected = true !!!\n");
                        }

                    }
                    catch (const std::exception& err)
                    {
                        LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                    }
		} //Release Mutex m_AudioDeviceStatesUpdateMutex if Arc is Success
		else{
                    LOGERR("CEC ARC Initiaition Failed !!!");
                    {
                      std::lock_guard<std::mutex> lock(m_AudioDeviceStatesUpdateMutex);
                      m_currentArcRoutingState = ARC_STATE_ARC_TERMINATED;
                    }//Release Mutex m_AudioDeviceStatesUpdateMutex if Arc failure
		}
            } else {
                LOGINFO("%s: The ARC initiation already done or m_systemAudioMode_Power_RequestedAndReceived [%d]", __FUNCTION__, m_systemAudioMode_Power_RequestedAndReceived);
            }
        }

        // --- onARCTerminationEventHandler ---
        void DisplaySettings::onARCTerminationEventHandler(const JsonObject& parameters) {
            string message;
	    string value;

            parameters.ToString(message);
            LOGINFO("[ARC Termination Event], %s : %s", __FUNCTION__, C_STR(message));

	    if (m_AudioDeviceSADState != AUDIO_DEVICE_SAD_CLEARED) {
		m_AudioDeviceSADState = AUDIO_DEVICE_SAD_CLEARED;
		m_requestSadRetrigger = false;
		LOGINFO("%s: Clearing Audio device SAD\n", __FUNCTION__);
		//clear the SAD list
		sad_list.clear();
	    } else {
		LOGINFO("SAD already cleared\n");
	    }

        int currentrcRoutingState = getCurrentArcRoutingState();
	    LOGINFO("Current ARC routing state before update m_currentArcRoutingState=%d\n ", currentrcRoutingState);
	    if (currentrcRoutingState != ARC_STATE_ARC_TERMINATED) {
                if (parameters.HasLabel("status")) {
                    value = parameters["status"].String();
                    std::lock_guard<std::mutex> lock(m_AudioDeviceStatesUpdateMutex);
                    m_currentArcRoutingState = ARC_STATE_ARC_TERMINATED;
		    m_requestSadRetrigger = false;
	            LOGINFO("Current ARC routing state after update m_currentArcRoutingState=%d\n ", m_currentArcRoutingState);
                    if(!value.compare("success")) {
		        try 
		        {
			    if(m_hdmiInAudioDeviceConnected ==  true) {
				m_hdmiInAudioDeviceConnected = false;
				if (m_arcEarcConnectionNotifiedToUI == ARC_EARC_CONNECTED) {
                                    connectedAudioPortUpdated(static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC), false);
				    m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED;
				} else {
				    LOGINFO("Not notifying UI since m_arcEarcConnectionNotifiedToUI = %d", m_arcEarcConnectionNotifiedToUI);
                                }
			    }
			    else {
			        LOGINFO("onARCTerminationEventHandler: Skip Disable ARC and not notifying the UI as  m_hdmiInAudioDeviceConnected = false\n");
			    }
	                }
                        catch (const std::exception& err)
                        {
                            LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                        }
                    }
                    else{
                        LOGERR("CEC onARCTerminationEventHandler Failed !!!");
                    }
                }//Release mutex m_AudioDeviceStatesUpdateMutex 
		else {
                    LOGERR("Field 'status' could not be found in the event's payload.");
                }
	    }
        }

        // --- onAudioDeviceConnectedStatusEventHandler ---
	void DisplaySettings::onAudioDeviceConnectedStatusEventHandler(const JsonObject& parameters)
	{
	    string value;

	    if (parameters.HasLabel("audioDeviceConnected"))
		value = parameters["audioDeviceConnected"].String();
	    
	    if(!value.compare("true")) {
	        m_hdmiCecAudioDeviceDetected = true;
            } else{
	            m_hdmiCecAudioDeviceDetected = false;
		        if (m_hdmiInAudioDeviceConnected == true) {
					LOGINFO("Audio device removed event Handler, clearing the states m_hdmiInAudioDeviceConnected =%d, m_currentArcRoutingState =%d", \
                    m_hdmiInAudioDeviceConnected, m_currentArcRoutingState);
				    m_hdmiInAudioDeviceConnected = false;	
		    	    m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_UNKNOWN;
                    m_currentArcRoutingState = ARC_STATE_ARC_TERMINATED;
		    m_requestSadRetrigger = false;
				    connectedAudioPortUpdated(static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC), false);
				    m_arcEarcConnectionNotifiedToUI = ARC_EARC_DISCONNECTED;
			    }
		        if (m_AudioDeviceSADState != AUDIO_DEVICE_SAD_CLEARED && m_AudioDeviceSADState != AUDIO_DEVICE_SAD_UNKNOWN) {
		            LOGINFO("%s: Clearing Audio device SAD previous state= %d current state = %d\n", __FUNCTION__, m_AudioDeviceSADState, AUDIO_DEVICE_SAD_CLEARED);
		            //clear the SAD list
		            sad_list.clear();
		            m_AudioDeviceSADState = AUDIO_DEVICE_SAD_CLEARED;
			    m_requestSadRetrigger = false;
		        } else {
		            LOGINFO("SAD already cleared\n");
	            }
                //if m_arcEarcAudioEnabled == true(case where arc/earc is already routed) we will not reset device type because it will be done from setEnableAudioPort during disable from the connectedAudioPort update
				if (m_arcEarcAudioEnabled == false && m_hdmiInAudioDeviceType != 0) {
					LOGINFO("Reset m_hdmiInAudioDeviceType since m_arcEarcAudioEnabled = %d", m_arcEarcAudioEnabled);
					m_hdmiInAudioDeviceType = 0;
				}

            }
	    LOGINFO("updated m_hdmiCecAudioDeviceDetected status [%d] ... \n", m_hdmiCecAudioDeviceDetected);

		if (m_hdmiCecAudioDeviceDetected)
		{
                    LOGINFO("Trigger Audio Device Power State Request status ... \n");
                    sendMsgToQueue(SEND_DEVICE_AUDIO_STATUS, NULL);
		    m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_REQUEST;
                    sendMsgToQueue(REQUEST_AUDIO_DEVICE_POWER_STATUS, NULL);

		} else {
                    LOGINFO("Audio Device is removed \n");
		}
                hdmiArcVolumeLevel = 0;
		hdmiArcMuteStatus = false;
        }

        // --- onAudioDevicePowerStatusEventHandler ---
	void DisplaySettings::onAudioDevicePowerStatusEventHandler(const JsonObject& parameters) {
            string value;
            if (parameters.HasLabel("powerStatus"))
                value = parameters["powerStatus"].String();

             int pState = 1;//STANDBY
             if (!TryParseIntInRange(value, INT_MIN, INT_MAX, pState)) {
                 LOGWARN("powerStatus is not a valid int\n");
                 return;
             }

	     LOGINFO("Audio Device Power State [%d] ... \n", pState);

             if(pState == AVR_POWER_STATE_ON) {//ON
                m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_ON;
	        m_systemAudioMode_Power_RequestedAndReceived = true; // received power ON msg from AVR.

                if (m_AudioDevicePowerOnStatusTimer.isActive()) {
	           LOGINFO("Stopping timer, Audio Device power status - m_hdmiInAudioDevicePowerState [%d]!!!\n", m_hdmiInAudioDevicePowerState);
                   retryPowerRequestCount = 0;
                   m_AudioDevicePowerOnStatusTimer.stop();
                }

                try {
                    int types = 0;
                    device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI_ARC0");
                    aPort.getSupportedARCTypes(&types);
                    if((types & static_cast<int32_t>(Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_EARC)) && (m_hdmiInAudioDeviceConnected == false)) {
			    LOGINFO("%s: Audio device is eArc m_hdmiInAudioDeviceConnected =%d",__FUNCTION__,m_hdmiInAudioDeviceConnected);
                        m_hdmiInAudioDeviceConnected = true;
			m_hdmiInAudioDeviceType = static_cast<int32_t>(Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_EARC);
			if (m_arcEarcConnectionNotifiedToUI == ARC_EARC_DISCONNECTED) {
			    // Notify UI that Audio device is connected and is in ON state
                            LOGINFO("Triggered from HPD: eARC audio device power on: Notify UI !!! \n");
                            connectedAudioPortUpdated(static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC), true);
			    m_arcEarcConnectionNotifiedToUI = ARC_EARC_CONNECTED;
			} else {
				LOGINFO("eARC connection notification is already sent m_arcEarcConnectionNotifiedToUI =%d", m_arcEarcConnectionNotifiedToUI);
			}
                    } else {
			if ((m_hdmiInAudioDeviceConnected == false) && !(m_ArcDetectionTimer.isActive())) {
			    // tinymix commad to detect eArc is failed, start the timer for 3 seconds
			    LOGINFO("Starting timer to detect eArc for %d milli seconds", ARC_DETECTION_CHECK_TIME_IN_MILLISECONDS);
		            m_ArcDetectionTimer.start(ARC_DETECTION_CHECK_TIME_IN_MILLISECONDS);
			}
		    }
                }
                catch (const std::exception& err)
                {
                    LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                }
             }
             else if(pState == AVR_POWER_STATE_STANDBY) {
                 m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_STANDBY;
             } else if (pState == AVR_POWER_STATE_STANDBY_TO_ON_TRANSITION) {
		     //Start a timer to re check the power status of AVR?
		     LOGINFO("Audio device  power status IN TRANSITION from STANDBY to ON, Requesting power status again pState=%d\n", pState);
		     m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_REQUEST;
                     sendMsgToQueue(REQUEST_AUDIO_DEVICE_POWER_STATUS, NULL);
	     }
        }

        // --- checkAudioDevicePowerStatusTimer ---
 void DisplaySettings::checkAudioDevicePowerStatusTimer()
 {

    lock_guard<mutex> lck(m_callMutex);
           if (m_subscribed && m_hdmiCecAudioDeviceDetected)
           {
                // Some AVR's and SB are not sending response for power on message even though it is in ON state
                // Send power request immediately to query power status of the AVR
                LOGINFO("[HDMI_ARC0] m_hdmiInAudioDevicePowerState [%d] \n", m_hdmiInAudioDevicePowerState);
		if (m_hdmiInAudioDevicePowerState != AUDIO_DEVICE_POWER_STATE_ON)
		{
                   m_hdmiInAudioDevicePowerState = AUDIO_DEVICE_POWER_STATE_REQUEST;
		   if ((retryPowerRequestCount == 2) || (retryPowerRequestCount == 4)) // Send Power On msg again for 3rd and 4th iteration
		   {
                        LOGINFO("[HDMI_ARC0] sendHdmiCecSinkAudioDevicePowerOn !!! \n");
                        sendMsgToQueue(SEND_AUDIO_DEVICE_POWERON_MSG, NULL);
		   }
                   sendMsgToQueue(REQUEST_AUDIO_DEVICE_POWER_STATUS, NULL);
		   retryPowerRequestCount++;
                   LOGINFO("[HDMI_ARC0] sendAudioDevicePowerStatusRequestMsg, retryPowerRequestCount [%d]\n", retryPowerRequestCount);
		}
            } else {
                LOGINFO("%s: No Audio device detected\n", __FUNCTION__);
            }

//            if (((m_hdmiInAudioDevicePowerState == AUDIO_DEVICE_POWER_STATE_ON) || (retryPowerRequestCount >= 5)) && m_AudioDevicePowerOnStatusTimer.isActive()) {
            if ((retryPowerRequestCount >= 5) && m_AudioDevicePowerOnStatusTimer.isActive()) {
	       m_systemAudioMode_Power_RequestedAndReceived = true; // resetting the Variable if power status not received.
	       LOGINFO("Stopping timer, Audio Device power status - m_hdmiInAudioDevicePowerState [%d]!!!\n", m_hdmiInAudioDevicePowerState);
               retryPowerRequestCount = 0;
               m_AudioDevicePowerOnStatusTimer.stop();
            }
 }

        // --- checkArcDeviceConnected ---
        void DisplaySettings::checkArcDeviceConnected() {
	    //Timer is invoked in case of delayed HPD
	    LOGINFO("Inside checkArcDeviceConnected\n");
	    static int retryArcCount = 0;
	    std::lock_guard<std::mutex> lock(m_callMutex);
            int types = 0;
	    try{
            device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI_ARC0");
            aPort.getSupportedARCTypes(&types);
	    if(m_currentArcRoutingState != ARC_STATE_ARC_INITIATED) {
	       if((types & static_cast<int32_t>(Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_EARC)) && (m_hdmiInAudioDeviceConnected == false)) {
                   m_hdmiInAudioDeviceConnected = true;
		   m_hdmiInAudioDeviceType = static_cast<int32_t>(Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_EARC);
		   if (m_arcEarcConnectionNotifiedToUI == ARC_EARC_DISCONNECTED) {
                       LOGINFO("Triggered from HPD: eARC audio device power on: Notify UI !!! \n");
                       connectedAudioPortUpdated(static_cast<int>(Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC), true);
		       m_arcEarcConnectionNotifiedToUI = ARC_EARC_CONNECTED;
		   } else {
		       LOGINFO("Arc connection notification is already sent m_arcEarcConnectionNotifiedToUI = %d", m_arcEarcConnectionNotifiedToUI);
		   }
               } else if(m_hdmiInAudioDeviceConnected == false) {
		    std::lock_guard<std::mutex> lock(m_AudioDeviceStatesUpdateMutex);
		    retryArcCount ++;
		    LOGINFO("device Type is ARC, checking if eARC - retryArcCount [%d]", retryArcCount);
		    if (retryArcCount >= 3 )
		    {
		        m_hdmiInAudioDeviceType = static_cast<int32_t>(Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_ARC);
                        if((m_currentArcRoutingState == ARC_STATE_ARC_TERMINATED) && (isCecEnabled == true)) {
			    LOGINFO("ARC_mode: Send dummy ARC initiation request... \n");
                            LOGINFO("ARC_mode: Notify Arc routing with m_currentArcRoutingStat [%d] \n", DisplaySettings::_instance->m_currentArcRoutingState );
                            m_currentArcRoutingState = ARC_STATE_REQUEST_ARC_INITIATION;
                            sendMsgToQueue(SEND_REQUEST_ARC_INITIATION, NULL);
                        } else {
			    LOGINFO("Arc initiation request not sent\n");
		        }
	            }
	       }//Release Mutex m_AudioDeviceStatesUpdateMutex
	    } else {
		    LOGINFO("Arc is already initiated m_currentArcRoutingState =%d", m_currentArcRoutingState);
	    }

	    if ( m_ArcDetectionTimer.isActive() && ((retryArcCount >= 3) || (m_currentArcRoutingState == ARC_STATE_ARC_INITIATED) || (m_hdmiInAudioDeviceType != 0)) ) {
	            retryArcCount = 0; /* reset counter */
		    LOGINFO("Stopping the eArc detection timer retryArcCount = %d, m_currentArcRoutingState = %d, m_hdmiInAudioDeviceType = %d",\
				    retryArcCount, m_currentArcRoutingState, m_hdmiInAudioDeviceType);
                    m_ArcDetectionTimer.stop();
            }
	    }
            catch (const std::exception& err)
            {
                LOG_DEVICE_EXCEPTION1(string(" Exception in checkArcDeviceConnected"));
	    }
	}	

    } // namespace Plugin
} // namespace WPEFramework
