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

#include "DeviceSettingsVideoPortImplementation.h"

#include "UtilsLogging.h"
#include <syscall.h>
#include <dlfcn.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <unistd.h>

using namespace std;

namespace {
    typedef struct _dlSymbolLookup {
        const char* name;
        void** dataptr;
    } dlSymbolLookup;

    typedef struct _videoPortConfigs {
        const dsVideoPortTypeConfig_t* pKConfigs;
        int* pKVideoPortConfigs_size;
        const dsVideoPortPortConfig_t* pKPorts;
        int* pKVideoPortPorts_size;
        dsVideoPortResolution_t* pKResolutionsSettings;
        int* pKResolutionsSettings_size;
    } videoPortConfigs_t;

    static bool LoadDLSymbols(void* pDLHandle, const dlSymbolLookup* symbols, const int numberOfSymbols)
    {
        int currentSymbols = 0;

        if ((pDLHandle == NULL) || (symbols == NULL)) {
            LOGERR("LoadDLSymbols(VideoPort): Invalid handle or symbols");
            return false;
        }

        for (int i = 0; i < numberOfSymbols; i++) {
            if ((symbols[i].dataptr == NULL) || (symbols[i].name == NULL)) {
                LOGERR("LoadDLSymbols(VideoPort): Invalid symbol entry at index %d", i);
                continue;
            }

            *(symbols[i].dataptr) = dlsym(pDLHandle, symbols[i].name);
            if (*(symbols[i].dataptr) != NULL) {
                currentSymbols++;
            } else {
                LOGWARN("LoadDLSymbols(VideoPort): [%s] not found", symbols[i].name);
            }
        }

        return (numberOfSymbols > 0) ? (currentSymbols == numberOfSymbols) : false;
    }

    static bool LoadVideoPortConfigFromHAL(videoPortConfigs_t& config)
    {
        void* pDLHandle = NULL;
        bool isSymbolsLoaded = false;

        memset(&config, 0, sizeof(config));

        dlerror();
        pDLHandle = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
        if (pDLHandle == NULL) {
            const char* dlErr = dlerror();
            LOGWARN("LoadVideoPortConfigFromHAL: dlopen failed for %s: %s", RDK_DSHAL_NAME, (dlErr ? dlErr : "unknown"));
            return false;
        }

        dlSymbolLookup videoPortConfigSymbols[] = {
            {"kVideoPortConfigs", (void**)&config.pKConfigs},
            {"kVideoPortConfigs_size", (void**)&config.pKVideoPortConfigs_size},
            {"kVideoPortPorts", (void**)&config.pKPorts},
            {"kVideoPortPorts_size", (void**)&config.pKVideoPortPorts_size},
            {"kResolutionsSettings", (void**)&config.pKResolutionsSettings},
            {"kResolutionsSettings_size", (void**)&config.pKResolutionsSettings_size}
        };

        isSymbolsLoaded = LoadDLSymbols(pDLHandle, videoPortConfigSymbols, sizeof(videoPortConfigSymbols) / sizeof(dlSymbolLookup));
        dlclose(pDLHandle);

        if (!isSymbolsLoaded) {
            LOGWARN("LoadVideoPortConfigFromHAL: Failed to load all video port symbols from HAL");
            return false;
        }

        if ((config.pKConfigs == NULL) || (config.pKPorts == NULL) || (config.pKResolutionsSettings == NULL) ||
            (config.pKVideoPortConfigs_size == NULL) || (config.pKVideoPortPorts_size == NULL) || (config.pKResolutionsSettings_size == NULL)) {
            LOGWARN("LoadVideoPortConfigFromHAL: HAL symbols loaded but one or more pointers are null");
            return false;
        }

        return true;
    }

