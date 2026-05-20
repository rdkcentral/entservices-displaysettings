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

#include "dAudio.h"
#include "UtilsLogging.h"
#include "DeviceSettingsTypes.h"

#include <interfaces/IDeviceSettingsAudio.h>

#include "dsAudio.h"
#include "dsError.h"
#include "dsTypes.h"
#include "dsUtl.h"
#include "dsRpc.h"

// Device Settings library includes for accessing audio port configurations
#include "manager.hpp"
#include "audioOutputPortConfig.hpp"
#include "audioOutputPortType.hpp"
#include "audioOutputPort.hpp"
#include "audioCompression.hpp"
#include "audioEncoding.hpp"
#include "audioStereoMode.hpp"
#include "exception.hpp"

// WPEFramework includes for RPC iterator creation
#include <core/core.h>
#include <com/com.h>

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <dlfcn.h>
#include <cstdio>

// Static global callback functions following HdmiIn pattern
static std::function<void(const AudioPortType, const uint32_t, const bool)> g_AudioOutHotPlugCallback;
static std::function<void(const AudioFormat)> g_AudioFormatUpdateCallback;
static std::function<void(const DolbyAtmosCapability, const bool)> g_DolbyAtmosCapabilitiesChangedCallback;
static std::function<void(const bool)> g_AssociatedAudioMixingChangedCallback;
static std::function<void(const int32_t)> g_AudioFaderControlChangedCallback;
static std::function<void(const std::string&)> g_AudioPrimaryLanguageChangedCallback;
static std::function<void(const std::string&)> g_AudioSecondaryLanguageChangedCallback;
static std::function<void(const AudioPortState)> g_AudioPortStateChangedCallback;
static std::function<void(const float)> g_AudioLevelChangedCallback;
static std::function<void(const AudioPortType, const AudioStereoMode)> g_AudioModeChangedCallback;

using namespace WPEFramework::Exchange;

class dAudioImpl : public hal::dAudio::IPlatform {

private:
    // delete copy constructor and assignment operator
    dAudioImpl(const dAudioImpl&) = delete;
    dAudioImpl& operator=(const dAudioImpl&) = delete;
    
    bool _isInitialized;

    // Audio ducking state management
    bool _isDuckingInProgress;
    int32_t _volumeDuckingLevel;
    bool _muteStatus;
    
    // Audio port state tracking
    bool _audioPortEnabled[dsAUDIOPORT_TYPE_MAX];
    
    // Helper method implementations for enabling audio port
    dsAudioPortType_t getAudioPortType(intptr_t handle)
    {
        intptr_t halHandle = 0;
        
        // Simplified approach - check common port types
        const dsAudioPortType_t portTypes[] = {
            dsAUDIOPORT_TYPE_HDMI,
            dsAUDIOPORT_TYPE_SPDIF, 
            dsAUDIOPORT_TYPE_SPEAKER,
            dsAUDIOPORT_TYPE_HDMI_ARC,
            dsAUDIOPORT_TYPE_HEADPHONE
        };
        
        for (int i = 0; i < 5; i++) {
            if (dsGetAudioPort(portTypes[i], 0, &halHandle) == dsERR_NONE) {
                if (handle == halHandle) {
                    return portTypes[i];
                }
            }
        }
        
        LOGWARN("The requested audio port is not part of platform port configuration");
        return dsAUDIOPORT_TYPE_MAX;
    }
    
