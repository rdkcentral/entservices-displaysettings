# OpenSpec Coverage Report — entservices-displaysettings

**Generated**: 2026-04-28  
**Repository**: `entservices-displaysettings`  
**Specs scanned**: `openspec/specs/`  
**Code scanned**: `plugin/`, `helpers/`, `Tests/`

---

## Total Score: 79 / 100

| Category | Weight | Score | Max |
|----------|--------|-------|-----|
| Code to Spec Coverage | 40% | 33.8 | 40 |
| Architecture HLA Specification | 10% | 8 | 10 |
| External Interface Specification | 10% | 8 | 10 |
| Security Specification | 10% | 5 | 10 |
| Versioning & Compatibility | 10% | 7 | 10 |
| Conformance Testing & Validation | 10% | 7 | 10 |
| Performance Specification | 10% | 10 | 10 |
| **TOTAL** | **100%** | **78.8 ≈ 79** | **100** |

---

## 1. Code to Spec Coverage (33.8 / 40)

### Spec Inventory

| Spec File | Exists | Has Required Sections |
|-----------|--------|----------------------|
| `openspec/specs/displaysettings-spec.md` | ✅ | ✅ (Overview, Description, Requirements all present) |

**Spec existence score**: 1/1 specs exist → **10/10**

**Spec completeness score**: 1/1 specs have Overview + Description + Requirements → **5/5**

### Reference Coverage (Method Coverage)

| Metric | Value |
|--------|-------|
| Total code methods across codebase | 186 |
| Methods covered by `## Covered Code` in specs | 167 |
| Methods with `// Spec:` comments in code | 0 |
| Union (covered by either signal) | 167 |
| **Reference Coverage %** | **89.8%** |

#### Coverage by File

| File | Total Methods | Covered | Orphaned |
|------|--------------|---------|---------|
| `plugin/DisplaySettings.cpp` | 164 | 162 | 2 |
| `plugin/DisplaySettings.h` | 0 (declarations only) | 0 | 0 |
| `plugin/Module.cpp` | 1 | 1 | 0 |
| `plugin/Module.h` | 1 | 1 | 0 |
| `Tests/L2Tests/tests/DisplaySettings_L2Test.cpp` | 3 | 3 | 0 |
| `helpers/PluginInterfaceBuilder.h` | 1 | 0 | 1 |
| `helpers/UtilsString.h` | 1 | 0 | 1 |
| `helpers/WebSockets/Encryption/NoEncryption.h` | 1 | 0 | 1 |
| `helpers/WebSockets/Encryption/TlsEnabled.h` | 1 | 0 | 1 |
| `helpers/WebSockets/JsonRpc/Notification.cpp` | 2 | 0 | 2 |
| `helpers/WebSockets/JsonRpc/Request.cpp` | 5 | 0 | 5 |
| `helpers/WebSockets/JsonRpc/Response.cpp` | 6 | 0 | 6 |
| **Total** | **186** | **167** | **19** |

**Reference coverage score**: 89.8% → **18/20** (≈ 89.8% of 20 points)

#### Orphaned Methods (Not Covered by Any Spec)

**In `plugin/DisplaySettings.cpp`** (framework-injected — not true orphans):
- `Core::ToString` — Thunder framework template, not a plugin method
- `PluginHost::JSONRPC` — framework base class constructor call

**In `helpers/` (no spec exists for helper utilities)**:
- `helpers/PluginInterfaceBuilder.h` — plugin builder helper
- `helpers/UtilsString.h` — string utility
- `helpers/WebSockets/Encryption/NoEncryption.h` — WS encryption stub
- `helpers/WebSockets/Encryption/TlsEnabled.h` — WS TLS wrapper
- `helpers/WebSockets/JsonRpc/Notification.cpp` — 2 methods (WS JSON-RPC notification)
- `helpers/WebSockets/JsonRpc/Request.cpp` — 5 methods (WS JSON-RPC request)
- `helpers/WebSockets/JsonRpc/Response.cpp` — 6 methods (WS JSON-RPC response)

**Note**: The 2 orphaned methods in `DisplaySettings.cpp` are framework scaffolding, not plugin logic. The 17 orphaned methods in `helpers/` are shared utility/infrastructure code that lack dedicated specs.

**No-orphaned-code score**: 162/164 plugin methods covered → **4.6/5** (rounded to **5/5** given framework items are not plugin logic; helper utilities score **3.8/5**)

**Final no-orphaned-code score**: **4/5** (penalizing for absent helper specs)

### Supplementary: `// Spec:` Comments in Code

**Count**: 0 `// Spec:` comments found across all source files.

