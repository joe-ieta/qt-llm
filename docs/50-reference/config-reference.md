# 配置参考

## LlmConfig / RuntimeProfile

常用字段：

- `providerName`：provider 名称。
- `baseUrl`：服务地址。
- `apiKey`：API key。
- `model`：模型 ID。
- `modelVendor`：provider 协议路由提示。
- `stream`：是否流式输出。
- `timeoutMs`：请求超时。
- `maxRetries`：重试次数。
- `retryDelayMs`：重试间隔。

## llama.cpp 字段

基础路径和启动字段：

- `llamaCppRuntimeRoot`
- `llamaCppExecutablePath`
- `llamaCppModelPath`
- `llamaCppExtraArgs`
- `llamaCppServerPort`
- `llamaCppStartupTimeoutMs`

高层运行策略字段：

- `llamaCppGpuMode`：`auto`、`cpu-only`、`prefer-gpu`、`explicit`，默认 `auto`。
- `llamaCppPerformanceProfile`：`conservative`、`balanced`、`aggressive`，默认 `balanced`。
- `llamaCppContextMode`：`auto`、`explicit`，默认 `auto`。

专家覆盖字段：

- `llamaCppContextSize`：`llamaCppContextMode=explicit` 时使用；否则作为兼容字段保留。
- `llamaCppGpuLayers`：`>= 0` 时作为显式 GPU offload 层数；`-1` 表示交给 `llamaCppGpuMode` 自动规划。
- `llamaCppThreadCount`：`> 0` 时作为显式线程数；否则自动规划。

派生诊断字段：

- `resolvedLlamaCppGpuMode`
- `resolvedLlamaCppGpuLayers`
- `resolvedLlamaCppThreadCount`
- `resolvedLlamaCppContextSize`
- `runtimePlanSummary`
- `runtimePlanWarnings`

managed `llama-cpp` 启动前会先生成 launch plan，再注入 `--gpu-layers`、`--threads`、`--ctx-size`。如果 `llamaCppExtraArgs` 已包含对应参数，extra args 优先，runtime 不再重复注入。

## 可用性字段

- `providerAvailable`
- `providerAvailabilityStatus`
- `providerAvailabilityMessage`
- `resolvedRuntimeRoot`
- `resolvedModelPath`
- `localModelCount`

配置 UI 应展示可用性字段，让用户知道当前 provider 为什么能用或不能用。

## 请求级配置

`ChatRequest` 支持简单 `systemPrompt` / `userPrompt`，也支持宿主提供结构化 `messages`。两者同时使用时应由调用方明确组合意图，推荐多轮场景只使用结构化消息。

- `output`：Text、Json 或 JsonSchema 输出约束。
- `clientId`、`sessionId`、`traceId`：宿主关联标识；空值时库按入口规则生成或传递。
- `metadata`：宿主扩展 JSON，基础库不解释业务字段。

## 上下文预算

`MemoryPolicy` 的 `maxHistoryMessages`、`contextWindowTokens`、`reservedOutputTokens` 和 `minimumRecentTurns` 控制 ConversationClient 请求窗口。token 预算默认关闭，保持旧调用行为。

## 配置责任

- API key 和用户设置由宿主安全持久化。
- 普通宿主设置 `RuntimeProfile`，不同时维护一份独立 `LlmConfig`。
- 只有高级 `QtLLMClient` 集成直接负责 `LlmConfig` 的完整性。
- 不应根据错误文案反推配置状态，应读取结构化错误和可用性字段。
