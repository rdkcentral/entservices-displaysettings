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

#include "UserPlugin.h"
#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include "UtilsLogging.h"
#include "audioOutputPort.hpp"
#include "audioOutputPortType.hpp"
#include "audioOutputPortConfig.hpp"
#include "dsUtl.h"
#include "dsError.h"
#include "list.hpp"
#include "host.hpp"
#include "exception.hpp"
#include "manager.hpp"

// IARM includes for direct DsMgr daemon communication
#include "libIBus.h"
#include "rdk/ds-rpc/dsRpc.h"
#include "rdk/ds-rpc/dsMgr.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 1
#define API_VERSION_NUMBER_PATCH 0

using namespace std;
using namespace WPEFramework;

namespace WPEFramework {
namespace {
    static Plugin::Metadata<Plugin::UserPlugin> metadata(
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

    SERVICE_REGISTRATION(UserPlugin, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    UserPlugin* UserPlugin::_instance = nullptr;

    UserPlugin::UserPlugin() : _service(nullptr), _connectionId(0), _fpdManager(nullptr), _hdmiInManager(nullptr), _compositeInManager(nullptr), _audioManager(nullptr), _videoPortManager(nullptr), _videoDeviceManager(nullptr), _displayManager(nullptr), _hostManager(nullptr), _hdmiInNotification(*this), _compositeInNotification(*this), _audioNotification(*this), _videoPortNotification(*this), _videoDeviceNotification(*this), _displayNotification(*this), _hostNotification(*this)
    {
        UserPlugin::_instance = this;
        SYSLOG(Logging::Startup, (_T("UserPlugin Constructor")));
        LOGINFO("Constructor");
    }

    UserPlugin::~UserPlugin()
    {
        UserPlugin::_instance = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("UserPlugin Destructor"))));
        LOGINFO("Destructor");
    }

    /* virtual */ const string UserPlugin::Initialize(PluginHost::IShell* service)
    {
        LOGINFO("Initialize");
        _service = service;
        _service->AddRef();

        // Get DeviceSettings interface
        _deviceSettings = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettings>("org.rdk.DeviceSettings");
        if (_deviceSettings != nullptr) {
            // Get individual interfaces from the main DeviceSettings interface
            _fpdManager = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettingsFPD>("org.rdk.DeviceSettings");
            _hdmiInManager = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettingsHDMIIn>("org.rdk.DeviceSettings");
            _compositeInManager = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettingsCompositeIn>("org.rdk.DeviceSettings");
            _audioManager = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettingsAudio>("org.rdk.DeviceSettings");
            _videoPortManager = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettingsVideoPort>("org.rdk.DeviceSettings");
            _videoDeviceManager = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettingsVideoDevice>("org.rdk.DeviceSettings");
            _displayManager = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettingsDisplay>("org.rdk.DeviceSettings");
            _hostManager = _service->QueryInterfaceByCallsign<Exchange::IDeviceSettingsHost>("org.rdk.DeviceSettings");
        } else {
            LOGERR("Could not obtain DeviceSettings interface");
        }

        // Register for HDMI In notifications if interface is available
        if (_hdmiInManager) {
            _hdmiInManager->Register(&_hdmiInNotification);
        }

        // Register for CompositeIn notifications if interface is available
        if (_compositeInManager) {
            _compositeInManager->Register(&_compositeInNotification);
        }

        // Register for Audio notifications if interface is available
        if (_audioManager) {
            _audioManager->Register(&_audioNotification);
        }

        // Register for VideoPort notifications if interface is available
        if (_videoPortManager) {
            _videoPortManager->Register(&_videoPortNotification);
        }

        // Register for VideoDevice notifications if interface is available
        if (_videoDeviceManager) {
            _videoDeviceManager->Register(&_videoDeviceNotification);
        }

        // Register for Display notifications if interface is available
        if (_displayManager) {
            _displayManager->Register(static_cast<Exchange::IDeviceSettingsDisplay::INotification*>(&_displayNotification));
            _displayManager->Register(static_cast<Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification*>(&_displayNotification));
        }

        // Register for Host notifications if interface is available
        if (_hostManager) {
            _hostManager->Register(&_hostNotification);
        }

        //Test DeviceSettings interfaces
        TestSimplifiedFPDAPIs();
        TestSimplifiedHDMIInAPIs();
        TestSelectHDMIInPortAPI();
        TestCompositeInAPIs();
        TestAudioAPIs();
        TestVideoPortAPIs();
        TestVideoDeviceAPIs();
        //TestDisplayAPIs();
        TestHostAPIs();

        Exchange::JUserPlugin::Register(*this, this);

        return (string());
    }

    /* virtual */ void UserPlugin::Deinitialize(PluginHost::IShell* service)
    {
        LOGINFO("Deinitialize");

        // Unregister HDMI In notifications and release HDMI In Manager
        if (_hdmiInManager) {
            _hdmiInManager->Unregister(&_hdmiInNotification);
            _hdmiInManager->Release();
            _hdmiInManager = nullptr;
        }

        // Unregister CompositeIn notifications and release CompositeIn Manager
        if (_compositeInManager) {
            _compositeInManager->Unregister(&_compositeInNotification);
            _compositeInManager->Release();
            _compositeInManager = nullptr;
        }

        // Unregister Audio notifications and release Audio Manager
        if (_audioManager) {
            _audioManager->Unregister(&_audioNotification);
            _audioManager->Release();
            _audioManager = nullptr;
        }

        // Unregister VideoPort notifications and release VideoPort Manager
        if (_videoPortManager) {
            _videoPortManager->Unregister(&_videoPortNotification);
            _videoPortManager->Release();
            _videoPortManager = nullptr;
        }

        // Unregister VideoDevice notifications and release VideoDevice Manager
        if (_videoDeviceManager) {
            _videoDeviceManager->Unregister(&_videoDeviceNotification);
            _videoDeviceManager->Release();
            _videoDeviceManager = nullptr;
        }

        // Unregister Display notifications and release Display Manager
        if (_displayManager) {
            _displayManager->Unregister(static_cast<Exchange::IDeviceSettingsDisplay::INotification*>(&_displayNotification));
            _displayManager->Unregister(static_cast<Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification*>(&_displayNotification));
            _displayManager->Release();
            _displayManager = nullptr;
        }

        // Unregister Host notifications and release Host Manager
        if (_hostManager) {
            _hostManager->Unregister(&_hostNotification);
            _hostManager->Release();
            _hostManager = nullptr;
        }

        // Release FPD Manager
        if (_fpdManager) {
            _fpdManager->Release();
            _fpdManager = nullptr;
        }

        // Release main DeviceSettings interface
        if (_deviceSettings) {
            _deviceSettings->Release();
            _deviceSettings = nullptr;
        }

        Exchange::JUserPlugin::Unregister(*this);
        _service = nullptr;
    }

    /***
     * @brief : send notification when system power state is changed
     *
     * @param1[in]  : powerState
     * @param2[out] : {"jsonrpc": "2.0",
     *		"method": "org.rdk.SystemServices.events.1.onSystemPowerStateChanged",
     *		"param":{"powerState": <string new power state mode>}}
    */
    void UserPlugin::onPowerStateChanged(string currentPowerState, string powerState)
    {
        LOGWARN("power state changed from '%s' to '%s'", currentPowerState.c_str(), powerState.c_str()); 
    }

    /* virtual */ string UserPlugin::Information() const
    {
        // No additional info to report.
        return (string());
    }

    // PowerManager functionality moved to separate plugin - simplified GetDevicePowerState implementation
    Core::hresult UserPlugin::GetDevicePowerState(std::string& powerState) const
    {
        // PowerManager functionality moved to separate plugin, return default state
        powerState = "UNKNOWN";
        LOGINFO("PowerManager functionality moved to separate plugin - returning UNKNOWN state");
        return Core::ERROR_NONE;
    }

    Core::hresult UserPlugin::GetVolumeLevel (const string& port, string& level) const
    {
        //LOGINFOMETHOD();
        //string audioPort = port.String();
        //device::List<device::AudioOutputPort> aPorts = device::Host::getInstance().getAudioOutputPorts();
        float volumelevel = 0;

        LOGINFO("GetVolumeLevel, port name: %s", port.c_str());
        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(port);
        if (aPort.getName().empty()){
            LOGERR("AudioOutputPort '%s' is not valid!", port.c_str());
            return Core::ERROR_GENERAL;
        }
            volumelevel = aPort.getLevel();
            level = to_string(volumelevel);
            LOGINFO("GetVolumeLevel, volumeLevel: %s", level.c_str());
    //    volumelevel = aPort.getLevel();

        return Core::ERROR_NONE;
    }

    void UserPlugin::TestSimplifiedFPDAPIs()
    {
        LOGINFO("========== Simplified FPD APIs Testing Framework ==========");

        if (!_fpdManager) {
            LOGERR("FPD Manager interface is not available");
            return;
        }

        LOGINFO("Testing only 4 FPD APIs: SetFPDBrightness, GetFPDBrightness, SetFPDState, GetFPDState");
        LOGINFO("Testing only POWER indicator");

        // Test FPD indicators - only POWER indicator
        Exchange::IDeviceSettingsFPD::FPDIndicator testIndicators[] = {
            Exchange::IDeviceSettingsFPD::FPDIndicator::DS_FPD_INDICATOR_POWER
        };

        const char* indicatorNames[] = {
            "POWER"
        };

        // Test the 4 specified FPD APIs for each indicator
        for (size_t i = 0; i < sizeof(testIndicators)/sizeof(testIndicators[0]); i++) {
            auto indicator = testIndicators[i];
            const char* indicatorName = indicatorNames[i];

            LOGINFO("---------- Testing FPD Indicator: %s ----------", indicatorName);

            // 1. GetFPDBrightness - Get current brightness value
            uint32_t originalBrightness = 0;
            Core::hresult result = _fpdManager->GetFPDBrightness(indicator, originalBrightness);
            LOGINFO("GetFPDBrightness: indicator=%s, result=%u, original_brightness=%u", indicatorName, result, originalBrightness);

            if (result == Core::ERROR_NONE) {
                // 2. SetFPDBrightness - Set test brightness value
                uint32_t testBrightness = (originalBrightness == 50) ? 75 : 50;
                result = _fpdManager->SetFPDBrightness(indicator, testBrightness, false);
                LOGINFO("SetFPDBrightness: indicator=%s, result=%u, test_brightness=%u", indicatorName, result, testBrightness);

                // Verify the brightness was set correctly
                uint32_t verifyBrightness = 0;
                _fpdManager->GetFPDBrightness(indicator, verifyBrightness);
                LOGINFO("GetFPDBrightness: indicator=%s, verified_brightness=%u", indicatorName, verifyBrightness);

                // Restore original brightness
                _fpdManager->SetFPDBrightness(indicator, originalBrightness, false);
                LOGINFO("SetFPDBrightness: indicator=%s, brightness restored to %u", indicatorName, originalBrightness);
            }

            // 3. GetFPDState - Get current state value
            Exchange::IDeviceSettingsFPD::FPDState originalState;
            result = _fpdManager->GetFPDState(indicator, originalState);
            LOGINFO("GetFPDState: indicator=%s, result=%u, original_state=%u", indicatorName, result, (uint32_t)originalState);

            if (result == Core::ERROR_NONE) {
                // 4. SetFPDState - Set test state value
                Exchange::IDeviceSettingsFPD::FPDState testState = (originalState == Exchange::IDeviceSettingsFPD::FPDState::DS_FPD_STATE_ON) ? 
                    Exchange::IDeviceSettingsFPD::FPDState::DS_FPD_STATE_OFF : Exchange::IDeviceSettingsFPD::FPDState::DS_FPD_STATE_ON;
                result = _fpdManager->SetFPDState(indicator, testState);
                LOGINFO("SetFPDState: indicator=%s, result=%u, test_state=%u", indicatorName, result, (uint32_t)testState);

                // Verify the state was set correctly
                Exchange::IDeviceSettingsFPD::FPDState verifyState;
                _fpdManager->GetFPDState(indicator, verifyState);
                LOGINFO("GetFPDState: indicator=%s, verified_state=%u", indicatorName, (uint32_t)verifyState);

                // Restore original state
                _fpdManager->SetFPDState(indicator, originalState);
                LOGINFO("SetFPDState: indicator=%s, state restored to %u", indicatorName, (uint32_t)originalState);
            }
        }

        // Test GetFPDDeviceConfig (new nested iterator API)
        Exchange::IDeviceSettingsFPD::IFPDDeviceConfigIterator* deviceConfigIterator = nullptr;
        Core::hresult deviceConfigResult = _fpdManager->GetFPDDeviceConfig(deviceConfigIterator);
        LOGINFO("GetFPDDeviceConfig: result=%u, iterator=%p", deviceConfigResult, deviceConfigIterator);

        if ((deviceConfigResult == Core::ERROR_NONE) && (deviceConfigIterator != nullptr)) {
            Exchange::IDeviceSettingsFPD::FPDDeviceConfigInfo deviceConfig;
            uint32_t index = 0;

            while (deviceConfigIterator->Next(deviceConfig) == true) {
                LOGINFO("GetFPDDeviceConfig[%u][Device]: deviceId=%u hardwareSku=%u firmwareBuild=%u capabilitiesMask=%u",
                    index, deviceConfig.deviceId, deviceConfig.hardwareSku, deviceConfig.firmwareBuild, deviceConfig.capabilitiesMask);
                LOGINFO("GetFPDDeviceConfig[%u][Device]: panelBrightness=%.2f ambientLightLevel=%.2f powerLimitPercent=%.2f refreshRateHz=%.2f",
                    index, deviceConfig.panelBrightness, deviceConfig.ambientLightLevel, deviceConfig.powerLimitPercent, deviceConfig.refreshRateHz);
                LOGINFO("GetFPDDeviceConfig[%u][Device]: boardTemperature=%.3f inputVoltage=%.3f totalPowerWatt=%.3f uptimeSeconds=%.3f",
                    index, deviceConfig.boardTemperature, deviceConfig.inputVoltage, deviceConfig.totalPowerWatt, deviceConfig.uptimeSeconds);
                LOGINFO("GetFPDDeviceConfig[%u][Device]: bootEpoch=%ld provisionEpoch=%ld lastSyncEpoch=%ld serialNumeric=%ld",
                    index, deviceConfig.bootEpoch, deviceConfig.provisionEpoch, deviceConfig.lastSyncEpoch, deviceConfig.serialNumeric);
                LOGINFO("GetFPDDeviceConfig[%u][Device]: deviceName=%s version=%s serialNumber=%s manufacturer=%s",
                    index,
                    deviceConfig.deviceName.c_str(),
                    deviceConfig.version.c_str(),
                    deviceConfig.serialNumber.c_str(),
                    deviceConfig.manufacturer.c_str());

                const Exchange::IDeviceSettingsFPD::FPDElementGroupInfo& group = deviceConfig.primaryGroup;
                LOGINFO("GetFPDDeviceConfig[%u][Group]: groupId=%u elementCount=%u activeCount=%u priority=%u",
                    index, group.groupId, group.elementCount, group.activeCount, group.priority);
                LOGINFO("GetFPDDeviceConfig[%u][Group]: groupBrightnessScale=%.3f powerBudgetPercent=%.3f heartbeatSec=%.3f failoverDelaySec=%.3f",
                    index, group.groupBrightnessScale, group.powerBudgetPercent, group.heartbeatSec, group.failoverDelaySec);
                LOGINFO("GetFPDDeviceConfig[%u][Group]: maxGroupPowerWatt=%.3f avgCurrentAmp=%.3f uptimeHours=%.3f temperatureAvg=%.3f",
                    index, group.maxGroupPowerWatt, group.avgCurrentAmp, group.uptimeHours, group.temperatureAvg);
                LOGINFO("GetFPDDeviceConfig[%u][Group]: createdEpoch=%ld modifiedEpoch=%ld scheduleId=%ld policyVersion=%ld",
                    index, group.createdEpoch, group.modifiedEpoch, group.scheduleId, group.policyVersion);
                LOGINFO("GetFPDDeviceConfig[%u][Group]: groupName=%s policyName=%s owner=%s comment=%s",
                    index,
                    group.groupName.c_str(),
                    group.policyName.c_str(),
                    group.owner.c_str(),
                    group.comment.c_str());

                const Exchange::IDeviceSettingsFPD::FPDElementConfigInfo& element = group.representativeElement;
                LOGINFO("GetFPDDeviceConfig[%u][Element]: elementId=%u gpioPin=%u pwmChannel=%u maxBrightness=%u minBrightness=%u",
                    index, element.elementId, element.gpioPin, element.pwmChannel, element.maxBrightness, element.minBrightness);
                LOGINFO("GetFPDDeviceConfig[%u][Element]: defaultBrightness=%.3f blinkRateHz=%.3f thermalLimitC=%.3f updateIntervalSec=%.3f",
                    index, element.defaultBrightness, element.blinkRateHz, element.thermalLimitC, element.updateIntervalSec);
                LOGINFO("GetFPDDeviceConfig[%u][Element]: maxCurrentAmp=%.3f operatingVoltage=%.3f calibrationFactor=%.3f lifetimeHours=%.3f",
                    index, element.maxCurrentAmp, element.operatingVoltage, element.calibrationFactor, element.lifetimeHours);
                LOGINFO("GetFPDDeviceConfig[%u][Element]: bootSequenceOrder=%ld lastFailureEpoch=%ld retryCount=%ld lockToken=%ld",
                    index, element.bootSequenceOrder, element.lastFailureEpoch, element.retryCount, element.lockToken);
                LOGINFO("GetFPDDeviceConfig[%u][Element]: elementName=%s elementType=%s ownerModule=%s diagnostics=%s",
                    index,
                    element.elementName.c_str(),
                    element.elementType.c_str(),
                    element.ownerModule.c_str(),
                    element.diagnostics.c_str());

                const Exchange::IDeviceSettingsFPD::FPDElementColorConfigInfo& colorConfig = element.colorConfig;
                LOGINFO("GetFPDDeviceConfig[%u][ColorConfig]: colorMode=%u defaultColorId=%u maxColorSlots=%u transitionMode=%u",
                    index, colorConfig.colorMode, colorConfig.defaultColorId, colorConfig.maxColorSlots, colorConfig.transitionMode);
                LOGINFO("GetFPDDeviceConfig[%u][ColorConfig]: fadeInSec=%.3f fadeOutSec=%.3f blinkDutyCycle=%.3f brightnessScale=%.3f",
                    index, colorConfig.fadeInSec, colorConfig.fadeOutSec, colorConfig.blinkDutyCycle, colorConfig.brightnessScale);
                LOGINFO("GetFPDDeviceConfig[%u][ColorConfig]: transitionCurveA=%.3f transitionCurveB=%.3f maxPowerWatt=%.3f voltageDrop=%.3f",
                    index, colorConfig.transitionCurveA, colorConfig.transitionCurveB, colorConfig.maxPowerWatt, colorConfig.voltageDrop);
                LOGINFO("GetFPDDeviceConfig[%u][ColorConfig]: lastPersistEpoch=%ld configVersion=%ld checksum=%ld migrationId=%ld",
                    index, colorConfig.lastPersistEpoch, colorConfig.configVersion, colorConfig.checksum, colorConfig.migrationId);
                LOGINFO("GetFPDDeviceConfig[%u][ColorConfig]: modeName=%s fallbackPolicy=%s colorSpace=%s transitionProfile=%s",
                    index,
                    colorConfig.modeName.c_str(),
                    colorConfig.fallbackPolicy.c_str(),
                    colorConfig.colorSpace.c_str(),
                    colorConfig.transitionProfile.c_str());

                const Exchange::IDeviceSettingsFPD::FPDSupportedColorInfo& color = colorConfig.primaryColor;
                LOGINFO("GetFPDDeviceConfig[%u][Color]: colorId=%u paletteIndex=%u rgbPacked=%u hsvHue=%u",
                    index, color.colorId, color.paletteIndex, color.rgbPacked, color.hsvHue);
                LOGINFO("GetFPDDeviceConfig[%u][Color]: hue=%.3f saturation=%.3f value=%.3f alpha=%.3f",
                    index, color.hue, color.saturation, color.value, color.alpha);
                LOGINFO("GetFPDDeviceConfig[%u][Color]: cieX=%.6f cieY=%.6f cieZ=%.6f wavelengthNm=%.3f",
                    index, color.cieX, color.cieY, color.cieZ, color.wavelengthNm);
                LOGINFO("GetFPDDeviceConfig[%u][Color]: creationEpoch=%ld lastUpdateEpoch=%ld usageCounter=%ld reservedLong=%ld",
                    index, color.creationEpoch, color.lastUpdateEpoch, color.usageCounter, color.reservedLong);
                LOGINFO("GetFPDDeviceConfig[%u][Color]: colorName=%s colorFamily=%s vendorTag=%s metadata=%s",
                    index,
                    color.colorName.c_str(),
                    color.colorFamily.c_str(),
                    color.vendorTag.c_str(),
                    color.metadata.c_str());

                const Exchange::IDeviceSettingsFPD::FPDColorRangeInfo& range = color.range;
                LOGINFO("GetFPDDeviceConfig[%u][Range]: minValue=%u maxValue=%u stepSize=%u precision=%u",
                    index, range.minValue, range.maxValue, range.stepSize, range.precision);
                LOGINFO("GetFPDDeviceConfig[%u][Range]: gain=%.6f offset=%.6f gamma=%.6f noiseFloor=%.6f",
                    index, range.gain, range.offset, range.gamma, range.noiseFloor);
                LOGINFO("GetFPDDeviceConfig[%u][Range]: calibrationSlope=%.6f calibrationIntercept=%.6f referenceLux=%.6f thermalDrift=%.6f",
                    index, range.calibrationSlope, range.calibrationIntercept, range.referenceLux, range.thermalDrift);
                LOGINFO("GetFPDDeviceConfig[%u][Range]: sampleCount=%ld stableWindowMs=%ld warmupMs=%ld expiryEpoch=%ld",
                    index, range.sampleCount, range.stableWindowMs, range.warmupMs, range.expiryEpoch);
                LOGINFO("GetFPDDeviceConfig[%u][Range]: profileName=%s unit=%s revision=%s notes=%s",
                    index,
                    range.profileName.c_str(),
                    range.unit.c_str(),
                    range.revision.c_str(),
                    range.notes.c_str());

                index++;
            }

            LOGINFO("GetFPDDeviceConfig: total entries iterated=%u", index);
            deviceConfigIterator->Release();
        } else {
            LOGERR("GetFPDDeviceConfig failed or returned null iterator");
        }

        LOGINFO("========== Simplified FPD APIs Testing Completed ==========\\n");
    }

