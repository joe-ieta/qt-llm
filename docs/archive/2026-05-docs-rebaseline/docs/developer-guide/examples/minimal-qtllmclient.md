# 示例：最小 `QtLLMClient` 接入

## 适用场景

- 只想最快打通一次 LLM 请求
- 暂时不需要 session、history、tool calling

## 示例代码

```cpp
#include "src/qtllm/core/qtllmclient.h"
#include "src/qtllm/core/llmconfig.h"

#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>

class MinimalChatController : public QObject
{
    Q_OBJECT
public:
    MinimalChatController(QPlainTextEdit *output, QPushButton *sendButton, QObject *parent = nullptr)
        : QObject(parent)
        , m_output(output)
        , m_sendButton(sendButton)
        , m_client(new qtllm::QtLLMClient(this))
    {
        qtllm::LlmConfig config;
        config.providerName = QStringLiteral("openai-compatible");
        config.baseUrl = QStringLiteral("http://127.0.0.1:11434");
        config.model = QStringLiteral("qwen2.5:7b");
        config.stream = true;
        config.timeoutMs = 60000;

        m_client->setConfig(config);
        m_client->setProviderByName(config.providerName);

        connect(m_client, &qtllm::QtLLMClient::tokenReceived, this, [this](const QString &token) {
            if (m_output) {
                m_output->insertPlainText(token);
            }
        });

        connect(m_client, &qtllm::QtLLMClient::completed, this, [this](const QString &) {
            if (m_sendButton) {
                m_sendButton->setEnabled(true);
            }
        });

        connect(m_client, &qtllm::QtLLMClient::errorOccurred, this, [this](const QString &message) {
            if (m_output) {
                m_output->appendPlainText(QStringLiteral("\\n[error] ") + message);
            }
            if (m_sendButton) {
                m_sendButton->setEnabled(true);
            }
        });

        connect(m_client, &qtllm::QtLLMClient::providerPayloadPrepared,
                this, [this](const QString &url, const QString &payloadJson) {
            Q_UNUSED(payloadJson)
            if (m_output) {
                m_output->appendPlainText(QStringLiteral("\\n[request] ") + url);
            }
        });
    }

    void sendPrompt(const QString &text)
    {
        if (!m_client || text.trimmed().isEmpty()) {
            return;
        }

        if (m_output) {
            m_output->clear();
        }
        if (m_sendButton) {
            m_sendButton->setEnabled(false);
        }

        m_client->sendPrompt(text.trimmed());
    }

private:
    QPlainTextEdit *m_output = nullptr;
    QPushButton *m_sendButton = nullptr;
    qtllm::QtLLMClient *m_client = nullptr;
};
```

## 说明

- 这是最薄的一层封装
- 不保存历史
- 不管理 session
- 不处理工具调用

## 下一步

如果你开始需要：

- 历史消息
- profile
- 多 session

就应切到 [conversationclient-chat.md](./conversationclient-chat.md)。
