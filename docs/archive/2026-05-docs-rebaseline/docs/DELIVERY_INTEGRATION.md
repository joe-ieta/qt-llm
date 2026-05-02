# Delivery and Integration

## 1. Deliverables

The repository currently delivers three major asset types.

### 1.1 Core Library

- path: `src/qtllm/`
- output: `qtllm` static library
- capabilities:
  - base LLM request orchestration
  - conversation persistence
  - tool runtime
  - MCP integration
  - logging and trace analysis

### 1.2 Application Entry Points

- `src/apps/simple_chat/`
- `src/apps/multi_client_chat/`
- `src/apps/mcp_server_manager/`
- `src/apps/tools_inside/`
- `src/apps/toolstudio/`

These applications are both integration references and subsystem hosts. They should no longer be described as a generic `examples/` area.

### 1.3 Documentation

Primary docs:
- `README.md`
- `PROJECT_SPEC.md`
- `ARCHITECTURE.md`
- `docs/REPOSITORY_STRUCTURE.md`
- `docs/TESTING_BASELINE.md`

## 2. Integration Modes

### Mode A: source-level integration

Suitable when the consumer wants to evolve with the repository and keep direct access to the library sources.

Basic path:
1. include `src/qtllm/`
2. add `src/qtllm/qtllm.pro` into the workspace
3. link the required Qt modules
4. construct `QtLLMClient` or `ConversationClient` in the host app

### Mode B: library-level integration

Suitable when `qtllm` is built and distributed as a standalone static library.

Basic path:
1. build `qtllm`
2. export public headers and the static library artifact
3. link them from the business project

## 3. Minimal Integration Surface

### Base chat

Minimal requirements:
- `QtLLMClient`
- `LlmConfig`
- one `ILLMProvider` implementation

Minimal config:
- `providerName`
- `baseUrl`
- `model`
- `apiKey` when required by the provider

### Conversation layer

For multi-client and multi-session usage:
- `ConversationClient`
- `ConversationClientFactory`
- `ConversationRepository`

### Tool layer

For tool calling:
- `ToolEnabledChatEntry`
- `LlmToolRegistry`
- `ToolExecutionLayer`
- `ToolCallOrchestrator`

### MCP layer

For MCP support:
- `McpServerManager`
- `McpServerRegistry`
- `McpToolSyncService`
- `IMcpClient`

## 4. Workspace Data Contract

Default workspace layout:

```text
.qtllm/
  clients/
  logs/
  mcp/
  tools/
  tools_inside/
```

Many runtime capabilities assume a workspace root. Consumers embedding the library should therefore make the workspace root explicit instead of relying entirely on the process working directory.

## 5. Integration Suggestions Inside This Repository

- use `simple_chat` to validate provider setup and base request flow
- use `multi_client_chat` to validate persistence and logging
- use `mcp_server_manager` to validate MCP registration, sync, and execution
- use `tools_inside` to validate trace and artifact recording
- use `toolstudio` to validate tool asset management
