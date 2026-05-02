# pdf_translator_agent

`pdf_translator_agent` 是业务 Agent 参考实现，用于验证文档翻译类工作流如何建立在 qt-llm 核心能力之上。

它包含：

- 应用启动和 UI。
- 文档翻译任务。
- 批处理队列控制。
- 工作流控制。
- 内置技能。
- MCP gateway。
- manifest 持久化。

业务 Agent 的职责是组合能力，不应把可复用 LLM/provider/runtime 逻辑写回 Agent 内部。
