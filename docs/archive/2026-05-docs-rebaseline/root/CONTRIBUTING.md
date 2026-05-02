# Contributing to qt-llm

## Development principles

- Keep `QtLLMClient -> ILLMProvider -> HttpExecutor` boundaries clean.
- Keep all network behavior asynchronous and non-blocking.
- Prefer small, compilable, incremental changes.
- Keep provider-specific logic inside provider classes.

## Build and verify

1. Open `qt-llm.pro` in Qt Creator, or run:
   - `qmake qt-llm.pro`
   - `make` (or `mingw32-make` on Windows)
2. Build `src/qtllm` and `src/examples/simple_chat`.

## Pull request checklist

- [ ] Code builds with qmake setup.
- [ ] No new heavy dependencies introduced.
- [ ] Documentation updated when architecture/scope changes.
- [ ] Changes are focused and minimal.