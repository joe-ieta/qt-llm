# 发布前一致性验证

本文说明 qt-llm 的发布前质量门禁。目标是用一条命令串行验证 Qt5/Qt6、STATIC/SHARED、安装包组件消费和源码集成，同时避免并行构建造成的本机资源争用。

## 验证范围

`scripts/verify-release.ps1` 首先执行活跃文档标准检查，然后依次执行以下检查：

- Qt 6 STATIC 源码配置、完整构建、6 项测试和安装。
- Qt 6 SHARED 源码配置、完整构建、6 项测试和安装。
- Qt 5 STATIC 源码配置、完整构建、6 项测试和安装。
- Qt 5 SHARED 源码配置、完整构建、6 项测试和安装。
- 每个安装结果分别验证 Core、可选组件、Conversation、兼容聚合目标和全部公开头文件，共 20 组下游工程。
- 每个 Qt/库类型组合验证一次 `add_subdirectory` 源码集成，共 4 组下游工程。
- 每个构建检查 MSVC 编译器和链接器告警，出现 `warning Cxxxx` 或 `warning LNKxxxx` 即失败。
- 检查安装后的 CMake 组件导出文件、版本文件和全部公开头文件。
- 确认四组构建的工程版本一致；传入预期版本时执行精确版本匹配。

全部步骤串行运行。任一步失败后立即停止，详细输出保存在验证构建目录的 `logs` 子目录中。

Windows 偶发占用 `qtllm_tests.exe` 时，脚本只对该测试目标重试一次，并在成功后再次执行完整构建确认。其他 LNK1104、重复失败或任意不同目标的占用仍视为发布阻断。

## 标准命令

```powershell
.\scripts\verify-release.ps1 -ExpectedVersion 0.2.10
```

默认工具链位置如下：

- CMake：`E:\Qt\Tools\CMake_64\bin\cmake.exe`
- Qt5：`E:\Qt\5.15.2\msvc2019_64`
- Qt6：`E:\Qt\6.10.3\msvc2022_64`
- 构建目录：`build-release-verification`
- 构建配置：`Release`

如需使用其他安装位置，应显式传参：

```powershell
.\scripts\verify-release.ps1 `
    -CMakePath ''D:\Qt\Tools\CMake_64\bin\cmake.exe'' `
    -Qt5Root ''D:\Qt\5.15.2\msvc2019_64'' `
    -Qt6Root ''D:\Qt\6.10.3\msvc2022_64'' `
    -BuildRoot ''D:\build\qtllm-release'' `
    -ExpectedVersion 0.2.10
```

## 告警治理原则

- 不通过关闭全部告警掩盖工程问题。
- Qt5/MSVC 仅定义 `_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING`，用于隔离 Qt 5.15 公共头文件对已弃用 `stdext::checked_array_iterator` 的依赖。
- SHARED/MSVC 仅忽略 LNK4197。该告警来自 QObject 显式导出与 `WINDOWS_EXPORT_ALL_SYMBOLS` 的有意重叠；保留自动导出是为了兼容尚未显式标注的既有公开符号。
- 发布验证仍拦截其他 MSVC 编译器和链接器告警。
- 告警治理不得改变公开类、函数签名、信号槽、运行行为或组件依赖。

## 发布判定

只有脚本输出以下全部结果时，才可判定发布前一致性验证通过：

- `source matrices: 4/4`
- `installed package consumers: 20/20`
- `add_subdirectory consumers: 4/4`
- `compiler/linker warning gate: passed`
- 输出版本与计划发布版本一致

发布标签、归档文件和远程推送仍是独立的发布操作，不由验证脚本自动创建或修改。
