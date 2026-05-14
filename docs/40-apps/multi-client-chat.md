# multi_client_chat

`multi_client_chat` 展示多 client、多 provider 和本地模型选择场景。

## 重点能力

- 管理多个 provider/client 配置。
- 支持本地内置 `llama-cpp`。
- 刷新本地 `.gguf` 模型列表。
- 用户从实际模型列表选择模型。
- 运行时状态应能反映 provider 是否可用。
- App 启动时会尝试拉起 managed `llama-server`，便于本地 provider 后续直接使用。

## 本地模型刷新

选择 `llama-cpp` 后，应通过 qt-llm 的模型发现逻辑获得模型列表。空的本地 `llama-cpp-runtime` 不应遮挡共享目录中的有效 runtime。
