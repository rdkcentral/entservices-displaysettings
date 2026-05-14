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
        _apiLock.Lock();
        _hdmiIn.GetHDMIInNumberOfInputs(count);
        _apiLock.Unlock();
        LOGINFO("GetHDMIInNumberOfInputs: SUCCESS - count=%d", count);

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInStatus(HDMIInStatus &hdmiStatus, IHDMIInPortConnectionStatusIterator*& portConnectionStatus) {

        LOGINFO("GetHDMIInStatus");

        _apiLock.Lock();
        _hdmiIn.GetHDMIInStatus(hdmiStatus, portConnectionStatus);
        _apiLock.Unlock();
        
        LOGINFO("GetHDMIInStatus: SUCCESS - platform call completed");
        LOGINFO("GetHDMIInStatus: activePort=%d, isPresented=%s", hdmiStatus.activePort, hdmiStatus.isPresented ? "true" : "false");


        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::SelectHDMIInPort(const HDMIInPort port, const bool requestAudioMix, const bool topMostPlane, const HDMIVideoPlaneType videoPlaneType) {

        LOGINFO("SelectHDMIInPort: port=%d, requestAudioMix=%s, topMostPlane=%s, videoPlaneType=%d",
            port, requestAudioMix ? "true" : "false", topMostPlane ? "true" : "false", videoPlaneType);
        _apiLock.Lock();
        _hdmiIn.SelectHDMIInPort(port, requestAudioMix, topMostPlane, videoPlaneType);
        _apiLock.Unlock();
        
        LOGINFO("SelectHDMIInPort: SUCCESS - platform call completed");


        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::ScaleHDMIInVideo(const HDMIInVideoRectangle videoPosition) {

        LOGINFO("ScaleHDMIInVideo: x=%d, y=%d, w=%d, h=%d", videoPosition.x, videoPosition.y, videoPosition.width, videoPosition.height);
        _apiLock.Lock();
        _hdmiIn.ScaleHDMIInVideo(videoPosition);
        _apiLock.Unlock();
        
        LOGINFO("ScaleHDMIInVideo: SUCCESS - platform call completed");


        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::SelectHDMIZoomMode(const HDMIInVideoZoom zoomMode) {

        LOGINFO("SelectHDMIZoomMode: zoomMode=%d", zoomMode);
        _apiLock.Lock();
        _hdmiIn.SelectHDMIZoomMode(zoomMode);
        _apiLock.Unlock();
        
        LOGINFO("SelectHDMIZoomMode: SUCCESS - platform call completed");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetSupportedGameFeaturesList(IHDMIInGameFeatureListIterator *& gameFeatureList) {

        LOGINFO("GetSupportedGameFeaturesList");
        _apiLock.Lock();
        _hdmiIn.GetSupportedGameFeaturesList(gameFeatureList);
        _apiLock.Unlock();
        
        LOGINFO("GetSupportedGameFeaturesList: SUCCESS - platform call completed");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInAVLatency(uint32_t &videoLatency, uint32_t &audioLatency) {

        LOGINFO("GetHDMIInAVLatency");
        _apiLock.Lock();
        _hdmiIn.GetHDMIInAVLatency(videoLatency, audioLatency);
        _apiLock.Unlock();
        LOGINFO("GetHDMIInAVLatency: SUCCESS - videoLatency=%u, audioLatency=%u", videoLatency, audioLatency);

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInAllmStatus(const HDMIInPort port, bool &allmStatus) {

        LOGINFO("GetHDMIInAllmStatus: port=%d", port);
        _apiLock.Lock();
        _hdmiIn.GetHDMIInAllmStatus(port, allmStatus);
        _apiLock.Unlock();
        
        LOGINFO("GetHDMIInAllmStatus: SUCCESS - port=%d, allmStatus=%s", port, allmStatus ? "true" : "false");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIInEdid2AllmSupport(const HDMIInPort port, bool &allmSupport) {

        LOGINFO("GetHDMIInEdid2AllmSupport: port=%d", port);
        _apiLock.Lock();
        _hdmiIn.GetHDMIInEdid2AllmSupport(port, allmSupport);
        _apiLock.Unlock();
        
        LOGINFO("GetHDMIInEdid2AllmSupport: SUCCESS - port=%d, allmSupport=%s", port, allmSupport ? "true" : "false");
        LOGINFO("GetHDMIInEdid2AllmSupport: SUCCESS - port=%d, allmSupport=%s", port, allmSupport ? "true" : "false");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::SetHDMIInEdid2AllmSupport(const HDMIInPort port, bool allmSupport) {

        LOGINFO("SetHDMIInEdid2AllmSupport: port=%d, allmSupport=%s", port, allmSupport ? "true" : "false");
        _apiLock.Lock();
        _hdmiIn.SetHDMIInEdid2AllmSupport(port, allmSupport);
        _apiLock.Unlock();
        
        LOGINFO("SetHDMIInEdid2AllmSupport: SUCCESS - platform call completed");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetEdidBytes(const HDMIInPort port, const uint16_t edidBytesLength, uint8_t edidBytes[]) {

        LOGINFO("GetEdidBytes: port=%d, edidBytesLength=%u", port, edidBytesLength);
        _apiLock.Lock();
        _hdmiIn.GetEdidBytes(port, edidBytesLength, edidBytes);
        _apiLock.Unlock();
        LOGINFO("GetEdidBytes: SUCCESS - port=%d, edidBytes[0]=0x%X", port, edidBytes[0]);


        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMISPDInformation(const HDMIInPort port, const uint16_t spdBytesLength, uint8_t spdBytes[]) {

        LOGINFO("GetHDMISPDInformation: port=%d, spdBytesLength=%u", port, spdBytesLength);
        if (spdBytes && spdBytesLength > 0) {
            spdBytes[0] = 0x00; // Example value
        }
        _apiLock.Lock();
        _hdmiIn.GetHDMISPDInformation(port, spdBytesLength, spdBytes);
        _apiLock.Unlock();
        
        LOGINFO("GetHDMISPDInformation: SUCCESS - platform call completed");
        LOGINFO("GetHDMISPDInformation: port=%d, spdBytes[0]=0x%X", port, spdBytes[0]);


        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIEdidVersion(const HDMIInPort port, HDMIInEdidVersion &edidVersion) {

        LOGINFO("GetHDMIEdidVersion: port=%d", port);
        _apiLock.Lock();
        _hdmiIn.GetHDMIEdidVersion(port, edidVersion);
        _apiLock.Unlock();
        LOGINFO("GetHDMIEdidVersion: SUCCESS - port=%d, edidVersion=%d", port, edidVersion);

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::SetHDMIEdidVersion(const HDMIInPort port, const HDMIInEdidVersion edidVersion) {

        LOGINFO("SetHDMIEdidVersion: port=%d, edidVersion=%d", port, edidVersion);
        _apiLock.Lock();
        _hdmiIn.SetHDMIEdidVersion(port, edidVersion);
        _apiLock.Unlock();
        
        LOGINFO("SetHDMIEdidVersion: SUCCESS - platform call completed");


        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIVideoMode(HDMIVideoPortResolution &videoPortResolution) {

        LOGINFO("GetHDMIVideoMode");

        _apiLock.Lock();
        _hdmiIn.GetHDMIVideoMode(videoPortResolution);
        _apiLock.Unlock();
        LOGINFO("GetHDMIVideoMode: SUCCESS - resolution=%s", videoPortResolution.name.c_str());

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetHDMIVersion(const HDMIInPort port, HDMIInCapabilityVersion &capabilityVersion) {

        LOGINFO("GetHDMIVersion: port=%d", port);
        _apiLock.Lock();
        _hdmiIn.GetHDMIVersion(port, capabilityVersion);
        _apiLock.Unlock();

        LOGINFO("GetHDMIVersion: SUCCESS - port=%d, capabilityVersion=%d", port, capabilityVersion);

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetVRRSupport(const HDMIInPort port, bool &vrrSupport) {

        LOGINFO("GetVRRSupport: port=%d", port);
        _apiLock.Lock();
        _hdmiIn.GetVRRSupport(port, vrrSupport);
        _apiLock.Unlock();
        LOGINFO("GetVRRSupport: SUCCESS - port=%d, vrrSupport=%s", port, vrrSupport ? "true" : "false");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::SetVRRSupport(const HDMIInPort port, const bool vrrSupport) {

        LOGINFO("SetVRRSupport: port=%d, vrrSupport=%s", port, vrrSupport ? "true" : "false");
        _apiLock.Lock();
        _hdmiIn.SetVRRSupport(port, vrrSupport);
        _apiLock.Unlock();
        
        LOGINFO("SetVRRSupport: SUCCESS - platform call completed");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsHdmiInImp::GetVRRStatus(const HDMIInPort port, HDMIInVRRStatus &vrrStatus) {

        LOGINFO("GetVRRStatus: port=%d", port);
        _apiLock.Lock();
        _hdmiIn.GetVRRStatus(port, vrrStatus);
        _apiLock.Unlock();
        LOGINFO("GetVRRStatus: SUCCESS - port=%d, vrrStatus.vrrType=%d", port, vrrStatus.vrrType);

        return Core::ERROR_NONE;
    }

} // namespace Plugin
} // namespace WPEFramework
