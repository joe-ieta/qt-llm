#include <context/contextwindowservice.h>

int main()
{
    qtllm::context::ContextWindowService service;
    qtllm::context::ContextWindowPolicy policy;
    const qtllm::context::ContextWindowResult result =
        service.select({}, policy);
    return result.isSuccess() ? 0 : 1;
}
