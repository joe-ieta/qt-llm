# 公共 API 索引

本索引按公开 CMake 组件组织。普通应用先阅读[对外开发接口与集成指南](../20-integration/public-api-guide.md)。

## CMake 目标

| 目标 | 主要公共区域 |
| --- | --- |
| `QtLlm::Core` | `core/`、`context/`、`streaming/`、`structuredoutput/`、基础 Provider 协议 |
| `QtLlm::Diagnostics` | `events/`、`logging/` |
| `QtLlm::Tools` | `tools/` 及 MCP、协议和执行运行时 |
| `QtLlm::LocalRuntime` | `runtime/` 和本地 llama.cpp 支撑 |
| `QtLlm::Conversation` | `host/`、`chat/`、`QtLLMClient` 和完整高层入口 |
| `QtLlm::QtLlm` | 兼容聚合目标 |

## 推荐宿主入口

| 类型 | 命名空间 | 头文件 | 作用 |
| --- | --- | --- | --- |
| `RuntimeFacade` | `qtllm::host` | `<host/runtimefacade.h>` | 宿主配置、模型目录和请求入口 |
| `RuntimeRequestHandle` | `qtllm::host` | `<host/runtimerequesthandle.h>` | 单请求状态、流、取消和唯一终态 |
| `RuntimeProfile` | `qtllm::host` | `<host/runtimeprofile.h>` | 宿主级 Provider/runtime 配置 |
| `ChatRequest` / `ChatResult` | `qtllm::host` | `<host/runtimeprofile.h>` | 高层请求与结果 |
| `ModelCatalogService` | `qtllm::host` | `<host/modelcatalogservice.h>` | 托管模型目录 |

## 请求与协议核心

| 类型 | 命名空间 | 头文件 |
| --- | --- | --- |
| `QtLLMClient` | `qtllm` | `<core/qtllmclient.h>` |
| `LlmConfig` | `qtllm` | `<core/llmconfig.h>` |
| `LlmMessage` / `LlmRequest` / `LlmResponse` / `LlmUsage` | `qtllm` | `<core/llmtypes.h>` |
| `LlmError` / `LlmErrorCategory` | `qtllm` | `<core/llmtypes.h>` |
| `ContextWindowService` | `qtllm::context` | `<context/contextwindowservice.h>` |
| `StructuredOutputService` | `qtllm::structuredoutput` | `<structuredoutput/structuredoutputservice.h>` |
| `OutputConstraint` / `StructuredOutputResult` | `qtllm::structuredoutput` | `<structuredoutput/structuredoutputtypes.h>` |
| `StreamChunkParser` | `qtllm` | `<streaming/streamchunkparser.h>` |

## 会话

| 类型 | 命名空间 | 头文件 |
| --- | --- | --- |
| `ConversationClient` | `qtllm::chat` | `<chat/conversationclient.h>` |
| `ConversationClientFactory` | `qtllm::chat` | `<chat/conversationclientfactory.h>` |
| `ConversationSnapshot` | `qtllm::chat` | `<chat/conversationsnapshot.h>` |
| `ConversationRepository` | `qtllm::storage` | `<storage/conversationrepository.h>` |
| `MemoryPolicy` | `qtllm::profile` | `<profile/memorypolicy.h>` |

## Provider 与本地运行时

| 类型 | 命名空间 | 头文件 |
| --- | --- | --- |
| `ILLMProvider` | `qtllm` | `<providers/illmprovider.h>` |
| `ProviderFactory` | `qtllm` | `<providers/providerfactory.h>` |
| `ManagedLlamaCppRuntime` | `qtllm::runtime` | `<runtime/managedllamacppruntime.h>` |
| `ManagedLlamaCppRuntimeService` | `qtllm::runtime` | `<runtime/managedllamacppruntimeservice.h>` |
| `LlamaCppRuntimeLease` | `qtllm::runtime` | `<runtime/managedllamacppruntimeservice.h>` |

具体 Provider 类属于高级扩展面；普通宿主通过 profile 和 facade 选择 Provider。

## 工具与 MCP

| 类型 | 命名空间 | 头文件 |
| --- | --- | --- |
| `ToolEnabledChatEntry` | `qtllm::tools` | `<tools/toolenabledchatentry.h>` |
| `LlmToolDefinition` / `LlmToolRegistry` | `qtllm::tools` | `<tools/llmtooldefinition.h>` / `<tools/llmtoolregistry.h>` |
| `ToolExecutionLayer` / `ToolCallOrchestrator` | `qtllm::tools::runtime` | `<tools/runtime/toolexecutionlayer.h>` / `<tools/runtime/toolcallorchestrator.h>` |
| `IToolExecutor` / `ToolRuntimeHooks` | `qtllm::tools::runtime` | `<tools/runtime/itoolexecutor.h>` / `<tools/runtime/toolruntimehooks.h>` |
| `McpServerManager` / `McpToolSyncService` | `qtllm::tools::mcp` | `<tools/mcp/mcpservermanager.h>` / `<tools/mcp/mcptoolsyncservice.h>` |
| `IMcpClient` / `DefaultMcpClient` | `qtllm::tools::mcp` | `<tools/mcp/imcpclient.h>` / `<tools/mcp/defaultmcpclient.h>` |

## 事件、日志与工具数据

- `qtllm::events::ILlmEventSink` / `LlmEventDispatcher`：基础执行事件。
- `qtllm::logging::QtLlmLogger`、`ILogSink`、`SignalLogSink`、`FileLogSink`：日志入口和接收端。
- `qtllm::toolsinside::*`：trace、事件、span、artifact、tool call 的可选存储和查询。
- `qtllm::toolsstudio::*`：工具目录、工作区、导入导出、合并和元数据覆盖。

ToolsInside 和 ToolStudio 不是普通请求的隐式依赖；只有显式初始化后才创建对应运行数据。

## 兼容说明

- 安装后保持 `<core/...>`、`<host/...>`、`<tools/...>` 等包含路径。
- Qt5/Qt6 和 STATIC/SHARED 使用相同 API 名称。
- `QtLlm::QtLlm` 保持既有完整链接方式。
- 公共头文件完整性由 `tests/consumers/public-api` 在四组安装矩阵中编译验证。
