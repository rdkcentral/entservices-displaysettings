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

#include "DeviceSettingsVideoDeviceImplementation.h"

#include "UtilsLogging.h"
#include <syscall.h>

using namespace std;

namespace WPEFramework {
namespace Plugin {

    //SERVICE_REGISTRATION(DeviceSettingsVideoDeviceImpl, 1, 0);

    DeviceSettingsVideoDeviceImpl::DeviceSettingsVideoDeviceImpl() : 
        _VideoDeviceNotifications(),
        _apiLock(),
        _callbackLock(),
        _videoDevice(VideoDevice::Create(*this))
    {
        LOGINFO("DeviceSettingsVideoDeviceImpl Constructor - Instance Address: %p", this);
    }

    DeviceSettingsVideoDeviceImpl::~DeviceSettingsVideoDeviceImpl() {
        LOGINFO("DeviceSettingsVideoDeviceImpl Destructor - Instance Address: %p", this);
    }

    template<typename Func, typename... Args>
    void DeviceSettingsVideoDeviceImpl::dispatchVideoDeviceEvent(Func notifyFunc, Args&&... args) {
        LOGINFO(">>");
        _callbackLock.Lock();
        for (auto& notification : _VideoDeviceNotifications) {
            auto start = std::chrono::steady_clock::now();
            (notification->*notifyFunc)(std::forward<Args>(args)...);
            auto elapsed = std::chrono::steady_clock::now() - start;
            LOGINFO("client %p took %" PRId64 "ms to process IVideoDevice event", notification, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        }
        _callbackLock.Unlock();
        LOGINFO("<<");
    }

    template <typename T>
    Core::hresult DeviceSettingsVideoDeviceImpl::Register(std::list<T*>& list, T* notification)
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
    Core::hresult DeviceSettingsVideoDeviceImpl::Unregister(std::list<T*>& list, const T* notification)
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

    Core::hresult DeviceSettingsVideoDeviceImpl::Register(Exchange::IDeviceSettingsVideoDevice::INotification* notification)
    {
        Core::hresult errorCode = Register(_VideoDeviceNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IVideoDevice %p, errorCode: %u", notification, errorCode);
        } else {
            LOGINFO("IVideoDevice %p registered successfully", notification);
        }
        return errorCode;
    }

    Core::hresult DeviceSettingsVideoDeviceImpl::Unregister(Exchange::IDeviceSettingsVideoDevice::INotification* notification)
    {
        Core::hresult errorCode = Unregister(_VideoDeviceNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IVideoDevice %p, errorcode: %u", notification, errorCode);
        } else {
            LOGINFO("IVideoDevice %p unregistered successfully", notification);
        }
        return errorCode;
    }

    // VideoDevice::INotification interface implementations (called by DS HAL)
    void DeviceSettingsVideoDeviceImpl::OnZoomSettingsChanged(const VideoDeviceZoom zoomSetting)
    {
        LOGINFO("DS HAL OnZoomSettingsChanged event: zoomSetting=%d", static_cast<int>(zoomSetting));
        dispatchVideoDeviceEvent(&Exchange::IDeviceSettingsVideoDevice::INotification::OnZoomSettingsChanged, zoomSetting);
    }

    void DeviceSettingsVideoDeviceImpl::OnDisplayFrameratePreChange(const string frameRate)
    {
        LOGINFO("DS HAL OnDisplayFrameratePreChange event: frameRate=%s", frameRate.c_str());
        dispatchVideoDeviceEvent(&Exchange::IDeviceSettingsVideoDevice::INotification::OnDisplayFrameratePreChange, frameRate);
    }

    void DeviceSettingsVideoDeviceImpl::OnDisplayFrameratePostChange(const string frameRate)
    {
        LOGINFO("DS HAL OnDisplayFrameratePostChange event: frameRate=%s", frameRate.c_str());
        dispatchVideoDeviceEvent(&Exchange::IDeviceSettingsVideoDevice::INotification::OnDisplayFrameratePostChange, frameRate);
    }

    // VideoDevice interface method implementations called by DeviceSettingsImp 
    uint32_t DeviceSettingsVideoDeviceImpl::GetVideoDeviceHandle(const int32_t index, int32_t &handle)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.GetVideoDeviceHandle(index, handle);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetVideoDeviceHandle succeeded: index=%d, handle=%d", index, handle);
        } else {
            LOGERR("GetVideoDeviceHandle failed: index=%d, error=%u", index, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::SetVideoDeviceDFC(const int32_t handle, const VideoDeviceZoom zoomSetting)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.SetVideoDeviceDFC(handle, zoomSetting);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetVideoDeviceDFC succeeded for handle: %d, zoomSetting: %d", handle, static_cast<int>(zoomSetting));
        } else {
            LOGERR("SetVideoDeviceDFC failed for handle: %d, zoomSetting: %d, error: %u", handle, static_cast<int>(zoomSetting), result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::GetVideoDeviceDFC(const int32_t handle, VideoDeviceZoom &zoomSetting)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.GetVideoDeviceDFC(handle, zoomSetting);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetVideoDeviceDFC succeeded for handle: %d, zoomSetting: %d", handle, static_cast<int>(zoomSetting));
        } else {
            LOGERR("GetVideoDeviceDFC failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::GetHDRCapabilities(const int32_t handle, int32_t &capabilities)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.GetHDRCapabilities(handle, capabilities);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetHDRCapabilities succeeded for handle: %d, capabilities: 0x%x", handle, capabilities);
        } else {
            LOGERR("GetHDRCapabilities failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::GetSupportedVideoCodingFormats(const int32_t handle, int32_t &supportedFormats)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.GetSupportedVideoCodingFormats(handle, supportedFormats);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetSupportedVideoCodingFormats succeeded for handle: %d, supportedFormats: 0x%x", handle, supportedFormats);
        } else {
            LOGERR("GetSupportedVideoCodingFormats failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::GetCodecInfo(const int32_t handle, const VideoDeviceCodec videoCodec, IDeviceSettingsVideoCodecProfileSupportIterator*& codecInfo)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.GetCodecInfo(handle, videoCodec, codecInfo);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetCodecInfo succeeded for handle: %d, videoCodec: %d", handle, static_cast<int>(videoCodec));
        } else {
            LOGERR("GetCodecInfo failed for handle: %d, videoCodec: %d, error: %u", handle, static_cast<int>(videoCodec), result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::DisableHDR(const int32_t handle, const bool disable)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.DisableHDR(handle, disable);
        if (result == Core::ERROR_NONE) {
            LOGINFO("DisableHDR succeeded for handle: %d, disable: %s", handle, disable ? "true" : "false");
        } else {
            LOGERR("DisableHDR failed for handle: %d, disable: %s, error: %u", handle, disable ? "true" : "false", result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::SetFRFMode(const int32_t handle, const int32_t frfmode)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.SetFRFMode(handle, frfmode);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetFRFMode succeeded for handle: %d, frfmode: %d", handle, frfmode);
        } else {
            LOGERR("SetFRFMode failed for handle: %d, frfmode: %d, error: %u", handle, frfmode, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::GetFRFMode(const int32_t handle, int32_t &frfmode)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.GetFRFMode(handle, frfmode);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetFRFMode succeeded for handle: %d, frfmode: %d", handle, frfmode);
        } else {
            LOGERR("GetFRFMode failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::GetCurrentDisplayFrameRate(const int32_t handle, string &framerate)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.GetCurrentDisplayFrameRate(handle, framerate);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetCurrentDisplayFrameRate succeeded for handle: %d, framerate: %s", handle, framerate.c_str());
        } else {
            LOGERR("GetCurrentDisplayFrameRate failed for handle: %d, error: %u", handle, result);
        }
        return result;
    }

    uint32_t DeviceSettingsVideoDeviceImpl::SetDisplayFrameRate(const int32_t handle, const string framerate)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _videoDevice.SetDisplayFrameRate(handle, framerate);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetDisplayFrameRate succeeded for handle: %d, framerate: %s", handle, framerate.c_str());
        } else {
            LOGERR("SetDisplayFrameRate failed for handle: %d, framerate: %s, error: %u", handle, framerate.c_str(), result);
        }
        return result;
    }

} // namespace Plugin
} // namespace WPEFramework