    static void PopulateVideoPortConfig(std::vector<VideoPortTypeConfig>& videoPortTypes,
                                        std::vector<VideoPortPortConfig>& videoPorts,
                                        std::vector<VideoPortResolution>& resolutions)
    {
        videoPortConfigs_t halConfig;
        memset(&halConfig, 0, sizeof(halConfig));
        const bool loadedFromHAL = LoadVideoPortConfigFromHAL(halConfig);

        videoPortTypes.clear();
        videoPorts.clear();
        resolutions.clear();

        if (!loadedFromHAL) {
            LOGWARN("PopulateVideoPortConfig: HAL config not available, returning empty config");
            return;
        }

        const int configCount = *(halConfig.pKVideoPortConfigs_size);
        const int portCount = *(halConfig.pKVideoPortPorts_size);
        const int resolutionCount = *(halConfig.pKResolutionsSettings_size);

        for (int i = 0; i < configCount; i++) {
            const dsVideoPortTypeConfig_t& cfg = halConfig.pKConfigs[i];

            VideoPortTypeConfig typeCfg;
            typeCfg.typeId = static_cast<VideoPortType>(cfg.typeId);
            typeCfg.name = (cfg.name ? cfg.name : "");
            typeCfg.dtcpSupported = cfg.dtcpSupported;
            typeCfg.hdcpSupported = cfg.hdcpSupported;
            typeCfg.restrictedResollution = cfg.restrictedResollution;
            if ((cfg.supportedResolutions != NULL) && (cfg.numSupportedResolutions > 0)) {
                std::ostringstream supportedResolutions;
                for (size_t j = 0; j < cfg.numSupportedResolutions; ++j) {
                    if (j != 0) {
                        supportedResolutions << ',';
                    }
                    supportedResolutions << cfg.supportedResolutions[j].name;
                }
                typeCfg.supportedResolutionNames = supportedResolutions.str();
            } else {
                typeCfg.supportedResolutionNames.clear();
            }
            videoPortTypes.push_back(typeCfg);
        }

        for (int i = 0; i < portCount; i++) {
            const dsVideoPortPortConfig_t& cfg = halConfig.pKPorts[i];

            VideoPortPortConfig portCfg;
            portCfg.videoPortType = static_cast<VideoPortType>(cfg.id.type);
            portCfg.videoPortIndex = cfg.id.index;
            portCfg.connectedAudioPortType = static_cast<int32_t>(cfg.connectedAOP.type);
            portCfg.connectedAudioPortIndex = cfg.connectedAOP.index;
            portCfg.defaultResolution = (cfg.defaultResolution ? cfg.defaultResolution : "");
            videoPorts.push_back(portCfg);
        }

        for (int i = 0; i < resolutionCount; i++) {
            const dsVideoPortResolution_t& cfg = halConfig.pKResolutionsSettings[i];

            VideoPortResolution resCfg;
            resCfg.name = cfg.name;
            resCfg.pixelResolution = static_cast<VideoResolution>(cfg.pixelResolution);
            resCfg.aspectRatio = static_cast<VideoAspectRatio>(cfg.aspectRatio);
            resCfg.stereoScopicMode = static_cast<VideoStereoScopicMode>(cfg.stereoScopicMode);
            resCfg.frameRate = static_cast<VideoFrameRate>(cfg.frameRate);
            resCfg.interlaced = cfg.interlaced;
            resolutions.push_back(resCfg);
        }

    LOGINFO("PopulateVideoPortConfig: Loaded config from HAL (videoPortTypes=%zu videoPorts=%zu resolutions=%zu)",
        videoPortTypes.size(), videoPorts.size(), resolutions.size());
    }

