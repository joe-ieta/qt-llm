# 发布检查清单

## 1. 范围

- 工作树中的代码、测试和文档属于同一发布目标。
- 未跟踪归档、运行数据、模型、密钥和构建目录不进入提交。
- `docs/releases/unreleased.md` 已准确描述尚未发布的变化。

## 2. 版本

- CMake `project(VERSION ...)` 与计划发布版本一致。
- `find_package(QtLlm <版本> EXACT CONFIG REQUIRED)` 通过。
- 发布说明文件名、标题、标签和归档名称使用同一版本。
- 标签指向准备发布的提交，而不是较早的同版本基线。

## 3. 自动门禁

```powershell
.\scripts\check-docs.ps1
.\scripts\verify-release.ps1 -ExpectedVersion <版本>
```

必须得到：

- active documents 检查通过。
- source matrices `4/4`。
- installed package consumers `20/20`。
- add_subdirectory consumers `4/4`。
- compiler/linker warning gate 通过。

## 4. 发布内容

- README、API、集成、构建和故障文档与当前代码一致。
- 发布说明区分已实现、受环境限制和计划能力。
- 兼容性影响、弃用项、迁移方式和验证环境明确。
- 不把真实 Provider/MCP/runtime 未执行的测试描述为已通过。

## 5. Git 与发布

- 提交后再次确认工作树只剩明确排除项。
- 确认远端分支祖先关系和标签不存在冲突。
- 推送提交后再创建并推送标签。
- Git 标签、GitHub Release 和归档指向同一提交。

发布验证脚本只验证，不自动提交、打标签、归档或推送。
