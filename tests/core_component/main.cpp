#include "context/contextwindowservice.h"

int main()
{
    QVector<qtllm::LlmMessage> messages;
    messages.append({QStringLiteral("system"), QStringLiteral("rules")});
    messages.append({QStringLiteral("user"), QStringLiteral("hello")});

    qtllm::context::ContextWindowPolicy policy;
    policy.maxMessages = 2;

    qtllm::context::ContextWindowService service;
    const qtllm::context::ContextWindowResult result =
        service.select(messages, policy);
    return result.isSuccess() && result.messages.size() == 2 ? 0 : 1;
}
