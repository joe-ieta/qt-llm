# Local `llama.cpp` Managed Integration

- Product: `qt-llm`
- Status: Active
- Updated: 2026-05-17

## Summary

`qt-llm` supports a managed `llama.cpp` integration through provider
`llama-cpp`.

The active model is:

- the runtime payload is expected to be app-local
- the managed model catalog can combine bundled and supplemental model folders
- host apps should consume the returned managed model list rather than infer
  search paths themselves

## Runtime Root

The preferred managed runtime root is:

```text
<applicationDir>/llama-cpp-runtime/
```

Expected shape:

```text
llama-cpp-runtime/
  bin/
    llama-server.exe
  models/
    *.gguf
  logs/
```

If `llamaCppRuntimeRoot` is explicitly set, that explicit value still wins.

Without an explicit override, the runtime root defaults to the current
application directory's `llama-cpp-runtime`.

This is a deliberate product boundary:

- runtime provisioning belongs to packaging/deployment
- runtime-root ranking should not be reimplemented by each host app

## Managed Model Catalog

The managed model catalog is broader than the bundled runtime root.

`qt-llm` aggregates models from:

1. bundled models under `<runtimeRoot>/models`
2. supplemental shared folders discovered as `<searchBase>/qtllm/models`

Current supplemental search bases include:

- the current application directory
- environment roots:
  - `ZNZ_HOME`
  - `ZNZ_BLACKBOARD`
  - `YIDA_HOME`
  - `YIDA_BLACKBOARD`
  - `IETA_HOME`
  - `IETA_BLACKBOARD`
- Windows shared roots such as `<drive>/LLMs`
- Linux shared roots such as `/home/.../LLMs`

This means host apps should not hardcode:

- runtime-root search order for models
- direct directory scanning rules
- list merge rules across bundled and shared model folders

Instead, host apps should call the model-list API and display the resulting
managed catalog.

## Model Selection

Host apps should use the managed catalog returned by `RuntimeFacade` /
`ModelCatalogService`.

Typical flow:

```cpp
qtllm::host::RuntimeFacade runtime;
runtime.setProfile(profile);
QString errorMessage;
const QList<qtllm::host::LocalModelInfo> models = runtime.listLocalModels(&errorMessage);
```

When a model is selected, the host can persist:

```cpp
profile.model = model.id;
profile.llamaCppModelPath = model.filePath;
```

Model resolution order inside `qt-llm` is:

1. explicit `llamaCppModelPath`
2. explicit `model` matched against the aggregated managed catalog
3. the single discovered `.gguf` file if there is exactly one

## Runtime Planning

Managed `llama.cpp` startup is policy-first.

Recommended host-facing fields are:

- `llamaCppGpuMode`: `auto`, `cpu-only`, `prefer-gpu`, `explicit`
- `llamaCppPerformanceProfile`: `conservative`, `balanced`, `aggressive`
- `llamaCppContextMode`: `auto`, `explicit`

`qt-llm` derives launch parameters such as:

- `--gpu-layers`
- `--threads`
- `--ctx-size`

Expert overrides still exist through:

- `llamaCppExtraArgs`
- explicit `llamaCppGpuLayers`
- explicit `llamaCppThreadCount`
- explicit `llamaCppContextSize`

Resolved diagnostics remain available through fields such as:

- `resolvedLlamaCppGpuMode`
- `resolvedLlamaCppGpuLayers`
- `resolvedLlamaCppThreadCount`
- `resolvedLlamaCppContextSize`
- `runtimePlanSummary`
- `runtimePlanWarnings`

## Lifecycle

`ManagedLlamaCppRuntime::ensureRunning()`:

1. resolves runtime layout
2. resolves the active model
3. derives the launch plan
4. starts `llama-server` when needed
5. reuses an existing server on the configured port when appropriate

`ManagedLlamaCppRuntime::stop()` terminates the process owned by the runtime
instance.

Tool apps may also connect `QCoreApplication::aboutToQuit` to
`ManagedLlamaCppRuntime::stop()` for explicit shutdown behavior.

## Host-App Boundary

For host apps, the active guidance is:

- expose high-level local-runtime intent
- consume the managed model catalog returned by `qt-llm`
- avoid making end users reason about runtime-root details or low-level launch
  knobs

For `qt-llm`, the active responsibility is:

- runtime-root interpretation
- bundled/supplemental model search semantics
- managed model-list aggregation
- launch planning
- managed `llama-server` lifecycle
