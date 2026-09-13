cmake_minimum_required(VERSION 3.16)

project(qtllm_core_package_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QtLlm CONFIG REQUIRED COMPONENTS Core)

add_executable(qtllm_core_package_consumer main.cpp)
target_link_libraries(qtllm_core_package_consumer PRIVATE QtLlm::Core)
