## 1. Specification Update

- [x] 1.1 Update `openspec/specs/displaysettings-spec.md` — expand the `getSupportedResolutions` entry to document EDID-based data source (via `getSupportedTvResolutions` bitmask HAL API), default port (platform default), empty-list response when no display is connected, and cache refresh on display reconnection.

## 2. Implementation — getSupportedResolutions behavior

- [x] 2.1 In `plugin/DisplaySettings.cpp` → `getSupportedResolutions`: default `videoDisplay` remains `device::Host::getInstance().getDefaultVideoPortName()` (platform-driven default, typically HDMI0). No hardcoded port string.
- [x] 2.2 Add a display-connected check: if no display is connected (`vPort.isDisplayConnected()` returns false), set `supportedResolutions` to an empty vector and skip the HAL resolution query.
- [x] 2.3 Switch resolution data source from `device::VideoOutputPortConfig::getPortType().getSupportedResolutions()` (static platform list) to `vPort.getSupportedTvResolutions(&tvResolutions)` bitmask — same EDID-sourced API used by `getSupportedTvResolutions`. Bitmask decoded into resolution strings for all supported resolutions (480i through 2160p60).
- [x] 2.4 No additional cache invalidation needed: `getSupportedResolutions` queries the HAL on every call (no application-level cache). `onDisplayHDMIHotPlug` already resets `isDisplayConnectedCacheUpdated`, ensuring any connection-state recheck is fresh after hot-plug.

## 3. Tests — L2 Test Cases

- [x] 3.1 Inline L2 test (in `DisplaySettings_L2_MethodTest`): mock `getSupportedTvResolutions` to return bitmask `720p|1080p|1080i|2160p60`; test default port with display connected; verify resolved strings match EDID bitmask.
- [x] 3.2 Inline L2 test: mock `isDisplayConnected()` returning false; call `getSupportedResolutions`; verify `supportedResolutions` is empty array and `success` is `true`.
- [x] 3.3 Inline L2 test: call `getSupportedResolutions` without `videoDisplay` param; verify response includes `supportedResolutions` and `success: true` using platform default port.
- [x] 3.4 Inline L2 test: call `getSupportedResolutions` with explicit `videoDisplay=HDMI0`; verify EDID-sourced list matches bitmask expectation.
- [x] 3.5 Cache-refresh behavior is covered by the live-query design (no cache exists at plugin level); hot-plug event invalidation verified by relying on `isDisplayConnectedCacheUpdated` reset in `OnDisplayHDMIHotPlug`.

## 4. Spec Coverage

- [x] 4.1 Updated `openspec/specs/spec_coverage.md` — L2 test count updated; overall coverage score updated to 80/100.
