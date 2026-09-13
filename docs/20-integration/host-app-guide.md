# Host App 集成手册

## 推荐入口

普通 Qt 应用从 `qtllm::host::RuntimeFacade` 开始，并链接 `QtLlm::Conversation`。它统一解释 `RuntimeProfile`、模型目录、Provider、本地运行时和请求生命周期。

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Conversation)
target_link_libraries(my_app PRIVATE QtLlm::Conversation)
```

## 职责边界

宿主负责 UI、用户设置、业务状态、业务数据和模型选择体验。qt-llm 负责配置映射、Provider 协议、请求执行、流式解析、结构化错误和可选本地运行时。

普通宿主不直接使用 `ProviderFactory`、`HttpExecutor`、`ManagedLlamaCppRuntime` 或具体 Provider。

## 异步请求

```cpp
#include <host/runtimefacade.h>
#include <host/runtimerequesthandle.h>

auto *runtime = new qtllm::host::RuntimeFacade(this);

qtllm::host::RuntimeProfile profile;
profile.providerName = QStringLiteral("llama-cpp");
profile.model = modelId;
profile.llamaCppModelPath = modelPath;
runtime->setProfile(profile);

qtllm::host::ChatRequest request;
request.userPrompt = prompt;

auto *handle = runtime->sendAsync(request);
connect(handle, &qtllm::host::RuntimeRequestHandle::tokenReceived,
        this, &MyWindow::appendToken);
connect(handle, &qtllm::host::RuntimeRequestHandle::finished,
        this, [handle](const qtllm::host::ChatResult &result) {
            // Use structured status and error fields.
            handle->deleteLater();
        });
```

每个句柄独立取消。新 UI 代码不应使用 `sendBlocking()`；该方法和旧 `send()`/`cancel()` 信号路径仅作为兼容接口保留。

## 消息与结构化输出

简单单轮调用可设置 `systemPrompt` 和 `userPrompt`。宿主自管多轮历史时应提供结构化 `messages`，不要把角色和内容拼成单一文本。

需要 JSON 或 JSON Schema 时设置 `ChatRequest::output`，并从 `ChatResult::response.structuredOutput` 读取解析与校验结果。业务有效性仍由宿主判断。

## 本地模型与状态

```cpp
QString errorMessage;
const auto models = runtime->listLocalModels(&errorMessage);
runtime->refreshRuntimeAvailability();
```

多个 `.gguf` 存在时由用户选择。界面应展示 Provider 可用性、已解析 runtime/model 路径和结构化错误，不应复制目录搜索规则。

## 下一步

- [完整接口分层](./public-api-guide.md)
- [请求句柄](../30-development/request-handle.md)
- [结构化输出](../30-development/structured-output.md)
- [本地运行时](./local-llamacpp-guide.md)
