# 会话窗口与上下文预算

## 定位

`ContextWindowService` 在统一 `LlmRequest` 发送前选择本次请求携带的消息。它只处理通用消息窗口、输入预算和原子轮次，不保存会话、不检索文档、不生成摘要，也不决定业务提示词。

`ConversationClient` 使用该服务替代原有的简单尾部消息截断。既有 `setHistory`、`history`、快照和可选 `ConversationRepository` 接口保持不变。

## 保留规则

- 所有 system 消息属于必保留内容。
- 消息按 user 发起的轮次分组，保留最近 `minimumRecentTurns` 个完整组。
- assistant 工具调用、对应 tool 结果和后续 assistant 回复位于同一轮次，选择或丢弃时保持原子性。
- 在必保留内容之外，从近到远加入完整轮次；任一轮次不能完整放入时停止继续扩展。
- 必保留内容超过消息数或 token 预算时返回 `context_required_content_exceeds_budget`，不静默截断必保留内容。

## 两种预算

`ContextWindowPolicy` 同时支持消息数和 token 限制：

- `maxMessages` 为零时不限制消息数。
- `contextWindowTokens` 为零时不启用 token 限制。
- `reservedOutputTokens` 从上下文窗口中预留给模型输出。
- 两种限制同时设置时必须同时满足。

默认 `EstimatedMessageTokenCounter` 使用稳定、偏保守的 UTF-8 大小估算，并把结果标记为 `Estimated`。宿主可实现 `IMessageTokenCounter` 并通过 `ConversationClient::setMessageTokenCounter` 注入模型对应的精确 tokenizer；服务将结果标记为计数器声明的 `Exact`。

## ConversationClient 接入

`MemoryPolicy` 增量增加 `contextWindowTokens`、`reservedOutputTokens` 和 `minimumRecentTurns`。旧字段 `maxHistoryMessages` 继续生效；新增 token 预算默认关闭，因此旧宿主不配置新字段时保持消息数策略。

每次发送前，`ConversationClient`：

1. 组合 profile 中的 system、persona 和 thinking style 消息。
2. 读取当前会话完整 history，不修改持久历史。
3. 应用上下文窗口并通过 `contextWindowEvaluated` 发布评估结果。
4. 成功时把选中消息写入统一 `LlmRequest`；失败时发出明确错误且不发送网络请求。

`lastContextWindowResult` 可用于诊断本次输入 token 数、可用输入预算和丢弃消息数。

## 存储模式

- 无存储：直接构造 `ConversationClient`，使用 `setHistory` 提供宿主历史，不创建数据库或文件。
- 宿主自管：每次调用前传入结构化 `LlmMessage` 历史，qt-llm 只生成请求窗口。
- 可选仓库：`ConversationClientFactory` 只有在显式设置 `ConversationRepository` 时才读写快照。

新预算字段由 `MemoryPolicy` JSON 增量保存。旧快照缺少这些字段时使用兼容默认值；旧单一 `history` 数组仍由现有回退逻辑读取。

## 非目标

- 不实现自动摘要、长期记忆或向量检索。
- 不接管宿主数据库和会话生命周期。
- 不内置模型专属 tokenizer；精确计数由可注入接口提供。
- 不修复业务侧不完整或无效的工具消息历史，只保证已形成轮次不会被部分裁剪。
