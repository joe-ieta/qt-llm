#include "chat/conversationclientfactory.h"

int main()
{
    qtllm::chat::ConversationClientFactory factory;
    return factory.clientIds().isEmpty() ? 0 : 1;
}
