cmake_minimum_required(VERSION 3.16)

project(qtllm_package_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(QTLLM_EXPECTED_VERSION)
    find_package(QtLlm ${QTLLM_EXPECTED_VERSION} EXACT CONFIG REQUIRED)
else()
    find_package(QtLlm CONFIG REQUIRED)
endif()

add_executable(qtllm_package_consumer main.cpp)
target_link_libraries(qtllm_package_consumer PRIVATE QtLlm::QtLlm)
