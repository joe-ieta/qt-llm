cmake_minimum_required(VERSION 3.16)

project(qt-llm
    VERSION 0.2.10
    LANGUAGES CXX
)

option(QTLLM_BUILD_APPS "Build sample apps and agent demo" ON)
option(QTLLM_BUILD_TESTS "Build tests target" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Sql)

if(QTLLM_BUILD_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(CMAKE_CONFIGURATION_TYPES)
    foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${CONFIG}" CONFIG_UPPER)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/lib/${CONFIG}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/lib/${CONFIG}")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_BINARY_DIR}/bin/${CONFIG}")
    endforeach()
endif()

add_subdirectory(src/qtllm)

if(QTLLM_BUILD_APPS)
    add_subdirectory(src/apps/simple_chat)
    add_subdirectory(src/apps/multi_client_chat)
    add_subdirectory(src/apps/mcp_server_manager)
    add_subdirectory(src/apps/tools_inside)
    add_subdirectory(src/apps/toolstudio)
    add_subdirectory(src/agents/pdf_translator_agent)
endif()

if(QTLLM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/qtllm_tests)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    set(QTLLM_FOUND_QT_VERSION "${Qt6_VERSION}")
else()
    set(QTLLM_FOUND_QT_VERSION "${Qt5_VERSION}")
endif()

message(STATUS "qt-llm: ${PROJECT_VERSION}")
message(STATUS "Using Qt: ${QTLLM_FOUND_QT_VERSION}")
