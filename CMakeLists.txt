cmake_minimum_required(VERSION 3.16)

project(qt-llm
    VERSION 0.2.10
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(PROJECT_IS_TOP_LEVEL)
    set(_qtllm_optional_targets_default ON)
    set(_qtllm_install_default ON)
else()
    set(_qtllm_optional_targets_default OFF)
    set(_qtllm_install_default OFF)
endif()

option(QTLLM_BUILD_APPS "Build sample apps and agent demo" ${_qtllm_optional_targets_default})
option(QTLLM_BUILD_TESTS "Build tests target" ${_qtllm_optional_targets_default})
option(QTLLM_ENABLE_INSTALL "Enable install and package export rules" ${_qtllm_install_default})

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

if(PROJECT_IS_TOP_LEVEL)
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
endif()

add_subdirectory(src/qtllm)

add_library(QtLlm::QtLlm ALIAS qtllm)
set_target_properties(qtllm PROPERTIES
    EXPORT_NAME QtLlm
    INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/qtllm>;$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

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

if(QTLLM_ENABLE_INSTALL)
    install(TARGETS qtllm EXPORT QtLlmTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    foreach(_qtllm_public_header_dir
            chat core host identity logging network profile providers runtime
            storage streaming tools toolsinside toolsstudio)
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/qtllm/${_qtllm_public_header_dir}"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h"
        )
    endforeach()
    install(EXPORT QtLlmTargets
        FILE QtLlmTargets.cmake
        NAMESPACE QtLlm::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QtLlm
    )
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/QtLlmConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/QtLlmConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QtLlm
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/QtLlmConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/QtLlmConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/QtLlmConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QtLlm
    )
endif()

message(STATUS "qt-llm: ${PROJECT_VERSION}")
message(STATUS "Using Qt: ${QTLLM_FOUND_QT_VERSION}")
