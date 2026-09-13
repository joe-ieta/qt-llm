# qt-llm 公共契约与兼容基线

- 工作包：QTL-00
- 状态：已完成
- 基线日期：2026-09-13
- qt-llm 基线：`v0.2.10` / `84af90c`
- 代表性宿主：`E:\CodexDev\CoReader`

## 1. 目的与边界

本文冻结 qt-llm 当前可观察接口和行为，为后续架构优化提供兼容边界。本文不是新 API 设计，不把 CoReader 业务模型下沉到 qt-llm，也不承诺当前所有公开头文件都永久保持同等稳定性。

后续工作包必须满足以下约束：

- 已被外部宿主使用的接口不得直接删除、改名或改变既有语义。
- 新能力优先通过新增类型、方法、信号或适配层提供。
- 现有弱契约可以增强，但增强不得让旧调用路径失效。
- 需要替换的过渡接口必须先提供替代入口、迁移说明和至少一个兼容版本周期。
- Qt5 与 Qt6 使用相同源码契约，但二进制产物分别构建和分发。

## 2. 兼容等级

| 等级 | 定义 | 变更规则 |
| --- | --- | --- |
| A | 已被 CoReader 或代表性宿主直接使用 | 保持源码与行为兼容；只能增量扩展或走弃用流程 |
| B | qt-llm 面向应用提供的公共入口，当前未在 CoReader 直接使用 | 保持源码兼容；允许在评审后补充更清晰的替代入口 |
| C | 实现型接口被宿主直接引用，属于过渡兼容面 | 替代能力落地前保持可用；不得继续扩大宿主依赖 |
| D | 内部实现细节 | 可重构，但不得改变 A/B/C 级可观察行为 |

当前库以静态目标交付，公共头文件尚无独立导出宏和稳定 ABI 承诺。QTL-01 负责建立正式命名空间 target、安装包和公共头文件边界；在此之前，本基线主要约束源码兼容与可观察行为。

## 3. 核心入口清单

### 3.1 `qtllm::QtLLMClient`，等级 A

头文件：`src/qtllm/core/qtllmclient.h`

| 类别 | 当前入口 |
| --- | --- |
| 配置 | `setConfig(const LlmConfig&)` |
| Provider | `setProvider(...)`、`setProviderByName(...)` |
| 工具循环 | `setToolCallOrchestrator(...)`、`setToolLoopContext(...)` |
| 请求 | `sendPrompt(...)`、`sendRequest(...)` |
| 取消 | `cancelCurrentRequest()` |
| 流式信号 | `tokenReceived(...)`、`reasoningTokenReceived(...)` |
| 终态相关信号 | `completed(...)`、`responseReceived(...)`、`errorOccurred(...)` |
| 诊断信号 | `providerPayloadPrepared(...)` |

兼容要求：CoReader 直接依赖配置、结构化请求、内容/推理流、完成、错误和取消。现有调用方把 `completed` 或 `errorOccurred` 当作终态，但当前接口没有形式化“恰好一个终态”契约；QTL-02 必须在保留旧信号的前提下补强。

### 3.2 `qtllm::host::RuntimeFacade`，等级 A

头文件：`src/qtllm/host/runtimefacade.h`

| 类别 | 当前入口 |
| --- | --- |
| 配置 | `setProfile(...)`、`profile()` |
| 本地模型 | `listLocalModels(...)`、`refreshRuntimeAvailability(...)` |
| 请求 | `send(...)`、`sendBlocking(...)` |
| 取消 | `cancel()` |
| 流式信号 | `tokenReceived(...)`、`reasoningTokenReceived(...)` |
| 终态相关信号 | `completed(ChatResult)`、`errorOccurred(ChatResult)` |
| 诊断信号 | `providerPayloadPrepared(...)`、`runtimeStatusChanged(...)` |

兼容要求：CoReader 使用 `RuntimeProfile`、本地模型枚举、运行状态刷新、Provider 载荷观察和阻塞请求。`sendBlocking(request, 0)` 使用 Profile 超时；显式正值优先于 Profile 超时。

### 3.3 `qtllm::chat::ConversationClient`，等级 B

头文件：`src/qtllm/chat/conversationclient.h`

当前公开能力包括配置与 Provider、会话创建/切换、历史设置与清理、普通或带工具消息发送、快照导入导出，以及内容流、推理流、完成、响应、错误、请求准备、载荷准备和状态变更信号。

兼容要求：保留现有会话 ID、历史与快照入口。QTL-04 和 QTL-08 可以增加统一请求句柄与宿主自管历史模式，不得强制现有调用方迁移后才能继续工作。

