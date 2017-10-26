#include "MainWindow.h"
#include <QApplication>

#include <QErrorMessage>

#define MAJOR 1
#define MINOR 1
#define PATCH 0
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define VERSION() STR(MAJOR) "." STR(MINOR) "." STR(PATCH)

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("The4chanCup");
    QCoreApplication::setOrganizationDomain("implyingrigged.info");
    QCoreApplication::setApplicationName("SEN_P-AI");
    QCoreApplication::setApplicationVersion(VERSION());

    if(StatTableReader::GetDebugPrivilege()){
        MainWindow w;
        w.show();
        return a.exec();
    } else {
        QErrorMessage msg;
        msg.showMessage("Program must have admin privilages.\nPlease \"Run as administrator\"");
        return a.exec();
    }
}
