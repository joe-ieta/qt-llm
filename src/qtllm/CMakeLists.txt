file(GLOB_RECURSE QTLLM_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER QTLLM_CORE_SOURCES EXCLUDE REGEX "/(release|debug)/")
list(FILTER QTLLM_CORE_SOURCES EXCLUDE REGEX "/moc_.*\\.cpp$")

file(GLOB_RECURSE QTLLM_CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER QTLLM_CORE_HEADERS EXCLUDE REGEX "/(release|debug)/")
list(FILTER QTLLM_CORE_HEADERS EXCLUDE REGEX "/moc_.*\\.h$")

set(QTLLM_LIGHTWEIGHT_CORE_SOURCES)
set(QTLLM_LIGHTWEIGHT_CORE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/llmconfig.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/llmtypes.h"
)
foreach(_qtllm_core_dir context identity network providers streaming structuredoutput)
    file(GLOB_RECURSE _qtllm_component_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${_qtllm_core_dir}/*.cpp"
    )
    file(GLOB_RECURSE _qtllm_component_headers CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${_qtllm_core_dir}/*.h"
    )
    list(APPEND QTLLM_LIGHTWEIGHT_CORE_SOURCES ${_qtllm_component_sources})
    list(APPEND QTLLM_LIGHTWEIGHT_CORE_HEADERS ${_qtllm_component_headers})
endforeach()

add_library(qtllm_core ${QTLLM_LIBRARY_TYPE}
    ${QTLLM_LIGHTWEIGHT_CORE_SOURCES}
    ${QTLLM_LIGHTWEIGHT_CORE_HEADERS}
)

add_library(qtllm::core ALIAS qtllm_core)

target_include_directories(qtllm_core
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(qtllm_core
    PRIVATE
    QTLLM_LIBRARY
)

target_link_libraries(qtllm_core
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

set_target_properties(qtllm_core PROPERTIES
    OUTPUT_NAME qtllm_core
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_library(qtllm ${QTLLM_LIBRARY_TYPE}
    ${QTLLM_CORE_SOURCES}
    ${QTLLM_CORE_HEADERS}
)

add_library(qtllm::qtllm ALIAS qtllm)

target_include_directories(qtllm
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(qtllm
    PRIVATE
    QTLLM_LIBRARY
)

target_link_libraries(qtllm
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)

set_target_properties(qtllm PROPERTIES
    OUTPUT_NAME qtllm
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
