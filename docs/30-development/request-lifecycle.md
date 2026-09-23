# qt-llm 请求生命周期契约

## 1. 目的与适用范围

本文说明 `QtLLMClient` 和底层 `HttpExecutor` 的单请求生命周期、终态、错误、取消及重试规则。该契约不包含工作流步骤重试、业务任务状态、宿主历史管理或 QTL-04 规划的独立请求句柄。

## 2. 逻辑请求与唯一终态

`QtLLMClient::sendRequest()` 受理请求后生成一个稳定 `requestId`，并发出 `requestStarted(requestId)`。同一逻辑请求中的工具跟进调用和 HTTP 重试沿用该 ID。

每个已受理请求只发出一次 `requestFinished(LlmResponse)`：

| 终态 | `success` | `canceled` | `error.category` |
| --- | --- | --- | --- |
| 成功 | `true` | `false` | `None` |
| 失败 | `false` | `false` | 具体失败类别 |
| 取消 | `false` | `true` | `Canceled` |

`requestFinished` 是新集成应使用的唯一终态信号。一次请求结束后，迟到的网络回调不会再次改变终态。

当客户端已有活动请求时，新的 `sendRequest()` 不会替换或取消当前请求，而是发出 `requestRejected("request_in_progress", ...)`。拒绝不是当前活动请求的终态。

## 3. 兼容信号

原有接口继续保留，映射规则如下：

| 唯一终态 | 兼容信号 |
| --- | --- |
| 成功 | `responseReceived(response)`，随后 `completed(text)` |
| 失败 | `errorOccurred(message)` |
| 取消 | `errorOccurred("Request canceled")` |

工具失败保护触发后只报告失败，不再同时发出 `responseReceived` 或 `completed`。已有只监听旧信号的宿主可以保持原接入方式，新宿主应优先监听 `requestFinished`，避免自行拼装终态。

## 4. 结构化错误

`LlmResponse::error` 提供稳定、与具体 Provider 文案解耦的错误信息：

| 字段 | 含义 |
| --- | --- |
| `category` | `Configuration`、`Runtime`、`Transport`、`Protocol`、`Tool` 或 `Canceled` |
| `code` | 稳定机器码，例如 `request_timeout`、`network_error`、`http_error` |
| `message` | 面向调用方的简短错误说明 |
| `diagnostic` | 底层网络或 Provider 诊断，不应直接作为业务判断条件 |
| `retryable` | 该次失败是否具备传输级重试条件 |
| `httpStatus` | 可用时返回 HTTP 状态，否则为 `0` |
| `attempt` | 产生最终错误的传输尝试序号，从 `1` 开始 |

HTTP 4xx/5xx 失败时，`message` 优先使用 Provider 响应体中的错误说明，取不到时回退为网络层诊断；`HttpRequestError::responseBody` 保留截断后的原始响应体供宿主展示。

兼容字段 `LlmResponse::errorMessage` 继续填充，并与 `error.message` 保持一致。

## 5. 取消语义

调用 `cancelCurrentRequest()` 时：

1. 若没有活动请求，不产生事件。
2. 若存在活动请求，先发出 `cancellationRequested(requestId)`。
3. 活动 HTTP 传输被中止；重试退避计时器被停止。
4. 请求最终通过一次 `requestFinished` 返回 `Canceled`，并通过旧 `errorOccurred` 兼容转接。

当前 `QtLLMClient` 不建立待启动请求队列，并发提交会被明确拒绝，因此不存在取消后仍自动启动的排队请求。Provider 选择和受管运行时启动仍是同步准备过程；控制权返回前不可抢占，这一限制不会被误报为已完成取消。独立句柄、并行请求和精确取消由 `RuntimeFacade::sendAsync()` 与 `RuntimeRequestHandle` 提供；本节仍描述单个 `QtLLMClient` 的单活动请求边界。

## 6. 重试与流重置

`HttpExecutor` 对超时、一般网络错误以及 HTTP `408`、`425`、`429` 和 `5xx` 进行配置范围内的重试。其他 HTTP 客户端错误不自动重试。

每次传输开始时发出 `attemptStarted(attempt)`。失败且准备重试时：

1. 发出 `attemptReset(previousAttempt, nextAttempt)`。
2. 清空上一 attempt 的响应缓冲。
3. `QtLLMClient` 清空内容、推理和流解析状态。
4. 发出 `streamReset(requestId, nextAttempt)`。
5. 退避计时结束后启动下一 attempt。

流式界面应在收到 `streamReset` 时删除上一 attempt 已展示的临时内容。最终 `LlmResponse` 只包含成功 attempt 的内容，不混入失败 attempt 的缓冲。退避期间取消会停止计时器，不会再次发起网络请求。

## 7. 集成建议

- 新代码以 `requestFinished` 作为唯一终态来源。
- 使用 `requestId` 关联日志、流和最终响应。
- 流式显示必须处理 `streamReset`；不要把增量文本直接视为最终结果。
- 业务回退依据 `error.category`、`error.code` 和 `retryable`，不要解析错误文案。
- 旧代码可继续监听 `completed`、`responseReceived` 和 `errorOccurred`，迁移期间不要同时把新旧信号分别计为终态。

## 8. 验证矩阵

| 环境 | 配置 | 全量构建 | CTest |
| --- | --- | --- | --- |
| Qt 6.10.3 / MSVC 2022 x64 | 通过 | STATIC/SHARED Release 通过 | 每组 `6/6` 通过 |
| Qt 5.15.2 / MSVC 2022 x64 | 通过 | STATIC/SHARED Release 通过，告警门禁通过 | 每组 `6/6` 通过 |

回归测试覆盖重试后响应缓冲隔离，以及退避期间取消不再启动下一次请求。工具失败的唯一终态由 `QtLLMClient::finishRequest()` 统一出口保证。
