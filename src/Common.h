#ifndef COMMON_H
#define COMMON_H
//Version
#define MAJOR 1
#define MINOR 2
#define PATCH 3

#include "Match.h"
#include "PipeServer.h"
#include "SettingsDialog.h"
#include "StatSelectionDialog.h"
#include "StatsInfo.h"
#include "StatTableReader.h"
#include "TeamDataTable.h"
#include <QDebug>
#include <QString>

#define dbgReturn(...) do{ qDebug("Function returned"); __VA_ARGS__; }while(false)

#endif // COMMON_H
