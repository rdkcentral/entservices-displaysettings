## 1. Specification Update

- [ ] 1.1 Update `openspec/specs/displaysettings-spec.md` — expand the `getSupportedResolutions` entry to document EDID-based data source for external ports, platform-defined list for built-in panels, default port `HDMI0`, empty-list response when no display is connected, and cache refresh on display reconnection.

## 2. Implementation — getSupportedResolutions behavior

- [ ] 2.1 In `plugin/DisplaySettings.cpp` → `getSupportedResolutions`: change the default `videoDisplay` fallback from `device::Host::getInstance().getDefaultVideoPortName()` to the literal `"HDMI0"` (per spec requirement REQ for default port).
- [ ] 2.2 Add a display-connected check for external ports: if the video output port is not a built-in panel and no display is connected (`vPort.isDisplayConnected()` returns false), set `supportedResolutions` to an empty vector and skip the HAL resolution query.
- [ ] 2.3 Verify that `getSupportedResolutions` relies on `device::VideoOutputPortConfig` (platform list for built-in) vs. EDID-sourced data for external ports; update the query path if the DS HAL provides a separate EDID-derived resolution list for hot-plug ports.
- [ ] 2.4 Ensure the supported resolution list for external ports is invalidated when `displayConnectionChanged` event fires (analogous to how `isResCacheUpdated` is used for current resolution cache); add cache invalidation logic if absent.

## 3. Tests — L2 Test Cases

- [ ] 3.1 Add L2 test `getSupportedResolutions_ExternalDisplayConnected`: mock an external HDMI0 port with a connected display; call `getSupportedResolutions`; verify `supportedResolutions` is non-empty and `success` is `true`.
- [ ] 3.2 Add L2 test `getSupportedResolutions_NoDisplayConnected`: mock an external HDMI0 port with no connected display (`isDisplayConnected` returns false); call `getSupportedResolutions`; verify `supportedResolutions` is an empty array and `success` is `true`.
- [ ] 3.3 Add L2 test `getSupportedResolutions_DefaultsToHDMI0`: call `getSupportedResolutions` without a `videoDisplay` parameter; verify the method queries the `HDMI0` port.
- [ ] 3.4 Add L2 test `getSupportedResolutions_BuiltInDisplay`: mock a built-in panel port; call `getSupportedResolutions` for that port; verify the platform capability list is returned regardless of connection state.
- [ ] 3.5 Add L2 test `getSupportedResolutions_CacheRefreshOnReconnect`: simulate `displayConnectionChanged` event followed by a call to `getSupportedResolutions`; verify the response reflects the new display's capability list (not the stale cached one).

## 4. Spec Coverage

- [ ] 4.1 Run `openspec coverage` and confirm all new `getSupportedResolutions` requirements map to L2 test scenarios; update `openspec/specs/spec_coverage.md` if needed.
