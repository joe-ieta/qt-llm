# 项目定位与开发集成指南

## 项目定位（面向外部集成）

qt-llm 是面向 Qt/C++ 的 LLM 集成底座。

它承担外部 Qt App 不想重复实现的横切能力：

- provider 兼容层（OpenAI/OpenAI-compatible、Ollama、vLLM、llama.cpp）
- 请求执行与流式 token 处理
- 运行态生命周期（本地 runtime 启停、端口检测、模型发现、运行规划）
- 会话与对话持久化支撑（可选）
- 工具调用与 MCP 集成（可选）
- 可观测数据（trace、事件、artifact、错误/状态）

外部 Host App 的目标是“接入即用”，不应把 provider 内部差异上推到上层 UI。

## 对外使用边界（推荐）

按能力复杂度从低到高分层接入：

- `RuntimeFacade`：推荐，适合大多数 Host App，负责 provider 映射、模型发现、可用性字段和请求执行入口。
- `ConversationClient`：适合聊天产品，复用 `RuntimeFacade` + `QtLLMClient` 的会话能力，处理多 session、历史、snapshot 与持久化。
- `QtLLMClient`：适合高级能力调用方，需要明确管理配置完整性、生命周期和错误策略时使用。
- `ToolEnabledChatEntry` / MCP 层：仅在需要工具调用或 MCP 场景使用。

## 外部项目接入流程（最小成功路径）

1. 在外部 Qt 工程的 CMake 中添加对 `qt-llm` 的链接（当前仓库已支持 CMake，并可在 Qt5/Qt6 间切换）。
2. 选用 `RuntimeFacade`，只保留以下职责在 Host App：
   - 用户与配置持久化
   - 模型与 profile 选择后传给 `RuntimeFacade`
   - 渲染 token、状态、错误和可用性信息
3. 统一调用路径：

```cpp
auto *runtime = new qtllm::host::RuntimeFacade(this);

qtllm::host::RuntimeProfile profile;
profile.providerName = QStringLiteral("llama-cpp");
profile.model = selectedModelId;
profile.llamaCppModelPath = selectedModelPath;
runtime->setProfile(profile);

connect(runtime, &qtllm::host::RuntimeFacade::tokenReceived, ...);
connect(runtime, &qtllm::host::RuntimeFacade::runtimeStatusChanged, ...);
connect(runtime, &qtllm::host::RuntimeFacade::completed, ...);

qtllm::host::ChatRequest req;
req.userPrompt = input;
runtime->send(req);
```

4. 使用 `runtime->listLocalModels()` 获取托管模型目录聚合列表，不要在 Host App 重复实现扫描规则。
5. 对高阶能力（会话、工具、MCP）分别切换到 `ConversationClient` 或 `ToolEnabledChatEntry`。

## 集成时的稳定性要求（必读）

- 优先使用 Qt 原生信号/槽处理异步状态，不在 UI 线程做网络阻塞。
- 不要把 `ProviderFactory`、`HttpExecutor`、`ManagedLlamaCppRuntime` 的生命周期硬塞到 UI 中。
- 统一使用 `providerAvailable`、`providerAvailabilityStatus`、`providerAvailabilityMessage` 等可用性字段做运行提示，用户可见“不可用原因”而非空白失败。
- 遇到问题以文档为准更新链条：配置字段、启动参数、错误码与状态码保持同步。

## 与源码结构的对应关系

- `src/qtllm/`：稳定 API 与运行时核心（可复用）
- `src/apps/`：示例/工具入口（仅参考）
- `src/agents/`：业务 Agent 参考，不作核心能力承载
- `tests/`：关键行为回归，优先保证 `qtllm_tests` 可复跑

## CMake 与版本兼容

本项目当前构建基线是：

- C++17
- Qt 5.x/6.x 双版本可编译（当前通过 CMake 自动匹配）
- Windows 首先验证，保持 Linux 兼容性

建议默认使用仓库根目录 `target_wrapper.bat` 统一注入 Qt 路径后再配置和构建。

## 文档分区的开发阅读顺序

- 先读：`docs/10-architecture/overview.md`、`docs/30-development/repository-structure.md`
- 再读：`docs/20-integration/host-app-guide.md`
- 再读：`docs/20-integration/conversationclient-guide.md` / `docs/20-integration/tool-mcp-guide.md`（按需）
- 最后：`docs/30-development/build-and-test.md` 与 `docs/50-reference/troubleshooting.md`
