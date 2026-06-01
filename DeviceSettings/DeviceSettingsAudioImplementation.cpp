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

#include "DeviceSettingsAudioImplementation.h"

#include "UtilsLogging.h"
#include <syscall.h>
#include <dlfcn.h>
#include <cstring>
#include <vector>
#include <type_traits>

using namespace std;

namespace {
    typedef struct _dlSymbolLookup {
        const char* name;
        void** dataptr;
    } dlSymbolLookup;

    typedef struct _audioConfigs {
        const dsAudioTypeConfig_t* pKConfigs;
        const dsAudioPortConfig_t* pKPorts;
        int* pKConfigSize;
        int* pKPortSize;
    } audioConfigs_t;

    static bool LoadDLSymbols(void* pDLHandle, const dlSymbolLookup* symbols, const int numberOfSymbols)
    {
        int currentSymbols = 0;

        if ((pDLHandle == NULL) || (symbols == NULL)) {
            LOGERR("LoadDLSymbols(Audio): Invalid handle or symbols");
            return false;
        }

        for (int i = 0; i < numberOfSymbols; i++) {
            if ((symbols[i].dataptr == NULL) || (symbols[i].name == NULL)) {
                LOGERR("LoadDLSymbols(Audio): Invalid symbol entry at index %d", i);
                continue;
            }

            *(symbols[i].dataptr) = dlsym(pDLHandle, symbols[i].name);
            if (*(symbols[i].dataptr) != NULL) {
                currentSymbols++;
            } else {
                LOGWARN("LoadDLSymbols(Audio): [%s] not found", symbols[i].name);
            }
        }

        return (numberOfSymbols > 0) ? (currentSymbols == numberOfSymbols) : false;
    }

    static bool LoadAudioConfigFromHAL(audioConfigs_t& config)
    {
        void* pDLHandle = NULL;
        bool isSymbolsLoaded = false;

        memset(&config, 0, sizeof(config));

        dlerror();
        pDLHandle = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
        if (pDLHandle == NULL) {
            const char* dlErr = dlerror();
            LOGWARN("LoadAudioConfigFromHAL: dlopen failed for %s: %s", RDK_DSHAL_NAME, (dlErr ? dlErr : "unknown"));
            return false;
        }

        dlSymbolLookup audioConfigSymbols[] = {
            {"kAudioConfigs", (void**)&config.pKConfigs},
            {"kAudioPorts", (void**)&config.pKPorts},
            {"kAudioConfigs_size", (void**)&config.pKConfigSize},
            {"kAudioPorts_size", (void**)&config.pKPortSize}
        };

        isSymbolsLoaded = LoadDLSymbols(pDLHandle, audioConfigSymbols, sizeof(audioConfigSymbols) / sizeof(dlSymbolLookup));
        dlclose(pDLHandle);

        if (!isSymbolsLoaded) {
            LOGWARN("LoadAudioConfigFromHAL: Failed to load all audio symbols from HAL");
            return false;
        }

        if ((config.pKConfigs == NULL) || (config.pKPorts == NULL) ||
            (config.pKConfigSize == NULL) || (config.pKPortSize == NULL)) {
            LOGWARN("LoadAudioConfigFromHAL: HAL symbols loaded but one or more pointers are null");
            return false;
        }

        return true;
    }

    template <typename EnumType>
    static uint32_t ToEnumMask(const EnumType* values, const size_t count)
    {
        static_assert(std::is_enum<EnumType>::value, "EnumType must be an enum");

        uint32_t mask = 0;
        for (size_t index = 0; index < count; ++index) {
            const uint32_t bit = static_cast<uint32_t>(values[index]);
            if (bit < (sizeof(mask) * 8)) {
                mask |= (1u << bit);
            }
        }

        return mask;
    }

