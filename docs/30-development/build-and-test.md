# 构建和测试

## 基线

- CMake 3.16 及以上。
- C++17。
- Qt 5.15.2 / Qt 6.10.3。
- Windows MSVC x64 是当前完整验证环境。
- `QTLLM_LIBRARY_TYPE=STATIC` 或 `SHARED`。

## Windows 环境包装器

根目录 `target_wrapper.bat` 选择 Qt、设置 `PATH` 和 `CMAKE_PREFIX_PATH`，并执行其后的命令。它不会修改父 PowerShell 会话。

Qt6：

```powershell
$env:QTVERSION = '6'
$env:QT6_ROOT = 'E:\Qt\6.10.3\msvc2022_64'
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build-qt6 -DQTLLM_LIBRARY_TYPE=STATIC -DQTLLM_BUILD_APPS=ON -DQTLLM_BUILD_TESTS=ON
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\cmake.exe" --build build-qt6 --config Release
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build-qt6 -C Release --output-on-failure
```

Qt5：

```powershell
$env:QTVERSION = '5'
$env:QT5_ROOT = 'E:\Qt\5.15.2\msvc2019_64'
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build-qt5 -DQTLLM_LIBRARY_TYPE=STATIC -DQTLLM_BUILD_APPS=ON -DQTLLM_BUILD_TESTS=ON
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\cmake.exe" --build build-qt5 --config Release
.\target_wrapper.bat "E:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir build-qt5 -C Release --output-on-failure
```

一个构建目录只能绑定一个 Qt 主版本和一种库类型。

## 常用选项

| 选项 | 值 | 说明 |
| --- | --- | --- |
| `QTLLM_LIBRARY_TYPE` | `STATIC` / `SHARED` | 库形态 |
| `QTLLM_BUILD_APPS` | `ON` / `OFF` | 示例与工具应用 |
| `QTLLM_BUILD_TESTS` | `ON` / `OFF` | 测试目标 |
| `QTLLM_ENABLE_INSTALL` | `ON` / `OFF` | 安装与包导出规则 |

作为子项目时，应用、测试和安装默认关闭，避免污染宿主工程。

## 标准检查

只检查活跃文档：

```powershell
.\scripts\check-docs.ps1
```

完整发布前验证：

```powershell
.\scripts\verify-release.ps1 -ExpectedVersion 0.2.10
```

完整门禁串行覆盖：

- Qt5/Qt6 × STATIC/SHARED 四组源码配置、全量构建、安装和每组 6 项 CTest。
- 每个安装包的 Core、组件、Conversation、聚合和全部公开头文件消费者，共 20/20。
- 四组 `add_subdirectory` 消费者。
- 安装头文件和 CMake 导出元数据完整性。
- 精确版本匹配。
- MSVC 编译器与链接器告警。

详细日志位于验证构建目录的 `logs/`。

## Qt Creator

以 CMake 项目打开根目录 `CMakeLists.txt`，为 Qt5 和 Qt6 使用独立构建目录。涉及 `Q_OBJECT`、导出宏或源文件列表变化时重新运行 CMake 配置。
