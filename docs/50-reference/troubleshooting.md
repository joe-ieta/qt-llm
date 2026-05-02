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

## Qt Creator 编译异常

先重新运行 qmake。涉及 `Q_OBJECT` 的新类或移动文件后，旧 moc 生成物可能失效。

## 文档和代码不一致

以代码为准，并同步更新对应文档。历史文档在 `docs/archive/`，不作为当前状态依据。
