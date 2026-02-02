# DisplaySettings Plugin - Architecture Documentation

## Overview

The DisplaySettings plugin is a WPEFramework (Thunder) plugin that provides a unified interface for managing display and audio settings on RDK devices. It abstracts the underlying Device Settings (DS) HAL layer to provide JSON-RPC based APIs for controlling video output, audio output, display properties, and related capabilities.

## System Architecture

### Component Hierarchy

```
┌─────────────────────────────────────────────────────┐
│          Client Applications / UI                    │
│          (Web Apps, Native Apps)                     │
└────────────────┬────────────────────────────────────┘
                 │ JSON-RPC
                 │ (Thunder Communication)
┌────────────────▼────────────────────────────────────┐
│         WPEFramework (Thunder)                       │
│         Plugin Host & JSON-RPC Dispatcher            │
└────────────────┬────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────┐
│         DisplaySettings Plugin                       │
│  ┌──────────────────────────────────────────────┐  │
│  │  JSON-RPC Interface Layer                    │  │
│  │  - Request/Response Handling                 │  │
│  │  - Parameter Validation                      │  │
│  │  - Event Notifications                       │  │
│  └──────────────┬───────────────────────────────┘  │
│                 │                                    │
│  ┌──────────────▼───────────────────────────────┐  │
│  │  Business Logic Layer                        │  │
│  │  - Resolution Management                     │  │
│  │  - Audio Port Control                        │  │
│  │  - Display Connection Handling               │  │
│  │  - Zoom/Picture Mode Settings                │  │
│  └──────────────┬───────────────────────────────┘  │
│                 │                                    │
│  ┌──────────────▼───────────────────────────────┐  │
│  │  Helper Utilities                            │  │
│  │  - UtilsString, UtilsJsonRpc                 │  │
│  │  - UtilsSynchro (Thread Safety)              │  │
│  │  - tptimer (Timer Management)                │  │
│  └──────────────┬───────────────────────────────┘  │
└─────────────────┼────────────────────────────────────┘
                  │
┌─────────────────▼────────────────────────────────────┐
│  Platform Abstraction Layer                          │
│  ┌──────────────────┐  ┌─────────────────────────┐  │
│  │  DS HAL (Device  │  │  IARM Bus               │  │
│  │  Settings)       │  │  (Inter-process Comm)   │  │
│  │  - Video Output  │  │  - Event Broadcasting   │  │
│  │  - Audio Output  │  │  - Cross-plugin Comm    │  │
│  │  - Display Device│  │                         │  │
│  └──────────────────┘  └─────────────────────────┘  │
│  ┌──────────────────┐  ┌─────────────────────────┐  │
│  │  RFC (Remote     │  │  TR-181 API             │  │
│  │  Feature Control)│  │  (Configuration)        │  │
│  └──────────────────┘  └─────────────────────────┘  │
└──────────────────────────────────────────────────────┘
                  │
┌─────────────────▼────────────────────────────────────┐
│         Hardware Layer                               │
│  - Video Output Ports (HDMI, Component, etc.)        │
│  - Audio Output Ports (HDMI ARC, SPDIF, etc.)        │
│  - Display/TV Hardware                               │
└──────────────────────────────────────────────────────┘
```

## Core Components

### 1. Plugin Framework Integration

The DisplaySettings plugin inherits from multiple Thunder framework interfaces:
- **PluginHost::IPlugin**: Core plugin lifecycle management (Initialize, Deinitialize)
- **PluginHost::JSONRPC**: JSON-RPC method registration and dispatch
- **Exchange::IDeviceOptimizeStateActivator**: Device power state optimization
- **device::Host Event Interfaces**: Display, audio, video, and HDMI event callbacks

### 2. JSON-RPC Interface Layer

Provides RESTful-style JSON-RPC methods for:
- **Display Management**: Resolution control, supported resolutions, display connections
- **Audio Management**: Audio port control, sound modes, volume, MS12 profiles
- **Video Management**: Zoom settings, picture modes, HDCP status
- **Capabilities**: Query supported features, formats, and configurations

### 3. Device Settings (DS) HAL Integration

The plugin communicates with the DS HAL layer through:
- **Manager**: Initializes and manages the DS subsystem
- **VideoOutputPort**: Controls video output ports (HDMI, Component, Composite)
- **AudioOutputPort**: Manages audio output ports and settings
- **Host**: Device-level operations and event subscriptions
- **VideoDevice/Display**: Display device management and EDID handling