    static void PopulateAudioConfig(std::vector<AudioTypeConfigInfo>& audioTypes,
                                    std::vector<AudioPortConfigInfo>& audioPorts)
    {
        audioConfigs_t halConfig;
        memset(&halConfig, 0, sizeof(halConfig));
        const bool loadedFromHAL = LoadAudioConfigFromHAL(halConfig);

        audioTypes.clear();
        audioPorts.clear();

        if (!loadedFromHAL) {
            LOGWARN("PopulateAudioConfig: HAL config not available, returning empty config");
            return;
        }

        const int typeCount = *(halConfig.pKConfigSize);
        const int portCount = *(halConfig.pKPortSize);

        for (int i = 0; i < typeCount; i++) {
            const dsAudioTypeConfig_t& cfg = halConfig.pKConfigs[i];

            AudioTypeConfigInfo typeCfg;
            typeCfg.typeId = cfg.typeId;
            typeCfg.name = (cfg.name ? cfg.name : "");
            typeCfg.supportedCompressionMask = (cfg.compressions != NULL)
                ? ToEnumMask(cfg.compressions, cfg.numSupportedCompressions)
                : 0;
            typeCfg.supportedEncodingMask = (cfg.encodings != NULL)
                ? ToEnumMask(cfg.encodings, cfg.numSupportedEncodings)
                : 0;
            typeCfg.supportedStereoModeMask = (cfg.stereoModes != NULL)
                ? ToEnumMask(cfg.stereoModes, cfg.numSupportedStereoModes)
                : 0;
            audioTypes.push_back(typeCfg);
        }

        for (int i = 0; i < portCount; i++) {
            const dsAudioPortConfig_t& cfg = halConfig.pKPorts[i];

            AudioPortConfigInfo portCfg;
            portCfg.audioPortType = static_cast<AudioPortType>(cfg.id.type);
            portCfg.audioPortIndex = cfg.id.index;
            if (cfg.connectedVOPs != NULL) {
                portCfg.connectedVideoPortType = static_cast<int32_t>(cfg.connectedVOPs->type);
                portCfg.connectedVideoPortIndex = cfg.connectedVOPs->index;
            } else {
                portCfg.connectedVideoPortType = -1;
                portCfg.connectedVideoPortIndex = -1;
            }
            audioPorts.push_back(portCfg);
        }

    LOGINFO("PopulateAudioConfig: Loaded config from HAL (audioTypes=%zu audioPorts=%zu)",
        audioTypes.size(), audioPorts.size());
    }
}

namespace WPEFramework {
namespace Plugin {

    DeviceSettingsAudioImpl::DeviceSettingsAudioImpl()
        : _audio(Audio::Create(*this))
    {
        LOGINFO("DeviceSettingsAudioImpl Constructor - Instance Address: %p", this);
    }

    DeviceSettingsAudioImpl::~DeviceSettingsAudioImpl() {
        LOGINFO("DeviceSettingsAudioImpl Destructor - Instance Address: %p", this);
    }

