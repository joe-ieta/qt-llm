# Provider 与 Runtime

## Provider

当前 provider factory 支持：

- `openai`
- `openai-compatible`
- `ollama`
- `vllm`
- `llama-cpp`
- 若干 OpenAI-compatible vendor alias，例如 `sglang`、`anthropic`、`google`、`gemini`、`deepseek`、`qwen`、`glm`、`zhipu`

provider 负责协议差异，Host App 不应复制 provider payload 逻辑。

## Managed llama.cpp

`ManagedLlamaCppRuntime` 负责：

- 判断 provider 是否属于托管 llama.cpp。
- 解析运行态目录。
- 发现 `llama-server`。
- 发现 `models/*.gguf`。
- 选择模型路径。
- 启动本地 server 并等待端口可用。
- 更新 `LlmConfig` 的可用性状态。

运行态目录会根据候选评分选择最合适目录。只有空目录的 `llama-cpp-runtime` 不应遮挡后续包含可执行文件和模型的共享目录。

## 可用性状态

`LlmConfig` 中与可用性相关的字段：

- `providerAvailable`
- `providerAvailabilityStatus`
- `providerAvailabilityMessage`
- `resolvedRuntimeRoot`
- `resolvedModelPath`
- `localModelCount`

配置界面应展示这些字段，而不是只在调用失败后显示普通网络错误。
