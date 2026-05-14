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

#include "DSPwrEventListener.h"
#include "DSProductTraitsHandler.h"  
#include "DSController.h"
#include "UtilsLogging.h"
#include "DeviceSettingsTypes.h"
#include "DeviceSettingsImplementation.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

//extern profile_t profileType;

#include "frontPanelIndicator.hpp"
#include "host.hpp"
#include "videoOutputPort.hpp"
#include "audioOutputPort.hpp"
#include "exception.hpp"
#include "manager.hpp"
#include "UtilsLogging.h"

// DS RPC header (already has extern "C" protection built-in)
#include "dsRpc.h"

// Extern declaration for EAS audio mode (from original dsMgr)
extern "C" {
    extern void _setEASAudioMode();
}

#define PWRMGR_REBOOT_REASON_MAINTENANCE "MAINTENANCE_REBOOT"

// Static variable accessible to functions outside namespace
static WPEFramework::Plugin::DSProductTraits::UXController* ux = nullptr;

namespace WPEFramework {
namespace Plugin {

DSPwrEventListener* DSPwrEventListener::_instance = nullptr;

DSPwrEventListener::DSPwrEventListener()
    : _pwrEventHandlerThreadID(0)
    , _stopThread(false)
    , _registeredPowerEventHandler(false)
    , _curState(PowerState::POWER_STATE_STANDBY)
    , _pwrMgrNotification(*this)
    , _service(nullptr)
    , _deviceSettings(nullptr)
{
    LOGINFO("DSPwrEventListener Constructor");
    memset(_standbyVideoPortSetting, 0, sizeof(_standbyVideoPortSetting));
    DSPwrEventListener::_instance = this;
    
    // Get DeviceSettings implementation instance
    _deviceSettings = DeviceSettingsImp::instance();
    if (!_deviceSettings) {
        LOGERR("Failed to get DeviceSettings implementation instance");
    }
}

DSPwrEventListener::~DSPwrEventListener()
{
    LOGINFO("DSPwrEventListener Destructor");
    Deinit();
}

void DSPwrEventListener::Init(PluginHost::IShell* service)
{
    LOGINFO("DSPwrEventListener::Init - Entering");
    
    _service = service;
    _service->AddRef();
    
    // profileType is already initialized in DeviceSettingsImplementation.cpp constructor
    // No need to call searchRdkProfile() again here

    if (profileType == TV) { // TV
        if (WPEFramework::Plugin::DSProductTraits::UXController::Initialize(WPEFramework::Plugin::DSProductTraits::DEFAULT_TV_PROFILE)) {
            ux = WPEFramework::Plugin::DSProductTraits::UXController::GetInstance();
        }
    } else { // STB
        if (WPEFramework::Plugin::DSProductTraits::UXController::Initialize(WPEFramework::Plugin::DSProductTraits::DEFAULT_STB_PROFILE_EUROPE)) {
            ux = WPEFramework::Plugin::DSProductTraits::UXController::GetInstance();
        }
    }
    
    if (nullptr == ux) {
        LOGINFO("DSMgr product traits not supported");
    }

    try {
        device::Manager::load();
        LOGINFO("device::Manager::load success");
    } catch (...) {
        LOGERR("Exception Caught during device::Manager::load");
    }

    IARM_Result_t rc;
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetStandbyVideoState, SetStandbyVideoState);
    if (IARM_RESULT_SUCCESS != rc) {
        LOGERR("IARM_Bus_RegisterCall Failed for SetStandbyVideoState, Error: %d", rc);
    }
    
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_GetStandbyVideoState, GetStandbyVideoState);
    if (IARM_RESULT_SUCCESS != rc) {
        LOGERR("IARM_Bus_RegisterCall Failed for GetStandbyVideoState, Error: %d", rc);
    }
    
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetAvPortState, SetAvPortState);
    if (IARM_RESULT_SUCCESS != rc) {
        LOGERR("IARM_Bus_RegisterCall Failed for SetAvPortState, Error: %d", rc);
    }
    
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetLEDStatus, SetLEDState);
    if (IARM_RESULT_SUCCESS != rc) {
        LOGERR("IARM_Bus_RegisterCall Failed for SetLEDStatus, Error: %d", rc);
    }
    
    rc = IARM_Bus_RegisterCall(IARM_BUS_DSMGR_API_SetRebootConfig, SetRebootConfig);
    if (IARM_RESULT_SUCCESS != rc) {
        LOGERR("IARM_Bus_RegisterCall Failed for SetRebootConfig, Error: %d", rc);
    }

    // Initialize mutexes and condition variables
    pthread_mutex_init(&_pwrEventQueueMutexLock, NULL);
    pthread_mutex_init(&_pwrEventMutexLock, NULL);
    pthread_cond_init(&_pwrEventMutexCond, NULL);

    _stopThread = false;
    if (pthread_create(&_pwrEventHandlerThreadID, NULL, PwrEventHandlingThreadFunc, this) != 0) {
        LOGERR("DSMgr PwrEventHandlingThread creation failed");
    }

    // Initialize PowerManager connection using retry pattern (like original dsMgr)
    LOGINFO("DSMgr PowerManager Connect setup in a Thread");
    PwrCtrlEstablishConnection();
}

