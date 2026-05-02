# CONTRIBUTING / ¹±Ï×Ö¸ÄÏ

## Summary / ÕªÒª
Contribution rules for maintaining clean architecture and stable cross-platform behavior.

## Development Principles
- Keep `QtLLMClient -> ILLMProvider -> HttpExecutor` boundaries clear
- Keep all networking asynchronous and non-blocking
- Prefer small, compilable, incremental changes
- Keep provider-specific logic inside provider classes

## Build and Verify
1. Open `qt-llm.pro` in Qt Creator, or run:
   - `qmake qt-llm.pro`
   - `make` / `mingw32-make`
2. Build `src/qtllm` and `src/examples/simple_chat`

## Pull Request Checklist
- [ ] Builds with qmake
- [ ] No unnecessary heavy dependency introduced
- [ ] Documentation updated for architecture/scope changes
- [ ] Changes remain focused and minimal