# 构建和测试

## 基线

- C++17
- Qt
- qmake
- Windows 是当前主要验证环境
- Linux 兼容性需要保持

## Windows 构建

在已配置 MSVC 和 Qt 环境的终端中：

```powershell
qmake qt-llm.pro
nmake /NOLOGO
```

仓库也包含 `target_wrapper.bat`，可在已配置环境中配合构建使用。

## 测试

```powershell
tests\qtllm_tests\release\qtllm_tests.exe
```

测试覆盖 provider factory、OpenAI-compatible 协议、stream parser、MCP、tool execution、日志轮转、紧凑 ID、本地 llama.cpp runtime 发现等。

## Qt Creator

Qt Creator 中打开 `qt-llm.pro`。如果 qmake 生成物失效，应重新运行 qmake 再构建。