void DSPwrEventListener::Deinit()
{
    LOGINFO("DSPwrEventListener::Deinit - Entering");

    if (_powerManagerPlugin) {
        _powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
        _powerManagerPlugin.Reset();
    }
    _registeredPowerEventHandler = false;

    pthread_mutex_lock(&_pwrEventMutexLock);
    _stopThread = true;
    pthread_cond_signal(&_pwrEventMutexCond);
    pthread_mutex_unlock(&_pwrEventMutexLock);

    LOGINFO("Before joining thread");
    pthread_join(_pwrEventHandlerThreadID, NULL);
    LOGINFO("Completed joining thread");

    pthread_mutex_lock(&_pwrEventQueueMutexLock);
    while (!_pwrEventQueue.empty()) {
        _pwrEventQueue.pop();
    }
    pthread_mutex_unlock(&_pwrEventQueueMutexLock);

    pthread_cond_destroy(&_pwrEventMutexCond);
    pthread_mutex_destroy(&_pwrEventQueueMutexLock);
    pthread_mutex_destroy(&_pwrEventMutexLock);

    if (_service) {
        _service->Release();
        _service = nullptr;
    }
}

void DSPwrEventListener::InitializePowerManager()
{
    LOGINFO("InitializePowerManager - Connecting to PowerManager plugin");
    PowerState pwrStateCur = PowerState::POWER_STATE_UNKNOWN;
    PowerState pwrStatePrev = PowerState::POWER_STATE_UNKNOWN;
    Core::hresult retStatus = Core::ERROR_GENERAL;
    
    _powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
        .withIShell(_service)
        .withRetryIntervalMS(200)
        .withRetryCount(25)
        .createInterface();

    registerPowerEventHandler();

    if (_powerManagerPlugin) {
        retStatus = _powerManagerPlugin->GetPowerState(pwrStateCur, pwrStatePrev);
    }
    
    if (Core::ERROR_NONE == retStatus) {
        _curState = pwrStateCur;
        LOGINFO("InitializePowerManager - Current power state: %d", _curState);
        PwrControllerFetchNinitStateValues();
    } else {
        LOGERR("InitializePowerManager - Failed to get power state");
    }
}

void DSPwrEventListener::registerPowerEventHandler()
{
    if (!_registeredPowerEventHandler && _powerManagerPlugin) {
        LOGINFO("Registering PowerManager event handler");
        _registeredPowerEventHandler = true;
        _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
    } else {
        LOGINFO("PowerManager event handler already registered or plugin not available");
    }
}

void PowerManagerNotification::OnPowerModeChanged(const PowerState currentState, const PowerState newState)
{
    _parent.onPowerModeChanged(currentState, newState);
}

void DSPwrEventListener::onPowerModeChanged(const PowerState currentState, const PowerState newState)
{
    LOGINFO("DSPwrEventListener::onPowerModeChanged - currentState: %d, newState: %d", currentState, newState);
    
    // Queue the event for thread processing (same pattern as dsMgr original)
    pthread_mutex_lock(&_pwrEventQueueMutexLock);
    _pwrEventQueue.emplace(currentState, newState);
    pthread_mutex_unlock(&_pwrEventQueueMutexLock);
    
    LOGINFO("Sending signal to thread for processing callback event");
    pthread_mutex_lock(&_pwrEventMutexLock);
    pthread_cond_signal(&_pwrEventMutexCond);
    pthread_mutex_unlock(&_pwrEventMutexLock);
}

