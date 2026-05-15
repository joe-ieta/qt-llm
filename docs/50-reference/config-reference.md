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
