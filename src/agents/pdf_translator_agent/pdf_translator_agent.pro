TEMPLATE = app
TARGET = pdf_translator_agent
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
    ../../qtllm/tools/mcp \
    ../../qtllm/logging \
    ../../qtllm/toolsinside \
    . \
    app \
    ui \
    domain \
    pipeline \
    skills/core \
    skills/builtin \
    skills/mcp \
    storage \
    viewer

HEADERS += \
    app/applicationbootstrap.h \
    ui/mainwindow.h \
    domain/documenttranslationtask.h \
    pipeline/documentworkflowcontroller.h \
    pipeline/batchtranslationqueuecontroller.h \
    skills/core/skilltypes.h \
    skills/core/iskill.h \
    skills/core/skillregistry.h \
    skills/core/modelrouter.h \
    skills/builtin/languagedetectskill.h \
    skills/builtin/pdftextextractskill.h \
    skills/builtin/termbaseresolveskill.h \
    skills/builtin/chunktranslateskill.h \
    skills/builtin/documentassembleskill.h \
    skills/mcp/mcpgateway.h \
    storage/manifestrepository.h \
    viewer/comparereaderwidget.h

SOURCES += \
    main.cpp \
    app/applicationbootstrap.cpp \
    ui/mainwindow.cpp \
    domain/documenttranslationtask.cpp \
    pipeline/documentworkflowcontroller.cpp \
    pipeline/batchtranslationqueuecontroller.cpp \
    skills/core/skillregistry.cpp \
    skills/core/modelrouter.cpp \
    skills/builtin/languagedetectskill.cpp \
    skills/builtin/pdftextextractskill.cpp \
    skills/builtin/termbaseresolveskill.cpp \
    skills/builtin/chunktranslateskill.cpp \
    skills/builtin/documentassembleskill.cpp \
    skills/mcp/mcpgateway.cpp \
    storage/manifestrepository.cpp \
    viewer/comparereaderwidget.cpp

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
