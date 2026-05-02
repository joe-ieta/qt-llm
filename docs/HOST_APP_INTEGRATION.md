# Host App Integration Guide

This document is a top-level project constraint for integrating `qt-llm` into
external Qt applications.

## Positioning

`qt-llm` owns the work of talking to LLM runtimes. A Host App should keep only
application settings, UI state, and business state. It should not duplicate
provider construction, llama.cpp runtime interpretation, request dispatch,
stream parsing, or local model discovery.

The recommended integration chain is:

```text
Host App
  -> qtllm::host::RuntimeFacade
  -> QtLLMClient
  -> Provider / Managed Runtime / HttpExecutor
```

For higher-level chat products, use:

```text
Host App
  -> ConversationClient
  -> QtLLMClient
  -> Provider / Managed Runtime / HttpExecutor
```

For tool and MCP products, use:

```text
Host App
  -> ToolEnabledChatEntry
  -> ConversationClient / QtLLMClient
  -> Tool Runtime / MCP / Provider / HttpExecutor
```

## Public Integration Paths

### RuntimeFacade

Use `qtllm::host::RuntimeFacade` when the Host App needs simple single-turn or
request-oriented LLM calls.

It owns:

- `RuntimeProfile` to `LlmConfig` mapping
- provider selection through `QtLLMClient`
- managed llama.cpp startup through the existing client path
- local GGUF model discovery
- streaming token signals
- blocking and non-blocking request convenience
- request-level trace IDs and `toolsinside` trace handoff
- runtime status signals for UI observability and control

Host Apps should prefer this layer for embedding `qt-llm` into another product.

### QtLLMClient

Use `QtLLMClient` when implementing a new library-level facade or an advanced
runtime component inside `qt-llm`.

External Host Apps may still use it directly for advanced cases, but direct
usage means the Host App accepts responsibility for configuration completeness
and provider lifecycle decisions.

### ConversationClient

Use `ConversationClient` for applications that need:

- multiple sessions
- persisted history
- client profiles and persona context
- conversation snapshots

It remains the recommended API for chat applications, while `RuntimeFacade`
remains the narrow Host App bridge for simple request execution.

### ToolEnabledChatEntry

Use `ToolEnabledChatEntry` for tool calling, MCP-backed workflows, and
tool-runtime integration. A simple Host App should not adopt this layer unless it
actually needs tool execution.

## Do Not Duplicate These Details in Host Apps

Normal Host Apps should not directly use these classes for model calls:

- `ProviderFactory`
- `ILLMProvider` implementations
- `HttpExecutor`
- `ManagedLlamaCppRuntime`
- provider-specific payload builders

These remain internal or advanced APIs. Keeping them behind `qt-llm` prevents
integration drift when provider behavior, managed runtime layout, or request
observability changes.

## RuntimeProfile

`RuntimeProfile` is the Host App settings shape. It is intentionally close to
the settings users already expose:

```cpp
qtllm::host::RuntimeProfile profile;
profile.providerName = QStringLiteral("llama-cpp");
profile.baseUrl = QStringLiteral("http://127.0.0.1:18080/v1");
profile.model = selectedModelId;
profile.stream = true;
profile.llamaCppModelPath = selectedModelPath;
profile.llamaCppContextSize = 4096;
profile.llamaCppGpuLayers = -1;

runtime->setProfile(profile);
```

The mapping from `RuntimeProfile` to `LlmConfig` belongs in `qt-llm`, not in
each Host App.

## Model Selection

For managed llama.cpp, Host Apps should use:

```cpp
QString error;
const QList<qtllm::host::LocalModelInfo> models = runtime->listLocalModels(&error);
```

When multiple `.gguf` files exist, the Host App should present the returned list
to the user and set the selected file path in `RuntimeProfile::llamaCppModelPath`.
This mirrors OpenAI-style model selection: the selected model is part of the
request profile instead of being guessed implicitly.

## Sending Requests

Non-blocking:

```cpp
auto *runtime = new qtllm::host::RuntimeFacade(this);
runtime->setProfile(profile);

connect(runtime, &qtllm::host::RuntimeFacade::tokenReceived,
        this, &MyWindow::appendToken);
connect(runtime, &qtllm::host::RuntimeFacade::completed,
        this, &MyWindow::handleCompleted);
connect(runtime, &qtllm::host::RuntimeFacade::errorOccurred,
        this, &MyWindow::handleError);
connect(runtime, &qtllm::host::RuntimeFacade::runtimeStatusChanged,
        this, &MyWindow::showRuntimeStatus);

qtllm::host::ChatRequest request;
request.systemPrompt = systemPrompt;
request.userPrompt = userPrompt;
runtime->send(request);
```

Blocking:

```cpp
qtllm::host::ChatResult result = runtime->sendBlocking(request, 60000);
```

Blocking calls are convenience APIs for worker threads, tests, and narrow
integration points. UI code should prefer the non-blocking API.

## Observability and Control

Host Apps should expose or consume these signals instead of inferring runtime
state from network behavior:

- `tokenReceived`
- `reasoningTokenReceived`
- `providerPayloadPrepared`
- `runtimeStatusChanged`
- `completed`
- `errorOccurred`

Request IDs, session IDs, and trace IDs may be supplied by the Host App through
`ChatRequest`; if omitted, `qt-llm` generates compact IDs. This keeps execution
perceptible, observable, and controllable without forcing the Host App to adopt
the full conversation or tool stack.

## Local Runtime Rule

Managed llama.cpp remains a hosted `llama-server` runtime managed by `qt-llm`.
The Host App should not start or probe the process itself in normal use. It may
persist user preferences such as runtime root, executable path, model path,
context size, GPU layers, thread count, and extra args, but interpretation and
startup belong to `qt-llm`.

When `RuntimeProfile::llamaCppRuntimeRoot` is not explicitly set, `qt-llm`
searches for `llama-cpp-runtime` in this order:

1. the running application's directory
2. directories pointed to by `ZNZ_HOME`, `ZNZ_BLACKBOARD`, `YIDA_HOME`,
   `YIDA_BLACKBOARD`, `IETA_HOME`, and `IETA_BLACKBOARD`
3. Linux: `/home/ieta/LLMs/llama-cpp-runtime` and
   `/home/IETA/LLMs/llama-cpp-runtime`
4. Windows: every drive root's `LLMs/llama-cpp-runtime`

If no runtime root is found, `qt-llm` records a runtime error and marks the
provider configuration unavailable. If a runtime root is found but
`llama-server` or a selected `.gguf` model is missing, the provider is also
marked unavailable with a specific status message.

Direct deployment of a separate llama.cpp, vLLM, Ollama, or SGLang service is
still supported through OpenAI-compatible providers. The choice is:

- use `RuntimeFacade` plus managed llama.cpp for the local-first embedded path
- use `RuntimeFacade` plus a remote-compatible provider for externally managed
  services
- use `ConversationClient` when sessions and history are part of the product
- use `ToolEnabledChatEntry` when tool execution is part of the product
