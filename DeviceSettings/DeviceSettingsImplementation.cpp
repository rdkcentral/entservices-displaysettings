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

#include "DeviceSettingsImplementation.h"
#include "DSController.h"
#include "DeviceSettingsFPDImplementation.h"
#include "DeviceSettingsHdmiInImplementation.h"
#include "DeviceSettingsAudioImplementation.h"
#include "DeviceSettingsHostImplementation.h"

#include "UtilsLogging.h"
#include "UtilsSearchRDKProfile.h"
#include <syscall.h>

using namespace std;

#define DELEGATE_TO_COMPONENT(component, method, ...) \
    ENTRY_LOG; \
    Core::hresult result = (component != nullptr) ? component->method(__VA_ARGS__) : Core::ERROR_UNAVAILABLE; \
    EXIT_LOG; \
    return result;

// Macro for methods that don't take parameters
#define DELEGATE_TO_COMPONENT_NO_PARAMS(component, method) \
    ENTRY_LOG; \
    Core::hresult result = (component != nullptr) ? component->method() : Core::ERROR_UNAVAILABLE; \
    EXIT_LOG; \
    return result;

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(DeviceSettingsImp, 1, 0);

    DeviceSettingsImp* DeviceSettingsImp::_instance = nullptr;

    DeviceSettingsImp::DeviceSettingsImp()
        : _dsController(DSController::Create(this))  // Direct dependency injection in initializer list
        , _fpdSettings(DeviceSettingsFPDImpl::Create())
        , _hdmiInSettings(DeviceSettingsHdmiInImp::Create())
        , _audioSettings(DeviceSettingsAudioImpl::Create())
        , _videoPortSettings(DeviceSettingsVideoPortImpl::Create())
        , _videoDeviceSettings(DeviceSettingsVideoDeviceImpl::Create())
        , _hostSettings(DeviceSettingsHostImpl::Create())
        , _displaySettings(DeviceSettingsDisplayImpl::Create())
        , _compositeInSettings(DeviceSettingsCompositeInImpl::Create())
        , mConnectionId(0)
    {
        // Set the static instance for backward compatibility (if still needed)
        DeviceSettingsImp::_instance = this;
        LOGINFO("DeviceSettingsImp Constructor - Instance Address: %p", this);
        LOGINFO("DSController implementation instance: %p", _dsController);

        // Initialize profile type
        profileType = searchRdkProfile();
        LOGINFO("Initialized profileType: %d (0=STB, 1=TV)", profileType);

        LOGINFO("FPD implementation instance: %p", _fpdSettings);
        LOGINFO("VideoPort implementation instance: %p", _videoPortSettings);
        LOGINFO("VideoDevice implementation instance: %p", _videoDeviceSettings);
        LOGINFO("Host implementation instance: %p", _hostSettings);
        LOGINFO("HDMIIn implementation instance: %p", _hdmiInSettings);
        LOGINFO("Audio implementation instance: %p", _audioSettings);
        LOGINFO("Display implementation instance: %p", _displaySettings);
        LOGINFO("CompositeIn implementation instance: %p", _compositeInSettings);

    }

    DeviceSettingsImp::~DeviceSettingsImp() {
        LOGINFO("DeviceSettingsImp Destructor - Instance Address: %p", this);
        
        // Clean up created implementation instances
        if (_fpdSettings != nullptr) {
            delete _fpdSettings;
            _fpdSettings = nullptr;
        }
        
        if (_hdmiInSettings != nullptr) {
            delete _hdmiInSettings;
            _hdmiInSettings = nullptr;
        }
        
        if (_audioSettings != nullptr) {
            delete _audioSettings;
            _audioSettings = nullptr;
        }
        
        if (_videoPortSettings != nullptr) {
            delete _videoPortSettings;
            _videoPortSettings = nullptr;
        }
        if (_videoDeviceSettings != nullptr) {
            delete _videoDeviceSettings;
            _videoDeviceSettings = nullptr;
        }
        if (_hostSettings != nullptr) {
            delete _hostSettings;
            _hostSettings = nullptr;
        }
        if (_displaySettings != nullptr) {
            delete _displaySettings;
            _displaySettings = nullptr;
        }
        if (_compositeInSettings != nullptr) {
            delete _compositeInSettings;
            _compositeInSettings = nullptr;
        }
        
        // Clean up DSController last as it provides system infrastructure
        if (_dsController != nullptr) {
            delete _dsController;
            _dsController = nullptr;
        }
        
    }

    Core::hresult DeviceSettingsImp::Configure(PluginHost::IShell* service)
    {
        LOGINFO("DeviceSettingsImp Configure called with service: %p", service);

        if (service == nullptr) {
            LOGERR("Service parameter is null");
            return Core::ERROR_BAD_REQUEST;
        }

        // Initialize DSController power event listener with the service
        if (_dsController != nullptr) {
            LOGINFO("Initializing DSController power event listener");
            _dsController->InitializePowerEventListener(service);
        } else {
            LOGERR("DSController is null - cannot initialize power event listener");
        }

        LOGINFO("DeviceSettingsImp configured successfully");
        return Core::ERROR_NONE;
    }

    // ============================================================================
    // IDeviceSettingsFPD interface implementation - delegate to _fpdSettings interface
    // ============================================================================
    
    Core::hresult DeviceSettingsImp::Register(Exchange::IDeviceSettingsFPD::INotification* notification) {
        Core::hresult result;
        if (_fpdSettings != nullptr) {
            result = _fpdSettings->Register(notification);
            LOGINFO("FPD Register: SUCCESS - forwarded to implementation");
        } else {
            LOGERR("FPD Register: FAILED - _fpdSettings is null");
            result = Core::ERROR_UNAVAILABLE;
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::Unregister(Exchange::IDeviceSettingsFPD::INotification* notification) {
        DELEGATE_TO_COMPONENT(_fpdSettings, Unregister, notification)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDTime(const FPDTimeFormat timeFormat, const uint32_t minutes, const uint32_t seconds) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDTime, timeFormat, minutes, seconds)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDScroll(const uint32_t scrollHoldDuration, const uint32_t nHorizontalScrollIterations, const uint32_t nVerticalScrollIterations) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDScroll, scrollHoldDuration, nHorizontalScrollIterations, nVerticalScrollIterations)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDBlink(const FPDIndicator indicator, const uint32_t blinkDuration, const uint32_t blinkIterations) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDBlink, indicator, blinkDuration, blinkIterations)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDBrightness(const FPDIndicator indicator, const uint32_t brightNess, const bool persist) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDBrightness, indicator, brightNess, persist)
    }
    
    Core::hresult DeviceSettingsImp::GetFPDBrightness(const FPDIndicator indicator, uint32_t &brightNess) {
        DELEGATE_TO_COMPONENT(_fpdSettings, GetFPDBrightness, indicator, brightNess)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDState(const FPDIndicator indicator, const FPDState state) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDState, indicator, state)
    }
    
    Core::hresult DeviceSettingsImp::GetFPDState(const FPDIndicator indicator, FPDState &state) {
        DELEGATE_TO_COMPONENT(_fpdSettings, GetFPDState, indicator, state)
    }
    
    Core::hresult DeviceSettingsImp::GetFPDColor(const FPDIndicator indicator, uint32_t &color) {
        DELEGATE_TO_COMPONENT(_fpdSettings, GetFPDColor, indicator, color)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDColor(const FPDIndicator indicator, const uint32_t color) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDColor, indicator, color)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDTextBrightness(const FPDTextDisplay textDisplay, const uint32_t brightNess) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDTextBrightness, textDisplay, brightNess)
    }
    
    Core::hresult DeviceSettingsImp::GetFPDTextBrightness(const FPDTextDisplay textDisplay, uint32_t &brightNess) {
        DELEGATE_TO_COMPONENT(_fpdSettings, GetFPDTextBrightness, textDisplay, brightNess)
    }
    
    Core::hresult DeviceSettingsImp::EnableFPDClockDisplay(const bool enable) {
        DELEGATE_TO_COMPONENT(_fpdSettings, EnableFPDClockDisplay, enable)
    }
    
    Core::hresult DeviceSettingsImp::GetFPDTimeFormat(FPDTimeFormat &fpdTimeFormat) {
        DELEGATE_TO_COMPONENT(_fpdSettings, GetFPDTimeFormat, fpdTimeFormat)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDTimeFormat(const FPDTimeFormat fpdTimeFormat) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDTimeFormat, fpdTimeFormat)
    }
    
    Core::hresult DeviceSettingsImp::SetFPDMode(const FPDMode fpdMode) {
        DELEGATE_TO_COMPONENT(_fpdSettings, SetFPDMode, fpdMode)
    }

    Core::hresult DeviceSettingsImp::GetFPDDeviceConfig(Exchange::IDeviceSettingsFPD::IFPDDeviceConfigIterator*& deviceConfig) {
        DELEGATE_TO_COMPONENT(_fpdSettings, GetFPDDeviceConfig, deviceConfig)
    }

    // ============================================================================
    // IDeviceSettingsHDMIIn interface implementation - delegate to _hdmiInSettings interface
    // ============================================================================
    
    Core::hresult DeviceSettingsImp::Register(Exchange::IDeviceSettingsHDMIIn::INotification* notification) {
        Core::hresult result;
        if (_hdmiInSettings != nullptr) {
            result = _hdmiInSettings->Register(notification);
            LOGINFO("HDMIIn Register: SUCCESS - forwarded to implementation");
        } else {
            LOGERR("HDMIIn Register: FAILED - _hdmiInSettings is null");
            result = Core::ERROR_UNAVAILABLE;
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::Unregister(Exchange::IDeviceSettingsHDMIIn::INotification* notification) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, Unregister, notification)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIInNumbefOfInputs(int32_t &count) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMIInNumbefOfInputs, count)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIInStatus(HDMIInStatus &hdmiStatus, IHDMIInPortConnectionStatusIterator*& portConnectionStatus) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMIInStatus, hdmiStatus, portConnectionStatus)
    }
    
    Core::hresult DeviceSettingsImp::SelectHDMIInPort(const HDMIInPort port, const bool requestAudioMix, const bool topMostPlane, const HDMIVideoPlaneType videoPlaneType) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, SelectHDMIInPort, port, requestAudioMix, topMostPlane, videoPlaneType)
    }
    
    Core::hresult DeviceSettingsImp::ScaleHDMIInVideo(const HDMIInVideoRectangle videoPosition) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, ScaleHDMIInVideo, videoPosition)
    }
    
    Core::hresult DeviceSettingsImp::SelectHDMIZoomMode(const HDMIInVideoZoom zoomMode) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, SelectHDMIZoomMode, zoomMode)
    }
    
    Core::hresult DeviceSettingsImp::GetSupportedGameFeaturesList(IHDMIInGameFeatureListIterator *& gameFeatureList) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetSupportedGameFeaturesList, gameFeatureList)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIInAVLatency(uint32_t &videoLatency, uint32_t &audioLatency) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMIInAVLatency, videoLatency, audioLatency)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIInAllmStatus(const HDMIInPort port, bool &allmStatus) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMIInAllmStatus, port, allmStatus)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIInEdid2AllmSupport(const HDMIInPort port, bool &allmSupport) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMIInEdid2AllmSupport, port, allmSupport)
    }
    
    Core::hresult DeviceSettingsImp::SetHDMIInEdid2AllmSupport(const HDMIInPort port, bool allmSupport) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, SetHDMIInEdid2AllmSupport, port, allmSupport)
    }
    
    Core::hresult DeviceSettingsImp::GetEdidBytes(const HDMIInPort port, const uint16_t edidBytesLength, uint8_t edidBytes[]) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetEdidBytes, port, edidBytesLength, edidBytes)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMISPDInformation(const HDMIInPort port, const uint16_t spdBytesLength, uint8_t spdBytes[]) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMISPDInformation, port, spdBytesLength, spdBytes)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIEdidVersion(const HDMIInPort port, HDMIInEdidVersion &edidVersion) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMIEdidVersion, port, edidVersion)
    }
    
    Core::hresult DeviceSettingsImp::SetHDMIEdidVersion(const HDMIInPort port, const HDMIInEdidVersion edidVersion) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, SetHDMIEdidVersion, port, edidVersion)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIVideoMode(HDMIVideoPortResolution &videoPortResolution) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMIVideoMode, videoPortResolution)
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIVersion(const HDMIInPort port, HDMIInCapabilityVersion &capabilityVersion) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetHDMIVersion, port, capabilityVersion)
    }
    
    Core::hresult DeviceSettingsImp::SetVRRSupport(const HDMIInPort port, const bool vrrSupport) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, SetVRRSupport, port, vrrSupport)
    }
    
    Core::hresult DeviceSettingsImp::GetVRRSupport(const HDMIInPort port, bool &vrrSupport) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetVRRSupport, port, vrrSupport)
    }
    
    Core::hresult DeviceSettingsImp::GetVRRStatus(const HDMIInPort port, HDMIInVRRStatus &vrrStatus) {
        DELEGATE_TO_COMPONENT(_hdmiInSettings, GetVRRStatus, port, vrrStatus)
    }

    // ============================================================================
    // IDeviceSettingsAudio interface implementation - delegate to _audioSettings interface
    // ============================================================================
    
    Core::hresult DeviceSettingsImp::Register(Exchange::IDeviceSettingsAudio::INotification* notification) {
        DELEGATE_TO_COMPONENT(_audioSettings, Register, notification)
    }
    
    Core::hresult DeviceSettingsImp::Unregister(Exchange::IDeviceSettingsAudio::INotification* notification) {
        DELEGATE_TO_COMPONENT(_audioSettings, Unregister, notification)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioPort(const AudioPortType type, const int32_t index, int32_t &handle) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioPort, type, index, handle)
    }
    
    // GetAudioPorts and GetSupportedAudioPorts methods removed - iterator type doesn't exist

    Core::hresult DeviceSettingsImp::GetAudioPortConfig(const AudioPortType audioPort, AudioConfig &audioConfig) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioPortConfig, audioPort, audioConfig)
    }

    Core::hresult DeviceSettingsImp::SetAudioPortConfig(const AudioPortType audioPort, const AudioConfig audioConfig) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioPortConfig, audioPort, audioConfig)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioCapabilities(const int32_t handle, int32_t &capabilities) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioCapabilities, handle, capabilities)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioMS12Capabilities(const int32_t handle, int32_t &capabilities) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioMS12Capabilities, handle, capabilities)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioFormat(const int32_t handle, AudioFormat &audioFormat) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioFormat, handle, audioFormat)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioEncoding(const int32_t handle, AudioEncoding &encoding) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioEncoding, handle, encoding)
    }
    
    Core::hresult DeviceSettingsImp::GetSupportedCompressions(const int32_t handle, IDeviceSettingsAudioCompressionIterator*& compressions) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetSupportedCompressions, handle, compressions)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioCompression(const int32_t handle, AudioCompression &compression) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioCompression, handle, compression)
    }

    Core::hresult DeviceSettingsImp::SetAudioCompression(const int32_t handle, const AudioCompression compression) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioCompression, handle, compression)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioLevel(const int32_t handle, const float audioLevel) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioLevel, handle, audioLevel)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioLevel(const int32_t handle, float &audioLevel) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioLevel, handle, audioLevel)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioGain(const int32_t handle, const float gainLevel) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioGain, handle, gainLevel)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioGain(const int32_t handle, float &gainLevel) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioGain, handle, gainLevel)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioMute(const int32_t handle, const bool mute) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioMute, handle, mute)
    }
    
    Core::hresult DeviceSettingsImp::IsAudioMuted(const int32_t handle, bool &muted) {
        DELEGATE_TO_COMPONENT(_audioSettings, IsAudioMuted, handle, muted)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioDucking(const int32_t handle, const AudioDuckingType duckingType, const AudioDuckingAction duckingAction, const uint8_t level) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioDucking, handle, duckingType, duckingAction, level)
    }
    
    Core::hresult DeviceSettingsImp::GetStereoMode(const int32_t handle, AudioStereoMode &mode) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetStereoMode, handle, mode)
    }
    
    Core::hresult DeviceSettingsImp::SetStereoMode(const int32_t handle, const AudioStereoMode mode, const bool persist) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetStereoMode, handle, mode, persist)
    }
    
    Core::hresult DeviceSettingsImp::SetAssociatedAudioMixing(const int32_t handle, const bool mixing) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAssociatedAudioMixing, handle, mixing)
    }
    
    Core::hresult DeviceSettingsImp::GetAssociatedAudioMixing(const int32_t handle, bool &mixing) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAssociatedAudioMixing, handle, mixing)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioFaderControl(const int32_t handle, const int32_t mixerBalance) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioFaderControl, handle, mixerBalance)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioFaderControl(const int32_t handle, int32_t &mixerBalance) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioFaderControl, handle, mixerBalance)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioPrimaryLanguage(const int32_t handle, const string primaryAudioLanguage) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioPrimaryLanguage, handle, primaryAudioLanguage)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioPrimaryLanguage(const int32_t handle, string &primaryAudioLanguage) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioPrimaryLanguage, handle, primaryAudioLanguage)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioSecondaryLanguage(const int32_t handle, const string secondaryAudioLanguage) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioSecondaryLanguage, handle, secondaryAudioLanguage)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioSecondaryLanguage(const int32_t handle, string &secondaryAudioLanguage) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioSecondaryLanguage, handle, secondaryAudioLanguage)
    }
    
    Core::hresult DeviceSettingsImp::IsAudioOutputConnected(const int32_t handle, bool &isConnected) {
        DELEGATE_TO_COMPONENT(_audioSettings, IsAudioOutputConnected, handle, isConnected)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioSinkDeviceAtmosCapability(const int32_t handle, DolbyAtmosCapability &atmosCapability) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioSinkDeviceAtmosCapability, handle, atmosCapability)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioAtmosOutputMode(const int32_t handle, const bool enable) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioAtmosOutputMode, handle, enable)
    }

    // Missing Audio interface delegation methods
    Core::hresult DeviceSettingsImp::IsAudioPortEnabled(const int32_t handle, bool &enabled) {
        DELEGATE_TO_COMPONENT(_audioSettings, IsAudioPortEnabled, handle, enabled)
    }

    Core::hresult DeviceSettingsImp::EnableAudioPort(const int32_t handle, const bool enable) {
        DELEGATE_TO_COMPONENT(_audioSettings, EnableAudioPort, handle, enable)
    }

    Core::hresult DeviceSettingsImp::GetSupportedARCTypes(const int32_t handle, int32_t &types) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetSupportedARCTypes, handle, types)
    }

    Core::hresult DeviceSettingsImp::SetSAD(const int32_t handle, const uint8_t sadList[], const uint8_t count) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetSAD, handle, sadList, count)
    }
    
    Core::hresult DeviceSettingsImp::EnableARC(const int32_t handle, const AudioARCStatus arcStatus) {
        DELEGATE_TO_COMPONENT(_audioSettings, EnableARC, handle, arcStatus)
    }
    
    Core::hresult DeviceSettingsImp::GetStereoAuto(const int32_t handle, int32_t &mode) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetStereoAuto, handle, mode)
    }
    
    Core::hresult DeviceSettingsImp::SetStereoAuto(const int32_t handle, const int32_t mode, const bool persist) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetStereoAuto, handle, mode, persist)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioEnablePersist(const int32_t handle, bool &enabled, string &portName) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioEnablePersist, handle, enabled, portName)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioEnablePersist(const int32_t handle, const bool enable, const string portName) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioEnablePersist, handle, enable, portName)
    }
    
    Core::hresult DeviceSettingsImp::IsAudioMSDecoded(const int32_t handle, bool &hasms11Decode) {
        DELEGATE_TO_COMPONENT(_audioSettings, IsAudioMSDecoded, handle, hasms11Decode)
    }
    
    Core::hresult DeviceSettingsImp::IsAudioMS12Decoded(const int32_t handle, bool &hasms12Decode) {
        DELEGATE_TO_COMPONENT(_audioSettings, IsAudioMS12Decoded, handle, hasms12Decode)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioLEConfig(const int32_t handle, bool &enabled) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioLEConfig, handle, enabled)
    }
    
    Core::hresult DeviceSettingsImp::EnableAudioLEConfig(const int32_t handle, const bool enable) {
        DELEGATE_TO_COMPONENT(_audioSettings, EnableAudioLEConfig, handle, enable)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioDelay(const int32_t handle, const uint32_t audioDelay) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioDelay, handle, audioDelay)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioDelay(const int32_t handle, uint32_t &audioDelay) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioDelay, handle, audioDelay)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioDelayOffset(const int32_t handle, const uint32_t delayOffset) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioDelayOffset, handle, delayOffset)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioDelayOffset(const int32_t handle, uint32_t &delayOffset) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioDelayOffset, handle, delayOffset)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioCompression(const int32_t handle, const int32_t compressionLevel) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioCompression, handle, compressionLevel)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioCompression(const int32_t handle, int32_t &compressionLevel) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioCompression, handle, compressionLevel)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioDialogEnhancement(const int32_t handle, const int32_t level) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioDialogEnhancement, handle, level)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioDialogEnhancement(const int32_t handle, int32_t &level) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioDialogEnhancement, handle, level)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioDolbyVolumeMode(const int32_t handle, const bool enable) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioDolbyVolumeMode, handle, enable)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioDolbyVolumeMode(const int32_t handle, bool &enabled) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioDolbyVolumeMode, handle, enabled)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioIntelligentEqualizerMode(const int32_t handle, const int32_t mode) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioIntelligentEqualizerMode, handle, mode)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioIntelligentEqualizerMode(const int32_t handle, int32_t &mode) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioIntelligentEqualizerMode, handle, mode)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioVolumeLeveller(const int32_t handle, const VolumeLeveller volumeLeveller) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioVolumeLeveller, handle, volumeLeveller)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioVolumeLeveller(const int32_t handle, VolumeLeveller &volumeLeveller) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioVolumeLeveller, handle, volumeLeveller)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioBassEnhancer(const int32_t handle, const int32_t boost) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioBassEnhancer, handle, boost)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioBassEnhancer(const int32_t handle, int32_t &boost) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioBassEnhancer, handle, boost)
    }
    
    Core::hresult DeviceSettingsImp::EnableAudioSurroudDecoder(const int32_t handle, const bool enable) {
        DELEGATE_TO_COMPONENT(_audioSettings, EnableAudioSurroudDecoder, handle, enable)
    }
    
    Core::hresult DeviceSettingsImp::IsAudioSurroudDecoderEnabled(const int32_t handle, bool &enabled) {
        DELEGATE_TO_COMPONENT(_audioSettings, IsAudioSurroudDecoderEnabled, handle, enabled)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioDRCMode(const int32_t handle, const int32_t drcMode) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioDRCMode, handle, drcMode)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioDRCMode(const int32_t handle, int32_t &drcMode) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioDRCMode, handle, drcMode)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioSurroudVirtualizer(const int32_t handle, const SurroundVirtualizer surroundVirtualizer) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioSurroudVirtualizer, handle, surroundVirtualizer)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioSurroudVirtualizer(const int32_t handle, SurroundVirtualizer &surroundVirtualizer) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioSurroudVirtualizer, handle, surroundVirtualizer)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioMISteering(const int32_t handle, const bool enable) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioMISteering, handle, enable)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioMISteering(const int32_t handle, bool &enable) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioMISteering, handle, enable)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioGraphicEqualizerMode(const int32_t handle, const int32_t mode) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioGraphicEqualizerMode, handle, mode)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioGraphicEqualizerMode(const int32_t handle, int32_t &mode) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioGraphicEqualizerMode, handle, mode)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioMS12ProfileList(const int32_t handle, IDeviceSettingsAudioMS12AudioProfileIterator*& ms12ProfileList) const {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioMS12ProfileList, handle, ms12ProfileList)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioMS12Profile(const int32_t handle, string &profile) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioMS12Profile, handle, profile)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioMS12Profile(const int32_t handle, const string profile) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioMS12Profile, handle, profile)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioMixerLevels(const int32_t handle, const AudioInput audioInput, const int32_t volume) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioMixerLevels, handle, audioInput, volume)
    }
    
    Core::hresult DeviceSettingsImp::SetAudioMS12SettingsOverride(const int32_t handle, const string profileName, const string profileSettingsName, const string profileSettingValue, const string profileState) {
        DELEGATE_TO_COMPONENT(_audioSettings, SetAudioMS12SettingsOverride, handle, profileName, profileSettingsName, profileSettingValue, profileState)
    }
    
    Core::hresult DeviceSettingsImp::ResetAudioDialogEnhancement(const int32_t handle) {
        DELEGATE_TO_COMPONENT(_audioSettings, ResetAudioDialogEnhancement, handle)
    }
    
    Core::hresult DeviceSettingsImp::ResetAudioBassEnhancer(const int32_t handle) {
        DELEGATE_TO_COMPONENT(_audioSettings, ResetAudioBassEnhancer, handle)
    }
    
    Core::hresult DeviceSettingsImp::ResetAudioSurroundVirtualizer(const int32_t handle) {
        DELEGATE_TO_COMPONENT(_audioSettings, ResetAudioSurroundVirtualizer, handle)
    }
    
    Core::hresult DeviceSettingsImp::ResetAudioVolumeLeveller(const int32_t handle) {
        DELEGATE_TO_COMPONENT(_audioSettings, ResetAudioVolumeLeveller, handle)
    }
    
    Core::hresult DeviceSettingsImp::GetAudioHDMIARCPortId(const int32_t handle, int32_t &portId) {
        DELEGATE_TO_COMPONENT(_audioSettings, GetAudioHDMIARCPortId, handle, portId)
    }

    // ============================================================================
    // IDeviceSettingsVideoPort interface implementation - delegate to _videoPortSettings interface
    // ============================================================================
    
    Core::hresult DeviceSettingsImp::Register(Exchange::IDeviceSettingsVideoPort::INotification* notification) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, Register, notification)
    }
    
    Core::hresult DeviceSettingsImp::Unregister(Exchange::IDeviceSettingsVideoPort::INotification* notification) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, Unregister, notification)
    }
    
    Core::hresult DeviceSettingsImp::GetVideoPort(const VideoPortType videoPort, const int32_t index, int32_t &handle) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, GetVideoPort, videoPort, index, handle)
    }
    
    Core::hresult DeviceSettingsImp::IsVideoPortEnabled(const int32_t handle, bool &enabled) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, IsVideoPortEnabled, handle, enabled)
    }
    
    Core::hresult DeviceSettingsImp::EnableVideoPort(const int32_t handle, const bool enabled) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, EnableVideoPort, handle, enabled)
    }
    
    Core::hresult DeviceSettingsImp::IsVideoPortDisplayConnected(const int32_t handle, bool &connected) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, IsVideoPortDisplayConnected, handle, connected)
    }
    
    Core::hresult DeviceSettingsImp::IsVideoPortActive(const int32_t handle, bool &active) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, IsVideoPortActive, handle, active)
    }
    
    Core::hresult DeviceSettingsImp::GetVideoPortResolution(const int32_t handle, VideoPortResolution &resolution) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, GetVideoPortResolution, handle, resolution)
    }
    
    Core::hresult DeviceSettingsImp::GetColorDepth(const int32_t handle, uint32_t &colorDepth) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, GetColorDepth, handle, colorDepth)
    }
    
    Core::hresult DeviceSettingsImp::GetColorSpace(const int32_t handle, VideoPortColorSpace &colorSpace) {
        VideoPortColorSpace internalColorSpace;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetColorSpace(handle, internalColorSpace) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            colorSpace = static_cast<VideoPortColorSpace>(internalColorSpace);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::GetQuantizationRange(const int32_t handle, VideoPortQuantizationRange &quantizationRange) {
        VideoPortQuantizationRange internalRange;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetQuantizationRange(handle, internalRange) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            quantizationRange = static_cast<VideoPortQuantizationRange>(internalRange);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::GetHDCPStatusOnVideoPort(const int32_t handle, Exchange::IDeviceSettingsVideoPort::HDCPStatus &hdcpStatus) {
        VideoPortHdcpStatus internalStatus;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetVideoPortHDCPStatus(handle, internalStatus) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            hdcpStatus = static_cast<Exchange::IDeviceSettingsVideoPort::HDCPStatus>(internalStatus);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::GetHDCPProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion) {
        VideoPortHdcpProtocolVersion internalVersion;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetHDCPProtocolVersionOnVideoPort(handle, internalVersion) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            hdcpVersion = static_cast<VideoPortHdcpProtocolVersion>(internalVersion);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::GetHDCPReceiverProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion) {
        VideoPortHdcpProtocolVersion internalVersion;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetHDCPReceiverProtocolVersionOnVideoPort(handle, internalVersion) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            hdcpVersion = static_cast<VideoPortHdcpProtocolVersion>(internalVersion);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::GetHDCPCurrentProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion) {
        VideoPortHdcpProtocolVersion internalVersion;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetHDCPCurrentProtocolVersionOnVideoPort(handle, internalVersion) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            hdcpVersion = static_cast<VideoPortHdcpProtocolVersion>(internalVersion);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::IsVideoPortDisplaySurround(const int32_t handle, bool &surround) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, IsVideoPortDisplaySurround, handle, surround)
    }
    
    Core::hresult DeviceSettingsImp::GetVideoPortDisplaySurroundMode(const int32_t handle, Exchange::IDeviceSettingsVideoPort::VideoPortSurroundMode &surroundMode) {
        VideoPortSurroundMode internalSurroundMode;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetVideoPortDisplaySurroundMode(handle, internalSurroundMode) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            surroundMode = static_cast<Exchange::IDeviceSettingsVideoPort::VideoPortSurroundMode>(internalSurroundMode);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::SetVideoPortResolution(const int32_t handle, const VideoPortResolution videoPortResolution, const bool persist, const bool forceCompatibility) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, SetVideoPortResolution, handle, videoPortResolution, persist, forceCompatibility)
    }
    
    Core::hresult DeviceSettingsImp::EnableHDCPOnVideoPort(const int32_t handle, const bool hdcpEnable, const uint8_t hdcpKey[], const uint16_t hdcpKeySize) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, EnableHDCPOnVideoPort, handle, hdcpEnable, hdcpKey, hdcpKeySize)
    }
    
    Core::hresult DeviceSettingsImp::IsHDCPEnabledOnVideoPort(const int32_t handle, bool &hdcpEnabled) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, IsHDCPEnabledOnVideoPort, handle, hdcpEnabled)
    }
    
    Core::hresult DeviceSettingsImp::GetTVHDRCapabilities(const int32_t handle, int32_t &capabilities) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, GetTVHDRCapabilities, handle, capabilities)
    }
    
    Core::hresult DeviceSettingsImp::GetTVSupportedResolutions(const int32_t handle, int32_t &resolutions) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, GetTVSupportedResolutions, handle, resolutions)
    }
    
    Core::hresult DeviceSettingsImp::SetForceDisable4K(const int32_t handle, const bool disable) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, SetForceDisable4K, handle, disable)
    }
    
    Core::hresult DeviceSettingsImp::GetForceDisable4K(const int32_t handle, bool &disabled) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, GetForceDisable4K, handle, disabled)
    }
    
    Core::hresult DeviceSettingsImp::IsVideoPortOutputHDR(const int32_t handle, bool &isHDR) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, IsVideoPortOutputHDR, handle, isHDR)
    }
    
    Core::hresult DeviceSettingsImp::ResetVideoPortOutputToSDR() {
        return _videoPortSettings ? _videoPortSettings->ResetVideoPortOutputToSDR() : Core::ERROR_GENERAL;
    }
    
    Core::hresult DeviceSettingsImp::GetHDMIPreference(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion) {
        VideoPortHdcpProtocolVersion internalVersion;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetHDMIPreference(handle, internalVersion) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            hdcpVersion = static_cast<VideoPortHdcpProtocolVersion>(internalVersion);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::SetHDMIPreference(const int32_t handle, const VideoPortHdcpProtocolVersion hdcpVersion) {
        return _videoPortSettings ? _videoPortSettings->SetHDMIPreference(handle, static_cast<VideoPortHdcpProtocolVersion>(hdcpVersion)) : Core::ERROR_GENERAL;
    }
    
    Core::hresult DeviceSettingsImp::GetVideoEOTF(const int32_t handle, HDRStandard &hdrStandard) {
        HDRStandard internalHdrStandard;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetVideoEOTF(handle, internalHdrStandard) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            hdrStandard = static_cast<Exchange::IDeviceSettingsVideoPort::HDRStandard>(internalHdrStandard);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::GetMatrixCoefficients(const int32_t handle, Exchange::IDeviceSettingsVideoPort::DisplayMatrixCoefficients &matrixCoefficients) {
        DisplayMatrixCoefficients internalMatrixCoefficients;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetMatrixCoefficients(handle, internalMatrixCoefficients) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            matrixCoefficients = static_cast<Exchange::IDeviceSettingsVideoPort::DisplayMatrixCoefficients>(internalMatrixCoefficients);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::GetCurrentOutputSettings(const int32_t handle, Exchange::IDeviceSettingsVideoPort::DSOutputSettings &outputSettings) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, GetCurrentOutputSettings, handle, outputSettings)
    }
    
    Core::hresult DeviceSettingsImp::SetBackgroundColor(const int32_t handle, const Exchange::IDeviceSettingsVideoPort::VideoBackgroundColor backgroundColor) {
        return _videoPortSettings ? _videoPortSettings->SetBackgroundColor(handle, static_cast<VideoBackgroundColor>(backgroundColor)) : Core::ERROR_GENERAL;
    }
    
    Core::hresult DeviceSettingsImp::SetForceHDRMode(const int32_t handle, const Exchange::IDeviceSettingsVideoPort::HDRStandard hdrMode) {
        return _videoPortSettings ? _videoPortSettings->SetForceHDRMode(handle, static_cast<HDRStandard>(hdrMode)) : Core::ERROR_GENERAL;
    }
    
    Core::hresult DeviceSettingsImp::GetColorDepthCapabilities(const int32_t handle, uint32_t &colorDepthCapabilities) {
        DELEGATE_TO_COMPONENT(_videoPortSettings, GetColorDepthCapabilities, handle, colorDepthCapabilities)
    }
    
    Core::hresult DeviceSettingsImp::GetPreferredColorDepth(const int32_t handle, Exchange::IDeviceSettingsVideoPort::DisplayColorDepth &colorDepth, const bool persist) {
        DisplayColorDepth internalColorDepth;
        Core::hresult result = _videoPortSettings ? _videoPortSettings->GetPreferredColorDepth(handle, internalColorDepth, persist) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            colorDepth = static_cast<Exchange::IDeviceSettingsVideoPort::DisplayColorDepth>(internalColorDepth);
        }
        return result;
    }
    
    Core::hresult DeviceSettingsImp::SetPreferredColorDepth(const int32_t handle, const Exchange::IDeviceSettingsVideoPort::DisplayColorDepth colorDepth, const bool persist) {
        return _videoPortSettings ? _videoPortSettings->SetPreferredColorDepth(handle, static_cast<DisplayColorDepth>(colorDepth), persist) : Core::ERROR_GENERAL;
    }

    // IDeviceSettingsVideoDevice interface implementation - delegate to _videoDeviceSettings interface

    Core::hresult DeviceSettingsImp::Register(Exchange::IDeviceSettingsVideoDevice::INotification* notification) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, Register, notification)
    }

    Core::hresult DeviceSettingsImp::Unregister(Exchange::IDeviceSettingsVideoDevice::INotification* notification) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, Unregister, notification)
    }

    Core::hresult DeviceSettingsImp::GetVideoDeviceHandle(const int32_t index, int32_t &handle) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, GetVideoDeviceHandle, index, handle)
    }

    Core::hresult DeviceSettingsImp::SetVideoDeviceDFC(const int32_t handle, const Exchange::IDeviceSettingsVideoDevice::VideoZoom zoom) {
        return _videoDeviceSettings ? _videoDeviceSettings->SetVideoDeviceDFC(handle, static_cast<VideoZoom>(zoom)) : Core::ERROR_GENERAL;
    }

    Core::hresult DeviceSettingsImp::GetVideoDeviceDFC(const int32_t handle, Exchange::IDeviceSettingsVideoDevice::VideoZoom &zoom) {
        VideoZoom internalZoom;
        Core::hresult result = _videoDeviceSettings ? _videoDeviceSettings->GetVideoDeviceDFC(handle, internalZoom) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            zoom = static_cast<Exchange::IDeviceSettingsVideoDevice::VideoZoom>(internalZoom);
        }
        return result;
    }

    Core::hresult DeviceSettingsImp::GetHDRCapabilities(const int32_t handle, int32_t &capabilities) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, GetHDRCapabilities, handle, capabilities)
    }

    Core::hresult DeviceSettingsImp::GetSupportedVideoCodingFormats(const int32_t handle, int32_t &supportedFormats) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, GetSupportedVideoCodingFormats, handle, supportedFormats)
    }

    Core::hresult DeviceSettingsImp::SetDisplayFrameRate(const int32_t handle, const string frameRate) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, SetDisplayFrameRate, handle, frameRate)
    }

    Core::hresult DeviceSettingsImp::GetCodecInfo(const int32_t handle, const Exchange::IDeviceSettingsVideoDevice::VideoCodec videoCodec, Exchange::IDeviceSettingsVideoDevice::IDeviceSettingsVideoCodecProfileSupportIterator *&codecInfo) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, GetCodecInfo, handle, static_cast<VideoCodec>(videoCodec), codecInfo)
    }

    Core::hresult DeviceSettingsImp::DisableHDR(const int32_t handle, const bool disable) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, DisableHDR, handle, disable)
    }

    Core::hresult DeviceSettingsImp::SetFRFMode(const int32_t handle, const int32_t frfmode) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, SetFRFMode, handle, frfmode)
    }

    Core::hresult DeviceSettingsImp::GetFRFMode(const int32_t handle, int32_t &frfmode) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, GetFRFMode, handle, frfmode)
    }

    Core::hresult DeviceSettingsImp::GetCurrentDisplayFrameRate(const int32_t handle, string &framerate) {
        DELEGATE_TO_COMPONENT(_videoDeviceSettings, GetCurrentDisplayFrameRate, handle, framerate)
    }

    // ============================================================================
    // IDeviceSettingsHost interface implementation - delegate to _hostSettings interface
    // ============================================================================
    
    Core::hresult DeviceSettingsImp::Register(Exchange::IDeviceSettingsHost::INotification* notification) {
        DELEGATE_TO_COMPONENT(_hostSettings, Register, notification)
    }

    Core::hresult DeviceSettingsImp::Unregister(Exchange::IDeviceSettingsHost::INotification* notification) {
        DELEGATE_TO_COMPONENT(_hostSettings, Unregister, notification)
    }

    Core::hresult DeviceSettingsImp::GetPreferredSleepMode(Exchange::IDeviceSettingsHost::SleepMode &mode) {
        HostSleepMode internalMode;
        Core::hresult result = _hostSettings ? _hostSettings->GetPreferredSleepMode(internalMode) : Core::ERROR_GENERAL;
        if (result == Core::ERROR_NONE) {
            mode = static_cast<Exchange::IDeviceSettingsHost::SleepMode>(internalMode);
        }
        return result;
    }

    Core::hresult DeviceSettingsImp::SetPreferredSleepMode(const Exchange::IDeviceSettingsHost::SleepMode mode) {
        return _hostSettings ? _hostSettings->SetPreferredSleepMode(static_cast<HostSleepMode>(mode)) : Core::ERROR_GENERAL;
    }

    Core::hresult DeviceSettingsImp::GetCPUTemperature(float &temperature) {
        DELEGATE_TO_COMPONENT(_hostSettings, GetCPUTemperature, temperature)
    }

    Core::hresult DeviceSettingsImp::GetHALVersion(uint32_t &versionNo) {
        DELEGATE_TO_COMPONENT(_hostSettings, GetHALVersion, versionNo)
    }

    Core::hresult DeviceSettingsImp::GetSoCID(string &socID) {
        DELEGATE_TO_COMPONENT(_hostSettings, GetSoCID, socID)
    }

    Core::hresult DeviceSettingsImp::GetEDID(uint8_t edId[], const uint16_t edIdLength) {
        DELEGATE_TO_COMPONENT(_hostSettings, GetEDID, edId, edIdLength)
    }

    Core::hresult DeviceSettingsImp::GetMS12ConfigType(string &ms12Config) {
        DELEGATE_TO_COMPONENT(_hostSettings, GetMS12ConfigType, ms12Config)
    }

    // ============================================================================
    // IDeviceSettingsDisplay interface implementation - delegate to _displaySettings interface
    // ============================================================================

    Core::hresult DeviceSettingsImp::Register(IDisplayNotification* notification) {
        DELEGATE_TO_COMPONENT(_displaySettings, Register, notification)
    }

    Core::hresult DeviceSettingsImp::Unregister(IDisplayNotification* notification) {
        DELEGATE_TO_COMPONENT(_displaySettings, Unregister, notification)
    }

    Core::hresult DeviceSettingsImp::GetDisplayEdid(const int32_t handle, DisplayEDID &edId, IDSVideoPortResolutionIterator*& supportedResolutionList) {
        DELEGATE_TO_COMPONENT(_displaySettings, GetDisplayEdid, handle, edId, supportedResolutionList)
    }

    Core::hresult DeviceSettingsImp::GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength) {
        DELEGATE_TO_COMPONENT(_displaySettings, GetDisplayEdidBytes, handle, edIdBytes, edidLength)
    }

    Core::hresult DeviceSettingsImp::GetDisplay(const DisplayPortType portType, const int32_t index, int32_t &handle) {
        DELEGATE_TO_COMPONENT(_displaySettings, GetDisplay, portType, index, handle)
    }

    Core::hresult DeviceSettingsImp::Register(IDisplayHDMIHotPlugNotification* notification) {
        DELEGATE_TO_COMPONENT(_displaySettings, Register, notification)
    }

    Core::hresult DeviceSettingsImp::Unregister(IDisplayHDMIHotPlugNotification* notification) {
        DELEGATE_TO_COMPONENT(_displaySettings, Unregister, notification)
    }

    Core::hresult DeviceSettingsImp::GetDisplayAspectRatio(const int32_t handle, Exchange::IDeviceSettingsDisplay::DisplayVideoAspectRatio &aspectRatio) {
        DELEGATE_TO_COMPONENT(_displaySettings, GetDisplayAspectRatio, handle, aspectRatio)
    }

    Core::hresult DeviceSettingsImp::SetAllmEnabled(const int32_t handle, const bool enabled) {
        DELEGATE_TO_COMPONENT(_displaySettings, SetAllmEnabled, handle, enabled)
    }

    Core::hresult DeviceSettingsImp::SetAVIContentType(const int32_t handle, const DisplayAVIContentType contentType) {
        DELEGATE_TO_COMPONENT(_displaySettings, SetAVIContentType, handle, contentType)
    }

    Core::hresult DeviceSettingsImp::SetAVIScanInformation(const int32_t handle, const DisplayAVIScanInformation scanInfo) {
        DELEGATE_TO_COMPONENT(_displaySettings, SetAVIScanInformation, handle, scanInfo)
    }

    // ============================================================================
    // IDeviceSettingsCompositeIn interface implementation - delegate to _compositeInSettings interface
    // ============================================================================

    Core::hresult DeviceSettingsImp::Register(Exchange::IDeviceSettingsCompositeIn::INotification* notification) {
        DELEGATE_TO_COMPONENT(_compositeInSettings, Register, notification)
    }

    Core::hresult DeviceSettingsImp::Unregister(Exchange::IDeviceSettingsCompositeIn::INotification* notification) {
        DELEGATE_TO_COMPONENT(_compositeInSettings, Unregister, notification)
    }

    Core::hresult DeviceSettingsImp::GetNrOfCompositeInputs(int32_t &nrCompositeInputs) {
        DELEGATE_TO_COMPONENT(_compositeInSettings, GetNrOfCompositeInputs, nrCompositeInputs)
    }

    Core::hresult DeviceSettingsImp::GetCompositeInStatus(CompositeInStatus &status) {
        DELEGATE_TO_COMPONENT(_compositeInSettings, GetCompositeInStatus, status)
    }

    Core::hresult DeviceSettingsImp::SelectCompositeInPort(const CompositeInPort port) {
        DELEGATE_TO_COMPONENT(_compositeInSettings, SelectCompositeInPort, port)
    }

    Core::hresult DeviceSettingsImp::ScaleCompositeInVideo(const CompositeInVideoRectangle videoRect) {
        DELEGATE_TO_COMPONENT(_compositeInSettings, ScaleCompositeInVideo, videoRect)
    }

    // Static instance method implementation
    DeviceSettingsImp* DeviceSettingsImp::instance(DeviceSettingsImp* DeviceSettingsImpl)
    {
        if (DeviceSettingsImpl != nullptr) {
            _instance = DeviceSettingsImpl;
        }
        return _instance;
    }

} // namespace Plugin
} // namespace WPEFramework
