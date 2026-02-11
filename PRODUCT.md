# DisplaySettings Plugin - Product Documentation

## Product Overview

The DisplaySettings plugin is a comprehensive display and audio management solution for RDK-powered devices. It provides a unified JSON-RPC interface for controlling video output, audio output, display capabilities, and user preferences across various hardware platforms including set-top boxes, smart TVs, and streaming devices.

## Key Features

### Display Management
- **Resolution Control**: Set and query display resolutions (480i, 720p, 1080i, 1080p, 4K, etc.)
- **Display Connection Detection**: Real-time monitoring of display connectivity (HDMI hot-plug)
- **EDID Parsing**: Automatic capability detection from connected displays
- **Supported Formats**: Query TV-supported resolutions and formats
- **Display Capabilities**: HDR support detection (HDR10, Dolby Vision, HLG)
- **HDCP Status**: Query and monitor HDCP version and protection status

### Audio Management
- **Multi-Port Support**: Manage HDMI, HDMI ARC, SPDIF, Headphone, and Speaker outputs
- **Audio Format Control**: Support for Stereo, Surround, Dolby Digital, Dolby Atmos, DTS
- **Sound Modes**: Configurable audio profiles (Movie, Music, Sports, Game, Night mode)
- **Volume Control**: Per-port volume management with mute support
- **MS12 Audio Profiles**: Advanced audio processing configurations
- **Audio Delay/Offset**: Synchronization controls for lip-sync correction
- **Audio Level Control**: Dialog enhancement, bass/treble adjustment, surround virtualizer

### Video Settings
- **Picture Modes**: Standard, Dynamic, Movie, Sports, Energy Saving, Vivid
- **Zoom Control**: Normal, Full, None, Direct, Auto, LBX 16x9, LBX 14x9, CCO, PillarBox
- **Aspect Ratio**: Automatic and manual aspect ratio management
- **Color Settings**: Brightness, contrast, saturation, hue, sharpness adjustments
- **Advanced Video**: Dynamic backlight, local dimming, color temperature

### Advanced Capabilities
- **Multi-Stream Decode (MS12)**: Advanced audio processing and mixing
- **Dolby Atmos & DTS:X**: Immersive audio format support
- **HDMI ARC/eARC**: Audio Return Channel for soundbar/AVR integration
- **CEC Integration**: Consumer Electronics Control for device synchronization
- **RFC Integration**: Remote feature control for A/B testing and gradual rollouts
- **Persistent Settings**: User preferences saved across reboots

## Use Cases

### Home Entertainment Systems
**Scenario**: User connects a 4K TV and wants optimal picture and sound quality
- Plugin auto-detects TV capabilities via EDID
- Recommends best resolution (4K@60Hz if supported)
- Enables HDR when both TV and content support it
- Configures HDMI ARC for soundbar audio output
- Applies user's preferred picture mode (e.g., "Movie" mode)

### Multi-Room Audio
**Scenario**: User has speakers in different rooms with varying capabilities
- Manage multiple audio output ports simultaneously
- Configure independent volume levels per room
- Support different audio formats per output (e.g., 5.1 for living room, stereo for bedroom)
- Synchronize audio across outputs or allow independent control

### Content Provider Integration
**Scenario**: Streaming service needs to adapt to device capabilities
- Query supported video resolutions and refresh rates
- Detect HDR/Dolby Vision support for content selection
- Check audio capabilities (Atmos, DTS:X) for stream quality
- Optimize bandwidth based on display capabilities

### Accessibility Features
**Scenario**: User with hearing impairment needs audio enhancement
- Enable dialog enhancement to boost speech clarity
- Configure audio description track output
- Adjust audio delay for hearing aid synchronization
- Enable visual indicators for audio events

### Energy Efficiency
**Scenario**: Reduce power consumption during extended usage
- Switch to "Energy Saving" picture mode
- Reduce backlight intensity
- Coordinate with PowerManager for thermal management
- Adjust settings based on ambient light conditions

## API Capabilities

### JSON-RPC Methods (Examples)

#### Display Methods
- `getSupportedResolutions`: List all device-supported resolutions
- `getSupportedTvResolutions`: Query TV-supported resolutions via EDID
- `setCurrentResolution`: Change active display resolution
- `getCurrentResolution`: Get current active resolution
- `getConnectedVideoDisplays`: List connected displays
- `getDisplayDetails`: Get detailed display information (EDID, HDR support)

#### Audio Methods
- `getSupportedAudioPorts`: List available audio outputs
- `getConnectedAudioPorts`: Query active audio connections
- `setEnableAudioPort`: Enable/disable specific audio port
- `getSoundMode`: Get current audio profile
- `setSoundMode`: Apply audio profile (Movie, Music, etc.)
- `setVolumeLevel`: Set output volume (0-100)
- `setMuted`: Mute/unmute audio output

