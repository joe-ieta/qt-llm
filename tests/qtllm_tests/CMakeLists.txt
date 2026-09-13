find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Sql Test)

file(GLOB_RECURSE QTLLM_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER QTLLM_TEST_SOURCES EXCLUDE REGEX "/(release|debug)/")
list(FILTER QTLLM_TEST_SOURCES EXCLUDE REGEX "/moc_.*\\.cpp$")

file(GLOB_RECURSE QTLLM_TEST_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER QTLLM_TEST_HEADERS EXCLUDE REGEX "/(release|debug)/")
list(FILTER QTLLM_TEST_HEADERS EXCLUDE REGEX "/moc_.*\\.h$")

add_executable(qtllm_tests
    ${QTLLM_TEST_SOURCES}
    ${QTLLM_TEST_HEADERS}
)

target_include_directories(qtllm_tests
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src/qtllm"
)

target_link_libraries(qtllm_tests
    PRIVATE
    qtllm
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(
    NAME qtllm_tests
    COMMAND qtllm_tests -o "${CMAKE_BINARY_DIR}/Testing/qtllm_tests.txt,txt"
)
