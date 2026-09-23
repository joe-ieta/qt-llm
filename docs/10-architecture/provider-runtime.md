# Provider 与本地运行时

## Provider 边界

Provider 负责协议 URL、请求载荷、普通响应和流式响应的差异，不负责 UI、业务提示词、会话数据库或工作流编排。

当前 `ProviderFactory` 支持：

- `openai`
- `openai-compatible`
- `ollama`
- `vllm`
- `llama-cpp`
- 使用 OpenAI-compatible 路由的厂商别名，例如 `sglang`、`anthropic`、`google`、`gemini`、`deepseek`、`qwen`、`glm` 和 `zhipu`

普通宿主通过 `RuntimeFacade` 选择 Provider；只有高级集成或测试直接使用 `ProviderFactory` 与具体 Provider。

## 运行时边界

`ManagedLlamaCppRuntime` 负责单个 llama.cpp 运行实例的布局解析、模型发现、启动参数规划、进程启动、健康检查和停止。

`ManagedLlamaCppRuntimeService` 在它之上提供：

- 相同运行条件下的实例共享。
- 每个调用方独立租约。
- 同端口不同实例的排队。
- 启动、排队和总时限区分。
- 获取阶段取消。
- 库自有进程与外部已有服务的所有权区分。

`RuntimeFacade` 默认持有共享服务；宿主也可注入更大生命周期的服务。直接使用旧 `QtLLMClient` 且不注入服务时，仍保持原有私有运行时行为。

## 模型目录与选择

默认 runtime 位于 `<applicationDir>/llama-cpp-runtime`，内置模型位于 `<runtimeRoot>/models`。模型目录还可以合并环境根目录和平台共享位置下的 `qtllm/models`。

模型解析顺序：

1. 显式 `llamaCppModelPath`。
2. `model` 与托管模型目录匹配。
3. 只有一个可用 `.gguf` 时选择该模型。

宿主应展示 `listLocalModels()` 返回的目录，不应复制搜索和合并规则。

## 可用性与诊断

主要字段包括 `providerAvailable`、`providerAvailabilityStatus`、`providerAvailabilityMessage`、`resolvedRuntimeRoot`、`resolvedModelPath` 和 `localModelCount`。宿主配置界面应展示这些状态，而不是等请求失败后只显示通用错误。

## 响应用量

OpenAI-compatible 的普通与流式响应会解析 Provider 上报的 `usage`，写入 `LlmResponse::usage`；流式请求通过 `stream_options.include_usage` 要求末尾用量块。

用量缺失表示未知：`LlmUsage::available` 为 `false`，token 计数不能按 0 解释。OpenAI Responses 路径当前不填充用量，同样保持 `available = false`。

## 非目标

- 不自动决定宿主产品的安装目录和升级策略。
- 不管理 Ollama 或远端 Provider 进程。
- 不把文档、翻译、RAG 或 Agent 工作流放入运行时层。
