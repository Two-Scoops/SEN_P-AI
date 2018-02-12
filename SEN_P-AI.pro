#-------------------------------------------------
#
# Project created by QtCreator 2017-09-14T18:28:54
#
#-------------------------------------------------

QT       += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SEN_P-AI
TEMPLATE = app
CONFIG += c++11 static


SOURCES +=\
    src/CustomTableStyle.cpp \
    src/main.cpp \
    src/MainWindow.cpp \
    src/Match.cpp \
    src/PipeServer.cpp \
    src/SettingsDialog.cpp \
    src/StatSelectionDialog.cpp \
    src/StatsInfo.cpp \
    src/StatTableReader.cpp \
    src/WikiEditor.cpp \
    lib/QR-Code-generator/QrSegment.cpp \
    lib/QR-Code-generator/QrCode.cpp \
    lib/QR-Code-generator/BitBuffer.cpp \
    src/QrTimestamp.cpp \
    src/MatchReader.cpp

HEADERS  += \
    src/Common.h \
    src/CustomTableStyle.h \
    src/MainWindow.h \
    src/Match.h \
    src/PipeServer.h \
    src/SettingsDialog.h \
    src/StatSelectionDialog.h \
    src/StatsInfo.h \
    src/StatTableReader.h \
    src/TeamDataTable.h \
    src/WikiEditor.h \
    src/WikiFootballbox.h \
    lib/QR-Code-generator/QrSegment.hpp \
    lib/QR-Code-generator/QrCode.hpp \
    lib/QR-Code-generator/BitBuffer.hpp \
    src/QrTimestamp.h \
    src/MatchReader.h

FORMS    += \
    ui/MainWindow.ui \
    ui/Match.ui \
    ui/SettingsDialog.ui \
    ui/StatSelectionDialog.ui \
    ui/MatchReader.ui


QMAKE_LFLAGS += /MANIFESTUAC:$$quote(\"level=\'requireAdministrator\' uiAccess=\'false\'\")
LIBS += -lAdvapi32
RC_FILE = resources/resource.rc


RESOURCES +=

DISTFILES += \
    resources/ohayou_momiji.ico \
    resources/resource.rc \
    LICENSE.txt \
    README.md \
    .gitignore \
    lib/QR-Code-generator/Readme.markdown