void DSPwrEventListener::PwrCtrlEstablishConnection()
{
    LOGINFO("DSPwrEventListener::PwrCtrlEstablishConnection - Entering");
    
    // Start retry thread for PowerManager connection (like original dsMgr pattern)
    pthread_t pwrConnectThreadID;
    
    if (pthread_create(&pwrConnectThreadID, NULL, PwrRetryEstablishConnThread, this) == 0) {
        if (pthread_detach(pwrConnectThreadID) != 0) {
            LOGERR("DSPwrEventListener PwrCtrlEstablishConnection Thread detach Failed");
        }
    } else {
        LOGERR("DSPwrEventListener PwrCtrlEstablishConnection Thread Creation Failed");
    }
}

void DSPwrEventListener::PwrControllerFetchNinitStateValues()
{
    LOGINFO("DSPwrEventListener::PwrControllerFetchNinitStateValues");
    
    PowerState powerStateBeforeReboot = PowerState::POWER_STATE_STANDBY;

    // Note: _curState is already set in InitializePowerManager from GetPowerState
    LOGINFO("Current Power State: %d", _curState);

    if (nullptr != ux) {
        ux->ApplyPostRebootConfig(_curState, powerStateBeforeReboot);
    }

    if (nullptr == ux) {
#ifndef DISABLE_LED_SYNC_IN_BOOTUP
        SetLEDStatus(_curState);
#endif
        SetAVPortsPowerState(_curState);
    }
}

void DSPwrEventListener::HandlePwrEventData(const PowerState currentState,
                                           const PowerState newState)
{
    LOGINFO("HandlePwrEventData - currentState: %d, newState: %d", currentState, newState);
    
    if (nullptr != ux) {
        ux->ApplyPowerStateChangeConfig(newState, currentState);
    } else {
#ifndef DISABLE_LED_SYNC_IN_BOOTUP
        SetLEDStatus(newState);
#endif
        SetAVPortsPowerState(newState);
    }
}

int DSPwrEventListener::SetLEDStatus(PowerState powerState)
{
    LOGINFO("SetLEDStatus - powerState: %d", powerState);
    
    try {
        if (_deviceSettings) {
            FPDIndicator indicator = static_cast<FPDIndicator>(dsFPD_INDICATOR_POWER);
            FPDState fpdState;
            
            if (PowerState::POWER_STATE_ON != powerState) {
                if (profileType == TV) {
                    fpdState = FPDState::DS_FPD_STATE_ON;
                    LOGINFO("Settings Power LED State to ON");
                } else {
                    fpdState = FPDState::DS_FPD_STATE_OFF;
                    LOGINFO("Settings Power LED State to OFF");
                }
            } else {
                fpdState = FPDState::DS_FPD_STATE_ON;
                LOGINFO("Settings Power LED State to ON");
            }

            uint32_t result = _deviceSettings->SetFPDState(indicator, fpdState);
            if (result != WPEFramework::Core::ERROR_NONE) {
                LOGERR("SetFPDState failed with error: %d", result);
                return -1;
            }
        } else {
            LOGERR("DeviceSettings implementation not available");
            return -1;
        }
    } catch (...) {
        LOGERR("Exception Caught during SetLEDStatus");
        return -1;
    }
    
    return 0;
}

