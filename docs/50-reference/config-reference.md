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

- `llamaCppRuntimeRoot`
- `llamaCppExecutablePath`
- `llamaCppModelPath`
- `llamaCppExtraArgs`
- `llamaCppServerPort`
- `llamaCppContextSize`
- `llamaCppGpuLayers`：`< 0` 表示 auto GPU offload，`0` 表示禁用，`> 0` 表示显式层数。
- `llamaCppThreadCount`
- `llamaCppStartupTimeoutMs`

managed `llama-cpp` 中，auto 模式会传入 `--gpu-layers 999`。如果 `llamaCppExtraArgs` 已包含 `--gpu-layers`、`--n-gpu-layers` 或 `-ngl`，runtime 不再注入该参数。

## 可用性字段

- `providerAvailable`
- `providerAvailabilityStatus`
- `providerAvailabilityMessage`
- `resolvedRuntimeRoot`
- `resolvedModelPath`
- `localModelCount`

配置 UI 应展示可用性字段，让用户知道当前 provider 为什么能用或不能用。
