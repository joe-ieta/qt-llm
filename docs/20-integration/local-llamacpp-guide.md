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

## 自动运行规划

managed `llama.cpp` 的默认集成模型是：Host App 选择 provider 和模型，qt-llm 根据模型文件、运行态目录和当前策略派生 `llama-server` 启动参数。Host App 不应把 `llamaCppGpuLayers`、`llamaCppThreadCount`、`llamaCppContextSize` 当成主要产品配置项暴露给普通用户。

推荐优先使用高层策略字段：

- `llamaCppGpuMode`：`auto`、`cpu-only`、`prefer-gpu`、`explicit`。
- `llamaCppPerformanceProfile`：`conservative`、`balanced`、`aggressive`。
- `llamaCppContextMode`：`auto`、`explicit`。

当前规划规则：

- `auto` / `prefer-gpu` 在检测到 GPU backend 库时注入 `--gpu-layers 999`，否则注入 `--gpu-layers 0` 并写入 warning。
- `cpu-only` 总是注入 `--gpu-layers 0`。
- `explicit` 或非负 `llamaCppGpuLayers` 使用显式层数。
- `llamaCppThreadCount > 0` 时作为显式线程数；否则按 performance profile 和 `QThread::idealThreadCount()` 自动派生。
- `llamaCppContextMode=auto` 时按模型文件大小和 performance profile 派生 `--ctx-size`；`explicit` 时使用 `llamaCppContextSize`。

如果 `llamaCppExtraArgs` 中已经包含 `--gpu-layers`、`--gpu-layers=<n>`、`--n-gpu-layers`、`--n-gpu-layers=<n>` 或 `-ngl`，runtime 不会再注入 GPU 参数。`--threads`、`-t`、`--ctx-size`、`-c` 也遵循同样的专家覆盖规则。

运行后可读取诊断字段：`resolvedLlamaCppGpuMode`、`resolvedLlamaCppGpuLayers`、`resolvedLlamaCppThreadCount`、`resolvedLlamaCppContextSize`、`runtimePlanSummary`、`runtimePlanWarnings`。配置 UI 应优先展示这些解析结果，而不是要求用户理解底层启动参数。

注意：当前自动规划只基于运行态中是否存在 GPU backend 库、模型文件大小和 CPU 线程数做保守推断；精确 VRAM/RAM 探测和按量化格式估算仍属于后续增强。


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
