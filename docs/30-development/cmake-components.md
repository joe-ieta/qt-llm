# CMake 组件与库边界

## 当前阶段

QTL-09 当前提供五个正式消费目标：

- `QtLlm::Core`：轻量协议核心，只链接 Qt Core 和 Qt Network。
- `QtLlm::Diagnostics`：基础事件分发和可选日志接收端，依赖 Core。
- `QtLlm::Tools`：工具注册、MCP 客户端和受控执行运行时，依赖 Core、Diagnostics 和 Qt Network。
- `QtLlm::LocalRuntime`：托管 llama.cpp 发现、进程和共享实例服务，依赖 Core、Diagnostics 和 Qt Network。
- `QtLlm::QtLlm`：兼容聚合目标，保持既有能力并继续链接 Qt Core、Qt Network 和 Qt Sql。

源码树内同时提供等价别名 `qtllm::core` 和 `qtllm::qtllm`。既有宿主无需修改 target 名称；希望只使用消息类型、上下文窗口、HTTP 执行、供应商协议、流解析和结构化输出的宿主可改用 `QtLlm::Core`。

`QtLlm::Core` 不包含 `QtLLMClient`、会话仓库、工具/MCP、本地进程管理、诊断适配器或应用资源。`QtLLMClient`、`RuntimeFacade`、`ConversationClient` 和 `ToolEnabledChatEntry` 由 `QtLlm::Conversation` 提供。

不要在同一目标中同时链接 `QtLlm::Core` 和 `QtLlm::QtLlm`。聚合目标已经包含核心实现，重复链接没有收益。

## 包发现

只消费轻量核心：

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Core)
target_link_libraries(my_app PRIVATE QtLlm::Core)
```

继续消费完整聚合库：

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS QtLlm)
target_link_libraries(my_app PRIVATE QtLlm::QtLlm)
```

不指定组件时保持旧行为，同时导入两个目标。未知的必选组件会让 `find_package` 明确失败。

可选组件按需查找：

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Diagnostics Tools LocalRuntime)
target_link_libraries(my_app PRIVATE
    QtLlm::Diagnostics
    QtLlm::Tools
    QtLlm::LocalRuntime
)
```

仓库内示例 App 与参考 Agent 同样使用正式目标集成：它们只链接 `QtLlm::Conversation` 等组件目标，并通过 `<host/...>`、`<chat/...>`、`<tools/...>` 等公开头文件路径包含，不再引用 `src/qtllm` 源目录或聚合目标，可作为组件式集成的参考。

## 静态与共享构建

`QTLLM_LIBRARY_TYPE` 接受 `STATIC` 或 `SHARED`，默认值为 `STATIC`，因此现有构建和产物名称不变：

```powershell
cmake -S . -B build -DQTLLM_LIBRARY_TYPE=STATIC
cmake -S . -B build-shared -DQTLLM_LIBRARY_TYPE=SHARED
```

Windows 共享构建使用 CMake 的自动符号导出；其他平台沿用编译器默认可见性。安装规则同时处理静态库、导入库、共享库和运行时文件。

## 安装内容

安装包分别导出 Core、Diagnostics、Tools、LocalRuntime 和兼容聚合目标文件。轻量与可选组件只查找 Qt Core、Qt Network；只有聚合组件要求 Qt Sql。公共安装头文件包含 `context` 和 `structuredoutput`，保证 QTL-05 与 QTL-08 的公开接口可被包消费者使用。

最小源码树消费测试位于 `tests/core_component` 和 `tests/optional_components`，安装包消费工程位于 `tests/consumers/package-core` 与 `tests/consumers/package-components`。

## 后续分层约束

- 先解除 `QtLLMClient` 对工具、事件、日志和本地运行时实现的直接组合，再移动物理目标。
- 会话组件保持存储可选，不把 Qt Sql 带入无存储模式。
- 工具/MCP、本地运行时和诊断分别形成单向依赖，不允许回指聚合目标。
- `QtLlm::QtLlm` 始终作为兼容入口，组件拆分不得改变其公开行为。

## STATIC/SHARED 与 Qt5/Qt6 支持

公开组件支持 Qt 5.15.2、Qt 6.10.3，以及静态库和共享库两种构建模式。下游工程继续通过安装后的 CMake 包使用目标，不应直接依赖构建目录中的库文件。

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Diagnostics Tools LocalRuntime Conversation)

target_link_libraries(my_application
    PRIVATE
    QtLlm::Diagnostics
    QtLlm::Tools
    QtLlm::LocalRuntime
    QtLlm::Conversation
)
```

兼容既有工程时仍可使用聚合目标：

```cmake
find_package(QtLlm CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE QtLlm::QtLlm)
```

构建约束如下：

- 静态构建通过目标使用要求自动向下游传递 `QTLLM_STATIC`。
- 共享库构建由各组件内部定义对应的导出宏，下游不得手工定义 `QTLLM_*_LIBRARY`。
- QObject 派生公开类使用所属组件导出声明，保证 `staticMetaObject`、虚表和信号槽边界可跨 Windows DLL 使用。
- 聚合目标只负责兼容链接与传递依赖，不改变组件职责或公开接口。
- Windows 继续保留 `WINDOWS_EXPORT_ALL_SYMBOLS` 兼容尚未显式标注的公开符号；QObject 显式导出造成的已知重叠仅使用 `/IGNORE:4197` 定向治理，其他编译和链接告警仍由发布门禁拦截。

QTL-09 验证矩阵覆盖 Qt5/Qt6、STATIC/SHARED 源码测试，以及安装包的组件式、Conversation 和聚合目标下游工程。所有组合均已完成配置、构建和实际运行。
