TEMPLATE = app
TARGET = toolstudio
CONFIG += c++17 warn_on link_prl
QT += core gui widgets network sql

msvc:QMAKE_CXXFLAGS += /utf-8

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

INCLUDEPATH += \
    ../../qtllm \
    ../../qtllm/core \
    ../../qtllm/tools \
    ../../qtllm/tools/runtime \
    ../../qtllm/tools/mcp \
    ../../qtllm/toolsstudio

HEADERS += \
    toolstudiocontroller.h \
    toolimportpreviewdialog.h \
    toolstudiowindow.h \
    toolsettingsdialog.h

SOURCES += \
    main.cpp \
    toolstudiocontroller.cpp \
    toolimportpreviewdialog.cpp \
    toolstudiowindow.cpp \
    toolsettingsdialog.cpp

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
