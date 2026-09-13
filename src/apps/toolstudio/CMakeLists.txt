find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network Sql)

file(GLOB_RECURSE TOOLSTUDIO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER TOOLSTUDIO_SOURCES EXCLUDE REGEX "/(release|debug)/")
list(FILTER TOOLSTUDIO_SOURCES EXCLUDE REGEX "/moc_.*\\.cpp$")

file(GLOB_RECURSE TOOLSTUDIO_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER TOOLSTUDIO_HEADERS EXCLUDE REGEX "/(release|debug)/")
list(FILTER TOOLSTUDIO_HEADERS EXCLUDE REGEX "/moc_.*\\.h$")

add_executable(toolstudio
    ${TOOLSTUDIO_SOURCES}
    ${TOOLSTUDIO_HEADERS}
)

target_include_directories(toolstudio
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src/qtllm"
)

target_link_libraries(toolstudio
    PRIVATE
    qtllm
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)
