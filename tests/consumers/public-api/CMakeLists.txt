cmake_minimum_required(VERSION 3.16)

project(qtllm_public_api_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(QTLLM_EXPECTED_VERSION)
    find_package(
        QtLlm ${QTLLM_EXPECTED_VERSION} EXACT CONFIG REQUIRED
        COMPONENTS Core Diagnostics Tools LocalRuntime Conversation QtLlm
    )
else()
    find_package(
        QtLlm CONFIG REQUIRED
        COMPONENTS Core Diagnostics Tools LocalRuntime Conversation QtLlm
    )
endif()

add_executable(qtllm_public_api_consumer main.cpp)
target_link_libraries(qtllm_public_api_consumer PRIVATE QtLlm::QtLlm)
