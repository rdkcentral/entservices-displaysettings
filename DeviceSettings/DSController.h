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
#include <functional>
#include <pthread.h>
#include <cstdlib>  // for NULL

#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>

// IARM includes for event handling
#include "iarmUtil.h"

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

#include "DeviceSettingsImplementation.h"
#include "DSPwrEventListener.h"
#include <interfaces/IDeviceSettingsVideoPort.h>
#include <interfaces/IDeviceSettingsAudio.h>
#include <interfaces/IDeviceSettingsDisplay.h>

#include "dsTypes.h"
#include "dsVideoPort.h"
#include "dsDisplay.h"
#include "dsAudio.h"

#include "DeviceSettingsTypes.h"

typedef struct _GMainLoop GMainLoop;
typedef int gboolean;
typedef void* gpointer;
typedef unsigned int guint;

namespace WPEFramework {
namespace Plugin {
    class DeviceSettingsImp;
    
    class DSController : public Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification {
    public:
        DSController(DeviceSettingsImp* deviceSettingsInstance);
        ~DSController();

        static DSController* Create(DeviceSettingsImp* deviceSettingsInstance);
        static DSController* instance(DSController* DSController = nullptr);

        DSController(const DSController&)            = delete;
        DSController& operator=(const DSController&) = delete;

        // Build QueryInterface implementation for Core::IUnknown
        BEGIN_INTERFACE_MAP(DSController)
            INTERFACE_ENTRY(Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification)
        END_INTERFACE_MAP

        // Implement Core::IUnknown methods
        uint32_t AddRef() const override {
            return Core::InterlockedIncrement(m_refCount);
        }
        
        uint32_t Release() const override {
            uint32_t l_Ref = Core::InterlockedDecrement(m_refCount);
            if (l_Ref == 0) {
                delete this;
            }
            return (l_Ref);
        }

    public:
        void InitializeIARM();
        uint32_t Start();
        uint32_t Stop();
        void Loop();

        void Init();
        void Deinit();

        void InitializeDeviceSettingsComponents();
        void DeinitializeDeviceSettingsComponents();
        void InitializePowerEventListener(PluginHost::IShell* service);
        void DeinitializePowerEventListener();

        int getEASMode() const { return _easMode; }
        
        void OnDisplayRxSense(const DisplayEvent displayEvent);
        void OnDisplayHDCPStatus();
        void OnDisplayHDMIHotPlug(const DisplayEvent displayEvent);
        
    private:
        void InitializeResolutionThread();
        void SetVideoPortResolution();
        void SetResolution(int32_t handle, dsVideoPortType_t portType);
        void SetAudioMode();
        void SetEASAudioMode();
        void SetBackgroundColor(dsVideoBackgroundColor_t color);
        void DumpHdmiEdidInfo(dsDisplayEDID_t* pedidData);
        void ScheduleEdidDump();
        
        void EventHandler(const char *owner, int eventId, void *data, size_t len);
        void SysModeChange(void *arg);
        
        int32_t GetVideoPortHandle(dsVideoPortType_t port);
        bool IsHDMIConnected();
        bool isComponentPortPresent();
        bool dsGetHDMIDDCLineStatus();
        
        static void setupPlatformConfig();
        static bool isEUPlatform();
        static bool getSecondaryResolution(char* res, char *secRes);
        static void parseResolution(const char* pResn, char* bResn);
        static void getFallBackResolution(char* Resn, char *fbResn, int flag);
        static bool isResolutionSupported(dsDisplayEDID_t *edidData, int numResolutions, 
                                         int pNumResolutions, char *Resn, int* index);
        
        static void* ResolutionThreadFunc(void *arg);
        
        static gboolean HeartbeatMsg(gpointer data);
        static gboolean SetResolutionHandler(gpointer data);
        static gboolean DumpEdidOnChecksumDiff(gpointer data);
        
        static void _EventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
        static IARM_Result_t _SysModeChange(void *arg);

    private:
        static DSController* _instance;
        
        // Injected DeviceSettings instance for dependency injection
        DeviceSettingsImp* _deviceSettingsInstance;
        
        DeviceSettingsImp* _deviceSettings;
        DSPwrEventListener* _pwrEventListener;
        
        static pthread_t _resolutionThreadID;
        static pthread_mutex_t _mutexLock;
        static pthread_cond_t _mutexCond;
        
        GMainLoop* _mainLoop;
        static guint _hotplugEventSrc;
        
        static volatile bool _dsMgr_thread_exit_flag;
        
        static int _tuneReady;
        static int _initResolutionFlag;
        static int _resolutionRetryCount;
        static bool _hdcpAuthenticated;
        static bool _ignoreEdid;
        static dsDisplayEvent_t _displayEventStatus;
        
        int _easMode;
        
        static bool IsEUPlatform;
        static char fallBackResolutionList[6][64];
        
    private:
        mutable Core::CriticalSection _apiLock;
        mutable Core::CriticalSection _callbackLock;
        
        // Reference counting for Core::IUnknown
        mutable uint32_t m_refCount;
    };
} // namespace Plugin
} // namespace WPEFramework
