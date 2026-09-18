find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2)

file(GLOB_RECURSE TOOLS_INSIDE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER TOOLS_INSIDE_SOURCES EXCLUDE REGEX "/(release|debug)/")
list(FILTER TOOLS_INSIDE_SOURCES EXCLUDE REGEX "/moc_.*\\.cpp$")

file(GLOB_RECURSE TOOLS_INSIDE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER TOOLS_INSIDE_HEADERS EXCLUDE REGEX "/(release|debug)/")
list(FILTER TOOLS_INSIDE_HEADERS EXCLUDE REGEX "/moc_.*\\.h$")

add_executable(tools_inside
    ${TOOLS_INSIDE_SOURCES}
    ${TOOLS_INSIDE_HEADERS}
    "${CMAKE_CURRENT_SOURCE_DIR}/qml.qrc"
)

target_link_libraries(tools_inside
    PRIVATE
    QtLlm::Conversation
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
)