    void UserPlugin::TestSimplifiedHDMIInAPIs()
    {
        LOGINFO("========== Complete HDMI In APIs Testing Framework ==========");

        if (!_hdmiInManager) {
            LOGERR("HDMI In Manager interface is not available");
            return;
        }

        LOGINFO("Testing ALL HDMI In APIs with get-set-restore pattern");

        // Test ports - comprehensive testing on multiple ports
        DeviceSettingsHDMIIn::HDMIInPort testPorts[] = {
            DeviceSettingsHDMIIn::HDMIInPort::DS_HDMI_IN_PORT_0,
            DeviceSettingsHDMIIn::HDMIInPort::DS_HDMI_IN_PORT_1,
            DeviceSettingsHDMIIn::HDMIInPort::DS_HDMI_IN_PORT_2
        };

        // 1. Test GetHDMIInNumbefOfInputs
        int32_t inputCount = 0;
        Core::hresult result = _hdmiInManager->GetHDMIInNumbefOfInputs(inputCount);
        LOGINFO("GetHDMIInNumbefOfInputs: result=%u, count=%d", result, inputCount);

        // 2. Test GetHDMIInStatus
        DeviceSettingsHDMIIn::HDMIInStatus hdmiStatus;
        DeviceSettingsHDMIIn::IHDMIInPortConnectionStatusIterator* portStatus = nullptr;
        result = _hdmiInManager->GetHDMIInStatus(hdmiStatus, portStatus);
        LOGINFO("GetHDMIInStatus: result=%u, activePort=%d, isPresented=%s", 
               result, static_cast<int>(hdmiStatus.activePort), hdmiStatus.isPresented ? "true" : "false");
        if (portStatus) {
            portStatus->Release();
        }

        // 3. Test GetHDMIInAVLatency (global method)
        uint32_t videoLatency = 0, audioLatency = 0;
        result = _hdmiInManager->GetHDMIInAVLatency(videoLatency, audioLatency);
        LOGINFO("GetHDMIInAVLatency: result=%u, videoLatency=%u, audioLatency=%u", 
               result, videoLatency, audioLatency);

        // 4. Test GetHDMIVideoMode (global method)
        DeviceSettingsHDMIIn::HDMIVideoPortResolution videoMode;
        result = _hdmiInManager->GetHDMIVideoMode(videoMode);
        LOGINFO("GetHDMIVideoMode: result=%u, name='%s', pixelRes=%d, aspectRatio=%d, frameRate=%d", 
               result, videoMode.name.c_str(), static_cast<int>(videoMode.pixelResolution), 
               static_cast<int>(videoMode.aspectRatio), static_cast<int>(videoMode.frameRate));

        // 5. Test GetSupportedGameFeaturesList (global method)
        DeviceSettingsHDMIIn::IHDMIInGameFeatureListIterator* gameFeatures = nullptr;
        result = _hdmiInManager->GetSupportedGameFeaturesList(gameFeatures);
        LOGINFO("GetSupportedGameFeaturesList: result=%u", result);
        if (gameFeatures && result == Core::ERROR_NONE) {
            LOGINFO("Iterating through game features:");
            DeviceSettingsHDMIIn::HDMIInGameFeatureList currentFeature;
            uint32_t featureIndex = 0;
            bool hasMore = true;
            while (hasMore) {
                hasMore = gameFeatures->Next(currentFeature);
                if (hasMore) {
                    LOGINFO("  GameFeature[%u]: '%s'", featureIndex, currentFeature.gameFeature.c_str());
                    featureIndex++;
                }
            }
            LOGINFO("Total game features found: %u", featureIndex);
            gameFeatures->Release();
        } else if (gameFeatures) {
            gameFeatures->Release();
        }

        // Test each port with all port-specific APIs
        for (auto port : testPorts) {
            LOGINFO("---------- Testing ALL APIs for HDMI In Port: %d ----------", static_cast<int>(port));

            // 6. Test GetHDMIVersion (read-only per port)
            DeviceSettingsHDMIIn::HDMIInCapabilityVersion hdmiVersion;
            result = _hdmiInManager->GetHDMIVersion(port, hdmiVersion);
            LOGINFO("GetHDMIVersion: result=%u, port=%d, version=%d", result, static_cast<int>(port), static_cast<int>(hdmiVersion));

            // 7. Test GetHDMIEdidVersion and SetHDMIEdidVersion (get-set-restore)
            DeviceSettingsHDMIIn::HDMIInEdidVersion originalEdidVersion;
            result = _hdmiInManager->GetHDMIEdidVersion(port, originalEdidVersion);
            LOGINFO("GetHDMIEdidVersion: result=%u, port=%d, original_version=%d", result, static_cast<int>(port), static_cast<int>(originalEdidVersion));

            if (result == Core::ERROR_NONE) {
                // Set test EDID version (toggle between 1.4 and 2.0)
                DeviceSettingsHDMIIn::HDMIInEdidVersion testEdidVersion = (originalEdidVersion == DeviceSettingsHDMIIn::HDMIInEdidVersion::HDMI_EDID_VER_14) ? 
                    DeviceSettingsHDMIIn::HDMIInEdidVersion::HDMI_EDID_VER_20 : DeviceSettingsHDMIIn::HDMIInEdidVersion::HDMI_EDID_VER_14;
                result = _hdmiInManager->SetHDMIEdidVersion(port, testEdidVersion);
                LOGINFO("SetHDMIEdidVersion: result=%u, port=%d, test_version=%d", result, static_cast<int>(port), static_cast<int>(testEdidVersion));

                // Verify the version was set
                DeviceSettingsHDMIIn::HDMIInEdidVersion verifyEdidVersion;
                _hdmiInManager->GetHDMIEdidVersion(port, verifyEdidVersion);
                LOGINFO("GetHDMIEdidVersion: port=%d, verified_version=%d", static_cast<int>(port), static_cast<int>(verifyEdidVersion));

                // Restore original version
                _hdmiInManager->SetHDMIEdidVersion(port, originalEdidVersion);
                LOGINFO("SetHDMIEdidVersion: port=%d, version restored to %d", static_cast<int>(port), static_cast<int>(originalEdidVersion));
            }

            // 8. Test GetHDMIInEdid2AllmSupport and SetHDMIInEdid2AllmSupport (get-set-restore)
            bool originalAllmSupport = false;
            result = _hdmiInManager->GetHDMIInEdid2AllmSupport(port, originalAllmSupport);
            LOGINFO("GetHDMIInEdid2AllmSupport: result=%u, port=%d, original_support=%s", result, static_cast<int>(port), originalAllmSupport ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Set test ALLM support (toggle value)
                bool testAllmSupport = !originalAllmSupport;
                result = _hdmiInManager->SetHDMIInEdid2AllmSupport(port, testAllmSupport);
                LOGINFO("SetHDMIInEdid2AllmSupport: result=%u, port=%d, test_support=%s", result, static_cast<int>(port), testAllmSupport ? "true" : "false");

                // Verify the support was set
                bool verifyAllmSupport = false;
                _hdmiInManager->GetHDMIInEdid2AllmSupport(port, verifyAllmSupport);
                LOGINFO("GetHDMIInEdid2AllmSupport: port=%d, verified_support=%s", static_cast<int>(port), verifyAllmSupport ? "true" : "false");

                // Restore original support
                _hdmiInManager->SetHDMIInEdid2AllmSupport(port, originalAllmSupport);
                LOGINFO("SetHDMIInEdid2AllmSupport: port=%d, support restored to %s", static_cast<int>(port), originalAllmSupport ? "true" : "false");
            }

            // 9. Test GetVRRSupport and SetVRRSupport (get-set-restore)
            bool originalVrrSupport = false;
            result = _hdmiInManager->GetVRRSupport(port, originalVrrSupport);
            LOGINFO("GetVRRSupport: result=%u, port=%d, original_support=%s", result, static_cast<int>(port), originalVrrSupport ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Set test VRR support (toggle value)
                bool testVrrSupport = !originalVrrSupport;
                result = _hdmiInManager->SetVRRSupport(port, testVrrSupport);
                LOGINFO("SetVRRSupport: result=%u, port=%d, test_support=%s", result, static_cast<int>(port), testVrrSupport ? "true" : "false");

                // Verify the support was set
                bool verifyVrrSupport = false;
                _hdmiInManager->GetVRRSupport(port, verifyVrrSupport);
                LOGINFO("GetVRRSupport: port=%d, verified_support=%s", static_cast<int>(port), verifyVrrSupport ? "true" : "false");

                // Restore original support
                _hdmiInManager->SetVRRSupport(port, originalVrrSupport);
                LOGINFO("SetVRRSupport: port=%d, support restored to %s", static_cast<int>(port), originalVrrSupport ? "true" : "false");
            }

            // 10. Test GetVRRStatus (read-only)
            DeviceSettingsHDMIIn::HDMIInVRRStatus vrrStatus;
            result = _hdmiInManager->GetVRRStatus(port, vrrStatus);
            LOGINFO("GetVRRStatus: result=%u, port=%d, vrrType=%d, framerate=%.2f", 
                   result, static_cast<int>(port), static_cast<int>(vrrStatus.vrrType), vrrStatus.vrrFreeSyncFramerateHz);

            // 11. Test GetHDMIInAllmStatus (read-only)
            bool allmStatus = false;
            result = _hdmiInManager->GetHDMIInAllmStatus(port, allmStatus);
            LOGINFO("GetHDMIInAllmStatus: result=%u, port=%d, status=%s", result, static_cast<int>(port), allmStatus ? "true" : "false");

            // 12. Test GetEdidBytes (read-only byte array)
            uint8_t edidBytes[256] = {0};
            result = _hdmiInManager->GetEdidBytes(port, 256, edidBytes);
            LOGINFO("GetEdidBytes: result=%u, port=%d, bytes_length=256", result, static_cast<int>(port));
            if (result == Core::ERROR_NONE) {
                LOGINFO("EDID bytes retrieved successfully for Port %d", static_cast<int>(port));
                // Optionally log first few bytes for verification
                LOGINFO("First 8 EDID bytes: %02X %02X %02X %02X %02X %02X %02X %02X", 
                       edidBytes[0], edidBytes[1], edidBytes[2], edidBytes[3], 
                       edidBytes[4], edidBytes[5], edidBytes[6], edidBytes[7]);
            }

            // 13. Test GetHDMISPDInformation (read-only byte array)
            uint8_t spdBytes[256] = {0};
            result = _hdmiInManager->GetHDMISPDInformation(port, 256, spdBytes);
            LOGINFO("GetHDMISPDInformation: result=%u, port=%d, bytes_length=256", result, static_cast<int>(port));
            if (result == Core::ERROR_NONE) {
                LOGINFO("SPD information retrieved successfully for Port %d", static_cast<int>(port));
                // Optionally log first few bytes for verification
                LOGINFO("First 8 SPD bytes: %02X %02X %02X %02X %02X %02X %02X %02X", 
                       spdBytes[0], spdBytes[1], spdBytes[2], spdBytes[3], 
                       spdBytes[4], spdBytes[5], spdBytes[6], spdBytes[7]);
            }

            // 14. Test SelectHDMIInPort (get-set-restore with current status)
            DeviceSettingsHDMIIn::HDMIInStatus currentStatus;
            DeviceSettingsHDMIIn::IHDMIInPortConnectionStatusIterator* currentPortStatus = nullptr;
            _hdmiInManager->GetHDMIInStatus(currentStatus, currentPortStatus);
            DeviceSettingsHDMIIn::HDMIInPort originalActivePort = currentStatus.activePort;
            if (currentPortStatus) {
                currentPortStatus->Release();
            }

            if (port != originalActivePort) {
                result = _hdmiInManager->SelectHDMIInPort(port, true, true, DeviceSettingsHDMIIn::HDMIVideoPlaneType::DS_HDMIIN_VIDEOPLANE_PRIMARY);
                LOGINFO("SelectHDMIInPort: result=%u, port=%d, audioMix=true, topMost=true, plane=PRIMARY", result, static_cast<int>(port));

                // Give device 20 seconds to switch ports
                LOGINFO("Waiting 20 seconds for device to switch to port %d...", static_cast<int>(port));
                sleep(20);

                // Verify port selection
                DeviceSettingsHDMIIn::HDMIInStatus verifyStatus;
                DeviceSettingsHDMIIn::IHDMIInPortConnectionStatusIterator* verifyPortStatus = nullptr;
                _hdmiInManager->GetHDMIInStatus(verifyStatus, verifyPortStatus);
                LOGINFO("SelectHDMIInPort: verified_active_port=%d", static_cast<int>(verifyStatus.activePort));
                if (verifyPortStatus) {
                    verifyPortStatus->Release();
                }

                // Restore original active port
                _hdmiInManager->SelectHDMIInPort(originalActivePort, true, true, DeviceSettingsHDMIIn::HDMIVideoPlaneType::DS_HDMIIN_VIDEOPLANE_PRIMARY);
                LOGINFO("SelectHDMIInPort: port=%d, selection restored to %d", static_cast<int>(port), static_cast<int>(originalActivePort));

                // Give device 20 seconds to restore to original port
                LOGINFO("Waiting 20 seconds for device to restore to port %d...", static_cast<int>(originalActivePort));
                sleep(20);
            } else {
                LOGINFO("SelectHDMIInPort: port=%d is already active, skipping test", static_cast<int>(port));
            }

            // 15. Test ScaleHDMIInVideo (set-restore pattern)
            DeviceSettingsHDMIIn::HDMIInVideoRectangle testRect = {100, 100, 1280, 720};
            result = _hdmiInManager->ScaleHDMIInVideo(testRect);
            LOGINFO("ScaleHDMIInVideo: result=%u, port=%d, rect=(x=%d, y=%d, w=%d, h=%d)", 
                   result, static_cast<int>(port), testRect.x, testRect.y, testRect.width, testRect.height);

            // Restore to full screen (default)
            DeviceSettingsHDMIIn::HDMIInVideoRectangle defaultRect = {0, 0, 1920, 1080};
            result = _hdmiInManager->ScaleHDMIInVideo(defaultRect);
            LOGINFO("ScaleHDMIInVideo: port=%d, scaling restored to full screen", static_cast<int>(port));

            // 16. Test SelectHDMIZoomMode (set-restore pattern)
            result = _hdmiInManager->SelectHDMIZoomMode(DeviceSettingsHDMIIn::HDMIInVideoZoom::DS_HDMIIN_VIDEO_ZOOM_FULL);
            LOGINFO("SelectHDMIZoomMode: result=%u, port=%d, mode=FULL", result, static_cast<int>(port));

            // Restore to default zoom mode
            result = _hdmiInManager->SelectHDMIZoomMode(DeviceSettingsHDMIIn::HDMIInVideoZoom::DS_HDMIIN_VIDEO_ZOOM_NONE);
            LOGINFO("SelectHDMIZoomMode: port=%d, zoom mode restored to NONE", static_cast<int>(port));

            LOGINFO("---------- Completed ALL API testing for HDMI In Port: %d ----------", static_cast<int>(port));
        }

        LOGINFO("========== Complete HDMI In APIs Testing Completed ==========\\n");
    }

