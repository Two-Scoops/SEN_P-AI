#ifndef STATTABLEREADER_H
#define STATTABLEREADER_H
#include <stdint.h>
#include <cmath>
#include <QString>
#include <QtCore>
#include <QPushButton>
#include "TeamDataTable.h"

#define MAX_PLAYERS 32

struct playerInfo {
    QString name;
    uint32_t ID;
    uint8_t condition;
};

struct teamInfo {
    QString name;
    uint32_t ID;
    uint8_t nPlayers;
    playerInfo player[32];
};


class StatTableReader: public QObject
{
    Q_OBJECT
public:
    StatTableReader(const wchar_t *name, QPushButton *butte, QObject *parent = 0): QObject(parent),
        _nHome(0), _nAway(0), _nPlayers(0), _homeID(0), _awayID(0),
        data(nullptr), currData(nullptr), prevData(nullptr),
        basePtr(nullptr), homePtr(nullptr), awayPtr(nullptr),
        proccessName(name), processHandle(nullptr),
        teamDataPtr(nullptr), currTeamData(&(teamData[0])), prevTeamData(&(teamData[1])){
        qDebug("Function Entered");
        butt = butte;
        connect(butte,&QPushButton::released,this,&StatTableReader::bruteForce);
    }

    int nHome() const{ return _nHome; }
    int nAway() const{ return _nAway; }
    int nPlayers() const{ return _nPlayers; }
    int homeID() const{ return _homeID; }
    int awayID() const{ return _awayID; }

    static int GetDebugPrivilege();
    const static size_t statCount;


    enum KnownStats{
        playerID = 0, goalsScored = 1, assists = 5, yellowCards = 46, redCards = 47, saves = 112, firstMinuteOnPitch = 114, lastMinuteOnPitch = 115, currentPosition = 123,
        ticksAsGK = 124, ticksAsCF = 136,
        playerRating = 138, realTimeOnPitch = 140,
    };
public slots:
    void update();
    void bruteForce(){
        if(status == Teams){
            status = BruteForce;
            emit action_changed(QStringLiteral("SEARCHING HARDER..."));
        }
    }

signals:
    void status_changed(QString str);
    void action_changed(QString str);

    void statTableUpdate(uint8_t *curr, uint8_t *prev);
    void table_lost();
    void table_found(uint8_t *data);
    void teamData_lost();
    void teamsChanged(qint64 timestamp, teamInfo home, teamInfo away);

    void teamDataChanged(size_t offset, uint8_t prevByte, uint8_t currByte);
private:
    int _nHome, _nAway, _nPlayers;
    uint32_t _homeID, _awayID;
    uint8_t *data, *currData, *prevData;

    void *basePtr, *homePtr, *awayPtr;
    const wchar_t* proccessName;
    void* processHandle;

    void *teamDataPtr;
    teamDataTable *currTeamData, *prevTeamData;
    teamDataTable teamData[2];

    enum {
        None, Process, Teams, Stats, BruteForce
    } status = None;
    QPushButton *butt;

    bool findProcess();
    bool updateProcess();
    bool findTeams();
    bool updateTeams();
    bool findStats();
    bool updateStats();
    bool bruteForceStats();
    uint32_t *searchSpace = nullptr, searchSize = 0;
    bool doStatsFound(uint8_t homeCount, uint8_t awayCount);
};



#endif // STATTABLEREADER_H
