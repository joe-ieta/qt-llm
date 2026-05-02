# ConversationClient 手册

`ConversationClient` 面向聊天类产品。它在 `QtLLMClient` 之上增加会话、历史、profile 和 snapshot。

## 适用场景

- 多会话聊天。
- 需要保存和恢复历史。
- 需要 persona/profile。
- 需要将聊天状态持久化。

## 主要能力

- 创建和切换 session。
- 维护 history。
- 清空历史。
- 发送普通用户消息。
- 发送带工具的用户消息。
- 导出和恢复 `ConversationSnapshot`。

## 与 RuntimeFacade 的区别

`RuntimeFacade` 是简单 Host App 的请求级入口。`ConversationClient` 是聊天产品入口。不要为了单次模型调用强行接入完整会话层。