    void UserPlugin::TestSelectHDMIInPortAPI()
    {
        LOGINFO("========== Testing SelectHDMIInPort API for Port 2 ==========");

        if (!_hdmiInManager) {
            LOGERR("HDMI In Manager interface is not available");
            return;
        }

        // Get original active port
        DeviceSettingsHDMIIn::HDMIInStatus initialStatus;
        DeviceSettingsHDMIIn::IHDMIInPortConnectionStatusIterator* initialPortStatus = nullptr;
        Core::hresult result = _hdmiInManager->GetHDMIInStatus(initialStatus, initialPortStatus);
        DeviceSettingsHDMIIn::HDMIInPort originalActivePort = initialStatus.activePort;
        LOGINFO("Original active port: %d", static_cast<int>(originalActivePort));
        if (initialPortStatus) {
            initialPortStatus->Release();
        }

        // Test switching to port 2
        DeviceSettingsHDMIIn::HDMIInPort targetPort = DeviceSettingsHDMIIn::HDMIInPort::DS_HDMI_IN_PORT_2;
        
        if (targetPort != originalActivePort) {
            LOGINFO("Switching to HDMI In Port 2...");
            result = _hdmiInManager->SelectHDMIInPort(targetPort, true, true, DeviceSettingsHDMIIn::HDMIVideoPlaneType::DS_HDMIIN_VIDEOPLANE_PRIMARY);
            LOGINFO("SelectHDMIInPort result: %u", result);

            if (result == Core::ERROR_NONE) {
                LOGINFO("Waiting 10 seconds for port switch...");
                sleep(10);

                // Verify port selection
                DeviceSettingsHDMIIn::HDMIInStatus verifyStatus;
                DeviceSettingsHDMIIn::IHDMIInPortConnectionStatusIterator* verifyPortStatus = nullptr;
                _hdmiInManager->GetHDMIInStatus(verifyStatus, verifyPortStatus);
                LOGINFO("Current active port after switch: %d", static_cast<int>(verifyStatus.activePort));
                if (verifyPortStatus) {
                    verifyPortStatus->Release();
                }

                // Restore to original port
                LOGINFO("Restoring to original port %d...", static_cast<int>(originalActivePort));
                _hdmiInManager->SelectHDMIInPort(originalActivePort, true, true, DeviceSettingsHDMIIn::HDMIVideoPlaneType::DS_HDMIIN_VIDEOPLANE_PRIMARY);
                sleep(10);
                LOGINFO("Port restored successfully");
            } else {
                LOGERR("Failed to switch to port 2, result: %u", result);
            }
        } else {
            LOGINFO("Port 2 is already active, no switching needed");
        }

        LOGINFO("========== SelectHDMIInPort API Test Completed ==========");
    }

    // HDMI In Event Handler Implementations
    void UserPlugin::OnHDMIInEventHotPlug(const Exchange::IDeviceSettingsHDMIIn::HDMIInPort port, const bool isConnected)
    {
        LOGINFO("========== HDMI In Event: Hot Plug ==========");
        LOGINFO("OnHDMIInEventHotPlug: port=%d, isConnected=%s", static_cast<int>(port), isConnected ? "true" : "false");

        // Add your custom logic here
        if (isConnected) {
            LOGINFO("HDMI device connected on port %d", static_cast<int>(port));
        } else {
            LOGINFO("HDMI device disconnected from port %d", static_cast<int>(port));
        }
    }

    void UserPlugin::OnHDMIInEventSignalStatus(const Exchange::IDeviceSettingsHDMIIn::HDMIInPort port, const Exchange::IDeviceSettingsHDMIIn::HDMIInSignalStatus signalStatus)
    {
        LOGINFO("========== HDMI In Event: Signal Status ==========");
        LOGINFO("OnHDMIInEventSignalStatus: port=%d, signalStatus=%d", static_cast<int>(port), static_cast<int>(signalStatus));

        // Add your custom logic here - using generic logging since exact enum values may vary
        LOGINFO("Signal status changed to %d on port %d", static_cast<int>(signalStatus), static_cast<int>(port));

        // Add specific handling based on your interface definition
        if (static_cast<int>(signalStatus) == 0) {
            LOGINFO("No signal detected on port %d", static_cast<int>(port));
        } else if (static_cast<int>(signalStatus) == 1) {
            LOGINFO("Stable signal detected on port %d", static_cast<int>(port));
        } else if (static_cast<int>(signalStatus) == 2) {
            LOGINFO("Unstable signal detected on port %d", static_cast<int>(port));
        } else {
            LOGINFO("Unknown signal status %d on port %d", static_cast<int>(signalStatus), static_cast<int>(port));
        }
    }

    void UserPlugin::OnHDMIInEventStatus(const Exchange::IDeviceSettingsHDMIIn::HDMIInPort activePort, const bool isPresented)
    {
        LOGINFO("========== HDMI In Event: Status ==========");
        LOGINFO("OnHDMIInEventStatus: activePort=%d, isPresented=%s", static_cast<int>(activePort), isPresented ? "true" : "false");

        // Add your custom logic here
        if (isPresented) {
            LOGINFO("HDMI input is being presented on port %d", static_cast<int>(activePort));
        } else {
            LOGINFO("HDMI input is not being presented on port %d", static_cast<int>(activePort));
        }
    }

    void UserPlugin::OnHDMIInVideoModeUpdate(const Exchange::IDeviceSettingsHDMIIn::HDMIInPort port, const Exchange::IDeviceSettingsHDMIIn::HDMIVideoPortResolution videoPortResolution)
    {
        LOGINFO("========== HDMI In Event: Video Mode Update ==========");
        LOGINFO("OnHDMIInVideoModeUpdate: port=%d", static_cast<int>(port));
        LOGINFO("Video resolution: name='%s', pixelResolution=%d, aspectRatio=%d, stereoScopicMode=%d, frameRate=%d, interlaced=%s", 
                videoPortResolution.name.c_str(),
                static_cast<int>(videoPortResolution.pixelResolution),
                static_cast<int>(videoPortResolution.aspectRatio),
                static_cast<int>(videoPortResolution.stereoScopicMode),
                static_cast<int>(videoPortResolution.frameRate),
                videoPortResolution.interlaced ? "true" : "false");

        // Add your custom logic here
        LOGINFO("Video mode changed on port %d to %s", static_cast<int>(port), videoPortResolution.name.c_str());
    }

    void UserPlugin::OnHDMIInAllmStatus(const Exchange::IDeviceSettingsHDMIIn::HDMIInPort port, const bool allmStatus)
    {
        LOGINFO("========== HDMI In Event: ALLM Status ==========");
        LOGINFO("OnHDMIInAllmStatus: port=%d, allmStatus=%s", static_cast<int>(port), allmStatus ? "enabled" : "disabled");

        // Add your custom logic here
        if (allmStatus) {
            LOGINFO("Auto Low Latency Mode (ALLM) enabled on port %d", static_cast<int>(port));
        } else {
            LOGINFO("Auto Low Latency Mode (ALLM) disabled on port %d", static_cast<int>(port));
        }
    }

    void UserPlugin::OnHDMIInAVIContentType(const Exchange::IDeviceSettingsHDMIIn::HDMIInPort port, const Exchange::IDeviceSettingsHDMIIn::HDMIInAviContentType aviContentType)
    {
        LOGINFO("========== HDMI In Event: AVI Content Type ==========");
        LOGINFO("OnHDMIInAVIContentType: port=%d, aviContentType=%d", static_cast<int>(port), static_cast<int>(aviContentType));

        // Add your custom logic here - using generic logging since exact enum values may vary
        LOGINFO("AVI Content Type changed to %d on port %d", static_cast<int>(aviContentType), static_cast<int>(port));

        // Add specific handling based on your interface definition
        if (static_cast<int>(aviContentType) == 0) {
            LOGINFO("AVI Content Type: Graphics on port %d", static_cast<int>(port));
        } else if (static_cast<int>(aviContentType) == 1) {
            LOGINFO("AVI Content Type: Photo on port %d", static_cast<int>(port));
        } else if (static_cast<int>(aviContentType) == 2) {
            LOGINFO("AVI Content Type: Cinema on port %d", static_cast<int>(port));
        } else if (static_cast<int>(aviContentType) == 3) {
            LOGINFO("AVI Content Type: Game on port %d", static_cast<int>(port));
        } else {
            LOGINFO("AVI Content Type: Unknown (%d) on port %d", static_cast<int>(aviContentType), static_cast<int>(port));
        }
    }

    void UserPlugin::OnHDMIInAVLatency(const int32_t audioDelay, const int32_t videoDelay)
    {
        LOGINFO("========== HDMI In Event: AV Latency ==========");
        LOGINFO("OnHDMIInAVLatency: audioDelay=%d ms, videoDelay=%d ms", audioDelay, videoDelay);

        // Add your custom logic here
        LOGINFO("Audio/Video latency updated - Audio: %d ms, Video: %d ms", audioDelay, videoDelay);

        if (audioDelay != videoDelay) {
            LOGINFO("Audio/Video sync required - difference: %d ms", abs(audioDelay - videoDelay));
        } else {
            LOGINFO("Audio/Video are in sync");
        }
    }

    void UserPlugin::OnHDMIInVRRStatus(const Exchange::IDeviceSettingsHDMIIn::HDMIInPort port, const Exchange::IDeviceSettingsHDMIIn::HDMIInVRRType vrrType)
    {
        LOGINFO("========== HDMI In Event: VRR Status ==========");
        LOGINFO("OnHDMIInVRRStatus: port=%d, vrrType=%d", static_cast<int>(port), static_cast<int>(vrrType));

        // Add your custom logic here - using generic logging since exact enum values may vary
        LOGINFO("VRR Type changed to %d on port %d", static_cast<int>(vrrType), static_cast<int>(port));

        // Add specific handling based on your interface definition
        if (static_cast<int>(vrrType) == 0) {
            LOGINFO("VRR Type: None on port %d", static_cast<int>(port));
        } else if (static_cast<int>(vrrType) == 1) {
            LOGINFO("VRR Type: FreeSync on port %d", static_cast<int>(port));
        } else if (static_cast<int>(vrrType) == 2) {
            LOGINFO("VRR Type: VRR on port %d", static_cast<int>(port));
        } else {
            LOGINFO("VRR Type: Unknown (%d) on port %d", static_cast<int>(vrrType), static_cast<int>(port));
        }
    }

