cmake_minimum_required(VERSION 3.16)

project(qtllm_conversation_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QtLlm REQUIRED COMPONENTS Conversation)

add_executable(qtllm_conversation_consumer main.cpp)
target_link_libraries(qtllm_conversation_consumer PRIVATE QtLlm::Conversation)