int DSPwrEventListener::SetAVPortsPowerState(PowerState powerState)
{
    LOGINFO("SetAVPortsPowerState - powerState: %d", powerState);
    
    try {
        if (PowerState::POWER_STATE_ON != powerState) {
            // Non-ON power state (standby or off) - certain ports may stay on in standby modes
            try {
                device::List<device::VideoOutputPort> videoPorts = device::Host::getInstance().getVideoOutputPorts();
                LOGINFO("Number of Video Ports: %zu", videoPorts.size());
                
                for (size_t i = 0; i < videoPorts.size(); i++) {
                    try {
                        device::VideoOutputPort vPort = videoPorts.at(i);
                        bool doEnable = GetVideoPortStandbySetting(vPort.getName().c_str());
                        LOGINFO("Video port %s will be %s for PowerState %d", 
                               vPort.getName().c_str(), 
                               (doEnable ? "enabled" : "disabled"), 
                               static_cast<int>(powerState));
                               
                        if ((false == doEnable) || (PowerState::POWER_STATE_OFF == powerState)) {
                            // Disable the port
                            // Get port type using DS HAL APIs for proper type identification
                            int portTypeId = 0;
                            // Use DS HAL to get port type ID - fallback to HDMI if unavailable 
                            if (vPort.getName().find("HDMI") != std::string::npos) {
                                portTypeId = dsVIDEOPORT_TYPE_HDMI;
                            } else if (vPort.getName().find("COMPONENT") != std::string::npos) {
                                portTypeId = dsVIDEOPORT_TYPE_COMPONENT;
                            } else {
                                portTypeId = dsVIDEOPORT_TYPE_HDMI; // default
                            }
                            dsVideoPortType_t videoPortType = static_cast<dsVideoPortType_t>(portTypeId);
                            uint32_t result = ConfigureVideoPort(vPort.getName(), 
                                                               static_cast<VideoPortType>(videoPortType), 
                                                               vPort.getIndex(), 
                                                               false);
                            if (result == WPEFramework::Core::ERROR_NONE) {
                                LOGINFO("VideoPort %s disabled for powerState %d", 
                                       vPort.getName().c_str(), static_cast<int>(powerState));
                            }
                        } else {
                            LOGINFO("VideoPort %s stays enabled for powerState %d", 
                                   vPort.getName().c_str(), static_cast<int>(powerState));
                        }
                    } catch (...) {
                        LOGERR("Exception caught in video port processing for port %zu", i);
                    }
                }
            } catch (...) {
                LOGERR("Exception caught during video port enumeration");
            }
            
            // Configure Audio Ports  
            try {
                device::List<device::AudioOutputPort> audioPorts = device::Host::getInstance().getAudioOutputPorts();
                LOGINFO("Number of Audio Ports: %zu", audioPorts.size());
                
                for (size_t i = 0; i < audioPorts.size(); i++) {
                    try {
                        device::AudioOutputPort aPort = audioPorts.at(i);
                        bool isConfigSkipped = false;
                        // Get port type using DS HAL APIs for proper type identification
                        int portTypeId = 0;
                        // Use DS HAL to get port type ID - fallback to HDMI Output if unavailable
                        if (aPort.getName().find("HDMI") != std::string::npos) {
                            portTypeId = dsAUDIOPORT_TYPE_HDMI;
                        } else if (aPort.getName().find("SPDIF") != std::string::npos) {
                            portTypeId = dsAUDIOPORT_TYPE_SPDIF;
                        } else {
                            portTypeId = dsAUDIOPORT_TYPE_HDMI; // default
                        }
                        dsAudioPortType_t audioPortType = static_cast<dsAudioPortType_t>(portTypeId);
                        
                        uint32_t result = ConfigureAudioPort(aPort.getName(),
                                                           static_cast<AudioPortType>(audioPortType),
                                                           aPort.getIndex(),
                                                           false,
                                                           &isConfigSkipped);
                        if (result == WPEFramework::Core::ERROR_NONE) {
                            LOGINFO("AudioPort %s disabled for powerState %d", 
                                   aPort.getName().c_str(), static_cast<int>(powerState));
                        }
                    } catch (...) {
                        LOGERR("Exception caught in audio port processing for port %zu", i);
                    }
                }
            } catch (...) {
                LOGERR("Exception caught during audio port enumeration");
            }
        } else {
            // POWER_STATE_ON - Enable all ports
            try {
                device::List<device::VideoOutputPort> videoPorts = device::Host::getInstance().getVideoOutputPorts();
                
                for (size_t i = 0; i < videoPorts.size(); i++) {
                    try {
                        device::VideoOutputPort vPort = videoPorts.at(i);
                        // Get port type using DS HAL APIs for proper type identification
                        int portTypeId = 0;
                        // Use DS HAL to get port type ID - fallback to HDMI if unavailable 
                        if (vPort.getName().find("HDMI") != std::string::npos) {
                            portTypeId = dsVIDEOPORT_TYPE_HDMI;
                        } else if (vPort.getName().find("COMPONENT") != std::string::npos) {
                            portTypeId = dsVIDEOPORT_TYPE_COMPONENT;
                        } else {
                            portTypeId = dsVIDEOPORT_TYPE_HDMI; // default
                        }
                        dsVideoPortType_t videoPortType = static_cast<dsVideoPortType_t>(portTypeId);
                        
                        uint32_t result = ConfigureVideoPort(vPort.getName(),
                                                           static_cast<VideoPortType>(videoPortType),
                                                           vPort.getIndex(),
                                                           true);
                        if (result == WPEFramework::Core::ERROR_NONE) {
                            LOGINFO("VideoPort %s enabled for powerState %d", 
                                   vPort.getName().c_str(), static_cast<int>(powerState));
                        }
                    } catch (...) {
                        LOGERR("Exception caught in video port processing for port %zu", i);
                    }
                }
                
                device::List<device::AudioOutputPort> audioPorts = device::Host::getInstance().getAudioOutputPorts();
                for (size_t i = 0; i < audioPorts.size(); i++) {
                    try {
                        device::AudioOutputPort aPort = audioPorts.at(i);
                        bool isConfigSkipped = false;
                        // Get port type using DS HAL APIs for proper type identification
                        int portTypeId = 0;
                        // Use DS HAL to get port type ID - fallback to HDMI Output if unavailable
                        if (aPort.getName().find("HDMI") != std::string::npos) {
                            portTypeId = dsAUDIOPORT_TYPE_HDMI;
                        } else if (aPort.getName().find("SPDIF") != std::string::npos) {
                            portTypeId = dsAUDIOPORT_TYPE_SPDIF;
                        } else {
                            portTypeId = dsAUDIOPORT_TYPE_HDMI; // default
                        }
                        dsAudioPortType_t audioPortType = static_cast<dsAudioPortType_t>(portTypeId);
                        
                        uint32_t result = ConfigureAudioPort(aPort.getName(),
                                                           static_cast<AudioPortType>(audioPortType),
                                                           aPort.getIndex(),
                                                           true,
                                                           &isConfigSkipped);
                        if (result == WPEFramework::Core::ERROR_NONE && !isConfigSkipped) {
                            LOGINFO("AudioPort %s enabled for powerState %d", 
                                   aPort.getName().c_str(), static_cast<int>(powerState));
                        }
                    } catch (...) {
                        LOGERR("Exception caught in audio port processing for port %zu", i);
                    }
                }
                
                // Special EAS mode handling
                if (DSController::instance()->getEASMode() == IARM_BUS_SYS_MODE_EAS) {
                    LOGINFO("Force Stereo in EAS mode");
                    // Set EAS audio mode using original dsMgr function
                    _setEASAudioMode();
                }
                
            } catch (...) {
                LOGERR("Exception caught during video port enumeration");
            }
        }
    } catch (...) {
        LOGERR("Exception Caught during SetAVPortsPowerState");
        return -1;
    }
    
    LOGINFO("Exiting SetAVPortsPowerState");
    return 0;
}

