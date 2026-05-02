# qt-llm 文档

本目录是 qt-llm 当前有效中文文档入口。历史文档已经归档到 `docs/archive/2026-05-docs-rebaseline/`，归档材料只用于追溯，不作为当前实现依据。

## 阅读路径

使用 qt-llm 的 Qt 应用开发者：

1. [项目定位](./00-project/positioning.md)
2. [Host App 集成手册](./20-integration/host-app-guide.md)
3. [本地 llama.cpp 集成](./20-integration/local-llamacpp-guide.md)
4. [配置参考](./50-reference/config-reference.md)

开发聊天应用：

1. [ConversationClient 手册](./20-integration/conversationclient-guide.md)
2. [multi_client_chat](./40-apps/multi-client-chat.md)
3. [运行数据布局](./50-reference/runtime-data-layout.md)

开发工具调用或 MCP 应用：

1. [工具和 MCP 集成](./20-integration/tool-mcp-guide.md)
2. [ToolsInside](./40-apps/tools-inside.md)
3. [ToolStudio](./40-apps/toolstudio.md)

参与 qt-llm 开发：

1. [顶层约束](../AI_RULES.md)
2. [总体架构](./10-architecture/overview.md)
3. [构建和测试](./30-development/build-and-test.md)
4. [编码规范](./30-development/coding-guidelines.md)

## 文档分区

- [00-project](./00-project/)：定位、功能、路线、待办、决策。
- [10-architecture](./10-architecture/)：架构和运行机制。
- [20-integration](./20-integration/)：外部集成手册。
- [30-development](./30-development/)：开发、测试、发布。
- [40-apps](./40-apps/)：示例 App 和 Agent。
- [50-reference](./50-reference/)：参考信息和故障处理。
- [releases](./releases/)：发布说明。
- [archive](./archive/)：历史文档。