### 3.4 `qtllm::tools::ToolEnabledChatEntry`，等级 B

头文件：`src/qtllm/tools/toolenabledchatentry.h`

当前公开能力包括发送消息、注入工具选择器/适配器/执行层/策略库/MCP 组件、设置追踪上下文和直接执行工具调用；对外信号包括内容流、推理流、完成、错误、工具选择结果和工具 Schema。

兼容要求：QTL-06 增加注册式宿主工具和外部执行模式时，现有内执行模式必须保持可用。

### 3.5 配套公共入口

| 接口 | 等级 | 当前作用 |
| --- | --- | --- |
| `ConversationClientFactory` | B | 获取、缓存、查找和持久化多客户端会话 |
| `ProviderFactory` | B | 按名称创建内置 Provider |
| `LlmToolRegistry` | B | 注册、移除、查询和恢复工具目录 |
| `QtLlmLogger` / `SignalLogSink` | A | 全局日志入口与 Qt 信号接收端 |
| `ManagedLlamaCppRuntime` | C | 本地运行时发现、模型枚举、启动、就绪检查和停止 |
| `builtInTools()` 等自由函数 | C | 当前时间、天气内置工具兼容面 |

## 4. 公共数据契约与默认值

### 4.1 核心请求类型

| 类型 | 当前字段语义 |
| --- | --- |
| `LlmMessage` | `role`、`content`，以及工具消息的名称、调用 ID 和调用列表 |
| `LlmRequest` | 消息列表、可选模型、`stream=true`、OpenAI 兼容工具 Schema 数组 |
| `LlmResponse` | 文本、`success=false`、错误消息、结构化助手消息和结束原因 |
| `LlmStreamDelta` | `channel="content"` 与增量文本 |
| `LlmToolCall` | 调用 ID、名称、JSON 参数、`type="function"` |

### 4.2 `LlmConfig` 与 `RuntimeProfile`

两者当前保持近似镜像，Profile 额外提供 `workspaceRoot`。下列默认值属于行为兼容面：

| 字段 | 默认值 |
| --- | --- |
| `stream` | `true` |
| `providerAvailable` | `true` |
| `timeoutMs` | `60000` |
| `maxRetries` | `0` |
| `retryDelayMs` | `400` |
| `llamaCppServerPort` | `18080` |
| `llamaCppContextSize` | `4096` |
| `llamaCppGpuLayers` | `-1`，表示自动卸载 |
| `llamaCppThreadCount` | `0`，表示自动选择 |
| `llamaCppStartupTimeoutMs` | `180000` |
| `llamaCppGpuMode` | `auto` |
| `llamaCppPerformanceProfile` | `balanced` |
| `llamaCppContextMode` | `auto` |

`ChatRequest` 当前承载 system/user prompt、client/session/trace ID 和宿主 metadata；`ChatResult` 返回成功标记、文本、Provider、模型、关联 ID、字符串错误码/错误消息和 metadata。后续统一请求模型必须能够无损表达这些字段。

### 4.3 工具类型

`LlmToolDefinition` 的稳定字段包括工具 ID、调用名、显示名、描述、输入 Schema、能力标签、类别、内置标记、可移除标记和启用标记。默认类别为 `custom`，默认非内置、可移除、启用。

`ToolExecutionContext` 已包含 client/session/request/trace 关联、Profile、LLM 配置、历史窗口和扩展 JSON。`ToolExecutionResult` 已包含成功标记、结构化输出、错误码、错误消息、耗时和可重试标记。QTL-06 应复用这些语义，不引入业务侧 DTO。

## 5. 当前行为基线

### 5.1 Provider 与协议

- Provider 名称在创建前执行去空白和小写归一化。
- 已有直接 Provider 包括 OpenAI、Ollama、llama.cpp 和 vLLM。
- OpenAI 兼容路由当前覆盖 `openai-compatible` 及 Anthropic、Google、DeepSeek、Qwen、GLM、Zhipu 等厂商别名。
- 未知 Provider 返回空实例，`QtLLMClient::setProviderByName` 通过错误信号报告不支持。
- 现有请求载荷、普通响应、SSE、JSON Lines、reasoning 增量和工具调用解析行为属于兼容面。

### 5.2 请求生命周期

- 请求以异步信号为主要返回方式；`RuntimeFacade` 另提供阻塞包装。
- 内容与推理增量使用不同信号。
- CoReader 目前用本地事件循环等待 `completed` 或 `errorOccurred`，取消时主动退出事件循环。
- 当前接口没有公共 request handle，也没有公开的唯一终态枚举。
- 并发发送、取消后终态、重试期间事件和重复终态尚未形成完整公共契约，由 QTL-02 负责补齐。

