# ConversationClient 集成手册

`qtllm::chat::ConversationClient` 在统一请求执行之上增加会话、历史、profile、上下文预算和快照，适用于聊天产品；单次模型调用优先使用 `RuntimeFacade`。

## 接入

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Conversation)
target_link_libraries(chat_app PRIVATE QtLlm::Conversation)
```

## 能力边界

- 创建、切换和查询 session。
- 设置、读取和清空结构化 `LlmMessage` 历史。
- 发送普通消息或带工具 Schema 的消息。
- 导出、恢复 `ConversationSnapshot`。
- 使用消息数和 token 双预算选择请求窗口。
- 可注入精确 `IMessageTokenCounter`。
- 可选使用 `ConversationRepository` 持久化。

它不负责业务账号、文档上下文、RAG、长期记忆或宿主数据库。

## 存储模式

- 无存储：直接使用 client，历史只在内存中存在。
- 宿主自管：调用前通过结构化消息设置历史。
- 可选仓库：只有显式提供 `ConversationRepository` 时才读写快照。

不要因为使用 ConversationClient 就假设会自动创建持久化文件。

## 上下文窗口

`MemoryPolicy` 控制 `maxHistoryMessages`、`contextWindowTokens`、`reservedOutputTokens` 和 `minimumRecentTurns`。system 消息和完整工具轮次按原子规则保留；预算不足时返回明确错误，不静默截断必保留内容。

监听 `contextWindowEvaluated` 或读取 `lastContextWindowResult()` 可获得估算精度、输入预算和丢弃消息数。

## 生命周期与信号

内容流、推理流、请求准备、Provider 载荷、完成、响应、错误和状态变化继续保持兼容。需要并行请求、精确取消和统一终态时，应从 `RuntimeFacade::sendAsync()` 获取独立句柄，而不是在一个 ConversationClient 上叠加宿主自建请求状态机。

## 相关文档

- [对外接口总览](./public-api-guide.md)
- [上下文窗口](../30-development/context-window-management.md)
- [运行数据布局](../50-reference/runtime-data-layout.md)
