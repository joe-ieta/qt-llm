# 本地 llama.cpp 托管集成

## 定位

qt-llm 可以通过 `llama-cpp` Provider 托管 `llama-server`。普通宿主使用 `RuntimeFacade` 和模型目录接口，不应复制 runtime 搜索、模型合并、启动参数和健康检查逻辑。

## 组件选择

- 通过 `RuntimeFacade` 使用本地模型：链接 `QtLlm::Conversation`。
- 直接使用运行实例和协调服务：链接 `QtLlm::LocalRuntime`。
- 兼容完整接入：链接 `QtLlm::QtLlm`。

## Runtime 布局

首选应用本地目录：

```text
<applicationDir>/llama-cpp-runtime/
  bin/
    llama-server.exe
  models/
    *.gguf
  logs/
```

显式设置 `llamaCppRuntimeRoot` 或 `llamaCppExecutablePath` 时，显式值优先。runtime 的下载、安装、升级和签名校验属于部署能力，当前库不自动完成。

## 模型目录

托管模型目录会合并：

1. `<runtimeRoot>/models` 中的内置模型。
2. 应用目录、环境根目录和平台共享根下发现的 `qtllm/models`。

宿主调用 `RuntimeFacade::listLocalModels()` 或 `ModelCatalogService` 获得结果。不要自行扫描磁盘并复制排序、去重和路径优先级。

模型解析顺序：

1. 显式 `llamaCppModelPath`。
2. `model` 与托管目录中的 ID 匹配。
3. 仅发现一个 `.gguf` 时自动选择。

## 运行规划

推荐宿主暴露高层策略：

- `llamaCppGpuMode`：`auto`、`cpu-only`、`prefer-gpu`、`explicit`。
- `llamaCppPerformanceProfile`：`conservative`、`balanced`、`aggressive`。
- `llamaCppContextMode`：`auto`、`explicit`。

qt-llm 根据策略、模型大小、GPU backend 和 CPU 线程派生 `--gpu-layers`、`--threads` 和 `--ctx-size`。`llamaCppExtraArgs` 及显式数值是专家覆盖，存在同名参数时不会重复注入。

## 实例、租约与所有权

`ManagedLlamaCppRuntimeService` 对相同运行条件共享实例并返回租约：

- `LibraryOwned`：由库启动，最后一个租约释放后停止。
- `ExternalService`：端口已有健康服务，释放租约不会关闭外部进程。
- 同端口的不同实例请求排队，区分排队、启动和总时限。
- 获取阶段可通过稳定 `requestId` 取消。

`RuntimeFacade` 默认使用共享服务；多个 facade 需要共享时，宿主可注入同一服务实例。

## 可用性和故障

宿主应展示 `providerAvailable`、`providerAvailabilityStatus`、`providerAvailabilityMessage`、`resolvedRuntimeRoot`、`resolvedModelPath`、`localModelCount` 以及运行规划诊断字段。

端口被占用时，只有 HTTP 健康探测通过才复用现有服务。启动实例会检查 `/health`，必要时回退 `/v1/models`。不要只根据端口打开就判定服务可用。

## 非目标

- 不提供模型管理 UI。
- 不自动下载或升级 runtime。
- 不管理 Ollama 和远端 Provider 进程。
- 不决定宿主产品的部署目录和业务模型选择策略。