### 5.3 受管 llama.cpp

- `ManagedLlamaCppRuntime` 提供默认布局、可用性刷新、模型枚举、启动、HTTP 就绪探测和停止。
- 默认端口为 `18080`；启动超时由配置控制。
- 已占用端口只有通过 HTTP 就绪探测后才视为可复用服务。
- 启动实例会等待 `/health`；特定不支持状态下可回退探测 `/v1/models`。
- CoReader 直接使用静态发现接口，并在应用退出时按端口和可执行文件路径双重确认后清理残留进程。
- 进程共享、所有权和跨客户端排队尚未形成公共服务契约，由 QTL-07 处理。

### 5.4 内置工具

- 当前内置工具 ID 为 `current_time` 和 `current_weather`。
- 内置工具类别为 `builtin`，`systemBuiltIn=true`，`removable=false`，默认启用。
- CoReader 当前提供同名兼容垫片，因此这些 ID、调用名和基本 Schema 在 QTL-06 提供正式注册/适配出口前不得改变。

### 5.5 ID

公共 ID 格式为 `<prefix>_<sortable-body>`。前缀仅允许小写字母和数字；进程内生成顺序单调递增。当前前缀为：

| 类型 | 前缀 | 类型 | 前缀 |
| --- | --- | --- | --- |
| Client | `cli` | Session | `ses` |
| Trace | `trc` | Request | `req` |
| Span | `spn` | Event | `evt` |
| ToolCall | `tcl` | Artifact | `art` |
| SupportLink | `lnk` | Workspace | `wsp` |
| Node | `nod` | Placement | `plc` |
| Package | `pkg` | Task | `tsk` |
| Queue | `que` | | |

### 5.6 默认存储位置

以下路径均相对进程当前目录，除非调用方显式提供根目录或 `workspaceRoot`：

| 能力 | 默认位置 |
| --- | --- |
| 会话快照 | `.qtllm/<uid>.json` |
| 工具目录 | `.qtllm/tools/catalog.json` |
| MCP 服务配置 | `.qtllm/mcp/servers.json` |
| 客户端工具策略 | `.qtllm/clients/<clientId>/tool_policy.json` |
| Tool Studio | `.qtllm/tools/studio/` |
| Tools Inside 索引 | `.qtllm/tools_inside/index.db` |
| Tools Inside 附件 | `.qtllm/tools_inside/artifacts/` |
| 文件日志 | `<workspaceRoot 或当前目录>/.qtllm/logs/<clientId>/` |

文件日志默认单文件上限 5 MiB、每客户端最多 20 个文件，空客户端 ID 归入 `_system`。路径和序列化字段的修改必须提供迁移或兼容读取。

## 6. CoReader 实际集成映射

| CoReader 场景 | qt-llm 依赖面 | 兼容等级 | 证据位置 |
| --- | --- | --- | --- |
| 桥接库构建 | 直接收集 qt-llm 源文件和头文件编入 `coreader_qtllm_bridge` | C | `qtllm-bridge/CMakeLists.txt` |
| 通用非流式模型调用 | `RuntimeProfile`、`ChatRequest`、`RuntimeFacade::sendBlocking` | A | `qtllm_direct_model_binding.cpp` |
| 流式模型调用与取消 | `QtLLMClient`、`LlmConfig`、`LlmRequest`、四类请求信号、取消 | A | `qtllm_direct_model_binding.cpp` |
| 文档翻译流 | `QtLLMClient`、内容/推理增量、完成和错误 | A | `document_translation_worker.cpp` |
| 本地模型设置 | `RuntimeFacade::refreshRuntimeAvailability`、`listLocalModels`、`profile` | A | `settings_dialog.cpp` |
| Provider 载荷观察 | `providerPayloadPrepared` | A | `qtllm_direct_model_binding.cpp` |
| 应用日志展示 | `QtLlmLogger`、`SignalLogSink`、`LogEvent` | A | `application_bootstrap.cpp`、`main_window.cpp` |
| 退出清理 | `ManagedLlamaCppRuntime` 静态发现和布局类型 | C | `managed_llama_cpp_process_cleanup.cpp` |
| 内置工具链接兼容 | `builtInTools`、`isBuiltInToolId`、`mergeWithBuiltInTools` | C | `qtllm_builtin_tools_shim.cpp` |
| 集成回归 | bridge contract 与 live translation 测试 | A | `tests/unit/qtllm_model_binding_contract_test.cpp`、`tests/unit/qtllm_live_translation_test.cpp` |

