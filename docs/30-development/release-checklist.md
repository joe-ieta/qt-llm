# 发布检查

发布前检查：

1. 工作树变更清晰，代码和文档属于同一发布目标。
2. qmake 成功。
3. 项目构建成功。
4. `qtllm_tests` 通过。
5. README 和 docs 中的功能状态与当前代码一致。
6. release notes 写明新增能力、修复、兼容性影响和验证命令。
7. tag 和 GitHub release 指向同一提交。

如果远端仓库地址迁移，先确认当前 remote，再推送 tag 和 release。
