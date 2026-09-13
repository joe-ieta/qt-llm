# CMake 集成 qt-llm

qt-llm 支持安装包 `find_package` 和源码 `add_subdirectory`。外部项目应链接公开目标，不直接收集 qt-llm 内部源文件。

## 组件目标

| 目标 | 用途 |
| --- | --- |
| `QtLlm::Core` | 消息、配置、协议、上下文、流解析和结构化输出基础能力 |
| `QtLlm::Diagnostics` | 基础事件与日志接收端 |
| `QtLlm::Tools` | 工具注册、策略、执行和 MCP |
| `QtLlm::LocalRuntime` | llama.cpp 运行时和共享实例服务 |
| `QtLlm::Conversation` | RuntimeFacade、请求句柄、QtLLMClient、会话和完整高层入口 |
| `QtLlm::QtLlm` | 保持既有完整能力的兼容聚合目标 |

## 安装包模式

```powershell
cmake -S . -B build -DQTLLM_LIBRARY_TYPE=STATIC -DQTLLM_BUILD_APPS=OFF -DQTLLM_BUILD_TESTS=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix <安装目录>
```

普通宿主：

```cmake
find_package(QtLlm CONFIG REQUIRED COMPONENTS Conversation)
target_link_libraries(my_app PRIVATE QtLlm::Conversation)
```

精确版本：

```cmake
find_package(QtLlm 0.2.10 EXACT CONFIG REQUIRED COMPONENTS Conversation)
```

设置 `CMAKE_PREFIX_PATH=<安装目录>`，或 `QtLlm_DIR=<安装目录>/lib/cmake/QtLlm`。

## 源码子项目模式

```cmake
set(QTLLM_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(QTLLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(QTLLM_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QTLLM_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)

add_subdirectory(path/to/qt-llm)
target_link_libraries(my_app PRIVATE QtLlm::QtLlm)
```

源码模式保留 `qtllm` 和 `qtllm::qtllm` 兼容目标。正式安装包名称使用 `QtLlm::*`。

## 二进制规则

- Qt5 与 Qt6 产物不可混用。
- STATIC 与 SHARED 使用相同的公开目标和包含路径，但产物不可混用。
- 静态使用定义由目标自动传递。
- 宿主不得定义 `QTLLM_*_LIBRARY`。
- 不要同时链接聚合目标及其内部组件。
- 不要依赖构建目录中的临时库文件或内部源文件布局。

## 已验证矩阵

| Qt | STATIC | SHARED | 安装包 | 源码子项目 | MSVC 告警门禁 |
| --- | --- | --- | --- | --- | --- |
| 6.10.3 | 通过 | 通过 | 通过 | 通过 | 通过 |
| 5.15.2 | 通过 | 通过 | 通过 | 通过 | 通过 |

标准证据由 `scripts/verify-release.ps1` 生成。
