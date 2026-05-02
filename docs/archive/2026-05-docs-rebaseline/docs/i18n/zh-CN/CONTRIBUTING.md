# CONTRIBUTING / 贡献指南

## 摘要 / Summary
本指南用于保证贡献内容符合架构边界与跨平台构建要求。

## 开发原则
- 保持 `QtLLMClient -> ILLMProvider -> HttpExecutor` 边界清晰
- 网络逻辑必须异步且非阻塞
- 优先小步、可编译、可验证的变更
- Provider 特有逻辑必须放在 provider 类中

## 构建与验证
1. 在 Qt Creator 打开 `qt-llm.pro`，或执行：
   - `qmake qt-llm.pro`
   - `make` / `mingw32-make`
2. 构建 `src/qtllm` 与 `src/examples/simple_chat`

## PR 检查项
- [ ] qmake 构建通过
- [ ] 未引入不必要重依赖
- [ ] 架构/范围变更已同步文档
- [ ] 变更范围聚焦且最小化