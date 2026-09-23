# QtLLMClient 高级集成手册

`qtllm::QtLLMClient` 是高级请求执行入口，供 facade、测试以及需要自行控制 Provider、配置和生命周期的调用方使用。普通宿主优先使用 `RuntimeFacade`。

## 接入

`QtLLMClient` 位于完整 Conversation 组件中：

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Conversation)
target_link_libraries(advanced_client PRIVATE QtLlm::Conversation)
```

`QtLlm::Core` 只包含轻量协议类型和服务，不包含 `QtLLMClient`。

## 主要接口

- `setConfig(const LlmConfig&)`
- `setProvider(...)` / `setProviderByName(...)`
- `setToolCallOrchestrator(...)`：传入 `nullptr` 关闭库内工具循环，工具调用随最终响应返回调用方处理。
- `setManagedLlamaCppRuntimeService(...)`
- `sendPrompt(...)` / `sendRequest(...)`
- `cancelCurrentRequest()`

## 生命周期契约

- 每个已受理请求产生稳定 `requestId`。
- `requestFinished(LlmResponse)` 是唯一终态。
- 活动请求存在时的新请求通过 `requestRejected` 明确拒绝，不替换当前请求。
- `streamReset` 表示重试前的增量内容必须撤销。
- 取消会停止活动传输和退避计时，并最终产生 Canceled 终态。
- `completed`、`responseReceived` 和 `errorOccurred` 继续作为兼容信号发送。

`QtLLMClient` 本身是单活动请求模型。需要同一宿主并行请求时，为每次请求使用 `RuntimeRequestHandle`，不要共享一个 client 强行并发。

## 调用方责任

直接使用此类时，调用方负责完整 `LlmConfig`、Provider 选择、对象线程、信号生命周期和高级依赖注入。业务判断应基于 `LlmResponse::error`，不要解析错误字符串。

## 相关文档

- [请求生命周期](../30-development/request-lifecycle.md)
- [结构化输出](../30-development/structured-output.md)
- [事件与诊断](../30-development/event-diagnostics.md)
