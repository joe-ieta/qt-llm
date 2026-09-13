# 故障处理

## 选择 llama-cpp 后模型列表为空

检查：

1. 是否存在有效 `llama-cpp-runtime`。
2. `models/` 下是否有 `.gguf` 文件。
3. 是否有空的程序目录 `llama-cpp-runtime` 遮挡共享 runtime。
4. 环境变量目录是否正确。
5. Windows 下 `E:\LLMs\llama-cpp-runtime` 这类共享目录是否存在。

当前实现会按候选评分选择更完整的 runtime，空目录不应遮挡有效共享目录。

## provider 不可用

查看：

- `providerAvailable`
- `providerAvailabilityStatus`
- `providerAvailabilityMessage`
- `resolvedRuntimeRoot`
- `resolvedModelPath`
- `localModelCount`

## llama-server 启动失败

检查：

- 可执行文件是否存在。
- 端口是否被占用。
- 模型路径是否存在。
- GPU layers、context size、extra args 是否被当前二进制支持。

## llama.cpp 仍然只跑 CPU

检查：

1. `llamaCppGpuLayers` 是否被 Host App 显式设为 `0`。
2. `llamaCppExtraArgs` 是否手动覆盖了 `--gpu-layers`、`--n-gpu-layers` 或 `-ngl`。
3. 当前 `llama-server` 是否是 GPU-capable build。
4. `llama-cpp-runtime/bin` 下是否存在 `ggml-cuda`、`ggml-vulkan`、`ggml-hip`、`ggml-sycl` 等 GPU backend 库。
5. `llama-server --list-devices` 是否能列出 GPU 设备。
6. llama.cpp 启动日志中是否报告 GPU backend 可用。

默认情况下，`llamaCppGpuLayers = -1` 会被解释为 auto，并传入 `--gpu-layers 999`。如果当前 runtime 只有 CPU backend，传参不会让它变成 GPU build，需要替换为包含 GPU backend 的 llama.cpp runtime。

## 多个 llama-server 遗留

检查：

1. 是否有历史遗留进程占用 `18080`。
2. 是否同时启动了多个使用 managed `llama-cpp` 的工具 App。
3. 是否有外部手工启动的 `llama-server`。

qt-llm 的 managed runtime 会在启动前检查目标端口，端口已有 server 时不再启动新进程。工具 App 通过 `aboutToQuit` 停止自己托管的进程，但不会强制关闭外部或历史遗留进程。

## Qt Creator 编译异常

先清理并重新配置 CMake。涉及 `Q_OBJECT` 的新类或移动文件后，先 rerun configure 以便重建 moc/rcc 产物。

## 文档和代码不一致

以代码为准，并同步更新对应文档。历史文档在 `docs/archive/`，不作为当前状态依据。

## 找不到 QtLlm CMake 包

检查：

1. `CMAKE_PREFIX_PATH` 是否指向安装前缀，而不是单个库文件。
2. 或者 `QtLlm_DIR` 是否指向 `<安装目录>/lib/cmake/QtLlm`。
3. Qt 主版本是否与 qt-llm 产物一致。
4. STATIC/SHARED 产物是否来自当前构建目录对应的安装结果。
5. 宿主是否使用 `QtLlm::Conversation` 等正式目标，而不是收集内部源码。

## Windows DLL 启动失败

将 qt-llm 安装目录的 `bin` 和对应 Qt 的 `bin` 放入运行时 `PATH`。不要混用 Qt5/Qt6 或 Debug/Release DLL。编译成功但程序无法启动通常是运行时搜索路径问题，不是头文件或 CMake 包发现问题。

## 发布验证失败

查看 `build-release-verification/logs` 或传入 `-BuildRoot` 下的 `logs`。脚本只对 `qtllm_tests.exe` 的已知单次 LNK1104 文件占用执行一次定向重试；其他编译、链接、测试、文档或版本错误都需要实际修复。
