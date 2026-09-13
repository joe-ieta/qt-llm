# CMake 集成 qt-llm

qt-llm 同时支持源码子项目和安装包消费。正式 target 为 `QtLlm::QtLlm`，仓库内原 target `qtllm` 保留兼容。

## 源码子项目模式

```cmake
add_subdirectory(path/to/qt-llm)
target_link_libraries(yApp PRIVATE QtLlm::QtLlm)
```

作为子项目时，示例应用、测试和安装规则默认关闭，且 qt-llm 不覆盖宿主输出目录。宿主可在 `add_subdirectory` 前显式设置 `QTLLM_BUILD_APPS`、`QTLLM_BUILD_TESTS` 或 `QTLLM_ENABLE_INSTALL`。

可独立验证的最小工程位于 `tests/consumers/subdirectory`，配置时传入 `-DQTLLM_SOURCE_DIR=<qt-llm源码目录>`。

## 安装包模式

先构建并安装 qt-llm：

```powershell
cmake -S . -B build -DQTLLM_BUILD_APPS=OFF -DQTLLM_BUILD_TESTS=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix <安装目录>
```

宿主工程使用：

```cmake
find_package(QtLlm CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE QtLlm::QtLlm)
```

将安装前缀加入 `CMAKE_PREFIX_PATH`，或配置 `QtLlm_DIR=<安装目录>/lib/cmake/QtLlm`。最小工程位于 `tests/consumers/package`。

## Qt 版本

配置阶段优先使用宿主 `CMAKE_PREFIX_PATH` 中可用的 Qt6，其次 Qt5。一个构建目录只能绑定一个 Qt 主版本，Qt5 与 Qt6 产物不可混用。

## 公共包含路径

为兼容现有宿主，安装后继续支持 `<core/qtllmclient.h>`、`<host/runtimefacade.h>` 和 `<logging/qtllmlogger.h>` 等当前包含方式。公共头文件边界后续只能按弃用规则收缩。

## 验证矩阵

| Qt | 工具链 | 独立库安装 | 源码子项目 | 安装包 `find_package` | 结果 |
| --- | --- | --- | --- | --- | --- |
| 6.10.3 | MSVC 2022 x64 | 通过 | 通过 | 通过 | 已验证 |
| 5.15.2 | MSVC 2019 x64，使用 VS 2022 生成器 | 通过 | 通过 | 通过 | 已验证，有 Qt5/STL 弃用警告 |

原有 Qt6 Release 全量构建与 CTest 同步通过。Qt5 和 Qt6 使用独立构建目录与安装前缀，未混用二进制产物。
