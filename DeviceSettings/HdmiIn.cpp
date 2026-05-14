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

#include "UtilsLogging.h"

#include "HdmiIn.h"
#include "DeviceSettingsTypes.h"

using IPlatform = hal::dHdmiIn::IPlatform;
using DefaultImpl = dHdmiInImpl;

#include "hal/dHdmiIn.h"
namespace hal {
namespace dHdmiIn {
    IPlatform::~IPlatform() {}
}
}

HdmiIn::HdmiIn(INotification& parent, std::shared_ptr<IPlatform> platform)
    : _platform(std::move(platform))
    , _parent(parent)
{
    Platform_init();
}

void HdmiIn::Platform_init()
{
    CallbackBundle bundle;
    bundle.OnHDMIInHotPlugEvent = [this](HDMIInPort port, bool isConnected) {
        this->OnHDMIInHotPlugEvent(port, isConnected);
    };
    bundle.OnHDMIInSignalStatusEvent = [this](HDMIInPort port, HDMIInSignalStatus signalStatus) {
        this->OnHDMIInSignalStatusEvent(port, signalStatus);
    };
    bundle.OnHDMIInStatusEvent = [this](HDMIInPort port, bool isConnected) {
        this->OnHDMIInStatusEvent(port, isConnected);
    };
    bundle.OnHDMIInVideoModeUpdateEvent = [this](HDMIInPort port, HDMIVideoPortResolution videoPortResolution) {
        this->OnHDMIInVideoModeUpdateEvent(port, videoPortResolution);
    };
    bundle.OnHDMIInAllmStatusEvent = [this](HDMIInPort port, bool allmStatus) {
        this->OnHDMIInAllmStatusEvent(port, allmStatus);
    };
    bundle.OnHDMIInAVIContentTypeEvent = [this](HDMIInPort port, HDMIInAviContentType aviContentType) {
        this->OnHDMIInAVIContentTypeEvent(port, aviContentType);
    };
    bundle.OnHDMIInAVLatencyEvent = [this](int32_t audioDelay, int32_t videoDelay) {
        this->OnHDMIInAVLatencyEvent(audioDelay, videoDelay);
    };
    bundle.OnHDMIInVRRStatusEvent = [this](HDMIInPort port, HDMIInVRRType vrrType) {
        this->OnHDMIInVRRStatusEvent(port, vrrType);
    };
    if (_platform) {
        this->platform().setAllCallbacks(bundle);
        this->platform().getPersistenceValue();
    }
}

void HdmiIn::OnHDMIInHotPlugEvent(const HDMIInPort port, const bool isConnected)
{
    _parent.OnHDMIInEventHotPlugNotification(port, isConnected);
}

void HdmiIn::OnHDMIInSignalStatusEvent(const HDMIInPort port, const HDMIInSignalStatus signalStatus)
{
    _parent.OnHDMIInEventSignalStatusNotification(port, signalStatus);
}

void HdmiIn::OnHDMIInStatusEvent(const HDMIInPort activePort, const bool isPresented)
{
    _parent.OnHDMIInEventStatusNotification(activePort, isPresented);
}

void HdmiIn::OnHDMIInVideoModeUpdateEvent(const HDMIInPort port, const HDMIVideoPortResolution videoPortResolution)
{
    _parent.OnHDMIInVideoModeUpdateNotification(port, videoPortResolution);
}

void HdmiIn::OnHDMIInAllmStatusEvent(const HDMIInPort port, const bool allmStatus)
{
    _parent.OnHDMIInAllmStatusNotification(port, allmStatus);
}

void HdmiIn::OnHDMIInAVIContentTypeEvent(const HDMIInPort port, const HDMIInAviContentType aviContentType)
{
    _parent.OnHDMIInAVIContentTypeNotification(port, aviContentType);
}

void HdmiIn::OnHDMIInAVLatencyEvent(const int32_t audioDelay, const int32_t videoDelay)
{
    _parent.OnHDMIInAVLatencyNotification(audioDelay, videoDelay);
}