### 4. Event Management System

The plugin implements an event-driven architecture:
- **Hardware Events**: Display connection/disconnection, HDMI hot-plug
- **Audio Events**: Audio port changes, format changes, ARC events
- **Video Events**: Resolution changes, format changes
- **HDMI Events**: HDCP status, CEC events (via HdmiCecSink plugin)

Events are propagated through:
1. DS HAL → Plugin event callbacks
2. Plugin → IARM bus broadcast (for legacy components)
3. Plugin → JSON-RPC notifications (for Thunder clients)

## Data Flow

### Resolution Change Flow
```
Client Request → JSON-RPC → Validate Resolution → Check Display Support
    → DS HAL setResolution() → Hardware Apply → Event Notification
    → IARM Broadcast → JSON-RPC Event → Client Update
```

### Audio Port Management Flow
```
Client Request → JSON-RPC → Validate Audio Port → Get Port Instance
    → DS HAL setEnableAudioPort() → Update Port State → Event Notification
    → Update Internal Cache → JSON-RPC Response
```

## Dependencies

### External Libraries
- **WPEFramework**: Plugin hosting, JSON-RPC, and service infrastructure
- **DeviceSettings (DS)**: Platform abstraction for display/audio hardware
- **IARM Bus**: Inter-process communication for RDK components
- **RFC**: Remote feature control and configuration management
- **TR-181**: Data model for configuration parameters

### Helper Modules
- **UtilsSynchro.hpp**: Thread-safe operations and synchronization primitives
- **UtilsJsonRpc.h**: JSON-RPC utilities for request/response handling
- **UtilsString.h**: String manipulation and conversion utilities
- **UtilsisValidInt.h**: Input validation utilities
- **tptimer.h**: Timer management for delayed operations
- **PowerManagerInterface.h**: Interface for power state coordination

## Threading and Synchronization

The plugin uses thread-safe patterns:
- **Mutex Protection**: Critical sections protected with std::mutex
- **Condition Variables**: For event-driven waiting (std::condition_variable)
- **Synchro Utilities**: Helper classes for IARM bus synchronization (UtilsSynchro.hpp)
- **Callback Thread Safety**: DS HAL callbacks are marshalled to plugin context

## Configuration and Persistence

### Configuration Sources
1. **Plugin Configuration**: DisplaySettings.config (startup parameters)
2. **RFC Parameters**: Runtime feature flags (RFC_PWRMGR2, etc.)
3. **TR-181 Data Model**: Persistent settings storage
4. **Device Properties**: Platform-specific capabilities

### Persistent Settings
- Zoom settings stored in `/opt/persistent/rdkservices/zoomSettings.json`
- Audio/video preferences maintained across reboots
- Display resolution cached for fast retrieval

## Integration Points

### Power Management Integration
- Registers with PowerManager plugin for power state transitions
- Optimizes device state based on power mode (ON, STANDBY, LIGHT_SLEEP, DEEP_SLEEP)
- Coordinates with thermal management for temperature-based throttling

### HdmiCecSink Integration
- Monitors CEC events for ARC (Audio Return Channel) control
- Handles system audio mode changes
- Processes short audio descriptors (SAD) for format capability

### System Mode Integration
- Responds to device mode changes (NORMAL, WAREHOUSE, EAS)
- Adjusts behavior based on operational mode

## Error Handling

The plugin implements comprehensive error handling:
- **Input Validation**: All JSON-RPC parameters validated before processing
- **DS HAL Error Mapping**: DS error codes translated to JSON-RPC error responses
- **Exception Safety**: C++ exceptions caught and converted to error codes
- **Logging**: Comprehensive logging using Thunder's tracing infrastructure

## Performance Considerations

- **Caching**: Resolution and capability data cached to minimize HAL calls
- **Lazy Initialization**: DS subsystem initialized only when needed
- **Event Coalescing**: Rapid events debounced to prevent notification storms
- **Asynchronous Operations**: Long-running operations handled asynchronously

## Security

- **Access Control**: Thunder's security token mechanism (can be disabled via DISABLE_SECURITY_TOKEN)
- **Input Sanitization**: All external inputs validated and sanitized
- **Privilege Separation**: Runs with minimal required privileges

This architecture enables the DisplaySettings plugin to provide a robust, performant, and maintainable interface for display and audio management on RDK devices.
