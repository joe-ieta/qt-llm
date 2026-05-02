# 工具和 MCP 集成

qt-llm 的工具能力由独立层提供，普通模型调用不需要接入它。

## 适用场景

- LLM 需要调用本地工具。
- 需要接入 MCP server。
- 需要记录工具调用链路。
- 需要在 ToolStudio 中管理工具定义和元数据。

## 主要模块

- `ToolEnabledChatEntry`：面向聊天入口的工具调用封装。
- `ToolSelectionLayer`：选择可用工具。
- `ToolExecutionLayer`：执行工具。
- `ToolCallOrchestrator`：编排工具调用。
- `providerprotocoladapters`：适配不同 provider 的 tool call 协议。
- `mcp/`：MCP server、client、tool sync。

## 建议

仅在产品确实需要工具调用时接入本层。简单 Host App 使用 `RuntimeFacade` 即可。
