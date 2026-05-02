# qt-llm 顶层约束

本文是 qt-llm 的顶层工程约束。后续代码、文档、示例和集成方案应优先遵守本文，再进入具体模块文档。

## 项目定位

qt-llm 是面向 Qt/C++ 应用的 LLM 集成基础库。项目的核心目标是把与 LLM 打交道的工作封装在 qt-llm 内部，让外部 Host App 只关注自身 UI、设置和业务状态。

项目不是 Web 优先的编排服务，也不是单一聊天窗口示例。它应提供可嵌入、可观察、可控制、可扩展的 Qt 本地集成能力。

## 核心要求

1. LLM 请求、provider 构建、流式解析、本地运行态托管和模型发现应优先由 qt-llm 负责。
2. 对外接口保持简单稳定，普通 Host App 优先使用 `qtllm::host::RuntimeFacade`。
3. 需要会话、历史和 profile 时使用 `ConversationClient`。
4. 需要工具调用、MCP 或工具执行链路时使用 `ToolEnabledChatEntry`。
5. 普通 Host App 不应直接依赖 `ProviderFactory`、`HttpExecutor`、`ManagedLlamaCppRuntime` 或具体 provider 实现。
6. 本地托管 provider，特别是内置 `llama.cpp`，必须提供可用性检测、模型发现、错误状态和运行态控制。
7. 执行过程必须可感知、可观测、可控制，不能只暴露最终文本。
8. UI、业务 Agent、工具管理和底层 LLM 运行逻辑保持分层。

## 文档职责

活跃文档使用中文维护，面向当前代码状态，不保存长期过程叙述。历史材料归档到 `docs/archive/`。

根目录文档职责：

- `README.md`：项目介绍、功能清单、快速使用方式。
- `AI_RULES.md`：顶层工程约束、文档职责、维护规则。
- `LICENSE`：许可证。

`docs/` 文档职责：

- `docs/README.md`：文档入口和阅读路径。
- `docs/00-project/`：项目定位、功能状态、路线图、当前待办、关键决策。
- `docs/10-architecture/`：技术架构、分层边界、provider/runtime、可观测性、身份与持久化。
- `docs/20-integration/`：Host App、核心 client、会话 client、本地 llama.cpp、工具/MCP 集成手册。
- `docs/30-development/`：构建测试、编码规范、仓库结构、发布检查。
- `docs/40-apps/`：示例 App 和参考 Agent 说明。
- `docs/50-reference/`：配置、API 索引、运行数据布局、故障处理。
- `docs/releases/`：发布说明索引和仍需保留的当前发布记录。
- `docs/archive/`：历史文档归档，不作为当前设计依据。

## 文档使用说明

使用者阅读顺序：

1. 先读 `README.md` 理解项目定位和能力。
2. 外部 Qt 应用集成先读 `docs/20-integration/host-app-guide.md`。
3. 需要本地模型运行时读 `docs/20-integration/local-llamacpp-guide.md`。
4. 需要会话和历史读 `docs/20-integration/conversationclient-guide.md`。
5. 需要工具调用或 MCP 读 `docs/20-integration/tool-mcp-guide.md`。
6. 参与开发先读 `docs/30-development/build-and-test.md` 和 `docs/30-development/coding-guidelines.md`。
7. 诊断问题先读 `docs/50-reference/troubleshooting.md`。

## 文档维护规则

1. 新增或改变公共集成方式时，必须同步更新 `docs/20-integration/`。
2. 改变核心分层、provider/runtime 生命周期或可观测性设计时，必须同步更新 `docs/10-architecture/`。
3. 改变构建、测试、发布流程时，必须同步更新 `docs/30-development/`。
4. 新增示例 App 或 Agent 时，必须在 `docs/40-apps/` 增加说明。
5. 已完成的阶段性计划不留在活跃文档中，应归档或压缩为当前状态。
6. `open-items.md` 只记录仍然有效的待办，不记录已完成历史。
7. 文档必须区分“已实现、部分实现、计划中”，不能把规划写成当前能力。
8. 中文文档必须使用真实 UTF-8 中文，不使用乱码文本或无意义转义文本。

## 编码约束

1. 优先使用 Qt 原生类型、信号槽、`QNetworkAccessManager` 和 Qt JSON API。
2. 网络请求保持异步，避免阻塞 UI 线程。
3. provider 相关逻辑放在 provider/runtime/client 层，不放入 UI。
4. 改动应小而明确，避免无关的大规模重构。
5. 新 ID 使用 `src/qtllm/identity/` 中的紧凑前缀 ID。
6. 新功能应配套最小可验证测试或示例。
7. Windows Qt Creator 编译体验是当前重要基线，Linux 兼容性也需要保留。
