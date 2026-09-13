# qt-llm

qt-llm 是面向 Qt/C++ 桌面应用的可嵌入 LLM 基础库。它统一提供模型请求、流式响应、结构化输出、请求取消与终态、Provider 协议、本地 llama.cpp 运行时、会话、工具/MCP 和可选诊断能力，使宿主应用专注于 UI、业务状态和自己的数据。

## 支持范围

- Qt 5.15.2 与 Qt 6.10.3。
- CMake 3.16 及以上、C++17、MSVC x64。
- STATIC 与 SHARED 库。
- OpenAI、OpenAI-compatible、Ollama、vLLM 和托管 llama.cpp。
- 安装包 `find_package` 与源码 `add_subdirectory` 两种集成方式。

Windows 是当前完整发布验证环境。Linux 兼容性需要保持，但尚未纳入同等级自动化矩阵。

## 选择入口

| 使用场景 | 推荐入口 | CMake 目标 |
| --- | --- | --- |
| 普通宿主、并行请求、精确取消 | `qtllm::host::RuntimeFacade` / `RuntimeRequestHandle` | `QtLlm::Conversation` |
| 多会话、历史、快照、可选持久化 | `qtllm::chat::ConversationClient` | `QtLlm::Conversation` |
| 工具调用或 MCP 聊天 | `qtllm::tools::ToolEnabledChatEntry` | `QtLlm::Conversation` |
| 高级请求执行与 Provider 控制 | `qtllm::QtLLMClient` | `QtLlm::Conversation` |
| 只使用消息、协议、上下文或结构化输出类型 | 对应轻量服务和数据类型 | `QtLlm::Core` |
| 兼容既有完整接入 | 原有公开接口 | `QtLlm::QtLlm` |

普通宿主不应直接组装 `ProviderFactory`、`HttpExecutor`、具体 Provider 或本地进程管理对象。

## 安装包接入

```cmake
find_package(QtLlm 0.2.10 CONFIG REQUIRED COMPONENTS Conversation)

target_link_libraries(my_app PRIVATE QtLlm::Conversation)
```

将安装前缀加入 `CMAKE_PREFIX_PATH`，或设置 `QtLlm_DIR=<安装目录>/lib/cmake/QtLlm`。既有工程可继续链接 `QtLlm::QtLlm`。

## 最小异步调用

```cpp
#include <host/runtimefacade.h>
#include <host/runtimerequesthandle.h>

qtllm::host::RuntimeProfile profile;
profile.providerName = QStringLiteral("llama-cpp");
profile.model = selectedModelId;
profile.llamaCppModelPath = selectedModelPath;

runtimeFacade->setProfile(profile);

qtllm::host::ChatRequest request;
request.userPrompt = userText;

auto *handle = runtimeFacade->sendAsync(request);
connect(handle, &qtllm::host::RuntimeRequestHandle::tokenReceived,
        this, &MyWindow::appendToken);
connect(handle, &qtllm::host::RuntimeRequestHandle::finished,
        this, [handle](const qtllm::host::ChatResult &result) {
            // result.success / result.canceled / result.error / result.response
            handle->deleteLater();
        });
```

UI 主线程的新代码应使用 `sendAsync()`。`send()`、`sendBlocking()`、`cancel()` 和原有信号继续保留，用于兼容既有调用方。

## 构建

Qt6 示例：

```powershell
$env:QTVERSION = '6'
$env:QT6_ROOT = 'E:\Qt\6.10.3\msvc2022_64'
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build-qt6 -DQTLLM_LIBRARY_TYPE=STATIC -DQTLLM_BUILD_APPS=ON -DQTLLM_BUILD_TESTS=ON
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\cmake.exe" --build build-qt6 --config Release
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build-qt6 -C Release --output-on-failure
```

完整发布前验证：

```powershell
.\scripts\verify-release.ps1 -ExpectedVersion 0.2.10
```

该门禁串行验证 Qt5/Qt6、STATIC/SHARED、源码测试、安装包消费、全部公开头文件和 `add_subdirectory` 集成。

## 文档

- [文档入口](./docs/README.md)
- [对外开发接口与集成](./docs/20-integration/public-api-guide.md)
- [Host App 集成](./docs/20-integration/host-app-guide.md)
- [CMake 组件](./docs/30-development/cmake-components.md)
- [构建与测试](./docs/30-development/build-and-test.md)
- [发布前验证](./docs/30-development/release-validation.md)
- [API 索引](./docs/50-reference/api-index.md)