bool DSPwrEventListener::GetVideoPortStandbySetting(const char* port)
{
    if (NULL == port) {
        LOGERR("Port name is NULL");
        return false;
    }
    
    for (int i = 0; i < MAX_NUM_VIDEO_PORTS; i++) {
        if (0 == strncasecmp(port, _standbyVideoPortSetting[i].port, DSMGR_MAX_VIDEO_PORT_NAME_LENGTH)) {
            return _standbyVideoPortSetting[i].isEnabled;
        }
    }
    return false; // Default: video port is disabled in standby mode
}



PowerState DSPwrEventListener::PwrMgrToPowerControllerPowerState(int pwrMgrState)
{
    PowerState powerState = PowerState::POWER_STATE_UNKNOWN;
    
    switch (pwrMgrState) {
        case 0: // PWRMGR_POWERSTATE_OFF
            powerState = PowerState::POWER_STATE_OFF;
            break;
        case 1: // PWRMGR_POWERSTATE_STANDBY
            powerState = PowerState::POWER_STATE_STANDBY;
            break;
        case 2: // PWRMGR_POWERSTATE_ON
            powerState = PowerState::POWER_STATE_ON;
            break;
        case 3: // PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP
            powerState = PowerState::POWER_STATE_STANDBY_LIGHT_SLEEP;
            break;
        case 4: // PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP
            powerState = PowerState::POWER_STATE_STANDBY_DEEP_SLEEP;
            break;
        default:
            LOGERR("Invalid Power State: %d", pwrMgrState);
            break;
    }
    
    LOGINFO("pwrMgrState=%d converted to powerState=%d", pwrMgrState, static_cast<int>(powerState));
    return powerState;
}

