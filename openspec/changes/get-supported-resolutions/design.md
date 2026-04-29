## Context

`getSupportedResolutions` is an existing JSON-RPC method of the `org.rdk.DisplaySettings` Thunder plugin (callsign `org.rdk.DisplaySettings`). It bridges the WPEFramework middleware layer with the Device Settings (DS) HAL via `device::Host` to query video output port capabilities.

The current specification describes a single-sentence behavior with no distinction between display types and no documented response for the no-display case. Platform teams have encountered inconsistencies:

- Built-in panel ports always return a fixed list; external HDMI ports should reflect connected-display EDID data.
- When no external display is connected, some implementations return a default list while others return an empty list, leading to client-side confusion.

The fix is a specification clarification — no API surface change. The response schema (`supportedResolutions[]` + `success`) remains unchanged.

## Goals / Non-Goals

**Goals:**
- Clarify the data source: EDID for external displays, platform capabilities for built-in panels.
- Define the default `videoDisplay` port as `HDMI0`.
- Specify built-in vs. external display behavioral differences.
- Mandate an empty `supportedResolutions` array when no external display is connected.
- Require the list to be refreshed when a different display is connected (cache invalidation on connect/disconnect events).

**Non-Goals:**
- No changes to the JSON-RPC method signature, response schema, or callsign.
- No changes to any other `DisplaySettings` method.
- No new HAL APIs — existing DS HAL port capability queries are sufficient.
- Not addressing `getSupportedTvResolutions` or `getSupportedSettopResolutions`; those are separate methods.

## Decisions

### Decision 1 — Return empty list (not an error) when no external display is connected

**Choice**: Return `{ "supportedResolutions": [], "success": true }`.

**Rationale**: Returning an empty array is a safe, non-breaking sentinel that clients can already handle (they check array length). Returning an error code would be a breaking change for existing callers that assume a `200`-style success when no display is attached.

**Alternatives considered**:
- Return `["none"]` (consistent with `getSupportedTvResolutions`) — rejected because `"none"` is not a valid resolution string in the context of `getSupportedResolutions`; it would conflict with caller filtering logic.
- Return an error object — rejected because it breaks backward compatibility.

### Decision 2 — Refresh resolution list on display connect/disconnect events

**Choice**: The implementation SHALL invalidate any cached supported-resolution list and re-query the DS HAL when a `displayConnectionChanged` event is received.

**Rationale**: EDID data changes when a different TV or monitor is connected. A stale cache would cause clients to see outdated resolutions after a hot-plug event.

**Alternatives considered**:
- Always query HAL on every call (no cache) — acceptable but unnecessary overhead on frequent polling; event-driven invalidation is already used for `getCurrentResolution`.

### Decision 3 — Built-in display returns platform-defined list regardless of physical connection state

**Choice**: For ports identified as built-in panels, the HAL returns a fixed platform capability list. This list is always non-empty and does not depend on an active connection.

**Rationale**: Built-in panels are always present; EDID negotiation is not applicable. The DS HAL abstractly provides the capability list for such ports.

## Risks / Trade-offs

- **[Risk] Existing callers expecting a non-empty default list for HDMI0 when no display is connected** → Migrated: the spec now explicitly documents empty-list behavior; clients should check array length rather than assuming content.
- **[Risk] Platform HAL implementations diverging** → Per-platform DS HAL adaptations must enforce the empty-list contract; QA L2 tests should assert this case.
- **[Risk] YAML parse warning in `openspec/config.yaml`** → Cosmetic, does not impact CLI operation; tracked separately.
