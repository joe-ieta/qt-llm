# qtllm 开发手册

本手册面向直接使用 `qtllm` 核心库开发应用的工程师，目标是回答三个问题：

1. `qtllm` 在整个工程里的位置是什么
2. 开发应用时应该调用哪些 API 类
3. 不同场景下，推荐的接线方式和调用流程是什么

## 阅读顺序

1. [01-architecture-and-layers.md](./01-architecture-and-layers.md)
2. [02-quick-start.md](./02-quick-start.md)
3. `core/` 下的核心接口类
4. `tools/`、`observability/` 下的能力专题
5. `scenarios/` 下的场景化开发流程
6. `reference/` 下的运行时数据和类索引

## 目录

- 分层与入门
  - [01-architecture-and-layers.md](./01-architecture-and-layers.md)
  - [02-quick-start.md](./02-quick-start.md)
- 核心接口
  - [core/qtllmclient.md](./core/qtllmclient.md)
  - [core/conversationclient.md](./core/conversationclient.md)
  - [core/conversationclientfactory-and-storage.md](./core/conversationclientfactory-and-storage.md)
  - [core/provider-system.md](./core/provider-system.md)
- 工具与 MCP
  - [tools/tool-enabled-chat-entry.md](./tools/tool-enabled-chat-entry.md)
  - [tools/tool-runtime.md](./tools/tool-runtime.md)
  - [tools/mcp-integration.md](./tools/mcp-integration.md)
- 观测与排障
  - [observability/logging-and-tracing.md](./observability/logging-and-tracing.md)
- 场景开发流程
  - [scenarios/basic-chat-app.md](./scenarios/basic-chat-app.md)
  - [scenarios/multi-session-app.md](./scenarios/multi-session-app.md)
  - [scenarios/tool-calling-app.md](./scenarios/tool-calling-app.md)
  - [scenarios/mcp-host-app.md](./scenarios/mcp-host-app.md)
  - [scenarios/debug-and-observe.md](./scenarios/debug-and-observe.md)
- 完整代码示例
  - [examples/README.md](./examples/README.md)
  - [examples/minimal-qtllmclient.md](./examples/minimal-qtllmclient.md)
  - [examples/conversationclient-chat.md](./examples/conversationclient-chat.md)
  - [examples/tool-enabled-chat.md](./examples/tool-enabled-chat.md)
  - [examples/mcp-host-bootstrap.md](./examples/mcp-host-bootstrap.md)
- 参考资料
  - [reference/runtime-data-layout.md](./reference/runtime-data-layout.md)
  - [reference/class-index.md](./reference/class-index.md)

## 阅读路径矩阵

### 只想最快跑通一次聊天请求

按这个顺序读：

1. [02-quick-start.md](./02-quick-start.md)
2. [core/qtllmclient.md](./core/qtllmclient.md)
3. [examples/minimal-qtllmclient.md](./examples/minimal-qtllmclient.md)

### 要做标准聊天窗口

按这个顺序读：

1. [01-architecture-and-layers.md](./01-architecture-and-layers.md)
2. [core/conversationclient.md](./core/conversationclient.md)
3. [scenarios/basic-chat-app.md](./scenarios/basic-chat-app.md)
4. [examples/conversationclient-chat.md](./examples/conversationclient-chat.md)

### 要做多 client / 多 session 应用

按这个顺序读：

1. [core/conversationclientfactory-and-storage.md](./core/conversationclientfactory-and-storage.md)
2. [core/conversationclient.md](./core/conversationclient.md)
3. [scenarios/multi-session-app.md](./scenarios/multi-session-app.md)
4. [examples/conversationclient-chat.md](./examples/conversationclient-chat.md)

### 要做带工具调用的应用

按这个顺序读：

1. [tools/tool-enabled-chat-entry.md](./tools/tool-enabled-chat-entry.md)
2. [tools/tool-runtime.md](./tools/tool-runtime.md)
3. [scenarios/tool-calling-app.md](./scenarios/tool-calling-app.md)
4. [examples/tool-enabled-chat.md](./examples/tool-enabled-chat.md)

### 要做 MCP 宿主或 MCP 工具型应用

按这个顺序读：

1. [tools/mcp-integration.md](./tools/mcp-integration.md)
2. [tools/tool-runtime.md](./tools/tool-runtime.md)
3. [scenarios/mcp-host-app.md](./scenarios/mcp-host-app.md)
4. [examples/mcp-host-bootstrap.md](./examples/mcp-host-bootstrap.md)

### 要做排障与运行时观测

按这个顺序读：

1. [observability/logging-and-tracing.md](./observability/logging-and-tracing.md)
2. [scenarios/debug-and-observe.md](./scenarios/debug-and-observe.md)
3. [reference/runtime-data-layout.md](./reference/runtime-data-layout.md)

## 使用原则

- 只做基础请求发送时，优先使用 `QtLLMClient`
- 需要会话、历史和 profile 时，优先使用 `ConversationClient`
- 需要工具调用时，优先使用 `ToolEnabledChatEntry`
- 需要 MCP 时，不要绕过工具体系单独接入，优先走共享 `LlmToolRegistry`
- 需要排障时，同时使用日志和 `toolsinside`，不要只看其中一套
