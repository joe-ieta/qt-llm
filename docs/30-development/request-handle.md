# 统一请求模型与异步请求句柄

## 适用范围

`RuntimeFacade::sendAsync()` 是宿主应用发起模型请求的主要入口。它同时适用于流式与非流式请求，也适用于单轮便利调用和由宿主管理历史的多轮消息调用。

旧有 `RuntimeFacade::send()`、`sendBlocking()`、`cancel()` 及其信号继续保留，外部项目无需立即迁移。UI 主线程的新代码应使用异步请求句柄，不应调用会进入嵌套事件循环的 `sendBlocking()`。

## 请求输入

`ChatRequest` 支持两种输入方式：

- `messages`：结构化消息列表，适合多轮上下文和角色消息；只要该列表非空，就优先于便利字段。
- `systemPrompt` 与 `userPrompt`：保留的单轮便利字段，适合简单调用。

`model` 可对当前请求覆盖 `RuntimeProfile::model`，`tools` 可携带 OpenAI 兼容工具定义。会话历史、上下文裁剪和业务提示词仍由宿主决定。

## 请求句柄

`sendAsync()` 立即返回由 `RuntimeFacade` 托管的 `RuntimeRequestHandle`。实际启动被排入对象所属线程的事件队列，因此调用方可以在返回后先连接信号，再接收任何终态。

每个句柄包含独立的：

- 请求 ID、原始请求和最终结果。
- `Pending`、`Running`、`CancelRequested`、`Succeeded`、`Failed`、`Canceled` 状态。
- 带请求 ID 的 token、reasoning token、重试流重置、供应商负载和终态信号。
- `cancel()` 取消入口。

不同句柄使用独立执行客户端，远程请求可以并行运行，事件不会依赖全局“当前请求”进行归属。本地受管运行实例的共享、排队与并发策略由 QTL-07 统一处理。

## 所有权和销毁

- 默认情况下句柄以 `RuntimeFacade` 为父对象，门面销毁时所有未释放句柄一并销毁。
- 调用方在收到 `finished()` 后可以对句柄调用 `deleteLater()`，也可以保留它读取最终状态与结果。
- 句柄及其信号在创建它的 Qt 线程运行；跨线程使用时应遵守 QObject 线程亲和性并采用队列连接。
- 主动销毁进行中的句柄会终止其子对象，但销毁后不保证再发送终态信号；需要终态确认时应先调用 `cancel()` 并等待 `finished()`。

## 结果与错误

`ChatResult` 保留原有文本、供应商、模型、会话与字符串错误字段，并补充：

- `requestId`：与句柄一致的请求关联 ID。
- `canceled`：明确区分取消与一般失败。
- `error`：结构化错误分类、错误码和诊断信息。
- `response`：底层统一 `LlmResponse`，包含完整终态与助手消息。

兼容字段与结构化字段来自同一个终态响应，不建立第二套结果解释逻辑。

## 最小集成示例

```cpp
qtllm::host::ChatRequest request;
request.messages.append({QStringLiteral("system"), QStringLiteral("You are concise.")});
request.messages.append({QStringLiteral("user"), QStringLiteral("Summarize this text.")});

auto *handle = facade->sendAsync(request);
connect(handle, &qtllm::host::RuntimeRequestHandle::tokenReceived,
        this, [](const QString &requestId, const QString &token) {
            // Route the delta by requestId.
        });
connect(handle, &qtllm::host::RuntimeRequestHandle::finished,
        this, [handle](const qtllm::host::ChatResult &result) {
            // Consume result.error/result.response as needed.
            handle->deleteLater();
        });
```

## 兼容迁移建议

1. 现有调用可以继续使用 `send()` 和原有无请求 ID 信号。
2. 新增并行请求或精确取消时，改用 `sendAsync()` 并保存句柄。
3. 多轮应用将既有历史转换为 `messages`，不要把会话存储职责移入 qt-llm。
4. UI 主线程逐步移除 `sendBlocking()`；后台兼容代码可暂时保留。