#### Video Methods
- `setZoomSetting`: Configure zoom/aspect ratio
- `getZoomSetting`: Query current zoom setting
- `setVideoFormat`: Set color format (SDR, HDR10, Dolby Vision)
- `getVideoFormat`: Query current video format

#### Capability Methods
- `getHdmiHDRSupport`: Check HDR capabilities
- `getTvHDRSupport`: Query TV's HDR support
- `getSettopHDRSupport`: Query STB's HDR capabilities
- `getSupportedMS12AudioProfiles`: List MS12 audio configurations

### Event Notifications

Clients can subscribe to real-time events:
- `connectedVideoDisplaysUpdated`: Display connection changes
- `resolutionChanged`: Active resolution changed
- `audioPortStateChanged`: Audio port enabled/disabled
- `audioFormatChanged`: Audio format/codec changed
- `hdcpStatusChanged`: HDCP protection status changed
- `videoFormatChanged`: Video format changed (SDR/HDR)

## Integration Benefits

### For OEMs/Device Manufacturers
- **Unified API**: Single interface across diverse hardware platforms
- **Hardware Abstraction**: Isolates application logic from HAL complexity
- **Plug-and-Play**: Automatic capability detection reduces configuration effort
- **Compliance**: Built-in support for HDCP, CEC, and industry standards

### For Application Developers
- **Simple Integration**: Well-documented JSON-RPC interface
- **Event-Driven**: Real-time notifications for responsive UIs
- **Capability Discovery**: Dynamic adaptation to device features
- **Cross-Platform**: Same API across set-tops, TVs, and streaming devices

### For Content Providers
- **Smart Adaptation**: Query capabilities to optimize content delivery
- **Quality Optimization**: Select best streams for device capabilities
- **Format Support**: Ensure compatibility (HDR, Atmos, 4K, etc.)
- **Bandwidth Efficiency**: Avoid sending unsupported formats

### For System Integrators
- **Modular Design**: Integrates seamlessly with other RDK services
- **Power Coordination**: Works with PowerManager for energy efficiency
- **CEC Coordination**: Integrates with HdmiCecSink for device control
- **RFC Support**: Enable/disable features remotely for A/B testing

## Performance Characteristics

### Response Time
- **Settings Changes**: < 100ms for most operations (volume, mode changes)
- **Resolution Changes**: 1-3 seconds (includes display negotiation)
- **Capability Queries**: < 50ms (cached data)

### Reliability
- **Uptime**: Designed for 24/7 operation
- **Error Recovery**: Graceful handling of hardware failures
- **Thread Safety**: Multi-threaded access protected
- **Resource Management**: Efficient memory and CPU usage

### Scalability
- **Concurrent Clients**: Supports multiple simultaneous JSON-RPC clients
- **Event Subscribers**: Handles multiple event notification subscribers
- **Multi-Port Management**: Manages multiple audio/video outputs concurrently

## Platform Support

### Supported Platforms
- Broadcom-based set-top boxes
- Amlogic-based devices
- Realtek-based platforms
- Generic Linux platforms (with appropriate DS HAL)

### Video Standards
- HDMI 1.4, 2.0, 2.1
- HDCP 1.4, 2.2, 2.3
- Component video (YPbPr)
- Composite video (CVBS)

### Audio Standards
- PCM (Stereo, Multi-channel)
- Dolby Digital (AC-3)
- Dolby Digital Plus (E-AC-3)
- Dolby Atmos
- DTS
- DTS:X
- AAC

### HDR Formats
- HDR10
- HDR10+
- Dolby Vision
- HLG (Hybrid Log-Gamma)

## Configuration and Customization

### Plugin Configuration
The plugin supports various compile-time and runtime configurations:
- **PLUGIN_DISPLAYSETTINGS**: Enable/disable the plugin
- **PLUGIN_TELEMETRY**: Enable telemetry logging
- **PLUGIN_CONTINUEWATCHING**: Enable watch history integration
- **DS_FOUND**: Device Settings HAL availability
- **RFC parameters**: Runtime feature toggles

### Customization Points
- **Picture Mode Presets**: OEMs can define custom picture modes
- **Sound Mode Profiles**: Customizable audio processing profiles
- **Default Settings**: Factory default preferences
- **Branding**: UI strings and labels can be customized

## Deployment and Testing

### Build Integration
```bash
# BitBake build
bitbake thunder-plugins

# CMake build
cmake -DPLUGIN_DISPLAYSETTINGS=ON ...
```

### Testing Support
- **L2 Test Suite**: Functional end-to-end tests
- **Mock HAL Support**: Testing without physical hardware
- **JSON-RPC Test Scripts**: Example curl commands for manual testing

### Monitoring and Debugging
- **Thunder Trace Logging**: Comprehensive debug logging
- **Telemetry Integration**: Operational metrics collection
- **Event Monitoring**: Real-time event stream for debugging

This DisplaySettings plugin provides a production-ready, feature-rich solution for display and audio management, designed to meet the demanding requirements of modern entertainment devices.
