# 本地模型运行实例服务

## 定位

`ManagedLlamaCppRuntimeService` 是由宿主显式持有的 llama.cpp 实例协调服务。它位于模型安装、模型目录管理和业务请求编排之间，只负责运行实例的身份、启动、共享、所有权和释放。

既有 `ManagedLlamaCppRuntime` 与 `QtLlmClient` 接口保持可用。本服务是增量能力；`RuntimeFacade` 默认持有并使用共享服务，直接构造且未注入服务的 `QtLlmClient` 仍保持原有私有运行时行为。

## 统一请求路径

`RuntimeFacade` 在自身生命周期内持有一个共享服务，并把它注入同步客户端和每个异步 `RuntimeRequestHandle`。同一门面发起的并发本地模型请求因此共享实例，而每个请求仍持有独立租约和请求状态。

- 宿主可通过 `setManagedLlamaCppRuntimeService` 注入更大作用域的服务，让多个门面共享同一实例协调器。
- 请求发送前取得租约，服务解析后的地址和请求超时用于实际 HTTP 请求。
- 请求成功、失败或取消完成时释放自己的租约；句柄销毁也会清理未释放租约。
- 传入空服务会恢复一个新的门面自有服务，不产生无管理状态。
- 直接使用旧 `QtLlmClient` 且不注入服务时，继续使用单客户端私有运行时，避免改变既有宿主行为。

## 实例身份

实例身份由以下运行条件共同确定：

- 运行根目录、服务程序路径与模型路径。
- 服务端口。
- GPU、线程、上下文和性能策略。
- 额外启动参数。

相同身份的并发获取只启动一次，其他调用进入等待并共享就绪实例。不同身份不能同时占用同一端口，后续调用进入队列，直到当前实例最后一个租约释放。

## 租约与所有权

`acquire` 返回 `LlamaCppRuntimeLease`。每个调用者获得独立 `leaseId`，但相同实例的 `instanceId` 一致。

- `LibraryOwned`：qt-llm 启动进程。最后一个租约释放或服务销毁时关闭该进程。
- `ExternalService`：端口上已有可用服务。释放租约和销毁管理服务都不会关闭外部进程。
- 一个调用者释放自己的租约不会影响仍持有租约的调用者。

宿主应在不再使用实例时调用 `release(leaseId)`。服务本身应具有清晰、稳定的宿主生命周期。

## 超时

`LlamaCppRuntimeAcquireOptions` 将不同阶段分开：

- `queueTimeoutMs`：等待同实例启动或等待端口释放的上限。
- `startupTimeoutMs`：服务进程启动并达到 HTTP 就绪的上限。
- `requestTimeoutMs`：随租约返回，供后续统一请求路径采用。
- `totalTimeoutMs`：一次实例获取从进入到返回的总上限。

结果使用 `QueueTimedOut`、`StartupTimedOut` 和 `TotalTimedOut` 区分阶段，不用一个模糊超时覆盖所有失败。

## 取消

调用方为获取请求提供稳定 `requestId`，可从其他线程调用 `cancelAcquire(requestId)`：

- 排队中的获取立即返回 `Canceled`。
- 启动中的获取通过线程安全停止标记协作取消探活等待。
- 取消一个等待者不会中断仍有其他调用者需要的共享启动。

取消实例获取与取消一次 LLM 请求是两个不同边界。统一请求路径在尚未取得租约时调用 `cancelAcquire(requestId)`；取得租约后继续使用现有请求传输取消。请求进入唯一终态后只释放自己的租约，不会中断其他调用者。

## 线程要求

管理服务可被多个调用线程并发使用。实际 `QProcess` 和 `ManagedLlamaCppRuntime` 固定在服务内部工作线程创建、启动和销毁，调用线程不会直接跨线程操作进程对象。

服务析构时不应再有外部线程进入其方法。该要求与普通 C++ 对象析构并发规则一致。

## 非目标

- 不下载、安装或选择模型。
- 不提供模型管理 UI。
- 不决定端口、模型来源和部署策略。
- 不管理 Ollama、远程 OpenAI 兼容服务或业务侧 Agent 生命周期。
