# 工具与 MCP 集成手册

工具/MCP 是可选层。简单模型请求不需要初始化工具仓库、MCP 客户端或 ToolsInside。

## 组件选择

- 只使用工具注册、策略、协议适配和 MCP 基础能力：链接 `QtLlm::Tools`。
- 使用会话式工具入口 `ToolEnabledChatEntry`：链接 `QtLlm::Conversation`。
- 兼容完整接入：链接 `QtLlm::QtLlm`。

## 主要接口

- `LlmToolRegistry`：注册、移除和查询工具定义。
- `ToolSelectionLayer`：按 profile 和策略选择工具。
- `ToolExecutionLayer`：授权预检、顺序/并发执行、超时、取消和 MCP 路由。
- `ToolCallOrchestrator`：解析调用、限制轮次、恢复后续提示。
- `ToolEnabledChatEntry`：组合会话、工具选择和工具循环。
- `McpServerManager` / `McpToolSyncService`：管理 MCP server 并同步工具目录。

## Internal 模式

默认 Internal 模式由注册到 `ToolExecutorRegistry` 的执行器或 MCP client 实际执行工具。并发默认关闭；重试只有在调用允许重试且提供幂等键时才发生。

## External 模式

已有独立 Agent Runtime 的宿主可选择 `ToolExecutionMode::External`：

1. 使用 `processAssistantResponse()` 解析调用并执行策略预检。
2. 读取 `pendingToolCalls`、`awaitingAuthorization` 和 `awaitingExternalResults`。
3. 宿主执行 ready 调用并保留 call ID。
4. 使用 `completeExternalResults()` 回填结果。
5. 使用返回的 `followUpPrompt` 继续模型请求。

qt-llm 不会在 External 模式中自行启动任意宿主程序。它负责稳定 ID、授权结果、数量限制、结果配对和工具循环恢复；调度、超时和取消由宿主负责。

## 授权与取消

`ToolRuntimeHooks::authorize()` 返回 Allow、Deny 或 Pending。Pending 不是失败，宿主应完成授权后重新提交。`cancelBySession()` 只向声明支持取消且仍在执行的执行器传播尽力取消。

## MCP 边界

MCP server 定义和同步可以持久化；真实 MCP 服务的可用性、权限和部署由宿主负责。运行环境不存在真实 server 时，相关端到端测试可能跳过，不应据此宣称协议服务已在线。

## 相关文档

- [外部工具执行](../30-development/external-tool-execution.md)
- [事件与诊断](../30-development/event-diagnostics.md)
- [API 索引](../50-reference/api-index.md)
