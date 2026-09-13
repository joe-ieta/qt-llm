# 对外开发接口与集成指南

## 1. 集成原则

qt-llm 对外提供分层接口。调用方应选择满足需求的最高层入口，不应跨层拼装内部对象。

| 层级 | 入口 | 适用场景 | CMake 目标 |
| --- | --- | --- | --- |
| A | `RuntimeFacade` / `RuntimeRequestHandle` | 普通宿主、并行请求、取消、模型与运行状态 | `QtLlm::Conversation` |
| B | `ConversationClient` | 多会话、历史、快照和可选持久化 | `QtLlm::Conversation` |
| B | `ToolEnabledChatEntry` | 工具调用与 MCP 聊天 | `QtLlm::Conversation` |
| B | `QtLLMClient` | 高级请求、Provider 和传输生命周期控制 | `QtLlm::Conversation` |
| C | Provider、HTTP、runtime、repository 具体类型 | 扩展、诊断和基础库内部开发 | 对应组件或 `QtLlm::QtLlm` |

A/B 级接口是外部应用优先使用的兼容面。C 级接口保持可用，但调用方需要承担更多生命周期和配置责任。

## 2. CMake 接入

完整宿主能力：

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Conversation)
target_link_libraries(my_app PRIVATE QtLlm::Conversation)
```

轻量组件：

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Core Diagnostics Tools LocalRuntime)
target_link_libraries(my_app PRIVATE QtLlm::Core QtLlm::Diagnostics)
```

既有工程可继续使用 `QtLlm::QtLlm`。不要在同一目标同时链接聚合目标和它已经包含的组件。

## 3. 推荐请求模型

新宿主使用 `RuntimeFacade::sendAsync()`。每次调用返回独立 `RuntimeRequestHandle`：

- `requestId()` 用于关联流、日志和终态。
- `state()` 表示请求状态。
- `cancel()` 只取消该句柄对应的请求。
- `tokenReceived` 与 `reasoningTokenReceived` 分离内容和推理增量。
- `streamReset` 要求 UI 丢弃失败重试产生的临时文本。
- `finished(ChatResult)` 是句柄唯一终态。

句柄默认以 facade 为父对象。需要终态确认时，不要直接销毁活动句柄；先调用 `cancel()` 并等待 `finished()`。

## 4. 结果与错误

`ChatResult` 同时保留兼容字段和统一结果：

- `success`：请求是否成功。
- `canceled`：是否由取消结束。
- `requestId`：请求关联 ID。
- `error`：结构化类别、错误码、诊断和可重试信息。
- `response`：底层 `LlmResponse`，包含助手消息、结束原因和结构化输出。

业务逻辑不得解析错误文案，应使用 `error.category`、`error.code` 和 `retryable`。

## 5. 数据和存储

- `RuntimeFacade` 不替宿主持久化业务会话。
- `ConversationClient` 可无存储运行，也可使用 `ConversationRepository`。
- ToolsInside、ToolStudio、MCP 配置和文件日志都是显式启用的可选存储。
- API key、业务数据库和产品账号仍由宿主管理。

## 6. 线程与生命周期

- 网络请求异步执行，不要在 UI 线程使用 `sendBlocking()` 新增代码。
- Qt 对象遵循父子对象和所属线程规则。
- 事件接收端同步运行，应短时、非阻塞并自行保证线程安全。
- 工具执行默认顺序；启用并行后，执行器、钩子和接收端必须支持并发。
- 共享本地运行时由租约控制，宿主释放自己的租约不能停止其他调用方正在使用的实例。

## 7. 兼容与版本

- Qt5 与 Qt6 使用相同目标名和公开包含路径，但二进制产物不可混用。
- STATIC 与 SHARED 使用相同上层 CMake 代码。
- 静态导出定义由 CMake 目标自动传递；宿主不得定义 `QTLLM_*_LIBRARY`。
- 小版本不删除现有 A/B/C 入口，不改变既有默认值和序列化键。
- 发布包可通过 `find_package(QtLlm <版本> EXACT CONFIG REQUIRED)` 执行精确版本校验。

## 8. 进一步阅读

- [Host App 集成](./host-app-guide.md)
- [ConversationClient](./conversationclient-guide.md)
- [QtLLMClient](./qtllmclient-guide.md)
- [本地 llama.cpp](./local-llamacpp-guide.md)
- [工具与 MCP](./tool-mcp-guide.md)
- [API 索引](../50-reference/api-index.md)
- [CMake 组件](../30-development/cmake-components.md)