void HdmiIn::OnHDMIInVRRStatusEvent(const HDMIInPort port, const HDMIInVRRType vrrType)
{
    _parent.OnHDMIInVRRStatusNotification(port, vrrType);
}

uint32_t HdmiIn::GetHDMIInNumberOfInputs(int32_t &count) {

    LOGINFO("GetHDMIInNumbefOfInputs");
    this->platform().GetHDMIInNumberOfInputs(count);
    LOGINFO("GetHDMIInNumberOfInputs: SUCCESS - count=%d", count);


    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetHDMIInStatus(HDMIInStatus &hdmiStatus, IHDMIInPortConnectionStatusIterator*& portConnectionStatus) {

    LOGINFO("GetHDMIInStatus");
    this->platform().GetHDMIInStatus(hdmiStatus, portConnectionStatus);
    portConnectionStatus = nullptr;
    LOGINFO("GetHDMIInStatus: SUCCESS - platform call completed");

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::SelectHDMIInPort(const HDMIInPort port, const bool requestAudioMix, const bool topMostPlane, const HDMIVideoPlaneType videoPlaneType) {

    LOGINFO("SelectHDMIInPort: port=%d, requestAudioMix=%s, topMostPlane=%s, videoPlaneType=%d",
        port, requestAudioMix ? "true" : "false", topMostPlane ? "true" : "false", videoPlaneType);
    this->platform().SelectHDMIInPort(port, requestAudioMix, topMostPlane, videoPlaneType);
    LOGINFO("SelectHDMIInPort: SUCCESS - platform call completed");

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::ScaleHDMIInVideo(const HDMIInVideoRectangle videoPosition) {

    LOGINFO("ScaleHDMIInVideo: x=%d, y=%d, w=%d, h=%d", videoPosition.x, videoPosition.y, videoPosition.width, videoPosition.height);
    this->platform().ScaleHDMIInVideo(videoPosition);
    LOGINFO("ScaleHDMIInVideo: SUCCESS - platform call completed");

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::SelectHDMIZoomMode(const HDMIInVideoZoom zoomMode) {

    LOGINFO("SelectHDMIZoomMode: zoomMode=%d", zoomMode);
    this->platform().SelectHDMIZoomMode(zoomMode);
    LOGINFO("SelectHDMIZoomMode: SUCCESS - platform call completed");

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetSupportedGameFeaturesList(IHDMIInGameFeatureListIterator *& gameFeatureList) {

    LOGINFO("GetSupportedGameFeaturesList");
    this->platform().GetSupportedGameFeaturesList(gameFeatureList);
    LOGINFO("GetSupportedGameFeaturesList: SUCCESS - platform call completed");
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetHDMIInAVLatency(uint32_t &videoLatency, uint32_t &audioLatency) {

    LOGINFO("GetHDMIInAVLatency");
    this->platform().GetHDMIInAVLatency(videoLatency, audioLatency);
    LOGINFO("GetHDMIInAVLatency: SUCCESS - videoLatency=%u, audioLatency=%u", videoLatency, audioLatency);

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetHDMIInAllmStatus(const HDMIInPort port, bool &allmStatus) {

    LOGINFO("GetHDMIInAllmStatus: port=%d", port);
    this->platform().GetHDMIInAllmStatus(port, allmStatus);
    LOGINFO("GetHDMIInAllmStatus: SUCCESS - port=%d, allmStatus=%s", port, allmStatus ? "true" : "false");
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetHDMIInEdid2AllmSupport(const HDMIInPort port, bool &allmSupport) {

    LOGINFO("GetHDMIInEdid2AllmSupport: port=%d", port);
    this->platform().GetHDMIInEdid2AllmSupport(port, allmSupport);
    LOGINFO("GetHDMIInEdid2AllmSupport: SUCCESS - port=%d, allmSupport=%s", port, allmSupport ? "true" : "false");
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::SetHDMIInEdid2AllmSupport(const HDMIInPort port, bool allmSupport) {

    LOGINFO("SetHDMIInEdid2AllmSupport: port=%d, allmSupport=%s", port, allmSupport ? "true" : "false");
    this->platform().SetHDMIInEdid2AllmSupport(port, allmSupport);
    LOGINFO("SetHDMIInEdid2AllmSupport: SUCCESS - platform call completed");
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetEdidBytes(const HDMIInPort port, const uint16_t edidBytesLength, uint8_t edidBytes[]) {

    LOGINFO("GetEdidBytes: port=%d, edidBytesLength=%u", port, edidBytesLength);
    this->platform().GetEdidBytes(port, edidBytesLength, edidBytes);
    LOGINFO("GetEdidBytes: SUCCESS - platform call completed");

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetHDMISPDInformation(const HDMIInPort port, const uint16_t spdBytesLength, uint8_t spdBytes[]) {

    LOGINFO("GetHDMISPDInformation: port=%d, spdBytesLength=%u", port, spdBytesLength);
    this->platform().GetHDMISPDInformation(port, spdBytesLength, spdBytes);
    LOGINFO("GetHDMISPDInformation: SUCCESS - platform call completed");

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetHDMIEdidVersion(const HDMIInPort port, HDMIInEdidVersion &edidVersion) {

    LOGINFO("GetHDMIEdidVersion: port=%d", port);
    this->platform().GetHDMIEdidVersion(port, edidVersion);
    LOGINFO("GetHDMIEdidVersion: SUCCESS - port=%d, edidVersion=%d", port, edidVersion);
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::SetHDMIEdidVersion(const HDMIInPort port, const HDMIInEdidVersion edidVersion) {

    this->platform().SetHDMIEdidVersion(port, edidVersion);
    LOGINFO("SetHDMIEdidVersion: SUCCESS - port=%d, edidVersion=%d", port, edidVersion);

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetHDMIVideoMode(HDMIVideoPortResolution &videoPortResolution) {

    LOGINFO("GetHDMIVideoMode");
    this->platform().GetHDMIVideoMode(videoPortResolution);
    LOGINFO("GetHDMIVideoMode: SUCCESS - platform call completed");

    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetHDMIVersion(const HDMIInPort port, HDMIInCapabilityVersion &capabilityVersion) {

    LOGINFO("GetHDMIVersion: port=%d", port);
    this->platform().GetHDMIVersion(port, capabilityVersion);
    LOGINFO("GetHDMIVersion: SUCCESS - port=%d, capabilityVersion=%d", port, capabilityVersion);
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetVRRSupport(const HDMIInPort port, bool &vrrSupport) {

    LOGINFO("GetVRRSupport: port=%d", port);
    this->platform().GetVRRSupport(port, vrrSupport);
    LOGINFO("GetVRRSupport: SUCCESS - port=%d, vrrSupport=%s", port, vrrSupport ? "true" : "false");
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::SetVRRSupport(const HDMIInPort port, const bool vrrSupport) {

    LOGINFO("SetVRRSupport: port=%d, vrrSupport=%s", port, vrrSupport ? "true" : "false");
    this->platform().SetVRRSupport(port, vrrSupport);
    LOGINFO("SetVRRSupport: SUCCESS - platform call completed");
    return WPEFramework::Core::ERROR_NONE;
}

uint32_t HdmiIn::GetVRRStatus(const HDMIInPort port, HDMIInVRRStatus &vrrStatus) {

    LOGINFO("GetVRRStatus: port=%d", port);
    memset(&vrrStatus, 0, sizeof(vrrStatus));
    this->platform().GetVRRStatus(port, vrrStatus);
    LOGINFO("GetVRRStatus: SUCCESS - port=%d, vrrType=%d", port, vrrStatus.vrrType);

    return WPEFramework::Core::ERROR_NONE;
}