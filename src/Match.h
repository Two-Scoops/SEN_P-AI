#ifndef MATCH_H
#define MATCH_H

#include <QWidget>
#include <QList>
#include <QVector>
#include <QDateTime>
#include <QJsonDocument>
#include <QMap>
#include "StatTableReader.h"
#include "StatSelectionDialog.h"
#include "StatsInfo.h"
#include <deque>
#include <sstream>
#include <iomanip>
#include <ctime>

class Match;

struct match_time {
    Match *match;
    int64_t timestamp;
    float gameMinute, injuryMinute;
    int32_t gameTick;

    QString minuteString() const;
    QJsonObject startEventObject() const;
};

struct stat_change {
    const match_time *when;
    stat_value newVal;
    int16_t statId;
    uint8_t segment;
    int8_t player;
};

enum eventType: unsigned char {
    goal, assist, ownGoal, yellowCard, redCard, subOn, subOff, teamsChanged, statsFound, statsLost, clockStopped, clockStarted
};

struct match_event {
    const match_time *when;
    int8_t player1, player2;
    eventType type;
    bool isHome;


    QString player1Name() const;
    QString player2Name() const;
    quint32 player1Id() const;
    quint32 player2Id() const;
    QString minuteString(){
        return when->minuteString();
    }
    QString eventLogString();
    QJsonDocument toJSON();
};


namespace Ui {
class Match;
}

class Match : public QWidget
{
    Q_OBJECT

public:
    explicit Match(StatTableReader *_reader, teamInfo homeInfo, teamInfo awayInfo, QWidget *parent = 0);
    ~Match();

    double& getLastValue(int player, int stat){
        return latestStats[vectorPosition(player,stat)];
    }

    void endMatch();

    QString toWikiFootballBox(QString current = QString());

    const uint8_t totalPlayers;
    const teamInfo home, away;
    int homeScore = 0, awayScore = 0;


    QString getPlayerName(int player) const{
        if(player < 0) return QString();
        return player > home.nPlayers ? away.player[player - home.nPlayers].name : home.player[player].name;
    }

    quint32 getPlayerId(int player) const{
        if(player < 0) return 0;
        return player > home.nPlayers ? away.player[player - home.nPlayers].ID : home.player[player].ID;
    }

    qint64 timestamp() const{ return matchCreationTime; }

    QString applyFilenameFormat(QString qstr, qint64 timestamp){
        QString homename = home.name, awayname = away.name;
        qstr.replace("%HOME",homename.remove(QRegExp("[^\\w@]")));
        qstr.replace("%AWAY",awayname.remove(QRegExp("[^\\w@]")));
        std::wstringstream string;
        std::time_t time = (std::time_t)(timestamp/1000);
        string<< std::put_time(std::gmtime(&time), qstr.toStdWString().data());
        return QString::fromStdWString(string.str());
    }
    //================Event handling==================//
public slots:
    void statsDisplayChanged(statsInfo &info);
    void showBenched(bool shown);
    void stats_found(uint8_t *data);

    void update(uint8_t *currData, uint8_t *prevData);

signals:
    void clock_stopped(quint64 timestamp, quint32 gameTick, float gameMinute, float injuryMinute);
    void clock_started(quint64 timestamp, quint32 gameTick, float gameMinute, float injuryMinute);
    void newEvent(match_event event, const QVector<match_event> &previousEvents);


    void table_lost(match_time *when);
    void table_found(match_time *when);

protected:
    //to handle copying the table via CTRL+C
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::Match *ui;
    StatTableReader *reader;
    static int darkenRate;
    static bool benchShown;

    //================Time-keeping==================//
    enum gameHalf {
        unknown =-1, firstHalf, firstHalfInjury, secondHalf, secondHalfInjury, firstET, firstETInjury, secondET, secondETInjury
    } currentHalf = unknown;
    int halfTimeTick = -1, extraTimeTick = -1, extraHalfTimeTick = -1;
    int tickLastUpdate = 0;
    int tickLastStopped = 0;
    int lastMinute = 0;
    int minuteTicks[121] = {-1};
    static int matchLength; //In (realtime) minutes (as specified in the match settings)


    //================Record-keeping==================//
    qint64 matchCreationTime, matchStartTime, matchEndTime;
    //2D array, Column-major, 64 rows, StatTableReader::stat_count columns
    QVector<double> latestStats;

    //List of times when stats were updated, DO NOT INSERT OR ERASE except from front/back
    std::deque<match_time> updateTimes;
    //Important events in the match, each referencing an updateTimes entry
    QVector<match_event> events;
    //All stat changes, separated by player and each referencing an updateTimes entry
    QVector<stat_change> playerStatHistory[64];

    static int goal_s, assist_s, owngoal_s, yellow_s, red_s, fmin_s, lmin_s, ticks_s;
    struct pEvent { bool is1; eventType type; };
    static QHash<int,pEvent> statEvents;


    //================Utility==================//
    void setLabels(int gameMinute, double time, double injuryTime);

    void recalcTime(int gameTick, float &gameMinute, float &injuryMinute);

    static int vectorPosition(int player, int stat){
        return player * (int)statsInfo::count() + stat;
    }
};

inline QString match_event::player1Name() const{ return when->match->getPlayerName(player1); }
inline QString match_event::player2Name() const{ return when->match->getPlayerName(player2); }
inline quint32 match_event::player1Id() const{ return when->match->getPlayerId(player1); }
inline quint32 match_event::player2Id() const{ return when->match->getPlayerId(player2); }

#endif // MATCH_H
