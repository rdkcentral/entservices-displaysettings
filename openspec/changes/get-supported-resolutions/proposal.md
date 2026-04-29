## Why

The current specification for `getSupportedResolutions` lacks clarity on the data source and behavioral differences between built-in and external displays, including the expected response when no external display is connected. These omissions create ambiguity in platform implementations and client-side error handling.

## What Changes

- Document that `getSupportedResolutions` returns resolutions sourced from the display's EDID (for external displays) or from platform capabilities (for built-in panels).
- Specify that the default `videoDisplay` port is `HDMI0` when the parameter is omitted.
- Clarify distinct behavior for built-in vs. external display ports.
- Define that the method SHALL return an empty list when no external display is connected.
- Specify that the supported resolution list SHALL be refreshed when a different display is connected.

## Capabilities

### New Capabilities
<!-- None — this change updates requirements for an existing capability. -->

### Modified Capabilities
- `displaysettings-spec`: Update requirements for `getSupportedResolutions` to clarify EDID/platform data source, default port (`HDMI0`), built-in vs. external display behavior, empty-list response when no display is connected, and list refresh on display reconnection.

## Impact

- `openspec/specs/displaysettings-spec.md` — `getSupportedResolutions` method entry updated with additional requirements.
- `plugin/DisplaySettings.cpp` / `plugin/DisplaySettings.h` — implementation may need to handle the no-display case (return empty list) and ensure the supported resolution cache is invalidated on display connect/disconnect events.
- No breaking API contract change; the response schema (`supportedResolutions` array + `success`) is unchanged.
