# Architecture

## 1. Project Positioning

`qt-llm` is organized around the `src/qtllm/` library. The applications under `src/apps/` are not just throwaway demos; they are hosts and integration surfaces for the library subsystems.

The repository should therefore be read as:
- one reusable Qt/C++ LLM library
- several runtime subsystems layered around it
- several applications that validate and expose those subsystems

## 2. Layered Structure

```text
Application Layer
  simple_chat / multi_client_chat / mcp_server_manager / tools_inside / toolstudio

Orchestration Layer
  ToolEnabledChatEntry
  ConversationClient / ConversationClientFactory

Core Request Layer
  QtLLMClient
  LlmConfig / LlmRequest / LlmResponse / llmtypes

Provider & Transport Layer
  ILLMProvider
  OpenAIProvider / OpenAICompatibleProvider / OllamaProvider / VllmProvider
  HttpExecutor / StreamChunkParser

Tool Runtime Layer
  LlmToolRegistry / BuiltInTools / ToolSelectionLayer / LlmToolAdapter
  ToolCallProtocolRouter / ToolCallOrchestrator / ToolExecutionLayer
  ToolCatalogRepository / ClientToolPolicyRepository

External Tool Layer
  MCP client / MCP server registry / MCP tool sync

Observability & Workspace Layer
  QtLlmLogger / FileLogSink / SignalLogSink
  ConversationRepository
  toolsinside
  toolsstudio
```

The dependency direction remains one-way:

```text
Apps/UI
  -> orchestration
  -> core client
  -> provider / transport
  -> external service
```

`logging` and `toolsinside` are side-channel observability systems. They attach to the runtime flow, but they do not define provider or UI behavior.

## 3. Module Responsibilities

### 3.1 `core`

Key files:
- `src/qtllm/core/qtllmclient.*`
- `src/qtllm/core/llmconfig.h`
- `src/qtllm/core/llmtypes.h`

Responsibilities:
- accept app-facing requests
- manage provider, executor, and streaming parser
- generate requestId and traceId
- own the base request lifecycle
- trigger the internal tool loop when an orchestrator is configured

Non-responsibilities:
- UI rendering
- tool catalog persistence policy
- MCP server CRUD

### 3.2 `chat` + `storage` + `profile`

Key files:
- `src/qtllm/chat/conversationclient.*`
- `src/qtllm/chat/conversationclientfactory.*`
- `src/qtllm/storage/conversationrepository.*`
- `src/qtllm/profile/*`

Responsibilities:
- manage `clientId`
- manage active session and history sessions per client
- assemble profile, memory policy, and history into the next request
- persist conversation snapshots under `.qtllm/clients/`

### 3.3 `providers` + `network` + `streaming`

Key files:
- `src/qtllm/providers/*`
- `src/qtllm/network/httpexecutor.*`
- `src/qtllm/streaming/streamchunkparser.*`

Responsibilities:
- isolate vendor protocol differences
- build URL, headers, and payloads
- parse full responses and streaming deltas
- keep transport asynchronous through the Qt network layer

Current main provider paths:
- `OpenAIProvider`: `/responses`
- `OpenAICompatibleProvider`: `/chat/completions`
- `OllamaProvider`, `VllmProvider`: retained as explicit provider entries

### 3.4 `tools`

Key files:
- `src/qtllm/tools/llmtoolregistry.*`
- `src/qtllm/tools/toolenabledchatentry.*`
- `src/qtllm/tools/runtime/*`
- `src/qtllm/tools/protocol/*`

Responsibilities:
- maintain the canonical tool model
- select tools for the current turn
- adapt internal tool definitions to provider-facing schemas
- parse model-returned tool calls
- dispatch built-in and external tool execution
- enforce failure guard, round limits, and follow-up generation

Canonical tool identity is split into:
- `toolId`: internal routing and persistence identifier
- `invocationName`: provider-visible function name
- `name`: UI-facing display name

### 3.5 `tools/mcp`

Key files:
- `src/qtllm/tools/mcp/mcpservermanager.*`
- `src/qtllm/tools/mcp/mcpserverregistry.*`
- `src/qtllm/tools/mcp/mcpserverrepository.*`
- `src/qtllm/tools/mcp/mcptoolsyncservice.*`
- `src/qtllm/tools/mcp/defaultmcpclient.*`

Responsibilities:
- manage MCP server definitions
- persist server configuration to `.qtllm/mcp/servers.json`
- import remote MCP tools into the shared `LlmToolRegistry`
- route `mcp::<serverId>::<toolName>` calls into the MCP client

### 3.6 `toolsinside`

Key files:
- `src/qtllm/toolsinside/toolsinsideruntime.*`
- `src/qtllm/toolsinside/toolsinsiderepository.*`
- `src/qtllm/toolsinside/toolsinsideartifactstore.*`
- `src/qtllm/toolsinside/toolsinsidequeryservice.*`
- `src/qtllm/toolsinside/toolsinsideadminservice.*`
- `src/qtllm/toolsinside/toolsinsidetracerecorder.*`

Responsibilities:
- record trace, event, span, artifact, tool call, and support-link data
- manage workspace-local `.qtllm/tools_inside/`
- expose timeline queries, artifact access, archive, and purge actions

This is not a generic logger. It is a structured analysis subsystem for tool and request chains.

### 3.7 `toolsstudio`

