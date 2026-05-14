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

#include <functional>
#include <unistd.h>
#include <memory>

#include <core/IAction.h>
#include <core/Time.h>
#include <core/WorkerPool.h>

#include "UtilsLogging.h"
#include "secure_wrapper.h"
#include "Display.h"

Display::Display(INotification& parent, std::shared_ptr<IPlatform> platform)
    : _platform(std::move(platform))
    , _parent(parent)
{
    LOGINFO("Display Constructor");
    Platform_init();
}

void Display::Platform_init()
{
    LOGINFO("Display Init - Setting up event callbacks");
    
    // Set up callback bundle for Display events - using global CallbackBundle pattern
    CallbackBundle bundle;
    
    bundle.OnDisplayRxSense = [this](const DisplayEvent displayEvent) {
        this->OnDisplayRxSense(displayEvent);
    };
    bundle.OnDisplayHDCPStatus = [this]() {
        this->OnDisplayHDCPStatus();
    };
    bundle.OnDisplayHDMIHotPlug = [this](const DisplayEvent displayEvent) {
        this->OnDisplayHDMIHotPlug(displayEvent);
    };
    
    if (_platform) {
        // Use interface method directly - no casting needed
        this->platform().setAllCallbacks(bundle);
        this->platform().getPersistenceValue();
    }
    
}

void Display::OnDisplayRxSense(const DisplayEvent displayEvent)
{
    LOGINFO("Display OnDisplayRxSense event: displayEvent=%d", static_cast<int>(displayEvent));
    _parent.OnDisplayRxSense(displayEvent);
}

void Display::OnDisplayHDCPStatus()
{
    LOGINFO("Display OnDisplayHDCPStatus event");
    _parent.OnDisplayHDCPStatus();
}

void Display::OnDisplayHDMIHotPlug(const DisplayEvent displayEvent)
{
    LOGINFO("Display OnDisplayHDMIHotPlug event: displayEvent=%d", static_cast<int>(displayEvent));
    _parent.OnDisplayHDMIHotPlug(displayEvent);
}

uint32_t Display::GetDisplayEdid(const int32_t handle, DisplayEDID &edId, IDSVideoPortResolutionIterator*& supportedResolutionList)
{
    uint32_t result = this->platform().GetDisplayEdid(handle, edId);
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetDisplayEdid succeeded: handle=%d", handle);
    } else {
        LOGERR("GetDisplayEdid failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::GetDisplayEdidBytes(const int32_t handle, uint8_t edIdBytes[], const uint16_t edidLength)
{
    uint32_t result = this->platform().GetDisplayEdidBytes(handle, edIdBytes, edidLength);
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetDisplayEdidBytes succeeded: handle=%d, edidLength=%d", handle, edidLength);
    } else {
        LOGERR("GetDisplayEdidBytes failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::DisplayInit()
{
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    // Initialize through platform interface - HAL is already initialized in constructor
    result = WPEFramework::Core::ERROR_NONE;
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("DisplayInit succeeded");
    } else {
        LOGERR("DisplayInit failed: error=%u", result);
    }
    return result;
}

uint32_t Display::DisplayTerm()
{
    uint32_t result = WPEFramework::Core::ERROR_NONE;
    // Termination handled by platform destructors
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("DisplayTerm succeeded");
    } else {
        LOGERR("DisplayTerm failed: error=%u", result);
    }
    return result;
}

uint32_t Display::GetDisplay(const int32_t type, const int32_t index, int32_t &handle)
{

    uint32_t result = this->platform().GetDisplay(type, index, handle);

    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("Display::GetDisplay SUCCESS: type=%d, index=%d, handle=%d", type, index, handle);
    } else {
        LOGERR("Display::GetDisplay FAILED: type=%d, index=%d, error=%u", type, index, result);
    }
    return result;
}

uint32_t Display::GetDisplayAspectRatio(const int32_t handle, DisplayVideoAspectRatio &aspectRatio)
{
    uint32_t result = this->platform().GetDisplayAspectRatio(handle, aspectRatio);
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetDisplayAspectRatio succeeded: handle=%d, aspectRatio=%d", handle, static_cast<int>(aspectRatio));
    } else {
        LOGERR("GetDisplayAspectRatio failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::SetAllmEnabled(const int32_t handle, const bool enabled)
{
    uint32_t result = this->platform().SetAllmEnabled(handle, enabled);
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetAllmEnabled succeeded: handle=%d, enabled=%s", handle, enabled ? "true" : "false");
    } else {
        LOGERR("SetAllmEnabled failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::SetAVIContentType(const int32_t handle, const int32_t contentType)
{
    uint32_t result = this->platform().SetAVIContentType(handle, contentType);
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetAVIContentType succeeded: handle=%d, contentType=%d", handle, contentType);
    } else {
        LOGERR("SetAVIContentType failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

uint32_t Display::SetAVIScanInformation(const int32_t handle, const int32_t scanInfo)
{
    uint32_t result = this->platform().SetAVIScanInformation(handle, scanInfo);
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetAVIScanInformation succeeded: handle=%d, scanInfo=%d", handle, scanInfo);
    } else {
        LOGERR("SetAVIScanInformation failed: handle=%d, error=%u", handle, result);
    }
    return result;
}

void Display::RegisterDisplayEventCallback()
{
    // Event callbacks are registered through platform initialization
    LOGINFO("RegisterDisplayEventCallback - handled by platform layer");
}

void Display::OnDisplayEvent(const int32_t handle, const DisplayEvent event, void *eventData)
{
    
    switch(event) {
        case DisplayEvent::DS_DISPLAY_RXSENSE_ON:
        case DisplayEvent::DS_DISPLAY_RXSENSE_OFF:
            OnDisplayRxSense(event);
            break;
            
        case DisplayEvent::DS_DISPLAY_HDCPPROTOCOL_CHANGE:
            OnDisplayHDCPStatus();
            break;
            
        case DisplayEvent::DS_DISPLAY_EVENT_CONNECTED:
        case DisplayEvent::DS_DISPLAY_EVENT_DISCONNECTED:
            OnDisplayHDMIHotPlug(event);
            break;
            
        default:
            LOGERR("Unknown display event: %d", static_cast<int>(event));
            break;
    }
    
}