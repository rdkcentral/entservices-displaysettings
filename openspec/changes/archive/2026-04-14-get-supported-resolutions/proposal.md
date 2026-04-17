## Why

DisplaySettings plugin needs to provide applications with the ability to query supported display resolutions to ensure optimal video output configuration. This enables applications to adapt their content delivery based on the capabilities of the connected display device, whether built-in or external (HDMI).

## What Changes

- Add new JSON-RPC method `getSupportedResolutions` to DisplaySettings plugin
- Support querying resolutions for different video display ports (HDMI0 by default)
- Return resolution list from platform capabilities (which may be influenced by display capabilities)
- Handle dynamic updates when external displays are connected/disconnected

## Capabilities

### New Capabilities

- `supported-resolutions-query`: Ability to retrieve the list of supported display resolutions from platform capabilities (via device layer abstraction), with support for multiple video display ports

### Modified Capabilities

<!-- No existing capabilities are being modified -->

## Impact

- **API**: New JSON-RPC method added to DisplaySettings plugin interface
- **Code**: Implementation in DisplaySettings plugin (DisplaySettings.cpp/h)
- **Dependencies**: Relies on Display Manager (DS) HAL for platform capability queries through device layer abstraction
- **Backward Compatibility**: Non-breaking change - adds new functionality without modifying existing APIs
