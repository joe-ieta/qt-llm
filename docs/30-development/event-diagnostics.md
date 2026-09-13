# qt-llm 基础事件与诊断接入

## 1. 目标

基础执行路径只发布 LLM 调用事件，不解释宿主的 task、step、workflow 或业务状态。事件默认没有接收端，因此仅使用 `QtLLMClient` 不会初始化 ToolsInside，也不会隐式创建 SQLite 数据库或诊断文件。

## 2. 公共接口

- `qtllm/events/illmeventsink.h` 定义可选事件接收端。
- `qtllm/events/llmeventdispatcher.h` 管理接收端并分发事件。

宿主实现 `ILlmEventSink` 的所需方法，再通过 `LlmEventDispatcher::addSink()` 注册。所有方法都有无操作默认实现，接收端只需覆盖关心的事件。同一共享实例只注册一次；`removeSink()` 后不再接收事件。

## 3. 事件范围

| 阶段 | 事件 | 关键关联 |
| --- | --- | --- |
| trace | `startTrace` | `clientId`、`sessionId`、`traceId` |
| 请求 | 准备、发送 | `traceId`、`requestId`、Provider、模型 |
| 重试 | attempt started、attempt reset | `requestId`、attempt |
| 流式 | delta、first token | `traceId`、`requestId`、channel |
| 响应 | response parsed | `traceId`、`requestId` |
| 工具 | 选择、解析、批次、调用、跟进、失败保护 | request、tool call、round |
| 取消 | cancellation requested | `traceId`、`requestId` |
| 终态 | completed、error | `traceId`、`requestId` |

同一逻辑请求在工具跟进和 HTTP 重试中保持同一 `requestId`。attempt 从 `1` 开始，重试前先发出 reset，再发出下一次 started。终态遵循 QTL-02 的唯一终态规则。

## 4. 投递和线程

事件在产生事件的线程同步投递。分发器先复制接收端快照并释放锁，再逐个回调，因此接收端可以在回调中安全增删接收端。

接收端应保持短时、非阻塞并自行保证线程安全。远端写入、较慢脱敏或批量持久化应由接收端投递到自己的工作线程；qt-llm 不替宿主决定队列、丢弃或重试策略。

## 5. 载荷与隐私

请求准备、Provider 载荷、工具参数、工具结果和最终文本可能包含敏感内容。默认无接收端即不记录。自定义接收端负责在写盘或外发前完成脱敏和访问控制。

流式 delta 事件只携带 channel 和文本长度，不复制正文。ToolsInside 继续使用 `ToolsInsideStoragePolicy` 与 `IToolsInsideRedactionPolicy` 控制正文、载荷、工具数据和结果持久化。

## 6. 可选接收端

### ToolsInside

`ToolsInsideRuntime` 初始化时把 `ToolsInsideTraceRecorder` 注册为事件接收端。显式调用 `configureWorkspaceRoot()`，或使用 ToolsInside 查询/管理能力，会按现有目录结构启用 SQLite、artifact 和 trace 记录。未初始化 ToolsInside 时，核心调用不访问该数据库。

现有 trace、span、artifact 和工具调用记录保持不变，并新增 attempt、reset、取消及无正文的流增量事件。

### 文件日志

文件日志仍通过 `QtLlmLogger::installFileSink()` 显式启用。事件接收端和日志接收端相互独立：事件用于稳定执行语义，日志用于可读诊断，不应从日志文案反推请求状态。

### 测试接收端

测试可实现轻量 `ILlmEventSink`，只记录事件名和关联 ID，不需要启动数据库。全局分发器属于进程级设施，测试结束时应移除自身接收端，避免跨用例污染。

## 7. 验证

Qt 6.10.3 与 Qt 5.15.2 的 Release 全目标构建和 CTest 均通过。事件测试覆盖重复注册去重、attempt/reset/终态顺序、关联 request ID 以及移除后停止投递。