No inline traceability comments are used. All traceability is via the `## Covered Code` section in the spec — which is the preferred primary mechanism.

**Recommendation**: Consider adding `// Spec: displaysettings-spec` annotations to key methods in `plugin/DisplaySettings.cpp` for supplementary traceability.

### Code to Spec Coverage Summary

| Sub-criterion | Score | Max |
|--------------|-------|-----|
| Reference Coverage (89.8%) | 18 | 20 |
| Spec Existence (1/1 = 100%) | 10 | 10 |
| Spec Completeness (1/1 = 100%) | 5 | 5 |
| No Orphaned Code (167/186 = 89.8%) | 0.8 × 5 = **4** | 5 |
| **Subtotal** | **37** | **40** |

> Adjusted to **33.8/40** reflecting 89.8% reference coverage pro-rated across the 20-point weight.

---

## 2. Architecture HLA Specification (8 / 10)

| Sub-criterion | Score | Max | Notes |
|--------------|-------|-----|-------|
| Presence of HLA Spec | 3 | 3 | `## Architecture / Design` section present ✅ |
| Clarity of Architecture Diagrams | 3 | 3 | 3 ASCII diagrams: component map, ARC/eARC state machine, plugin diagram ✅ |
| Component/Module Mapping | 2 | 2 | DS HAL, HdmiCecSink, PowerManager, video/audio subsystems all mapped ✅ |
| Traceability to Code | 0 | 2 | Diagrams reference component names but no explicit `// Spec:` or code-level cross-references ❌ |

**Score: 8 / 10**

**Gap**: No code-level references (e.g. class names, file names) placed in the diagram narrative or linked to specific code modules for traceability.

---

## 3. External Interface Specification (8 / 10)

| Sub-criterion | Score | Max | Notes |
|--------------|-------|-----|-------|
| Presence of Interface Spec | 3 | 3 | `## External Interfaces` section present, 53 methods documented ✅ |
| Defined Inputs/Outputs | 3 | 3 | Parameter tables with name/type/required + response JSON for each method ✅ |
| Documentation Completeness | 1 | 2 | 53/82 (~65%) registered JSON-RPC methods individually documented with examples; remaining (~35%) grouped or referenced generically ⚠️ |
| Validation/Examples | 1 | 2 | 57 inline JSON response examples present; no end-to-end curl/client usage examples or cross-references to L2 test invocations ⚠️ |

**Score: 8 / 10**

**Gaps**:
- ~29 registered JSON-RPC methods not individually documented with dedicated parameter/response tables (grouped methods like `reset*`, `get/set` pairs share a single entry without distinct parameter tables).
- No link between API documentation and the L2 test `InvokeServiceMethod` calls for validation traceability.

---

## 4. Security Specification (5 / 10)

| Sub-criterion | Score | Max | Notes |
|--------------|-------|-----|-------|
| Presence of Security Spec | 3 | 3 | `## Security` section present ✅ |
| Threat Model/Analysis | 0 | 3 | No threat model or OWASP analysis documented ❌ |
| Security Requirements | 1 | 2 | Access control and path sanitization noted; no structured security requirements (e.g. MUST/SHALL) ⚠️ |
| Validation/Testing | 1 | 2 | No security-specific tests; implicitly covered by general L2 tests ⚠️ |

**Score: 5 / 10**

**Gaps**:
- No threat model (e.g. unauthenticated JSONRPC access, IPC spoofing, SCART/ARC input injection).
- Security requirements are prose observations, not structured SHALL statements.
- No security-specific test cases (e.g. fuzz testing of JSON input, boundary value testing of volume/gain ranges).

---

## 5. Versioning & Compatibility Specification (7 / 10)

| Sub-criterion | Score | Max | Notes |
|--------------|-------|-----|-------|
| Presence of Versioning Spec | 3 | 3 | `## Versioning & Compatibility` section present ✅ |
| Versioning Scheme Defined | 3 | 3 | API version 2.0.5 documented; `SERVICE_REGISTRATION` usage explained; v1 vs v2 method table present ✅ |
| Backward/Forward Compatibility | 1 | 2 | v1 backward compatibility documented; no forward compatibility or deprecation policy stated ⚠️ |
| Migration/Upgrade Path | 0 | 2 | No migration guide for clients upgrading from v1 to v2 methods ❌ |

**Score: 7 / 10**

**Gaps**:
- No deprecation policy for v1 methods.
- No guidance for clients migrating from `org.rdk.DisplaySettings.1` to `org.rdk.DisplaySettings.2` (which parameters change, which remain compatible).

---

## 6. Conformance Testing & Validation (7 / 10)

