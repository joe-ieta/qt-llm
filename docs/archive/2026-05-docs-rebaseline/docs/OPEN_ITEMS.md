# Open Items

This file tracks active follow-up work that should remain visible after the
managed llama.cpp baseline lands.

## Managed llama.cpp follow-up

- Extend `RuntimeFacade` model catalog support to include remote `/models`
  listing so Host Apps can use one model-selection API for local and remote
  providers.
- Add richer `RuntimeFacade::runtimeStatusChanged` states for managed runtime
  layout validation, executable probing, process startup, health checks, request
  cancellation, and shutdown.
- Add focused tests for `RuntimeProfile` to `LlmConfig` mapping, `sendBlocking`
  immediate error handling, provider switching through `RuntimeFacade`, and
  managed llama.cpp model selection.
- Add a minimal Host App example that links only against `qtllm` and uses
  `qtllm::host::RuntimeFacade` instead of direct provider/runtime classes.
- Package per-platform `llama-server` binaries for Windows and Linux into the
  discovered shared `llama-cpp-runtime/bin` layout.
- Add a settings view for `llamaCppRuntimeRoot`, `llamaCppModelPath`,
  `llamaCppContextSize`, `llamaCppGpuLayers`, `llamaCppThreadCount`, and extra
  server arguments.
- Add runtime log capture from the managed `llama-server` process into the
  existing `QtLlmLogger` and `toolsinside` trace surface.
- Add health-check and startup progress UI so long model loads do not look like
  an unresponsive chat request.
- Extend the discovered `llama-cpp-runtime/models` selector with model
  metadata, size, quantization hints, and per-model runtime defaults.
- Add Linux packaging verification for the managed llama.cpp runtime directory
  layout.
- Re-evaluate a future in-process llama.cpp driver only after the hosted
  `llama-server` path is stable.
