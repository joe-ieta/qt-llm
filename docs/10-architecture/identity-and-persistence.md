# 身份与持久化

qt-llm 使用紧凑前缀 ID 表达业务和观测对象。公共接口仍使用 `QString`，内部生成逻辑集中在 `src/qtllm/identity/`。

常见前缀：

- `cli`：client
- `ses`：session
- `trc`：trace
- `req`：request
- `spn`：span
- `evt`：event
- `tcl`：tool call
- `art`：artifact
- `wsp`：workspace
- `nod`：node
- `pkg`：package
- `tsk`：task
- `que`：queue

## 持久化

- 会话 snapshot 由 `ConversationSnapshot` 表达。
- `ConversationClientFactory` 可管理多个 client 并触发保存。
- ToolsInside 和 ToolStudio 使用各自 repository 管理运行数据和工具数据。

ID 结构变化时，内部持久化数据可以按版本策略迁移或重新初始化；公共接口不应暴露 GUID/UUID 依赖。
