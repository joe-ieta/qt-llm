# Identity Architecture

## Status

Accepted architectural baseline for future implementation work.

This document defines the repository-wide identity model for `qt-llm`.
It is intentionally written before code migration so that later work follows a fixed target instead of redesigning IDs during implementation.

## Why This Exists

The current codebase uses many direct `QUuid::createUuid()` calls across conversation state, tool tracing, ToolStudio, and agent workflows.

That approach gives uniqueness, but it does not give:

- readable type information
- stable repository-wide naming rules
- string-sortable chronological order
- trace-friendly operator experience
- a single implementation point for ID generation

For `toolsinside` and runtime observability in particular, raw GUIDs reduce the value of logs, SQLite inspection, UI trace browsing, and cross-module debugging.

## Goals

- Replace scattered GUID generation with one repository-wide ID design.
- Make IDs readable in logs, SQLite tables, and UI trace surfaces.
- Keep chronological ordering visible from the ID itself where practical.
- Enforce type-specific prefixes so humans can identify object classes quickly.
- Allow re-initialization of runtime SQLite schemas and runtime data layout to match the new design.
- Preserve existing public library interfaces exposed by `qtllm`.

## Non-Goals

- Backward compatibility for old persisted runtime data.
- In-place migration of legacy SQLite rows, JSON snapshots, or artifacts.
- Breaking or renaming public `qtllm` APIs currently used by downstream applications.
- Replacing externally supplied IDs from providers or protocols when those IDs are part of external contracts.

## Hard Constraints

## 1. Public API Stability

`qtllm` is a reusable base library and is already consumed externally.

Therefore:

- Existing public methods, signals, and externally visible types must remain source-compatible unless a separate explicit deprecation decision is made.
- Existing public ID-bearing interfaces should continue to use `QString` as their transport type.
- Internal ID redesign must happen behind existing API boundaries.
- Any new typed wrappers or helper classes must be additive, not mandatory for current callers.

Examples:

- `ConversationClient::uid() const` stays available.
- `ConversationClient::activeSessionId() const` stays available.
- `ToolExecutionContext` fields remain `QString` unless additive wrappers are introduced internally.

## 2. No Direct GUID Generation in Business Logic

After the migration starts, new code must not call `QUuid::createUuid()` directly for repository-managed identities.

Instead, all internally owned IDs must be created by a shared identity module under `src/qtllm/`.

Allowed exceptions:

- temporary non-domain identifiers such as local Qt connection names that never leave a narrow implementation scope
- compatibility glue where a third-party API requires a UUID specifically
- test code that is explicitly testing UUID handling

## 3. Runtime Data Can Be Recreated

For this initiative, persisted runtime state is not a compatibility boundary.

The following may be redesigned and re-initialized:

- `toolsinside` SQLite schema
- runtime artifact path layout
- runtime JSON snapshots that are internal-only
- ToolStudio internal workspace persistence if needed

Any such reset must still be documented before implementation.

## Proposed Identity Model

## Shared Format

All repository-owned IDs should follow:

```text
<prefix>_<body>
```

Where:

- `<prefix>` is a short lowercase type code
- `<body>` is a compact, lexicographically sortable encoded value

Recommended body design:

- time-ordered numeric payload
- custom epoch
- node/process component
- per-timestamp sequence
- Crockford Base32 encoding

This is intentionally aligned with the proven shape used in `CoReader/agent-runtime/src/runtime/identity/compact_id.*`.

## Required Properties

The identity generator must provide:

- global uniqueness within practical runtime assumptions
- monotonic ordering within a process
- stable string validation
- prefix-based type identification
- ability to decode order value for diagnostics and testing

## Canonical Prefixes

The exact prefix table should be implemented centrally and treated as architecture, not module-local preference.

Initial baseline:

- `cli` : conversation client
- `ses` : conversation session
- `trc` : trace
- `req` : LLM request
- `spn` : trace span
- `evt` : trace event
- `tcl` : tool call
- `art` : artifact
- `lnk` : support link
- `wsp` : ToolStudio workspace
- `nod` : ToolStudio category node
- `plc` : ToolStudio placement
- `pkg` : ToolStudio import/export package
- `tsk` : business task
- `que` : queue item or queue instance

Rules:

- Prefixes are repository-global and must not be redefined per module.
- New prefixes require updating this document and the central identity implementation.
- Prefixes should stay short, mnemonic, and lowercase ASCII.

## Ownership Rules

## Repository-Owned IDs

These IDs are generated by `qtllm` and must follow the new compact format:

