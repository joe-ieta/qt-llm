# AI_RULES.md

These rules are intended for coding agents working in this repository.

## Always do

1. Prefer Qt-native solutions over third-party abstractions.
2. Keep network requests asynchronous.
3. Avoid blocking the UI thread.
4. Preserve the `QtLLMClient` → `ILLMProvider` abstraction boundary.
5. Keep code small, readable, and composable.
6. Make minimal, targeted changes instead of wide speculative refactors.
7. Favor source code that compiles cleanly in Qt Creator.
8. Prefer OpenAI-compatible transport shapes for reusable provider logic.
9. Put provider-specific logic into provider classes, not into UI code.
10. Add documentation when introducing new architectural concepts.

## Avoid

1. Adding heavy dependencies without strong justification.
2. Mixing UI rendering details into networking or provider code.
3. Hard-coding one provider throughout the project.
4. Introducing synchronous waiting patterns.
5. Using complex template metaprogramming unless clearly necessary.
6. Large-scale restructures without updating all context files.

## Expected implementation style

- Use Qt signals/slots for async notifications.
- Use `QNetworkAccessManager` for HTTP.
- Use `QJsonObject`, `QJsonArray`, and `QJsonDocument` for JSON payloads.
- Use `QString`, `QByteArray`, and Qt containers where natural.
- Keep headers focused and implementation files straightforward.

## When extending features

- Update `ARCHITECTURE.md` if a new subsystem appears.
- Update `PROJECT_SPEC.md` if scope changes.
- Update `docs/DECISIONS.md` for important design choices.