    template<typename Func, typename... Args>
    void DeviceSettingsAudioImpl::dispatchAudioEvent(Func notifyFunc, Args&&... args) {
        LOGINFO(">>");
        _callbackLock.Lock();
        for (auto& notification : _AudioNotifications) {
            auto start = std::chrono::steady_clock::now();
            (notification->*notifyFunc)(std::forward<Args>(args)...);
            auto elapsed = std::chrono::steady_clock::now() - start;
            LOGINFO("client %p took %" PRId64 "ms to process IAudio event", notification, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        }
        _callbackLock.Unlock();
        LOGINFO("<<");
    }

    template <typename T>
    Core::hresult DeviceSettingsAudioImpl::Register(std::list<T*>& list, T* notification)
    {
        uint32_t status = Core::ERROR_GENERAL;
        ASSERT(nullptr != notification);

        _callbackLock.Lock();
        // Make sure we can't register the same notification callback multiple times
        if (std::find(list.begin(), list.end(), notification) == list.end()) {
            list.push_back(notification);
            notification->AddRef();
            status = Core::ERROR_NONE;
        } else {
            LOGWARN("Notification %p already registered - skipping", notification);
        }
        _callbackLock.Unlock();

        return status;
    }

    template <typename T>
    Core::hresult DeviceSettingsAudioImpl::Unregister(std::list<T*>& list, const T* notification)
    {
        uint32_t status = Core::ERROR_GENERAL;
        ASSERT(nullptr != notification);
        _callbackLock.Lock();

        // Make sure we can't unregister the same notification callback multiple times
        auto itr = std::find(list.begin(), list.end(), notification);
        if (itr != list.end()) {
            (*itr)->Release();
            list.erase(itr);
            status = Core::ERROR_NONE;
        }

        _callbackLock.Unlock();
        return status;
    }

    Core::hresult DeviceSettingsAudioImpl::Register(DeviceSettingsAudio::INotification* notification)
    {
        Core::hresult errorCode = Register(_AudioNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IAudio %p, errorCode: %u", notification, errorCode);
        } else {
            LOGINFO("IAudio %p registered successfully", notification);
        }
        return errorCode;
    }

    Core::hresult DeviceSettingsAudioImpl::Unregister(DeviceSettingsAudio::INotification* notification)
    {
        Core::hresult errorCode = Unregister(_AudioNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IAudio %p, errorcode: %u", notification, errorCode);
        } else {
            LOGINFO("IAudio %p unregistered successfully", notification);
        }
        return errorCode;
    }

    // Audio notification implementations - hardware callbacks
    void DeviceSettingsAudioImpl::OnAssociatedAudioMixingChanged(bool mixing)
    {
        LOGINFO("OnAssociatedAudioMixingChanged event Received: mixing=%s", mixing ? "true" : "false");
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAssociatedAudioMixingChanged, mixing);
    }

    void DeviceSettingsAudioImpl::OnAudioFaderControlChanged(int32_t mixerBalance)
    {
        LOGINFO("OnAudioFaderControlChanged event Received: mixerBalance=%d", mixerBalance);
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAudioFaderControlChanged, mixerBalance);
    }