    void UserPlugin::TestAudioAPIs()
    {
        LOGINFO("========== Complete Audio APIs Testing Framework ==========");

        if (!_audioManager) {
            LOGERR("Audio Manager interface is not available");
            return;
        }

        LOGINFO("Testing ALL Audio APIs with get-set-restore pattern");

        // Test audio port types - comprehensive testing on multiple port types
        Exchange::IDeviceSettingsAudio::AudioPortType testPortTypes[] = {
            Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMI,
            Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_SPDIF,
            Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_SPEAKER,
            Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC
        };

        const char* portTypeNames[] = {
            "HDMI", "SPDIF", "SPEAKER", "HDMI_ARC"
        };

        // Test each port type with all port-specific APIs
        for (size_t i = 0; i < sizeof(testPortTypes)/sizeof(testPortTypes[0]); i++) {
            auto portType = testPortTypes[i];
            const char* portTypeName = portTypeNames[i];

            LOGINFO("---------- Testing ALL Audio APIs for Port Type: %s ----------", portTypeName);

            // 1. Test GetAudioPort - Get handle for this port type
            int32_t handle = -1;
            Core::hresult result = _audioManager->GetAudioPort(portType, 0, handle);
            LOGINFO("GetAudioPort: portType=%s, result=%u, handle=%d", portTypeName, result, handle);

            if (result != Core::ERROR_NONE) {
                LOGWARN("Skipping tests for %s port - port handle not available", portTypeName);
                continue;
            }

            // 2. Test IsAudioPortEnabled and EnableAudioPort (get-set-restore)
            bool originalEnabled = false;
            result = _audioManager->IsAudioPortEnabled(handle, originalEnabled);
            LOGINFO("IsAudioPortEnabled: handle=%d, result=%u, original_enabled=%s", handle, result, originalEnabled ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Toggle enable state for testing
                bool testEnabled = !originalEnabled;
                result = _audioManager->EnableAudioPort(handle, testEnabled);
                LOGINFO("EnableAudioPort: handle=%d, result=%u, test_enabled=%s", handle, result, testEnabled ? "true" : "false");

                // Verify the state was changed
                bool verifyEnabled = false;
                _audioManager->IsAudioPortEnabled(handle, verifyEnabled);
                LOGINFO("IsAudioPortEnabled: handle=%d, verified_enabled=%s", handle, verifyEnabled ? "true" : "false");

                // Restore original state
                _audioManager->EnableAudioPort(handle, originalEnabled);
                LOGINFO("EnableAudioPort: handle=%d, enabled restored to %s", handle, originalEnabled ? "true" : "false");
            }

            // 3. Test GetStereoMode and SetStereoMode (get-set-restore)
            Exchange::IDeviceSettingsAudio::StereoMode originalStereoMode;
            result = _audioManager->GetStereoMode(handle, originalStereoMode);
            LOGINFO("GetStereoMode: handle=%d, result=%u, original_mode=%d", handle, result, static_cast<int>(originalStereoMode));

            if (result == Core::ERROR_NONE) {
                // Set test stereo mode (toggle between stereo and surround)
                Exchange::IDeviceSettingsAudio::StereoMode testStereoMode = 
                    (originalStereoMode == Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_STEREO) ? 
                    Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_SURROUND : 
                    Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_STEREO;
                result = _audioManager->SetStereoMode(handle, testStereoMode, false);
                LOGINFO("SetStereoMode: handle=%d, result=%u, test_mode=%d", handle, result, static_cast<int>(testStereoMode));

                // Verify the mode was set
                Exchange::IDeviceSettingsAudio::StereoMode verifyStereoMode;
                _audioManager->GetStereoMode(handle, verifyStereoMode);
                LOGINFO("GetStereoMode: handle=%d, verified_mode=%d", handle, static_cast<int>(verifyStereoMode));

                // Restore original mode
                _audioManager->SetStereoMode(handle, originalStereoMode, false);
                LOGINFO("SetStereoMode: handle=%d, mode restored to %d", handle, static_cast<int>(originalStereoMode));
            }

            // 4. Test GetStereoAuto and SetStereoAuto (get-set-restore)
            int32_t originalStereoAuto = 0;
            result = _audioManager->GetStereoAuto(handle, originalStereoAuto);
            LOGINFO("GetStereoAuto: handle=%d, result=%u, original_auto=%d", handle, result, originalStereoAuto);

            if (result == Core::ERROR_NONE) {
                // Toggle stereo auto value
                int32_t testStereoAuto = (originalStereoAuto == 0) ? 1 : 0;
                result = _audioManager->SetStereoAuto(handle, testStereoAuto, false);
                LOGINFO("SetStereoAuto: handle=%d, result=%u, test_auto=%d", handle, result, testStereoAuto);

                // Verify the auto setting was changed
                int32_t verifyStereoAuto = 0;
                _audioManager->GetStereoAuto(handle, verifyStereoAuto);
                LOGINFO("GetStereoAuto: handle=%d, verified_auto=%d", handle, verifyStereoAuto);

                // Restore original setting
                _audioManager->SetStereoAuto(handle, originalStereoAuto, false);
                LOGINFO("SetStereoAuto: handle=%d, auto restored to %d", handle, originalStereoAuto);
            }

            // 5. Test IsAudioMuted and SetAudioMute (get-set-restore)
            bool originalMuted = false;
            result = _audioManager->IsAudioMuted(handle, originalMuted);
            LOGINFO("IsAudioMuted: handle=%d, result=%u, original_muted=%s", handle, result, originalMuted ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Toggle mute state
                bool testMuted = !originalMuted;
                result = _audioManager->SetAudioMute(handle, testMuted);
                LOGINFO("SetAudioMute: handle=%d, result=%u, test_muted=%s", handle, result, testMuted ? "true" : "false");

                // Verify mute state was changed
                bool verifyMuted = false;
                _audioManager->IsAudioMuted(handle, verifyMuted);
                LOGINFO("IsAudioMuted: handle=%d, verified_muted=%s", handle, verifyMuted ? "true" : "false");

                // Restore original mute state
                _audioManager->SetAudioMute(handle, originalMuted);
                LOGINFO("SetAudioMute: handle=%d, muted restored to %s", handle, originalMuted ? "true" : "false");
            }

            // 6. Test GetAudioLevel and SetAudioLevel (get-set-restore)
            float originalAudioLevel = 0.0f;
            result = _audioManager->GetAudioLevel(handle, originalAudioLevel);
            LOGINFO("GetAudioLevel: handle=%d, result=%u, original_level=%.2f", handle, result, originalAudioLevel);

            if (result == Core::ERROR_NONE) {
                // Set test audio level (use different level for testing)
                float testAudioLevel = (originalAudioLevel == 50.0f) ? 75.0f : 50.0f;
                result = _audioManager->SetAudioLevel(handle, testAudioLevel);
                LOGINFO("SetAudioLevel: handle=%d, result=%u, test_level=%.2f", handle, result, testAudioLevel);

                // Verify audio level was set
                float verifyAudioLevel = 0.0f;
                _audioManager->GetAudioLevel(handle, verifyAudioLevel);
                LOGINFO("GetAudioLevel: handle=%d, verified_level=%.2f", handle, verifyAudioLevel);

                // Restore original audio level
                _audioManager->SetAudioLevel(handle, originalAudioLevel);
                LOGINFO("SetAudioLevel: handle=%d, level restored to %.2f", handle, originalAudioLevel);
            }

            // 7. Test GetAudioGain and SetAudioGain (get-set-restore)
            float originalAudioGain = 0.0f;
            result = _audioManager->GetAudioGain(handle, originalAudioGain);
            LOGINFO("GetAudioGain: handle=%d, result=%u, original_gain=%.2f", handle, result, originalAudioGain);

            if (result == Core::ERROR_NONE) {
                // Set test audio gain (use different gain for testing)
                float testAudioGain = (originalAudioGain == 0.0f) ? 2.5f : 0.0f;
                result = _audioManager->SetAudioGain(handle, testAudioGain);
                LOGINFO("SetAudioGain: handle=%d, result=%u, test_gain=%.2f", handle, result, testAudioGain);

                // Verify audio gain was set
                float verifyAudioGain = 0.0f;
                _audioManager->GetAudioGain(handle, verifyAudioGain);
                LOGINFO("GetAudioGain: handle=%d, verified_gain=%.2f", handle, verifyAudioGain);

                // Restore original audio gain
                _audioManager->SetAudioGain(handle, originalAudioGain);
                LOGINFO("SetAudioGain: handle=%d, gain restored to %.2f", handle, originalAudioGain);
            }

            // 8. Test GetAudioDelay and SetAudioDelay (get-set-restore)
            uint32_t originalAudioDelay = 0;
            result = _audioManager->GetAudioDelay(handle, originalAudioDelay);
            LOGINFO("GetAudioDelay: handle=%d, result=%u, original_delay=%u ms", handle, result, originalAudioDelay);

            if (result == Core::ERROR_NONE) {
                // Set test audio delay (use different delay for testing)
                uint32_t testAudioDelay = (originalAudioDelay == 0) ? 100 : 0;
                result = _audioManager->SetAudioDelay(handle, testAudioDelay);
                LOGINFO("SetAudioDelay: handle=%d, result=%u, test_delay=%u ms", handle, result, testAudioDelay);

                // Verify audio delay was set
                uint32_t verifyAudioDelay = 0;
                _audioManager->GetAudioDelay(handle, verifyAudioDelay);
                LOGINFO("GetAudioDelay: handle=%d, verified_delay=%u ms", handle, verifyAudioDelay);

                // Restore original audio delay
                _audioManager->SetAudioDelay(handle, originalAudioDelay);
                LOGINFO("SetAudioDelay: handle=%d, delay restored to %u ms", handle, originalAudioDelay);
            }

            // 9. Test GetAudioDelayOffset and SetAudioDelayOffset (get-set-restore)
            uint32_t originalDelayOffset = 0;
            result = _audioManager->GetAudioDelayOffset(handle, originalDelayOffset);
            LOGINFO("GetAudioDelayOffset: handle=%d, result=%u, original_offset=%u ms", handle, result, originalDelayOffset);

            if (result == Core::ERROR_NONE) {
                // Set test delay offset
                uint32_t testDelayOffset = (originalDelayOffset == 0) ? 50 : 0;
                result = _audioManager->SetAudioDelayOffset(handle, testDelayOffset);
                LOGINFO("SetAudioDelayOffset: handle=%d, result=%u, test_offset=%u ms", handle, result, testDelayOffset);

                // Verify delay offset was set
                uint32_t verifyDelayOffset = 0;
                _audioManager->GetAudioDelayOffset(handle, verifyDelayOffset);
                LOGINFO("GetAudioDelayOffset: handle=%d, verified_offset=%u ms", handle, verifyDelayOffset);

                // Restore original delay offset
                _audioManager->SetAudioDelayOffset(handle, originalDelayOffset);
                LOGINFO("SetAudioDelayOffset: handle=%d, offset restored to %u ms", handle, originalDelayOffset);
            }

            // 10. Test GetAudioFormat (read-only)
            Exchange::IDeviceSettingsAudio::AudioFormat audioFormat;
            result = _audioManager->GetAudioFormat(handle, audioFormat);
            LOGINFO("GetAudioFormat: handle=%d, result=%u, format=%d", handle, result, static_cast<int>(audioFormat));

            // 11. Test GetAudioEncoding (read-only)
            Exchange::IDeviceSettingsAudio::AudioEncoding audioEncoding;
            result = _audioManager->GetAudioEncoding(handle, audioEncoding);
            LOGINFO("GetAudioEncoding: handle=%d, result=%u, encoding=%d", handle, result, static_cast<int>(audioEncoding));

            // 12. Test GetAudioEnablePersist and SetAudioEnablePersist (get-set-restore)
            bool originalPersistEnabled = false;
            string originalPortName;
            result = _audioManager->GetAudioEnablePersist(handle, originalPersistEnabled, originalPortName);
            LOGINFO("GetAudioEnablePersist: handle=%d, result=%u, original_enabled=%s, port_name='%s'", 
                   handle, result, originalPersistEnabled ? "true" : "false", originalPortName.c_str());

            if (result == Core::ERROR_NONE) {
                // Toggle persist enabled state
                bool testPersistEnabled = !originalPersistEnabled;
                string testPortName = portTypeName;
                result = _audioManager->SetAudioEnablePersist(handle, testPersistEnabled, testPortName);
                LOGINFO("SetAudioEnablePersist: handle=%d, result=%u, test_enabled=%s, test_port_name='%s'", 
                       handle, result, testPersistEnabled ? "true" : "false", testPortName.c_str());

                // Verify persist setting was changed
                bool verifyPersistEnabled = false;
                string verifyPortName;
                _audioManager->GetAudioEnablePersist(handle, verifyPersistEnabled, verifyPortName);
                LOGINFO("GetAudioEnablePersist: handle=%d, verified_enabled=%s, verified_port_name='%s'", 
                       handle, verifyPersistEnabled ? "true" : "false", verifyPortName.c_str());

                // Restore original persist setting
                _audioManager->SetAudioEnablePersist(handle, originalPersistEnabled, originalPortName);
                LOGINFO("SetAudioEnablePersist: handle=%d, persist restored to %s", handle, originalPersistEnabled ? "true" : "false");
            }

            // 13. Test IsAudioMSDecoded (read-only)
            bool hasms11Decode = false;
            result = _audioManager->IsAudioMSDecoded(handle, hasms11Decode);
            LOGINFO("IsAudioMSDecoded: handle=%d, result=%u, hasMS11Decode=%s", handle, result, hasms11Decode ? "true" : "false");

            // 14. Test IsAudioMS12Decoded (read-only)
            bool hasms12Decode = false;
            result = _audioManager->IsAudioMS12Decoded(handle, hasms12Decode);
            LOGINFO("IsAudioMS12Decoded: handle=%d, result=%u, hasMS12Decode=%s", handle, result, hasms12Decode ? "true" : "false");

            // 15. Test GetAudioLEConfig and EnableAudioLEConfig (get-set-restore)
            bool originalLEEnabled = false;
            result = _audioManager->GetAudioLEConfig(handle, originalLEEnabled);
            LOGINFO("GetAudioLEConfig: handle=%d, result=%u, original_LE_enabled=%s", handle, result, originalLEEnabled ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Toggle LE config state
                bool testLEEnabled = !originalLEEnabled;
                result = _audioManager->EnableAudioLEConfig(handle, testLEEnabled);
                LOGINFO("EnableAudioLEConfig: handle=%d, result=%u, test_LE_enabled=%s", handle, result, testLEEnabled ? "true" : "false");

                // Verify LE config was changed
                bool verifyLEEnabled = false;
                _audioManager->GetAudioLEConfig(handle, verifyLEEnabled);
                LOGINFO("GetAudioLEConfig: handle=%d, verified_LE_enabled=%s", handle, verifyLEEnabled ? "true" : "false");

                // Restore original LE config
                _audioManager->EnableAudioLEConfig(handle, originalLEEnabled);
                LOGINFO("EnableAudioLEConfig: handle=%d, LE enabled restored to %s", handle, originalLEEnabled ? "true" : "false");
            }

            // 16. Test GetAudioSinkDeviceAtmosCapability (read-only)
            Exchange::IDeviceSettingsAudio::DolbyAtmosCapability atmosCapability;
            result = _audioManager->GetAudioSinkDeviceAtmosCapability(handle, atmosCapability);
            LOGINFO("GetAudioSinkDeviceAtmosCapability: handle=%d, result=%u, atmos_capability=%d", 
                   handle, result, static_cast<int>(atmosCapability));

            // 17. Test SetAudioAtmosOutputMode (set-restore pattern)
            result = _audioManager->SetAudioAtmosOutputMode(handle, true);
            LOGINFO("SetAudioAtmosOutputMode: handle=%d, result=%u, enable=true", handle, result);

            // Restore to disabled state
            result = _audioManager->SetAudioAtmosOutputMode(handle, false);
            LOGINFO("SetAudioAtmosOutputMode: handle=%d, atmos output restored to disabled", handle);

            // 18. Test SetAudioDucking (test different ducking actions)
            result = _audioManager->SetAudioDucking(handle, 
                Exchange::IDeviceSettingsAudio::AudioDuckingType::AUDIO_DUCKINGTYPE_ABSOLUTE,
                Exchange::IDeviceSettingsAudio::AudioDuckingAction::AUDIO_DUCKINGACTION_START, 70);
            LOGINFO("SetAudioDucking: handle=%d, result=%u, type=ABSOLUTE, action=START, level=70", handle, result);

            // Stop ducking to restore normal audio
            result = _audioManager->SetAudioDucking(handle, 
                Exchange::IDeviceSettingsAudio::AudioDuckingType::AUDIO_DUCKINGTYPE_ABSOLUTE,
                Exchange::IDeviceSettingsAudio::AudioDuckingAction::AUDIO_DUCKINGACTION_STOP, 0);
            LOGINFO("SetAudioDucking: handle=%d, ducking stopped to restore normal audio", handle);

            // 19. Test GetAudioCompression and SetAudioCompression (get-set-restore)
            int32_t originalCompression = 0;
            result = _audioManager->GetAudioCompression(handle, originalCompression);
            LOGINFO("GetAudioCompression: handle=%d, result=%u, original_compression=%d", handle, result, originalCompression);

            if (result == Core::ERROR_NONE) {
                // Set test compression level
                int32_t testCompression = (originalCompression == 0) ? 2 : 0;
                result = _audioManager->SetAudioCompression(handle, testCompression);
                LOGINFO("SetAudioCompression: handle=%d, result=%u, test_compression=%d", handle, result, testCompression);

                // Verify compression was set
                int32_t verifyCompression = 0;
                _audioManager->GetAudioCompression(handle, verifyCompression);
                LOGINFO("GetAudioCompression: handle=%d, verified_compression=%d", handle, verifyCompression);

                // Restore original compression
                _audioManager->SetAudioCompression(handle, originalCompression);
                LOGINFO("SetAudioCompression: handle=%d, compression restored to %d", handle, originalCompression);
            }

            // 20. Test GetAudioDialogEnhancement and SetAudioDialogEnhancement (get-set-restore)
            int32_t originalDialogLevel = 0;
            result = _audioManager->GetAudioDialogEnhancement(handle, originalDialogLevel);
            LOGINFO("GetAudioDialogEnhancement: handle=%d, result=%u, original_level=%d", handle, result, originalDialogLevel);

            if (result == Core::ERROR_NONE) {
                // Set test dialog enhancement level
                int32_t testDialogLevel = (originalDialogLevel == 0) ? 5 : 0;
                result = _audioManager->SetAudioDialogEnhancement(handle, testDialogLevel);
                LOGINFO("SetAudioDialogEnhancement: handle=%d, result=%u, test_level=%d", handle, result, testDialogLevel);

                // Verify dialog enhancement was set
                int32_t verifyDialogLevel = 0;
                _audioManager->GetAudioDialogEnhancement(handle, verifyDialogLevel);
                LOGINFO("GetAudioDialogEnhancement: handle=%d, verified_level=%d", handle, verifyDialogLevel);

                // Restore original dialog level
                _audioManager->SetAudioDialogEnhancement(handle, originalDialogLevel);
                LOGINFO("SetAudioDialogEnhancement: handle=%d, dialog level restored to %d", handle, originalDialogLevel);
            }

            // 21. Test GetAudioDolbyVolumeMode and SetAudioDolbyVolumeMode (get-set-restore)
            bool originalDolbyVolume = false;
            result = _audioManager->GetAudioDolbyVolumeMode(handle, originalDolbyVolume);
            LOGINFO("GetAudioDolbyVolumeMode: handle=%d, result=%u, original_enabled=%s", handle, result, originalDolbyVolume ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Toggle dolby volume mode
                bool testDolbyVolume = !originalDolbyVolume;
                result = _audioManager->SetAudioDolbyVolumeMode(handle, testDolbyVolume);
                LOGINFO("SetAudioDolbyVolumeMode: handle=%d, result=%u, test_enabled=%s", handle, result, testDolbyVolume ? "true" : "false");

                // Verify dolby volume mode was set
                bool verifyDolbyVolume = false;
                _audioManager->GetAudioDolbyVolumeMode(handle, verifyDolbyVolume);
                LOGINFO("GetAudioDolbyVolumeMode: handle=%d, verified_enabled=%s", handle, verifyDolbyVolume ? "true" : "false");

                // Restore original dolby volume mode
                _audioManager->SetAudioDolbyVolumeMode(handle, originalDolbyVolume);
                LOGINFO("SetAudioDolbyVolumeMode: handle=%d, dolby volume restored to %s", handle, originalDolbyVolume ? "true" : "false");
            }

            // 22. Test GetAudioIntelligentEqualizerMode and SetAudioIntelligentEqualizerMode (get-set-restore)
            int32_t originalEqualizerMode = 0;
            result = _audioManager->GetAudioIntelligentEqualizerMode(handle, originalEqualizerMode);
            LOGINFO("GetAudioIntelligentEqualizerMode: handle=%d, result=%u, original_mode=%d", handle, result, originalEqualizerMode);

            if (result == Core::ERROR_NONE) {
                // Set test equalizer mode
                int32_t testEqualizerMode = (originalEqualizerMode == 0) ? 1 : 0;
                result = _audioManager->SetAudioIntelligentEqualizerMode(handle, testEqualizerMode);
                LOGINFO("SetAudioIntelligentEqualizerMode: handle=%d, result=%u, test_mode=%d", handle, result, testEqualizerMode);

                // Verify equalizer mode was set
                int32_t verifyEqualizerMode = 0;
                _audioManager->GetAudioIntelligentEqualizerMode(handle, verifyEqualizerMode);
                LOGINFO("GetAudioIntelligentEqualizerMode: handle=%d, verified_mode=%d", handle, verifyEqualizerMode);

                // Restore original equalizer mode
                _audioManager->SetAudioIntelligentEqualizerMode(handle, originalEqualizerMode);
                LOGINFO("SetAudioIntelligentEqualizerMode: handle=%d, equalizer mode restored to %d", handle, originalEqualizerMode);
            }

            // 23. Test GetAudioVolumeLeveller and SetAudioVolumeLeveller (get-set-restore)
            Exchange::IDeviceSettingsAudio::VolumeLeveller originalVolumeLeveller;
            result = _audioManager->GetAudioVolumeLeveller(handle, originalVolumeLeveller);
            LOGINFO("GetAudioVolumeLeveller: handle=%d, result=%u, original_mode=%d, original_level=%d", 
                   handle, result, originalVolumeLeveller.mode, originalVolumeLeveller.level);

            if (result == Core::ERROR_NONE) {
                // Set test volume leveller
                Exchange::IDeviceSettingsAudio::VolumeLeveller testVolumeLeveller;
                testVolumeLeveller.mode = (originalVolumeLeveller.mode == 0) ? 1 : 0;
                testVolumeLeveller.level = (originalVolumeLeveller.level == 5) ? 7 : 5;
                result = _audioManager->SetAudioVolumeLeveller(handle, testVolumeLeveller);
                LOGINFO("SetAudioVolumeLeveller: handle=%d, result=%u, test_mode=%d, test_level=%d", 
                       handle, result, testVolumeLeveller.mode, testVolumeLeveller.level);

                // Verify volume leveller was set
                Exchange::IDeviceSettingsAudio::VolumeLeveller verifyVolumeLeveller;
                _audioManager->GetAudioVolumeLeveller(handle, verifyVolumeLeveller);
                LOGINFO("GetAudioVolumeLeveller: handle=%d, verified_mode=%d, verified_level=%d", 
                       handle, verifyVolumeLeveller.mode, verifyVolumeLeveller.level);

                // Restore original volume leveller
                _audioManager->SetAudioVolumeLeveller(handle, originalVolumeLeveller);
                LOGINFO("SetAudioVolumeLeveller: handle=%d, volume leveller restored", handle);
            }

            // 24. Test GetAudioBassEnhancer and SetAudioBassEnhancer (get-set-restore)
            int32_t originalBassBoost = 0;
            result = _audioManager->GetAudioBassEnhancer(handle, originalBassBoost);
            LOGINFO("GetAudioBassEnhancer: handle=%d, result=%u, original_boost=%d", handle, result, originalBassBoost);

            if (result == Core::ERROR_NONE) {
                // Set test bass boost
                int32_t testBassBoost = (originalBassBoost == 0) ? 3 : 0;
                result = _audioManager->SetAudioBassEnhancer(handle, testBassBoost);
                LOGINFO("SetAudioBassEnhancer: handle=%d, result=%u, test_boost=%d", handle, result, testBassBoost);

                // Verify bass boost was set
                int32_t verifyBassBoost = 0;
                _audioManager->GetAudioBassEnhancer(handle, verifyBassBoost);
                LOGINFO("GetAudioBassEnhancer: handle=%d, verified_boost=%d", handle, verifyBassBoost);

                // Restore original bass boost
                _audioManager->SetAudioBassEnhancer(handle, originalBassBoost);
                LOGINFO("SetAudioBassEnhancer: handle=%d, bass boost restored to %d", handle, originalBassBoost);
            }

            // 25. Test IsAudioSurroudDecoderEnabled and EnableAudioSurroudDecoder (get-set-restore)
            bool originalSurroundDecoder = false;
            result = _audioManager->IsAudioSurroudDecoderEnabled(handle, originalSurroundDecoder);
            LOGINFO("IsAudioSurroudDecoderEnabled: handle=%d, result=%u, original_enabled=%s", handle, result, originalSurroundDecoder ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Toggle surround decoder state
                bool testSurroundDecoder = !originalSurroundDecoder;
                result = _audioManager->EnableAudioSurroudDecoder(handle, testSurroundDecoder);
                LOGINFO("EnableAudioSurroudDecoder: handle=%d, result=%u, test_enabled=%s", handle, result, testSurroundDecoder ? "true" : "false");

                // Verify surround decoder was set
                bool verifySurroundDecoder = false;
                _audioManager->IsAudioSurroudDecoderEnabled(handle, verifySurroundDecoder);
                LOGINFO("IsAudioSurroudDecoderEnabled: handle=%d, verified_enabled=%s", handle, verifySurroundDecoder ? "true" : "false");

                // Restore original surround decoder
                _audioManager->EnableAudioSurroudDecoder(handle, originalSurroundDecoder);
                LOGINFO("EnableAudioSurroudDecoder: handle=%d, surround decoder restored to %s", handle, originalSurroundDecoder ? "true" : "false");
            }

            // 26. Test GetAudioDRCMode and SetAudioDRCMode (get-set-restore)
            int32_t originalDRCMode = 0;
            result = _audioManager->GetAudioDRCMode(handle, originalDRCMode);
            LOGINFO("GetAudioDRCMode: handle=%d, result=%u, original_drc_mode=%d", handle, result, originalDRCMode);

            if (result == Core::ERROR_NONE) {
                // Set test DRC mode
                int32_t testDRCMode = (originalDRCMode == 0) ? 1 : 0;
                result = _audioManager->SetAudioDRCMode(handle, testDRCMode);
                LOGINFO("SetAudioDRCMode: handle=%d, result=%u, test_drc_mode=%d", handle, result, testDRCMode);

                // Verify DRC mode was set
                int32_t verifyDRCMode = 0;
                _audioManager->GetAudioDRCMode(handle, verifyDRCMode);
                LOGINFO("GetAudioDRCMode: handle=%d, verified_drc_mode=%d", handle, verifyDRCMode);

                // Restore original DRC mode
                _audioManager->SetAudioDRCMode(handle, originalDRCMode);
                LOGINFO("SetAudioDRCMode: handle=%d, DRC mode restored to %d", handle, originalDRCMode);
            }

            // 27. Test GetAudioSurroudVirtualizer and SetAudioSurroudVirtualizer (get-set-restore)
            Exchange::IDeviceSettingsAudio::SurroundVirtualizer originalVirtualizer;
            result = _audioManager->GetAudioSurroudVirtualizer(handle, originalVirtualizer);
            LOGINFO("GetAudioSurroudVirtualizer: handle=%d, result=%u, original_mode=%d, original_boost=%d", 
                   handle, result, originalVirtualizer.mode, originalVirtualizer.boost);

            if (result == Core::ERROR_NONE) {
                // Set test virtualizer
                Exchange::IDeviceSettingsAudio::SurroundVirtualizer testVirtualizer;
                testVirtualizer.mode = (originalVirtualizer.mode == 0) ? 1 : 0;
                testVirtualizer.boost = (originalVirtualizer.boost == 50) ? 75 : 50;
                result = _audioManager->SetAudioSurroudVirtualizer(handle, testVirtualizer);
                LOGINFO("SetAudioSurroudVirtualizer: handle=%d, result=%u, test_mode=%d, test_boost=%d", 
                       handle, result, testVirtualizer.mode, testVirtualizer.boost);

                // Verify virtualizer was set
                Exchange::IDeviceSettingsAudio::SurroundVirtualizer verifyVirtualizer;
                _audioManager->GetAudioSurroudVirtualizer(handle, verifyVirtualizer);
                LOGINFO("GetAudioSurroudVirtualizer: handle=%d, verified_mode=%d, verified_boost=%d", 
                       handle, verifyVirtualizer.mode, verifyVirtualizer.boost);

                // Restore original virtualizer
                _audioManager->SetAudioSurroudVirtualizer(handle, originalVirtualizer);
                LOGINFO("SetAudioSurroudVirtualizer: handle=%d, virtualizer restored", handle);
            }

            // 28. Test GetAudioMISteering and SetAudioMISteering (get-set-restore)
            bool originalMISteering = false;
            result = _audioManager->GetAudioMISteering(handle, originalMISteering);
            LOGINFO("GetAudioMISteering: handle=%d, result=%u, original_enabled=%s", handle, result, originalMISteering ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Toggle MI steering
                bool testMISteering = !originalMISteering;
                result = _audioManager->SetAudioMISteering(handle, testMISteering);
                LOGINFO("SetAudioMISteering: handle=%d, result=%u, test_enabled=%s", handle, result, testMISteering ? "true" : "false");

                // Verify MI steering was set
                bool verifyMISteering = false;
                _audioManager->GetAudioMISteering(handle, verifyMISteering);
                LOGINFO("GetAudioMISteering: handle=%d, verified_enabled=%s", handle, verifyMISteering ? "true" : "false");

                // Restore original MI steering
                _audioManager->SetAudioMISteering(handle, originalMISteering);
                LOGINFO("SetAudioMISteering: handle=%d, MI steering restored to %s", handle, originalMISteering ? "true" : "false");
            }

            // 29. Test GetAudioGraphicEqualizerMode and SetAudioGraphicEqualizerMode (get-set-restore)
            int32_t originalGraphicEQMode = 0;
            result = _audioManager->GetAudioGraphicEqualizerMode(handle, originalGraphicEQMode);
            LOGINFO("GetAudioGraphicEqualizerMode: handle=%d, result=%u, original_mode=%d", handle, result, originalGraphicEQMode);

            if (result == Core::ERROR_NONE) {
                // Set test graphic equalizer mode
                int32_t testGraphicEQMode = (originalGraphicEQMode == 0) ? 1 : 0;
                result = _audioManager->SetAudioGraphicEqualizerMode(handle, testGraphicEQMode);
                LOGINFO("SetAudioGraphicEqualizerMode: handle=%d, result=%u, test_mode=%d", handle, result, testGraphicEQMode);

                // Verify graphic equalizer mode was set
                int32_t verifyGraphicEQMode = 0;
                _audioManager->GetAudioGraphicEqualizerMode(handle, verifyGraphicEQMode);
                LOGINFO("GetAudioGraphicEqualizerMode: handle=%d, verified_mode=%d", handle, verifyGraphicEQMode);

                // Restore original graphic equalizer mode
                _audioManager->SetAudioGraphicEqualizerMode(handle, originalGraphicEQMode);
                LOGINFO("SetAudioGraphicEqualizerMode: handle=%d, graphic EQ mode restored to %d", handle, originalGraphicEQMode);
            }

            // 30. Test GetAssociatedAudioMixing and SetAssociatedAudioMixing (get-set-restore)
            bool originalAudioMixing = false;
            result = _audioManager->GetAssociatedAudioMixing(handle, originalAudioMixing);
            LOGINFO("GetAssociatedAudioMixing: handle=%d, result=%u, original_mixing=%s", handle, result, originalAudioMixing ? "true" : "false");

            if (result == Core::ERROR_NONE) {
                // Toggle associated audio mixing
                bool testAudioMixing = !originalAudioMixing;
                result = _audioManager->SetAssociatedAudioMixing(handle, testAudioMixing);
                LOGINFO("SetAssociatedAudioMixing: handle=%d, result=%u, test_mixing=%s", handle, result, testAudioMixing ? "true" : "false");

                // Verify associated audio mixing was set
                bool verifyAudioMixing = false;
                _audioManager->GetAssociatedAudioMixing(handle, verifyAudioMixing);
                LOGINFO("GetAssociatedAudioMixing: handle=%d, verified_mixing=%s", handle, verifyAudioMixing ? "true" : "false");

                // Restore original associated audio mixing
                _audioManager->SetAssociatedAudioMixing(handle, originalAudioMixing);
                LOGINFO("SetAssociatedAudioMixing: handle=%d, audio mixing restored to %s", handle, originalAudioMixing ? "true" : "false");
            }

            // 31. Test GetAudioFaderControl and SetAudioFaderControl (get-set-restore)
            int32_t originalFaderBalance = 0;
            result = _audioManager->GetAudioFaderControl(handle, originalFaderBalance);
            LOGINFO("GetAudioFaderControl: handle=%d, result=%u, original_balance=%d", handle, result, originalFaderBalance);

            if (result == Core::ERROR_NONE) {
                // Set test fader balance
                int32_t testFaderBalance = (originalFaderBalance == 0) ? 10 : 0;
                result = _audioManager->SetAudioFaderControl(handle, testFaderBalance);
                LOGINFO("SetAudioFaderControl: handle=%d, result=%u, test_balance=%d", handle, result, testFaderBalance);

                // Verify fader balance was set
                int32_t verifyFaderBalance = 0;
                _audioManager->GetAudioFaderControl(handle, verifyFaderBalance);
                LOGINFO("GetAudioFaderControl: handle=%d, verified_balance=%d", handle, verifyFaderBalance);

                // Restore original fader balance
                _audioManager->SetAudioFaderControl(handle, originalFaderBalance);
                LOGINFO("SetAudioFaderControl: handle=%d, fader balance restored to %d", handle, originalFaderBalance);
            }

            // 32. Test GetAudioPrimaryLanguage and SetAudioPrimaryLanguage (get-set-restore)
            string originalPrimaryLanguage;
            result = _audioManager->GetAudioPrimaryLanguage(handle, originalPrimaryLanguage);
            LOGINFO("GetAudioPrimaryLanguage: handle=%d, result=%u, original_language='%s'", handle, result, originalPrimaryLanguage.c_str());

            if (result == Core::ERROR_NONE) {
                // Set test primary language
                string testPrimaryLanguage = (originalPrimaryLanguage == "eng") ? "spa" : "eng";
                result = _audioManager->SetAudioPrimaryLanguage(handle, testPrimaryLanguage);
                LOGINFO("SetAudioPrimaryLanguage: handle=%d, result=%u, test_language='%s'", handle, result, testPrimaryLanguage.c_str());

                // Verify primary language was set
                string verifyPrimaryLanguage;
                _audioManager->GetAudioPrimaryLanguage(handle, verifyPrimaryLanguage);
                LOGINFO("GetAudioPrimaryLanguage: handle=%d, verified_language='%s'", handle, verifyPrimaryLanguage.c_str());

                // Restore original primary language
                _audioManager->SetAudioPrimaryLanguage(handle, originalPrimaryLanguage);
                LOGINFO("SetAudioPrimaryLanguage: handle=%d, primary language restored to '%s'", handle, originalPrimaryLanguage.c_str());
            }

            // 33. Test GetAudioSecondaryLanguage and SetAudioSecondaryLanguage (get-set-restore)
            string originalSecondaryLanguage;
            result = _audioManager->GetAudioSecondaryLanguage(handle, originalSecondaryLanguage);
            LOGINFO("GetAudioSecondaryLanguage: handle=%d, result=%u, original_language='%s'", handle, result, originalSecondaryLanguage.c_str());

            if (result == Core::ERROR_NONE) {
                // Set test secondary language
                string testSecondaryLanguage = (originalSecondaryLanguage == "eng") ? "fra" : "eng";
                result = _audioManager->SetAudioSecondaryLanguage(handle, testSecondaryLanguage);
                LOGINFO("SetAudioSecondaryLanguage: handle=%d, result=%u, test_language='%s'", handle, result, testSecondaryLanguage.c_str());

                // Verify secondary language was set
                string verifySecondaryLanguage;
                _audioManager->GetAudioSecondaryLanguage(handle, verifySecondaryLanguage);
                LOGINFO("GetAudioSecondaryLanguage: handle=%d, verified_language='%s'", handle, verifySecondaryLanguage.c_str());

                // Restore original secondary language
                _audioManager->SetAudioSecondaryLanguage(handle, originalSecondaryLanguage);
                LOGINFO("SetAudioSecondaryLanguage: handle=%d, secondary language restored to '%s'", handle, originalSecondaryLanguage.c_str());
            }

            // 34. Test read-only capability methods
            int32_t audioCapabilities = 0;
            result = _audioManager->GetAudioCapabilities(handle, audioCapabilities);
            LOGINFO("GetAudioCapabilities: handle=%d, result=%u, capabilities=0x%X", handle, result, audioCapabilities);

            int32_t ms12Capabilities = 0;
            result = _audioManager->GetAudioMS12Capabilities(handle, ms12Capabilities);
            LOGINFO("GetAudioMS12Capabilities: handle=%d, result=%u, capabilities=0x%X", handle, result, ms12Capabilities);

            bool isOutputConnected = false;
            result = _audioManager->IsAudioOutputConnected(handle, isOutputConnected);
            LOGINFO("IsAudioOutputConnected: handle=%d, result=%u, connected=%s", handle, result, isOutputConnected ? "true" : "false");

            // 35. Test MS12 Profile methods (get-set-restore)
            string originalMS12Profile;
            result = _audioManager->GetAudioMS12Profile(handle, originalMS12Profile);
            LOGINFO("GetAudioMS12Profile: handle=%d, result=%u, original_profile='%s'", handle, result, originalMS12Profile.c_str());

            if (result == Core::ERROR_NONE && !originalMS12Profile.empty()) {
                // Set test MS12 profile
                string testMS12Profile = (originalMS12Profile == "Movie") ? "Music" : "Movie";
                result = _audioManager->SetAudioMS12Profile(handle, testMS12Profile);
                LOGINFO("SetAudioMS12Profile: handle=%d, result=%u, test_profile='%s'", handle, result, testMS12Profile.c_str());

                // Verify MS12 profile was set
                string verifyMS12Profile;
                _audioManager->GetAudioMS12Profile(handle, verifyMS12Profile);
                LOGINFO("GetAudioMS12Profile: handle=%d, verified_profile='%s'", handle, verifyMS12Profile.c_str());

                // Restore original MS12 profile
                _audioManager->SetAudioMS12Profile(handle, originalMS12Profile);
                LOGINFO("SetAudioMS12Profile: handle=%d, MS12 profile restored to '%s'", handle, originalMS12Profile.c_str());
            }

            // 36. Test MS12 Profile List (read-only)
            Exchange::IDeviceSettingsAudio::IDeviceSettingsAudioMS12AudioProfileIterator* ms12ProfileList = nullptr;
            result = _audioManager->GetAudioMS12ProfileList(handle, ms12ProfileList);
            LOGINFO("GetAudioMS12ProfileList: handle=%d, result=%u", handle, result);
            if (ms12ProfileList && result == Core::ERROR_NONE) {
                LOGINFO("Iterating through MS12 profiles:");
                Exchange::IDeviceSettingsAudio::MS12AudioProfile currentProfile;
                uint32_t profileIndex = 0;
                bool hasMore = true;
                while (hasMore) {
                    hasMore = ms12ProfileList->Next(currentProfile);
                    if (hasMore) {
                        LOGINFO("  MS12Profile[%u]: '%s'", profileIndex, currentProfile.audioProfile.c_str());
                        profileIndex++;
                    }
                }
                LOGINFO("Total MS12 profiles found: %u", profileIndex);
                ms12ProfileList->Release();
            } else if (ms12ProfileList) {
                ms12ProfileList->Release();
            }

            // 37. Test additional advanced methods
            result = _audioManager->SetAudioMixerLevels(handle, Exchange::IDeviceSettingsAudio::AudioInput::AUDIO_INPUT_PRIMARY, 75);
            LOGINFO("SetAudioMixerLevels: handle=%d, result=%u, input=PRIMARY, volume=75", handle, result);

            // Test reset methods
            result = _audioManager->ResetAudioDialogEnhancement(handle);
            LOGINFO("ResetAudioDialogEnhancement: handle=%d, result=%u", handle, result);

            result = _audioManager->ResetAudioBassEnhancer(handle);
            LOGINFO("ResetAudioBassEnhancer: handle=%d, result=%u", handle, result);

            result = _audioManager->ResetAudioSurroundVirtualizer(handle);
            LOGINFO("ResetAudioSurroundVirtualizer: handle=%d, result=%u", handle, result);

            result = _audioManager->ResetAudioVolumeLeveller(handle);
            LOGINFO("ResetAudioVolumeLeveller: handle=%d, result=%u", handle, result);

            // 38. Test HDMI ARC specific methods (only for HDMI ARC ports)
            if (portType == Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC) {
                int32_t supportedARCTypes = 0;
                result = _audioManager->GetSupportedARCTypes(handle, supportedARCTypes);
                LOGINFO("GetSupportedARCTypes: handle=%d, result=%u, types=0x%X", handle, result, supportedARCTypes);

                // Test EnableARC
                Exchange::IDeviceSettingsAudio::AudioARCStatus arcStatus;
                arcStatus.arcType = Exchange::IDeviceSettingsAudio::AudioARCType::AUDIO_ARCTYPE_ARC;
                arcStatus.status = true;
                result = _audioManager->EnableARC(handle, arcStatus);
                LOGINFO("EnableARC: handle=%d, result=%u, arcType=ARC, status=true", handle, result);

                // Test SetSAD (Short Audio Descriptor)
                uint8_t sadList[15] = {0x09, 0x07, 0x07, 0x15, 0x07, 0x50, 0x3D, 0x07, 0x50, 0x1F, 0x07, 0x57, 0x5F, 0x00, 0x67};
                result = _audioManager->SetSAD(handle, sadList, 15);
                LOGINFO("SetSAD: handle=%d, result=%u, count=15", handle, result);

                // Get ARC port ID
                int32_t arcPortId = -1;
                result = _audioManager->GetAudioHDMIARCPortId(handle, arcPortId);
                LOGINFO("GetAudioHDMIARCPortId: handle=%d, result=%u, portId=%d", handle, result, arcPortId);
            }

            // 39. Test MS12 Settings Override
            result = _audioManager->SetAudioMS12SettingsOverride(handle, "Movie", "DialogEnhancement", "5", "ADD");
            LOGINFO("SetAudioMS12SettingsOverride: handle=%d, result=%u, profile=Movie, setting=DialogEnhancement, value=5, state=ADD", handle, result);

            LOGINFO("---------- Completed ALL Audio API testing for Port Type: %s ----------", portTypeName);
        }

        LOGINFO("========== Complete Audio APIs Testing Completed ==========\\n");
    }

