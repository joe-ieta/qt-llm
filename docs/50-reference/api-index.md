# API 索引

## Host

- `qtllm::host::RuntimeFacade`
- `qtllm::host::RuntimeProfile`
- `qtllm::host::ChatRequest`
- `qtllm::host::ChatResult`
- `qtllm::host::LocalModelInfo`
- `qtllm::host::ModelCatalogService`

## Core

- `qtllm::LlmConfig`
- `qtllm::LlmRequest`
- `qtllm::LlmResponse`
- `qtllm::QtLLMClient`

## Chat

- `qtllm::ConversationClient`
- `qtllm::ConversationClientFactory`
- `qtllm::ConversationSnapshot`

## Provider / Runtime

- `qtllm::ILLMProvider`
- `qtllm::ProviderFactory`
- `qtllm::runtime::ManagedLlamaCppRuntime`

普通 Host App 主要使用 Host 和 Chat 层；Provider / Runtime 属于内部或高级 API。
