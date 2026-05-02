# CODING_GUIDELINES / ´úÂë¹æ·¶

## Summary / ÕªÒª
Practical Qt/C++ conventions for clarity, maintainability, and predictable integration behavior.

## General Conventions
- Use include guards or `#pragma once`
- Keep classes small and focused
- Use `QObject` only when needed
- Prefer explicit constructors and `override`

## Naming
- Types: `PascalCase`
- Methods: `camelCase`
- Members: `m_memberName`
- Interfaces: `I*` prefix

## Source Organization
- Keep headers API-focused
- Keep source files straightforward
- Avoid large inline implementations in headers

## Error Handling
- Emit explicit errors with clear messages
- Avoid silent failures
- Prefer Qt-style error signaling over exception-heavy patterns

## Networking and JSON
- Use `QNetworkRequest`/`QNetworkReply`
- Set headers explicitly
- Keep request building in provider layer
- Use Qt JSON types; do not handcraft JSON strings

## Documentation
- Document ownership and responsibility of new classes
- Update docs together with architecture changes