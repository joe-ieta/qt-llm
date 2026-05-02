TEMPLATE = app
TARGET = tools_inside
CONFIG += c++17 warn_on link_prl
QT += core gui quick quickcontrols2 sql network

msvc:QMAKE_CXXFLAGS += /utf-8

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

INCLUDEPATH += \
    ../../qtllm \
    ../../qtllm/core \
    ../../qtllm/providers \
    ../../qtllm/network \
    ../../qtllm/streaming \
    ../../qtllm/chat \
    ../../qtllm/storage \
    ../../qtllm/profile \
    ../../qtllm/tools \
    ../../qtllm/tools/runtime \
    ../../qtllm/toolsinside

HEADERS += \
    toolsinsidebrowser.h

SOURCES += \
    main.cpp \
    toolsinsidebrowser.cpp

RESOURCES += \
    qml.qrc

QTLLM_LIB_DIR = $$clean_path($$OUT_PWD/../../qtllm/lib)
CONFIG(debug, debug|release): QTLLM_LIB_DIR = $$clean_path($$QTLLM_LIB_DIR/debug)
CONFIG(release, debug|release): QTLLM_LIB_DIR = $$clean_path($$QTLLM_LIB_DIR/release)

win32-msvc* {
    QTLLM_LIB_FILE = $$QTLLM_LIB_DIR/qtllm.lib
} else {
    QTLLM_LIB_FILE = $$QTLLM_LIB_DIR/libqtllm.a
}

LIBS += -L$$QTLLM_LIB_DIR -lqtllm
PRE_TARGETDEPS += $$QTLLM_LIB_FILE
