# 架构说明镜像

当前权威文档：
- [../../../ARCHITECTURE.md](../../../ARCHITECTURE.md)

当前架构结论：
- `src/qtllm/` 是工程中心
- `src/apps/` 是聊天、MCP、链路分析和工具管理的宿主层
- 主请求链路是 `ToolEnabledChatEntry / ConversationClient -> QtLLMClient -> Provider -> HttpExecutor`
- `toolsinside` 和 `toolstudio` 都已经是已实现子系统，不再只是设计草稿
