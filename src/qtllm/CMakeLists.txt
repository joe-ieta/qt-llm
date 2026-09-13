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

add_library(qtllm STATIC
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
)
