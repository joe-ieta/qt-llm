# 构建和测试

## 基线

- C++17
- Qt 5.x / Qt 6.x（通过 CMake 自动匹配）
- Windows 是当前主要验证环境
- Linux 兼容性需要保持

## Windows 构建

在已配置 MSVC 和 Qt 环境的终端中：

```powershell
target_wrapper.bat cmake -S . -B build -DQTLLM_BUILD_APPS=ON -DQTLLM_BUILD_TESTS=ON
target_wrapper.bat cmake --build build --config Release
```

如需指定 Qt 版本，可设置 `QTVERSION` 与对应路径变量（例如 `QTVERSION=6` + `QT6_ROOT`，或 `QTVERSION=5` + `QT5_ROOT`）。

`target_wrapper.bat` 会注入对应版本的 Qt 工具链与 `CMAKE_PREFIX_PATH`，并保持 `qt-creator`/MSBuild 可复用。

## 一行式全量验证（推荐）

```powershell
cmd /c "target_wrapper.bat cmake -S . -B build -DQTLLM_BUILD_APPS=ON -DQTLLM_BUILD_TESTS=ON && cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure"
```

该命令完成“配置 + Release 编译 + 全量测试（含 qtllm_tests）”一次性执行。

## 测试

```powershell
target_wrapper.bat cmake --build build --config Release --target qtllm_tests
./build/bin/Release/qtllm_tests.exe
```

测试覆盖 provider factory、OpenAI-compatible 协议、stream parser、MCP、tool execution、日志轮转、紧凑 ID、本地 llama.cpp runtime 发现等。

## Qt Creator

Qt Creator 中打开根目录，按 CMake 项目打开 `CMakeLists.txt`。
