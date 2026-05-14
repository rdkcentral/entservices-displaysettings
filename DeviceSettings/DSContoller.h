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

#pragma once

#include "Module.h"

#include <memory>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <pthread.h>

#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>

//#include <interfaces/IDeviceSettingsManager.h>
#include <interfaces/IDeviceSettingsAudio.h>
#include <interfaces/IDeviceSettingsCompositeIn.h>
#include <interfaces/IDeviceSettingsDisplay.h>
#include <interfaces/IDeviceSettingsFPD.h>
#include <interfaces/IDeviceSettingsHDMIIn.h>
#include <interfaces/IDeviceSettingsHost.h>
#include <interfaces/IDeviceSettingsVideoDevice.h>
#include <interfaces/IDeviceSettingsVideoPort.h>


#include "fpd.h"
#include "HdmiIn.h"

#include "list.hpp"
#include "DeviceSettingsTypes.h"

// DS HAL headers with built-in C++ protection
#include "dsTypes.h"
#include "dsVideoPort.h"
#include "dsDisplay.h"
#include "dsAudio.h"

// GLib forward declarations
typedef struct _GMainLoop GMainLoop;
typedef int gboolean;
typedef void* gpointer;
typedef unsigned int guint;

namespace WPEFramework {
namespace Plugin {
    class DSController
        {
    public:
        // We do not allow this plugin to be copied !!
        DSController();
        ~DSController();

        static DSController* instance(DSController* DSController = nullptr);

        // We do not allow this plugin to be copied !!
        DSController(const DSController&)            = delete;
        DSController& operator=(const DSController&) = delete;

    public:
        class EXTERNAL LambdaJob : public Core::IDispatch {
        protected:
            LambdaJob(DSController* impl, std::function<void()> lambda)
                : _impl(impl)
                , _lambda(std::move(lambda))
            {
            }

        public:
            LambdaJob()                            = delete;
            LambdaJob(const LambdaJob&)            = delete;
            LambdaJob& operator=(const LambdaJob&) = delete;
            ~LambdaJob() {}

            static Core::ProxyType<Core::IDispatch> Create(DSController* impl, std::function<void()> lambda)
            {
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<LambdaJob>::Create(impl, std::move(lambda))));
            }

            virtual void Dispatch()
            {
                _lambda();
            }

        private:
            DSController* _impl;
            std::function<void()> _lambda;
        };

    public:
        // Main initialization and lifecycle methods
        void DeviceManager_Init();
        void InitializeIARM();
        uint32_t Start();
        uint32_t Stop();
        void Loop();
        
        // DSMgr functionality methods
        void Init();
        void Deinit();
        
    private:
        // Internal methods migrated from dsMgr daemon
        void InitializeResolutionThread();
        void SetVideoPortResolution();
        void SetResolution(intptr_t* handle, dsVideoPortType_t portType);
        void SetAudioMode();
        void SetEASAudioMode();
        void SetBackgroundColor(dsVideoBackgroundColor_t color);
        void DumpHdmiEdidInfo(dsDisplayEDID_t* pedidData);
        
        // Event handlers
        void EventHandler(const char *owner, int eventId, void *data, size_t len);
        void SysModeChange(void *arg);
        
        // Helper methods
        static intptr_t GetVideoPortHandle(dsVideoPortType_t port);
        static bool IsHDMIConnected();
        
        // Thread function
        static void* ResolutionThreadFunc(void *arg);
        
        // GLib callback functions
        static gboolean HeartbeatMsg(gpointer data);
        static gboolean SetResolutionHandler(gpointer data);
        static gboolean DumpEdidOnChecksumDiff(gpointer data);

    private:
        static DSController* _instance;
        
        // Thread synchronization
        pthread_t _resolutionThreadID;
        pthread_mutex_t _mutexLock;
        pthread_cond_t _mutexCond;
        
        // GLib main loop
        GMainLoop* _mainLoop;
        guint _hotplugEventSrc;
        
        // State variables
        int _tuneReady;
        int _initResolutionFlag;
        int _resolutionRetryCount;
        bool _hdcpAuthenticated;
        bool _ignoreEdid;
        dsDisplayEvent_t _displayEventStatus;
        int _easMode;  // IARM_Bus_Daemon_SysMode_t equivalent

        // State variables
        int _tuneReady;
        int _initResolutionFlag;
        int _resolutionRetryCount;
        bool _hdcpAuthenticated;
        bool _ignoreEdid;
        dsDisplayEvent_t _displayEventStatus;
        int _easMode;  // IARM_Bus_Daemon_SysMode_t equivalent
        
    private:
        // lock to guard all apis of DeviceSettings
        mutable Core::CriticalSection _apiLock;
        // lock to guard all notification from DeviceSettings to clients and also their callback register & unregister
        mutable Core::CriticalSection _callbackLock;
    };
} // namespace Plugin
} // namespace WPEFramework
