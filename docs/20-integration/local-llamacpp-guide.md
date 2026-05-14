# 本地 llama.cpp 集成

qt-llm 支持内置托管 `llama.cpp` 的 `llama-server`，provider 名称使用 `llama-cpp`。

## 运行态目录

推荐布局：

```text
llama-cpp-runtime/
  bin/
    llama-server.exe
  models/
    model-a.gguf
    model-b.gguf
  logs/
```

Linux 下可执行文件名通常是 `llama-server`。

## 查找顺序

未显式指定 `llamaCppRuntimeRoot` 时，qt-llm 按顺序查找：

1. 当前程序目录下的 `llama-cpp-runtime`
2. 环境变量根目录下的 `llama-cpp-runtime`
   - `ZNZ_HOME`
   - `ZNZ_BLACKBOARD`
   - `YIDA_HOME`
   - `YIDA_BLACKBOARD`
   - `IETA_HOME`
   - `IETA_BLACKBOARD`
3. Linux：
   - `/home/ieta/LLMs/llama-cpp-runtime`
   - `/home/IETA/LLMs/llama-cpp-runtime`
4. Windows：
   - 每个磁盘根目录下的 `LLMs/llama-cpp-runtime`

候选目录会按可用性评分选择。只有空目录的本地 `llama-cpp-runtime` 不应遮挡后续共享目录。

## 模型选择

`models/` 下可以放多个 `.gguf`。App 应调用模型列表接口，把实际列表展示给用户，再把用户选择写入：

```cpp
profile.model = model.id;
profile.llamaCppModelPath = model.filePath;
```

这与 OpenAI API 的模型选择方式一致：模型是请求配置的一部分，而不是 runtime 隐式猜测。

## GPU offload

`llamaCppGpuLayers` 使用三态整数语义：

- `< 0`：auto。managed runtime 会传入 `--gpu-layers 999`，让 llama.cpp 在可用时尽可能 offload 到 GPU。
- `0`：禁用 GPU offload，传入 `--gpu-layers 0`。
- `> 0`：显式 offload 层数，传入对应数值。

如果 `llamaCppExtraArgs` 中已经包含 `--gpu-layers`、`--gpu-layers=<n>`、`--n-gpu-layers`、`--n-gpu-layers=<n>` 或 `-ngl`，runtime 不会再注入默认 GPU 参数，调用方可以完全接管该参数。

注意：传入 GPU layers 只表示请求 GPU offload。实际能否使用 GPU，取决于 `llama-cpp-runtime/bin` 是否包含 GPU backend 库，例如 CUDA、Vulkan、HIP、SYCL 等。只有 CPU backend 的 runtime 即使传入 `--gpu-layers 999`，也只能 CPU 运行。

## 进程生命周期

工具 App 启动时可以通过 `startManagedLlamaCppRuntimeForApp()` 预拉起 managed `llama-server`。该入口会把 runtime 对象挂到 App 生命周期上，并在 `aboutToQuit` 时显式停止进程。

`ManagedLlamaCppRuntime::ensureRunning()` 启动前会检查目标端口。如果端口已经有 server 监听，当前实例会复用已有 server，不再启动第二个进程。这可以避免工具启动预拉起一次、首次模型请求又拉起一次的问题。

注意：如果端口上已有外部或历史遗留的 `llama-server`，qt-llm 不会强行杀掉它，只会复用该端口。需要切换模型时，应先停止旧 server 或改用新的端口。

## 常见状态

- runtime root 未找到。
- 可执行文件未找到。
- 模型目录为空。
- 选择的模型文件不存在。
- server 端口启动超时。

这些状态应通过 provider 可用性字段展示给用户。