Key files:
- `src/qtllm/toolsstudio/toolcatalogservice.*`
- `src/qtllm/toolsstudio/toolworkspaceservice.*`
- `src/qtllm/toolsstudio/toolimportexportservice.*`
- `src/qtllm/toolsstudio/toolmergeservice.*`

Responsibilities:
- manage the tool catalog
- apply editable metadata overrides
- manage tool workspaces, category trees, and placements
- support workspace export, import, and merge

Runtime data mainly lives under `.qtllm/tools/studio/`.

### 3.8 `logging`

Key files:
- `src/qtllm/logging/qtllmlogger.*`
- `src/qtllm/logging/filelogsink.*`
- `src/qtllm/logging/signallogsink.*`

Responsibilities:
- provide a unified logging entry point
- write per-client JSONL files under `.qtllm/logs/<clientId>/`
- push log events into desktop UI surfaces

`logging` is for operational troubleshooting. `toolsinside` is for trace-grade analysis. They are complementary, not interchangeable.

## 4. Application Layer

`src/apps/` currently contains five entry points:
- `simple_chat`
  - minimal integration surface for `QtLLMClient`
- `multi_client_chat`
  - conversation persistence, multi-client/session flow, and log viewing
- `mcp_server_manager`
  - MCP server management and MCP-backed chat entry
- `tools_inside`
  - QML browser for traces, timelines, artifacts, and support links
- `toolstudio`
  - tool catalog, workspace, placement, import, and export management

These applications are part of the project architecture, not an unrelated examples area.

## 5. Main Runtime Flows

### 5.1 Base Chat Flow

```text
UI
  -> ConversationClient::sendUserMessage
  -> QtLLMClient::sendRequest
  -> Provider::buildRequest/buildPayload
  -> HttpExecutor::post
  -> Provider::parseResponse / parseStreamDeltas
  -> ConversationClient appends assistant message
  -> UI receives completed / responseReceived
```

Notes:
- `ConversationClient` builds the request from profile and history
- `QtLLMClient` owns requestId, traceId, streaming, and final response handling

### 5.2 Conversation Persistence Flow

```text
ConversationClientFactory::acquire(clientId)
  -> ConversationRepository::loadSnapshot
  -> ConversationClient restores sessions/history
  -> historyChanged / sessionsChanged / configChanged
  -> ConversationRepository::saveSnapshot
```

Persistence unit:
- client-level snapshot containing active session, history sessions, config, profile, and message history

### 5.3 Tool Calling Flow

```text
UI
  -> ToolEnabledChatEntry::sendUserMessage
  -> ToolSelectionLayer selects tools
  -> ILlmToolAdapter builds provider-facing schema
  -> ConversationClient::sendUserMessageWithTools
  -> QtLLMClient sends request
  -> ToolCallOrchestrator::processAssistantResponse
  -> ToolExecutionLayer::executeBatch
  -> follow-up prompt is generated
  -> QtLLMClient dispatches the next request
```

Key boundaries:
- the app-facing tool entry is `ToolEnabledChatEntry`
- the actual loop execution center is `QtLLMClient + ToolCallOrchestrator`
- failure guard and round limits live in `ToolCallOrchestrator`

### 5.4 MCP Sync and Execution Flow

```text
McpServerManager
  -> McpServerRepository persistence
  -> McpToolSyncService::syncServerTools
  -> LlmToolRegistry registers MCP tools
  -> ToolExecutionLayer detects mcp:: prefix
  -> DefaultMcpClient::callTool
```

Important detail:
- MCP tools share the same registry with built-in tools
- execution routing happens later, based on the canonical `toolId`

### 5.5 `toolsinside` Observability Flow

Current trace recording points include:
- `ToolEnabledChatEntry`
- `ConversationClient`
- `QtLLMClient`
- `ToolExecutionLayer`

Basic flow:

```text
user turn starts
  -> startTrace
  -> recordToolSelection
  -> recordRequestPrepared / recordRequestDispatched
  -> recordResponseParsed
  -> recordToolCallStarted / recordToolCallFinished
  -> recordTraceCompleted / recordTraceError
```

Storage split:
- structured index: SQLite `index.db`
- large payloads: artifact files

### 5.6 Tool Studio Workflow

```text
ToolStudioWindow
  -> ToolStudioController
  -> ToolCatalogService
  -> ToolWorkspaceService
  -> ToolImportExportService / ToolMergeService
```

This subsystem manages tool assets and workspaces, not runtime conversation execution.

## 6. Workspace Persistence Layout

Typical runtime layout:

```text
.qtllm/
  clients/
    <clientId>.json
  logs/
    <clientId>/*.jsonl
  mcp/
    servers.json
  tools/
    catalog.json
    studio/
      workspace_index.json
      workspaces/*.json
  tools_inside/
    index.db
    artifacts/
    archive/
```

## 7. Architectural Conclusion

The current project should be read with the following assumptions:
- `qtllm` is the center of gravity
- `ConversationClient` is the conversation and context orchestration layer
- `ToolEnabledChatEntry` is the app-side tool orchestration entry
- `QtLLMClient` is the request lifecycle and tool-loop execution center
- `toolsinside` is the structured observability subsystem
- `toolstudio` is the tool asset management subsystem
- `src/apps/` is not an afterthought; it is part of the architecture

Any future core-library optimization on this branch should preserve those boundaries instead of pushing app-specific policy into the library core.
