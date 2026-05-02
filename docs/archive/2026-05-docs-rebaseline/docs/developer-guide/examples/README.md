# 完整代码示例

本目录提供“可直接参考的接线骨架”，与 `scenarios/` 的区别是：

- `scenarios/`
  - 解释开发流程、对象关系和设计意图
- `examples/`
  - 提供更完整的代码片段，适合直接照着搭建

## 示例列表

- [minimal-qtllmclient.md](./minimal-qtllmclient.md)
  - 最小 `QtLLMClient` 接入
- [conversationclient-chat.md](./conversationclient-chat.md)
  - 基于 `ConversationClient` 的聊天窗口骨架
- [tool-enabled-chat.md](./tool-enabled-chat.md)
  - 带工具调用的聊天骨架
- [mcp-host-bootstrap.md](./mcp-host-bootstrap.md)
  - 带 MCP server 管理和工具同步的宿主骨架

## 使用建议

- 先按示例跑通，再回到对应专题页理解细节
- 示例以“结构清晰”为目标，不追求贴满所有 UI 细节
- 示例默认不含业务逻辑，只演示 `qtllm` 的接线方式
