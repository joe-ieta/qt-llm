# AI_RULES / AI 协作规则

## Summary / 摘要
Rules for coding agents contributing to qt-llmlite.

## Always
1. Prefer Qt-native solutions
2. Keep networking asynchronous
3. Avoid blocking UI thread
4. Preserve `QtLLMClient -> ILLMProvider` boundary
5. Keep changes focused and minimal
6. Keep code readable for Qt Creator users

## Avoid
1. Heavy dependencies without strong reason
2. Mixing UI logic into provider/network layers
3. Hard-coding a single provider
4. Synchronous waiting patterns

## Implementation Style
- Use Qt signals/slots
- Use `QNetworkAccessManager`
- Use Qt JSON types (`QJsonObject`, `QJsonArray`, `QJsonDocument`)
- Keep header/source boundaries clear

## Documentation Updates
- Update architecture/spec/decisions docs when design changes