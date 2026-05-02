# Testing Baseline

## 1. Scope

The current automated tests are concentrated in `tests/qtllm_tests/` and mainly cover:
- `ProviderFactory` provider mapping
- `OpenAIProvider` request build and response parsing
- `OpenAICompatibleProvider` request build and response parsing
- `StreamChunkParser` fragmented stream handling

`tools`, `MCP`, `toolsinside`, and `toolstudio` still rely primarily on manual smoke validation.

## 2. Test Project

- path: `tests/qtllm_tests/`
- project file: `tests/qtllm_tests/qtllm_tests.pro`
- framework: Qt Test

## 3. How to Run

### Qt Creator

1. open `qt-llm.pro`
2. build `src/qtllm`
3. build and run `tests/qtllm_tests`

### Command line

```bash
qmake qt-llm.pro
make
./tests/qtllm_tests/qtllm_tests
```

Windows (MSVC example):

```bat
qmake qt-llm.pro -spec win32-msvc "CONFIG+=debug"
nmake /NOLOGO
.	ests\qtllm_tests\debug\qtllm_tests.exe
```

## 4. Manual Smoke Baseline

### `simple_chat`

- basic request sending
- provider switching
- streaming and non-streaming responses

### `multi_client_chat`

- client creation and restore
- session switching
- history persistence
- file log output and UI log display

### `mcp_server_manager`

- MCP server scan, add, remove, and persistence
- capability detail loading
- MCP tool sync into the shared registry
- MCP-backed chat path

### `tools_inside`

- workspace selection
- client/session/trace list loading
- timeline, tool-call, and artifact display
- archive and purge actions

### `toolstudio`

- tool catalog loading
- workspace create/open/remove
- category tree and placement editing
- import, export, and merge preview

## 5. Current Testing Conclusion

The current testing strategy is still "core library automation + subsystem smoke verification". As the core library is strengthened further, the highest-value next automation targets are:
- tool-loop branches and failure guard
- MCP sync and execution routing
- `toolsinside` recording consistency
- `toolstudio` persistence and import/export behavior
