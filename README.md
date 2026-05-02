# qt-llm

qt-llm 是面向 Qt/C++ 应用的 LLM 集成基础库。它把模型访问、provider 封装、流式响应、本地运行态托管、会话管理、工具调用、MCP 和运行观测能力集中到一个可嵌入的 Qt 工程中。

项目定位很直接：让外部 Qt 应用不用重复理解各类 LLM 服务细节，只通过稳定的 Qt/C++ 接口接入模型能力。

## 主要功能

- OpenAI、OpenAI-compatible、Ollama、vLLM、llama.cpp provider。
- 内置托管 llama.cpp，本地发现 `llama-cpp-runtime` 和 `.gguf` 模型。
- `RuntimeFacade`：面向 Host App 的简单单轮调用接口。
- `QtLLMClient`：底层请求执行、流式 token、provider payload、取消请求。
- `ConversationClient`：多会话、历史、profile、快照和持久化。
- 工具调用层：工具定义、选择、执行、协议适配。
- MCP 支持：MCP server 注册、同步、调用。
- ToolsInside：运行 trace、事件、span、artifact、tool call 记录和查询。
- ToolStudio：工具目录、导入导出、工作区和元数据管理。
- 示例 App：simple_chat、multi_client_chat、mcp_server_manager、tools_inside、toolstudio。
- 参考 Agent：pdf_translator_agent。

## 典型使用方式

普通 Host App 推荐使用：

```cpp
qtllm::host::RuntimeFacade runtime;

qtllm::host::RuntimeProfile profile;
profile.providerName = QStringLiteral("llama-cpp");
profile.model = selectedModelId;
profile.llamaCppModelPath = selectedModelPath;
runtime.setProfile(profile);

qtllm::host::ChatRequest request;
request.systemPrompt = QStringLiteral("You are a helpful assistant.");
request.userPrompt = userText;
runtime.send(request);
```

使用层级选择：

- 简单模型调用：`qtllm::host::RuntimeFacade`
- 高级请求控制：`QtLLMClient`
- 多会话聊天：`ConversationClient`
- 工具调用或 MCP：`ToolEnabledChatEntry`

普通 Host App 不建议直接使用 `ProviderFactory`、`HttpExecutor`、`ManagedLlamaCppRuntime` 或具体 provider 类。

## 本地 llama.cpp

当选择 `llama-cpp` provider 时，qt-llm 可以托管 `llama-server`。默认查找顺序：

1. 当前程序目录下的 `llama-cpp-runtime`
2. `ZNZ_HOME`、`ZNZ_BLACKBOARD`、`YIDA_HOME`、`YIDA_BLACKBOARD`、`IETA_HOME`、`IETA_BLACKBOARD` 指向目录下的 `llama-cpp-runtime`
3. Linux：`/home/ieta/LLMs/llama-cpp-runtime`、`/home/IETA/LLMs/llama-cpp-runtime`
4. Windows：所有磁盘根目录下的 `LLMs/llama-cpp-runtime`

模型放在运行态目录的 `models/` 下，多个 `.gguf` 模型由 App 展示列表并让用户选择，选择结果写入 `llamaCppModelPath`。

## 工程结构

```text
src/qtllm/                    核心库
src/apps/                     示例和工具 App
src/agents/                   参考业务 Agent
tests/qtllm_tests/            Qt 单元测试
docs/                         当前中文文档
docs/archive/                 历史文档归档
```

## 构建

项目当前基线是 Qt + qmake + C++17。

```powershell
qmake qt-llm.pro
nmake /NOLOGO
```

测试入口：

```powershell
tests\qtllm_tests\release\qtllm_tests.exe
```

## 文档入口

- 顶层约束：[AI_RULES.md](./AI_RULES.md)
- 文档索引：[docs/README.md](./docs/README.md)
- Host App 集成：[docs/20-integration/host-app-guide.md](./docs/20-integration/host-app-guide.md)
- 本地 llama.cpp：[docs/20-integration/local-llamacpp-guide.md](./docs/20-integration/local-llamacpp-guide.md)
- 故障处理：[docs/50-reference/troubleshooting.md](./docs/50-reference/troubleshooting.md)