    // Audio Event Handler Implementations
    void UserPlugin::OnAssociatedAudioMixingChanged(bool mixing)
    {
        LOGINFO("========== Audio Event: Associated Audio Mixing Changed ==========");
        LOGINFO("OnAssociatedAudioMixingChanged: mixing=%s", mixing ? "enabled" : "disabled");

        // Add your custom logic here
        if (mixing) {
            LOGINFO("Associated audio mixing enabled");
        } else {
            LOGINFO("Associated audio mixing disabled");
        }
    }

    void UserPlugin::OnAudioFaderControlChanged(int32_t mixerBalance)
    {
        LOGINFO("========== Audio Event: Audio Fader Control Changed ==========");
        LOGINFO("OnAudioFaderControlChanged: mixerBalance=%d", mixerBalance);

        // Add your custom logic here
        LOGINFO("Audio fader balance changed to %d", mixerBalance);
    }

    void UserPlugin::OnAudioPrimaryLanguageChanged(const string& primaryLanguage)
    {
        LOGINFO("========== Audio Event: Primary Language Changed ==========");
        LOGINFO("OnAudioPrimaryLanguageChanged: primaryLanguage='%s'", primaryLanguage.c_str());

        // Add your custom logic here
        LOGINFO("Primary audio language changed to %s", primaryLanguage.c_str());
    }

    void UserPlugin::OnAudioSecondaryLanguageChanged(const string& secondaryLanguage)
    {
        LOGINFO("========== Audio Event: Secondary Language Changed ==========");
        LOGINFO("OnAudioSecondaryLanguageChanged: secondaryLanguage='%s'", secondaryLanguage.c_str());

        // Add your custom logic here
        LOGINFO("Secondary audio language changed to %s", secondaryLanguage.c_str());
    }

    void UserPlugin::OnAudioOutHotPlug(Exchange::IDeviceSettingsAudio::AudioPortType portType, uint32_t uiPortNumber, bool isPortConnected)
    {
        LOGINFO("========== Audio Event: Audio Output Hot Plug ==========");
        LOGINFO("OnAudioOutHotPlug: portType=%d, portNumber=%u, isConnected=%s", 
               static_cast<int>(portType), uiPortNumber, isPortConnected ? "true" : "false");

        // Add your custom logic here
        const char* portTypeName = "UNKNOWN";
        switch (portType) {
            case Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMI:
                portTypeName = "HDMI";
                break;
            case Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_SPDIF:
                portTypeName = "SPDIF";
                break;
            case Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_SPEAKER:
                portTypeName = "SPEAKER";
                break;
            case Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMIARC:
                portTypeName = "HDMI_ARC";
                break;
            default:
                break;
        }

        if (isPortConnected) {
            LOGINFO("Audio device connected on %s port %u", portTypeName, uiPortNumber);
        } else {
            LOGINFO("Audio device disconnected from %s port %u", portTypeName, uiPortNumber);
        }
    }

