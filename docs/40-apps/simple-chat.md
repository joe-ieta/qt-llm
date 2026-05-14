# simple_chat

`simple_chat` 是最小聊天示例，适合展示 Host App 如何通过 qt-llm 发送基本请求。

当前建议它保持为 `RuntimeFacade` 的参考示例：

- 配置 provider。
- 选择模型。
- 发送 prompt。
- 接收 token 和完成事件。
- 展示错误状态。
- App 启动时会尝试拉起 managed `llama-server`，如果本地 runtime 或模型不可用，只记录状态，不阻断窗口启动。

它不应引入复杂会话、工具或 MCP 逻辑。
