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

file(GLOB_RECURSE QTLLM_DIAGNOSTICS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/events/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/logging/*.cpp"
)
file(GLOB_RECURSE QTLLM_DIAGNOSTICS_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/events/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/logging/*.h"
)

add_library(qtllm_diagnostics ${QTLLM_LIBRARY_TYPE}
    ${QTLLM_DIAGNOSTICS_SOURCES}
    ${QTLLM_DIAGNOSTICS_HEADERS}
)
add_library(qtllm::diagnostics ALIAS qtllm_diagnostics)
target_include_directories(qtllm_diagnostics PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(qtllm_diagnostics PRIVATE QTLLM_LIBRARY)
target_link_libraries(qtllm_diagnostics
    PUBLIC
    qtllm_core
    Qt${QT_VERSION_MAJOR}::Core
)
set_target_properties(qtllm_diagnostics PROPERTIES
    OUTPUT_NAME qtllm_diagnostics
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

file(GLOB_RECURSE QTLLM_TOOLS_COMPONENT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp"
)
file(GLOB_RECURSE QTLLM_TOOLS_COMPONENT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.h"
)
list(FILTER QTLLM_TOOLS_COMPONENT_SOURCES EXCLUDE REGEX "/toolenabledchatentry\.cpp$")
list(FILTER QTLLM_TOOLS_COMPONENT_HEADERS EXCLUDE REGEX "/toolenabledchatentry\.h$")

add_library(qtllm_tools ${QTLLM_LIBRARY_TYPE}
    ${QTLLM_TOOLS_COMPONENT_SOURCES}
    ${QTLLM_TOOLS_COMPONENT_HEADERS}
)
add_library(qtllm::tools ALIAS qtllm_tools)
target_include_directories(qtllm_tools PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(qtllm_tools PRIVATE QTLLM_LIBRARY)
target_link_libraries(qtllm_tools
    PUBLIC
    qtllm_core
    qtllm_diagnostics
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)
set_target_properties(qtllm_tools PROPERTIES
    OUTPUT_NAME qtllm_tools
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

file(GLOB_RECURSE QTLLM_LOCAL_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.cpp"
)
file(GLOB_RECURSE QTLLM_LOCAL_RUNTIME_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.h"
)

add_library(qtllm_local_runtime ${QTLLM_LIBRARY_TYPE}
    ${QTLLM_LOCAL_RUNTIME_SOURCES}
    ${QTLLM_LOCAL_RUNTIME_HEADERS}
)
add_library(qtllm::local_runtime ALIAS qtllm_local_runtime)
target_include_directories(qtllm_local_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(qtllm_local_runtime PRIVATE QTLLM_LIBRARY)
target_link_libraries(qtllm_local_runtime
    PUBLIC
    qtllm_core
    qtllm_diagnostics
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)
set_target_properties(qtllm_local_runtime PROPERTIES
    OUTPUT_NAME qtllm_local_runtime
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

set(QTLLM_CONVERSATION_SOURCES)
set(QTLLM_CONVERSATION_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/qtllmclient.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/toolenabledchatentry.h"
)
foreach(_qtllm_conversation_dir chat host profile storage toolsinside toolsstudio)
    file(GLOB_RECURSE _qtllm_component_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${_qtllm_conversation_dir}/*.cpp"
    )
    file(GLOB_RECURSE _qtllm_component_headers CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${_qtllm_conversation_dir}/*.h"
    )
    list(APPEND QTLLM_CONVERSATION_SOURCES ${_qtllm_component_sources})
    list(APPEND QTLLM_CONVERSATION_HEADERS ${_qtllm_component_headers})
endforeach()
list(APPEND QTLLM_CONVERSATION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/core/qtllmclient.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/toolenabledchatentry.cpp"
)

add_library(qtllm_conversation ${QTLLM_LIBRARY_TYPE}
    ${QTLLM_CONVERSATION_SOURCES}
    ${QTLLM_CONVERSATION_HEADERS}
)
add_library(qtllm::conversation ALIAS qtllm_conversation)
target_include_directories(qtllm_conversation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(qtllm_conversation PRIVATE QTLLM_LIBRARY)
target_link_libraries(qtllm_conversation
    PUBLIC
    qtllm_core
    qtllm_diagnostics
    qtllm_tools
    qtllm_local_runtime
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)
set_target_properties(qtllm_conversation PROPERTIES
    OUTPUT_NAME qtllm_conversation
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_library(qtllm ${QTLLM_LIBRARY_TYPE}
    compatibility/qtllmcompatibility.cpp
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
    qtllm_conversation
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)

set_target_properties(qtllm PROPERTIES
    OUTPUT_NAME qtllm
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