    void UserPlugin::OnAudioFormatUpdate(Exchange::IDeviceSettingsAudio::AudioFormat audioFormat)
    {
        LOGINFO("========== Audio Event: Audio Format Update ==========");
        LOGINFO("OnAudioFormatUpdate: audioFormat=%d", static_cast<int>(audioFormat));

        // Add your custom logic here
        const char* formatName = "UNKNOWN";
        switch (audioFormat) {
            case Exchange::IDeviceSettingsAudio::AudioFormat::AUDIO_FORMAT_PCM:
                formatName = "PCM";
                break;
            case Exchange::IDeviceSettingsAudio::AudioFormat::AUDIO_FORMAT_DOLBY_AC3:
                formatName = "DOLBY_AC3";
                break;
            case Exchange::IDeviceSettingsAudio::AudioFormat::AUDIO_FORMAT_DOLBY_EAC3:
                formatName = "DOLBY_EAC3";
                break;
            case Exchange::IDeviceSettingsAudio::AudioFormat::AUDIO_FORMAT_DOLBY_EAC3_ATMOS:
                formatName = "DOLBY_EAC3_ATMOS";
                break;
            default:
                break;
        }

        LOGINFO("Audio format updated to %s", formatName);
    }

    void UserPlugin::OnDolbyAtmosCapabilitiesChanged(Exchange::IDeviceSettingsAudio::DolbyAtmosCapability atmosCapability, bool status)
    {
        LOGINFO("========== Audio Event: Dolby Atmos Capabilities Changed ==========");
        LOGINFO("OnDolbyAtmosCapabilitiesChanged: atmosCapability=%d, status=%s", 
               static_cast<int>(atmosCapability), status ? "available" : "not available");

        // Add your custom logic here
        const char* capabilityName = "UNKNOWN";
        switch (atmosCapability) {
            case Exchange::IDeviceSettingsAudio::DolbyAtmosCapability::AUDIO_DOLBY_ATMOS_NOT_SUPPORTED:
                capabilityName = "NOT_SUPPORTED";
                break;
            case Exchange::IDeviceSettingsAudio::DolbyAtmosCapability::AUDIO_DOLBY_ATMOS_DDPLUS_STREAM:
                capabilityName = "DDPLUS_STREAM";
                break;
            case Exchange::IDeviceSettingsAudio::DolbyAtmosCapability::AUDIO_DOLBY_ATMOS_METADATA:
                capabilityName = "METADATA";
                break;
            default:
                break;
        }

        LOGINFO("Dolby Atmos capability %s is %s", capabilityName, status ? "available" : "not available");
    }

    void UserPlugin::OnAudioPortStateChanged(Exchange::IDeviceSettingsAudio::AudioPortState audioPortState)
    {
        LOGINFO("========== Audio Event: Audio Port State Changed ==========");
        LOGINFO("OnAudioPortStateChanged: audioPortState=%d", static_cast<int>(audioPortState));

        // Add your custom logic here
        const char* stateName = "UNKNOWN";
        switch (audioPortState) {
            case Exchange::IDeviceSettingsAudio::AudioPortState::AUDIO_PORT_STATE_UNINITIALIZED:
                stateName = "UNINITIALIZED";
                break;
            case Exchange::IDeviceSettingsAudio::AudioPortState::AUDIO_PORT_STATE_INITIALIZED:
                stateName = "INITIALIZED";
                break;
            default:
                break;
        }

        LOGINFO("Audio port state changed to %s", stateName);
    }

    void UserPlugin::OnAudioModeEvent(Exchange::IDeviceSettingsAudio::AudioPortType audioPortType, Exchange::IDeviceSettingsAudio::StereoMode audioMode)
    {
        LOGINFO("========== Audio Event: Audio Mode Event ==========");
        LOGINFO("OnAudioModeEvent: audioPortType=%d, audioMode=%d", 
               static_cast<int>(audioPortType), static_cast<int>(audioMode));

        // Add your custom logic here
        const char* portTypeName = "UNKNOWN";
        switch (audioPortType) {
            case Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_HDMI:
                portTypeName = "HDMI";
                break;
            case Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_SPDIF:
                portTypeName = "SPDIF";
                break;
            case Exchange::IDeviceSettingsAudio::AudioPortType::AUDIO_PORT_TYPE_SPEAKER:
                portTypeName = "SPEAKER";
                break;
            default:
                break;
        }

        const char* modeName = "UNKNOWN";
        switch (audioMode) {
            case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_MONO:
                modeName = "MONO";
                break;
            case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_STEREO:
                modeName = "STEREO";
                break;
            case Exchange::IDeviceSettingsAudio::StereoMode::AUDIO_STEREO_SURROUND:
                modeName = "SURROUND";
                break;
            default:
                break;
        }

        LOGINFO("Audio mode changed to %s on %s port", modeName, portTypeName);
    }

    void UserPlugin::OnAudioLevelChangedEvent(int32_t audioLevel)
    {
        LOGINFO("========== Audio Event: Audio Level Changed ==========");
        LOGINFO("OnAudioLevelChangedEvent: audioLevel=%d", audioLevel);

        // Add your custom logic here
        LOGINFO("Audio level changed to %d", audioLevel);
    }

