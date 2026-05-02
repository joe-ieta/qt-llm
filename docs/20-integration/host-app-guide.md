# Host App 集成手册

外部 Qt 应用集成 qt-llm 时，推荐从 `qtllm::host::RuntimeFacade` 开始。

## 推荐路径

```text
Host App
  -> RuntimeFacade
  -> QtLLMClient
  -> Provider / Managed Runtime / HttpExecutor
```

Host App 负责：

- UI 状态。
- 用户设置持久化。
- 业务状态。
- 选择模型和显示运行状态。

qt-llm 负责：

- RuntimeProfile 到 LlmConfig 的解释。
- provider 生命周期。
- 本地 llama.cpp runtime 查找和启动。
- 模型发现。
- 请求执行和流式解析。
- 错误和运行状态信号。

## 最小示例

```cpp
auto *runtime = new qtllm::host::RuntimeFacade(this);

qtllm::host::RuntimeProfile profile;
profile.providerName = QStringLiteral("llama-cpp");
profile.model = modelId;
profile.llamaCppModelPath = modelPath;
runtime->setProfile(profile);

connect(runtime, &qtllm::host::RuntimeFacade::tokenReceived,
        this, &MyWindow::appendToken);
connect(runtime, &qtllm::host::RuntimeFacade::runtimeStatusChanged,
        this, &MyWindow::showRuntimeStatus);
connect(runtime, &qtllm::host::RuntimeFacade::completed,
        this, &MyWindow::showResult);

qtllm::host::ChatRequest request;
request.userPrompt = prompt;
runtime->send(request);
```

## 模型列表

```cpp
QString error;
const auto models = runtime->listLocalModels(&error);
```

多个 `.gguf` 模型存在时，应展示列表并由用户选择，不应隐式猜测。

## 不推荐

普通 Host App 不应直接使用：

- `ProviderFactory`
- `HttpExecutor`
- `ManagedLlamaCppRuntime`
- provider 具体实现类
