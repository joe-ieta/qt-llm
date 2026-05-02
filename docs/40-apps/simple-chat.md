# simple_chat

`simple_chat` 是最小聊天示例，适合展示 Host App 如何通过 qt-llm 发送基本请求。

当前建议它保持为 `RuntimeFacade` 的参考示例：

- 配置 provider。
- 选择模型。
- 发送 prompt。
- 接收 token 和完成事件。
- 展示错误状态。

它不应引入复杂会话、工具或 MCP 逻辑。