    static void dumpConfig(const std::vector<VideoPortTypeConfig>& videoPortTypes,
                           const std::vector<VideoPortPortConfig>& videoPorts,
                           const std::vector<VideoPortResolution>& resolutions)
    {
        if (-1 == access("/opt/dsMgrDumpDeviceConfigs", F_OK)) {
            LOGINFO("dumpConfig(VideoPort): Dumping of Device configs is disabled");
            return;
        }

        LOGINFO("\n=============== Dump DeviceSettings VideoPort Cached Config ===============");
        LOGINFO("VideoPortTypes count=%zu", videoPortTypes.size());
        for (size_t i = 0; i < videoPortTypes.size(); ++i) {
            const VideoPortTypeConfig& cfg = videoPortTypes[i];
            LOGINFO("videoPortTypes[%zu]: typeId=%d name=%s dtcpSupported=%s hdcpSupported=%s restrictedResolution=%s supportedResolutionNames=%s",
                i,
                static_cast<int>(cfg.typeId),
                cfg.name.c_str(),
                cfg.dtcpSupported ? "true" : "false",
                cfg.hdcpSupported ? "true" : "false",
                cfg.restrictedResollution ? "true" : "false",
                cfg.supportedResolutionNames.c_str());
        }

        LOGINFO("VideoPorts count=%zu", videoPorts.size());
        for (size_t i = 0; i < videoPorts.size(); ++i) {
            const VideoPortPortConfig& cfg = videoPorts[i];
            LOGINFO("videoPorts[%zu]: videoPortType=%d videoPortIndex=%d connectedAudioPortType=%d connectedAudioPortIndex=%d defaultResolution=%s",
                i,
                static_cast<int>(cfg.videoPortType),
                cfg.videoPortIndex,
                cfg.connectedAudioPortType,
                cfg.connectedAudioPortIndex,
                cfg.defaultResolution.c_str());
        }

        LOGINFO("Resolutions count=%zu", resolutions.size());
        for (size_t i = 0; i < resolutions.size(); ++i) {
            const VideoPortResolution& cfg = resolutions[i];
            LOGINFO("resolutions[%zu]: name=%s pixelResolution=%d aspectRatio=%d stereoScopicMode=%d frameRate=%d interlaced=%s",
                i,
                cfg.name.c_str(),
                static_cast<int>(cfg.pixelResolution),
                static_cast<int>(cfg.aspectRatio),
                static_cast<int>(cfg.stereoScopicMode),
                static_cast<int>(cfg.frameRate),
                cfg.interlaced ? "true" : "false");
        }

        LOGINFO("=============== Dump DeviceSettings VideoPort Cached Config done ===============\n");
    }
}

namespace WPEFramework {
namespace Plugin {

    //SERVICE_REGISTRATION(DeviceSettingsVideoPortImpl, 1, 0);

    DeviceSettingsVideoPortImpl::DeviceSettingsVideoPortImpl() : 
        _VideoPortNotifications(),
        _apiLock(),
        _callbackLock(),
        _videoPort(VideoPort::Create(*this))
    {
        InitializeVideoPortConfigCache();
        LOGINFO("DeviceSettingsVideoPortImpl Constructor - Instance Address: %p", this);
    }

    DeviceSettingsVideoPortImpl::~DeviceSettingsVideoPortImpl() {
        LOGINFO("DeviceSettingsVideoPortImpl Destructor - Instance Address: %p", this);
    }

    void DeviceSettingsVideoPortImpl::InitializeVideoPortConfigCache()
    {
        _apiLock.Lock();
        PopulateVideoPortConfig(_cachedVideoPortTypes, _cachedVideoPorts, _cachedResolutions);
        dumpConfig(_cachedVideoPortTypes, _cachedVideoPorts, _cachedResolutions);
        _apiLock.Unlock();

        LOGINFO("InitializeVideoPortConfigCache: videoPortTypes=%zu videoPorts=%zu resolutions=%zu",
            _cachedVideoPortTypes.size(), _cachedVideoPorts.size(), _cachedResolutions.size());
    }

