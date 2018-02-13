#include "MainWindow.h"
#include <QApplication>
#include <QErrorMessage>
#include <QDebug>
#include <cstdio>
#include <cstring>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define VERSION() STR(MAJOR) "." STR(MINOR) "." STR(PATCH)

QtMessageHandler defaultHandler;
FILE *dbg;
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg){
    const char *msgType = "Unknown";
    switch (type) {
    case QtDebugMsg:   msgType =    "Debug"; break;
    case QtInfoMsg:    msgType =     "Info"; break;
    case QtWarningMsg: msgType =  "Warning"; break;
    case QtCriticalMsg:msgType = "Critical"; break;
    case QtFatalMsg:   msgType =    "Fatal"; break;
    }
    const char *fnName = context.function;
    for(fnName = std::strchr(context.function,'('); fnName > context.function && *fnName != ' ';fnName--);

    QByteArray localMsg = msg.toLocal8Bit();
    std::fprintf(dbg,"%s: %-18s(line%4u of %s)\n", msgType, localMsg.constData(), context.line, fnName+1);
    std::fflush(dbg);
    defaultHandler(type,context,msg);
}

int main(int argc, char *argv[])
{
    dbg = std::fopen("SEN_P-AI_debug.log","w");
    defaultHandler = qInstallMessageHandler(messageHandler);
    QApplication a(argc, argv);
    qDebug("SEN:P-AI Started");
    QCoreApplication::setOrganizationName("The4chanCup");
    QCoreApplication::setOrganizationDomain("implyingrigged.info");
    QCoreApplication::setApplicationName("SEN_P-AI");
    QCoreApplication::setApplicationVersion(VERSION());

    qDebug("Getting debug privileges");
    if(StatTableReader::GetDebugPrivilege()){
        qDebug("Got privileges");
        MainWindow w;
        w.show();
        return a.exec();
    } else {
        qDebug("Privileges denied");
        QErrorMessage msg;
        msg.showMessage("Program must have admin privilages.\nPlease \"Run as administrator\"");
        return a.exec();
    }
}