- client IDs
- session IDs
- trace IDs
- request IDs
- span IDs
- event IDs
- tool call IDs when the ID is internally created
- artifact IDs
- support link IDs
- ToolStudio workspace, node, placement, and package IDs
- agent task and queue IDs

## Externally Originated IDs

These IDs may originate outside `qtllm` and should not be rewritten blindly:

- provider protocol IDs
- MCP server-side call IDs
- externally imported business IDs

When an external ID needs internal persistence, prefer one of these patterns:

- keep the external ID in a dedicated field such as `externalCallId`
- generate a repository-owned internal ID separately
- document the mapping explicitly in runtime records

## Interface Policy

The repository must distinguish between:

- interface shape
- internal identity semantics

The interface shape remains stable:

- callers still pass and receive `QString`
- current function names stay intact
- current signal signatures stay intact

The semantics behind those `QString` values change:

- newly created IDs follow the compact prefix-based format
- validation and formatting become centralized
- internal storage and trace rendering can rely on type prefixes

## Runtime and SQLite Implications

The new identity model is allowed to drive schema cleanup instead of being constrained by existing tables.

### `toolsinside`

Planned direction:

- recreate SQLite schema for `ti_traces`, `ti_spans`, `ti_events`, `ti_tool_calls`, `ti_artifacts`, and `ti_support_links`
- keep ID columns as `TEXT`
- rename or add columns only when it materially improves clarity
- stop generating hidden random IDs inside repositories when a higher layer should own identity

Preferred rule:

- recorder or orchestration layer creates semantic IDs
- repository persists them
- repository should not invent domain IDs as a fallback except in tightly scoped internal cases

### Artifact and Path Layout

Artifact directory structure may be redesigned to align with the new trace and artifact identity semantics.

The new layout should optimize:

- inspectability by humans
- grouping by trace/session/client as needed
- safe deletion and archive operations

## Central Module Shape

Implementation should add one shared module under `src/qtllm/`, for example:

```text
src/qtllm/identity/
  compactid.h
  compactid.cpp
```

Minimum API:

```cpp
enum class IdKind;

QString generateId(IdKind kind);
QString generateIdWithPrefix(QStringView prefix);
bool isValidId(QStringView value);
bool hasIdPrefix(QStringView value, QStringView prefix);
quint64 decodeIdOrder(QStringView value, bool *ok = nullptr);
```

Optional additive helpers:

- `prefixForKind(IdKind kind)`
- `isLegacyUuid(QStringView value)` for transitional diagnostics during rollout

## Phased Work Plan

The migration must happen in bounded phases.

## Phase 0: Documentation Lock

Done by this document set.

Exit criteria:

- repository-level constraints are written
- ID prefix table is fixed
- public API stability rule is explicit

## Phase 1: Central Identity Module

Implement the shared generator and validation helpers.

Exit criteria:

- one canonical implementation exists
- unit tests cover uniqueness, ordering, validation, and prefix mapping

## Phase 2: Observability First

Migrate the highest-value trace surfaces first:

- `ToolEnabledChatEntry`
- `QtLLMClient`
- `ToolsInsideTraceRecorder`
- `ToolsInsideRepository`
- `ToolsInsideArtifactStore`
- `toolsinside` browser-facing data assumptions

Exit criteria:

- trace, request, span, tool call, artifact, and event IDs all use compact prefixed IDs
- `toolsinside` SQLite can be recreated from scratch under the new scheme

## Phase 3: Conversation Domain

Migrate:

- `ConversationClientFactory`
- `ConversationClient`
- related storage

Exit criteria:

- client and session IDs use the central scheme
- public APIs remain unchanged

## Phase 4: ToolStudio and Agents

Migrate:

- ToolStudio workspace/node/placement/package IDs
- PDF translator task and queue IDs
- any remaining repository-owned runtime IDs

Exit criteria:

- direct GUID generation is eliminated from domain code

## Phase 5: Guardrails

Add enforcement:

- tests for prefix rules
- grep-based CI check or review rule against new direct `QUuid::createUuid()` domain usage
- documentation references in developer guide

Exit criteria:

- the architecture is self-reinforcing

## Review Checklist For Future Changes

Any future ID-related change should answer:

1. Is this ID repository-owned or external?
2. If repository-owned, why is it not using the central identity module?
3. Does the change preserve existing public `qtllm` API signatures?
4. Does the change introduce a new prefix, and if so has this document been updated?
5. Does the change alter runtime persistence or SQLite schema, and if so is the reset explicitly documented?

## Decision Summary

The repository will move from scattered GUID generation to a centralized compact prefixed identity system.

This migration is allowed to reset internal runtime persistence, but it must not break the current public `qtllm` interface surface.
