find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network Sql)

file(GLOB_RECURSE MULTI_CLIENT_CHAT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER MULTI_CLIENT_CHAT_SOURCES EXCLUDE REGEX "/(release|debug)/")
list(FILTER MULTI_CLIENT_CHAT_SOURCES EXCLUDE REGEX "/moc_.*\\.cpp$")

file(GLOB_RECURSE MULTI_CLIENT_CHAT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER MULTI_CLIENT_CHAT_HEADERS EXCLUDE REGEX "/(release|debug)/")
list(FILTER MULTI_CLIENT_CHAT_HEADERS EXCLUDE REGEX "/moc_.*\\.h$")

add_executable(multi_client_chat
    ${MULTI_CLIENT_CHAT_SOURCES}
    ${MULTI_CLIENT_CHAT_HEADERS}
)

target_include_directories(multi_client_chat
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src/qtllm"
)

target_link_libraries(multi_client_chat
    PRIVATE
    qtllm
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)