void DSPwrEventListener::InitPwrControllerEvt()
{
    LOGINFO("DSPwrEventListener::InitPwrControllerEvt - Entering");

    // Initialize mutexes and condition variables (already done in constructor)
    // Thread is already created in Init() method
    
    // This method is kept for compatibility with original dsMgr pattern
    // The actual mutex/thread initialization happens in Init()
    LOGINFO("Power Controller Event handling initialized");
}

void DSPwrEventListener::DeinitPwrControllerEvt()
{
    LOGINFO("DSPwrEventListener::DeinitPwrControllerEvt - Entering");

    // Stop thread and cleanup
    pthread_mutex_lock(&_pwrEventMutexLock);
    _stopThread = true;
    pthread_cond_signal(&_pwrEventMutexCond);
    pthread_mutex_unlock(&_pwrEventMutexLock);

    LOGINFO("Before joining thread");
    pthread_join(_pwrEventHandlerThreadID, NULL);
    LOGINFO("Completed joining thread");

    // Clean the queue with guarding mutex
    pthread_mutex_lock(&_pwrEventQueueMutexLock);
    while (!_pwrEventQueue.empty()) {
        _pwrEventQueue.pop();
    }
    pthread_mutex_unlock(&_pwrEventQueueMutexLock);

    // Destroy condition variable and mutexes (handled in destructor)
    LOGINFO("Power Controller Event handling deinitialized");
}

} // namespace Plugin  
} // namespace WPEFramework