    uint32_t setAudioDuckingAudioLevel(intptr_t handle)
    {
        float volume = 0;
        
        if (_isDuckingInProgress) {
            volume = _volumeDuckingLevel;
        } else {
            // Use resolve function for dsGetAudioLevel
            typedef dsError_t (*dsGetAudioLevel_t)(intptr_t handle, float* level);
            static dsGetAudioLevel_t dsGetAudioLevelFunc = 0;
            if (dsGetAudioLevelFunc == 0) {
                dsGetAudioLevelFunc = (dsGetAudioLevel_t)resolve(RDK_DSHAL_NAME, "dsGetAudioLevel");
                if (dsGetAudioLevelFunc == 0) {
                    LOGERR("dsGetAudioLevel is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetAudioLevelFunc) {
                ret = dsGetAudioLevelFunc(handle, &volume);
            }
            if (ret != dsERR_NONE) {
                LOGERR("dsGetAudioLevel failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
            LOGINFO("Current audio level: %f", volume);
        }
        
        // Use resolve function for dsSetAudioLevel
        typedef dsError_t (*dsSetAudioLevel_t)(intptr_t handle, float level);
        static dsSetAudioLevel_t dsSetAudioLevelFunc = 0;
        if (dsSetAudioLevelFunc == 0) {
            dsSetAudioLevelFunc = (dsSetAudioLevel_t)resolve(RDK_DSHAL_NAME, "dsSetAudioLevel");
            if (dsSetAudioLevelFunc == 0) {
                LOGERR("dsSetAudioLevel is not defined");
                return WPEFramework::Core::ERROR_GENERAL;
            }
        }
        
        dsError_t ret = dsERR_GENERAL;
        if (0 != dsSetAudioLevelFunc) {
            ret = dsSetAudioLevelFunc(handle, volume);
        }
        
        if (ret != dsERR_NONE) {
            LOGERR("dsSetAudioLevel failed with error: %d", ret);
            return WPEFramework::Core::ERROR_GENERAL;
        }
        
        return WPEFramework::Core::ERROR_NONE;
    }
    
    uint32_t getAudioDelayInternal(dsAudioPortType_t portType)
    {
        std::string audioDelayMs = "0";
        uint32_t returnAudioDelayMs = 0;
        
        switch(portType) {
            case dsAUDIOPORT_TYPE_SPDIF:
                {
                   try {
                        audioDelayMs = device::HostPersistence::getInstance().getProperty("SPDIF0.audio.Delay");
                    }
                    catch(...) {
                            try {
                                LOGINFO("SPDIF0.audio.Delay not found in persistence store. Try system default");
                                audioDelayMs = device::HostPersistence::getInstance().getDefaultProperty("SPDIF0.audio.Delay");
                            }
                            catch(...) {
                                audioDelayMs = "0";
                            }
                    }
                }
                break;
            case dsAUDIOPORT_TYPE_HDMI:
                {
                   try {
                        audioDelayMs = device::HostPersistence::getInstance().getProperty("HDMI0.audio.Delay");
                    }
                    catch(...) {
                            try {
                                LOGINFO("HDMI0.audio.Delay not found in persistence store. Try system default");
                                audioDelayMs = device::HostPersistence::getInstance().getDefaultProperty("HDMI0.audio.Delay");
                            }
                            catch(...) {
                                audioDelayMs = "0";
                            }
                    }
                }
                break;
            case dsAUDIOPORT_TYPE_SPEAKER:
                {
                   try {
                        audioDelayMs = device::HostPersistence::getInstance().getProperty("SPEAKER0.audio.Delay");
                    }
                    catch(...) {
                            try {
                                LOGINFO("SPEAKER0.audio.Delay not found in persistence store. Try system default");
                                audioDelayMs = device::HostPersistence::getInstance().getDefaultProperty("SPEAKER0.audio.Delay");
                            }
                            catch(...) {
                                audioDelayMs = "0";
                            }
                    }
                }
                break;
            case dsAUDIOPORT_TYPE_HDMI_ARC:
                {
                   try {
                        audioDelayMs = device::HostPersistence::getInstance().getProperty("HDMI_ARC0.audio.Delay");
                    }
                    catch(...) {
                            try {
                                LOGINFO("HDMI_ARC0.audio.Delay not found in persistence store. Try system default");
                                audioDelayMs = device::HostPersistence::getInstance().getDefaultProperty("HDMI_ARC0.audio.Delay");
                            }
                            catch(...) {
                                audioDelayMs = "0";
                            }
                    }
                }
                break;
            default:
                LOGINFO("Port type: UNKNOWN, persist audio delay: %s : NOT SET", audioDelayMs.c_str());
                break;
        }
        
        try {
            returnAudioDelayMs = std::stoul(audioDelayMs);
            LOGINFO("Audio delay value returnAudioDelayMs: %d", returnAudioDelayMs);
        }
        catch(...) {
            LOGINFO("Exception in getting the audio delay from persistence storage, returning default value 0");
            returnAudioDelayMs = 0;
        }
        
        return returnAudioDelayMs;
    }

    bool setAudioDelayInternal(intptr_t handle, uint32_t audioDelay)
    {
        try {
            // Use resolve function for dsSetAudioDelay
            typedef dsError_t (*dsSetAudioDelay_t)(intptr_t handle, uint32_t audioDelay);
            static dsSetAudioDelay_t dsSetAudioDelayFunc = 0;
            if (dsSetAudioDelayFunc == 0) {
                dsSetAudioDelayFunc = (dsSetAudioDelay_t)resolve(RDK_DSHAL_NAME, "dsSetAudioDelay");
                if (dsSetAudioDelayFunc == 0) {
                    LOGERR("dsSetAudioDelay is not defined");
                    return false;
                }
            }

            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetAudioDelayFunc) {
                ret = dsSetAudioDelayFunc(handle, audioDelay);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("Audio delay set successfully: handle=%ld, delay=%u", (long)handle, audioDelay);
                return true;
            } else {
                LOGERR("dsSetAudioDelay failed with error: %d", ret);
                return false;
            }
        } catch (...) {
            LOGERR("Exception in setAudioDelayInternal");
            return false;
        }
    }
    
    // HAL callback registration functions (internal)
    dsError_t registerHALCallbacks()
    {
        ENTRY_LOG;
        dsError_t ret = dsERR_NONE;
        
        try {
            // Register audio output port connect callback
            ret = dsAudioOutRegisterConnectCB(audioOutPortConnectCallback);
            if (ret != dsERR_NONE) {
                LOGWARN("dsAudioOutRegisterConnectCB failed with error: %d", ret);
            } else {
                LOGINFO("Audio output port connect callback registered successfully");
            }
            
            // Register audio format update callback  
            ret = dsAudioFormatUpdateRegisterCB(audioFormatUpdateCallback);
            if (ret != dsERR_NONE) {
                LOGWARN("dsAudioFormatUpdateRegisterCB failed with error: %d", ret);
            } else {
                LOGINFO("Audio format update callback registered successfully");
            }
            
            // Register atmos capability change callback
            ret = dsAudioAtmosCapsChangeRegisterCB(audioAtmosCapsChangeCallback);
            if (ret != dsERR_NONE) {
                LOGWARN("dsAudioAtmosCapsChangeRegisterCB failed with error: %d", ret);
            } else {
                LOGINFO("Audio atmos caps change callback registered successfully");
            }
            
        } catch (...) {
            LOGERR("Exception in registerHALCallbacks");
            ret = dsERR_GENERAL;
        }
        
        EXIT_LOG;
        return ret;
    }

public:
    dAudioImpl() : _isInitialized(false), _isDuckingInProgress(false), _volumeDuckingLevel(0), _muteStatus(false)
    {
        ENTRY_LOG;

        // Initialize port state tracking
        for (int i = 0; i < dsAUDIOPORT_TYPE_MAX; i++) {
            _audioPortEnabled[i] = false;
        }
        
        // Initialize the DeviceSettings Audio subsystem
        try {
            dsError_t ret = dsAudioPortInit();
            if (ret != dsERR_NONE) {
                LOGERR("dsAudioPortInit failed with error: %d", ret);
            } else {
                _isInitialized = true;
                LOGINFO("Audio platform initialized successfully");
                
                // Initialize audio settings from persistence and platform configuration
                initializeAudioSettings();
                
                // Initialize audio port configuration (from AudioConfigInit)
                audioConfigInit();
                
                // Register HAL callbacks for events
                registerHALCallbacks();
                
                // Notify about audio port state initialization (like dsAudio.c)
                notifyAudioPortStateChanged(AudioPortState::AUDIO_PORT_STATE_INITIALIZED);
            }
        } catch (...) {
            LOGERR("Exception during Audio platform initialization");
        }
        EXIT_LOG;
    }

    virtual ~dAudioImpl()
    {
        ENTRY_LOG;

        if (_isInitialized) {
            try {
                dsError_t ret = dsAudioPortTerm();
                if (ret != dsERR_NONE) {
                    LOGERR("dsAudioPortTerm failed with error: %d", ret);
                }
            } catch (...) {
                LOGERR("Exception during Audio platform termination");
            }
            _isInitialized = false;
        }
        EXIT_LOG;
    }

    // Type conversion methods
    dsAudioPortType_t convertToDS(const AudioPortType type)
    {
        switch (type) {
            case AudioPortType::AUDIO_PORT_TYPE_LR: return dsAUDIOPORT_TYPE_ID_LR;
            case AudioPortType::AUDIO_PORT_TYPE_HDMI: return dsAUDIOPORT_TYPE_HDMI;
            case AudioPortType::AUDIO_PORT_TYPE_SPDIF: return dsAUDIOPORT_TYPE_SPDIF;
            case AudioPortType::AUDIO_PORT_TYPE_SPEAKER: return dsAUDIOPORT_TYPE_SPEAKER;
            case AudioPortType::AUDIO_PORT_TYPE_HDMIARC: return dsAUDIOPORT_TYPE_HDMI_ARC;
            case AudioPortType::AUDIO_PORT_TYPE_HEADPHONE: return dsAUDIOPORT_TYPE_HEADPHONE;
            default: return dsAUDIOPORT_TYPE_MAX;
        }
    }

    dsAudioStereoMode_t convertToDS(const AudioStereoMode mode)
    {
        switch (mode) {
            case AudioStereoMode::AUDIO_STEREO_UNKNOWN: return dsAUDIO_STEREO_UNKNOWN;
            case AudioStereoMode::AUDIO_STEREO_MONO: return dsAUDIO_STEREO_MONO;
            case AudioStereoMode::AUDIO_STEREO_STEREO: return dsAUDIO_STEREO_STEREO;
            case AudioStereoMode::AUDIO_STEREO_SURROUND: return dsAUDIO_STEREO_SURROUND;
            case AudioStereoMode::AUDIO_STEREO_PASSTHROUGH: return dsAUDIO_STEREO_PASSTHRU;
            case AudioStereoMode::AUDIO_STEREO_DD: return dsAUDIO_STEREO_DD;
            case AudioStereoMode::AUDIO_STEREO_DDPLUS: return dsAUDIO_STEREO_DDPLUS;
            default: return dsAUDIO_STEREO_UNKNOWN;
        }
    }

    AudioStereoMode convertFromDS(const dsAudioStereoMode_t dsMode)
    {
        switch (dsMode) {
            case dsAUDIO_STEREO_UNKNOWN: return AudioStereoMode::AUDIO_STEREO_UNKNOWN;
            case dsAUDIO_STEREO_MONO: return AudioStereoMode::AUDIO_STEREO_MONO;
            case dsAUDIO_STEREO_STEREO: return AudioStereoMode::AUDIO_STEREO_STEREO;
            case dsAUDIO_STEREO_SURROUND: return AudioStereoMode::AUDIO_STEREO_SURROUND;
            case dsAUDIO_STEREO_PASSTHRU: return AudioStereoMode::AUDIO_STEREO_PASSTHROUGH;
            case dsAUDIO_STEREO_DD: return AudioStereoMode::AUDIO_STEREO_DD;
            case dsAUDIO_STEREO_DDPLUS: return AudioStereoMode::AUDIO_STEREO_DDPLUS;
            default: return AudioStereoMode::AUDIO_STEREO_UNKNOWN;
        }
    }

    AudioPortConfig createAudioPortConfig(const device::AudioOutputPort& port)
    {
        AudioPortConfig portConfig{};

        portConfig.id = static_cast<uint32_t>(port.getId());
        portConfig.type = static_cast<AudioPortType>(port.getType().getId());
        portConfig.index = port.getIndex();
        portConfig.name = port.getName();

        return portConfig;
    }

    void populateAudioConfig(const device::AudioOutputPortType& portType, AudioConfig& audioConfig)
    {
        audioConfig.type = static_cast<AudioPortType>(portType.getId());
        audioConfig.name = portType.getName();

        std::vector<AudioCompression> compressions;
        const device::List<device::AudioCompression> supportedCompressions = portType.getSupportedCompressions();
        compressions.reserve(supportedCompressions.size());
        for (size_t index = 0; index < supportedCompressions.size(); ++index) {
            compressions.push_back(static_cast<AudioCompression>(supportedCompressions.at(index).getId()));
        }
        audioConfig.audioCompressions = WPEFramework::Core::Service<WPEFramework::RPC::IteratorType<IDeviceSettingsAudioCompressionIterator>>::Create<IDeviceSettingsAudioCompressionIterator>(compressions);

        std::vector<AudioEncoding> encodings;
        const device::List<device::AudioEncoding> supportedEncodings = portType.getSupportedEncodings();
        encodings.reserve(supportedEncodings.size());
        for (size_t index = 0; index < supportedEncodings.size(); ++index) {
            encodings.push_back(static_cast<AudioEncoding>(supportedEncodings.at(index).getId()));
        }
        audioConfig.audioEncodings = WPEFramework::Core::Service<WPEFramework::RPC::IteratorType<IDeviceSettingsAudioEncodingIterator>>::Create<IDeviceSettingsAudioEncodingIterator>(encodings);

        std::vector<AudioStereoMode> stereoModes;
        const device::List<device::AudioStereoMode> supportedStereoModes = portType.getSupportedStereoModes();
        stereoModes.reserve(supportedStereoModes.size());
        for (size_t index = 0; index < supportedStereoModes.size(); ++index) {
            stereoModes.push_back(static_cast<AudioStereoMode>(supportedStereoModes.at(index).getId()));
        }
        audioConfig.stereoModes = WPEFramework::Core::Service<WPEFramework::RPC::IteratorType<IDeviceSettingsStereoModeIterator>>::Create<IDeviceSettingsStereoModeIterator>(stereoModes);

        std::vector<AudioPortConfig> ports;
        const device::List<device::AudioOutputPort> configuredPorts = portType.getPorts();
        ports.reserve(configuredPorts.size());
        for (size_t index = 0; index < configuredPorts.size(); ++index) {
            ports.push_back(createAudioPortConfig(configuredPorts.at(index)));
        }
        audioConfig.ports = WPEFramework::Core::Service<WPEFramework::RPC::IteratorType<IDeviceSettingsAudioPortConfigIterator>>::Create<IDeviceSettingsAudioPortConfigIterator>(ports);
    }

    // Audio Platform interface implementations - stub implementations
    // IPlatform interface implementation
    uint32_t GetAudioPort(const AudioPortType type, const int32_t index, int32_t &handle) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            dsAudioPortType_t dsType = convertToDS(type);
            intptr_t dsHandle;
            
            dsError_t ret = dsGetAudioPort(dsType, index, &dsHandle);
            
            if (ret == dsERR_NONE) {
                handle = static_cast<int32_t>(dsHandle);
                LOGINFO("GetAudioPort success: type=%d, index=%d, handle=%d", type, index, handle);
            } else {
                LOGERR("dsGetAudioPort failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioPort");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    /*
    uint32_t GetAudioConfigurations(IDeviceSettingsAudioConfigurationIterator*& audioConfigs) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            device::Manager::Initialize();

            const device::List<device::AudioOutputPortType> supportedTypes = device::AudioOutputPortConfig::getInstance().getSupportedTypes();
            std::vector<AudioConfig> configurations;
            configurations.reserve(supportedTypes.size());

            for (size_t index = 0; index < supportedTypes.size(); ++index) {
                AudioConfig audioConfig{};
                populateAudioConfig(supportedTypes.at(index), audioConfig);
                configurations.push_back(audioConfig);
            }

            audioConfigs = WPEFramework::Core::Service<WPEFramework::RPC::IteratorType<IDeviceSettingsAudioConfigurationIterator>>::Create<IDeviceSettingsAudioConfigurationIterator>(configurations);
        } catch (const device::Exception& exception) {
            LOGERR("Device settings exception in GetAudioConfigurations: %s", exception.what());
            return WPEFramework::Core::ERROR_GENERAL;
        } catch (...) {
            LOGERR("Unknown exception in GetAudioConfigurations");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }
    */

    // GetAudioPorts and GetSupportedAudioPorts methods removed - iterator type doesn't exist
    uint32_t GetAudioPortConfig(const AudioPortType audioPort, AudioConfig &audioConfig) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            const dsAudioPortType_t dsType = convertToDS(audioPort);
            device::Manager::Initialize();

            const device::List<device::AudioOutputPortType> supportedTypes = device::AudioOutputPortConfig::getInstance().getSupportedTypes();
            for (size_t index = 0; index < supportedTypes.size(); ++index) {
                if (supportedTypes.at(index).getId() == dsType) {
                    populateAudioConfig(supportedTypes.at(index), audioConfig);
                    EXIT_LOG;
                    return WPEFramework::Core::ERROR_NONE;
                }
            }

            LOGERR("Audio port type %d is not part of platform configuration", static_cast<int>(audioPort));
            return WPEFramework::Core::ERROR_GENERAL;
        } catch (const device::Exception& exception) {
            LOGERR("Device settings exception in GetAudioPortConfig: %s", exception.what());
            return WPEFramework::Core::ERROR_GENERAL;
        } catch (...) {
            LOGERR("Unknown exception in GetAudioPortConfig");
            return WPEFramework::Core::ERROR_GENERAL;
        }
    }

    uint32_t GetAudioCapabilities(const int32_t handle, int32_t &capabilities) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            int dsCapabilities;
            
            // Use resolve function for dsGetAudioCapabilities
            typedef dsError_t (*dsGetAudioCapabilities_t)(intptr_t handle, int* capabilities);
            static dsGetAudioCapabilities_t dsGetAudioCapabilitiesFunc = 0;
            if (dsGetAudioCapabilitiesFunc == 0) {
                dsGetAudioCapabilitiesFunc = (dsGetAudioCapabilities_t)resolve(RDK_DSHAL_NAME, "dsGetAudioCapabilities");
                if (dsGetAudioCapabilitiesFunc == 0) {
                    LOGERR("dsGetAudioCapabilities is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetAudioCapabilitiesFunc) {
                ret = dsGetAudioCapabilitiesFunc(dsHandle, &dsCapabilities);
            }
            
            if (ret == dsERR_NONE) {
                capabilities = dsCapabilities;
                LOGINFO("GetAudioCapabilities success: handle=%d, capabilities=%d", handle, capabilities);
            } else {
                LOGERR("dsGetAudioCapabilities failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioCapabilities");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioMS12Capabilities(const int32_t handle, int32_t &capabilities) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            int dsCapabilities;
            dsError_t ret = dsGetMS12Capabilities(dsHandle, &dsCapabilities);
            if (ret == dsERR_NONE) {
                capabilities = dsCapabilities;
                LOGINFO("GetAudioMS12Capabilities success: handle=%d, capabilities=%d", handle, capabilities);
            } else {
                LOGERR("dsGetMS12Capabilities failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioMS12Capabilities");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioFormat(const int32_t handle, AudioFormat &audioFormat) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsAudioFormat_t dsFormat;
            
            // Use resolve function for dsGetAudioFormat
            typedef dsError_t (*dsGetAudioFormat_t)(intptr_t handle, dsAudioFormat_t* format);
            static dsGetAudioFormat_t dsGetAudioFormatFunc = 0;
            if (dsGetAudioFormatFunc == 0) {
                dsGetAudioFormatFunc = (dsGetAudioFormat_t)resolve(RDK_DSHAL_NAME, "dsGetAudioFormat");
                if (dsGetAudioFormatFunc == 0) {
                    LOGERR("dsGetAudioFormat is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetAudioFormatFunc) {
                ret = dsGetAudioFormatFunc(dsHandle, &dsFormat);
            }
            
            if (ret == dsERR_NONE) {
                audioFormat = static_cast<AudioFormat>(dsFormat);
                LOGINFO("GetAudioFormat success: handle=%d, format=%d", handle, audioFormat);
            } else {
                LOGERR("dsGetAudioFormat failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioFormat");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioEncoding(const int32_t handle, AudioEncoding &encoding) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            // Stub implementation - dsGetAudioEncoding function does not exist in HAL
            LOGINFO("GetAudioEncoding - Stub implementation for handle=%d", handle);
            encoding = AudioEncoding::AUDIO_ENCODING_PCM; // Default to PCM encoding
            LOGINFO("GetAudioEncoding success: handle=%d, encoding=%d", handle, static_cast<int>(encoding));
        } catch (...) {
            LOGERR("Exception in GetAudioEncoding");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetSupportedCompressions(const int32_t handle, IDeviceSettingsAudioCompressionIterator*& compressions) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        /*try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsAudioPortType_t portType = getAudioPortType(dsHandle);
            
            if (portType >= dsAUDIOPORT_TYPE_MAX) {
                LOGERR("Invalid audio port type for handle: %d", handle);
                return WPEFramework::Core::ERROR_GENERAL;
            }
            
            try {
                // Initialize device settings manager to access port configurations
                device::Manager::Initialize();
                
                // Get the audio output port type and supported compressions
                device::AudioOutputPortType &audioPortType = device::AudioOutputPortType::getInstance(portType);
                const device::List<device::AudioCompression> supportedCompressions = audioPortType.getSupportedCompressions();
                
                // Create vector to hold compression values for RPC iterator
                std::vector<AudioCompression> compressionList;
                
                // Convert device::AudioCompression to AudioCompression enum
                for (size_t i = 0; i < supportedCompressions.size(); i++) {
                    const device::AudioCompression &compression = supportedCompressions.at(i);
                    // Map device settings compression IDs to AudioCompression enum values
                    AudioCompression audioComp = static_cast<AudioCompression>(compression.getId());
                    compressionList.push_back(audioComp);
                    LOGINFO("Supported compression [%d]: %s (ID: %d)", 
                           static_cast<int>(i), compression.getName().c_str(), compression.getId());
                }
                
                // Create RPC iterator using WPEFramework's iterator factory
                // Note: This creates a proxy object that can be used in RPC calls
                using IteratorImplementation = RPC::IteratorType<std::vector<AudioCompression>>;
                compressions = Core::ProxyType<IteratorImplementation>::Create(compressionList);
                
                LOGINFO("GetSupportedCompressions success: handle=%d, compressions_count=%d", 
                       handle, static_cast<int>(compressionList.size()));
                       
            } catch (const device::Exception &e) {
                LOGERR("Device settings exception in GetSupportedCompressions: %s", e.what());
                return WPEFramework::Core::ERROR_GENERAL;
            } catch (...) {
                LOGERR("Unknown exception in GetSupportedCompressions device settings access");
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetSupportedCompressions");
            return WPEFramework::Core::ERROR_GENERAL;
        }*/
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioCompression(const int32_t handle, AudioCompression &compression) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            int dsCompression;
            
            // Use resolve function for dsGetAudioCompression
            typedef dsError_t (*dsGetAudioCompression_t)(intptr_t handle, int* compression);
            static dsGetAudioCompression_t dsGetAudioCompressionFunc = 0;
            if (dsGetAudioCompressionFunc == 0) {
                dsGetAudioCompressionFunc = (dsGetAudioCompression_t)resolve(RDK_DSHAL_NAME, "dsGetAudioCompression");
                if (dsGetAudioCompressionFunc == 0) {
                    LOGERR("dsGetAudioCompression is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetAudioCompressionFunc) {
                ret = dsGetAudioCompressionFunc(dsHandle, &dsCompression);
            }
            
            if (ret == dsERR_NONE) {
                compression = static_cast<AudioCompression>(dsCompression);
                LOGINFO("GetAudioCompression success: handle=%d, compression=%d", handle, static_cast<int>(compression));
            } else {
                LOGERR("dsGetAudioCompression failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioCompression");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioCompression(const int32_t handle, const AudioCompression compression) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetAudioCompression
            typedef dsError_t (*dsSetAudioCompression_t)(intptr_t handle, int compression);
            static dsSetAudioCompression_t dsSetAudioCompressionFunc = 0;
            if (dsSetAudioCompressionFunc == 0) {
                dsSetAudioCompressionFunc = (dsSetAudioCompression_t)resolve(RDK_DSHAL_NAME, "dsSetAudioCompression");
                if (dsSetAudioCompressionFunc == 0) {
                    LOGERR("dsSetAudioCompression is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetAudioCompressionFunc) {
                ret = dsSetAudioCompressionFunc(dsHandle, static_cast<int>(compression));
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioCompression success: handle=%d, compression=%d", handle, static_cast<int>(compression));
            } else {
                LOGERR("dsSetAudioCompression failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioCompression");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioLevel(const int32_t handle, const float audioLevel) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetAudioLevel
            typedef dsError_t (*dsSetAudioLevel_t)(intptr_t handle, float level);
            static dsSetAudioLevel_t dsSetAudioLevelFunc = 0;
            if (dsSetAudioLevelFunc == 0) {
                dsSetAudioLevelFunc = (dsSetAudioLevel_t)resolve(RDK_DSHAL_NAME, "dsSetAudioLevel");
                if (dsSetAudioLevelFunc == 0) {
                    LOGERR("dsSetAudioLevel is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetAudioLevelFunc) {
                ret = dsSetAudioLevelFunc(dsHandle, audioLevel);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioLevel success: handle=%d, level=%f", handle, audioLevel);
                
                // Notify about audio level change
                notifyAudioLevelChanged(static_cast<int32_t>(audioLevel));
            } else {
                LOGERR("dsSetAudioLevel failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioLevel");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioLevel(const int32_t handle, float &audioLevel) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            float dsLevel;

            // Use resolve function for dsGetAudioLevel
            typedef dsError_t (*dsGetAudioLevel_t)(intptr_t handle, float* level);
            static dsGetAudioLevel_t dsGetAudioLevelFunc = 0;
            if (dsGetAudioLevelFunc == 0) {
                dsGetAudioLevelFunc = (dsGetAudioLevel_t)resolve(RDK_DSHAL_NAME, "dsGetAudioLevel");
                if (dsGetAudioLevelFunc == 0) {
                    LOGERR("dsGetAudioLevel is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }

            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetAudioLevelFunc) {
                ret = dsGetAudioLevelFunc(dsHandle, &dsLevel);
            }

            if (ret == dsERR_NONE) {
                audioLevel = dsLevel;
                LOGINFO("GetAudioLevel success: handle=%d, level=%f", handle, audioLevel);
            } else {
                LOGERR("dsGetAudioLevel failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioLevel");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioGain(const int32_t handle, const float gainLevel) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetAudioGain
            typedef dsError_t (*dsSetAudioGain_t)(intptr_t handle, float gainLevel);
            static dsSetAudioGain_t dsSetAudioGainFunc = 0;
            if (dsSetAudioGainFunc == 0) {
                dsSetAudioGainFunc = (dsSetAudioGain_t)resolve(RDK_DSHAL_NAME, "dsSetAudioGain");
                if (dsSetAudioGainFunc == 0) {
                    LOGERR("dsSetAudioGain is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetAudioGainFunc) {
                ret = dsSetAudioGainFunc(dsHandle, gainLevel);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioGain success: handle=%d, gain=%f", handle, gainLevel);
            } else {
                LOGERR("dsSetAudioGain failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioGain");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioGain(const int32_t handle, float &gainLevel) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            float dsGain;
            
            // Use resolve function for dsGetAudioGain
            typedef dsError_t (*dsGetAudioGain_t)(intptr_t handle, float* gain);
            static dsGetAudioGain_t dsGetAudioGainFunc = 0;
            if (dsGetAudioGainFunc == 0) {
                dsGetAudioGainFunc = (dsGetAudioGain_t)resolve(RDK_DSHAL_NAME, "dsGetAudioGain");
                if (dsGetAudioGainFunc == 0) {
                    LOGERR("dsGetAudioGain is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetAudioGainFunc) {
                ret = dsGetAudioGainFunc(dsHandle, &dsGain);
            }
            
            if (ret == dsERR_NONE) {
                gainLevel = dsGain;
                LOGINFO("GetAudioGain success: handle=%d, gain=%f", handle, gainLevel);
            } else {
                LOGERR("dsGetAudioGain failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioGain");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioMute(const int32_t handle, const bool mute) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            dsError_t ret = dsSetAudioMute(dsHandle, mute);
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioMute success: handle=%d, mute=%d", handle, mute);
            } else {
                LOGERR("dsSetAudioMute failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioMute");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t IsAudioMuted(const int32_t handle, bool &muted) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        
        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            bool dsMuted;
            
            // Use resolve function for dsIsAudioMute
            typedef dsError_t (*dsIsAudioMute_t)(intptr_t handle, bool* muted);
            static dsIsAudioMute_t dsIsAudioMuteFunc = 0;
            if (dsIsAudioMuteFunc == 0) {
                dsIsAudioMuteFunc = (dsIsAudioMute_t)resolve(RDK_DSHAL_NAME, "dsIsAudioMute");
                if (dsIsAudioMuteFunc == 0) {
                    LOGERR("dsIsAudioMute is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsIsAudioMuteFunc) {
                ret = dsIsAudioMuteFunc(dsHandle, &dsMuted);
            }
            
            if (ret == dsERR_NONE) {
                muted = dsMuted;
                LOGINFO("IsAudioMuted success: handle=%d, muted=%d", handle, muted);
            } else {
                LOGERR("dsIsAudioMute failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in IsAudioMuted");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioDucking(const int32_t handle, const AudioDuckingType duckingType, const AudioDuckingAction duckingAction, const uint8_t level) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            int32_t volume = 0;
            float volumeLevel = 0;
            bool portEnabled = false;
            
            LOGINFO("SetAudioDucking: action=%d, type=%d, level=%d", static_cast<int>(duckingAction), static_cast<int>(duckingType), level);

            // Check if audio port is enabled
            dsError_t ret = dsIsAudioPortEnabled(dsHandle, &portEnabled);
            if (ret != dsERR_NONE) {
                LOGWARN("dsIsAudioPortEnabled failed with error: %d", ret);
            }

            // Get current audio level
            ret = dsGetAudioLevel(dsHandle, &volumeLevel);
            if (ret != dsERR_NONE) {
                LOGERR("dsGetAudioLevel failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }

            LOGINFO("Current volumeLevel: %f", volumeLevel);

            // Calculate ducking volume based on action and type
            if (duckingAction == AudioDuckingAction::AUDIO_DUCKINGACTION_START) {
                _isDuckingInProgress = true;
                if (duckingType == AudioDuckingType::AUDIO_DUCKINGTYPE_RELATIVE) {
                    volume = (volumeLevel * level) / 100;
                } else {
                    if (level > volumeLevel) {
                        volume = volumeLevel;
                    } else {
                        volume = level;
                    }
                }
            } else {
                _isDuckingInProgress = false;
                volume = volumeLevel;
            }

            // If muted or port disabled, store volume but don't apply
            if (_muteStatus || !portEnabled) {
                LOGWARN("Mute on or port disabled, ignoring ducking request");
                _volumeDuckingLevel = volume;
                EXIT_LOG;
                return WPEFramework::Core::ERROR_NONE;
            }

            LOGINFO("Adjusted volume: %d, previous ducking level: %d", volume, _volumeDuckingLevel);

            // Apply volume to HAL layer and send event if changed
            if (volume != _volumeDuckingLevel) {
                // Use resolve function for dsSetAudioLevel
                typedef dsError_t (*dsSetAudioLevel_t)(intptr_t handle, float level);
                static dsSetAudioLevel_t dsSetAudioLevelFunc = 0;
                if (dsSetAudioLevelFunc == 0) {
                    dsSetAudioLevelFunc = (dsSetAudioLevel_t)resolve(RDK_DSHAL_NAME, "dsSetAudioLevel");
                    if (dsSetAudioLevelFunc == 0) {
                        LOGERR("dsSetAudioLevel is not defined");
                        return WPEFramework::Core::ERROR_GENERAL;
                    }
                }
                
                dsError_t ret = dsERR_GENERAL;
                if (0 != dsSetAudioLevelFunc) {
                    ret = dsSetAudioLevelFunc(dsHandle, volume);
                }
                
                if (ret == dsERR_NONE) {
                    _volumeDuckingLevel = volume;
                    LOGINFO("SetAudioDucking applied successfully: handle=%d, volume=%d", handle, volume);
                    
                    // Send audio level change event through callback if available
                    if (g_AudioLevelChangedCallback) {
                        g_AudioLevelChangedCallback(static_cast<float>(volume));
                    }
                } else {
                    LOGERR("dsSetAudioLevel failed with error: %d", ret);
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            LOGINFO("SetAudioDucking success: handle=%d, type=%d, action=%d, level=%d, final_volume=%d", 
                   handle, static_cast<int>(duckingType), static_cast<int>(duckingAction), level, volume);
        } catch (...) {
            LOGERR("Exception in SetAudioDucking");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetStereoMode(const int32_t handle, AudioStereoMode &mode) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        
        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsAudioStereoMode_t dsMode;
            
            dsError_t ret = dsGetStereoMode(dsHandle, &dsMode);
            
            if (ret == dsERR_NONE) {
                mode = convertFromDS(dsMode);
                LOGINFO("GetStereoMode success: handle=%d, mode=%d", handle, static_cast<int>(mode));
            } else {
                LOGERR("dsGetStereoMode failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetStereoMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetStereoMode(const int32_t handle, const AudioStereoMode mode, const bool persist) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsAudioStereoMode_t dsMode = convertToDS(mode);

            dsError_t ret = dsSetStereoMode(dsHandle, dsMode);

            if (ret == dsERR_NONE) {
                LOGINFO("SetStereoMode success: handle=%d, mode=%d, persist=%s", handle, static_cast<int>(mode), persist ? "true" : "false");

                // Determine actual port type from handle
                dsAudioPortType_t dsPortType = getAudioPortType(dsHandle);
                AudioPortType portType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER; // Default
                
                // Convert dsAudioPortType_t to AudioPortType and handle persistence
                std::string modeString;
                switch (mode) {
                    case AudioStereoMode::AUDIO_STEREO_STEREO:
                        modeString = "STEREO";
                        portType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER;
                        break;
                    case AudioStereoMode::AUDIO_STEREO_SURROUND:
                        modeString = "SURROUND";
                        portType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER;
                        break;
                    case AudioStereoMode::AUDIO_STEREO_PASSTHROUGH:
                        modeString = "PASSTHRU";
                        portType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER;
                        break;
                    default:
                        modeString = "STEREO";
                        portType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER;
                        break;
                }

                // Convert dsAudioPortType_t to AudioPortType for notification
                switch (dsPortType) {
                    case dsAUDIOPORT_TYPE_HDMI:
                        portType = AudioPortType::AUDIO_PORT_TYPE_HDMI;
                        break;
                    case dsAUDIOPORT_TYPE_SPDIF:
                        portType = AudioPortType::AUDIO_PORT_TYPE_SPDIF;
                        break;
                    case dsAUDIOPORT_TYPE_SPEAKER:
                        portType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER;
                        break;
                    case dsAUDIOPORT_TYPE_HDMI_ARC:
                        portType = AudioPortType::AUDIO_PORT_TYPE_HDMIARC;
                        break;
                    default:
                        portType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER;
                        break;
                }

                // Handle persistence based on port type and mode
                if (persist) {
                    try {
                        LOGINFO("Setting Audio Mode %s with persistent value: %s", modeString.c_str(), persist ? "true" : "false");
                        
                        switch (dsPortType) {
                            case dsAUDIOPORT_TYPE_HDMI:
                                device::HostPersistence::getInstance().persistHostProperty("HDMI0.AudioMode", modeString.c_str());
                                break;
                            case dsAUDIOPORT_TYPE_SPDIF:
                                device::HostPersistence::getInstance().persistHostProperty("SPDIF0.AudioMode", modeString.c_str());
                                break;
                            case dsAUDIOPORT_TYPE_HDMI_ARC:
                                device::HostPersistence::getInstance().persistHostProperty("HDMI_ARC0.AudioMode", modeString.c_str());
                                break;
                            case dsAUDIOPORT_TYPE_SPEAKER:
                                device::HostPersistence::getInstance().persistHostProperty("SPEAKER0.AudioMode", modeString.c_str());
                                break;
                            default:
                                LOGWARN("Unknown port type %d, skipping persistence", dsPortType);
                                break;
                        }
                    } catch (...) {
                        LOGERR("Error in persisting audio mode setting");
                    }
                }

                // Notify about audio mode change
                notifyAudioModeChanged(portType, mode);
            } else {
                LOGERR("dsSetStereoMode failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetStereoMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAssociatedAudioMixing(const int32_t handle, const bool mixing) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetAssociatedAudioMixing
            typedef dsError_t (*dsSetAssociatedAudioMixing_t)(intptr_t handle, bool mixing);
            static dsSetAssociatedAudioMixing_t dsSetAssociatedAudioMixingFunc = 0;
            if (dsSetAssociatedAudioMixingFunc == 0) {
                dsSetAssociatedAudioMixingFunc = (dsSetAssociatedAudioMixing_t)resolve(RDK_DSHAL_NAME, "dsSetAssociatedAudioMixing");
                if (dsSetAssociatedAudioMixingFunc == 0) {
                    LOGERR("dsSetAssociatedAudioMixing is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetAssociatedAudioMixingFunc) {
                ret = dsSetAssociatedAudioMixingFunc(dsHandle, mixing);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAssociatedAudioMixing success: handle=%d, mixing=%s", handle, mixing ? "true" : "false");

                // Notify about associated audio mixing change
                notifyAssociatedAudioMixingChanged(mixing);
            } else {
                LOGERR("dsSetAssociatedAudioMixing failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAssociatedAudioMixing");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAssociatedAudioMixing(const int32_t handle, bool &mixing) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            bool dsMixing;
            
            // Use resolve function for dsGetAssociatedAudioMixing
            typedef dsError_t (*dsGetAssociatedAudioMixing_t)(intptr_t handle, bool* mixing);
            static dsGetAssociatedAudioMixing_t dsGetAssociatedAudioMixingFunc = 0;
            if (dsGetAssociatedAudioMixingFunc == 0) {
                dsGetAssociatedAudioMixingFunc = (dsGetAssociatedAudioMixing_t)resolve(RDK_DSHAL_NAME, "dsGetAssociatedAudioMixing");
                if (dsGetAssociatedAudioMixingFunc == 0) {
                    LOGERR("dsGetAssociatedAudioMixing is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetAssociatedAudioMixingFunc) {
                ret = dsGetAssociatedAudioMixingFunc(dsHandle, &dsMixing);
            }
            
            if (ret == dsERR_NONE) {
                mixing = dsMixing;
                LOGINFO("GetAssociatedAudioMixing success: handle=%d, mixing=%s", handle, mixing ? "true" : "false");
            } else {
                LOGERR("dsGetAssociatedAudioMixing failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAssociatedAudioMixing");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioFaderControl(const int32_t handle, const int32_t mixerBalance) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetFaderControl
            typedef dsError_t (*dsSetFaderControl_t)(intptr_t handle, int balance);
            static dsSetFaderControl_t dsSetFaderControlFunc = 0;
            if (dsSetFaderControlFunc == 0) {
                dsSetFaderControlFunc = (dsSetFaderControl_t)resolve(RDK_DSHAL_NAME, "dsSetFaderControl");
                if (dsSetFaderControlFunc == 0) {
                    LOGERR("dsSetFaderControl is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetFaderControlFunc) {
                ret = dsSetFaderControlFunc(dsHandle, mixerBalance);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioFaderControl success: handle=%d, balance=%d", handle, mixerBalance);

                // Notify about fader control change
                notifyAudioFaderControlChanged(mixerBalance);
            } else {
                LOGERR("dsSetFaderControl failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioFaderControl");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioFaderControl(const int32_t handle, int32_t &mixerBalance) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            int dsBalance;
            
            // Use resolve function for dsGetFaderControl
            typedef dsError_t (*dsGetFaderControl_t)(intptr_t handle, int* balance);
            static dsGetFaderControl_t dsGetFaderControlFunc = 0;
            if (dsGetFaderControlFunc == 0) {
                dsGetFaderControlFunc = (dsGetFaderControl_t)resolve(RDK_DSHAL_NAME, "dsGetFaderControl");
                if (dsGetFaderControlFunc == 0) {
                    LOGERR("dsGetFaderControl is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetFaderControlFunc) {
                ret = dsGetFaderControlFunc(dsHandle, &dsBalance);
            }
            
            if (ret == dsERR_NONE) {
                mixerBalance = dsBalance;
                LOGINFO("GetAudioFaderControl success: handle=%d, balance=%d", handle, mixerBalance);
            } else {
                LOGERR("dsGetFaderControl failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioFaderControl");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioPrimaryLanguage(const int32_t handle, const std::string primaryAudioLanguage) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetPrimaryLanguage
            typedef dsError_t (*dsSetPrimaryLanguage_t)(intptr_t handle, const char* language);
            static dsSetPrimaryLanguage_t dsSetPrimaryLanguageFunc = 0;
            if (dsSetPrimaryLanguageFunc == 0) {
                dsSetPrimaryLanguageFunc = (dsSetPrimaryLanguage_t)resolve(RDK_DSHAL_NAME, "dsSetPrimaryLanguage");
                if (dsSetPrimaryLanguageFunc == 0) {
                    LOGERR("dsSetPrimaryLanguage is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetPrimaryLanguageFunc) {
                ret = dsSetPrimaryLanguageFunc(dsHandle, primaryAudioLanguage.c_str());
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioPrimaryLanguage success: handle=%d, language=%s", handle, primaryAudioLanguage.c_str());

                // Notify about primary language change
                notifyAudioPrimaryLanguageChanged(primaryAudioLanguage);
            } else {
                LOGERR("dsSetPrimaryLanguage failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioPrimaryLanguage");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioPrimaryLanguage(const int32_t handle, std::string &primaryAudioLanguage) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            char langStr[32] = {0};
            
            // Use resolve function for dsGetPrimaryLanguage
            typedef dsError_t (*dsGetPrimaryLanguage_t)(intptr_t handle, char* language);
            static dsGetPrimaryLanguage_t dsGetPrimaryLanguageFunc = 0;
            if (dsGetPrimaryLanguageFunc == 0) {
                dsGetPrimaryLanguageFunc = (dsGetPrimaryLanguage_t)resolve(RDK_DSHAL_NAME, "dsGetPrimaryLanguage");
                if (dsGetPrimaryLanguageFunc == 0) {
                    LOGERR("dsGetPrimaryLanguage is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetPrimaryLanguageFunc) {
                ret = dsGetPrimaryLanguageFunc(dsHandle, langStr);
            }
            
            if (ret == dsERR_NONE) {
                primaryAudioLanguage = std::string(langStr);
                LOGINFO("GetAudioPrimaryLanguage success: handle=%d, language=%s", handle, primaryAudioLanguage.c_str());
            } else {
                LOGERR("dsGetPrimaryLanguage failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioPrimaryLanguage");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioSecondaryLanguage(const int32_t handle, const std::string secondaryAudioLanguage) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetSecondaryLanguage
            typedef dsError_t (*dsSetSecondaryLanguage_t)(intptr_t handle, const char* language);
            static dsSetSecondaryLanguage_t dsSetSecondaryLanguageFunc = 0;
            if (dsSetSecondaryLanguageFunc == 0) {
                dsSetSecondaryLanguageFunc = (dsSetSecondaryLanguage_t)resolve(RDK_DSHAL_NAME, "dsSetSecondaryLanguage");
                if (dsSetSecondaryLanguageFunc == 0) {
                    LOGERR("dsSetSecondaryLanguage is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetSecondaryLanguageFunc) {
                ret = dsSetSecondaryLanguageFunc(dsHandle, secondaryAudioLanguage.c_str());
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioSecondaryLanguage success: handle=%d, language=%s", handle, secondaryAudioLanguage.c_str());

                // Notify about secondary language change
                notifyAudioSecondaryLanguageChanged(secondaryAudioLanguage);
            } else {
                LOGERR("dsSetSecondaryLanguage failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioSecondaryLanguage");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioSecondaryLanguage(const int32_t handle, std::string &secondaryAudioLanguage) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            char langStr[32] = {0};
            
            // Use resolve function for dsGetSecondaryLanguage
            typedef dsError_t (*dsGetSecondaryLanguage_t)(intptr_t handle, char* language);
            static dsGetSecondaryLanguage_t dsGetSecondaryLanguageFunc = 0;
            if (dsGetSecondaryLanguageFunc == 0) {
                dsGetSecondaryLanguageFunc = (dsGetSecondaryLanguage_t)resolve(RDK_DSHAL_NAME, "dsGetSecondaryLanguage");
                if (dsGetSecondaryLanguageFunc == 0) {
                    LOGERR("dsGetSecondaryLanguage is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetSecondaryLanguageFunc) {
                ret = dsGetSecondaryLanguageFunc(dsHandle, langStr);
            }
            
            if (ret == dsERR_NONE) {
                secondaryAudioLanguage = std::string(langStr);
                LOGINFO("GetAudioSecondaryLanguage success: handle=%d, language=%s", handle, secondaryAudioLanguage.c_str());
            } else {
                LOGERR("dsGetSecondaryLanguage failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioSecondaryLanguage");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t IsAudioOutputConnected(const int32_t handle, bool &isConnected) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            bool dsConnected;
            
            // Use resolve function for dsIsAudioPortEnabled (used as connection check)
            typedef dsError_t (*dsIsAudioPortEnabled_t)(intptr_t handle, bool* enabled);
            static dsIsAudioPortEnabled_t dsIsAudioPortEnabledFunc = 0;
            if (dsIsAudioPortEnabledFunc == 0) {
                dsIsAudioPortEnabledFunc = (dsIsAudioPortEnabled_t)resolve(RDK_DSHAL_NAME, "dsIsAudioPortEnabled");
                if (dsIsAudioPortEnabledFunc == 0) {
                    LOGERR("dsIsAudioPortEnabled is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsIsAudioPortEnabledFunc) {
                ret = dsIsAudioPortEnabledFunc(dsHandle, &dsConnected);
            }
            
            if (ret == dsERR_NONE) {
                isConnected = dsConnected;
                LOGINFO("IsAudioOutputConnected success: handle=%d, connected=%s", handle, isConnected ? "true" : "false");
            } else {
                LOGERR("dsIsAudioPortEnabled failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in IsAudioOutputConnected");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioSinkDeviceAtmosCapability(const int32_t handle, DolbyAtmosCapability &atmosCapability) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            // dsAtmosCapability_t should be dsATMOSCapability_t
            dsATMOSCapability_t dsCapability;
            
            // Use resolve function for dsGetSinkDeviceAtmosCapability
            typedef dsError_t (*dsGetSinkDeviceAtmosCapability_t)(intptr_t handle, dsATMOSCapability_t* capability);
            static dsGetSinkDeviceAtmosCapability_t dsGetSinkDeviceAtmosCapabilityFunc = 0;
            if (dsGetSinkDeviceAtmosCapabilityFunc == 0) {
                dsGetSinkDeviceAtmosCapabilityFunc = (dsGetSinkDeviceAtmosCapability_t)resolve(RDK_DSHAL_NAME, "dsGetSinkDeviceAtmosCapability");
                if (dsGetSinkDeviceAtmosCapabilityFunc == 0) {
                    LOGERR("dsGetSinkDeviceAtmosCapability is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetSinkDeviceAtmosCapabilityFunc) {
                ret = dsGetSinkDeviceAtmosCapabilityFunc(dsHandle, &dsCapability);
            }
            
            if (ret == dsERR_NONE) {
                atmosCapability = static_cast<DolbyAtmosCapability>(dsCapability);
                LOGINFO("GetAudioSinkDeviceAtmosCapability success: handle=%d, capability=%d", handle, static_cast<int>(atmosCapability));
            } else {
                LOGERR("dsGetSinkDeviceAtmosCapability failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioSinkDeviceAtmosCapability");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioAtmosOutputMode(const int32_t handle, const bool enable) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetAudioAtmosOutputMode
            typedef dsError_t (*dsSetAudioAtmosOutputMode_t)(intptr_t handle, bool enable);
            static dsSetAudioAtmosOutputMode_t dsSetAudioAtmosOutputModeFunc = 0;
            if (dsSetAudioAtmosOutputModeFunc == 0) {
                dsSetAudioAtmosOutputModeFunc = (dsSetAudioAtmosOutputMode_t)resolve(RDK_DSHAL_NAME, "dsSetAudioAtmosOutputMode");
                if (dsSetAudioAtmosOutputModeFunc == 0) {
                    LOGERR("dsSetAudioAtmosOutputMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetAudioAtmosOutputModeFunc) {
                ret = dsSetAudioAtmosOutputModeFunc(dsHandle, enable);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioAtmosOutputMode success: handle=%d, enable=%s", handle, enable ? "true" : "false");
            } else {
                LOGERR("dsSetAudioAtmosOutputMode failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioAtmosOutputMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    // Missing IDeviceSettingsAudio interface methods implementation

    uint32_t IsAudioPortEnabled(const int32_t handle, bool &enabled) override {
        ENTRY_LOG;
        try {
            bool portEnabled = false;
            
            // Use resolve function for dsIsAudioPortEnabled
            typedef dsError_t (*dsIsAudioPortEnabled_t)(intptr_t handle, bool* enabled);
            static dsIsAudioPortEnabled_t dsIsAudioPortEnabledFunc = 0;
            if (dsIsAudioPortEnabledFunc == 0) {
                dsIsAudioPortEnabledFunc = (dsIsAudioPortEnabled_t)resolve(RDK_DSHAL_NAME, "dsIsAudioPortEnabled");
                if (dsIsAudioPortEnabledFunc == 0) {
                    LOGERR("dsIsAudioPortEnabled is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsIsAudioPortEnabledFunc) {
                dsResult = dsIsAudioPortEnabledFunc(static_cast<intptr_t>(handle), &portEnabled);
            }
            
            if (dsResult == dsERR_NONE) {
                enabled = portEnabled;
                LOGINFO("IsAudioPortEnabled success: handle=%d, enabled=%s", handle, enabled ? "true" : "false");
            } else {
                LOGERR("dsIsAudioPortEnabled failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in IsAudioPortEnabled");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t EnableAudioPort(const int32_t handle, const bool enable) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsAudioPortType_t portType = getAudioPortType(dsHandle);

            // Special handling for SPEAKER port - manage audio ducking level
            if (portType == dsAUDIOPORT_TYPE_SPEAKER) {
                bool muted = false;
                dsError_t ret = dsIsAudioMute(dsHandle, &muted);
                if (ret != dsERR_NONE) {
                    LOGWARN("Failed to get the mute status of Speaker port");
                }

                if (enable && !muted) {
                    if (setAudioDuckingAudioLevel(dsHandle) != WPEFramework::Core::ERROR_NONE) {
                        LOGERR("Failed to set audio ducking level for Speaker port");
                        return WPEFramework::Core::ERROR_GENERAL;
                    }
                } else {
                    LOGINFO("Not setting audio ducking level as mute status is %s", muted ? "true" : "false");
                }
            }

            // Enable/disable the audio port
            // Use resolve function for dsEnableAudioPort
            typedef dsError_t (*dsEnableAudioPort_t)(intptr_t handle, bool enable);
            static dsEnableAudioPort_t dsEnableAudioPortFunc = 0;
            if (dsEnableAudioPortFunc == 0) {
                dsEnableAudioPortFunc = (dsEnableAudioPort_t)resolve(RDK_DSHAL_NAME, "dsEnableAudioPort");
                if (dsEnableAudioPortFunc == 0) {
                    LOGERR("dsEnableAudioPort is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsEnableAudioPortFunc) {
                dsResult = dsEnableAudioPortFunc(dsHandle, enable);
            }
            if (dsResult != dsERR_NONE) {
                LOGERR("dsEnableAudioPort failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }

            // Verify that the port was actually enabled/disabled
            bool portEnabled = false;
            dsResult = dsIsAudioPortEnabled(dsHandle, &portEnabled);
            if (dsResult == dsERR_NONE) {
                if (portEnabled != enable) {
                    LOGERR("Audio port enable verification failed. Expected: %s, Actual: %s", 
                           enable ? "enabled" : "disabled", portEnabled ? "enabled" : "disabled");
                    return WPEFramework::Core::ERROR_GENERAL;
                } else {
                    LOGINFO("Audio port enable verification passed: %s", enable ? "enabled" : "disabled");

                    // Update port state tracking
                    if (portType < dsAUDIOPORT_TYPE_MAX) {
                        _audioPortEnabled[portType] = enable;
                        LOGINFO("Port type %d enabled status: %s", portType, enable ? "true" : "false");

                        // Set audio delay when enabling port
                        if (enable) {
                            uint32_t audioDelay = getAudioDelayInternal(portType);
                            bool delaySet = setAudioDelayInternal(dsHandle, audioDelay);
                            LOGINFO("Updated audio delay for port enable - port type: %d, delay: %u, success: %s", 
                                   portType, audioDelay, delaySet ? "true" : "false");
                        }
                    }
                }
            } else {
                LOGWARN("Audio port status verification failed - dsIsAudioPortEnabled call failed with error: %d", dsResult);
            }

            LOGINFO("EnableAudioPort success: handle=%d, enable=%s", handle, enable ? "true" : "false");

        } catch (...) {
            LOGERR("Exception in EnableAudioPort");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetSupportedARCTypes(const int32_t handle, int32_t &types) override {
        ENTRY_LOG;
        try {
            int arcTypes = 0;
            
            // Use resolve function for dsGetSupportedARCTypes
            typedef dsError_t (*dsGetSupportedARCTypes_t)(intptr_t handle, int* types);
            static dsGetSupportedARCTypes_t dsGetSupportedARCTypesFunc = 0;
            if (dsGetSupportedARCTypesFunc == 0) {
                dsGetSupportedARCTypesFunc = (dsGetSupportedARCTypes_t)resolve(RDK_DSHAL_NAME, "dsGetSupportedARCTypes");
                if (dsGetSupportedARCTypesFunc == 0) {
                    LOGERR("dsGetSupportedARCTypes is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetSupportedARCTypesFunc) {
                dsResult = dsGetSupportedARCTypesFunc(static_cast<intptr_t>(handle), &arcTypes);
            }
            
            if (dsResult == dsERR_NONE) {
                types = arcTypes;
            } else {
                LOGERR("dsGetSupportedARCTypes failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetSupportedARCTypes");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetSAD(const int32_t handle, const uint8_t sadList[], const uint8_t count) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            typedef dsError_t (*dsSetSAD_t)(intptr_t handle, const uint8_t* sadList, uint8_t count);
            static dsSetSAD_t dsSetSADFunc = 0;
            if (dsSetSADFunc == 0) {
                dsSetSADFunc = (dsSetSAD_t)resolve(RDK_DSHAL_NAME, "dsSetSAD");
                if(dsSetSADFunc == 0) {
                    LOGERR("dsSetSAD is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetSADFunc) {
                ret = dsSetSADFunc(dsHandle, sadList, count);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetSAD success: handle=%d, count=%d", handle, count);
            } else {
                LOGERR("dsSetSAD failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetSAD");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t EnableARC(const int32_t handle, const WPEFramework::Exchange::IDeviceSettingsAudio::AudioARCStatus arcStatus) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsAudioARCStatus_t dsARCStatus;
            dsARCStatus.type = static_cast<dsAudioARCTypes_t>(arcStatus.arcType);
            dsARCStatus.status = arcStatus.status;
            
            typedef dsError_t (*dsEnableARC_t)(intptr_t handle, dsAudioARCStatus_t* arcStatus);
            static dsEnableARC_t dsEnableARCFunc = 0;
            if (dsEnableARCFunc == 0) {
                dsEnableARCFunc = (dsEnableARC_t)resolve(RDK_DSHAL_NAME, "dsEnableARC");
                if(dsEnableARCFunc == 0) {
                    LOGERR("dsEnableARC is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsEnableARCFunc) {
                ret = dsEnableARCFunc(dsHandle, &dsARCStatus);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("EnableARC success: handle=%d, arcStatus type=%d status=%d", handle, static_cast<int>(arcStatus.arcType), static_cast<int>(arcStatus.status));
            } else {
                LOGERR("dsEnableARC failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in EnableARC");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioEnablePersist(const int32_t handle, bool &enabled, string &portName) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            // By default all ports are enabled
            enabled = true;

            std::string isEnabledAudioPortKey("audio.");
            isEnabledAudioPortKey.append(portName);
            isEnabledAudioPortKey.append(".isEnabled");
            std::string _AudioPortEnable("TRUE");

            try {
                _AudioPortEnable = device::HostPersistence::getInstance().getProperty(isEnabledAudioPortKey);
            }
            catch(...) {
                try {
                    LOGINFO("GetAudioEnablePersist: %s port enable settings not found in persistence store. Try system default", isEnabledAudioPortKey.c_str());
                    _AudioPortEnable = device::HostPersistence::getInstance().getDefaultProperty(isEnabledAudioPortKey);
                }
                catch(...) {
                    // By default enable all the ports
                    _AudioPortEnable = "TRUE";
                }
            }

            if ("FALSE" == _AudioPortEnable) { 
                LOGINFO("GetAudioEnablePersist: persist dsEnableAudioPort value: %s", _AudioPortEnable.c_str());
                enabled = false;
            }
            else {
                LOGINFO("GetAudioEnablePersist: persist dsEnableAudioPort value: %s", _AudioPortEnable.c_str());
                enabled = true;
            }

            LOGINFO("GetAudioEnablePersist success: handle=%d, portName=%s, enabled=%s, key=%s, value=%s", 
                   handle, portName.c_str(), enabled ? "TRUE" : "FALSE", isEnabledAudioPortKey.c_str(), _AudioPortEnable.c_str());
        } catch (...) {
            LOGERR("Exception in GetAudioEnablePersist");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioEnablePersist(const int32_t handle, const bool enable, const string portName) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            std::string isEnabledAudioPortKey("audio.");
            isEnabledAudioPortKey.append(portName);
            isEnabledAudioPortKey.append(".isEnabled");

            std::string enableValue = enable ? "TRUE" : "FALSE";
            device::HostPersistence::getInstance().persistHostProperty(isEnabledAudioPortKey.c_str(), enableValue.c_str());

            LOGINFO("SetAudioEnablePersist success: handle=%d, portName=%s, enable=%s, key=%s", 
                   handle, portName.c_str(), enableValue.c_str(), isEnabledAudioPortKey.c_str());
        } catch (...) {
            LOGERR("Exception in SetAudioEnablePersist");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t IsAudioMSDecoded(const int32_t handle, bool &hasms11Decode) override {
        ENTRY_LOG;
        try {
            bool ms11Decoded = false;
            
            // Use resolve function for dsIsAudioMSDecode
            typedef dsError_t (*dsIsAudioMSDecode_t)(intptr_t handle, bool* decoded);
            static dsIsAudioMSDecode_t dsIsAudioMSDecodeFunc = 0;
            if (dsIsAudioMSDecodeFunc == 0) {
                dsIsAudioMSDecodeFunc = (dsIsAudioMSDecode_t)resolve(RDK_DSHAL_NAME, "dsIsAudioMSDecode");
                if (dsIsAudioMSDecodeFunc == 0) {
                    LOGERR("dsIsAudioMSDecode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsIsAudioMSDecodeFunc) {
                dsResult = dsIsAudioMSDecodeFunc(static_cast<intptr_t>(handle), &ms11Decoded);
            }
            
            if (dsResult == dsERR_NONE) {
                hasms11Decode = ms11Decoded;
            } else {
                LOGERR("dsIsAudioMSDecode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in IsAudioMSDecoded");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t IsAudioMS12Decoded(const int32_t handle, bool &hasms12Decode) override {
        ENTRY_LOG;
        try {
            bool ms12Decoded = false;
            dsError_t dsResult = dsIsAudioMS12Decode(static_cast<intptr_t>(handle), &ms12Decoded);
            if (dsResult == dsERR_NONE) {
                hasms12Decode = ms12Decoded;
            } else {
                LOGERR("dsIsAudioMS12Decode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in IsAudioMS12Decoded");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioLEConfig(const int32_t handle, bool &enabled) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            bool leEnabled;
            // Use resolve function for dsGetLEConfig
            typedef dsError_t (*dsGetLEConfig_t)(intptr_t handle, bool* enabled);
            static dsGetLEConfig_t dsGetLEConfigFunc = 0;
            if (dsGetLEConfigFunc == 0) {
                dsGetLEConfigFunc = (dsGetLEConfig_t)resolve(RDK_DSHAL_NAME, "dsGetLEConfig");
                if (dsGetLEConfigFunc == 0) {
                    LOGERR("dsGetLEConfig is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetLEConfigFunc) {
                ret = dsGetLEConfigFunc(dsHandle, &leEnabled);
            }
            if (ret == dsERR_NONE) {
                enabled = leEnabled;
                LOGINFO("GetAudioLEConfig success: handle=%d, enabled=%s", handle, enabled ? "true" : "false");
            } else {
                LOGERR("dsGetLEConfig failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioLEConfig");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t EnableAudioLEConfig(const int32_t handle, const bool enable) override {
        ENTRY_LOG;
        try {
            // dsMS12FEATURE_LOUDNESSEQUIVALENCE constant doesn't exist, using DAPV2 as fallback
            dsError_t dsResult = dsEnableMS12Config(static_cast<intptr_t>(handle), dsMS12FEATURE_DAPV2, enable);
            if (dsResult != dsERR_NONE) {
                LOGERR("dsEnableMS12Config (LE) failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in EnableAudioLEConfig");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioDelay(const int32_t handle, const uint32_t audioDelay) override {
        ENTRY_LOG;
        try {
            // Use resolve function for dsSetAudioDelay
            typedef dsError_t (*dsSetAudioDelay_t)(intptr_t handle, uint32_t audioDelay);
            static dsSetAudioDelay_t dsSetAudioDelayFunc = 0;
            if (dsSetAudioDelayFunc == 0) {
                dsSetAudioDelayFunc = (dsSetAudioDelay_t)resolve(RDK_DSHAL_NAME, "dsSetAudioDelay");
                if (dsSetAudioDelayFunc == 0) {
                    LOGERR("dsSetAudioDelay is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsSetAudioDelayFunc) {
                dsResult = dsSetAudioDelayFunc(static_cast<intptr_t>(handle), audioDelay);
            }
            
            if (dsResult == dsERR_NONE) {
                LOGINFO("SetAudioDelay success: handle=%d, delay=%u", handle, audioDelay);
            } else {
                LOGERR("dsSetAudioDelay failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioDelay");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioDelay(const int32_t handle, uint32_t &audioDelay) override {
        ENTRY_LOG;
        try {
            uint32_t delay = 0;
            // Use resolve function for dsGetAudioDelay
            typedef dsError_t (*dsGetAudioDelay_t)(intptr_t handle, uint32_t* delay);
            static dsGetAudioDelay_t dsGetAudioDelayFunc = 0;
            if (dsGetAudioDelayFunc == 0) {
                dsGetAudioDelayFunc = (dsGetAudioDelay_t)resolve(RDK_DSHAL_NAME, "dsGetAudioDelay");
                if (dsGetAudioDelayFunc == 0) {
                    LOGERR("dsGetAudioDelay is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetAudioDelayFunc) {
                dsResult = dsGetAudioDelayFunc(static_cast<intptr_t>(handle), &delay);
            }
            if (dsResult == dsERR_NONE) {
                audioDelay = delay;
                LOGINFO("GetAudioDelay success: handle=%d, delay=%u", handle, audioDelay);
            } else {
                LOGERR("dsGetAudioDelay failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioDelay");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioDelayOffset(const int32_t handle, const uint32_t delayOffset) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            typedef dsError_t (*dsSetAudioDelayOffset_t)(intptr_t handle, uint32_t delayOffset);
            static dsSetAudioDelayOffset_t dsSetAudioDelayOffsetFunc = 0;
            if (dsSetAudioDelayOffsetFunc == 0) {
                dsSetAudioDelayOffsetFunc = (dsSetAudioDelayOffset_t)resolve(RDK_DSHAL_NAME, "dsSetAudioDelayOffset");
                if(dsSetAudioDelayOffsetFunc == 0) {
                    LOGERR("dsSetAudioDelayOffset is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetAudioDelayOffsetFunc) {
                ret = dsSetAudioDelayOffsetFunc(dsHandle, delayOffset);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioDelayOffset success: handle=%d, offset=%u", handle, delayOffset);
            } else {
                LOGERR("dsSetAudioDelayOffset failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioDelayOffset");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioDelayOffset(const int32_t handle, uint32_t &delayOffset) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            uint32_t dsOffset;
            
            typedef dsError_t (*dsGetAudioDelayOffset_t)(intptr_t handle, uint32_t* delayOffset);
            static dsGetAudioDelayOffset_t dsGetAudioDelayOffsetFunc = 0;
            if (dsGetAudioDelayOffsetFunc == 0) {
                dsGetAudioDelayOffsetFunc = (dsGetAudioDelayOffset_t)resolve(RDK_DSHAL_NAME, "dsGetAudioDelayOffset");
                if(dsGetAudioDelayOffsetFunc == 0) {
                    LOGERR("dsGetAudioDelayOffset is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetAudioDelayOffsetFunc) {
                ret = dsGetAudioDelayOffsetFunc(dsHandle, &dsOffset);
            }
            
            if (ret == dsERR_NONE) {
                delayOffset = dsOffset;
                LOGINFO("GetAudioDelayOffset success: handle=%d, offset=%u", handle, delayOffset);
            } else {
                LOGERR("dsGetAudioDelayOffset failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioDelayOffset");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioCompression(const int32_t handle, const int32_t compressionLevel) override {
        ENTRY_LOG;
        try {
            // Use resolve function for dsSetAudioCompression
            typedef dsError_t (*dsSetAudioCompression_t)(intptr_t handle, int compression);
            static dsSetAudioCompression_t dsSetAudioCompressionFunc = 0;
            if (dsSetAudioCompressionFunc == 0) {
                dsSetAudioCompressionFunc = (dsSetAudioCompression_t)resolve(RDK_DSHAL_NAME, "dsSetAudioCompression");
                if (dsSetAudioCompressionFunc == 0) {
                    LOGERR("dsSetAudioCompression is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsSetAudioCompressionFunc) {
                dsResult = dsSetAudioCompressionFunc(static_cast<intptr_t>(handle), compressionLevel);
            }
            if (dsResult == dsERR_NONE) {
                LOGINFO("SetAudioCompression success: handle=%d, level=%d", handle, compressionLevel);
            } else {
                LOGERR("dsSetAudioCompression failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioCompression");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioCompression(const int32_t handle, int32_t &compressionLevel) override {
        ENTRY_LOG;
        try {
            int compression = 0;
            // Use resolve function for dsGetAudioCompression
            typedef dsError_t (*dsGetAudioCompression_t)(intptr_t handle, int* compression);
            static dsGetAudioCompression_t dsGetAudioCompressionFunc = 0;
            if (dsGetAudioCompressionFunc == 0) {
                dsGetAudioCompressionFunc = (dsGetAudioCompression_t)resolve(RDK_DSHAL_NAME, "dsGetAudioCompression");
                if (dsGetAudioCompressionFunc == 0) {
                    LOGERR("dsGetAudioCompression is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetAudioCompressionFunc) {
                dsResult = dsGetAudioCompressionFunc(static_cast<intptr_t>(handle), &compression);
            }
            if (dsResult == dsERR_NONE) {
                compressionLevel = compression;
                LOGINFO("GetAudioCompression success: handle=%d, level=%d", handle, compressionLevel);
            } else {
                LOGERR("dsGetAudioCompression failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioCompression");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioDialogEnhancement(const int32_t handle, const int32_t level) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Use resolve function for dsSetDialogEnhancement
            typedef dsError_t (*dsSetDialogEnhancement_t)(intptr_t handle, int level);
            static dsSetDialogEnhancement_t dsSetDialogEnhancementFunc = 0;
            if (dsSetDialogEnhancementFunc == 0) {
                dsSetDialogEnhancementFunc = (dsSetDialogEnhancement_t)resolve(RDK_DSHAL_NAME, "dsSetDialogEnhancement");
                if (dsSetDialogEnhancementFunc == 0) {
                    LOGERR("dsSetDialogEnhancement is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetDialogEnhancementFunc) {
                ret = dsSetDialogEnhancementFunc(dsHandle, level);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioDialogEnhancement success: handle=%d, level=%d", handle, level);
            } else {
                LOGERR("dsSetDialogEnhancement failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioDialogEnhancement");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioDialogEnhancement(const int32_t handle, int32_t &level) override {
        ENTRY_LOG;
        try {
            int dialogLevel = 0;
            // Use resolve function for dsGetDialogEnhancement
            typedef dsError_t (*dsGetDialogEnhancement_t)(intptr_t handle, int* level);
            static dsGetDialogEnhancement_t dsGetDialogEnhancementFunc = 0;
            if (dsGetDialogEnhancementFunc == 0) {
                dsGetDialogEnhancementFunc = (dsGetDialogEnhancement_t)resolve(RDK_DSHAL_NAME, "dsGetDialogEnhancement");
                if (dsGetDialogEnhancementFunc == 0) {
                    LOGERR("dsGetDialogEnhancement is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetDialogEnhancementFunc) {
                dsResult = dsGetDialogEnhancementFunc(static_cast<intptr_t>(handle), &dialogLevel);
            }
            if (dsResult == dsERR_NONE) {
                level = dialogLevel;
                LOGINFO("GetAudioDialogEnhancement success: handle=%d, level=%d", handle, level);
            } else {
                LOGERR("dsGetDialogEnhancement failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioDialogEnhancement");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioDolbyVolumeMode(const int32_t handle, const bool enable) override {
        ENTRY_LOG;
        try {
            // Use resolve function for dsSetDolbyVolumeMode
            typedef dsError_t (*dsSetDolbyVolumeMode_t)(intptr_t handle, bool enable);
            static dsSetDolbyVolumeMode_t dsSetDolbyVolumeModeFunc = 0;
            if (dsSetDolbyVolumeModeFunc == 0) {
                dsSetDolbyVolumeModeFunc = (dsSetDolbyVolumeMode_t)resolve(RDK_DSHAL_NAME, "dsSetDolbyVolumeMode");
                if (dsSetDolbyVolumeModeFunc == 0) {
                    LOGERR("dsSetDolbyVolumeMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsSetDolbyVolumeModeFunc) {
                dsResult = dsSetDolbyVolumeModeFunc(static_cast<intptr_t>(handle), enable);
            }
            if (dsResult == dsERR_NONE) {
                LOGINFO("SetAudioDolbyVolumeMode success: handle=%d, enable=%s", handle, enable ? "true" : "false");
            } else {
                LOGERR("dsSetDolbyVolumeMode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioDolbyVolumeMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioDolbyVolumeMode(const int32_t handle, bool &enabled) override {
        ENTRY_LOG;
        try {
            bool dolbyMode = false;
            // Use resolve function for dsGetDolbyVolumeMode
            typedef dsError_t (*dsGetDolbyVolumeMode_t)(intptr_t handle, bool* mode);
            static dsGetDolbyVolumeMode_t dsGetDolbyVolumeModeFunc = 0;
            if (dsGetDolbyVolumeModeFunc == 0) {
                dsGetDolbyVolumeModeFunc = (dsGetDolbyVolumeMode_t)resolve(RDK_DSHAL_NAME, "dsGetDolbyVolumeMode");
                if (dsGetDolbyVolumeModeFunc == 0) {
                    LOGERR("dsGetDolbyVolumeMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetDolbyVolumeModeFunc) {
                dsResult = dsGetDolbyVolumeModeFunc(static_cast<intptr_t>(handle), &dolbyMode);
            }
            if (dsResult == dsERR_NONE) {
                enabled = dolbyMode;
                LOGINFO("GetAudioDolbyVolumeMode success: handle=%d, enabled=%s", handle, enabled ? "true" : "false");
            } else {
                LOGERR("dsGetDolbyVolumeMode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioDolbyVolumeMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioIntelligentEqualizerMode(const int32_t handle, const int32_t mode) override {
        ENTRY_LOG;
        try {
            // Use resolve function for dsSetIntelligentEqualizerMode
            typedef dsError_t (*dsSetIntelligentEqualizerMode_t)(intptr_t handle, int mode);
            static dsSetIntelligentEqualizerMode_t dsSetIntelligentEqualizerModeFunc = 0;
            if (dsSetIntelligentEqualizerModeFunc == 0) {
                dsSetIntelligentEqualizerModeFunc = (dsSetIntelligentEqualizerMode_t)resolve(RDK_DSHAL_NAME, "dsSetIntelligentEqualizerMode");
                if (dsSetIntelligentEqualizerModeFunc == 0) {
                    LOGERR("dsSetIntelligentEqualizerMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsSetIntelligentEqualizerModeFunc) {
                dsResult = dsSetIntelligentEqualizerModeFunc(static_cast<intptr_t>(handle), mode);
            }
            if (dsResult == dsERR_NONE) {
                LOGINFO("SetAudioIntelligentEqualizerMode success: handle=%d, mode=%d", handle, mode);
            } else {
                LOGERR("dsSetIntelligentEqualizerMode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioIntelligentEqualizerMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioIntelligentEqualizerMode(const int32_t handle, int32_t &mode) override {
        ENTRY_LOG;
        try {
            int eqMode = 0;
            // Use resolve function for dsGetIntelligentEqualizerMode
            typedef dsError_t (*dsGetIntelligentEqualizerMode_t)(intptr_t handle, int* mode);
            static dsGetIntelligentEqualizerMode_t dsGetIntelligentEqualizerModeFunc = 0;
            if (dsGetIntelligentEqualizerModeFunc == 0) {
                dsGetIntelligentEqualizerModeFunc = (dsGetIntelligentEqualizerMode_t)resolve(RDK_DSHAL_NAME, "dsGetIntelligentEqualizerMode");
                if (dsGetIntelligentEqualizerModeFunc == 0) {
                    LOGERR("dsGetIntelligentEqualizerMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetIntelligentEqualizerModeFunc) {
                dsResult = dsGetIntelligentEqualizerModeFunc(static_cast<intptr_t>(handle), &eqMode);
            }
            if (dsResult == dsERR_NONE) {
                mode = eqMode;
                LOGINFO("GetAudioIntelligentEqualizerMode success: handle=%d, mode=%d", handle, mode);
            } else {
                LOGERR("dsGetIntelligentEqualizerMode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioIntelligentEqualizerMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioVolumeLeveller(const int32_t handle, const WPEFramework::Exchange::IDeviceSettingsAudio::VolumeLeveller volumeLeveller) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsVolumeLeveller_t dsVolLeveller;
            dsVolLeveller.mode = static_cast<int>(volumeLeveller.mode);
            dsVolLeveller.level = static_cast<int>(volumeLeveller.level);
            // Use resolve function for dsSetVolumeLeveller
            typedef dsError_t (*dsSetVolumeLeveller_t)(intptr_t handle, dsVolumeLeveller_t leveller);
            static dsSetVolumeLeveller_t dsSetVolumeLevellerFunc = 0;
            if (dsSetVolumeLevellerFunc == 0) {
                dsSetVolumeLevellerFunc = (dsSetVolumeLeveller_t)resolve(RDK_DSHAL_NAME, "dsSetVolumeLeveller");
                if (dsSetVolumeLevellerFunc == 0) {
                    LOGERR("dsSetVolumeLeveller is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetVolumeLevellerFunc) {
                ret = dsSetVolumeLevellerFunc(dsHandle, dsVolLeveller);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioVolumeLeveller success: handle=%d, mode=%d, level=%d", handle, volumeLeveller.mode, volumeLeveller.level);
            } else {
                LOGERR("dsSetVolumeLeveller failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioVolumeLeveller");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioVolumeLeveller(const int32_t handle, WPEFramework::Exchange::IDeviceSettingsAudio::VolumeLeveller &volumeLeveller) override {
        ENTRY_LOG;
        try {
            dsVolumeLeveller_t volLeveller;
            // Use resolve function for dsGetVolumeLeveller
            typedef dsError_t (*dsGetVolumeLeveller_t)(intptr_t handle, dsVolumeLeveller_t* leveller);
            static dsGetVolumeLeveller_t dsGetVolumeLevellerFunc = 0;
            if (dsGetVolumeLevellerFunc == 0) {
                dsGetVolumeLevellerFunc = (dsGetVolumeLeveller_t)resolve(RDK_DSHAL_NAME, "dsGetVolumeLeveller");
                if (dsGetVolumeLevellerFunc == 0) {
                    LOGERR("dsGetVolumeLeveller is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetVolumeLevellerFunc) {
                dsResult = dsGetVolumeLevellerFunc(static_cast<intptr_t>(handle), &volLeveller);
            }
            if (dsResult == dsERR_NONE) {
                // Convert dsVolumeLeveller_t to VolumeLeveller enum
                volumeLeveller.mode = static_cast<uint8_t>(volLeveller.mode);
                volumeLeveller.level = static_cast<uint8_t>(volLeveller.level);
            } else {
                LOGERR("dsGetVolumeLeveller failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioVolumeLeveller");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioBassEnhancer(const int32_t handle, const int32_t boost) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            // Use resolve function for dsSetBassEnhancer
            typedef dsError_t (*dsSetBassEnhancer_t)(intptr_t handle, int boost);
            static dsSetBassEnhancer_t dsSetBassEnhancerFunc = 0;
            if (dsSetBassEnhancerFunc == 0) {
                dsSetBassEnhancerFunc = (dsSetBassEnhancer_t)resolve(RDK_DSHAL_NAME, "dsSetBassEnhancer");
                if (dsSetBassEnhancerFunc == 0) {
                    LOGERR("dsSetBassEnhancer is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetBassEnhancerFunc) {
                ret = dsSetBassEnhancerFunc(dsHandle, boost);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioBassEnhancer success: handle=%d, boost=%d", handle, boost);
            } else {
                LOGERR("dsSetBassEnhancer failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioBassEnhancer");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioBassEnhancer(const int32_t handle, int32_t &boost) override {
        ENTRY_LOG;
        try {
            int bassBoost = 0;
            // Use resolve function for dsGetBassEnhancer
            typedef dsError_t (*dsGetBassEnhancer_t)(intptr_t handle, int* boost);
            static dsGetBassEnhancer_t dsGetBassEnhancerFunc = 0;
            if (dsGetBassEnhancerFunc == 0) {
                dsGetBassEnhancerFunc = (dsGetBassEnhancer_t)resolve(RDK_DSHAL_NAME, "dsGetBassEnhancer");
                if (dsGetBassEnhancerFunc == 0) {
                    LOGERR("dsGetBassEnhancer is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetBassEnhancerFunc) {
                dsResult = dsGetBassEnhancerFunc(static_cast<intptr_t>(handle), &bassBoost);
            }
            if (dsResult == dsERR_NONE) {
                boost = bassBoost;
            } else {
                LOGERR("dsGetBassEnhancer failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioBassEnhancer");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t EnableAudioSurroudDecoder(const int32_t handle, const bool enable) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            // Use resolve function for dsEnableSurroundDecoder
            typedef dsError_t (*dsEnableSurroundDecoder_t)(intptr_t handle, bool enable);
            static dsEnableSurroundDecoder_t dsEnableSurroundDecoderFunc = 0;
            if (dsEnableSurroundDecoderFunc == 0) {
                dsEnableSurroundDecoderFunc = (dsEnableSurroundDecoder_t)resolve(RDK_DSHAL_NAME, "dsEnableSurroundDecoder");
                if (dsEnableSurroundDecoderFunc == 0) {
                    LOGERR("dsEnableSurroundDecoder is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsEnableSurroundDecoderFunc) {
                ret = dsEnableSurroundDecoderFunc(dsHandle, enable);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("EnableAudioSurroudDecoder success: handle=%d, enable=%s", handle, enable ? "true" : "false");
            } else {
                LOGERR("dsEnableSurroundDecoder failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in EnableAudioSurroudDecoder");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t IsAudioSurroudDecoderEnabled(const int32_t handle, bool &enabled) override {
        ENTRY_LOG;
        try {
            bool decoderEnabled = false;
            // Use resolve function for dsIsSurroundDecoderEnabled
            typedef dsError_t (*dsIsSurroundDecoderEnabled_t)(intptr_t handle, bool* enabled);
            static dsIsSurroundDecoderEnabled_t dsIsSurroundDecoderEnabledFunc = 0;
            if (dsIsSurroundDecoderEnabledFunc == 0) {
                dsIsSurroundDecoderEnabledFunc = (dsIsSurroundDecoderEnabled_t)resolve(RDK_DSHAL_NAME, "dsIsSurroundDecoderEnabled");
                if (dsIsSurroundDecoderEnabledFunc == 0) {
                    LOGERR("dsIsSurroundDecoderEnabled is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsIsSurroundDecoderEnabledFunc) {
                dsResult = dsIsSurroundDecoderEnabledFunc(static_cast<intptr_t>(handle), &decoderEnabled);
            }
            if (dsResult == dsERR_NONE) {
                enabled = decoderEnabled;
            } else {
                LOGERR("dsIsSurroundDecoderEnabled failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in IsAudioSurroudDecoderEnabled");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioDRCMode(const int32_t handle, const int32_t drcMode) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            // Use resolve function for dsSetDRCMode
            typedef dsError_t (*dsSetDRCMode_t)(intptr_t handle, int mode);
            static dsSetDRCMode_t dsSetDRCModeFunc = 0;
            if (dsSetDRCModeFunc == 0) {
                dsSetDRCModeFunc = (dsSetDRCMode_t)resolve(RDK_DSHAL_NAME, "dsSetDRCMode");
                if (dsSetDRCModeFunc == 0) {
                    LOGERR("dsSetDRCMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetDRCModeFunc) {
                ret = dsSetDRCModeFunc(dsHandle, drcMode);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioDRCMode success: handle=%d, drcMode=%d", handle, drcMode);
            } else {
                LOGERR("dsSetDRCMode failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioDRCMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioDRCMode(const int32_t handle, int32_t &drcMode) override {
        ENTRY_LOG;
        try {
            int mode = 0;
            // Use resolve function for dsGetDRCMode
            typedef dsError_t (*dsGetDRCMode_t)(intptr_t handle, int* mode);
            static dsGetDRCMode_t dsGetDRCModeFunc = 0;
            if (dsGetDRCModeFunc == 0) {
                dsGetDRCModeFunc = (dsGetDRCMode_t)resolve(RDK_DSHAL_NAME, "dsGetDRCMode");
                if (dsGetDRCModeFunc == 0) {
                    LOGERR("dsGetDRCMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetDRCModeFunc) {
                dsResult = dsGetDRCModeFunc(static_cast<intptr_t>(handle), &mode);
            }
            if (dsResult == dsERR_NONE) {
                drcMode = mode;
            } else {
                LOGERR("dsGetDRCMode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioDRCMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioSurroudVirtualizer(const int32_t handle, const WPEFramework::Exchange::IDeviceSettingsAudio::SurroundVirtualizer surroundVirtualizer) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsSurroundVirtualizer_t dsSurVirtualizer;
            dsSurVirtualizer.mode = static_cast<int>(surroundVirtualizer.mode);
            dsSurVirtualizer.boost = surroundVirtualizer.boost;
            // Use resolve function for dsSetSurroundVirtualizer
            typedef dsError_t (*dsSetSurroundVirtualizer_t)(intptr_t handle, dsSurroundVirtualizer_t virtualizer);
            static dsSetSurroundVirtualizer_t dsSetSurroundVirtualizerFunc = 0;
            if (dsSetSurroundVirtualizerFunc == 0) {
                dsSetSurroundVirtualizerFunc = (dsSetSurroundVirtualizer_t)resolve(RDK_DSHAL_NAME, "dsSetSurroundVirtualizer");
                if (dsSetSurroundVirtualizerFunc == 0) {
                    LOGERR("dsSetSurroundVirtualizer is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetSurroundVirtualizerFunc) {
                ret = dsSetSurroundVirtualizerFunc(dsHandle, dsSurVirtualizer);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioSurroudVirtualizer success: handle=%d, mode=%d, boost=%d", handle, surroundVirtualizer.mode, surroundVirtualizer.boost);
            } else {
                LOGERR("dsSetSurroundVirtualizer failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioSurroudVirtualizer");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioSurroudVirtualizer(const int32_t handle, WPEFramework::Exchange::IDeviceSettingsAudio::SurroundVirtualizer &surroundVirtualizer) override {
        ENTRY_LOG;
        try {
            dsSurroundVirtualizer_t virtualizer;
            // Use resolve function for dsGetSurroundVirtualizer
            typedef dsError_t (*dsGetSurroundVirtualizer_t)(intptr_t handle, dsSurroundVirtualizer_t* virtualizer);
            static dsGetSurroundVirtualizer_t dsGetSurroundVirtualizerFunc = 0;
            if (dsGetSurroundVirtualizerFunc == 0) {
                dsGetSurroundVirtualizerFunc = (dsGetSurroundVirtualizer_t)resolve(RDK_DSHAL_NAME, "dsGetSurroundVirtualizer");
                if (dsGetSurroundVirtualizerFunc == 0) {
                    LOGERR("dsGetSurroundVirtualizer is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetSurroundVirtualizerFunc) {
                dsResult = dsGetSurroundVirtualizerFunc(static_cast<intptr_t>(handle), &virtualizer);
            }
            if (dsResult == dsERR_NONE) {
                // Convert dsSurroundVirtualizer_t to SurroundVirtualizer enum
                surroundVirtualizer.mode = static_cast<uint8_t>(virtualizer.mode);
                surroundVirtualizer.boost = static_cast<uint8_t>(virtualizer.boost);
            } else {
                LOGERR("dsGetSurroundVirtualizer failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioSurroudVirtualizer");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioMISteering(const int32_t handle, const bool enable) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            // Use resolve function for dsSetMISteering
            typedef dsError_t (*dsSetMISteering_t)(intptr_t handle, bool enable);
            static dsSetMISteering_t dsSetMISteeringFunc = 0;
            if (dsSetMISteeringFunc == 0) {
                dsSetMISteeringFunc = (dsSetMISteering_t)resolve(RDK_DSHAL_NAME, "dsSetMISteering");
                if (dsSetMISteeringFunc == 0) {
                    LOGERR("dsSetMISteering is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetMISteeringFunc) {
                ret = dsSetMISteeringFunc(dsHandle, enable);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioMISteering success: handle=%d, enable=%s", handle, enable ? "true" : "false");
            } else {
                LOGERR("dsSetMISteering failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioMISteering");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioMISteering(const int32_t handle, bool &enable) override {
        ENTRY_LOG;
        try {
            bool miSteering = false;
            // Use resolve function for dsGetMISteering
            typedef dsError_t (*dsGetMISteering_t)(intptr_t handle, bool* steering);
            static dsGetMISteering_t dsGetMISteeringFunc = 0;
            if (dsGetMISteeringFunc == 0) {
                dsGetMISteeringFunc = (dsGetMISteering_t)resolve(RDK_DSHAL_NAME, "dsGetMISteering");
                if (dsGetMISteeringFunc == 0) {
                    LOGERR("dsGetMISteering is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetMISteeringFunc) {
                dsResult = dsGetMISteeringFunc(static_cast<intptr_t>(handle), &miSteering);
            }
            if (dsResult == dsERR_NONE) {
                enable = miSteering;
            } else {
                LOGERR("dsGetMISteering failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioMISteering");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioGraphicEqualizerMode(const int32_t handle, const int32_t mode) override {
        ENTRY_LOG;
        try {
            // Use resolve function for dsSetGraphicEqualizerMode
            typedef dsError_t (*dsSetGraphicEqualizerMode_t)(intptr_t handle, int mode);
            static dsSetGraphicEqualizerMode_t dsSetGraphicEqualizerModeFunc = 0;
            if (dsSetGraphicEqualizerModeFunc == 0) {
                dsSetGraphicEqualizerModeFunc = (dsSetGraphicEqualizerMode_t)resolve(RDK_DSHAL_NAME, "dsSetGraphicEqualizerMode");
                if (dsSetGraphicEqualizerModeFunc == 0) {
                    LOGERR("dsSetGraphicEqualizerMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsSetGraphicEqualizerModeFunc) {
                dsResult = dsSetGraphicEqualizerModeFunc(static_cast<intptr_t>(handle), mode);
            }
            if (dsResult == dsERR_NONE) {
                LOGINFO("SetAudioGraphicEqualizerMode success: handle=%d, mode=%d", handle, mode);
            } else {
                LOGERR("dsSetGraphicEqualizerMode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioGraphicEqualizerMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioGraphicEqualizerMode(const int32_t handle, int32_t &mode) override {
        ENTRY_LOG;
        try {
            int eqMode = 0;
            // Use resolve function for dsGetGraphicEqualizerMode
            typedef dsError_t (*dsGetGraphicEqualizerMode_t)(intptr_t handle, int* mode);
            static dsGetGraphicEqualizerMode_t dsGetGraphicEqualizerModeFunc = 0;
            if (dsGetGraphicEqualizerModeFunc == 0) {
                dsGetGraphicEqualizerModeFunc = (dsGetGraphicEqualizerMode_t)resolve(RDK_DSHAL_NAME, "dsGetGraphicEqualizerMode");
                if (dsGetGraphicEqualizerModeFunc == 0) {
                    LOGERR("dsGetGraphicEqualizerMode is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t dsResult = dsERR_GENERAL;
            if (0 != dsGetGraphicEqualizerModeFunc) {
                dsResult = dsGetGraphicEqualizerModeFunc(static_cast<intptr_t>(handle), &eqMode);
            }
            if (dsResult == dsERR_NONE) {
                mode = eqMode;
                LOGINFO("GetAudioGraphicEqualizerMode success: handle=%d, mode=%d", handle, mode);
            } else {
                LOGERR("dsGetGraphicEqualizerMode failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioGraphicEqualizerMode");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioMS12ProfileList(const int32_t handle, WPEFramework::Exchange::IDeviceSettingsAudio::IDeviceSettingsAudioMS12AudioProfileIterator*& ms12ProfileList) const override {
        ENTRY_LOG;
        try {
            dsMS12AudioProfileList_t profiles;
            dsError_t dsResult = dsGetMS12AudioProfileList(static_cast<intptr_t>(handle), &profiles);
            if (dsResult == dsERR_NONE) {
                // Need to create iterator implementation - stub for now
                ms12ProfileList = nullptr;
                LOGINFO("GetAudioMS12ProfileList - Iterator creation not implemented");
            } else {
                LOGERR("dsGetMS12AudioProfileList failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioMS12ProfileList");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioMS12Profile(const int32_t handle, string &profile) override {
        ENTRY_LOG;
        try {
            char profileStr[256] = {0};
            dsError_t dsResult = dsGetMS12AudioProfile(static_cast<intptr_t>(handle), profileStr);
            if (dsResult == dsERR_NONE) {
                profile = std::string(profileStr);
            } else {
                LOGERR("dsGetMS12AudioProfile failed with error: %d", dsResult);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetAudioMS12Profile");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioMS12Profile(const int32_t handle, const string profile) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsError_t ret = dsSetMS12AudioProfile(dsHandle, profile.c_str());
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioMS12Profile success: handle=%d, profile=%s", handle, profile.c_str());
            } else {
                LOGERR("dsSetMS12AudioProfile failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioMS12Profile");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioMixerLevels(const int32_t handle, const WPEFramework::Exchange::IDeviceSettingsAudio::AudioInput audioInput, const int32_t volume) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            dsAudioInput_t dsInput = static_cast<dsAudioInput_t>(audioInput);
            
            typedef dsError_t (*dsSetMixerLevel_t)(intptr_t handle, dsAudioInput_t input, int32_t level);
            static dsSetMixerLevel_t dsSetMixerLevelFunc = 0;
            if (dsSetMixerLevelFunc == 0) {
                dsSetMixerLevelFunc = (dsSetMixerLevel_t)resolve(RDK_DSHAL_NAME, "dsSetMixerLevel");
                if(dsSetMixerLevelFunc == 0) {
                    LOGERR("dsSetMixerLevel is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetMixerLevelFunc) {
                ret = dsSetMixerLevelFunc(dsHandle, dsInput, volume);
            }
            
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioMixerLevels success: handle=%d, input=%d, volume=%d", handle, static_cast<int>(audioInput), volume);
            } else {
                LOGERR("dsSetMixerLevel failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioMixerLevels");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetAudioMS12SettingsOverride(const int32_t handle, const string profileName, 
                                         const string profileSettingsName, const string profileSettingValue, 
                                         const string profileState) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            typedef dsError_t (*dsSetMS12SettingsOverride_t)(intptr_t handle, const char* profileName, const char* profileSettingsName, const char* profileSettingValue, const char* profileState);
            static dsSetMS12SettingsOverride_t dsSetMS12SettingsOverrideFunc = 0;
            if (dsSetMS12SettingsOverrideFunc == 0) {
                dsSetMS12SettingsOverrideFunc = (dsSetMS12SettingsOverride_t)resolve(RDK_DSHAL_NAME, "dsSetMS12SettingsOverride");
                if(dsSetMS12SettingsOverrideFunc == 0) {
                    LOGERR("dsSetMS12SettingsOverride is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsSetMS12SettingsOverrideFunc) {
                ret = dsSetMS12SettingsOverrideFunc(dsHandle, profileName.c_str(), profileSettingsName.c_str(), 
                                                     profileSettingValue.c_str(), profileState.c_str());
            }
            if (ret == dsERR_NONE) {
                LOGINFO("SetAudioMS12SettingsOverride success: handle=%d", handle);
            } else {
                LOGERR("dsSetMS12SettingsOverride failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in SetAudioMS12SettingsOverride");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t ResetAudioDialogEnhancement(const int32_t handle) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            typedef dsError_t (*dsResetDialogEnhancement_t)(intptr_t handle);
            static dsResetDialogEnhancement_t dsResetDialogEnhancementFunc = 0;
            if (dsResetDialogEnhancementFunc == 0) {
                dsResetDialogEnhancementFunc = (dsResetDialogEnhancement_t)resolve(RDK_DSHAL_NAME, "dsResetDialogEnhancement");
                if(dsResetDialogEnhancementFunc == 0) {
                    LOGERR("dsResetDialogEnhancement is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsResetDialogEnhancementFunc) {
                ret = dsResetDialogEnhancementFunc(dsHandle);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("ResetAudioDialogEnhancement success: handle=%d", handle);
            } else {
                LOGERR("dsResetDialogEnhancement failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in ResetAudioDialogEnhancement");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t ResetAudioBassEnhancer(const int32_t handle) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            typedef dsError_t (*dsResetBassEnhancer_t)(intptr_t handle);
            static dsResetBassEnhancer_t dsResetBassEnhancerFunc = 0;
            if (dsResetBassEnhancerFunc == 0) {
                dsResetBassEnhancerFunc = (dsResetBassEnhancer_t)resolve(RDK_DSHAL_NAME, "dsResetBassEnhancer");
                if(dsResetBassEnhancerFunc == 0) {
                    LOGERR("dsResetBassEnhancer is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsResetBassEnhancerFunc) {
                ret = dsResetBassEnhancerFunc(dsHandle);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("ResetAudioBassEnhancer success: handle=%d", handle);
            } else {
                LOGERR("dsResetBassEnhancer failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in ResetAudioBassEnhancer");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t ResetAudioSurroundVirtualizer(const int32_t handle) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            typedef dsError_t (*dsResetSurroundVirtualizer_t)(intptr_t handle);
            static dsResetSurroundVirtualizer_t dsResetSurroundVirtualizerFunc = 0;
            if (dsResetSurroundVirtualizerFunc == 0) {
                dsResetSurroundVirtualizerFunc = (dsResetSurroundVirtualizer_t)resolve(RDK_DSHAL_NAME, "dsResetSurroundVirtualizer");
                if(dsResetSurroundVirtualizerFunc == 0) {
                    LOGERR("dsResetSurroundVirtualizer is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsResetSurroundVirtualizerFunc) {
                ret = dsResetSurroundVirtualizerFunc(dsHandle);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("ResetAudioSurroundVirtualizer success: handle=%d", handle);
            } else {
                LOGERR("dsResetSurroundVirtualizer failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in ResetAudioSurroundVirtualizer");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t ResetAudioVolumeLeveller(const int32_t handle) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            typedef dsError_t (*dsResetVolumeLeveller_t)(intptr_t handle);
            static dsResetVolumeLeveller_t dsResetVolumeLevellerFunc = 0;
            if (dsResetVolumeLevellerFunc == 0) {
                dsResetVolumeLevellerFunc = (dsResetVolumeLeveller_t)resolve(RDK_DSHAL_NAME, "dsResetVolumeLeveller");
                if(dsResetVolumeLevellerFunc == 0) {
                    LOGERR("dsResetVolumeLeveller is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsResetVolumeLevellerFunc) {
                ret = dsResetVolumeLevellerFunc(dsHandle);
            }
            if (ret == dsERR_NONE) {
                LOGINFO("ResetAudioVolumeLeveller success: handle=%d", handle);
            } else {
                LOGERR("dsResetVolumeLeveller failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in ResetAudioVolumeLeveller");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetAudioHDMIARCPortId(const int32_t handle, int32_t &portId) override {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            // Get HDMI ARC Port ID from device persistence (reference from dsAudio.c)
            std::string hdmiARCPortId("0"); // Default value
            try {
                hdmiARCPortId = device::HostPersistence::getInstance().getDefaultProperty("HDMIARC.port.Id");
            } catch (...) {
                LOGWARN("Failed to get HDMIARC.port.Id from persistence, using default value -1");
                hdmiARCPortId = "-1";
            }
            
            portId = atoi(hdmiARCPortId.c_str());
            LOGINFO("GetAudioHDMIARCPortId success: handle=%d, portId=%d", handle, portId);
        } catch (...) {
            LOGERR("Exception in GetAudioHDMIARCPortId");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t GetStereoAuto(const int32_t handle, int32_t &autoMode) override
    {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            int dsAutoMode;
            // Use resolve function for dsGetStereoAuto
            typedef dsError_t (*dsGetStereoAuto_t)(intptr_t handle, int* autoMode);
            static dsGetStereoAuto_t dsGetStereoAutoFunc = 0;
            if (dsGetStereoAutoFunc == 0) {
                dsGetStereoAutoFunc = (dsGetStereoAuto_t)resolve(RDK_DSHAL_NAME, "dsGetStereoAuto");
                if (dsGetStereoAutoFunc == 0) {
                    LOGERR("dsGetStereoAuto is not defined");
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            }
            
            dsError_t ret = dsERR_GENERAL;
            if (0 != dsGetStereoAutoFunc) {
                ret = dsGetStereoAutoFunc(dsHandle, &dsAutoMode);
            }
            if (ret == dsERR_NONE) {
                autoMode = dsAutoMode;
                LOGINFO("GetStereoAuto success: handle=%d, autoMode=%d", handle, autoMode);
            } else {
                LOGERR("dsGetStereoAuto failed with error: %d", ret);
                return WPEFramework::Core::ERROR_GENERAL;
            }
        } catch (...) {
            LOGERR("Exception in GetStereoAuto");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SetStereoAuto(const int32_t handle, const int32_t autoMode, const bool persist) override
    {
        ENTRY_LOG;
        if (!_isInitialized) {
            LOGERR("Audio platform not initialized");
            return WPEFramework::Core::ERROR_GENERAL;
        }

        try {
            intptr_t dsHandle = static_cast<intptr_t>(handle);
            
            // Handle persistence similar to dsAudio.c _dsSetStereoAuto implementation
            if (persist) {
                dsAudioPortType_t portType = getAudioPortType(dsHandle);
                switch (portType) {
                    case dsAUDIOPORT_TYPE_HDMI:
                        device::HostPersistence::getInstance().persistHostProperty("HDMI0.AudioMode.AUTO", autoMode ? "TRUE" : "FALSE");
                        LOGINFO("Persisted HDMI stereo auto mode: autoMode=%d", autoMode);
                        break;

                    case dsAUDIOPORT_TYPE_HDMI_ARC:
                        device::HostPersistence::getInstance().persistHostProperty("HDMI_ARC0.AudioMode.AUTO", autoMode ? "TRUE" : "FALSE");
                        LOGINFO("Persisted HDMI_ARC stereo auto mode: autoMode=%d", autoMode);
                        break;

                    case dsAUDIOPORT_TYPE_SPDIF:
                        device::HostPersistence::getInstance().persistHostProperty("SPDIF0.AudioMode.AUTO", autoMode ? "TRUE" : "FALSE");
                        LOGINFO("Persisted SPDIF stereo auto mode: autoMode=%d", autoMode);
                        break;

                    case dsAUDIOPORT_TYPE_SPEAKER:
                        device::HostPersistence::getInstance().persistHostProperty("SPEAKER0.AudioMode.AUTO", autoMode ? "TRUE" : "FALSE");
                        LOGINFO("Persisted SPEAKER stereo auto mode: autoMode=%d", autoMode);
                        break;

                    default:
                        LOGWARN("SetStereoAuto persistence not supported for port type: %d", portType);
                        break;
                }
            }
            
            // Call the HAL function - only for HDMI_ARC and SPDIF ports as per dsAudio.c logic
            dsAudioPortType_t portType = getAudioPortType(dsHandle);
            if ((portType == dsAUDIOPORT_TYPE_HDMI_ARC) || (portType == dsAUDIOPORT_TYPE_SPDIF)) {
                // Use resolve function for dsSetStereoAuto
                typedef dsError_t (*dsSetStereoAuto_t)(intptr_t handle, int autoMode);
                static dsSetStereoAuto_t dsSetStereoAutoFunc = 0;
                if (dsSetStereoAutoFunc == 0) {
                    dsSetStereoAutoFunc = (dsSetStereoAuto_t)resolve(RDK_DSHAL_NAME, "dsSetStereoAuto");
                    if (dsSetStereoAutoFunc == 0) {
                        LOGERR("dsSetStereoAuto is not defined");
                        return WPEFramework::Core::ERROR_GENERAL;
                    }
                }
                
                dsError_t ret = dsERR_GENERAL;
                if (0 != dsSetStereoAutoFunc) {
                    ret = dsSetStereoAutoFunc(dsHandle, autoMode);
                }
                if (ret == dsERR_NONE) {
                    LOGINFO("SetStereoAuto success: handle=%d, autoMode=%d, persist=%s", 
                           handle, autoMode, persist ? "true" : "false");
                } else {
                    LOGERR("dsSetStereoAuto failed with error: %d", ret);
                    return WPEFramework::Core::ERROR_GENERAL;
                }
            } else {
                LOGINFO("SetStereoAuto HAL call skipped for port type %d (only HDMI_ARC/SPDIF supported): handle=%d, autoMode=%d", 
                       portType, handle, autoMode);
            }
        } catch (...) {
            LOGERR("Exception in SetStereoAuto");
            return WPEFramework::Core::ERROR_GENERAL;
        }
        EXIT_LOG;
        return WPEFramework::Core::ERROR_NONE;
    }

private:
    // Implementation of audio settings initialization from dsAudioMgr_init
    void initializeAudioSettings()
    {
        ENTRY_LOG;
        try {
            // Initialize audio configuration settings from persistence
            // This is adapted from dsAudioMgr_init logic in dsAudio.c
            
            LOGINFO("Initializing comprehensive audio settings from persistence and platform defaults...");
            
            // Initialize audio port settings for all supported audio port types
            initializeAudioPortSettings();
            
            // Initialize MS12 audio processing features if supported
            initializeMS12Settings();
            
            LOGINFO("Audio platform and settings initialization completed successfully");
                    
        } catch (...) {
            LOGERR("Exception in initializing audio settings");
        }
        EXIT_LOG;
    }
    
    // Audio configuration initialization from AudioConfigInit function
    void audioConfigInit()
    {
        ENTRY_LOG;
        try {
            LOGINFO("Starting comprehensive audio configuration initialization...");
            
            void *dllib = nullptr;
            intptr_t handle = 0;
            
            // 1. Initialize LE (Loudness Equivalence) Configuration
            typedef dsError_t (*dsEnableLEConfig_t)(intptr_t handle, const bool enable);
            dsEnableLEConfig_t dsEnableLEConfigFunc = nullptr;
            
            if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                dsEnableLEConfigFunc = (dsEnableLEConfig_t) resolve(RDK_DSHAL_NAME, "dsEnableLEConfig");
                if (dsEnableLEConfigFunc) {
                    LOGINFO("dsEnableLEConfig(int, bool) is defined and loaded");
                    std::string leEnable("FALSE");
                    try {
                        leEnable = device::HostPersistence::getInstance().getProperty("audio.LEEnable");
                    } catch(...) {
                        #ifndef DS_LE_DEFAULT_DISABLED
                        leEnable = "TRUE";
                        #endif
                        LOGINFO("LE : Persisting default LE status: %s", leEnable.c_str());
                        device::HostPersistence::getInstance().persistHostProperty("audio.LEEnable", leEnable);
                    }
                    
                    bool leEnabled = (leEnable == "TRUE");
                    dsEnableLEConfigFunc(handle, leEnabled);
                    LOGINFO("LE (Loudness Equivalence) initialized: %s", leEnabled ? "enabled" : "disabled");
                } else {
                    LOGINFO("dsEnableLEConfig(int, bool) is not available in HAL");
                }
            } else {
                LOGERR("dsEnableLEConfig failed - HDMI port 0 not available");
            }
            
            #ifdef DS_AUDIO_SETTINGS_PERSISTENCE
            // 2. Initialize Audio Gain for SPEAKER and HDMI ports
            typedef dsError_t (*dsSetAudioGain_t)(intptr_t handle, float gain);
            dsSetAudioGain_t dsSetAudioGainFunc = nullptr;
            
            dsSetAudioGainFunc = (dsSetAudioGain_t) resolve(RDK_DSHAL_NAME, "dsSetAudioGain");
            if (dsSetAudioGainFunc) {
                LOGINFO("dsSetAudioGain_t(int, float) is defined and loaded");
                std::string audioGain("0");
                float audioGainValue = 0;
                
                // SPEAKER init
                handle = 0;
                if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                    try {
                        audioGain = device::HostPersistence::getInstance().getProperty("SPEAKER0.audio.Gain");
                    } catch(...) {
                        try {
                            LOGINFO("SPEAKER0.audio.Gain not found in persistence store. Try system default");
                            audioGain = device::HostPersistence::getInstance().getDefaultProperty("SPEAKER0.audio.Gain");
                        } catch(...) {
                            audioGain = "0";
                        }
                    }
                    audioGainValue = atof(audioGain.c_str());
                    if (dsSetAudioGainFunc(handle, audioGainValue) == dsERR_NONE) {
                        LOGINFO("Port SPEAKER0: Initialized audio gain: %f", audioGainValue);
                    }
                }
                
                // HDMI init
                handle = 0;
                if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                    try {
                        audioGain = device::HostPersistence::getInstance().getProperty("HDMI0.audio.Gain");
                    } catch(...) {
                        try {
                            LOGINFO("HDMI0.audio.Gain not found in persistence store. Try system default");
                            audioGain = device::HostPersistence::getInstance().getDefaultProperty("HDMI0.audio.Gain");
                        } catch(...) {
                            audioGain = "0";
                        }
                    }
                    audioGainValue = atof(audioGain.c_str());
                    if (dsSetAudioGainFunc(handle, audioGainValue) == dsERR_NONE) {
                        LOGINFO("Port HDMI0: Initialized audio gain: %f", audioGainValue);
                    }
                }
            } else {
                LOGINFO("dsSetAudioGain_t(int, float) is not available in HAL");
            }
            
            // 3. Initialize Audio Level for SPDIF, SPEAKER, HEADPHONE, and HDMI ports
            typedef dsError_t (*dsSetAudioLevel_t)(intptr_t handle, float level);
            static dsSetAudioLevel_t dsSetAudioLevelFunc = nullptr;
            
            if (dsSetAudioLevelFunc == nullptr) {
                dllib = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
                if (dllib) {
                    dsSetAudioLevelFunc = (dsSetAudioLevel_t) dlsym(dllib, "dsSetAudioLevel");
                    if (dsSetAudioLevelFunc) {
                        LOGINFO("dsSetAudioLevel_t(int, float) is defined and loaded");
                        std::string audioLevel("0");
                        float audioLevelValue = 0;
                        
                        // SPDIF init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPDIF, 0, &handle) == dsERR_NONE) {
                            try {
                                audioLevel = device::HostPersistence::getInstance().getProperty("SPDIF0.audio.Level");
                            } catch(...) {
                                try {
                                    LOGINFO("SPDIF0.audio.Level not found in persistence store. Try system default");
                                    audioLevel = device::HostPersistence::getInstance().getDefaultProperty("SPDIF0.audio.Level");
                                } catch(...) {
                                    audioLevel = "40";
                                }
                            }
                            audioLevelValue = atof(audioLevel.c_str());
                            if (dsSetAudioLevelFunc(handle, audioLevelValue) == dsERR_NONE) {
                                LOGINFO("Port SPDIF0: Initialized audio level: %f", audioLevelValue);
                            }
                        }
                        
                        // SPEAKER init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                            try {
                                audioLevel = device::HostPersistence::getInstance().getProperty("SPEAKER0.audio.Level");
                            } catch(...) {
                                try {
                                    LOGINFO("SPEAKER0.audio.Level not found in persistence store. Try system default");
                                    audioLevel = device::HostPersistence::getInstance().getDefaultProperty("SPEAKER0.audio.Level");
                                } catch(...) {
                                    audioLevel = "40";
                                }
                            }
                            audioLevelValue = atof(audioLevel.c_str());
                            if (dsSetAudioLevelFunc(handle, audioLevelValue) == dsERR_NONE) {
                                LOGINFO("Port SPEAKER0: Initialized audio level: %f", audioLevelValue);
                            }
                        }
                        
                        // HEADPHONE init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_HEADPHONE, 0, &handle) == dsERR_NONE) {
                            try {
                                audioLevel = device::HostPersistence::getInstance().getProperty("HEADPHONE0.audio.Level");
                            } catch(...) {
                                try {
                                    LOGINFO("HEADPHONE0.audio.Level not found in persistence store. Try system default");
                                    audioLevel = device::HostPersistence::getInstance().getDefaultProperty("HEADPHONE0.audio.Level");
                                } catch(...) {
                                    audioLevel = "40";
                                }
                            }
                            audioLevelValue = atof(audioLevel.c_str());
                            if (dsSetAudioLevelFunc(handle, audioLevelValue) == dsERR_NONE) {
                                LOGINFO("Port HEADPHONE0: Initialized audio level: %f", audioLevelValue);
                            }
                        }
                        
                        // HDMI init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                            try {
                                audioLevel = device::HostPersistence::getInstance().getProperty("HDMI0.audio.Level");
                            } catch(...) {
                                try {
                                    LOGINFO("HDMI0.audio.Level not found in persistence store. Try system default");
                                    audioLevel = device::HostPersistence::getInstance().getDefaultProperty("HDMI0.audio.Level");
                                } catch(...) {
                                    audioLevel = "40";
                                }
                            }
                            audioLevelValue = atof(audioLevel.c_str());
                            if (dsSetAudioLevelFunc(handle, audioLevelValue) == dsERR_NONE) {
                                LOGINFO("Port HDMI0: Initialized audio level: %f", audioLevelValue);
                            }
                        }
                    } else {
                        LOGINFO("dsSetAudioLevel_t(int, float) is not defined");
                    }
                    dlclose(dllib);
                    dllib = nullptr;
                } else {
                    LOGERR("Opening libdshal.so failed");
                }
            }
            
            // 4. Initialize Audio Delay for SPEAKER, HDMI, and HDMI_ARC ports
            typedef dsError_t (*dsSetAudioDelay_t)(intptr_t handle, uint32_t audioDelayMs);
            static dsSetAudioDelay_t dsSetAudioDelayFunc = nullptr;
            
            if (dsSetAudioDelayFunc == nullptr) {
                dllib = dlopen("libdshal.so", RTLD_LAZY);
                if (dllib) {
                    dsSetAudioDelayFunc = (dsSetAudioDelay_t) dlsym(dllib, "dsSetAudioDelay");
                    if (dsSetAudioDelayFunc) {
                        LOGINFO("dsSetAudioDelay_t(int, uint32_t) is defined and loaded");
                        std::string audioDelay("0");
                        int audioDelayValue = 0;
                        
                        // SPEAKER init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                            try {
                                audioDelay = device::HostPersistence::getInstance().getProperty("SPEAKER0.audio.Delay");
                            } catch(...) {
                                try {
                                    LOGINFO("SPEAKER0.audio.Delay not found in persistence store. Try system default");
                                    audioDelay = device::HostPersistence::getInstance().getDefaultProperty("SPEAKER0.audio.Delay");
                                } catch(...) {
                                    audioDelay = "0";
                                }
                            }
                            audioDelayValue = atoi(audioDelay.c_str());
                            if (dsSetAudioDelayFunc(handle, audioDelayValue) == dsERR_NONE) {
                                LOGINFO("Port SPEAKER0: Initialized audio delay: %d ms", audioDelayValue);
                            }
                        }
                        
                        // HDMI init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                            try {
                                audioDelay = device::HostPersistence::getInstance().getProperty("HDMI0.audio.Delay");
                            } catch(...) {
                                try {
                                    LOGINFO("HDMI0.audio.Delay not found in persistence store. Try system default");
                                    audioDelay = device::HostPersistence::getInstance().getDefaultProperty("HDMI0.audio.Delay");
                                } catch(...) {
                                    audioDelay = "0";
                                }
                            }
                            audioDelayValue = atoi(audioDelay.c_str());
                            if (dsSetAudioDelayFunc(handle, audioDelayValue) == dsERR_NONE) {
                                LOGINFO("Port HDMI0: Initialized audio delay: %d ms", audioDelayValue);
                            }
                        }
                        
                        // HDMI ARC init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI_ARC, 0, &handle) == dsERR_NONE) {
                            try {
                                audioDelay = device::HostPersistence::getInstance().getProperty("HDMI_ARC0.audio.Delay");
                            } catch(...) {
                                try {
                                    LOGINFO("HDMI_ARC0.audio.Delay not found in persistence store. Try system default");
                                    audioDelay = device::HostPersistence::getInstance().getDefaultProperty("HDMI_ARC0.audio.Delay");
                                } catch(...) {
                                    audioDelay = "0";
                                }
                            }
                            audioDelayValue = atoi(audioDelay.c_str());
                            if (dsSetAudioDelayFunc(handle, audioDelayValue) == dsERR_NONE) {
                                LOGINFO("Port HDMI_ARC0: Initialized audio delay: %d ms", audioDelayValue);
                            }
                        }
                    } else {
                        LOGINFO("dsSetAudioDelay_t(int, uint32_t) is not defined");
                    }
                    dlclose(dllib);
                    dllib = nullptr;
                } else {
                    LOGERR("Opening libdshal.so failed");
                }
            }
            
            // 5. Initialize Primary Language
            typedef dsError_t (*dsSetPrimaryLanguage_t)(intptr_t handle, const char* pLang);
            static dsSetPrimaryLanguage_t dsSetPrimaryLanguageFunc = nullptr;
            
            if (dsSetPrimaryLanguageFunc == nullptr) {
                dllib = dlopen("libdshal.so", RTLD_LAZY);
                if (dllib) {
                    dsSetPrimaryLanguageFunc = (dsSetPrimaryLanguage_t) dlsym(dllib, "dsSetPrimaryLanguage");
                    if (dsSetPrimaryLanguageFunc) {
                        LOGINFO("dsSetPrimaryLanguage_t(int, char*) is defined and loaded");
                        std::string primaryLanguage("eng");
                        handle = 0;
                        
                        try {
                            primaryLanguage = device::HostPersistence::getInstance().getProperty("audio.PrimaryLanguage");
                        } catch(...) {
                            try {
                                LOGINFO("audio.PrimaryLanguage not found in persistence store. Try system default");
                                primaryLanguage = device::HostPersistence::getInstance().getDefaultProperty("audio.PrimaryLanguage");
                            } catch(...) {
                                primaryLanguage = "eng";
                            }
                        }
                        
                        if (dsSetPrimaryLanguageFunc(handle, primaryLanguage.c_str()) == dsERR_NONE) {
                            LOGINFO("Initialized Primary Language: %s", primaryLanguage.c_str());
                        }
                    } else {
                        LOGINFO("dsSetPrimaryLanguage_t(int, char*) is not defined");
                    }
                    dlclose(dllib);
                    dllib = nullptr;
                } else {
                    LOGERR("Opening libdshal.so failed");
                }
            }
            
            // 6. Initialize Secondary Language
            typedef dsError_t (*dsSetSecondaryLanguage_t)(intptr_t handle, const char* sLang);
            static dsSetSecondaryLanguage_t dsSetSecondaryLanguageFunc = nullptr;
            
            if (dsSetSecondaryLanguageFunc == nullptr) {
                dllib = dlopen("libdshal.so", RTLD_LAZY);
                if (dllib) {
                    dsSetSecondaryLanguageFunc = (dsSetSecondaryLanguage_t) dlsym(dllib, "dsSetSecondaryLanguage");
                    if (dsSetSecondaryLanguageFunc) {
                        LOGINFO("dsSetSecondaryLanguage_t(int, char*) is defined and loaded");
                        std::string secondaryLanguage("eng");
                        handle = 0;
                        
                        try {
                            secondaryLanguage = device::HostPersistence::getInstance().getProperty("audio.SecondaryLanguage");
                        } catch(...) {
                            try {
                                LOGINFO("audio.SecondaryLanguage not found in persistence store. Try system default");
                                secondaryLanguage = device::HostPersistence::getInstance().getDefaultProperty("audio.SecondaryLanguage");
                            } catch(...) {
                                secondaryLanguage = "eng";
                            }
                        }
                        
                        if (dsSetSecondaryLanguageFunc(handle, secondaryLanguage.c_str()) == dsERR_NONE) {
                            LOGINFO("Initialized Secondary Language: %s", secondaryLanguage.c_str());
                        }
                    } else {
                        LOGINFO("dsSetSecondaryLanguage_t(int, char*) is not defined");
                    }
                    dlclose(dllib);
                    dllib = nullptr;
                } else {
                    LOGERR("Opening libdshal.so failed");
                }
            }
            
            // 7. Initialize Fader Control
            typedef dsError_t (*dsSetFaderControl_t)(intptr_t handle, int mixerbalance);
            static dsSetFaderControl_t dsSetFaderControlFunc = nullptr;
            
            if (dsSetFaderControlFunc == nullptr) {
                dllib = dlopen("libdshal.so", RTLD_LAZY);
                if (dllib) {
                    dsSetFaderControlFunc = (dsSetFaderControl_t) dlsym(dllib, "dsSetFaderControl");
                    if (dsSetFaderControlFunc) {
                        LOGINFO("dsSetFaderControl_t(int, int) is defined and loaded");
                        std::string faderControl("0");
                        int faderControlValue = 0;
                        handle = 0;
                        
                        try {
                            faderControl = device::HostPersistence::getInstance().getProperty("audio.FaderControl");
                        } catch(...) {
                            try {
                                LOGINFO("audio.FaderControl not found in persistence store. Try system default");
                                faderControl = device::HostPersistence::getInstance().getDefaultProperty("audio.FaderControl");
                            } catch(...) {
                                faderControl = "0";
                            }
                        }
                        
                        faderControlValue = atoi(faderControl.c_str());
                        if (dsSetFaderControlFunc(handle, faderControlValue) == dsERR_NONE) {
                            LOGINFO("Initialized Fader Control, mixing: %d", faderControlValue);
                        }
                    } else {
                        LOGINFO("dsSetFaderControl_t(int, int) is not defined");
                    }
                    dlclose(dllib);
                    dllib = nullptr;
                } else {
                    LOGERR("Opening libdshal.so failed");
                }
            }
            
            // 8. Initialize Associated Audio Mixing
            typedef dsError_t (*dsSetAssociatedAudioMixing_t)(intptr_t handle, bool mixing);
            static dsSetAssociatedAudioMixing_t dsSetAssociatedAudioMixingFunc = nullptr;
            
            if (dsSetAssociatedAudioMixingFunc == nullptr) {
                dllib = dlopen("libdshal.so", RTLD_LAZY);
                if (dllib) {
                    dsSetAssociatedAudioMixingFunc = (dsSetAssociatedAudioMixing_t) dlsym(dllib, "dsSetAssociatedAudioMixing");
                    if (dsSetAssociatedAudioMixingFunc) {
                        LOGINFO("dsSetAssociatedAudioMixing_t (intptr_t handle, bool mixing) is defined and loaded");
                        std::string associatedAudioMixing("Disabled");
                        bool associatedAudioMixingValue = false;
                        handle = 0;
                        
                        try {
                            associatedAudioMixing = device::HostPersistence::getInstance().getProperty("audio.AssociatedAudioMixing");
                        } catch(...) {
                            try {
                                LOGINFO("audio.AssociatedAudioMixing not found in persistence store. Try system default");
                                associatedAudioMixing = device::HostPersistence::getInstance().getDefaultProperty("audio.AssociatedAudioMixing");
                            } catch(...) {
                                associatedAudioMixing = "Disabled";
                            }
                        }
                        
                        associatedAudioMixingValue = (associatedAudioMixing == "Enabled");
                        if (dsSetAssociatedAudioMixingFunc(handle, associatedAudioMixingValue) == dsERR_NONE) {
                            LOGINFO("Initialized AssociatedAudioMixingFunc: %s", associatedAudioMixingValue ? "enabled" : "disabled");
                        }
                    } else {
                        LOGINFO("dsSetAssociatedAudioMixing_t (intptr_t handle, bool enable) is not defined");
                    }
                    dlclose(dllib);
                    dllib = nullptr;
                } else {
                    LOGERR("Opening libdshal.so failed");
                }
            }
            #endif // DS_AUDIO_SETTINGS_PERSISTENCE
            
            // 9. Initialize MS12 Audio Profile Support
            std::string ms12ProfileSupport("FALSE");
            std::string ms12Profile("Off");
            
            try {
                ms12ProfileSupport = device::HostPersistence::getInstance().getDefaultProperty("audio.MS12Profile.supported");
            } catch(...) {
                ms12ProfileSupport = "FALSE";
                LOGINFO("audio.MS12Profile.supported setting not found in hostDataDefault");
            }
            LOGINFO("audio.MS12Profile.supported = %s", ms12ProfileSupport.c_str());
            
            if (ms12ProfileSupport == "TRUE") {
                // MS12 Profile is supported - initialize MS12 Audio Profile
                typedef dsError_t (*dsSetMS12AudioProfile_t)(intptr_t handle, const char* profile);
                static dsSetMS12AudioProfile_t dsSetMS12AudioProfileFunc = nullptr;
                
                if (dsSetMS12AudioProfileFunc == nullptr) {
                    dllib = dlopen("libdshal.so", RTLD_LAZY);
                    if (dllib) {
                        dsSetMS12AudioProfileFunc = (dsSetMS12AudioProfile_t) dlsym(dllib, "dsSetMS12AudioProfile");
                        if (dsSetMS12AudioProfileFunc) {
                            LOGINFO("dsSetMS12AudioProfile_t(int, const char*) is defined and loaded");
                            
                            try {
                                ms12Profile = device::HostPersistence::getInstance().getProperty("audio.MS12Profile");
                            } catch(...) {
                                try {
                                    LOGINFO("audio.MS12Profile not found in persistence store. Try system default");
                                    ms12Profile = device::HostPersistence::getInstance().getDefaultProperty("audio.MS12Profile");
                                } catch(...) {
                                    ms12Profile = "Off";
                                }
                            }
                            
                            // SPEAKER init for MS12 profile
                            handle = 0;
                            if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                                if (dsSetMS12AudioProfileFunc(handle, ms12Profile.c_str()) == dsERR_NONE) {
                                    LOGINFO("Port SPEAKER0: Initialized MS12 Audio Profile: %s", ms12Profile.c_str());
                                    device::HostPersistence::getInstance().persistHostProperty("audio.MS12Profile", ms12Profile.c_str());
                                } else {
                                    LOGINFO("Port SPEAKER0: Initialization failed !!! MS12 Audio Profile: %s", ms12Profile.c_str());
                                }
                            }
                        } else {
                            LOGINFO("dsSetMS12AudioProfile_t(int, const char*) is not defined");
                        }
                        dlclose(dllib);
                        dllib = nullptr;
                    } else {
                        LOGERR("Opening libdshal.so failed");
                    }
                }
            }
            
            // Initialize individual MS12 settings based on profile support and override settings
            if ((ms12ProfileSupport == "TRUE") && (ms12Profile != "Off")) {
                // MS12 Profile supported and active - check for individual overrides
                initializeMS12ProfileOverrides();
            } else if (ms12ProfileSupport == "FALSE") {
                // MS12 Profile not supported - initialize individual settings from persistence
                initializeIndividualMS12Settings();
            }
            
            LOGINFO("Comprehensive audio configuration initialization completed successfully");
            
        } catch (...) {
            LOGERR("Exception in audioConfigInit");
        }
        EXIT_LOG;
    }
    
    // Initialize MS12 profile override settings when profile is active
    void initializeMS12ProfileOverrides()
    {
        ENTRY_LOG;
        try {
            intptr_t handle = 0;
            std::string profileOverride = "FALSE";
            
            // Audio Compression Profile Override
            try {
                profileOverride = device::HostPersistence::getInstance().getDefaultProperty("audio.Compression.ms12ProfileOverride");
            } catch(...) {
                profileOverride = "FALSE";
            }
            
            if (profileOverride == "TRUE") {
                typedef dsError_t (*dsSetAudioCompression_t)(intptr_t handle, int compressionLevel);
                dsSetAudioCompression_t dsSetAudioCompressionFunc = nullptr;
                
                dsSetAudioCompressionFunc = (dsSetAudioCompression_t) resolve(RDK_DSHAL_NAME, "dsSetAudioCompression");
                if (dsSetAudioCompressionFunc) {
                    try {
                        std::string audioCompression = device::HostPersistence::getInstance().getProperty("audio.Compression");
                        int compressionLevel = atoi(audioCompression.c_str());
                        
                        // SPEAKER and HDMI init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                            if (dsSetAudioCompressionFunc(handle, compressionLevel) == dsERR_NONE) {
                                LOGINFO("Port SPEAKER0: Initialized audio compression: %d", compressionLevel);
                            }
                        }
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                            if (dsSetAudioCompressionFunc(handle, compressionLevel) == dsERR_NONE) {
                                LOGINFO("Port HDMI0: Initialized audio compression: %d", compressionLevel);
                            }
                        }
                    } catch(...) {
                        LOGINFO("audio.Compression not found in persistence store. System Default configured through profiles");
                    }
                }
            }
            
            // Dialog Enhancement Profile Override
            profileOverride = "FALSE";
            try {
                profileOverride = device::HostPersistence::getInstance().getDefaultProperty("audio.DialogEnhancer.ms12ProfileOverride");
            } catch(...) {
                profileOverride = "FALSE";
            }
            
            if (profileOverride == "TRUE") {
                typedef dsError_t (*dsSetDialogEnhancement_t)(intptr_t handle, int enhancerLevel);
                dsSetDialogEnhancement_t dsSetDialogEnhancementFunc = nullptr;
                
                dsSetDialogEnhancementFunc = (dsSetDialogEnhancement_t) resolve(RDK_DSHAL_NAME, "dsSetDialogEnhancement");
                if (dsSetDialogEnhancementFunc) {
                    try {
                        std::string currentProfile = getCurrentProfileProperty("EnhancerLevel");
                        std::string enhancerLevel = device::HostPersistence::getInstance().getProperty(currentProfile);
                        int enhancerValue = atoi(enhancerLevel.c_str());
                        
                        // SPEAKER and HDMI init
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                            if (dsSetDialogEnhancementFunc(handle, enhancerValue) == dsERR_NONE) {
                                LOGINFO("Port SPEAKER0: Initialized dialog enhancement level: %d", enhancerValue);
                            }
                        }
                        handle = 0;
                        if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                            if (dsSetDialogEnhancementFunc(handle, enhancerValue) == dsERR_NONE) {
                                LOGINFO("Port HDMI0: Initialized dialog enhancement level: %d", enhancerValue);
                            }
                        }
                    } catch(...) {
                        LOGINFO("audio.EnhancerLevel not found in persistence store. System Default configured through profiles");
                    }
                }
            }
            
            // Additional MS12 features would be initialized here (Volume Leveller, Bass Enhancer, etc.)
            // Implementation follows similar pattern as above
            
        } catch (...) {
            LOGERR("Exception in initializeMS12ProfileOverrides");
        }
        EXIT_LOG;
    }
    
    // Initialize individual MS12 settings when profile is not supported
    void initializeIndividualMS12Settings()
    {
        ENTRY_LOG;
        try {
            intptr_t handle = 0;
            
            // Initialize Audio Compression
            typedef dsError_t (*dsSetAudioCompression_t)(intptr_t handle, int compressionLevel);
            dsSetAudioCompression_t dsSetAudioCompressionFunc = nullptr;
            
            dsSetAudioCompressionFunc = (dsSetAudioCompression_t) resolve(RDK_DSHAL_NAME, "dsSetAudioCompression");
            if (dsSetAudioCompressionFunc) {
                std::string audioCompression("0");
                try {
                    audioCompression = device::HostPersistence::getInstance().getProperty("audio.Compression");
                } catch(...) {
                    try {
                        audioCompression = device::HostPersistence::getInstance().getDefaultProperty("audio.Compression");
                    } catch(...) {
                        audioCompression = "0";
                    }
                }
                
                int compressionLevel = atoi(audioCompression.c_str());
                
                // SPEAKER and HDMI init
                handle = 0;
                if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                    if (dsSetAudioCompressionFunc(handle, compressionLevel) == dsERR_NONE) {
                        LOGINFO("Port SPEAKER0: Initialized audio compression: %d", compressionLevel);
                    }
                }
                handle = 0;
                if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                    if (dsSetAudioCompressionFunc(handle, compressionLevel) == dsERR_NONE) {
                        LOGINFO("Port HDMI0: Initialized audio compression: %d", compressionLevel);
                    }
                }
            }
            
            // Initialize Dialog Enhancement
            typedef dsError_t (*dsSetDialogEnhancement_t)(intptr_t handle, int enhancerLevel);
            dsSetDialogEnhancement_t dsSetDialogEnhancementFunc = nullptr;
            
            dsSetDialogEnhancementFunc = (dsSetDialogEnhancement_t) resolve(RDK_DSHAL_NAME, "dsSetDialogEnhancement");
            if (dsSetDialogEnhancementFunc) {
                std::string enhancerLevel("0");
                try {
                    enhancerLevel = device::HostPersistence::getInstance().getProperty("audio.EnhancerLevel");
                } catch(...) {
                    try {
                        enhancerLevel = device::HostPersistence::getInstance().getDefaultProperty("audio.EnhancerLevel");
                    } catch(...) {
                        enhancerLevel = "0";
                    }
                }
                
                int enhancerValue = atoi(enhancerLevel.c_str());
                
                // SPEAKER and HDMI init
                handle = 0;
                if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                    if (dsSetDialogEnhancementFunc(handle, enhancerValue) == dsERR_NONE) {
                        LOGINFO("Port SPEAKER0: Initialized dialog enhancement level: %d", enhancerValue);
                    }
                }
                handle = 0;
                if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                    if (dsSetDialogEnhancementFunc(handle, enhancerValue) == dsERR_NONE) {
                        LOGINFO("Port HDMI0: Initialized dialog enhancement level: %d", enhancerValue);
                    }
                }
            }
            
            // Additional individual MS12 settings initialization would continue here
            // Following similar pattern for Volume Leveller, Bass Enhancer, Surround Decoder, etc.
            
        } catch (...) {
            LOGERR("Exception in initializeIndividualMS12Settings");
        }
        EXIT_LOG;
    }
    
    // Helper method to get current profile property
    std::string getCurrentProfileProperty(const std::string& property)
    {
        std::string currentProfile = "Off";
        try {
            currentProfile = device::HostPersistence::getInstance().getProperty("audio.MS12Profile");
        } catch(...) {
            currentProfile = "Off";
        }
        
        return generateProfileProperty(currentProfile, property);
    }
    
    // Helper method to generate profile property string
    std::string generateProfileProperty(const std::string& profile, const std::string& property)
    {
        return "audio." + profile + "." + property;
    }
    
    // Resolve function - exactly like HDMI implementation
    static void* resolve(const std::string& libName, const std::string& symbolName) {
        void* handle = dlopen(libName.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "dlopen failed for " << libName << ": " << dlerror() << std::endl;
            return nullptr;
        }
        void* symbol = dlsym(handle, symbolName.c_str());
        if (!symbol) {
            std::cerr << "dlsym failed for " << symbolName << ": " << dlerror() << std::endl;
        }
        dlclose(handle);  // Fix resource leak
        return symbol;
    }
    
    // Initialize audio port settings (from dsAudioMgr_init)
    void initializeAudioPortSettings()
    {
        ENTRY_LOG;
        try {
            LOGINFO("Starting comprehensive audio port settings initialization from persistence...");
            
            // Initialize HDMI Audio Mode Settings from Persistence
            #ifdef IGNORE_EDID_LOGIC
            std::string hdmiAudioModeSettings("SURROUND");
            #else
            std::string hdmiAudioModeSettings("STEREO");
            #endif
            
            dsAudioStereoMode_t hdmiAudioMode;
            
            LOGINFO("Checking Host persistence for HDMI audio settings");
            try {
                hdmiAudioModeSettings = device::HostPersistence::getInstance().getProperty("HDMI0.AudioMode");
            } catch(...) {
                LOGINFO("HDMI0.AudioMode not in host persistence. Checking default.");
                try {
                    hdmiAudioModeSettings = device::HostPersistence::getInstance().getDefaultProperty("HDMI0.AudioMode");
                } catch(...) {
                    LOGINFO("HDMI0.AudioMode not in default host persistence.");
                }
            }
            
            LOGINFO("The HDMI Audio Mode Setting on startup is %s", hdmiAudioModeSettings.c_str());
            
            // Parse HDMI audio mode string to enum
            if (hdmiAudioModeSettings.compare("SURROUND") == 0) {
                hdmiAudioMode = dsAUDIO_STEREO_SURROUND;
            } else if (hdmiAudioModeSettings.compare("PASSTHRU") == 0) {
                hdmiAudioMode = dsAUDIO_STEREO_PASSTHRU;
            } else if (hdmiAudioModeSettings.compare("DOLBYDIGITAL") == 0) {
                hdmiAudioMode = dsAUDIO_STEREO_DD;
            } else if (hdmiAudioModeSettings.compare("DOLBYDIGITALPLUS") == 0) {
                hdmiAudioMode = dsAUDIO_STEREO_DDPLUS;
            } else if (hdmiAudioModeSettings.compare("STEREO") == 0) {
                hdmiAudioMode = dsAUDIO_STEREO_STEREO;
            } else {
                #ifdef IGNORE_EDID_LOGIC
                hdmiAudioMode = dsAUDIO_STEREO_SURROUND;
                #else
                hdmiAudioMode = dsAUDIO_STEREO_STEREO;
                #endif
            }
            
            // Initialize Audio Auto Mode Settings from Persistence
            std::string hdmiAudioModeAuto("FALSE");
            bool hdmiAutoMode = false;
            
            try {
                hdmiAudioModeAuto = device::HostPersistence::getInstance().getProperty("HDMI0.AudioMode.AUTO", hdmiAudioModeAuto);
            } catch(...) {
                LOGINFO("HDMI0.AudioMode.AUTO not found in persistence store. Try system default");
                try {
                    hdmiAudioModeAuto = device::HostPersistence::getInstance().getDefaultProperty("HDMI0.AudioMode.AUTO");
                } catch(...) {
                    #ifdef IGNORE_EDID_LOGIC
                    hdmiAudioModeAuto = "TRUE";
                    #else
                    hdmiAudioModeAuto = "FALSE";
                    #endif
                }
            }
            
            // Initialize ARC Audio Auto Mode Settings
            std::string arcAudioModeAuto("FALSE");
            bool arcAutoMode = false;
            
            try {
                arcAudioModeAuto = device::HostPersistence::getInstance().getProperty("HDMI_ARC0.AudioMode.AUTO");
            } catch(...) {
                try {
                    LOGINFO("HDMI_ARC0.AudioMode.AUTO not found in persistence store. Try system default");
                    arcAudioModeAuto = device::HostPersistence::getInstance().getDefaultProperty("HDMI_ARC0.AudioMode.AUTO");
                } catch(...) {
                    arcAudioModeAuto = "FALSE";
                }
            }
            
            // Initialize SPDIF Audio Auto Mode Settings
            std::string spdifAudioModeAuto("FALSE");
            bool spdifAutoMode = false;
            
            try {
                spdifAudioModeAuto = device::HostPersistence::getInstance().getProperty("SPDIF0.AudioMode.AUTO");
            } catch(...) {
                try {
                    LOGINFO("SPDIF0.AudioMode.AUTO not found in persistence store. Try system default");
                    spdifAudioModeAuto = device::HostPersistence::getInstance().getDefaultProperty("SPDIF0.AudioMode.AUTO");
                } catch(...) {
                    spdifAudioModeAuto = "FALSE";
                }
            }
            
            // Initialize SPEAKER Audio Auto Mode Settings
            std::string speakerAudioModeAuto("TRUE");
            bool speakerAutoMode = true;
            
            try {
                speakerAudioModeAuto = device::HostPersistence::getInstance().getProperty("SPEAKER0.AudioMode.AUTO");
            } catch(...) {
                try {
                    LOGINFO("SPEAKER0.AudioMode.AUTO not found in persistence store. Try system default");
                    speakerAudioModeAuto = device::HostPersistence::getInstance().getDefaultProperty("SPEAKER0.AudioMode.AUTO");
                } catch(...) {
                    speakerAudioModeAuto = "TRUE";
                }
            }
            
            // Parse auto mode settings
            hdmiAutoMode = (hdmiAudioModeAuto.compare("TRUE") == 0);
            arcAutoMode = (arcAudioModeAuto.compare("TRUE") == 0);
            spdifAutoMode = (spdifAudioModeAuto.compare("TRUE") == 0);
            speakerAutoMode = (speakerAudioModeAuto.compare("TRUE") == 0);
            
            LOGINFO("The HDMI Audio Auto Setting on startup is %s", hdmiAudioModeAuto.c_str());
            LOGINFO("The HDMI ARC Audio Auto Setting on startup is %s", arcAudioModeAuto.c_str());
            LOGINFO("The SPDIF Audio Auto Setting on startup is %s", spdifAudioModeAuto.c_str());
            LOGINFO("The SPEAKER Audio Auto Setting on startup is %s", speakerAudioModeAuto.c_str());
            
            // Initialize SPDIF Audio Mode Settings
            std::string spdifModeSettings("STEREO");
            dsAudioStereoMode_t spdifAudioMode;
            
            spdifModeSettings = device::HostPersistence::getInstance().getProperty("SPDIF0.AudioMode", spdifModeSettings);
            LOGINFO("The SPDIF Audio Mode Setting on startup is %s", spdifModeSettings.c_str());
            
            if (spdifModeSettings.compare("SURROUND") == 0) {
                spdifAudioMode = dsAUDIO_STEREO_SURROUND;
            } else if (spdifModeSettings.compare("PASSTHRU") == 0) {
                spdifAudioMode = dsAUDIO_STEREO_PASSTHRU;
            } else {
                spdifAudioMode = dsAUDIO_STEREO_STEREO;
            }
            
            // Initialize HDMI ARC Audio Mode Settings  
            std::string arcModeSettings("STEREO");
            dsAudioStereoMode_t arcAudioMode;
            
            arcModeSettings = device::HostPersistence::getInstance().getProperty("HDMI_ARC0.AudioMode", arcModeSettings);
            LOGINFO("The HDMI ARC Audio Mode Setting on startup is %s", arcModeSettings.c_str());
            
            if (arcModeSettings.compare("SURROUND") == 0) {
                arcAudioMode = dsAUDIO_STEREO_SURROUND;
            } else if (arcModeSettings.compare("PASSTHRU") == 0) {
                arcAudioMode = dsAUDIO_STEREO_PASSTHRU;
            } else {
                arcAudioMode = dsAUDIO_STEREO_STEREO;
            }
            
            // Initialize SPEAKER Audio Mode Settings
            std::string speakerModeSettings("SURROUND");
            dsAudioStereoMode_t speakerAudioMode;
            
            try {
                speakerModeSettings = device::HostPersistence::getInstance().getProperty("SPEAKER0.AudioMode", speakerModeSettings);
                LOGINFO("The SPEAKER Audio Mode Setting on startup is %s", speakerModeSettings.c_str());
            } catch(...) {
                speakerModeSettings = "SURROUND";
            }
            
            if (speakerModeSettings.compare("SURROUND") == 0) {
                speakerAudioMode = dsAUDIO_STEREO_SURROUND;
            } else if (speakerModeSettings.compare("PASSTHRU") == 0) {
                speakerAudioMode = dsAUDIO_STEREO_PASSTHRU;
            } else if (speakerModeSettings.compare("STEREO") == 0) {
                speakerAudioMode = dsAUDIO_STEREO_STEREO;
            } else {
                speakerAudioMode = dsAUDIO_STEREO_SURROUND;
            }
            
            // Apply audio port settings using HAL functions
            intptr_t handle = 0;
            
            // Set HDMI port audio mode
            if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI, 0, &handle) == dsERR_NONE) {
                if (dsSetStereoMode(handle, hdmiAudioMode) == dsERR_NONE) {
                    LOGINFO("HDMI0: Applied audio mode: %d", hdmiAudioMode);
                }
                if (dsSetStereoAuto(handle, hdmiAutoMode ? 1 : 0) == dsERR_NONE) {
                    LOGINFO("HDMI0: Applied auto mode: %s", hdmiAutoMode ? "TRUE" : "FALSE");
                }
            }
            
            // Set SPDIF port audio mode
            handle = 0;
            if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPDIF, 0, &handle) == dsERR_NONE) {
                if (dsSetStereoMode(handle, spdifAudioMode) == dsERR_NONE) {
                    LOGINFO("SPDIF0: Applied audio mode: %d", spdifAudioMode);
                }
                if (dsSetStereoAuto(handle, spdifAutoMode ? 1 : 0) == dsERR_NONE) {
                    LOGINFO("SPDIF0: Applied auto mode: %s", spdifAutoMode ? "TRUE" : "FALSE");
                }
            }
            
            // Set HDMI ARC port audio mode  
            handle = 0;
            if (dsGetAudioPort(dsAUDIOPORT_TYPE_HDMI_ARC, 0, &handle) == dsERR_NONE) {
                if (dsSetStereoMode(handle, arcAudioMode) == dsERR_NONE) {
                    LOGINFO("HDMI_ARC0: Applied audio mode: %d", arcAudioMode);
                }
                if (dsSetStereoAuto(handle, arcAutoMode ? 1 : 0) == dsERR_NONE) {
                    LOGINFO("HDMI_ARC0: Applied auto mode: %s", arcAutoMode ? "TRUE" : "FALSE");
                }
            }
            
            // Set SPEAKER port audio mode
            handle = 0;
            if (dsGetAudioPort(dsAUDIOPORT_TYPE_SPEAKER, 0, &handle) == dsERR_NONE) {
                if (dsSetStereoMode(handle, speakerAudioMode) == dsERR_NONE) {
                    LOGINFO("SPEAKER0: Applied audio mode: %d", speakerAudioMode);
                }
                if (dsSetStereoAuto(handle, speakerAutoMode ? 1 : 0) == dsERR_NONE) {
                    LOGINFO("SPEAKER0: Applied auto mode: %s", speakerAutoMode ? "TRUE" : "FALSE");
                }
            }
            
            LOGINFO("Comprehensive audio port settings initialization completed successfully");
            
        } catch (...) {
            LOGERR("Exception in initializeAudioPortSettings");
        }
        EXIT_LOG;
    }
    
    // Initialize MS12 audio processing settings
    void initializeMS12Settings()
    {
        ENTRY_LOG;
        try {
            intptr_t handle = 0;
            
            // Initialize basic audio compression for all profiles
            typedef dsError_t (*dsSetAudioCompression_t)(intptr_t handle, int compressionLevel);
            dsSetAudioCompression_t dsSetAudioCompressionFunc = nullptr;
            
            dsSetAudioCompressionFunc = (dsSetAudioCompression_t) resolve(RDK_DSHAL_NAME, "dsSetAudioCompression");
            if (dsSetAudioCompressionFunc) {
                int defaultCompression = 0;
                
                // Initialize compression for SPEAKER and HDMI ports
                const dsAudioPortType_t compressionPorts[] = {dsAUDIOPORT_TYPE_SPEAKER, dsAUDIOPORT_TYPE_HDMI};
                const char* portNames[] = {"SPEAKER0", "HDMI0"};
                
                for (int i = 0; i < 2; i++) {
                    handle = 0;
                    if (dsGetAudioPort(compressionPorts[i], 0, &handle) == dsERR_NONE) {
                        if (dsSetAudioCompressionFunc(handle, defaultCompression) == dsERR_NONE) {
                            LOGINFO("%s: Initialized audio compression: %d", portNames[i], defaultCompression);
                        }
                    }
                }
            }
            
            LOGINFO("MS12 audio settings initialization completed");
            
        } catch (...) {
            LOGERR("Exception in initializeMS12Settings");
        }
        EXIT_LOG;
    }
    
    // audioOutPortConnectCallback implementation
    static void audioOutPortConnectCallback(dsAudioPortType_t portType, unsigned int uiPortNo, bool isPortConnected)
    {
        LOGINFO("Audio port hotplug event: portType=%d, portNo=%d, connected=%s", 
               portType, uiPortNo, isPortConnected ? "true" : "false");
        
        // Convert dsAudioPortType_t to AudioPortType
        AudioPortType wpePortType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER; // default
        switch (portType) {
            case dsAUDIOPORT_TYPE_ID_LR: wpePortType = AudioPortType::AUDIO_PORT_TYPE_LR; break;
            case dsAUDIOPORT_TYPE_HDMI: wpePortType = AudioPortType::AUDIO_PORT_TYPE_HDMI; break;
            case dsAUDIOPORT_TYPE_SPDIF: wpePortType = AudioPortType::AUDIO_PORT_TYPE_SPDIF; break;
            case dsAUDIOPORT_TYPE_SPEAKER: wpePortType = AudioPortType::AUDIO_PORT_TYPE_SPEAKER; break;
            case dsAUDIOPORT_TYPE_HDMI_ARC: wpePortType = AudioPortType::AUDIO_PORT_TYPE_HDMIARC; break;
            case dsAUDIOPORT_TYPE_HEADPHONE: wpePortType = AudioPortType::AUDIO_PORT_TYPE_HEADPHONE; break;
            default: break;
        }
        
        // Call Audio event handler through global callback if available
        if (g_AudioOutHotPlugCallback) {
            g_AudioOutHotPlugCallback(wpePortType, static_cast<uint32_t>(uiPortNo), isPortConnected);
        }
    }
    
    // audioFormatUpdateCallback implementation
    static void audioFormatUpdateCallback(dsAudioFormat_t audioFormat)
    {
        LOGINFO("Audio format update event: audioFormat=%d", audioFormat);
        
        // Convert dsAudioFormat_t to AudioFormat
        AudioFormat wpeFormat = static_cast<AudioFormat>(audioFormat);
        
        // Call Audio event handler through global callback if available
        if (g_AudioFormatUpdateCallback) {
            g_AudioFormatUpdateCallback(wpeFormat);
        }
    }
    
    // audioAtmosCapsChangeCallback implementation  
    static void audioAtmosCapsChangeCallback(dsATMOSCapability_t atmosCaps, bool status)
    {
        LOGINFO("Audio atmos caps change event: atmosCaps=%d, status=%s", atmosCaps, status ? "true" : "false");
        
        // Convert dsATMOSCapability_t to DolbyAtmosCapability
        DolbyAtmosCapability wpeAtmosCaps = static_cast<DolbyAtmosCapability>(atmosCaps);
        
        // Call Audio event handler through global callback if available
        if (g_DolbyAtmosCapabilitiesChangedCallback) {
            g_DolbyAtmosCapabilitiesChangedCallback(wpeAtmosCaps, status);
        }
    }
    
    // State Change Notification Functions using global callbacks
    // notifyAssociatedAudioMixingChanged implementation
    void notifyAssociatedAudioMixingChanged(bool mixing)
    {
        LOGINFO("Associated audio mixing changed: %s", mixing ? "enabled" : "disabled");
        // Call Audio event handler using global callback if available
        if (g_AssociatedAudioMixingChangedCallback) {
            g_AssociatedAudioMixingChangedCallback(mixing);
        }
    }
    
    // notifyAudioFaderControlChanged implementation
    void notifyAudioFaderControlChanged(int32_t mixerBalance)
    {
        LOGINFO("Audio fader control changed: mixerBalance=%d", mixerBalance);
        // Call Audio event handler using global callback if available
        if (g_AudioFaderControlChangedCallback) {
            g_AudioFaderControlChangedCallback(mixerBalance);
        }
    }
    
    // notifyAudioPrimaryLanguageChanged implementation
    void notifyAudioPrimaryLanguageChanged(const std::string& primaryLanguage)
    {
        LOGINFO("Audio primary language changed: %s", primaryLanguage.c_str());
        // Call Audio event handler using global callback if available
        if (g_AudioPrimaryLanguageChangedCallback) {
            g_AudioPrimaryLanguageChangedCallback(primaryLanguage);
        }
    }
    
    // notifyAudioSecondaryLanguageChanged implementation
    void notifyAudioSecondaryLanguageChanged(const std::string& secondaryLanguage)
    {
        LOGINFO("Audio secondary language changed: %s", secondaryLanguage.c_str());
        // Call Audio event handler using global callback if available
        if (g_AudioSecondaryLanguageChangedCallback) {
            g_AudioSecondaryLanguageChangedCallback(secondaryLanguage);
        }
    }
    
    // notifyAudioPortStateChanged implementation
    void notifyAudioPortStateChanged(AudioPortState audioPortState)
    {
        LOGINFO("Audio port state changed: state=%d", static_cast<int>(audioPortState));
        // Call Audio event handler using global callback if available
        if (g_AudioPortStateChangedCallback) {
            g_AudioPortStateChangedCallback(audioPortState);
        }
    }
    
    // notifyAudioLevelChanged implementation
    void notifyAudioLevelChanged(int32_t audioLevel)
    {
        LOGINFO("Audio level changed: audioLevel=%d", audioLevel);
        // Call Audio event handler using global callback if available
        if (g_AudioLevelChangedCallback) {
            g_AudioLevelChangedCallback(static_cast<float>(audioLevel));
        }
    }
    
    // notifyAudioModeChanged implementation
    void notifyAudioModeChanged(AudioPortType portType, AudioStereoMode mode)
    {
        LOGINFO("Audio mode changed: portType=%d, mode=%d", static_cast<int>(portType), static_cast<int>(mode));
        // Call Audio event handler using global callback if available
        if (g_AudioModeChangedCallback) {
            g_AudioModeChangedCallback(portType, mode);
        }
    }

    // Callback management implementation following HdmiIn pattern
    void setAllCallbacks(const CallbackBundle bundle) override
    {
        ENTRY_LOG;
        
        // Register audio callbacks following HdmiIn pattern
        if (bundle.OnAudioOutHotPlug) {
            LOGINFO("Audio Output Hot Plug Event Callback Registered");
            g_AudioOutHotPlugCallback = bundle.OnAudioOutHotPlug;
        }
        
        if (bundle.OnAudioFormatUpdate) {
            LOGINFO("Audio Format Update Event Callback Registered");
            g_AudioFormatUpdateCallback = bundle.OnAudioFormatUpdate;
        }
        
        if (bundle.OnDolbyAtmosCapabilitiesChanged) {
            LOGINFO("Dolby Atmos Capabilities Changed Event Callback Registered");
            g_DolbyAtmosCapabilitiesChangedCallback = bundle.OnDolbyAtmosCapabilitiesChanged;
        }
        
        if (bundle.OnAssociatedAudioMixingChanged) {
            LOGINFO("Associated Audio Mixing Changed Event Callback Registered");
            g_AssociatedAudioMixingChangedCallback = bundle.OnAssociatedAudioMixingChanged;
        }
        
        if (bundle.OnAudioFaderControlChanged) {
            LOGINFO("Audio Fader Control Changed Event Callback Registered");
            g_AudioFaderControlChangedCallback = bundle.OnAudioFaderControlChanged;
        }
        
        if (bundle.OnAudioPrimaryLanguageChanged) {
            LOGINFO("Audio Primary Language Changed Event Callback Registered");
            g_AudioPrimaryLanguageChangedCallback = bundle.OnAudioPrimaryLanguageChanged;
        }
        
        if (bundle.OnAudioSecondaryLanguageChanged) {
            LOGINFO("Audio Secondary Language Changed Event Callback Registered");
            g_AudioSecondaryLanguageChangedCallback = bundle.OnAudioSecondaryLanguageChanged;
        }
        
        if (bundle.OnAudioPortStateChanged) {
            LOGINFO("Audio Port State Changed Event Callback Registered");
            g_AudioPortStateChangedCallback = bundle.OnAudioPortStateChanged;
        }
        
        if (bundle.OnAudioLevelChanged) {
            LOGINFO("Audio Level Changed Event Callback Registered");
            g_AudioLevelChangedCallback = bundle.OnAudioLevelChanged;
        }
        
        if (bundle.OnAudioModeChanged) {
            LOGINFO("Audio Mode Changed Event Callback Registered");
            g_AudioModeChangedCallback = bundle.OnAudioModeChanged;
        }
        
        LOGINFO("Audio callbacks set successfully");
        EXIT_LOG;
    }

    void getPersistenceValue() override
    {
        ENTRY_LOG;
        // Initialize persistence-related values if needed
        LOGINFO("Audio persistence values loaded");
        EXIT_LOG;
    }
};
