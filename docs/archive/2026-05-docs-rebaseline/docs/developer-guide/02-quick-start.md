# 快速开始

## 1. 最小接入目标

如果你要在新的 Qt 应用中接入 `qtllm`，最小目标通常是：

1. 配置 Provider
2. 发送一条用户消息
3. 接收流式 token 或最终结果

## 2. 最小调用步骤

### 步骤 1：创建客户端

最小路径可以直接使用 `QtLLMClient`。

### 步骤 2：配置 `LlmConfig`

最常用配置包括：

- `providerName`
- `baseUrl`
- `apiKey`
- `model`
- `modelVendor`
- `stream`
- `timeoutMs`
- `maxRetries`

### 步骤 3：设置 Provider

两种方式：

- `setProviderByName(providerName)`
- `setProvider(std::unique_ptr<ILLMProvider>)`

通常优先用 `setProviderByName(...)`。

### 步骤 4：连接信号

最常用信号：

- `tokenReceived`
- `reasoningTokenReceived`
- `responseReceived`
- `completed`
- `errorOccurred`
- `providerPayloadPrepared`

### 步骤 5：发送请求

可选：

- `sendPrompt(...)`
- `sendRequest(...)`

## 3. 最小示例

```cpp
qtllm::QtLLMClient *client = new qtllm::QtLLMClient(this);

qtllm::LlmConfig config;
config.providerName = QStringLiteral("openai-compatible");
config.baseUrl = QStringLiteral("http://127.0.0.1:11434");
config.model = QStringLiteral("qwen2.5:7b");
config.stream = true;

client->setConfig(config);
client->setProviderByName(config.providerName);

connect(client, &qtllm::QtLLMClient::tokenReceived, this, [this](const QString &token) {
    ui->outputEdit->insertPlainText(token);
});

connect(client, &qtllm::QtLLMClient::completed, this, [this](const QString &text) {
    qDebug() << "completed:" << text;
});

connect(client, &qtllm::QtLLMClient::errorOccurred, this, [this](const QString &message) {
    qWarning() << "llm error:" << message;
});

client->sendPrompt(QStringLiteral("你好，请介绍一下你自己。"));
```

## 4. 什么时候不用 `QtLLMClient`

如果你的应用满足下面任一条件，建议不要停留在 `QtLLMClient`：

- 需要多 session
- 需要持久化历史消息
- 需要 profile
- 需要工具调用
- 需要 MCP

对应建议：

- 会话型应用：用 `ConversationClient`
- 工具型应用：用 `ToolEnabledChatEntry`

## 5. 进阶接入顺序

推荐按下面顺序增加能力：

1. `QtLLMClient`
2. `ConversationClient`
3. `ConversationClientFactory`
4. `ToolEnabledChatEntry`
5. `McpServerManager`
6. `toolsinside` 与日志

这样最容易定位问题，也最符合当前工程的层次设计。
