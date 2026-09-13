# 结构化输出与模型能力查询

## 设计边界

qt-llm 负责表达输出约束、构建供应商参数、解析 JSON、校验受控 JSON Schema，并返回可判定的失败类别。Schema 字段含义、结果质量和业务回退仍由宿主负责。

文本请求是默认行为。现有调用不设置 OutputConstraint 时，请求负载和结果语义保持不变。

## 输出约束

OutputConstraint 提供三种格式：

- Text：默认文本模式，不增加约束或结构化校验。
- Json：要求最终正文是一个有效 JSON 值。
- JsonSchema：要求最终正文是有效 JSON，且满足宿主提供的 Schema。

约束模式分为：

- Native：使用供应商协议的原生参数；适配器明确不支持时，请求在发送前失败。
- Prompt：调用方显式选择通用提示词约束，不会伪装成原生能力。

库不会在 Native 不可用时静默切换到 Prompt。自动修复和额外模型调用默认关闭，当前实现也不会修改、截取或重新生成模型原文。

## 受控 Schema 子集

首批支持以下关键字：$schema、type、properties、required、items、enum、additionalProperties、title 和 description。

additionalProperties 仅支持布尔值。type 支持 object、array、string、number、integer、boolean 和 null。未列出的关键字会在发送前返回 structured_output_schema_unsupported，避免把部分校验误报为完整 JSON Schema 支持。

## 供应商映射

| 适配器协议 | JSON | JSON Schema | 原生参数 |
| --- | --- | --- | --- |
| OpenAI Responses | 支持映射 | 支持映射 | text.format |
| OpenAI-compatible、vLLM、Ollama、llama.cpp | 支持映射 | 支持映射 | response_format |
| Google Gemini 路由 | 支持映射 | 支持映射 | generationConfig |
| Anthropic 路由 | 不支持映射 | 不支持映射 | 可显式选择 Prompt |
| 未知适配器 | 未知 | 未知 | 不宣称支持 |

“支持映射”只表示适配器能够表达该协议参数，不表示任意具体模型都支持。模型能力必须结合能力快照判断。

## 能力快照

RuntimeFacade::modelCapabilities() 返回 ModelCapabilitySnapshot，分别描述流式、工具调用、JSON 和 JSON Schema 能力。

每项能力同时包含：

- adapterSupport 和 adapterSource：适配器协议映射能力及其来源。
- modelSupport 和 modelSource：具体模型证据及其来源。
- effectiveSupport：综合结果。

具体模型没有配置或探测证据时，modelSupport 保持 Unknown。只有适配器和模型证据都为 Supported 时，综合结果才是 Supported；任一方明确 Unsupported 时，综合结果是 Unsupported。

RuntimeProfile::modelCapabilities 可注入来自 ModelConfiguration 或 RuntimeProbe 的证据。qt-llm 不根据供应商名称直接推断具体模型能力。

## 结果与错误

LlmResponse::structuredOutput 保留 requested、rawText、value、syntaxValid、schemaValid 和 violations。调用失败沿用 Transport、Runtime 等类别；JSON 语法错误使用 Parsing；Schema 不匹配使用 Schema，因此三者可以稳定区分。

主要错误码：

| 错误码 | 含义 |
| --- | --- |
| structured_output_schema_missing | JsonSchema 请求没有 Schema |
| structured_output_schema_unsupported | Schema 使用了未支持的关键字或形态 |
| structured_output_unsupported | 适配器或模型配置明确不支持原生约束 |
| structured_output_invalid_json | 模型最终正文不是有效 JSON |
| structured_output_schema_mismatch | JSON 有效但不满足 Schema |

## 流式边界

流式 token 仍作为预览事件即时发送。只有供应商终态到达、完整正文解析成功且 Schema 校验通过后，最终结果才标记成功。预览片段本身不表示结构化结果有效。

## 集成步骤

1. 在 ChatRequest::output 中选择 Json 或 JsonSchema。
2. 根据 RuntimeFacade::modelCapabilities() 判断证据强度。
3. 明确选择 Native 或 Prompt，不依赖静默降级。
4. 从 ChatResult::response.structuredOutput 读取解析值和校验信息。
5. 在宿主侧执行业务有效性判断和回退。
