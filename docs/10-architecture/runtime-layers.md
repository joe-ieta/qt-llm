# 运行分层

## RuntimeFacade

`qtllm::host::RuntimeFacade` 是普通 Host App 的首选入口。它接收 `RuntimeProfile` 和 `ChatRequest`，内部映射到 `LlmConfig` 并调用 `QtLLMClient`。

职责：

- RuntimeProfile 到 LlmConfig 的映射。
- 本地模型列表发现。
- provider 可用性刷新。
- 非阻塞发送、阻塞发送、取消请求。
- token、reasoning token、完成、错误、payload、runtime 状态信号。

## QtLLMClient

`QtLLMClient` 是底层请求执行核心。它负责：

- 根据 `LlmConfig` 构建 provider。
- 调用 provider 构造请求。
- 通过 HTTP executor 执行请求。
- 解析普通响应和流式响应。
- 在 managed llama.cpp 场景中确保本地 runtime 可用。

## ConversationClient

`ConversationClient` 叠加会话语义：

- 多 session。
- 历史消息。
- profile/persona。
- snapshot 和 restore。
- 持久化工厂。

## ToolEnabledChatEntry

工具调用入口负责把聊天请求、工具选择、provider 协议和工具执行串起来。它只应在产品需要 tool calling 或 MCP 时使用。
