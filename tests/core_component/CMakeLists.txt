add_executable(qtllm_core_component_test
    main.cpp
)

target_link_libraries(qtllm_core_component_test
    PRIVATE
    QtLlm::Core
)

add_test(
    NAME qtllm_core_component_test
    COMMAND qtllm_core_component_test
)
