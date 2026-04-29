## Why

The current specification for `getSupportedResolutions` lacks clarity on the data source and the expected response when no display is connected. These omissions create ambiguity in platform implementations and client-side error handling.

## What Changes

- Document that `getSupportedResolutions` returns resolutions sourced from the connected display's EDID data (via the `getSupportedTvResolutions` HAL API) for any connected port.
- Specify that the default `videoDisplay` port is the platform default port (as returned by `getDefaultVideoPortName()`, typically `HDMI0`) when the parameter is omitted.
- Define that the method SHALL return an empty list when no display is connected on the requested port.
- Specify that the supported resolution list SHALL be refreshed when a different display is connected.

## Capabilities

### New Capabilities
<!-- None — this change updates requirements for an existing capability. -->

### Modified Capabilities
- `displaysettings-spec`: Update requirements for `getSupportedResolutions` to clarify EDID-based data source (via `getSupportedTvResolutions` HAL bitmask), default port (platform default), empty-list response when no display is connected, and list refresh on display reconnection.

## Impact

- `openspec/specs/displaysettings-spec.md` — `getSupportedResolutions` method entry updated with additional requirements.
- `plugin/DisplaySettings.cpp` — resolution query switched from `VideoOutputPortConfig::getPortType().getSupportedResolutions()` to `vPort.getSupportedTvResolutions()` bitmask; `isDisplayConnected()` guard added to return empty array when no display is connected.
- No breaking API contract change; the response schema (`supportedResolutions` array + `success`) is unchanged.
