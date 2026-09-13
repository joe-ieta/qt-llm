# 外部工具执行模式

## 定位

ToolCallOrchestrator 默认保持 Internal 模式，由 qt-llm 的 ToolExecutionLayer 执行工具。已有 Agent Runtime 的宿主可显式选择 External 模式，由宿主执行实际工具，但继续复用 qt-llm 的工具调用解析、稳定 ID、策略预检和供应商结果格式。

## 外部执行流程

1. 调用 setExecutionMode(ToolExecutionMode::External)。
2. 使用 processAssistantResponse 解析模型工具调用。
3. 检查 ToolLoopOutcome 的 pendingToolCalls、toolResults、awaitingAuthorization 和 awaitingExternalResults。
4. 宿主执行 ready 的调用，并保持 callId。
5. 使用 completeExternalResults 提交结果。
6. 使用返回的 followUpPrompt 继续同一模型请求链。

每个待执行调用都包含供应商 callId、externalCallId 和 qt-llm 生成的 internalToolCallId。宿主遗漏某个结果时，qt-llm 返回 external_tool_result_missing，不会把其他结果错误配对。

## 授权

ToolRuntimeHooks::authorize 返回 Allow、Deny 或 Pending：

- Allow：进入 readyRequests。
- Deny：产生 tool_authorization_denied。
- Pending：产生 tool_authorization_pending，并设置 awaitingAuthorization。

Pending 不是失败结果，宿主应等待用户或策略系统决定后重新提交该调用。授权回调不得执行 UI 阻塞操作。

## 限额

prepareBatch 按 ClientToolPolicy::maxToolsPerTurn 处理批次。超出部分产生 tool_limit_exceeded 和 LimitExceeded 状态，不再静默丢弃。

## 执行治理规则

- 默认顺序执行，只有 `ToolExecutionPolicy::enableParallelExecution` 显式开启后才并发。
- 并发上限同时受全局 `maxParallelCalls` 和工具级 `toolConcurrencyOverrides` 约束。
- 只有调用显式设置 `retryAllowed` 且提供非空 `idempotencyKey` 时，失败结果才可按 `maxRetries` 重试。
- 超过工具级或默认超时时间的调用返回 `TimedOut`，并向支持取消的执行器发送尽力取消请求。
- `cancelBySession` 只传播给当前会话中仍在执行且声明支持取消的执行器。
- 外部模式仍由宿主负责实际调度、取消和超时；qt-llm 负责预检、配对结果并恢复工具循环。

## 并发和超时边界

启用并发后，调用方提供的执行器、事件接收方、日志接收方和运行时钩子必须支持并发调用。默认关闭并发，因此既有调用仍保持顺序行为。

同步执行器接口无法强制终止忽略取消的底层调用，超时状态会在执行返回后判定。需要严格截止时间的工具应在执行器内部实现可中断 I/O，并通过 `supportsCancellation()` 和 `cancel()` 接入取消传播。

上述能力不改变 MCP 客户端消费与 MCP Server 发布的边界，也不引入业务工具或工作流编排。
