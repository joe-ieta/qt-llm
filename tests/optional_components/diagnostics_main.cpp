#include "events/llmeventdispatcher.h"
#include "logging/qtllmlogger.h"

int main()
{
    qtllm::events::LlmEventDispatcher::instance().clearSinks();
    qtllm::logging::QtLlmLogger::instance().clearSinks();
    return qtllm::events::LlmEventDispatcher::instance().sinkCount() == 0 ? 0 : 1;
}
