# QtLLMClient 手册

`QtLLMClient` 是核心请求执行入口，适合 qt-llm 内部 facade、测试、高级集成和需要自行掌控配置完整性的调用方。

## 主要能力

- 设置 `LlmConfig`。
- 发送 prompt 或完整 request。
- 取消当前请求。
- 接收 token、reasoning token、完成、错误和 provider payload。
- 通过 provider 执行不同协议。

## 使用建议

普通 Host App 优先使用 `RuntimeFacade`。只有当调用方明确需要低层控制，并愿意负责配置完整性和生命周期时，才直接使用 `QtLLMClient`。

## 与 RuntimeFacade 的关系

`RuntimeFacade` 内部使用 `QtLLMClient`。这样可以确保本地 runtime、模型发现和可用性状态的解释留在 qt-llm 内部。
