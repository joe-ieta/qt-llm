cmake_minimum_required(VERSION 3.16)

project(qtllm_subdirectory_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT QTLLM_SOURCE_DIR)
    message(FATAL_ERROR "QTLLM_SOURCE_DIR must point to the qt-llm source tree")
endif()

add_subdirectory("${QTLLM_SOURCE_DIR}" qtllm-source)

add_executable(qtllm_subdirectory_consumer main.cpp)
target_link_libraries(qtllm_subdirectory_consumer PRIVATE QtLlm::QtLlm)