当前扫描未发现 CoReader 直接使用 `ConversationClient` 或 `ToolEnabledChatEntry`。这两类应继续保持通用能力，不得因 CoReader 未使用而删除，也不得引入阅读、翻译、文档或 Agent 业务概念。

## 7. Qt 与宿主兼容矩阵

| 组合 | 构建模式 | 当前证据 | 状态 |
| --- | --- | --- | --- |
| Qt 6.10.3 + MSVC 2022 x64 | qt-llm 独立 CMake Release | 核心库、应用、工具、Agent、测试全部构建；CTest 通过 | 已验证 |
| Qt 5.15.2 + MSVC 2022 x64 | qt-llm 独立 CMake | STATIC/SHARED、CTest、安装包与源码消费者均由当前发布门禁验证 | 已验证 |
| CoReader + Qt6 | 源码收集到 `coreader_qtllm_bridge` | 实际 CMake 和调用代码已映射 | 当前使用 |
| CoReader + 正式 qt-llm target | `add_subdirectory` 或 `find_package` | 尚未提供 | QTL-01 |
| CoReader + Qt5 | 不属于当前 CoReader 构建基线 | 无 | 不承诺 |

## 8. 测试可追溯性

基线测试文件：`tests/qtllm_tests/tst_qtllm.cpp`。`v0.2.10` Release 结果为 41 项通过、0 项失败、2 项因当前运行环境限制跳过。

| 行为域 | 已有覆盖 |
| --- | --- |
| Compact ID | 前缀、顺序、校验、解码、非法输入 |
| 会话与工厂 | 客户端 ID、会话 ID、持久化会话 ID |
| Provider | 已知 Provider、厂商别名、未知 Provider |
| llama.cpp | 补充模型目录、GPU 默认值、CPU-only、端口复用、HTTP 就绪等待 |
| OpenAI 兼容协议 | URL、载荷、Anthropic/Google、SSE、reasoning、工具调用、Ollama JSON Lines |
| OpenAI Responses | URL、工具清理、普通与流式函数调用 |
| MCP 与工具执行 | MCP 工具同步、按调用名执行 |
| 日志 | 按客户端轮转 |
| 流解析 | 分片输入和尾行处理 |

当前明确未覆盖或覆盖不足：

- `QtLLMClient` 和 `RuntimeFacade` 的唯一终态、取消后终态与并发请求策略。
- 结构化错误码在各 Provider、超时、取消、重试和本地运行失败间的一致性。
- `ConversationClient` 的完整会话切换、历史窗口与快照兼容矩阵。
- `ToolEnabledChatEntry` 的授权、外部执行、取消和重复调用语义。
- 默认存储布局的升级兼容测试。
- Qt5 构建与最小宿主消费测试。
- 两个被跳过的 MCP stdio/HTTP-like 真实传输冒烟测试。

## 9. 弃用规则

- 小版本不得删除 A/B/C 级入口或改变既有默认值。
- 替代接口发布时，旧入口至少保留一个明确发布周期，并在发布说明和迁移指南中标注。
- 旧信号继续发出；新增统一事件不得要求调用方同时监听两套入口才能完成请求。
- 字符串 Provider 名称、内置工具 ID、ID 前缀、错误码和序列化键视为协议数据，修改时必须提供兼容映射。
- 公共结构体新增字段应提供默认值；重排、改型和删除字段视为潜在 ABI/源码破坏。
- C 级接口只能在正式替代能力、CoReader 迁移和跨项目验证完成后收缩。
- 任何弃用不得把 CoReader 的翻译、阅读、工作区或 Agent 业务概念引入 qt-llm。

## 10. QTL-00 验收结论

- 四个核心入口、配套公共类型、默认值、信号和行为已映射到源码。
- 当前存储路径、ID、Provider、内置工具与受管运行时行为已形成冻结清单。
- 关键行为已关联现有测试，缺口已明确归入后续工作包。
- CoReader 当前构建、调用、日志、本地模型、清理和兼容垫片依赖均已映射。
- QTL-01、QTL-02、QTL-03 可以在本基线约束下并行启动。

## 11. 当前开发线复核（2026-09-13）

本文件前述接口、默认值和 v0.2.10 基线提交用于兼容追溯。其后的 QTL-01 至 QTL-10 已以增量方式完成，未删除既有入口；新增请求句柄、结构化输出、上下文窗口、工具治理、运行实例服务、事件接收端和 CMake 组件详见当前集成文档。

当前发布前矩阵为 Qt5/Qt6 × STATIC/SHARED 四组，每组 CTest 6/6；安装包消费者 20/20；`add_subdirectory` 消费者 4/4；公开头文件安装与编译检查通过。当前开发线尚未创建 v0.2.10 之后的新正式版本。
