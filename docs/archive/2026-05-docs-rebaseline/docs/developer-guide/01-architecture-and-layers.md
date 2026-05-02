# 分层与职责

## 1. 总体分层

从开发者角度，`qtllm` 工程可以理解为下面五层：

```text
App / UI
  -> 应用入口和界面

Orchestration
  -> ToolEnabledChatEntry
  -> ConversationClient / ConversationClientFactory

Core Client
  -> QtLLMClient

Provider / Transport
  -> ILLMProvider
  -> ProviderFactory
  -> HttpExecutor / StreamChunkParser

Runtime Subsystems
  -> tools / MCP / logging / toolsinside / storage
```

## 2. 各层该做什么

### 应用层

负责：

- UI
- 业务按钮、菜单、窗口
- 组装 `qtllm` 提供的接口类
- 把信号映射到界面行为

不负责：

- 直接拼厂商协议 JSON
- 直接处理工具循环细节
- 直接管理 Provider 流式解析

### 编排层

主要类：

- `ConversationClient`
- `ConversationClientFactory`
- `ToolEnabledChatEntry`

职责：

- 组织一次用户输入的上下文
- 管理 session 和历史消息
- 组织工具选择和工具循环入口

### 核心请求层

主要类：

- `QtLLMClient`

职责：

- 发送请求
- 管理当前 Provider
- 处理流式返回
- 在需要时进入内部 tool loop

### Provider 与传输层

主要类：

- `ILLMProvider`
- `ProviderFactory`
- `HttpExecutor`
- `StreamChunkParser`

职责：

- 构造 URL、Header、Payload
- 解析响应
- 保持网络层异步

### 运行时子系统

包括：

- `tools`
- `tools/mcp`
- `logging`
- `toolsinside`
- `storage`

职责：

- 工具目录与工具执行
- MCP server 接入
- 日志
- trace 和 artifact 观测
- 会话持久化

## 3. 开发时如何选入口

### 只做单轮聊天

选：

- `QtLLMClient`

原因：

- 最简单
- 最贴近底层
- 适合最小接入

### 需要会话与历史

选：

- `ConversationClient`
- `ConversationClientFactory`

原因：

- 自动管理 session
- 自动拼接 profile 和 history
- 适合桌面聊天应用

### 需要工具调用

选：

- `ToolEnabledChatEntry`
- `ConversationClient`
- `LlmToolRegistry`

原因：

- `ToolEnabledChatEntry` 是应用侧工具编排入口
- 可以把工具选择、schema 适配和执行链路统一接好

### 需要 MCP

选：

- `McpServerManager`
- `McpToolSyncService`
- `ToolExecutionLayer`

原因：

- MCP 在当前工程中不是独立聊天通道，而是共享工具体系的一部分

## 4. 主调用链路

### 基础聊天

```text
UI
  -> ConversationClient::sendUserMessage
  -> QtLLMClient::sendRequest
  -> ILLMProvider
  -> HttpExecutor
  -> responseReceived / completed
```

### 工具调用

```text
UI
  -> ToolEnabledChatEntry::sendUserMessage
  -> ToolSelectionLayer
  -> ConversationClient::sendUserMessageWithTools
  -> QtLLMClient
  -> ToolCallOrchestrator
  -> ToolExecutionLayer
  -> follow-up prompt
  -> QtLLMClient 再次发起请求
```

### MCP 调用

```text
McpServerManager
  -> McpToolSyncService
  -> LlmToolRegistry
  -> ToolExecutionLayer
  -> IMcpClient
```

## 5. 开发边界建议

- UI 代码不要直接依赖 `ILLMProvider` 的具体实现细节
- 应用不要自己拼工具 follow-up prompt
- 工具权限、工具执行和工具路由尽量留在 `tools/runtime`
- 需要排障时不要只看 `completed`，还要结合日志与 trace