    // VideoPort Event Handler Implementations
    void UserPlugin::OnResolutionPostChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange resolution)
    {
        LOGINFO("========== VideoPort Event: Resolution Post Change ==========");
        LOGINFO("OnResolutionPostChange: width=%u, height=%u", resolution.width, resolution.height);

        // Add your custom logic here
        LOGINFO("Video resolution changed to %ux%u after change", resolution.width, resolution.height);
    }

    void UserPlugin::OnResolutionPreChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange resolution)
    {
        LOGINFO("========== VideoPort Event: Resolution Pre Change ==========");
        LOGINFO("OnResolutionPreChange: width=%u, height=%u", resolution.width, resolution.height);

        // Add your custom logic here - preparation before resolution changes
        LOGINFO("Video resolution will change to %ux%u", resolution.width, resolution.height);
    }

    void UserPlugin::OnHDCPStatusChange(const Exchange::IDeviceSettingsVideoPort::HDCPStatus hdcpStatus)
    {
        LOGINFO("========== VideoPort Event: HDCP Status Change ==========");
        LOGINFO("OnHDCPStatusChange: hdcpStatus=%d", static_cast<int>(hdcpStatus));

        // Add your custom logic here
        const char* statusName = "UNKNOWN";
        switch (hdcpStatus) {
            case Exchange::IDeviceSettingsVideoPort::HDCPStatus::DS_HDCP_STATUS_UNPOWERED:
                statusName = "UNPOWERED";
                break;
            case Exchange::IDeviceSettingsVideoPort::HDCPStatus::DS_HDCP_STATUS_UNAUTHENTICATED:
                statusName = "UNAUTHENTICATED";
                break;
            case Exchange::IDeviceSettingsVideoPort::HDCPStatus::DS_HDCP_STATUS_AUTHENTICATED:
                statusName = "AUTHENTICATED";
                break;
            case Exchange::IDeviceSettingsVideoPort::HDCPStatus::DS_HDCP_STATUS_AUTHENTICATIONFAILURE:
                statusName = "AUTHENTICATION_FAILURE";
                break;
            case Exchange::IDeviceSettingsVideoPort::HDCPStatus::DS_HDCP_STATUS_INPROGRESS:
                statusName = "IN_PROGRESS";
                break;
            case Exchange::IDeviceSettingsVideoPort::HDCPStatus::DS_HDCP_STATUS_PORTDISABLED:
                statusName = "PORT_DISABLED";
                break;
            default:
                break;
        }

        LOGINFO("HDCP status changed to %s", statusName);
    }

    void UserPlugin::OnVideoFormatUpdate(const Exchange::IDeviceSettingsVideoPort::HDRStandard videoFormatHDR)
    {
        LOGINFO("========== VideoPort Event: Video Format Update ==========");
        LOGINFO("OnVideoFormatUpdate: videoFormatHDR=0x%x", static_cast<uint16_t>(videoFormatHDR));

        // Add your custom logic here
        LOGINFO("Video HDR format updated to 0x%x", static_cast<uint16_t>(videoFormatHDR));
        
        // Log individual HDR standards if multiple are set (it's a bit mask)
        if (videoFormatHDR & Exchange::IDeviceSettingsVideoPort::HDRStandard::DS_HDRSTANDARD_HDR10) {
            LOGINFO("HDR10 format detected");
        }
        if (videoFormatHDR & Exchange::IDeviceSettingsVideoPort::HDRStandard::DS_HDRSTANDARD_HLG) {
            LOGINFO("HLG format detected");
        }
        if (videoFormatHDR & Exchange::IDeviceSettingsVideoPort::HDRStandard::DS_HDRSTANDARD_DOLBYVISION) {
            LOGINFO("Dolby Vision format detected");
        }
        if (videoFormatHDR & Exchange::IDeviceSettingsVideoPort::HDRStandard::DS_HDRSTANDARD_HDR10PLUS) {
            LOGINFO("HDR10+ format detected");
        }
        if (videoFormatHDR & Exchange::IDeviceSettingsVideoPort::HDRStandard::DS_HDRSTANDARD_SDR) {
            LOGINFO("SDR format detected");
        }
    }

    // VideoDevice Event Handler Implementations
    void UserPlugin::OnZoomSettingsChanged(const Exchange::IDeviceSettingsVideoDevice::VideoZoom zoomSetting)
    {
        LOGINFO("========== VideoDevice Event: Zoom Settings Changed ==========");
        LOGINFO("OnZoomSettingsChanged: zoomSetting=%d", static_cast<int>(zoomSetting));

        // Add your custom logic here
        const char* zoomName = "UNKNOWN";
        switch (zoomSetting) {
            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_NONE:
                zoomName = "NONE";
                break;
            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_FULL:
                zoomName = "FULL";
                break;
            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_16_9:
                zoomName = "LB_16_9";
                break;
            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_14_9:
                zoomName = "LB_14_9";
                break;
            case Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_PLATFORM:
                zoomName = "PLATFORM";
                break;
            default:
                break;
        }
        
        LOGINFO("VideoDevice zoom mode changed to %s", zoomName);
    }

    void UserPlugin::OnDisplayFrameratePreChange(const string frameRate)
    {
        LOGINFO("========== VideoDevice Event: Display Framerate Pre-Change ==========");
        LOGINFO("OnDisplayFrameratePreChange: frameRate=%s", frameRate.c_str());

        // Add your custom logic here
    }

    void UserPlugin::OnDisplayFrameratePostChange(const string frameRate)
    {
        LOGINFO("========== VideoDevice Event: Display Framerate Post-Change ==========");
        LOGINFO("OnDisplayFrameratePostChange: frameRate=%s", frameRate.c_str());

        // Add your custom logic here
    }

    void UserPlugin::TestVideoPortAPIs()
    {
        LOGINFO("========== Complete VideoPort APIs Testing Framework ==========");

        if (!_videoPortManager) {
            LOGERR("VideoPort Manager is not available");
            return;
        }

        LOGINFO("Testing ALL VideoPort APIs with get-set-restore pattern");

        // Test video port types - comprehensive testing on multiple port types
        Exchange::IDeviceSettingsVideoPort::VideoPort testPortTypes[] = {
            Exchange::IDeviceSettingsVideoPort::VideoPort::DS_VIDEO_PORT_TYPE_HDMI,
            Exchange::IDeviceSettingsVideoPort::VideoPort::DS_VIDEO_PORT_TYPE_DVI,
            Exchange::IDeviceSettingsVideoPort::VideoPort::DS_VIDEO_PORT_TYPE_COMPONENT,
            Exchange::IDeviceSettingsVideoPort::VideoPort::DS_VIDEO_PORT_TYPE_INTERNAL
        };

        const char* portTypeNames[] = {"HDMI", "DVI", "COMPONENT", "INTERNAL"};
        int numPortTypes = sizeof(testPortTypes) / sizeof(testPortTypes[0]);

        for (int i = 0; i < numPortTypes; i++) {
            Exchange::IDeviceSettingsVideoPort::VideoPort portType = testPortTypes[i];
            const char* portTypeName = portTypeNames[i];

            LOGINFO("---------- Testing ALL VideoPort APIs for Port Type: %s ----------", portTypeName);

            // 1. Test GetVideoPort - Get handle for this port type
            int32_t handle = -1;
            Core::hresult result = _videoPortManager->GetVideoPort(portType, 0, handle);
            LOGINFO("GetVideoPort: portType=%d, index=0, result=%u, handle=%d", static_cast<int>(portType), result, handle);

            if (result == Core::ERROR_NONE) {
                // 2. Test IsVideoPortEnabled and EnableVideoPort (get-set-restore)
                bool originalEnabled = false;
                result = _videoPortManager->IsVideoPortEnabled(handle, originalEnabled);
                LOGINFO("IsVideoPortEnabled: handle=%d, result=%u, originalEnabled=%s", handle, result, originalEnabled ? "true" : "false");
                
                if (result == Core::ERROR_NONE) {
                    // Test enabling/disabling
                    bool testEnabled = !originalEnabled;
                    result = _videoPortManager->EnableVideoPort(handle, testEnabled);
                    LOGINFO("EnableVideoPort: handle=%d, result=%u, test_enabled=%s", handle, result, testEnabled ? "true" : "false");
                    
                    // Verify change
                    bool currentEnabled = false;
                    result = _videoPortManager->IsVideoPortEnabled(handle, currentEnabled);
                    LOGINFO("IsVideoPortEnabled (after change): handle=%d, result=%u, currentEnabled=%s", handle, result, currentEnabled ? "true" : "false");
                    
                    // Restore original value
                    result = _videoPortManager->EnableVideoPort(handle, originalEnabled);
                    LOGINFO("EnableVideoPort (restore): handle=%d, result=%u, restored_enabled=%s", handle, result, originalEnabled ? "true" : "false");
                }

                // 3. Test IsVideoPortDisplayConnected (read-only)
                bool isConnected = false;
                result = _videoPortManager->IsVideoPortDisplayConnected(handle, isConnected);
                LOGINFO("IsVideoPortDisplayConnected: handle=%d, result=%u, isConnected=%s", handle, result, isConnected ? "true" : "false");

                // 4. Test IsVideoPortDisplaySurround (read-only)
                bool isSurround = false;
                result = _videoPortManager->IsVideoPortDisplaySurround(handle, isSurround);
                LOGINFO("IsVideoPortDisplaySurround: handle=%d, result=%u, isSurround=%s", handle, result, isSurround ? "true" : "false");

                // 5. Test GetVideoPortDisplaySurroundMode (read-only)
                Exchange::IDeviceSettingsVideoPort::VideoPortSurroundMode surroundMode;
                result = _videoPortManager->GetVideoPortDisplaySurroundMode(handle, surroundMode);
                LOGINFO("GetVideoPortDisplaySurroundMode: handle=%d, result=%u, surroundMode=%d", handle, result, static_cast<int>(surroundMode));

                // 6. Test GetVideoPortResolution and SetVideoPortResolution (get-set-restore)
                Exchange::IDeviceSettingsVideoPort::VideoPortResolution originalResolution;
                result = _videoPortManager->GetVideoPortResolution(handle, originalResolution);
                LOGINFO("GetVideoPortResolution: handle=%d, result=%u", handle, result);
                LOGINFO("Original Resolution: name='%s', pixelRes=%d, aspectRatio=%d, stereoMode=%d, frameRate=%d, interlaced=%s",
                        originalResolution.name.c_str(), static_cast<int>(originalResolution.pixelResolution),
                        static_cast<int>(originalResolution.aspectRatio), static_cast<int>(originalResolution.stereoScopicMode),
                        static_cast<int>(originalResolution.frameRate), originalResolution.interlaced ? "true" : "false");

                if (result == Core::ERROR_NONE) {
                    // Create test resolution (modify frame rate for testing)
                    Exchange::IDeviceSettingsVideoPort::VideoPortResolution testResolution = originalResolution;
                    testResolution.frameRate = (originalResolution.frameRate == Exchange::IDeviceSettingsVideoPort::VideoFrameRate::DS_VIDEO_FRAMERATE_30) ? 
                                               Exchange::IDeviceSettingsVideoPort::VideoFrameRate::DS_VIDEO_FRAMERATE_60 : 
                                               Exchange::IDeviceSettingsVideoPort::VideoFrameRate::DS_VIDEO_FRAMERATE_30;
                    testResolution.name = "TestResolution_" + originalResolution.name;

                    result = _videoPortManager->SetVideoPortResolution(handle, testResolution, false, false);
                    LOGINFO("SetVideoPortResolution: handle=%d, result=%u, test_name='%s', test_frameRate=%d", 
                            handle, result, testResolution.name.c_str(), static_cast<int>(testResolution.frameRate));

                    // Verify change
                    Exchange::IDeviceSettingsVideoPort::VideoPortResolution currentResolution;
                    result = _videoPortManager->GetVideoPortResolution(handle, currentResolution);
                    LOGINFO("GetVideoPortResolution (after change): result=%u, current_name='%s', current_frameRate=%d",
                            result, currentResolution.name.c_str(), static_cast<int>(currentResolution.frameRate));

                    // Restore original resolution
                    result = _videoPortManager->SetVideoPortResolution(handle, originalResolution, false, false);
                    LOGINFO("SetVideoPortResolution (restore): handle=%d, result=%u", handle, result);
                }

                // 7. Test HDCP related APIs
                // 7a. Test IsHDCPEnabledOnVideoPort (read current state)
                bool originalHDCPEnabled = false;
                result = _videoPortManager->IsHDCPEnabledOnVideoPort(handle, originalHDCPEnabled);
                LOGINFO("IsHDCPEnabledOnVideoPort: handle=%d, result=%u, originalHDCPEnabled=%s", handle, result, originalHDCPEnabled ? "true" : "false");

                // 7b. Test EnableHDCPOnVideoPort (enable/disable with dummy key)
                if (result == Core::ERROR_NONE) {
                    // Test enabling HDCP with dummy key
                    uint8_t dummyKey[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
                    result = _videoPortManager->EnableHDCPOnVideoPort(handle, true, dummyKey, sizeof(dummyKey));
                    LOGINFO("EnableHDCPOnVideoPort: handle=%d, result=%u, enable=true, keySize=%d", handle, result, (int)sizeof(dummyKey));

                    // Verify HDCP status
                    bool currentHDCPEnabled = false;
                    result = _videoPortManager->IsHDCPEnabledOnVideoPort(handle, currentHDCPEnabled);
                    LOGINFO("IsHDCPEnabledOnVideoPort (after enable): result=%u, currentHDCPEnabled=%s", result, currentHDCPEnabled ? "true" : "false");

                    // Restore original HDCP state
                    result = _videoPortManager->EnableHDCPOnVideoPort(handle, originalHDCPEnabled, dummyKey, sizeof(dummyKey));
                    LOGINFO("EnableHDCPOnVideoPort (restore): handle=%d, result=%u, restored=%s", handle, result, originalHDCPEnabled ? "true" : "false");
                }

                // 7c. Test GetHDCPStatusOnVideoPort (read-only)
                Exchange::IDeviceSettingsVideoPort::HDCPStatus hdcpStatus;
                result = _videoPortManager->GetHDCPStatusOnVideoPort(handle, hdcpStatus);
                LOGINFO("GetHDCPStatusOnVideoPort: handle=%d, result=%u, hdcpStatus=%d", handle, result, static_cast<int>(hdcpStatus));

                // 7d. Test HDCP Protocol Version APIs (read-only)
                Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion hdcpVersion;
                result = _videoPortManager->GetHDCPProtocolVersionOnVideoPort(handle, hdcpVersion);
                LOGINFO("GetHDCPProtocolVersionOnVideoPort: handle=%d, result=%u, hdcpVersion=%d", handle, result, static_cast<int>(hdcpVersion));

                Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion hdcpRxVersion;
                result = _videoPortManager->GetHDCPReceiverProtocolVersionOnVideoPort(handle, hdcpRxVersion);
                LOGINFO("GetHDCPReceiverProtocolVersionOnVideoPort: handle=%d, result=%u, hdcpRxVersion=%d", handle, result, static_cast<int>(hdcpRxVersion));

                Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion hdcpCurrentVersion;
                result = _videoPortManager->GetHDCPCurrentProtocolVersionOnVideoPort(handle, hdcpCurrentVersion);
                LOGINFO("GetHDCPCurrentProtocolVersionOnVideoPort: handle=%d, result=%u, hdcpCurrentVersion=%d", handle, result, static_cast<int>(hdcpCurrentVersion));

                // 8. Test HDMI Preference APIs (get-set-restore)
                Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion originalHdmiPreference;
                result = _videoPortManager->GetHDMIPreference(handle, originalHdmiPreference);
                LOGINFO("GetHDMIPreference: handle=%d, result=%u, originalHdmiPreference=%d", handle, result, static_cast<int>(originalHdmiPreference));

                if (result == Core::ERROR_NONE) {
                    // Set test HDMI preference
                    Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion testHdmiPreference = 
                        (originalHdmiPreference == Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion::DS_HDCP_VERSION_1X) ? 
                        Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion::DS_HDCP_VERSION_2X : 
                        Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion::DS_HDCP_VERSION_1X;

                    result = _videoPortManager->SetHDMIPreference(handle, testHdmiPreference);
                    LOGINFO("SetHDMIPreference: handle=%d, result=%u, testHdmiPreference=%d", handle, result, static_cast<int>(testHdmiPreference));

                    // Verify change
                    Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion currentHdmiPreference;
                    result = _videoPortManager->GetHDMIPreference(handle, currentHdmiPreference);
                    LOGINFO("GetHDMIPreference (after change): result=%u, currentHdmiPreference=%d", result, static_cast<int>(currentHdmiPreference));

                    // Restore original preference
                    result = _videoPortManager->SetHDMIPreference(handle, originalHdmiPreference);
                    LOGINFO("SetHDMIPreference (restore): handle=%d, result=%u", handle, result);
                }

                // 9. Test IsVideoPortActive (read-only)
                bool isActive = false;
                result = _videoPortManager->IsVideoPortActive(handle, isActive);
                LOGINFO("IsVideoPortActive: handle=%d, result=%u, isActive=%s", handle, result, isActive ? "true" : "false");

                // 10. Test TV capabilities (read-only)
                int32_t hdrCapabilities = 0;
                result = _videoPortManager->GetTVHDRCapabilities(handle, hdrCapabilities);
                LOGINFO("GetTVHDRCapabilities: handle=%d, result=%u, hdrCapabilities=0x%x", handle, result, hdrCapabilities);

                int32_t supportedResolutions = 0;
                result = _videoPortManager->GetTVSupportedResolutions(handle, supportedResolutions);
                LOGINFO("GetTVSupportedResolutions: handle=%d, result=%u, supportedResolutions=0x%x", handle, result, supportedResolutions);

                // 11. Test Force Disable 4K APIs (get-set-restore)
                bool originalForceDisable4K = false;
                result = _videoPortManager->GetForceDisable4K(handle, originalForceDisable4K);
                LOGINFO("GetForceDisable4K: handle=%d, result=%u, originalForceDisable4K=%s", handle, result, originalForceDisable4K ? "true" : "false");

                if (result == Core::ERROR_NONE) {
                    // Test toggling force disable 4K
                    bool testForceDisable4K = !originalForceDisable4K;
                    result = _videoPortManager->SetForceDisable4K(handle, testForceDisable4K);
                    LOGINFO("SetForceDisable4K: handle=%d, result=%u, testForceDisable4K=%s", handle, result, testForceDisable4K ? "true" : "false");

                    // Verify change
                    bool currentForceDisable4K = false;
                    result = _videoPortManager->GetForceDisable4K(handle, currentForceDisable4K);
                    LOGINFO("GetForceDisable4K (after change): result=%u, currentForceDisable4K=%s", result, currentForceDisable4K ? "true" : "false");

                    // Restore original value
                    result = _videoPortManager->SetForceDisable4K(handle, originalForceDisable4K);
                    LOGINFO("SetForceDisable4K (restore): handle=%d, result=%u", handle, result);
                }

                // 12. Test IsVideoPortOutputHDR (read-only)
                bool isHDROutput = false;
                result = _videoPortManager->IsVideoPortOutputHDR(handle, isHDROutput);
                LOGINFO("IsVideoPortOutputHDR: handle=%d, result=%u, isHDROutput=%s", handle, result, isHDROutput ? "true" : "false");

                // 13. Test Display Format APIs (read-only)
                Exchange::IDeviceSettingsVideoPort::HDRStandard videoEOTF;
                result = _videoPortManager->GetVideoEOTF(handle, videoEOTF);
                LOGINFO("GetVideoEOTF: handle=%d, result=%u, videoEOTF=0x%x", handle, result, static_cast<uint16_t>(videoEOTF));

                Exchange::IDeviceSettingsVideoPort::DisplayMatrixCoefficients matrixCoefficients;
                result = _videoPortManager->GetMatrixCoefficients(handle, matrixCoefficients);
                LOGINFO("GetMatrixCoefficients: handle=%d, result=%u, matrixCoefficients=%d", handle, result, static_cast<int>(matrixCoefficients));

                uint32_t colorDepth = 0;
                result = _videoPortManager->GetColorDepth(handle, colorDepth);
                LOGINFO("GetColorDepth: handle=%d, result=%u, colorDepth=0x%x", handle, result, colorDepth);

                Exchange::IDeviceSettingsVideoPort::DisplayColorSpace colorSpace;
                result = _videoPortManager->GetColorSpace(handle, colorSpace);
                LOGINFO("GetColorSpace: handle=%d, result=%u, colorSpace=%d", handle, result, static_cast<int>(colorSpace));

                Exchange::IDeviceSettingsVideoPort::DisplayQuantizationRange quantizationRange;
                result = _videoPortManager->GetQuantizationRange(handle, quantizationRange);
                LOGINFO("GetQuantizationRange: handle=%d, result=%u, quantizationRange=%d", handle, result, static_cast<int>(quantizationRange));

                // 14. Test GetCurrentOutputSettings (read-only composite)
                Exchange::IDeviceSettingsVideoPort::DSOutputSettings outputSettings;
                result = _videoPortManager->GetCurrentOutputSettings(handle, outputSettings);
                LOGINFO("GetCurrentOutputSettings: handle=%d, result=%u", handle, result);
                LOGINFO("OutputSettings: videoEotf=0x%x, matrixCoeff=%d, colorDepth=0x%x, colorSpace=%d, quantRange=%d",
                        static_cast<uint16_t>(outputSettings.videoEotf), static_cast<int>(outputSettings.matrixCoefficients),
                        outputSettings.colorDepth, static_cast<int>(outputSettings.colorSpace),
                        static_cast<int>(outputSettings.quantizationRange));

                // 15. Test Color Depth Capabilities and Preferences
                uint32_t colorDepthCapabilities = 0;
                result = _videoPortManager->GetColorDepthCapabilities(handle, colorDepthCapabilities);
                LOGINFO("GetColorDepthCapabilities: handle=%d, result=%u, capabilities=0x%x", handle, result, colorDepthCapabilities);

                // Test preferred color depth (get-set-restore)
                Exchange::IDeviceSettingsVideoPort::DisplayColorDepth originalPreferredColorDepth;
                result = _videoPortManager->GetPreferredColorDepth(handle, originalPreferredColorDepth, false);
                LOGINFO("GetPreferredColorDepth: handle=%d, result=%u, originalPreferredColorDepth=%d", handle, result, static_cast<int>(originalPreferredColorDepth));

                if (result == Core::ERROR_NONE) {
                    // Test setting different color depth
                    Exchange::IDeviceSettingsVideoPort::DisplayColorDepth testColorDepth = 
                        (originalPreferredColorDepth == Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_8BIT) ?
                        Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_10BIT :
                        Exchange::IDeviceSettingsVideoPort::DisplayColorDepth::DS_DISPLAY_COLORDEPTH_8BIT;

                    result = _videoPortManager->SetPreferredColorDepth(handle, testColorDepth, false);
                    LOGINFO("SetPreferredColorDepth: handle=%d, result=%u, testColorDepth=%d", handle, result, static_cast<int>(testColorDepth));

                    // Verify change
                    Exchange::IDeviceSettingsVideoPort::DisplayColorDepth currentPreferredColorDepth;
                    result = _videoPortManager->GetPreferredColorDepth(handle, currentPreferredColorDepth, false);
                    LOGINFO("GetPreferredColorDepth (after change): result=%u, currentPreferredColorDepth=%d", result, static_cast<int>(currentPreferredColorDepth));

                    // Restore original preference
                    result = _videoPortManager->SetPreferredColorDepth(handle, originalPreferredColorDepth, false);
                    LOGINFO("SetPreferredColorDepth (restore): handle=%d, result=%u", handle, result);
                }

                // 16. Test Set APIs (write-only, careful with these)
                result = _videoPortManager->SetBackgroundColor(handle, Exchange::IDeviceSettingsVideoPort::VideoBackgroundColor::DS_VIDEO_BGCOLOR_BLACK);
                LOGINFO("SetBackgroundColor: handle=%d, result=%u, backgroundColor=BLACK", handle, result);

                result = _videoPortManager->SetForceHDRMode(handle, Exchange::IDeviceSettingsVideoPort::HDRStandard::DS_HDRSTANDARD_SDR);
                LOGINFO("SetForceHDRMode: handle=%d, result=%u, hdrMode=SDR", handle, result);

                // 17. Test ResetVideoPortOutputToSDR (global reset)
                result = _videoPortManager->ResetVideoPortOutputToSDR();
                LOGINFO("ResetVideoPortOutputToSDR: result=%u", result);

            } else {
                LOGWARN("Could not get handle for %s video port - skipping tests for this port type", portTypeName);
            }

            LOGINFO("---------- Completed ALL VideoPort API testing for Port Type: %s ----------", portTypeName);
        }

        LOGINFO("========== Complete VideoPort APIs Testing Completed ==========\\n");
    }

    void UserPlugin::TestVideoDeviceAPIs()
    {
        LOGINFO("========== Complete VideoDevice APIs Testing Framework ==========");

        if (!_videoDeviceManager) {
            LOGERR("VideoDevice Manager is not available");
            return;
        }

        LOGINFO("Testing ALL VideoDevice APIs with get-set-restore pattern");

        // 1. Test GetVideoDeviceHandle (requires index parameter)
        int32_t videoDeviceHandle = -1;
        Core::hresult result = _videoDeviceManager->GetVideoDeviceHandle(0, videoDeviceHandle);
        LOGINFO("GetVideoDeviceHandle: result=%u, handle=%d", result, videoDeviceHandle);

        if (result == Core::ERROR_NONE) {
            // 2. Test SetVideoDeviceDFC (get current, set test, restore) - this triggers OnVideoDeviceDFCChanged event
            LOGINFO("---------- Testing VideoDevice DFC APIs ----------");
            
            // First test: Set a test DFC zoom mode
            Exchange::IDeviceSettingsVideoDevice::VideoZoom testZoomMode1 = Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_FULL;
            result = _videoDeviceManager->SetVideoDeviceDFC(videoDeviceHandle, testZoomMode1);
            LOGINFO("SetVideoDeviceDFC: handle=%d, result=%u, zoom_mode=FULL", videoDeviceHandle, result);

            // Another test: Set different zoom mode
            Exchange::IDeviceSettingsVideoDevice::VideoZoom testZoomMode2 = Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_LB_16_9;
            result = _videoDeviceManager->SetVideoDeviceDFC(videoDeviceHandle, testZoomMode2);
            LOGINFO("SetVideoDeviceDFC: handle=%d, result=%u, zoom_mode=LB_16_9", videoDeviceHandle, result);

            // Test third zoom mode
            Exchange::IDeviceSettingsVideoDevice::VideoZoom testZoomMode3 = Exchange::IDeviceSettingsVideoDevice::VideoZoom::DS_VIDEO_DEVICE_ZOOM_NONE;
            result = _videoDeviceManager->SetVideoDeviceDFC(videoDeviceHandle, testZoomMode3);
            LOGINFO("SetVideoDeviceDFC: handle=%d, result=%u, zoom_mode=NONE", videoDeviceHandle, result);

            // 3. Test GetHDRCapabilities
            LOGINFO("---------- Testing VideoDevice HDR Capabilities APIs ----------");
            int32_t hdrCapabilities = 0;
            result = _videoDeviceManager->GetHDRCapabilities(videoDeviceHandle, hdrCapabilities);
            LOGINFO("GetHDRCapabilities: handle=%d, result=%u, hdrCapabilities=0x%x", videoDeviceHandle, result, hdrCapabilities);

            // 4. Test GetSupportedVideoCodingFormats
            LOGINFO("---------- Testing VideoDevice Supported Video Coding Formats APIs ----------");
            int32_t supportedFormats = 0;
            result = _videoDeviceManager->GetSupportedVideoCodingFormats(videoDeviceHandle, supportedFormats);
            LOGINFO("GetSupportedVideoCodingFormats: handle=%d, result=%u, supportedFormats=0x%x", videoDeviceHandle, result, supportedFormats);

            // Test GetCodecInfo for different codec types
            LOGINFO("---------- Testing GetCodecInfo API ----------");
            Exchange::IDeviceSettingsVideoDevice::IDeviceSettingsVideoCodecProfileSupportIterator* codecInfo = nullptr;
            result = _videoDeviceManager->GetCodecInfo(videoDeviceHandle, Exchange::IDeviceSettingsVideoDevice::VideoCodec::DS_VIDEO_CODEC_MPEG2, codecInfo);
            LOGINFO("GetCodecInfo: handle=%d, codec=MPEG2, result=%u", videoDeviceHandle, result);
            if (codecInfo) {
                codecInfo->Release();
            }

            // 5. Test DisableHDR
            LOGINFO("---------- Testing VideoDevice DisableHDR API ----------");
            result = _videoDeviceManager->DisableHDR(videoDeviceHandle, true);
            LOGINFO("DisableHDR: handle=%d, disable=true, result=%u", videoDeviceHandle, result);
            
            result = _videoDeviceManager->DisableHDR(videoDeviceHandle, false);
            LOGINFO("DisableHDR: handle=%d, disable=false, result=%u", videoDeviceHandle, result);

            // 6. Test FRF Mode APIs
            LOGINFO("---------- Testing VideoDevice FRF Mode APIs ----------");
            int32_t originalFrfMode = -1;
            result = _videoDeviceManager->GetFRFMode(videoDeviceHandle, originalFrfMode);
            LOGINFO("GetFRFMode: handle=%d, result=%u, frfMode=%d", videoDeviceHandle, result, originalFrfMode);
            
            if (result == Core::ERROR_NONE) {
                int32_t testFrfMode = 1;
                result = _videoDeviceManager->SetFRFMode(videoDeviceHandle, testFrfMode);
                LOGINFO("SetFRFMode: handle=%d, frfMode=%d, result=%u", videoDeviceHandle, testFrfMode, result);
                
                // Restore original
                result = _videoDeviceManager->SetFRFMode(videoDeviceHandle, originalFrfMode);
                LOGINFO("SetFRFMode (restore): handle=%d, frfMode=%d, result=%u", videoDeviceHandle, originalFrfMode, result);
            }

            // 7. Test Display Frame Rate APIs  
            LOGINFO("---------- Testing VideoDevice Display Frame Rate APIs ----------");
            string currentFrameRate;
            result = _videoDeviceManager->GetCurrentDisplayFrameRate(videoDeviceHandle, currentFrameRate);
            LOGINFO("GetCurrentDisplayFrameRate: handle=%d, result=%u, frameRate=%s", videoDeviceHandle, result, currentFrameRate.c_str());
            
            string testFrameRate = "24";
            result = _videoDeviceManager->SetDisplayFrameRate(videoDeviceHandle, testFrameRate);
            LOGINFO("SetDisplayFrameRate: handle=%d, frameRate=%s, result=%u", videoDeviceHandle, testFrameRate.c_str(), result);

        } else {
            LOGERR("Failed to get VideoDevice handle, skipping VideoDevice API tests");
        }
        
        LOGINFO("VideoDevice API testing completed.");
    }

    void UserPlugin::OnSleepModeChanged(const Exchange::IDeviceSettingsHost::SleepMode sleepMode)
    {
        LOGINFO("========== Host Event: Sleep Mode Changed ==========");
        LOGINFO("OnSleepModeChanged: sleepMode=%d", static_cast<int>(sleepMode));

        // Add your custom logic here
        switch (sleepMode) {
            case Exchange::IDeviceSettingsHost::SleepMode::DS_HOST_SLEEPMODE_LIGHT:
                LOGINFO("Sleep mode changed to LIGHT");
                break;
            case Exchange::IDeviceSettingsHost::SleepMode::DS_HOST_SLEEPMODE_DEEP:
                LOGINFO("Sleep mode changed to DEEP");
                break;
            default:
                LOGINFO("Sleep mode changed to UNKNOWN (%d)", static_cast<int>(sleepMode));
                break;
        }
    }

    void UserPlugin::TestHostAPIs()
    {
        LOGINFO("========== Complete Host APIs Testing Framework ==========");

        if (!_hostManager) {
            LOGERR("Host Manager interface is not available");
            return;
        }

        LOGINFO("Testing ALL Host APIs with get-set-restore pattern");

        // 1. Test GetPreferredSleepMode and SetPreferredSleepMode (get-set-restore)
        Exchange::IDeviceSettingsHost::SleepMode originalSleepMode;
        Core::hresult result = _hostManager->GetPreferredSleepMode(originalSleepMode);
        LOGINFO("GetPreferredSleepMode: result=%u, original_mode=%d", result, static_cast<int>(originalSleepMode));

        if (result == Core::ERROR_NONE) {
            // Set test sleep mode (toggle between light and deep)
            Exchange::IDeviceSettingsHost::SleepMode testSleepMode = 
                (originalSleepMode == Exchange::IDeviceSettingsHost::SleepMode::DS_HOST_SLEEPMODE_LIGHT) ? 
                Exchange::IDeviceSettingsHost::SleepMode::DS_HOST_SLEEPMODE_DEEP : 
                Exchange::IDeviceSettingsHost::SleepMode::DS_HOST_SLEEPMODE_LIGHT;
            result = _hostManager->SetPreferredSleepMode(testSleepMode);
            LOGINFO("SetPreferredSleepMode: result=%u, test_mode=%d", result, static_cast<int>(testSleepMode));

            // Verify the mode was set
            Exchange::IDeviceSettingsHost::SleepMode verifySleepMode;
            _hostManager->GetPreferredSleepMode(verifySleepMode);
            LOGINFO("GetPreferredSleepMode: verified_mode=%d", static_cast<int>(verifySleepMode));

            // Restore original mode
            _hostManager->SetPreferredSleepMode(originalSleepMode);
            LOGINFO("SetPreferredSleepMode: mode restored to %d", static_cast<int>(originalSleepMode));
        }

        // 2. Test GetCPUTemperature (read-only)
        float cpuTemperature = 0.0f;
        result = _hostManager->GetCPUTemperature(cpuTemperature);
        LOGINFO("GetCPUTemperature: result=%u, temperature=%.2fC", result, cpuTemperature);

        // 3. Test GetHALVersion (read-only)
        uint32_t halVersion = 0;
        result = _hostManager->GetHALVersion(halVersion);
        LOGINFO("GetHALVersion: result=%u, version=0x%x (%d.%d)", result, halVersion, 
               (halVersion >> 16) & 0xFFFF, halVersion & 0xFFFF);

        // 4. Test GetSoCID (read-only)
        string socID;
        result = _hostManager->GetSoCID(socID);
        LOGINFO("GetSoCID: result=%u, socID='%s'", result, socID.c_str());

        // 5. Test GetEDID (read-only byte array)
        uint8_t edidBytes[1024] = {0};
        result = _hostManager->GetEDID(edidBytes, 1024);
        LOGINFO("GetEDID: result=%u, bytes_length=1024", result);
        if (result == Core::ERROR_NONE) {
            LOGINFO("EDID bytes retrieved successfully");
            // Optionally log first few bytes for verification
            LOGINFO("First 8 EDID bytes: %02X %02X %02X %02X %02X %02X %02X %02X", 
                   edidBytes[0], edidBytes[1], edidBytes[2], edidBytes[3], 
                   edidBytes[4], edidBytes[5], edidBytes[6], edidBytes[7]);
        }

        // 6. Test GetMS12ConfigType (read-only)
        string ms12Config;
        result = _hostManager->GetMS12ConfigType(ms12Config);
        LOGINFO("GetMS12ConfigType: result=%u, ms12Config='%s'", result, ms12Config.c_str());

        LOGINFO("========== Complete Host APIs Testing Completed ==========\\n");
    }

    // ========== Display Event Handlers ==========

    void UserPlugin::OnDisplayRxSense(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent)
    {
        LOGINFO("========== Display RX Sense Event ==========");
        LOGINFO("Display RX Sense Event: displayEvent=%d", static_cast<int>(displayEvent));
        
        if (displayEvent == Exchange::IDeviceSettingsDisplay::DisplayEvent::DS_DISPLAY_RXSENSE_ON) {
            LOGINFO("Display RX Sense: DISPLAY CONNECTED (RXSENSE_ON)");
        } else if (displayEvent == Exchange::IDeviceSettingsDisplay::DisplayEvent::DS_DISPLAY_RXSENSE_OFF) {
            LOGINFO("Display RX Sense: DISPLAY DISCONNECTED (RXSENSE_OFF)");
        } else {
            LOGINFO("Display RX Sense: UNKNOWN EVENT (%d)", static_cast<int>(displayEvent));
        }
    }

    void UserPlugin::OnDisplayHDCPStatus()
    {
        LOGINFO("========== Display HDCP Status Event ==========");
        LOGINFO("Display HDCP Status changed");
        // Note: This event indicates HDCP status changed but doesn't provide the new status
        // Application would typically call GetHDCPStatus after receiving this event
    }

    void UserPlugin::OnDisplayHDMIHotPlug(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent)
    {
        LOGINFO("========== Display HDMI Hot Plug Event ==========");
        LOGINFO("Display HDMI Hot Plug Event: displayEvent=%d", static_cast<int>(displayEvent));
        
        if (displayEvent == Exchange::IDeviceSettingsDisplay::DisplayEvent::DS_DISPLAY_EVENT_CONNECTED) {
            LOGINFO("Display HDMI: DISPLAY CONNECTED - Cable plugged in");
        } else if (displayEvent == Exchange::IDeviceSettingsDisplay::DisplayEvent::DS_DISPLAY_EVENT_DISCONNECTED) {
            LOGINFO("Display HDMI: DISPLAY DISCONNECTED - Cable unplugged");
        } else {
            LOGINFO("Display HDMI: UNKNOWN EVENT (%d)", static_cast<int>(displayEvent));
        }
    }

    void UserPlugin::TestDisplayAPIs()
    {
        LOGINFO("========== Display EDID APIs Testing Framework ==========");

        if (!_displayManager) {
            LOGERR("Display Manager interface is not available");
            return;
        }

        LOGINFO("Testing Display EDID APIs for validation: GetDisplayEdid and GetDisplayEdidBytes");

        // Test display port types - focus on HDMI as primary target
        Exchange::IDeviceSettingsDisplay::DisplayPortType testPortTypes[] = {
            Exchange::IDeviceSettingsDisplay::DisplayPortType::DS_DISPLAY_PORT_TYPE_HDMI,
            Exchange::IDeviceSettingsDisplay::DisplayPortType::DS_DISPLAY_PORT_TYPE_DVI
        };

        const char* portTypeNames[] = {"HDMI", "DVI"};
        int numPortTypes = sizeof(testPortTypes) / sizeof(testPortTypes[0]);

        for (int i = 0; i < numPortTypes; i++) {
            Exchange::IDeviceSettingsDisplay::DisplayPortType portType = testPortTypes[i];
            const char* portTypeName = portTypeNames[i];

            LOGINFO("---------- Testing Display EDID APIs for Port Type: %s ----------", portTypeName);

            // Get display handle for this port type
            int32_t handle = -1;
            Core::hresult result = _displayManager->GetDisplay(portType, 0, handle);
            LOGINFO("GetDisplay: portType=%d (%s), index=0, result=%u, handle=%d", 
                   static_cast<int>(portType), portTypeName, result, handle);

            if (result == Core::ERROR_NONE && handle != -1) {
                // Test GetDisplayEdid (structured EDID data validation)
                Exchange::IDeviceSettingsDisplay::DisplayEDID edid;
                Exchange::IDeviceSettingsDisplay::IDSVideoPortResolutionIterator* resolutionIterator = nullptr;
                result = _displayManager->GetDisplayEdid(handle, edid, resolutionIterator);
                LOGINFO("GetDisplayEdid: handle=%d, result=%u", handle, result);
                
                if (result == Core::ERROR_NONE) {
                    LOGINFO("EDID Validation - ProductCode:%d, SerialNumber:%d, ManufactureYear:%d, ManufactureWeek:%d", 
                           edid.productCode, edid.serialNumber, edid.manufactureYear, edid.manufactureWeek);
                    LOGINFO("EDID Validation - HDMIDeviceType:%s, IsRepeater:%s, MonitorName:'%s'", 
                           edid.hdmiDeviceType ? "true" : "false", 
                           edid.isRepeater ? "true" : "false", 
                           edid.monitorName.c_str());
                    LOGINFO("EDID Validation - PhysicalAddress:%d.%d.%d.%d, SupportedResolutions:%d", 
                           edid.physicalAddressA, edid.physicalAddressB, 
                           edid.physicalAddressC, edid.physicalAddressD, 
                           edid.numOfSupportedResolution);
                    
                    // Clean up resolution iterator if valid
                    if (resolutionIterator) {
                        resolutionIterator->Release();
                    }
                }

                // Test GetDisplayEdidBytes (raw EDID byte array validation)
                uint8_t edidBytes[256] = {0};
                result = _displayManager->GetDisplayEdidBytes(handle, edidBytes, 256);
                LOGINFO("GetDisplayEdidBytes: handle=%d, result=%u, edidLength=256", handle, result);
                
                if (result == Core::ERROR_NONE) {
                    LOGINFO("EDID Raw Bytes validation successful");
                    LOGINFO("First 8 EDID bytes: %02X %02X %02X %02X %02X %02X %02X %02X", 
                           edidBytes[0], edidBytes[1], edidBytes[2], edidBytes[3], 
                           edidBytes[4], edidBytes[5], edidBytes[6], edidBytes[7]);
                    
                    // Validate EDID header pattern (should start with 0x00FFFFFFFFFFFF00)
                    if (edidBytes[0] == 0x00 && edidBytes[1] == 0xFF && edidBytes[7] == 0x00) {
                        LOGINFO("EDID header pattern validation: PASSED");
                    } else {
                        LOGINFO("EDID header pattern validation: WARNING - Non-standard pattern");
                    }
                }

            } else {
                LOGINFO("Skipping EDID validation for %s port (handle not available)", portTypeName);
            }

            LOGINFO("---------- Completed Display EDID APIs validation for %s ----------\\n", portTypeName);
        }

        LOGINFO("========== Display EDID APIs Testing Completed ==========\\n");
    }

    // ========== CompositeIn Event Handlers ==========

    void UserPlugin::OnCompositeInHotPlug(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const bool isConnected)
    {
        LOGINFO("========== CompositeIn Hot Plug Event ==========");
        LOGINFO("CompositeIn Hot Plug Event: port=%d, isConnected=%s", static_cast<int>(port), isConnected ? "true" : "false");
        
        const char* portName = "UNKNOWN";
        switch (port) {
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_0:
                portName = "PORT_0";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_1:
                portName = "PORT_1";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_NONE:
                portName = "PORT_NONE";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_MAX:
                portName = "PORT_MAX";
                break;
        }
        
        if (isConnected) {
            LOGINFO("CompositeIn %s: CABLE CONNECTED", portName);
        } else {
            LOGINFO("CompositeIn %s: CABLE DISCONNECTED", portName);
        }
    }

    void UserPlugin::OnCompositeInSignalStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus signalStatus)
    {
        LOGINFO("========== CompositeIn Signal Status Event ==========");
        LOGINFO("CompositeIn Signal Status Event: port=%d, signalStatus=%d", static_cast<int>(port), static_cast<int>(signalStatus));
        
        const char* portName = "UNKNOWN";
        switch (port) {
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_0:
                portName = "PORT_0";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_1:
                portName = "PORT_1";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_NONE:
                portName = "PORT_NONE";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_MAX:
                portName = "PORT_MAX";
                break;
        }
        
        const char* statusName = "UNKNOWN";
        switch (signalStatus) {
            case Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus::DS_COMPOSITE_IN_SIGNAL_STATUS_NONE:
                statusName = "NONE";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus::DS_COMPOSITE_IN_SIGNAL_STATUS_NOSIGNAL:
                statusName = "NO_SIGNAL";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus::DS_COMPOSITE_IN_SIGNAL_STATUS_UNSTABLE:
                statusName = "UNSTABLE";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus::DS_COMPOSITE_IN_SIGNAL_STATUS_NOTSUPPORTED:
                statusName = "NOT_SUPPORTED";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus::DS_COMPOSITE_IN_SIGNAL_STATUS_STABLE:
                statusName = "STABLE";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus::DS_COMPOSITE_IN_SIGNAL_STATUS_MAX:
                statusName = "MAX";
                break;
        }
        
        LOGINFO("CompositeIn %s Signal Status: %s", portName, statusName);
    }

    void UserPlugin::OnCompositeInStatus(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const bool isPresented)
    {
        LOGINFO("========== CompositeIn Status Event ==========");
        LOGINFO("CompositeIn Status Event: activePort=%d, isPresented=%s", static_cast<int>(activePort), isPresented ? "true" : "false");
        
        const char* portName = "UNKNOWN";
        switch (activePort) {
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_0:
                portName = "PORT_0";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_1:
                portName = "PORT_1";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_NONE:
                portName = "PORT_NONE";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_MAX:
                portName = "PORT_MAX";
                break;
        }
        
        if (isPresented) {
            LOGINFO("CompositeIn %s: VIDEO PRESENTED TO USER", portName);
        } else {
            LOGINFO("CompositeIn %s: VIDEO NOT PRESENTED", portName);
        }
    }

    void UserPlugin::OnCompositeInVideoModeUpdate(const Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const Exchange::IDeviceSettingsCompositeIn::DisplayVideoPortResolution videoResolution)
    {
        LOGINFO("========== CompositeIn Video Mode Update Event ==========");
        LOGINFO("CompositeIn Video Mode Update Event: activePort=%d", static_cast<int>(activePort));
        
        const char* portName = "UNKNOWN";
        switch (activePort) {
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_0:
                portName = "PORT_0";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_1:
                portName = "PORT_1";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_NONE:
                portName = "PORT_NONE";
                break;
            case Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_MAX:
                portName = "PORT_MAX";
                break;
        }
        
        LOGINFO("CompositeIn %s Video Resolution Update:", portName);
        LOGINFO("  Resolution Name: %s", videoResolution.name.c_str());
        LOGINFO("  Pixel Resolution: 0x%x", static_cast<uint32_t>(videoResolution.pixelResolution));
        LOGINFO("  Aspect Ratio: %d", static_cast<int>(videoResolution.aspectRatio));
        LOGINFO("  Frame Rate: %d", static_cast<int>(videoResolution.frameRate));
        LOGINFO("  Interlaced: %s", videoResolution.interlaced ? "true" : "false");
    }

    void UserPlugin::TestCompositeInAPIs()
    {
        LOGINFO("========== Complete CompositeIn APIs Testing Framework ==========");

        if (!_compositeInManager) {
            LOGERR("CompositeIn Manager interface is not available");
            return;
        }

        LOGINFO("Testing ALL CompositeIn APIs with comprehensive coverage");

        // 1. Test GetNrOfCompositeInputs (read-only)
        int32_t nrCompositeInputs = 0;
        Core::hresult result = _compositeInManager->GetNrOfCompositeInputs(nrCompositeInputs);
        LOGINFO("GetNrOfCompositeInputs: result=%u, nrCompositeInputs=%d", result, nrCompositeInputs);

        if (result == Core::ERROR_NONE && nrCompositeInputs > 0) {
            // 2. Test GetCompositeInStatus (read-only)
            Exchange::IDeviceSettingsCompositeIn::CompositeInStatus status;
            result = _compositeInManager->GetCompositeInStatus(status);
            LOGINFO("GetCompositeInStatus: result=%u", result);
            
            if (result == Core::ERROR_NONE) {
                LOGINFO("CompositeIn Status:");
                LOGINFO("  isPresented: %s", status.isPresented ? "true" : "false");
                LOGINFO("  activePort: %d", static_cast<int>(status.activePort));
                LOGINFO("  isPort0Connected: %s", status.isPort0Connected ? "true" : "false");
                LOGINFO("  isPort1Connected: %s", status.isPort1Connected ? "true" : "false");
            }

            // 3. Test SelectCompositeInPort (test different ports)
            Exchange::IDeviceSettingsCompositeIn::CompositeInPort testPorts[] = {
                Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_0,
                Exchange::IDeviceSettingsCompositeIn::CompositeInPort::DS_COMPOSITE_IN_PORT_1
            };
            
            const char* portNames[] = {"PORT_0", "PORT_1"};
            int numPorts = sizeof(testPorts) / sizeof(testPorts[0]);
            
            for (int i = 0; i < numPorts && i < nrCompositeInputs; i++) {
                result = _compositeInManager->SelectCompositeInPort(testPorts[i]);
                LOGINFO("SelectCompositeInPort: port=%d (%s), result=%u", 
                       static_cast<int>(testPorts[i]), portNames[i], result);
                
                // Get status after port selection to verify
                if (result == Core::ERROR_NONE) {
                    Exchange::IDeviceSettingsCompositeIn::CompositeInStatus newStatus;
                    result = _compositeInManager->GetCompositeInStatus(newStatus);
                    if (result == Core::ERROR_NONE) {
                        LOGINFO("After port selection - activePort: %d", static_cast<int>(newStatus.activePort));
                    }
                }
            }

            // 4. Test ScaleCompositeInVideo (test different scaling rectangles)
            Exchange::IDeviceSettingsCompositeIn::VideoRectangle testRectangles[] = {
                {0, 0, 1920, 1080},      // Full HD
                {100, 100, 1280, 720},   // 720p with offset
                {200, 200, 640, 480},    // 480p with offset
                {0, 0, 720, 576}         // PAL
            };
            
            const char* rectNames[] = {"Full_HD", "720p_Offset", "480p_Offset", "PAL"};
            int numRectangles = sizeof(testRectangles) / sizeof(testRectangles[0]);
            
            for (int i = 0; i < numRectangles; i++) {
                result = _compositeInManager->ScaleCompositeInVideo(testRectangles[i]);
                LOGINFO("ScaleCompositeInVideo: rect=%s (%d,%d,%dx%d), result=%u", 
                       rectNames[i],
                       testRectangles[i].x, testRectangles[i].y,
                       testRectangles[i].width, testRectangles[i].height,
                       result);
            }

            // 5. Restore original port selection
            if (result == Core::ERROR_NONE) {
                Exchange::IDeviceSettingsCompositeIn::CompositeInStatus originalStatus;
                result = _compositeInManager->GetCompositeInStatus(originalStatus);
                if (result == Core::ERROR_NONE) {
                    LOGINFO("Test completed - Current active port: %d", static_cast<int>(originalStatus.activePort));
                }
            }

        } else {
            LOGINFO("No CompositeIn inputs available for testing (nrCompositeInputs: %d)", nrCompositeInputs);
        }

        LOGINFO("========== Complete CompositeIn APIs Testing Completed ==========\\n");
    }

} // namespace Plugin
} // namespace WPEFramework
