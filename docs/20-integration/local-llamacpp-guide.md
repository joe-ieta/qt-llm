# 本地 llama.cpp 集成

qt-llm 支持内置托管 `llama.cpp` 的 `llama-server`，provider 名称使用 `llama-cpp`。

## 运行态目录

推荐布局：

```text
llama-cpp-runtime/
  bin/
    llama-server.exe
  models/
    model-a.gguf
    model-b.gguf
  logs/
```

Linux 下可执行文件名通常是 `llama-server`。

## 查找顺序

未显式指定 `llamaCppRuntimeRoot` 时，qt-llm 按顺序查找：

1. 当前程序目录下的 `llama-cpp-runtime`
2. 环境变量根目录下的 `llama-cpp-runtime`
   - `ZNZ_HOME`
   - `ZNZ_BLACKBOARD`
   - `YIDA_HOME`
   - `YIDA_BLACKBOARD`
   - `IETA_HOME`
   - `IETA_BLACKBOARD`
3. Linux：
   - `/home/ieta/LLMs/llama-cpp-runtime`
   - `/home/IETA/LLMs/llama-cpp-runtime`
4. Windows：
   - 每个磁盘根目录下的 `LLMs/llama-cpp-runtime`

候选目录会按可用性评分选择。只有空目录的本地 `llama-cpp-runtime` 不应遮挡后续共享目录。

## 模型选择

`models/` 下可以放多个 `.gguf`。App 应调用模型列表接口，把实际列表展示给用户，再把用户选择写入：

```cpp
profile.model = model.id;
profile.llamaCppModelPath = model.filePath;
```

这与 OpenAI API 的模型选择方式一致：模型是请求配置的一部分，而不是 runtime 隐式猜测。

## 常见状态

- runtime root 未找到。
- 可执行文件未找到。
- 模型目录为空。
- 选择的模型文件不存在。
- server 端口启动超时。

这些状态应通过 provider 可用性字段展示给用户。
