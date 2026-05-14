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
#include "CompositeIn.h"
#include "hal/dCompositeInImpl.h"

CompositeIn::CompositeIn(INotification& parent, std::shared_ptr<IPlatform> platform)
    : _platform(std::move(platform))
    , _parent(parent)
{
    LOGINFO("CompositeIn Constructor");
    Platform_init();
}

void CompositeIn::Platform_init()
{
    LOGINFO("CompositeIn Init - Setting up event callbacks");
    
    // Set up callback bundle for CompositeIn events - using global CallbackBundle pattern
    CallbackBundle bundle;
    
    bundle.OnCompositeInHotPlug = [this](const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const bool isConnected) {
        this->OnCompositeInHotPlug(port, isConnected);  // Call public method (matches other components)
    };
    bundle.OnCompositeInSignalStatus = [this](const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus signalStatus) {
        this->OnCompositeInSignalStatus(port, signalStatus);  // Call public method (matches other components)
    };
    bundle.OnCompositeInStatus = [this](const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const bool isPresented) {
        this->OnCompositeInStatus(activePort, isPresented);  // Call public method (matches other components)
    };
    bundle.OnCompositeInVideoModeUpdate = [this](const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const WPEFramework::Exchange::IDeviceSettingsCompositeIn::DisplayVideoPortResolution videoResolution) {
        this->OnCompositeInVideoModeUpdate(activePort, videoResolution);  // Call public method (matches other components)
    };
    
    if (_platform) {
        // Use interface method directly - no casting needed
        this->platform().setAllCallbacks(bundle);
        this->platform().getPersistenceValue();
    }
    
}

// CompositeIn interface methods - delegate to platform HAL implementation
uint32_t CompositeIn::GetNrOfCompositeInputs(int32_t &nrCompositeInputs)
{
    LOGINFO("GetNrOfCompositeInputs");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetNrOfCompositeInputs(nrCompositeInputs);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetNrOfCompositeInputs: SUCCESS - platform call completed successfully, nrCompositeInputs=%d", nrCompositeInputs);
    } else {
        LOGERR("GetNrOfCompositeInputs: FAILED - result=%u", result);
    }
    return result;
}

uint32_t CompositeIn::GetCompositeInStatus(CompositeInStatus &status)
{
    LOGINFO("GetCompositeInStatus");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetCompositeInStatus(status);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetCompositeInStatus: SUCCESS - activePort=%d, isPresented=%s", 
                static_cast<int>(status.activePort), status.isPresented ? "true" : "false");
    } else {
        LOGERR("GetCompositeInStatus: FAILED - result=%u", result);
    }
    return result;
}

uint32_t CompositeIn::SelectCompositeInPort(const CompositeInPort port)
{
    LOGINFO("SelectCompositeInPort: port=%d", static_cast<int>(port));
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SelectCompositeInPort(port);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SelectCompositeInPort: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SelectCompositeInPort: FAILED - result=%u", result);
    }
    return result;
}

uint32_t CompositeIn::ScaleCompositeInVideo(const CompositeInVideoRectangle videoRect)
{
    LOGINFO("ScaleCompositeInVideo: x=%d, y=%d, width=%d, height=%d", 
            videoRect.x, videoRect.y, videoRect.width, videoRect.height);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().ScaleCompositeInVideo(videoRect);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("ScaleCompositeInVideo: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("ScaleCompositeInVideo: FAILED - result=%u", result);
    }
    return result;
}

// Public event methods - Called by HAL callbacks to forward to INotification parent (matches other components)
void CompositeIn::OnCompositeInHotPlug(const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const bool isConnected)
{
    LOGINFO("CompositeIn OnCompositeInHotPlug event: port=%d, isConnected=%s", static_cast<int>(port), isConnected ? "true" : "false");
    _parent.OnCompositeInHotPlug(port, isConnected);
}

void CompositeIn::OnCompositeInSignalStatus(const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInPort port, const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInSignalStatus signalStatus)
{
    LOGINFO("CompositeIn OnCompositeInSignalStatus event: port=%d, signalStatus=%d", static_cast<int>(port), static_cast<int>(signalStatus));
    _parent.OnCompositeInSignalStatus(port, signalStatus);
}

void CompositeIn::OnCompositeInStatus(const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const bool isPresented)
{
    LOGINFO("CompositeIn OnCompositeInStatus event: activePort=%d, isPresented=%s", static_cast<int>(activePort), isPresented ? "true" : "false");
    _parent.OnCompositeInStatus(activePort, isPresented);
}

void CompositeIn::OnCompositeInVideoModeUpdate(const WPEFramework::Exchange::IDeviceSettingsCompositeIn::CompositeInPort activePort, const WPEFramework::Exchange::IDeviceSettingsCompositeIn::DisplayVideoPortResolution videoResolution)
{
    LOGINFO("CompositeIn OnCompositeInVideoModeUpdate event: activePort=%d", static_cast<int>(activePort));
    _parent.OnCompositeInVideoModeUpdate(activePort, videoResolution);
}