// Static member functions defined outside namespace with full qualification
IARM_Result_t WPEFramework::Plugin::DSPwrEventListener::SetStandbyVideoState(void* arg)
{
    if (NULL == arg) {
        return IARM_RESULT_INVALID_PARAM;
    }
    
    if (!_instance) {
        return IARM_RESULT_INVALID_STATE;
    }
    
    dsMgrStandbyVideoStateParam_t* param = (dsMgrStandbyVideoStateParam_t*)arg;
    param->result = 0;

    int i = 0;
    for (i = 0; i < MAX_NUM_VIDEO_PORTS; i++) {
        if (0 == strncasecmp(param->port, _instance->_standbyVideoPortSetting[i].port, DSMGR_MAX_VIDEO_PORT_NAME_LENGTH)) {
            _instance->_standbyVideoPortSetting[i].isEnabled = ((0 == param->isEnabled) ? false : true);
            break;
        }
    }
    
    if (MAX_NUM_VIDEO_PORTS == i) {
        for (i = 0; i < MAX_NUM_VIDEO_PORTS; i++) {
            if ('\0' == _instance->_standbyVideoPortSetting[i].port[0]) {
                strncpy(_instance->_standbyVideoPortSetting[i].port, param->port, (DSMGR_MAX_VIDEO_PORT_NAME_LENGTH - 1));
                _instance->_standbyVideoPortSetting[i].isEnabled = ((0 == param->isEnabled) ? false : true);
                break;
            }
        }
    }
    
    if (MAX_NUM_VIDEO_PORTS == i) {
        LOGERR("Error! Out of room to write new video port setting for standby mode");
    }
    
    // Apply setting immediately if currently in standby state (like original dsMgr)
    try {
        if (PowerState::POWER_STATE_ON != _instance->_curState && PowerState::POWER_STATE_OFF != _instance->_curState) {
            // We're currently in one of the standby states. Apply this new setting right away.
            LOGINFO("Setting standby %s port status to %s immediately", 
                   param->port, (param->isEnabled ? "enabled" : "disabled"));
            
            device::VideoOutputPort& vPort = device::Host::getInstance().getVideoOutputPort(param->port);
            if (1 == param->isEnabled) {
                vPort.enable();
            } else {
                vPort.disable();
            }
        } else {
            LOGINFO("Video port %s will be %s when going into standby mode", 
                   param->port, (param->isEnabled ? "enabled" : "disabled"));
        }
    } catch (...) {
        LOGERR("Exception caught during immediate video port setting for %s. Possible bad video port", param->port);
        param->result = -1;
    }
    
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t WPEFramework::Plugin::DSPwrEventListener::GetStandbyVideoState(void* arg)
{
    if (NULL == arg) {
        return IARM_RESULT_INVALID_PARAM;
    }
    
    if (!_instance) {
        return IARM_RESULT_INVALID_STATE;
    }
    
    dsMgrStandbyVideoStateParam_t* param = (dsMgrStandbyVideoStateParam_t*)arg;
    param->isEnabled = (_instance->GetVideoPortStandbySetting(param->port) ? 1 : 0);
    param->result = 0;
    
    return IARM_RESULT_SUCCESS;
}

void* WPEFramework::Plugin::DSPwrEventListener::PwrRetryEstablishConnThread(void* arg)
{
    LOGINFO("PwrRetryEstablishConnThread: Entry");
    DSPwrEventListener* listener = static_cast<DSPwrEventListener*>(arg);
    
    while (true) {
        // Check if PowerManager connection is successful
        if (listener->_powerManagerPlugin && listener->_registeredPowerEventHandler) {
            LOGINFO("PwrRetryEstablishConnThread PowerManager connection is success");
            listener->PwrControllerFetchNinitStateValues();
            break;
        } else {
            // Retry PowerManager initialization after delay
            usleep(DSMGR_PWR_CNTRL_CONNECT_WAIT_TIME_MS);
            listener->InitializePowerManager();
        }
    }
    LOGINFO("PwrRetryEstablishConnThread Completed Exit");
    return arg;
}

void* WPEFramework::Plugin::DSPwrEventListener::PwrEventHandlingThreadFunc(void* arg)
{
    LOGINFO("PwrEventHandlingThreadFunc: Entry");
    DSPwrEventListener* listener = static_cast<DSPwrEventListener*>(arg);

    while (true) {
        pthread_mutex_lock(&listener->_pwrEventMutexLock);
        LOGINFO("PwrEventHandlingThreadFunc... Wait for Events");
        
        pthread_mutex_lock(&listener->_pwrEventQueueMutexLock);
        bool queueEmpty = listener->_pwrEventQueue.empty();
        pthread_mutex_unlock(&listener->_pwrEventQueueMutexLock);
        
        while (!listener->_stopThread && queueEmpty) {
            pthread_cond_wait(&listener->_pwrEventMutexCond, &listener->_pwrEventMutexLock);
            pthread_mutex_lock(&listener->_pwrEventQueueMutexLock);
            queueEmpty = listener->_pwrEventQueue.empty();
            pthread_mutex_unlock(&listener->_pwrEventQueueMutexLock);
        }
        
        if (listener->_stopThread) {
            LOGINFO("PwrEventHandlingThreadFunc Exiting due to stop thread");
            pthread_mutex_unlock(&listener->_pwrEventMutexLock);
            break;
        }
        pthread_mutex_unlock(&listener->_pwrEventMutexLock);

        pthread_mutex_lock(&listener->_pwrEventQueueMutexLock);
        while (!listener->_pwrEventQueue.empty()) {
            DSMgr_Power_Event_State_t pwrEvent = listener->_pwrEventQueue.front();
            listener->_pwrEventQueue.pop();
            pthread_mutex_unlock(&listener->_pwrEventQueueMutexLock);
            
            listener->HandlePwrEventData(pwrEvent.currentState, pwrEvent.newState);
            
            pthread_mutex_lock(&listener->_pwrEventQueueMutexLock);
        }
        pthread_mutex_unlock(&listener->_pwrEventQueueMutexLock);
    }
    return arg;
}

IARM_Result_t WPEFramework::Plugin::DSPwrEventListener::SetAvPortState(void* arg) {
    
    if (nullptr == arg || nullptr == _instance) {
        return IARM_RESULT_INVALID_PARAM;
    }
    
    dsMgrAVPortStateParam_t* param = (dsMgrAVPortStateParam_t*)arg;
    PowerState powerState = _instance->PwrMgrToPowerControllerPowerState(param->avPortPowerState);
    
    if (PowerState::POWER_STATE_UNKNOWN != powerState) {
        _instance->SetAVPortsPowerState(powerState);
    }
    
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t WPEFramework::Plugin::DSPwrEventListener::SetLEDState(void* arg)
{
    if (NULL == arg || !_instance) {
        return IARM_RESULT_INVALID_PARAM;
    }
    
    dsMgrLEDStatusParam_t* param = (dsMgrLEDStatusParam_t*)arg;
    PowerState powerState = _instance->PwrMgrToPowerControllerPowerState(param->ledState);
    
    if (PowerState::POWER_STATE_UNKNOWN != powerState) {
        _instance->SetLEDStatus(powerState);
    }
    
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t WPEFramework::Plugin::DSPwrEventListener::SetRebootConfig(void* arg)
{
    if (NULL == arg) {
        return IARM_RESULT_INVALID_PARAM;
    }
    
    dsMgrRebootConfigParam_t* param = (dsMgrRebootConfigParam_t*)arg;
    param->reboot_reason_custom[sizeof(param->reboot_reason_custom) - 1] = '\0';
    
    if (nullptr != ux) {
        PowerState powerState = _instance->PwrMgrToPowerControllerPowerState(param->powerState);
        
        if (PowerState::POWER_STATE_UNKNOWN != powerState) {
            if (0 == strncmp(PWRMGR_REBOOT_REASON_MAINTENANCE, param->reboot_reason_custom, 
                            sizeof(param->reboot_reason_custom))) {
                ux->ApplyPreMaintenanceRebootConfig(powerState);
            } else {
                ux->ApplyPreRebootConfig(powerState);
            }
        }
    }
    
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

// DeviceSettings component methods (replacing legacy RPC calls)
uint32_t WPEFramework::Plugin::DSPwrEventListener::ConfigureVideoPort(const std::string& portName, VideoPortType portType, int index, bool requestEnable)
{
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    
    if (!_deviceSettings) {
        LOGERR("DeviceSettings implementation not available");
        return result;
    }
    
    try {
        int32_t handle = 0;
        result = _deviceSettings->GetVideoPort(portType, index, handle);
        
        if (result == WPEFramework::Core::ERROR_NONE && handle != 0) {
            result = _deviceSettings->EnableVideoPort(handle, requestEnable);
            if (result == WPEFramework::Core::ERROR_NONE) {
                LOGINFO("VideoPort %s successfully %s", portName.c_str(), (requestEnable ? "enabled" : "disabled"));
            } else {
                LOGERR("Failed to set video port %s state, Error: %d", portName.c_str(), result);
            }
        } else {
            LOGERR("Failed to get video port %s handle, Error: %d", portName.c_str(), result);
        }
    } catch (...) {
        LOGERR("Exception caught during ConfigureVideoPort for %s", portName.c_str());
        result = WPEFramework::Core::ERROR_GENERAL;
    }
    
    return result;
}

uint32_t WPEFramework::Plugin::DSPwrEventListener::ConfigureAudioPort(const std::string& portName, AudioPortType portType, int index, bool requestEnable, bool* isConfigurationSkippedPtr)
{
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    
    if (!isConfigurationSkippedPtr) {
        return WPEFramework::Core::ERROR_BAD_REQUEST;
    }
    
    *isConfigurationSkippedPtr = false;
    
    if (!_deviceSettings) {
        LOGERR("DeviceSettings implementation not available");
        return result;
    }
    
    try {
        int32_t handle = 0;
        result = _deviceSettings->GetAudioPort(portType, index, handle);
        
        if (result == WPEFramework::Core::ERROR_NONE && handle != 0) {
            if (requestEnable) {
                // Check if port should be enabled based on persistent settings
                bool persistEnabled = true;
                result = _deviceSettings->IsAudioPortEnabled(handle, persistEnabled);
                if (result == WPEFramework::Core::ERROR_NONE) {
                    if (!persistEnabled) {
                        *isConfigurationSkippedPtr = true;
                        LOGINFO("Enable AudioPort %s skipped - persistent state is disabled", portName.c_str());
                        return WPEFramework::Core::ERROR_NONE;
                    }
                }
            }
            
            result = _deviceSettings->EnableAudioPort(handle, requestEnable);
            if (result == WPEFramework::Core::ERROR_NONE) {
                LOGINFO("AudioPort %s successfully %s", portName.c_str(), (requestEnable ? "enabled" : "disabled"));
            } else {
                LOGERR("Failed to set audio port %s state, Error: %d", portName.c_str(), result);
            }
        } else {
            LOGERR("Failed to get audio port %s handle, Error: %d", portName.c_str(), result);
        }
    } catch (...) {
        LOGERR("Exception caught during ConfigureAudioPort for %s", portName.c_str());
        result = WPEFramework::Core::ERROR_GENERAL;
    }
    
    return result;
}
