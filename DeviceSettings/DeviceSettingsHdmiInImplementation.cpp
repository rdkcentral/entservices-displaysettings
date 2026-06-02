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

#include "DeviceSettingsHdmiInImplementation.h"

#include "UtilsLogging.h"
#include <syscall.h>

using namespace std;

namespace WPEFramework {
namespace Plugin {

    // Only DeviceSettingsImp should have SERVICE_REGISTRATION
    // This implementation is aggregated by DeviceSettingsImp
    //SERVICE_REGISTRATION(DeviceSettingsHdmiInImp, 1, 0);

    DeviceSettingsHdmiInImp::DeviceSettingsHdmiInImp()
        : _hdmiIn(HdmiIn::Create(*this))
    {
        LOGINFO("DeviceSettingsHdmiInImp Constructor - Instance Address: %p", this);
    }

    DeviceSettingsHdmiInImp::~DeviceSettingsHdmiInImp() {
        LOGINFO("DeviceSettingsHdmiInImp Destructor - Instance Address: %p", this);
    }

    template<typename Func, typename... Args>
    void DeviceSettingsHdmiInImp::dispatchHDMIInEvent(Func notifyFunc, Args&&... args) {
        LOGINFO(">>");
        _callbackLock.Lock();
        for (auto& notification : _HDMIInNotifications) {
            auto start = std::chrono::steady_clock::now();
            (notification->*notifyFunc)(std::forward<Args>(args)...);
            auto elapsed = std::chrono::steady_clock::now() - start;
            LOGINFO("client %p took %" PRId64 "ms to process IHDMIIn event", notification, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        }
        _callbackLock.Unlock();
        LOGINFO("<<");
    }

    template <typename T>
    Core::hresult DeviceSettingsHdmiInImp::Register(std::list<T*>& list, T* notification)
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
    Core::hresult DeviceSettingsHdmiInImp::Unregister(std::list<T*>& list, const T* notification)
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


    Core::hresult DeviceSettingsHdmiInImp::Register(DeviceSettingsHDMIIn::INotification* notification)
    {
        Core::hresult errorCode = Register(_HDMIInNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IHDMIIn %p, errorCode: %u", notification, errorCode);
        } else {
            LOGINFO("IHDMIIn %p registered successfully", notification);
        }
        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::Unregister(DeviceSettingsHDMIIn::INotification* notification)
    {
        Core::hresult errorCode = Unregister(_HDMIInNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IHDMIIn %p, errorcode: %u", notification, errorCode);
        } else {
            LOGINFO("IHDMIIn %p unregistered successfully", notification);
        }
        return errorCode;
    }

    void DeviceSettingsHdmiInImp::OnHDMIInEventHotPlugNotification(const HDMIInPort port, const bool isConnected)
    {
        LOGINFO("OnHDMIInEventHotPlug event Received");
        dispatchHDMIInEvent(&DeviceSettingsHDMIIn::INotification::OnHDMIInEventHotPlug, port, isConnected);
    }

    void DeviceSettingsHdmiInImp::OnHDMIInEventSignalStatusNotification(const HDMIInPort port, const HDMIInSignalStatus signalStatus)
    {
        LOGINFO("OnHDMIInEventSignalStatus event Received");
        dispatchHDMIInEvent(&DeviceSettingsHDMIIn::INotification::OnHDMIInEventSignalStatus, port, signalStatus);
    }

    void DeviceSettingsHdmiInImp::OnHDMIInAVLatencyNotification(const int32_t audioDelay, const int32_t videoDelay)
    {
        LOGINFO("OnHDMIInAVLatency event Received");
        dispatchHDMIInEvent(&DeviceSettingsHDMIIn::INotification::OnHDMIInAVLatency, audioDelay, videoDelay);
    }

    void DeviceSettingsHdmiInImp::OnHDMIInEventStatusNotification(const HDMIInPort activePort, const bool isPresented)
    {
        LOGINFO("OnHDMIInEventStatus event Received");
        dispatchHDMIInEvent(&DeviceSettingsHDMIIn::INotification::OnHDMIInEventStatus, activePort, isPresented);
    }

    void DeviceSettingsHdmiInImp::OnHDMIInVideoModeUpdateNotification(const HDMIInPort port, const HDMIVideoPortResolution videoPortResolution)
    {
        LOGINFO("OnHDMIInVideoModeUpdate event Received");
        dispatchHDMIInEvent(&DeviceSettingsHDMIIn::INotification::OnHDMIInVideoModeUpdate, port, videoPortResolution);
    }

    void DeviceSettingsHdmiInImp::OnHDMIInAllmStatusNotification(const HDMIInPort port, const bool allmStatus)
    {
        LOGINFO("OnHDMIInAllmStatus event Received");
        dispatchHDMIInEvent(&DeviceSettingsHDMIIn::INotification::OnHDMIInAllmStatus, port, allmStatus);
    }

    void DeviceSettingsHdmiInImp::OnHDMIInAVIContentTypeNotification(const HDMIInPort port, const HDMIInAviContentType aviContentType)
    {
        LOGINFO("OnHDMIInAVIContentType event Received");
        dispatchHDMIInEvent(&DeviceSettingsHDMIIn::INotification::OnHDMIInAVIContentType, port, aviContentType);
    }

    void DeviceSettingsHdmiInImp::OnHDMIInVRRStatusNotification(const HDMIInPort port, const HDMIInVRRType vrrType)
    {
        LOGINFO("OnHDMIInVRRStatus event Received");
        dispatchHDMIInEvent(&DeviceSettingsHDMIIn::INotification::OnHDMIInVRRStatus, port, vrrType);
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInNumbefOfInputs(int32_t &count) {

        LOGINFO("GetHDMIInNumberOfInputs");
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetHDMIInNumberOfInputs(count) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMIInNumberOfInputs: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetHDMIInNumberOfInputs: SUCCESS - count=%d", count);

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInStatus(HDMIInStatus &hdmiStatus, IHDMIInPortConnectionStatusIterator*& portConnectionStatus) {

        LOGINFO("GetHDMIInStatus");
        Core::hresult errorCode = Core::ERROR_GENERAL;

        _apiLock.Lock();
        if (_hdmiIn.GetHDMIInStatus(hdmiStatus, portConnectionStatus) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMIInStatus: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("GetHDMIInStatus: SUCCESS - platform call completed");
        LOGINFO("GetHDMIInStatus: activePort=%d, isPresented=%s", hdmiStatus.activePort, hdmiStatus.isPresented ? "true" : "false");


        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::SelectHDMIInPort(const HDMIInPort port, const bool requestAudioMix, const bool topMostPlane, const HDMIVideoPlaneType videoPlaneType) {

        LOGINFO("SelectHDMIInPort: port=%d, requestAudioMix=%s, topMostPlane=%s, videoPlaneType=%d",
            port, requestAudioMix ? "true" : "false", topMostPlane ? "true" : "false", videoPlaneType);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.SelectHDMIInPort(port, requestAudioMix, topMostPlane, videoPlaneType) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SelectHDMIInPort: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("SelectHDMIInPort: SUCCESS - platform call completed");


        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::ScaleHDMIInVideo(const HDMIInVideoRectangle videoPosition) {

        LOGINFO("ScaleHDMIInVideo: x=%d, y=%d, w=%d, h=%d", videoPosition.x, videoPosition.y, videoPosition.width, videoPosition.height);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.ScaleHDMIInVideo(videoPosition) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("ScaleHDMIInVideo: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("ScaleHDMIInVideo: SUCCESS - platform call completed");


        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::SelectHDMIZoomMode(const HDMIInVideoZoom zoomMode) {

        LOGINFO("SelectHDMIZoomMode: zoomMode=%d", zoomMode);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.SelectHDMIZoomMode(zoomMode) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SelectHDMIZoomMode: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("SelectHDMIZoomMode: SUCCESS - platform call completed");

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetSupportedGameFeaturesList(IHDMIInGameFeatureListIterator *& gameFeatureList) {

        LOGINFO("GetSupportedGameFeaturesList");
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetSupportedGameFeaturesList(gameFeatureList) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetSupportedGameFeaturesList: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("GetSupportedGameFeaturesList: SUCCESS - platform call completed");

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInAVLatency(uint32_t &videoLatency, uint32_t &audioLatency) {

        LOGINFO("GetHDMIInAVLatency");
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetHDMIInAVLatency(videoLatency, audioLatency) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMIInAVLatency: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetHDMIInAVLatency: SUCCESS - videoLatency=%u, audioLatency=%u", videoLatency, audioLatency);

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInAllmStatus(const HDMIInPort port, bool &allmStatus) {

        LOGINFO("GetHDMIInAllmStatus: port=%d", port);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetHDMIInAllmStatus(port, allmStatus) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMIInAllmStatus: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("GetHDMIInAllmStatus: SUCCESS - port=%d, allmStatus=%s", port, allmStatus ? "true" : "false");

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInEdid2AllmSupport(const HDMIInPort port, bool &allmSupport) {

        LOGINFO("GetHDMIInEdid2AllmSupport: port=%d", port);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetHDMIInEdid2AllmSupport(port, allmSupport) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMIInEdid2AllmSupport: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("GetHDMIInEdid2AllmSupport: SUCCESS - port=%d, allmSupport=%s", port, allmSupport ? "true" : "false");

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::SetHDMIInEdid2AllmSupport(const HDMIInPort port, bool allmSupport) {

        LOGINFO("SetHDMIInEdid2AllmSupport: port=%d, allmSupport=%s", port, allmSupport ? "true" : "false");
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.SetHDMIInEdid2AllmSupport(port, allmSupport) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetHDMIInEdid2AllmSupport: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("SetHDMIInEdid2AllmSupport: SUCCESS - platform call completed");

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetEdidBytes(const HDMIInPort port, const uint16_t edidBytesLength, uint8_t edidBytes[]) {

        LOGINFO("GetEdidBytes: port=%d, edidBytesLength=%u", port, edidBytesLength);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetEdidBytes(port, edidBytesLength, edidBytes) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetEdidBytes: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetEdidBytes: SUCCESS - port=%d, edidBytes[0]=0x%X", port, edidBytes[0]);


        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMISPDInformation(const HDMIInPort port, const uint16_t spdBytesLength, uint8_t spdBytes[]) {

        LOGINFO("GetHDMISPDInformation: port=%d, spdBytesLength=%u", port, spdBytesLength);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        if (spdBytes && spdBytesLength > 0) {
            spdBytes[0] = 0x00; // Example value
        }
        _apiLock.Lock();
        if (_hdmiIn.GetHDMISPDInformation(port, spdBytesLength, spdBytes) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMISPDInformation: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("GetHDMISPDInformation: SUCCESS - platform call completed");
        LOGINFO("GetHDMISPDInformation: port=%d, spdBytes[0]=0x%X", port, spdBytes[0]);


        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIEdidVersion(const HDMIInPort port, HDMIInEdidVersion &edidVersion) {

        LOGINFO("GetHDMIEdidVersion: port=%d", port);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetHDMIEdidVersion(port, edidVersion) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMIEdidVersion: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetHDMIEdidVersion: SUCCESS - port=%d, edidVersion=%d", port, edidVersion);

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::SetHDMIEdidVersion(const HDMIInPort port, const HDMIInEdidVersion edidVersion) {

        LOGINFO("SetHDMIEdidVersion: port=%d, edidVersion=%d", port, edidVersion);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.SetHDMIEdidVersion(port, edidVersion) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetHDMIEdidVersion: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("SetHDMIEdidVersion: SUCCESS - platform call completed");


        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIVideoMode(HDMIVideoPortResolution &videoPortResolution) {

        LOGINFO("GetHDMIVideoMode");
        Core::hresult errorCode = Core::ERROR_GENERAL;

        _apiLock.Lock();
        if (_hdmiIn.GetHDMIVideoMode(videoPortResolution) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMIVideoMode: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetHDMIVideoMode: SUCCESS - resolution=%s", videoPortResolution.name.c_str());

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIVersion(const HDMIInPort port, HDMIInCapabilityVersion &capabilityVersion) {

        LOGINFO("GetHDMIVersion: port=%d", port);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetHDMIVersion(port, capabilityVersion) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetHDMIVersion: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetHDMIVersion: SUCCESS - port=%d, capabilityVersion=%d", port, capabilityVersion);

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetVRRSupport(const HDMIInPort port, bool &vrrSupport) {

        LOGINFO("GetVRRSupport: port=%d", port);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetVRRSupport(port, vrrSupport) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetVRRSupport: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetVRRSupport: SUCCESS - port=%d, vrrSupport=%s", port, vrrSupport ? "true" : "false");

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::SetVRRSupport(const HDMIInPort port, const bool vrrSupport) {

        LOGINFO("SetVRRSupport: port=%d, vrrSupport=%s", port, vrrSupport ? "true" : "false");
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.SetVRRSupport(port, vrrSupport) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetVRRSupport: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("SetVRRSupport: SUCCESS - platform call completed");

        return errorCode;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetVRRStatus(const HDMIInPort port, HDMIInVRRStatus &vrrStatus) {

        LOGINFO("GetVRRStatus: port=%d", port);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_hdmiIn.GetVRRStatus(port, vrrStatus) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetVRRStatus: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetVRRStatus: SUCCESS - port=%d, vrrStatus.vrrType=%d", port, vrrStatus.vrrType);

        return errorCode;
    }

} // namespace Plugin
} // namespace WPEFramework