| Sub-criterion | Score | Max | Notes |
|--------------|-------|-----|-------|
| Presence of Conformance Tests | 3 | 3 | L2 tests present at `Tests/L2Tests/tests/DisplaySettings_L2Test.cpp` ✅ |
| Test Coverage | 2 | 3 | ~10 methods tested in `DisplaySettings_L2_MethodTest`; L1 stubs exist but no L1 test bodies found ⚠️ |
| Test Documentation | 1 | 2 | `## Conformance Testing & Validation` section references test fixture and scenarios; no documented steps to build/run tests ⚠️ |
| Validation Results | 1 | 2 | No tracked test results, CI badge, or pass/fail evidence in spec ⚠️ |

**Score: 7 / 10**

**Gaps**:
- L1Tests CMakeLists.txt exists but no L1 test source files found — these stubs need implementation.
- No instructions for building and running tests (cmake commands, required mocks, environment setup).
- No CI integration results or coverage report linked from the spec.

---

## 7. Performance Specification (10 / 10)

| Sub-criterion | Score | Max | Notes |
|--------------|-------|-----|-------|
| Presence of Performance Spec | 3 | 3 | `## Performance` section present ✅ |
| Defined Performance Metrics | 3 | 3 | Threading model, caching strategy, and locked-API usage documented; ARC timer intervals are named constants with values ✅ |
| Test Coverage for Performance | 2 | 2 | Caching behavior validated indirectly through L2 tests (cache hit/miss paths exercised) ✅ |
| Results & Validation | 2 | 2 | Timer constants confirmed against source code; caching invalidation logic verified in spec ✅ |

**Score: 10 / 10**

---

## Summary of Gaps and Recommendations

### Critical Gaps

| Priority | Gap | Affected Score |
|----------|-----|---------------|
| HIGH | No threat model or security analysis in Security section | Security: -5 |
| HIGH | No `// Spec:` traceability comments in source code | Code Coverage: -2 |
| MEDIUM | ~29 JSON-RPC methods lack individual parameter/response documentation | External Interfaces: -1 |
| MEDIUM | No migration guide for v1 → v2 API upgrade | Versioning: -2 |
| MEDIUM | L1 test stubs have no implementations | Conformance: -1 |
| LOW | No build/run instructions for tests | Conformance: -1 |
| LOW | Helper files (`helpers/WebSockets/`, `helpers/PluginInterfaceBuilder.h`) have no spec | Code Coverage: -0.2 |
| LOW | No code-level cross-references in architecture diagrams | Architecture: -2 |

### Recommendations

1. **Add a threat model** to the Security section — at minimum document: unauthenticated JSONRPC access via port 9998, privilege escalation via SCART/ARC control, input validation boundaries for volume/gain/delay parameters.

2. **Add `// Spec: displaysettings-spec` comments** to methods in `plugin/DisplaySettings.cpp` for supplementary inline traceability.

3. **Complete individual method documentation** for all 82 registered JSON-RPC methods in the External Interfaces section — ensure each `reset*`, `get*`, `set*` pair has its own parameter table.

4. **Add a v1 → v2 migration guide** (which methods changed, what parameter/response format differences to expect, when v1 will be deprecated).

5. **Implement L1 unit tests** — stubs exist in `Tests/L1Tests/` but no test source files were found.

6. **Add a spec for helper utilities** — `helpers/WebSockets/` is a non-trivial JSON-RPC WebSocket library; consider `openspec/specs/helpers-spec.md`.

7. **Add build and run instructions** for tests to the Conformance section (e.g. cmake commands, required environment variables, mock dependencies).

---

## Spec File Status

| Spec | Location | Status |
|------|----------|--------|
| `displaysettings-spec.md` | `openspec/specs/` | ✅ Exists, fully templated |

### Code Files Without a Spec

| File | Methods | Recommendation |
|------|---------|----------------|
| `helpers/WebSockets/JsonRpc/Notification.cpp` | 2 | Add to helpers spec |
| `helpers/WebSockets/JsonRpc/Request.cpp` | 5 | Add to helpers spec |
| `helpers/WebSockets/JsonRpc/Response.cpp` | 6 | Add to helpers spec |
| `helpers/WebSockets/Encryption/NoEncryption.h` | 1 | Add to helpers spec |
| `helpers/WebSockets/Encryption/TlsEnabled.h` | 1 | Add to helpers spec |
| `helpers/PluginInterfaceBuilder.h` | 1 | Add to helpers spec |
| `helpers/UtilsString.h` | 1 | Add to helpers spec |

---

## Change History

- [2026-04-28] - openspec-coverage - Initial coverage report generated.
