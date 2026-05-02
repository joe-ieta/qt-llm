# Open Items

This file tracks active follow-up work that should remain visible after the
managed llama.cpp baseline lands.

## Managed llama.cpp follow-up

- Package per-platform `llama-server` binaries for Windows and Linux instead of
  requiring users to place the executable manually under `llama-cpp-runtime/bin`.
- Add a settings view for `llamaCppRuntimeRoot`, `llamaCppModelPath`,
  `llamaCppContextSize`, `llamaCppGpuLayers`, `llamaCppThreadCount`, and extra
  server arguments.
- Add runtime log capture from the managed `llama-server` process into the
  existing `QtLlmLogger` and `toolsinside` trace surface.
- Add health-check and startup progress UI so long model loads do not look like
  an unresponsive chat request.
- Extend the app-local `llama-cpp-runtime/models` selector with model metadata,
  size, quantization hints, and per-model runtime defaults.
- Add Linux packaging verification for the managed llama.cpp runtime directory
  layout.
- Re-evaluate a future in-process llama.cpp driver only after the hosted
  `llama-server` path is stable.
