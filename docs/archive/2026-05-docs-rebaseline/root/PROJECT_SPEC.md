# Project Spec

## 1. Naming and Positioning

- repository: `qt-llm`
- library target: `qtllm`
- historical alias: `qt-llmlite`

The project now serves three roles at once:
1. a reusable Qt/C++ LLM integration library
2. a runtime stack for tool calling, MCP, logging, and trace analysis
3. a set of desktop applications that host and validate those subsystems

## 2. Current Scope

The current scope includes:
- base LLM request/response handling
- host-facing runtime facade for external Qt applications
- provider abstraction and provider factory
- multi-client and multi-session conversation persistence
- profile-driven context construction
- built-in tool catalog and execution framework
- tool selection, protocol adaptation, execution, and multi-round follow-up
- MCP server registration, sync, execution, and persistence
- structured logging and UI log delivery
- `toolsinside` trace and artifact analysis
- `toolstudio` tool catalog and workspace management

## 3. Design Goals

1. keep the implementation Qt/C++ native
2. keep provider abstraction stable
3. keep networking and tool execution asynchronous
4. keep the library core decoupled from UI policy
5. support local-first and intranet-friendly deployment
6. evolve by adding subsystems rather than rewriting the core request path
7. keep Host App integration behind stable `qtllm` APIs instead of making each
   Host App understand provider factories, managed runtime layout, or transport
   internals

## 4. Technical Baseline

- language: C++17
- framework: Qt
- build system: qmake
- main Qt modules: `core`, `network`, `sql`
- UI surfaces: Qt Widgets and QML/Qt Quick

Constraints:
- avoid heavy runtime dependencies
- avoid scattering vendor protocol details into UI and orchestration layers
- avoid mixing application-specific policy into provider and transport code
- avoid duplicating `RuntimeProfile` to `LlmConfig` mapping, local GGUF model
  discovery, or managed llama.cpp startup logic in external Host Apps

## 5. Module Baseline

### 5.1 Core Library

`src/qtllm/` currently contains:
- `core`
- `chat`
- `host`
- `profile`
- `storage`
- `providers`
- `network`
- `streaming`
- `tools`
- `logging`
- `toolsinside`
- `toolsstudio`

### 5.2 Applications

`src/apps/` currently contains:
- `simple_chat`
- `multi_client_chat`
- `mcp_server_manager`
- `tools_inside`
- `toolstudio`

### 5.3 Tests

- `tests/qtllm_tests/`

## 6. First-Class Runtime Flows

The project currently has at least five first-class runtime flows:
1. base chat request flow
2. conversation persistence and restore flow
3. tool selection and tool execution flow
4. MCP sync and execution flow
5. `toolsinside` trace analysis flow

Flows 3 through 5 are now part of the baseline architecture and should not be described as optional side demos.

## 7. Current Status

### Available today

- `QtLLMClient` async request orchestration
- `RuntimeFacade` Host App request facade and local model discovery
- `ConversationClientFactory` multi-client persistence
- `OpenAIProvider` and `OpenAICompatibleProvider`
- tool loop, protocol routing, execution, and failure guard
- MCP registry, repository, manager, and sync services
- `toolsinside` repository, artifact store, query service, and admin service
- `toolstudio` catalog, workspace, import/export, and merge services
- multiple desktop application entry points

### Still being strengthened

- broader provider edge-case coverage
- tool loop hardening and error branches
- MCP long-chain stability
- consistency between logs and trace data
- architecture and documentation convergence

## 8. Branch Focus

This branch is currently focused on core-library strengthening rather than business logic changes:
- clarify module boundaries
- improve consistency between provider, tool runtime, MCP, and `toolsinside`
- reduce documentation drift and naming ambiguity

## 9. Non-goals

The project is not trying to become:
- a model inference framework
- a heavy server-side orchestration platform
- a cloud-vendor-specific SDK wrapper
- a single monolithic app that absorbs every subsystem

This documentation refresh also does not change business behavior.

## 10. Host App Integration Rule

External Qt applications should use
[docs/HOST_APP_INTEGRATION.md](./docs/HOST_APP_INTEGRATION.md) as the canonical
integration guide.

The preferred path for simple embedding is `qtllm::host::RuntimeFacade`.
`QtLLMClient` remains the core request engine. `ConversationClient` remains the
multi-session chat API. `ToolEnabledChatEntry` remains the tool/MCP entry.

Provider creation, managed llama.cpp runtime interpretation, local model
discovery, request execution, stream parsing, logging, tracing, and cancellation
must remain owned by `qt-llm` unless a Host App explicitly opts into advanced
low-level integration.
