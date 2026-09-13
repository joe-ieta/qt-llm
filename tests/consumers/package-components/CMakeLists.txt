cmake_minimum_required(VERSION 3.16)

project(qtllm_optional_components_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QtLlm CONFIG REQUIRED COMPONENTS Diagnostics Tools LocalRuntime)

add_executable(qtllm_optional_components_consumer main.cpp)
target_link_libraries(qtllm_optional_components_consumer
    PRIVATE
    QtLlm::Diagnostics
    QtLlm::Tools
    QtLlm::LocalRuntime
)
