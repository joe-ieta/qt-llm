# tools_inside

`tools_inside` 用于查看和分析 qt-llm 运行过程中的 trace、事件、span、artifact 和 tool call。

适用场景：

- 调试 LLM 请求。
- 分析工具调用流程。
- 检查 Agent 工作流执行过程。
- 追踪失败请求的上下文。

ToolsInside 是项目“执行过程可观测”的主要参考界面。

启动时，App 会尝试拉起 managed `llama-server`。如果本地 runtime 不完整，失败会进入日志，不影响 trace 浏览界面启动。