    template<typename Func, typename... Args>
    void DeviceSettingsVideoPortImpl::dispatchVideoPortEvent(Func notifyFunc, Args&&... args) {
        LOGINFO(">>");
        _callbackLock.Lock();
        for (auto& notification : _VideoPortNotifications) {
            auto start = std::chrono::steady_clock::now();
            (notification->*notifyFunc)(std::forward<Args>(args)...);
            auto elapsed = std::chrono::steady_clock::now() - start;
            LOGINFO("client %p took %" PRId64 "ms to process IVideoPort event", notification, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        }
        _callbackLock.Unlock();
        LOGINFO("<<");
    }

    template <typename T>
    Core::hresult DeviceSettingsVideoPortImpl::Register(std::list<T*>& list, T* notification)
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
    Core::hresult DeviceSettingsVideoPortImpl::Unregister(std::list<T*>& list, const T* notification)
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

    Core::hresult DeviceSettingsVideoPortImpl::Register(Exchange::IDeviceSettingsVideoPort::INotification* notification)
    {
        Core::hresult errorCode = Register(_VideoPortNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IVideoPort %p, errorCode: %u", notification, errorCode);
        } else {
            LOGINFO("IVideoPort %p registered successfully", notification);
        }
        return errorCode;
    }

    Core::hresult DeviceSettingsVideoPortImpl::Unregister(Exchange::IDeviceSettingsVideoPort::INotification* notification)
    {
        Core::hresult errorCode = Unregister(_VideoPortNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IVideoPort %p, errorcode: %u", notification, errorCode);
        } else {
            LOGINFO("IVideoPort %p unregistered successfully", notification);
        }
        return errorCode;
    }

    // Intermediate notification methods removed - DS HAL callbacks now directly call dispatchVideoPortEvent

    // VideoPort::INotification interface implementations (called by DS HAL)
    void DeviceSettingsVideoPortImpl::OnResolutionPreChange(const ResolutionChange resolution)
    {
        LOGINFO("DS HAL OnResolutionPreChange event: width=%u, height=%u", resolution.width, resolution.height);
        dispatchVideoPortEvent(&Exchange::IDeviceSettingsVideoPort::INotification::OnResolutionPreChange, resolution);
    }

    void DeviceSettingsVideoPortImpl::OnResolutionPostChange(const ResolutionChange resolution)
    {
        LOGINFO("DS HAL OnResolutionPostChange event: width=%u, height=%u", resolution.width, resolution.height);
        dispatchVideoPortEvent(&Exchange::IDeviceSettingsVideoPort::INotification::OnResolutionPostChange, resolution);
    }

    void DeviceSettingsVideoPortImpl::OnHDCPStatusChange(const VideoPortHdcpStatus hdcpStatus)
    {
        LOGINFO("DS HAL OnHDCPStatusChange event: hdcpStatus=%d", static_cast<int>(hdcpStatus));
        dispatchVideoPortEvent(&Exchange::IDeviceSettingsVideoPort::INotification::OnHDCPStatusChange, hdcpStatus);
    }

    void DeviceSettingsVideoPortImpl::OnVideoFormatUpdate(const HDRStandard videoFormatHDR)
    {
        LOGINFO("DS HAL OnVideoFormatUpdate event: videoFormatHDR=0x%x", static_cast<uint16_t>(videoFormatHDR));
        dispatchVideoPortEvent(&Exchange::IDeviceSettingsVideoPort::INotification::OnVideoFormatUpdate, videoFormatHDR);
    }

    // VideoPort interface method implementations called by DeviceSettingsImp 
    uint32_t DeviceSettingsVideoPortImpl::GetVideoPort(const VideoPortType videoPort, const int32_t index, int32_t &handle)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetVideoPort(videoPort, index, handle);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetVideoPort succeeded: videoPort=%d, index=%d, handle=%d", static_cast<int>(videoPort), index, handle);
        } else {
            LOGERR("GetVideoPort failed: videoPort=%d, index=%d, error=%u", static_cast<int>(videoPort), index, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetVideoPortConfig(IVideoPortTypeConfigIterator*& videoPortTypes,
                                                             IVideoPortPortConfigIterator*& videoPorts,
                                     IVideoPortResolutionIterator*& resolutions)
    {
        std::vector<VideoPortTypeConfig> typeConfigs;
        std::vector<VideoPortPortConfig> portConfigs;
        std::vector<VideoPortResolution> resolutionConfigs;

        _apiLock.Lock();
        typeConfigs = _cachedVideoPortTypes;
        portConfigs = _cachedVideoPorts;
        resolutionConfigs = _cachedResolutions;
        _apiLock.Unlock();

        dumpConfig(typeConfigs, portConfigs, resolutionConfigs);

        using VideoPortTypeIterator = RPC::IteratorType<IVideoPortTypeConfigIterator>;
        using VideoPortPortIterator = RPC::IteratorType<IVideoPortPortConfigIterator>;
        using ResolutionIterator = RPC::IteratorType<IVideoPortResolutionIterator>;

        videoPortTypes = Core::Service<VideoPortTypeIterator>::Create<IVideoPortTypeConfigIterator>(typeConfigs);
        videoPorts = Core::Service<VideoPortPortIterator>::Create<IVideoPortPortConfigIterator>(portConfigs);
        resolutions = Core::Service<ResolutionIterator>::Create<IVideoPortResolutionIterator>(resolutionConfigs);

        LOGINFO("GetVideoPortConfig: returning cached config videoPortTypes=%zu videoPorts=%zu resolutions=%zu",
            typeConfigs.size(), portConfigs.size(), resolutionConfigs.size());
        return Core::ERROR_NONE;
    }

    uint32_t DeviceSettingsVideoPortImpl::IsVideoPortEnabled(const int32_t handle, bool &enabled)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.IsVideoPortEnabled(handle, enabled);
        if (result == Core::ERROR_NONE) {
            LOGINFO("IsVideoPortEnabled succeeded for handle: %d, enabled: %s", handle, enabled ? "true" : "false");
        } else {
            LOGERR("IsVideoPortEnabled failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::EnableVideoPort(const int32_t handle, const bool enabled)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.EnableVideoPort(handle, enabled);
        if (result == Core::ERROR_NONE) {
            LOGINFO("EnableVideoPort succeeded for handle: %d, enabled: %s", handle, enabled ? "true" : "false");
        } else {
            LOGERR("EnableVideoPort failed for handle: %d, enabled: %s, error: %u", handle, enabled ? "true" : "false", result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::IsVideoPortDisplayConnected(const int32_t handle, bool &connected)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.IsVideoPortDisplayConnected(handle, connected);
        if (result == Core::ERROR_NONE) {
            LOGINFO("IsVideoPortDisplayConnected succeeded for handle: %d, connected: %s", handle, connected ? "true" : "false");
        } else {
            LOGERR("IsVideoPortDisplayConnected failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::IsVideoPortActive(const int32_t handle, bool &active)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.IsVideoPortActive(handle, active);
        if (result == Core::ERROR_NONE) {
            LOGINFO("IsVideoPortActive succeeded for handle: %d, active: %s", handle, active ? "true" : "false");
        } else {
            LOGERR("IsVideoPortActive failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetVideoPortResolution(const int32_t handle, VideoPortResolution &resolution)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetVideoPortResolution(handle, resolution);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetVideoPortResolution succeeded for handle: %d", handle);
        } else {
            LOGERR("GetVideoPortResolution failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::SetVideoPortResolution(const int32_t handle, const VideoPortResolution resolution, const bool persist, const bool forceCompatibility)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetVideoPortResolution(handle, resolution, persist, forceCompatibility);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetVideoPortResolution succeeded for handle: %d, persist: %s, forceCompatibility: %s", handle, persist ? "true" : "false", forceCompatibility ? "true" : "false");
        } else {
            LOGERR("SetVideoPortResolution failed for handle: %d, persist: %s, forceCompatibility: %s, error: %u", handle, persist ? "true" : "false", forceCompatibility ? "true" : "false", result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetColorSpace(const int32_t handle, VideoPortColorSpace &colorSpace)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetColorSpace(handle, colorSpace);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetColorSpace succeeded for handle: %d", handle);
        } else {
            LOGERR("GetColorSpace failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::SetColorSpace(const int32_t handle, const VideoPortColorSpace colorSpace, const bool persist)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetColorSpace(handle, colorSpace);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetColorSpace succeeded for handle: %d, persist: %s", handle, persist ? "true" : "false");
        } else {
            LOGERR("SetColorSpace failed for handle: %d, persist: %s, error: %u", handle, persist ? "true" : "false", result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetQuantizationRange(const int32_t handle, VideoPortQuantizationRange &quantizationRange)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetQuantizationRange(handle, quantizationRange);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetQuantizationRange succeeded for handle: %d", handle);
        } else {
            LOGERR("GetQuantizationRange failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::SetQuantizationRange(const int32_t handle, const VideoPortQuantizationRange quantizationRange, const bool persist)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetVideoPortQuantizationRange(handle, quantizationRange);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetQuantizationRange succeeded for handle: %d, persist: %s", handle, persist ? "true" : "false");
        } else {
            LOGERR("SetQuantizationRange failed for handle: %d, persist: %s, error: %u", handle, persist ? "true" : "false", result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetVideoPortHDCPStatus(const int32_t handle, VideoPortHdcpStatus &hdcpStatus)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetVideoPortHDCPStatus(handle, hdcpStatus);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetVideoPortHDCPStatus succeeded for handle: %d", handle);
        } else {
            LOGERR("GetVideoPortHDCPStatus failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetHDCPProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetHDCPProtocolVersionOnVideoPort(handle, hdcpVersion);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetHDCPProtocolVersionOnVideoPort succeeded for handle: %d", handle);
        } else {
            LOGERR("GetHDCPProtocolVersionOnVideoPort failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetVideoPortHDCPCurrentProtocol(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetHDCPCurrentProtocolVersionOnVideoPort(handle, hdcpVersion);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetVideoPortHDCPCurrentProtocol succeeded for handle: %d", handle);
        } else {
            LOGERR("GetVideoPortHDCPCurrentProtocol failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::SetVideoPortHDCPProfile(const int32_t handle, const VideoPortHdcpProtocolVersion hdcpVersion, const bool persist)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetHDMIPreference(handle, hdcpVersion);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetVideoPortHDCPProfile succeeded for handle: %d, persist: %s", handle, persist ? "true" : "false");
        } else {
            LOGERR("SetVideoPortHDCPProfile failed for handle: %d, persist: %s, error: %u", handle, persist ? "true" : "false", result);
        }
        return result;
    }

    // Additional VideoPort methods - stub implementations for now
    uint32_t DeviceSettingsVideoPortImpl::GetMatrixCoefficients(const int32_t handle, DisplayMatrixCoefficients &matrixCoefficients)
    {
        uint32_t result = Core::ERROR_GENERAL;
        DisplayMatrixCoefficients displayMatrixCoefficients;
        result = _videoPort.GetMatrixCoefficients(handle, displayMatrixCoefficients);
        if (result == Core::ERROR_NONE) {
            matrixCoefficients = static_cast<DisplayMatrixCoefficients>(displayMatrixCoefficients);
            LOGINFO("GetMatrixCoefficients succeeded: handle=%d, matrixCoefficients=%d", handle, static_cast<int>(matrixCoefficients));
        } else {
            LOGERR("GetMatrixCoefficients failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetCurrentOutputSettings(const int32_t handle, DSOutputSettings &outputSettings)
    {
        uint32_t result = Core::ERROR_GENERAL;
        DSOutputSettings dsOutputSettings;
        result = _videoPort.GetCurrentOutputSettings(handle, dsOutputSettings);
        if (result == Core::ERROR_NONE) {
            // Convert DSOutputSettings to DSOutputSettings
            outputSettings.videoEotf = static_cast<HDRStandard>(dsOutputSettings.videoEotf);
            outputSettings.matrixCoefficients = static_cast<DisplayMatrixCoefficients>(dsOutputSettings.matrixCoefficients);
            outputSettings.colorDepth = dsOutputSettings.colorDepth;
            outputSettings.colorSpace = static_cast<VideoPortColorSpace>(dsOutputSettings.colorSpace);
            outputSettings.quantizationRange = static_cast<VideoPortQuantizationRange>(dsOutputSettings.quantizationRange);
            LOGINFO("GetCurrentOutputSettings succeeded: handle=%d", handle);
        } else {
            LOGERR("GetCurrentOutputSettings failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::SetBackgroundColor(const int32_t handle, const VideoBackgroundColor backgroundColor)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetBackgroundColor(handle, backgroundColor);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetBackgroundColor succeeded: handle=%d, backgroundColor=%d", handle, static_cast<int>(backgroundColor));
        } else {
            LOGERR("SetBackgroundColor failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::SetForceHDRMode(const int32_t handle, const HDRStandard hdrMode)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetForceHDRMode(handle, hdrMode);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetForceHDRMode succeeded: handle=%d, hdrMode=%d", handle, static_cast<int>(hdrMode));
        } else {
            LOGERR("SetForceHDRMode failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetColorDepthCapabilities(const int32_t handle, uint32_t &colorDepthCapabilities)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetColorDepthCapabilities(handle, colorDepthCapabilities);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetColorDepthCapabilities succeeded: handle=%d, colorDepthCapabilities=0x%x", handle, colorDepthCapabilities);
        } else {
            LOGERR("GetColorDepthCapabilities failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetPreferredColorDepth(const int32_t handle, DisplayColorDepth &colorDepth, const bool persist)
    {
        uint32_t result = Core::ERROR_GENERAL;
        DisplayColorDepth displayColorDepth;
        result = _videoPort.GetPreferredColorDepth(handle, displayColorDepth, persist);
        if (result == Core::ERROR_NONE) {
            colorDepth = static_cast<DisplayColorDepth>(displayColorDepth);
            LOGINFO("GetPreferredColorDepth succeeded: handle=%d, colorDepth=%d, persist=%s", handle, static_cast<int>(colorDepth), persist ? "true" : "false");
        } else {
            LOGERR("GetPreferredColorDepth failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::SetPreferredColorDepth(const int32_t handle, const DisplayColorDepth colorDepth, const bool persist)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetPreferredColorDepth(handle, static_cast<DisplayColorDepth>(colorDepth), persist);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetPreferredColorDepth succeeded: handle=%d, colorDepth=%d, persist=%s", handle, static_cast<int>(colorDepth), persist ? "true" : "false");
        } else {
            LOGERR("SetPreferredColorDepth failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    // Additional methods required by DeviceSettingsImplementation.cpp and IDeviceSettingsVideoPort.h interface
    
    uint32_t DeviceSettingsVideoPortImpl::GetColorDepth(const int32_t handle, uint32_t &colorDepth)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetColorDepth(handle, colorDepth);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetColorDepth succeeded: handle=%d, colorDepth=%u", handle, colorDepth);
        } else {
            LOGERR("GetColorDepth failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::EnableHDCPOnVideoPort(const int32_t handle, const bool hdcpEnable, const uint8_t hdcpKey[], const uint16_t hdcpKeySize)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.EnableHDCPOnVideoPort(handle, hdcpEnable, hdcpKey, hdcpKeySize);
        if (result == Core::ERROR_NONE) {
            LOGINFO("EnableHDCPOnVideoPort succeeded: handle=%d, hdcpEnable=%s", handle, hdcpEnable ? "true" : "false");
        } else {
            LOGERR("EnableHDCPOnVideoPort failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::IsHDCPEnabledOnVideoPort(const int32_t handle, bool &hdcpEnabled)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.IsHDCPEnabledOnVideoPort(handle, hdcpEnabled);
        if (result == Core::ERROR_NONE) {
            LOGINFO("IsHDCPEnabledOnVideoPort succeeded: handle=%d, hdcpEnabled=%s", handle, hdcpEnabled ? "true" : "false");
        } else {
            LOGERR("IsHDCPEnabledOnVideoPort failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::GetTVHDRCapabilities(const int32_t handle, int32_t &capabilities)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetTVHDRCapabilities(handle, capabilities);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetTVHDRCapabilities succeeded: handle=%d, capabilities=0x%x", handle, capabilities);
        } else {
            LOGERR("GetTVHDRCapabilities failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::GetTVSupportedResolutions(const int32_t handle, int32_t &resolutions)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetTVSupportedResolutions(handle, resolutions);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetTVSupportedResolutions succeeded: handle=%d, resolutions=0x%x", handle, resolutions);
        } else {
            LOGERR("GetTVSupportedResolutions failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::SetForceDisable4K(const int32_t handle, const bool disable)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetForceDisable4K(handle, disable);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetForceDisable4K succeeded: handle=%d, disable=%s", handle, disable ? "true" : "false");
        } else {
            LOGERR("SetForceDisable4K failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::GetForceDisable4K(const int32_t handle, bool &disabled)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetForceDisable4K(handle, disabled);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetForceDisable4K succeeded: handle=%d, disabled=%s", handle, disabled ? "true" : "false");
        } else {
            LOGERR("GetForceDisable4K failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::IsVideoPortOutputHDR(const int32_t handle, bool &isHDR)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.IsVideoPortOutputHDR(handle, isHDR);
        if (result == Core::ERROR_NONE) {
            LOGINFO("IsVideoPortOutputHDR succeeded: handle=%d, isHDR=%s", handle, isHDR ? "true" : "false");
        } else {
            LOGERR("IsVideoPortOutputHDR failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::ResetVideoPortOutputToSDR()
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.ResetVideoPortOutputToSDR();
        if (result == Core::ERROR_NONE) {
            LOGINFO("ResetVideoPortOutputToSDR succeeded");
        } else {
            LOGERR("ResetVideoPortOutputToSDR failed: error=%u", result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::GetHDMIPreference(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetHDMIPreference(handle, hdcpVersion);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetHDMIPreference succeeded: handle=%d, hdcpVersion=%d", handle, static_cast<int>(hdcpVersion));
        } else {
            LOGERR("GetHDMIPreference failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::SetHDMIPreference(const int32_t handle, const VideoPortHdcpProtocolVersion hdcpVersion)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.SetHDMIPreference(handle, hdcpVersion);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetHDMIPreference succeeded: handle=%d, hdcpVersion=%d", handle, static_cast<int>(hdcpVersion));
        } else {
            LOGERR("SetHDMIPreference failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::GetVideoEOTF(const int32_t handle, HDRStandard &hdrStandard)
    {
        uint32_t result = Core::ERROR_GENERAL;
        HDRStandard interfaceHdrStandard;
        result = _videoPort.GetVideoEOTF(handle, interfaceHdrStandard);
        if (result == Core::ERROR_NONE) {
            hdrStandard = static_cast<HDRStandard>(interfaceHdrStandard);
            LOGINFO("GetVideoEOTF succeeded: handle=%d, hdrStandard=%d", handle, static_cast<int>(hdrStandard));
        } else {
            LOGERR("GetVideoEOTF failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::IsVideoPortDisplaySurround(const int32_t handle, bool &surround)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.IsVideoPortDisplaySurround(handle, surround);
        if (result == Core::ERROR_NONE) {
            LOGINFO("IsVideoPortDisplaySurround succeeded: handle=%d, surround=%s", handle, surround ? "true" : "false");
        } else {
            LOGERR("IsVideoPortDisplaySurround failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }
    
    uint32_t DeviceSettingsVideoPortImpl::GetVideoPortDisplaySurroundMode(const int32_t handle, VideoPortSurroundMode &surroundMode)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetVideoPortDisplaySurroundMode(handle, surroundMode);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetVideoPortDisplaySurroundMode succeeded: handle=%d, surroundMode=%d", handle, static_cast<int>(surroundMode));
        } else {
            LOGERR("GetVideoPortDisplaySurroundMode failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetHDCPReceiverProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetHDCPReceiverProtocolVersionOnVideoPort(handle, hdcpVersion);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetHDCPReceiverProtocolVersionOnVideoPort succeeded: handle=%d, hdcpVersion=%d", handle, static_cast<int>(hdcpVersion));
        } else {
            LOGERR("GetHDCPReceiverProtocolVersionOnVideoPort failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoPortImpl::GetHDCPCurrentProtocolVersionOnVideoPort(const int32_t handle, VideoPortHdcpProtocolVersion &hdcpVersion)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoPort.GetHDCPCurrentProtocolVersionOnVideoPort(handle, hdcpVersion);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetHDCPCurrentProtocolVersionOnVideoPort succeeded: handle=%d, hdcpVersion=%d", handle, static_cast<int>(hdcpVersion));
        } else {
            LOGERR("GetHDCPCurrentProtocolVersionOnVideoPort failed: handle=%d, error=%u", handle, result);
        }
        return result;
    }

} // namespace Plugin
} // namespace WPEFramework