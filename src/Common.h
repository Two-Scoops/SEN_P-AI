#ifndef COMMON_H
#define COMMON_H

#include "Match.h"
#include "PipeServer.h"
#include "SettingsDialog.h"
#include "StatSelectionDialog.h"
#include "StatsInfo.h"
#include "StatTableReader.h"
#include "TeamDataTable.h"

extern teamInfo currHome, currAway;
#include <QDebug>
#include <QString>
#include <sstream>
#include <iomanip>
#include <ctime>

inline QString applyFilenameFormat(QString qstr){
    QString home = currHome.name, away = currAway.name;
    qstr.replace("%HOME",home.remove(QRegExp("[^\\w@]")));
    qstr.replace("%AWAY",away.remove(QRegExp("[^\\w@]")));
    std::wstringstream string;
    std::time_t time;
    string<< std::put_time(std::gmtime(&time), qstr.toStdWString().data());
    return QString::fromStdWString(string.str());
}

#endif // COMMON_H
