# Provider And Runtime

## Provider

The current provider factory supports:

- `openai`
- `openai-compatible`
- `ollama`
- `vllm`
- `llama-cpp`
- OpenAI-compatible vendor aliases such as `sglang`, `anthropic`, `google`,
  `gemini`, `deepseek`, `qwen`, `glm`, and `zhipu`

Providers own protocol-shape differences. Host apps should not duplicate
provider payload construction logic.

## Managed `llama.cpp`

`ManagedLlamaCppRuntime` owns:

- deciding whether a provider is a managed `llama.cpp` provider
- resolving the managed runtime root
- resolving the `llama-server` executable
- building the managed model catalog
- resolving the active model path
- deriving the launch plan
- starting the local server and waiting for the target port
- updating `LlmConfig` availability and resolved diagnostics

The active model is:

- the runtime root defaults to the app-local `llama-cpp-runtime`
- bundled models come from `<runtimeRoot>/models`
- supplemental models may be aggregated from shared `qtllm/models` roots

This keeps runtime packaging and model discovery inside `qt-llm` instead of
spreading directory rules into each host app.

## Availability State

The main availability-related `LlmConfig` fields are:

- `providerAvailable`
- `providerAvailabilityStatus`
- `providerAvailabilityMessage`
- `resolvedRuntimeRoot`
- `resolvedModelPath`
- `localModelCount`

Host configuration UIs should surface these fields as availability and
diagnostic information instead of only showing a generic request failure after
invocation time.
