# API 索引

## Host

- `qtllm::host::RuntimeFacade`
- `qtllm::host::RuntimeProfile`（struct）
- `qtllm::host::ChatRequest`（struct）
- `qtllm::host::ChatResult`（struct）
- `qtllm::host::LocalModelInfo`（struct）
- `qtllm::host::ModelCatalogService`

## Core

- `qtllm::core::LlmConfig`（struct）
- `qtllm::core::LlmRequest`（struct）
- `qtllm::core::LlmResponse`（struct）
- `qtllm::QtLLMClient`

## Chat

- `qtllm::ConversationClient`
- `qtllm::ConversationClientFactory`
- `qtllm::ConversationSnapshot`（struct）

## Provider / Runtime

- `qtllm::ILLMProvider`
- `qtllm::ProviderFactory`
- `qtllm::runtime::ManagedLlamaCppRuntime`

普通 Host App 主要使用 Host 和 Chat 层；Provider / Runtime 属于内部或高级 API。
