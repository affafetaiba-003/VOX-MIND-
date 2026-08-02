QT += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET   = VoxMind
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h \
    voxmind_engine.h

win32 {
    LIBS += -lsapi    \
            -lole32   \
            -loleaut32\
            -lshell32 \
            -luser32  \
            -ladvapi32\
            -lpowrprof\
            -lwinmm

    # Architecture + Windows version defines — MUST come before any include
    DEFINES += _AMD64_
    DEFINES += UNICODE
    DEFINES += _UNICODE
    DEFINES += _CRT_SECURE_NO_WARNINGS
    DEFINES += _WIN32_WINNT=0x0601
    DEFINES += NTDDI_VERSION=0x06010000
    DEFINES += WIN32_LEAN_AND_MEAN

    # Windows SDK include paths — adjust version number if needed
    INCLUDEPATH += "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um"
    INCLUDEPATH += "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared"
    INCLUDEPATH += "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt"

    # MinGW: suppress pragma warnings and unknown attributes
    win32-g++ {
        QMAKE_CXXFLAGS += -Wno-unknown-pragmas
        QMAKE_CXXFLAGS += -Wno-attributes
        QMAKE_CXXFLAGS += -Wno-ignored-attributes
        QMAKE_CXXFLAGS += -Wno-multichar
        QMAKE_CXXFLAGS += -w
    }

    # MSVC: suppress specific warnings
    win32-msvc* {
        QMAKE_CXXFLAGS += /wd4996
        QMAKE_CXXFLAGS += /wd4244
        QMAKE_CXXFLAGS += /wd4267
    }
}
