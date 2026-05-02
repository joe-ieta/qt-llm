# CODING_GUIDELINES.md

## Style baseline

This repository follows practical Qt/C++ style with emphasis on clarity.

## General conventions

- Use include guards or `#pragma once` consistently.
- Keep classes small and purposeful.
- Use `QObject` only when signals/slots or Qt object hierarchy are needed.
- Prefer explicit constructors.
- Prefer `override` on virtual overrides.
- Avoid macros except where Qt requires them.

## Naming

- Types: `PascalCase`
- Methods: `camelCase`
- Members: `m_memberName`
- Signals: action-oriented names such as `tokenReceived`, `requestFinished`, `errorOccurred`
- Interfaces: prefix with `I`, such as `ILLMProvider`

## Header/source organization

- Headers declare public API clearly.
- Source files should keep logic direct and readable.
- Avoid placing large implementation blocks inline in headers.

## Error handling

- Prefer explicit error signals with descriptive messages.
- Avoid silent failure.
- Avoid exception-heavy patterns; remain Qt-style.

## Networking

- Use `QNetworkRequest` and `QNetworkReply` cleanly.
- Set headers explicitly.
- Keep request construction provider-owned.
- Keep request execution network-owned.

## JSON

- Use `QJsonObject`, `QJsonArray`, `QJsonDocument`.
- Keep payload construction readable.
- Avoid string-building JSON by hand.

## Documentation expectations

When adding a new class, include:

- what it is for
- who owns it
- its main responsibility

When changing architecture, update the markdown docs as part of the same change.

## Identity rules

- Repository-owned IDs must come from the shared identity module under `src/qtllm/`.
- Do not call `QUuid::createUuid()` directly in domain logic for client, session, trace, request, span, event, tool-call, artifact, workspace, task, or similar persisted IDs.
- Keep public `qtllm` interfaces source-compatible unless an explicit architecture decision says otherwise.
- Existing public ID-bearing APIs should remain `QString`-based even if internal generation semantics change.
- If a change introduces a new internal ID type or prefix, update [docs/IDENTITY_ARCHITECTURE.md](./docs/IDENTITY_ARCHITECTURE.md) in the same change.
