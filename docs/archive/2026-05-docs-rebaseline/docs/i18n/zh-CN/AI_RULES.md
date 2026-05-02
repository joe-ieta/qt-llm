# AI_RULES / AI 协作规则

## 摘要 / Summary
本文件定义编码代理在 qt-llmlite 中的协作规则。

## 必须遵守
1. 优先 Qt 原生方案
2. 网络保持异步
3. 不阻塞 UI 线程
4. 保持 `QtLLMClient -> ILLMProvider` 边界
5. 变更最小且聚焦
6. 代码应适配 Qt Creator 开发体验

## 禁止事项
1. 无充分理由引入重依赖
2. 将 UI 逻辑混入 provider/network 层
3. 全局硬编码单一 provider
4. 引入同步等待模式

## 实现风格
- 使用 Qt signals/slots
- 使用 `QNetworkAccessManager`
- 使用 Qt JSON 类型
- 保持头文件与实现文件边界清晰

## 文档同步
- 架构或范围变更时同步更新相关文档