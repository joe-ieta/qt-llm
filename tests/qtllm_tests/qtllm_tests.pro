TEMPLATE = app
TARGET = qtllm_tests
CONFIG += c++17 testcase console warn_on link_prl
QT += core network sql testlib

HEADERS += \
    tst_qtllm.h

INCLUDEPATH += \
    ../../src/qtllm \
    ../../src/qtllm/identity \
    ../../src/qtllm/core \
    ../../src/qtllm/providers \
    ../../src/qtllm/network \
    ../../src/qtllm/streaming \
    ../../src/qtllm/logging \
    ../../src/qtllm/tools \
    ../../src/qtllm/tools/runtime \
    ../../src/qtllm/tools/mcp

SOURCES += \
    tst_qtllm.cpp

QTLLM_LIB_DIR = $$clean_path($$OUT_PWD/../../src/qtllm/lib)
CONFIG(debug, debug|release): QTLLM_LIB_DIR = $$clean_path($$QTLLM_LIB_DIR/debug)
CONFIG(release, debug|release): QTLLM_LIB_DIR = $$clean_path($$QTLLM_LIB_DIR/release)

win32-msvc* {
    QTLLM_LIB_FILE = $$QTLLM_LIB_DIR/qtllm.lib
} else {
    QTLLM_LIB_FILE = $$QTLLM_LIB_DIR/libqtllm.a
}

LIBS += -L$$QTLLM_LIB_DIR -lqtllm
PRE_TARGETDEPS += $$QTLLM_LIB_FILE
