# RELEASE_READINESS_CHECKLIST / 发布准备检查单

## 范围
当前基线覆盖：多 Client/Session、工具运行时、MCP 注册与同步、OpenAI 与 OpenAI-compatible Provider、统一日志。

## 关键检查项
- [ ] 所有库、测试与示例应用可构建
- [ ] `simple_chat`、`multi_client_chat`、`mcp_server_manager_demo` 可启动
- [ ] OpenAI 走 `/responses`，OpenAI-compatible 走 `/chat/completions`
- [ ] MCP Server 可持久化、可重新加载、可同步工具
- [ ] 日志按 client 分目录落盘并可在 UI 中观察
- [ ] `toolId` / `invocationName` / `name` 语义保持一致
