# 类索引

## 核心请求与会话

- `qtllm::QtLLMClient`
  - 底层请求执行器，负责 Provider、HTTP、流式返回和工具循环执行
- `qtllm::chat::ConversationClient`
  - 会话编排器，负责 session、history、profile 和消息发送
- `qtllm::chat::ConversationClientFactory`
  - 多 client 管理与恢复入口
- `qtllm::storage::ConversationRepository`
  - 会话快照持久化

## Provider

- `qtllm::ILLMProvider`
  - Provider 抽象接口
- `qtllm::ProviderFactory`
  - 按名称创建 Provider
- `qtllm::OpenAIProvider`
  - OpenAI `/responses`
- `qtllm::OpenAICompatibleProvider`
  - `/chat/completions` 兼容路径
- `qtllm::OllamaProvider`
  - Ollama Provider 入口
- `qtllm::VllmProvider`
  - vLLM Provider 入口

## 工具体系

- `qtllm::tools::ToolEnabledChatEntry`
  - 应用侧工具编排入口
- `qtllm::tools::LlmToolDefinition`
  - canonical 工具定义
- `qtllm::tools::LlmToolRegistry`
  - 工具注册表
- `qtllm::tools::ToolSelectionLayer`
  - 本轮工具选择器
- `qtllm::tools::ILlmToolAdapter`
  - 工具 schema 适配器接口
- `qtllm::tools::runtime::ToolCallOrchestrator`
  - 工具循环编排器
- `qtllm::tools::runtime::ToolExecutionLayer`
  - 工具执行层
- `qtllm::tools::runtime::ClientToolPolicyRepository`
  - client 工具权限策略

## MCP

- `qtllm::tools::mcp::McpServerManager`
  - MCP server 管理入口
- `qtllm::tools::mcp::McpServerRegistry`
  - MCP server 内存注册表
- `qtllm::tools::mcp::McpServerRepository`
  - MCP server 持久化
- `qtllm::tools::mcp::McpToolSyncService`
  - MCP tool 同步服务
- `qtllm::tools::mcp::IMcpClient`
  - MCP 调用接口
- `qtllm::tools::mcp::DefaultMcpClient`
  - 默认 MCP client 实现

## 观测

- `qtllm::logging::QtLlmLogger`
  - 统一日志入口
- `qtllm::logging::FileLogSink`
  - 文件日志 sink
- `qtllm::logging::SignalLogSink`
  - UI 信号日志 sink
- `qtllm::toolsinside::ToolsInsideRuntime`
  - `toolsinside` 运行时入口
- `qtllm::toolsinside::ToolsInsideTraceRecorder`
  - trace 记录器
- `qtllm::toolsinside::ToolsInsideQueryService`
  - trace 查询服务
- `qtllm::toolsinside::ToolsInsideAdminService`
  - archive / purge 等管理服务

## 工具资产管理

- `qtllm::toolsstudio::ToolCatalogService`
  - 工具目录服务
- `qtllm::toolsstudio::ToolWorkspaceService`
  - 工具工作区服务
- `qtllm::toolsstudio::ToolImportExportService`
  - 导入导出服务
- `qtllm::toolsstudio::ToolMergeService`
  - 工作区合并服务

## 进一步阅读

- 基础请求：[`../core/qtllmclient.md`](../core/qtllmclient.md)
- 会话：[`../core/conversationclient.md`](../core/conversationclient.md)
- 工具：[`../tools/tool-runtime.md`](../tools/tool-runtime.md)
- MCP：[`../tools/mcp-integration.md`](../tools/mcp-integration.md)
- 观测：[`../observability/logging-and-tracing.md`](../observability/logging-and-tracing.md)
