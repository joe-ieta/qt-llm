# 可观测性与控制

qt-llm 的执行过程不能只暴露最终文本。调用方应能感知 token、payload、runtime 状态、错误和 trace。

## 运行信号

常用信号：

- `tokenReceived`
- `reasoningTokenReceived`
- `providerPayloadPrepared`
- `runtimeStatusChanged`
- `completed`
- `errorOccurred`

## 控制能力

- `QtLLMClient::cancelCurrentRequest()`
- `RuntimeFacade::cancel()`
- managed runtime 的启动和停止。
- provider 可用性刷新。

## ToolsInside

ToolsInside 提供：

- trace summary
- timeline
- span
- artifact
- tool call
- support link

这些数据用于分析 LLM 请求、工具调用、MCP 调用和 Agent 工作流。
