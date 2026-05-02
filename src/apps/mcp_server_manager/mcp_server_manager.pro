TEMPLATE = app
TARGET = mcp_server_manager
CONFIG += c++17 warn_on link_prl
QT += core gui widgets network sql

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
    ../../qtllm/tools/mcp

HEADERS += \
    manualmcpserverdialog.h \
    mcpchatwindow.h \
    mcpscanlocationrepository.h \
    mcpserverdiscoveryservice.h \
    mcpservermanagerwindow.h

SOURCES += \
    main.cpp \
    manualmcpserverdialog.cpp \
    mcpchatwindow.cpp \
    mcpscanlocationrepository.cpp \
    mcpserverdiscoveryservice.cpp \
    mcpservermanagerwindow.cpp

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