    void DeviceSettingsAudioImpl::OnAudioPrimaryLanguageChanged(const std::string& primaryLanguage)
    {
        LOGINFO("OnAudioPrimaryLanguageChanged event Received: primaryLanguage=%s", primaryLanguage.c_str());
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAudioPrimaryLanguageChanged, primaryLanguage);
    }

    void DeviceSettingsAudioImpl::OnAudioSecondaryLanguageChanged(const std::string& secondaryLanguage)
    {
        LOGINFO("OnAudioSecondaryLanguageChanged event Received: secondaryLanguage=%s", secondaryLanguage.c_str());
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAudioSecondaryLanguageChanged, secondaryLanguage);
    }

    void DeviceSettingsAudioImpl::OnAudioOutHotPlug(AudioPortType portType, uint32_t uiPortNumber, bool isPortConnected)
    {
        LOGINFO("OnAudioOutHotPlug event Received: portType=%d, port=%u, connected=%s", portType, uiPortNumber, isPortConnected ? "true" : "false");
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAudioOutHotPlug, portType, uiPortNumber, isPortConnected);
    }

    void DeviceSettingsAudioImpl::OnAudioFormatUpdate(AudioFormat audioFormat)
    {
        LOGINFO("OnAudioFormatUpdate event Received: audioFormat=%d", audioFormat);
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAudioFormatUpdate, audioFormat);
    }

    void DeviceSettingsAudioImpl::OnDolbyAtmosCapabilitiesChanged(DolbyAtmosCapability atmosCapability, bool status)
    {
        LOGINFO("OnDolbyAtmosCapabilitiesChanged event Received: capability=%d, status=%s", atmosCapability, status ? "true" : "false");
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnDolbyAtmosCapabilitiesChanged, atmosCapability, status);
    }

    void DeviceSettingsAudioImpl::OnAudioPortStateChanged(AudioPortState audioPortState)
    {
        LOGINFO("OnAudioPortStateChanged event Received: audioPortState=%d", audioPortState);
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAudioPortStateChanged, audioPortState);
    }

    void DeviceSettingsAudioImpl::OnAudioLevelChangedEvent(int32_t audioLevel)
    {
        LOGINFO("OnAudioLevelChangedEvent event Received: audioLevel=%d", audioLevel);
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAudioLevelChangedEvent, audioLevel);
    }

    void DeviceSettingsAudioImpl::OnAudioModeEvent(AudioPortType audioPortType, AudioStereoMode audioMode)
    {
        LOGINFO("OnAudioModeEvent event Received: portType=%d, mode=%d", audioPortType, audioMode);
        dispatchAudioEvent(&DeviceSettingsAudio::INotification::OnAudioModeEvent, audioPortType, audioMode);
    }

    // Audio port management
    Core::hresult DeviceSettingsAudioImpl::GetAudioPort(const AudioPortType type, const int32_t index, int32_t &handle) {
        LOGINFO("GetAudioPort: type=%d, index=%d", type, index);
        uint32_t result = _audio.GetAudioPort(type, index, handle);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioConfig(IAudioTypeConfigIterator*& audioTypes,
                                  IAudioPortConfigIterator*& audioPorts) {
        std::vector<AudioTypeConfigInfo> typeConfigs;
        std::vector<AudioPortConfigInfo> portConfigs;

        PopulateAudioConfig(typeConfigs, portConfigs);

        using AudioTypeIterator = RPC::IteratorType<IAudioTypeConfigIterator>;
        using AudioPortIterator = RPC::IteratorType<IAudioPortConfigIterator>;

        audioTypes = Core::Service<AudioTypeIterator>::Create<IAudioTypeConfigIterator>(typeConfigs);
        audioPorts = Core::Service<AudioPortIterator>::Create<IAudioPortConfigIterator>(portConfigs);

        LOGINFO("GetAudioConfig: audioTypes=%zu audioPorts=%zu",
            typeConfigs.size(), portConfigs.size());
        return Core::ERROR_NONE;
    }

    // GetAudioPorts and GetSupportedAudioPorts methods removed - iterator type doesn't exist

    Core::hresult DeviceSettingsAudioImpl::GetAudioPortConfig(const AudioPortType audioPort, AudioConfig &audioConfig) {
        LOGINFO("GetAudioPortConfig: audioPort=%d", audioPort);
        uint32_t result = _audio.GetAudioPortConfig(audioPort, audioConfig);
        return result;
    }

    // Audio capabilities
    Core::hresult DeviceSettingsAudioImpl::GetAudioCapabilities(const int32_t handle, int32_t &capabilities) {
        LOGINFO("GetAudioCapabilities: handle=%d", handle);
        uint32_t result = _audio.GetAudioCapabilities(handle, capabilities);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioMS12Capabilities(const int32_t handle, int32_t &capabilities) {
        LOGINFO("GetAudioMS12Capabilities: handle=%d", handle);
        uint32_t result = _audio.GetAudioMS12Capabilities(handle, capabilities);
        return result;
    }

    // Audio format and encoding
    Core::hresult DeviceSettingsAudioImpl::GetAudioFormat(const int32_t handle, AudioFormat &audioFormat) {
        LOGINFO("GetAudioFormat: handle=%d", handle);
        uint32_t result = _audio.GetAudioFormat(handle, audioFormat);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioEncoding(const int32_t handle, AudioEncoding &encoding) {
        LOGINFO("GetAudioEncoding: handle=%d", handle);
        uint32_t result = _audio.GetAudioEncoding(handle, encoding);
        return result;
    }

    // Audio level and volume control
    Core::hresult DeviceSettingsAudioImpl::SetAudioLevel(const int32_t handle, const float audioLevel) {
        LOGINFO("SetAudioLevel: handle=%d, audioLevel=%.2f", handle, audioLevel);
        uint32_t result = _audio.SetAudioLevel(handle, audioLevel);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioLevel(const int32_t handle, float &audioLevel) {
        LOGINFO("GetAudioLevel: handle=%d", handle);
        uint32_t result = _audio.GetAudioLevel(handle, audioLevel);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::SetAudioGain(const int32_t handle, const float gainLevel) {
        LOGINFO("SetAudioGain: handle=%d, gainLevel=%.2f", handle, gainLevel);
        uint32_t result = _audio.SetAudioGain(handle, gainLevel);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioGain(const int32_t handle, float &gainLevel) {
        LOGINFO("GetAudioGain: handle=%d", handle);
        uint32_t result = _audio.GetAudioGain(handle, gainLevel);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::SetAudioMute(const int32_t handle, const bool mute) {
        LOGINFO("SetAudioMute: handle=%d, mute=%s", handle, mute ? "true" : "false");
        uint32_t result = _audio.SetAudioMute(handle, mute);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::IsAudioMuted(const int32_t handle, bool &muted) {
        LOGINFO("IsAudioMuted: handle=%d", handle);
        uint32_t result = _audio.IsAudioMuted(handle, muted);
        return result;
    }

    // Audio ducking
    Core::hresult DeviceSettingsAudioImpl::SetAudioDucking(const int32_t handle, const AudioDuckingType duckingType, const AudioDuckingAction duckingAction, const uint8_t level) {
        LOGINFO("SetAudioDucking: handle=%d, duckingType=%d, duckingAction=%d, level=%d", handle, duckingType, duckingAction, level);
        uint32_t result = _audio.SetAudioDucking(handle, duckingType, duckingAction, level);
        return result;
    }

    // Stereo mode
    Core::hresult DeviceSettingsAudioImpl::GetStereoMode(const int32_t handle, AudioStereoMode &mode) {
        LOGINFO("GetStereoMode: handle=%d", handle);
        uint32_t result = _audio.GetStereoMode(handle, mode);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::SetStereoMode(const int32_t handle, const AudioStereoMode mode, const bool persist) {
        LOGINFO("SetStereoMode: handle=%d, mode=%d, persist=%s", handle, mode, persist ? "true" : "false");
        uint32_t result = _audio.SetStereoMode(handle, mode, persist);
        return result;
    }

    // Associated audio mixing
    Core::hresult DeviceSettingsAudioImpl::SetAssociatedAudioMixing(const int32_t handle, const bool mixing) {
        LOGINFO("SetAssociatedAudioMixing: handle=%d, mixing=%s", handle, mixing ? "true" : "false");
        uint32_t result = _audio.SetAssociatedAudioMixing(handle, mixing);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAssociatedAudioMixing(const int32_t handle, bool &mixing) {
        LOGINFO("GetAssociatedAudioMixing: handle=%d", handle);
        uint32_t result = _audio.GetAssociatedAudioMixing(handle, mixing);
        return result;
    }

    // Audio fader control
    Core::hresult DeviceSettingsAudioImpl::SetAudioFaderControl(const int32_t handle, const int32_t mixerBalance) {
        LOGINFO("SetAudioFaderControl: handle=%d, mixerBalance=%d", handle, mixerBalance);
        uint32_t result = _audio.SetAudioFaderControl(handle, mixerBalance);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioFaderControl(const int32_t handle, int32_t &mixerBalance) {
        LOGINFO("GetAudioFaderControl: handle=%d", handle);
        uint32_t result = _audio.GetAudioFaderControl(handle, mixerBalance);
        return result;
    }

    // Audio language settings
    Core::hresult DeviceSettingsAudioImpl::SetAudioPrimaryLanguage(const int32_t handle, const std::string primaryAudioLanguage) {
        LOGINFO("SetAudioPrimaryLanguage: handle=%d, primaryAudioLanguage=%s", handle, primaryAudioLanguage.c_str());
        uint32_t result = _audio.SetAudioPrimaryLanguage(handle, primaryAudioLanguage);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioPrimaryLanguage(const int32_t handle, std::string &primaryAudioLanguage) {
        LOGINFO("GetAudioPrimaryLanguage: handle=%d", handle);
        uint32_t result = _audio.GetAudioPrimaryLanguage(handle, primaryAudioLanguage);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::SetAudioSecondaryLanguage(const int32_t handle, const std::string secondaryAudioLanguage) {
        LOGINFO("SetAudioSecondaryLanguage: handle=%d, secondaryAudioLanguage=%s", handle, secondaryAudioLanguage.c_str());
        uint32_t result = _audio.SetAudioSecondaryLanguage(handle, secondaryAudioLanguage);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioSecondaryLanguage(const int32_t handle, std::string &secondaryAudioLanguage) {
        LOGINFO("GetAudioSecondaryLanguage: handle=%d", handle);
        uint32_t result = _audio.GetAudioSecondaryLanguage(handle, secondaryAudioLanguage);
        return result;
    }

    // Output connection status
    Core::hresult DeviceSettingsAudioImpl::IsAudioOutputConnected(const int32_t handle, bool &isConnected) {
        LOGINFO("IsAudioOutputConnected: handle=%d", handle);
        uint32_t result = _audio.IsAudioOutputConnected(handle, isConnected);
        return result;
    }

    // Dolby Atmos
    Core::hresult DeviceSettingsAudioImpl::GetAudioSinkDeviceAtmosCapability(const int32_t handle, DolbyAtmosCapability &atmosCapability) {
        LOGINFO("GetAudioSinkDeviceAtmosCapability: handle=%d", handle);
        uint32_t result = _audio.GetAudioSinkDeviceAtmosCapability(handle, atmosCapability);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::SetAudioAtmosOutputMode(const int32_t handle, const bool enable) {
        LOGINFO("SetAudioAtmosOutputMode: handle=%d, enable=%s", handle, enable ? "true" : "false");
        uint32_t result = _audio.SetAudioAtmosOutputMode(handle, enable);
        return result;
    }

    // Stub implementations for compression methods
    Core::hresult DeviceSettingsAudioImpl::GetSupportedCompressions(const int32_t handle, IDeviceSettingsAudioCompressionIterator*& compressions) {
        LOGINFO("GetSupportedCompressions: handle=%d - STUB IMPLEMENTATION", handle);
        uint32_t result = _audio.GetSupportedCompressions(handle, compressions);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetAudioCompression(const int32_t handle, AudioCompression &compression) {
        LOGINFO("GetAudioCompression: handle=%d - STUB IMPLEMENTATION", handle);
        uint32_t result = _audio.GetAudioCompression(handle, compression);
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::SetAudioCompression(const int32_t handle, const AudioCompression compression) {
        LOGINFO("SetAudioCompression: handle=%d, compression=%d - STUB IMPLEMENTATION", handle, compression);
        uint32_t result = _audio.SetAudioCompression(handle, compression);
        return result;
    }

    // Additional stub implementations for other methods would go here
    Core::hresult DeviceSettingsAudioImpl::GetMS12Capabilities(const int32_t handle, IDeviceSettingsAudioCompressionIterator*& compressions) {
        LOGINFO("GetMS12Capabilities: handle=%d - STUB IMPLEMENTATION", handle);
        uint32_t result = WPEFramework::Core::ERROR_GENERAL;
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::GetStereoAuto(const int32_t handle, int32_t &mode) {
        LOGINFO("GetStereoAuto: handle=%d - STUB IMPLEMENTATION", handle);
        uint32_t result = WPEFramework::Core::ERROR_GENERAL;
        return result;
    }

    Core::hresult DeviceSettingsAudioImpl::SetStereoAuto(const int32_t handle, const int32_t mode, const bool persist) {
        LOGINFO("SetStereoAuto: handle=%d, mode=%d, persist=%s - STUB IMPLEMENTATION", handle, mode, persist ? "true" : "false");
        uint32_t result = WPEFramework::Core::ERROR_GENERAL;
        return result;
    }

    // Missing Audio interface methods implementation
    
    Core::hresult DeviceSettingsAudioImpl::IsAudioPortEnabled(const int32_t handle, bool &enabled) {
        uint32_t result = _audio.IsAudioPortEnabled(handle, enabled);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::EnableAudioPort(const int32_t handle, const bool enable) {
        uint32_t result = _audio.EnableAudioPort(handle, enable);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetSupportedARCTypes(const int32_t handle, int32_t &types) {
        uint32_t result = _audio.GetSupportedARCTypes(handle, types);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetSAD(const int32_t handle, const uint8_t sadList[], const uint8_t count) {
        uint32_t result = _audio.SetSAD(handle, sadList, count);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::EnableARC(const int32_t handle, const AudioARCStatus arcStatus) {
        uint32_t result = _audio.EnableARC(handle, arcStatus);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioEnablePersist(const int32_t handle, bool &enabled, string &portName) {
        uint32_t result = _audio.GetAudioEnablePersist(handle, enabled, portName);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioEnablePersist(const int32_t handle, const bool enable, const string portName) {
        uint32_t result = _audio.SetAudioEnablePersist(handle, enable, portName);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::IsAudioMSDecoded(const int32_t handle, bool &hasms11Decode) {
        uint32_t result = _audio.IsAudioMSDecoded(handle, hasms11Decode);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::IsAudioMS12Decoded(const int32_t handle, bool &hasms12Decode) {
        uint32_t result = _audio.IsAudioMS12Decoded(handle, hasms12Decode);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioLEConfig(const int32_t handle, bool &enabled) {
        uint32_t result = _audio.GetAudioLEConfig(handle, enabled);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::EnableAudioLEConfig(const int32_t handle, const bool enable) {
        uint32_t result = _audio.EnableAudioLEConfig(handle, enable);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioDelay(const int32_t handle, const uint32_t audioDelay) {
        uint32_t result = _audio.SetAudioDelay(handle, audioDelay);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioDelay(const int32_t handle, uint32_t &audioDelay) {
        uint32_t result = _audio.GetAudioDelay(handle, audioDelay);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioDelayOffset(const int32_t handle, const uint32_t delayOffset) {
        uint32_t result = _audio.SetAudioDelayOffset(handle, delayOffset);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioDelayOffset(const int32_t handle, uint32_t &delayOffset) {
        uint32_t result = _audio.GetAudioDelayOffset(handle, delayOffset);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioCompression(const int32_t handle, const int32_t compressionLevel) {
        uint32_t result = _audio.SetAudioCompression(handle, compressionLevel);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioCompression(const int32_t handle, int32_t &compressionLevel) {
        uint32_t result = _audio.GetAudioCompression(handle, compressionLevel);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioDialogEnhancement(const int32_t handle, const int32_t level) {
        uint32_t result = _audio.SetAudioDialogEnhancement(handle, level);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioDialogEnhancement(const int32_t handle, int32_t &level) {
        uint32_t result = _audio.GetAudioDialogEnhancement(handle, level);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioDolbyVolumeMode(const int32_t handle, const bool enable) {
        uint32_t result = _audio.SetAudioDolbyVolumeMode(handle, enable);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioDolbyVolumeMode(const int32_t handle, bool &enabled) {
        uint32_t result = _audio.GetAudioDolbyVolumeMode(handle, enabled);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioIntelligentEqualizerMode(const int32_t handle, const int32_t mode) {
        uint32_t result = _audio.SetAudioIntelligentEqualizerMode(handle, mode);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioIntelligentEqualizerMode(const int32_t handle, int32_t &mode) {
        uint32_t result = _audio.GetAudioIntelligentEqualizerMode(handle, mode);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioVolumeLeveller(const int32_t handle, const VolumeLeveller volumeLeveller) {
        uint32_t result = _audio.SetAudioVolumeLeveller(handle, volumeLeveller);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioVolumeLeveller(const int32_t handle, VolumeLeveller &volumeLeveller) {
        uint32_t result = _audio.GetAudioVolumeLeveller(handle, volumeLeveller);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioBassEnhancer(const int32_t handle, const int32_t boost) {
        uint32_t result = _audio.SetAudioBassEnhancer(handle, boost);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioBassEnhancer(const int32_t handle, int32_t &boost) {
        uint32_t result = _audio.GetAudioBassEnhancer(handle, boost);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::EnableAudioSurroudDecoder(const int32_t handle, const bool enable) {
        uint32_t result = _audio.EnableAudioSurroudDecoder(handle, enable);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::IsAudioSurroudDecoderEnabled(const int32_t handle, bool &enabled) {
        uint32_t result = _audio.IsAudioSurroudDecoderEnabled(handle, enabled);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioDRCMode(const int32_t handle, const int32_t drcMode) {
        uint32_t result = _audio.SetAudioDRCMode(handle, drcMode);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioDRCMode(const int32_t handle, int32_t &drcMode) {
        uint32_t result = _audio.GetAudioDRCMode(handle, drcMode);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioSurroudVirtualizer(const int32_t handle, const SurroundVirtualizer surroundVirtualizer) {
        uint32_t result = _audio.SetAudioSurroudVirtualizer(handle, surroundVirtualizer);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioSurroudVirtualizer(const int32_t handle, SurroundVirtualizer &surroundVirtualizer) {
        uint32_t result = _audio.GetAudioSurroudVirtualizer(handle, surroundVirtualizer);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioMISteering(const int32_t handle, const bool enable) {
        uint32_t result = _audio.SetAudioMISteering(handle, enable);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioMISteering(const int32_t handle, bool &enable) {
        uint32_t result = _audio.GetAudioMISteering(handle, enable);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioGraphicEqualizerMode(const int32_t handle, const int32_t mode) {
        uint32_t result = _audio.SetAudioGraphicEqualizerMode(handle, mode);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioGraphicEqualizerMode(const int32_t handle, int32_t &mode) {
        uint32_t result = _audio.GetAudioGraphicEqualizerMode(handle, mode);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioMS12ProfileList(const int32_t handle, IDeviceSettingsAudioMS12AudioProfileIterator*& ms12ProfileList) const {
        uint32_t result = _audio.GetAudioMS12ProfileList(handle, ms12ProfileList);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioMS12Profile(const int32_t handle, string &profile) {
        uint32_t result = _audio.GetAudioMS12Profile(handle, profile);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioMS12Profile(const int32_t handle, const string profile) {
        uint32_t result = _audio.SetAudioMS12Profile(handle, profile);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioMixerLevels(const int32_t handle, const AudioInput audioInput, const int32_t volume) {
        uint32_t result = _audio.SetAudioMixerLevels(handle, audioInput, volume);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::SetAudioMS12SettingsOverride(const int32_t handle, const string profileName, const string profileSettingsName, const string profileSettingValue, const string profileState) {
        uint32_t result = _audio.SetAudioMS12SettingsOverride(handle, profileName, profileSettingsName, profileSettingValue, profileState);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::ResetAudioDialogEnhancement(const int32_t handle) {
        uint32_t result = _audio.ResetAudioDialogEnhancement(handle);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::ResetAudioBassEnhancer(const int32_t handle) {
        uint32_t result = _audio.ResetAudioBassEnhancer(handle);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::ResetAudioSurroundVirtualizer(const int32_t handle) {
        uint32_t result = _audio.ResetAudioSurroundVirtualizer(handle);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::ResetAudioVolumeLeveller(const int32_t handle) {
        uint32_t result = _audio.ResetAudioVolumeLeveller(handle);
        return result;
    }
    
    Core::hresult DeviceSettingsAudioImpl::GetAudioHDMIARCPortId(const int32_t handle, int32_t &portId) {
        uint32_t result = _audio.GetAudioHDMIARCPortId(handle, portId);
        return result;
    }

} // namespace Plugin
} // namespace WPEFramework