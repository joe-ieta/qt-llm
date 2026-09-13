# CMake 组件与库边界

## 当前阶段

QTL-09 当前提供五个正式消费目标：

- `QtLlm::Core`：轻量协议核心，只链接 Qt Core 和 Qt Network。
- `QtLlm::Diagnostics`：基础事件分发和可选日志接收端，依赖 Core。
- `QtLlm::Tools`：工具注册、MCP 客户端和受控执行运行时，依赖 Core、Diagnostics 和 Qt Network。
- `QtLlm::LocalRuntime`：托管 llama.cpp 发现、进程和共享实例服务，依赖 Core、Diagnostics 和 Qt Network。
- `QtLlm::QtLlm`：兼容聚合目标，保持既有能力并继续链接 Qt Core、Qt Network 和 Qt Sql。

源码树内同时提供等价别名 `qtllm::core` 和 `qtllm::qtllm`。既有宿主无需修改 target 名称；希望只使用消息类型、上下文窗口、HTTP 执行、供应商协议、流解析和结构化输出的宿主可改用 `QtLlm::Core`。

`QtLlm::Core` 不包含 `QtLLMClient`、会话仓库、工具/MCP、本地进程管理、诊断适配器或应用资源。`ToolEnabledChatEntry` 因组合会话与工具能力暂留兼容聚合目标，不属于独立 Tools 组件。

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
