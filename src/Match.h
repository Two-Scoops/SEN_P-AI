#ifndef MATCH_H
#define MATCH_H

#include <QWidget>
#include <QList>
#include <QDateTime>
#include <QJsonDocument>
#include "StatTableReader.h"
#include "StatSelectionDialog.h"
#include "StatsInfo.h"
#include <unordered_map>
#include <vector>
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

enum eventType: char {
    unknown = -1, goal = 0, ownGoal, yellowCard, redCard, subOn,
    teamsChanged, statsFound, statsLost, clockStopped, clockStarted,
    //clockStopped reasons:
    foul, offside, goalkick, cornerkick, throwin, halfend
};

struct match_event {
    const match_time *when;
    int8_t player1, player2;
    eventType type, reason;
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

enum FileAction: bool{
    load = false,
    save = true
};


namespace Ui {
class Match;
}

class Match : public QWidget
{
    Q_OBJECT
    friend class MatchReader;
    QFontMetrics getFont() const;
    match_time* addUpdateTime(float gameMinute = -1.0f, int gameTick = 0, float injuryTime = -1.0f){
        qDebug("Function Entered");
        updateTimes.push_back(match_time{this,QDateTime::currentMSecsSinceEpoch(),gameMinute,injuryTime,gameTick});
        return &updateTimes.back();
    }
    void addEvent(const match_time *when, eventType type, int8_t p1, int8_t p2 = -1, eventType reason = unknown){
        qDebug("Function Entered");
        addEvent(match_event{when,p1,p2,type,reason, p1 < home.nPlayers});
    }
    void addEvent(match_event event);
    QString addStatCell(int r, int c, double val);
    void setColumnWidths(std::vector<int> &maxWidths);
    void setLabels(int gameMinute, double time, double injuryTime);
    void updateTableStat(int p, int s, double val, QString& str, bool changed);
    bool isHome(int player) const{
        return player < home.nPlayers;
    }
    void logEvent(match_event &event);
    int  getLogScroll();
    void setLogScroll(int val);

public:
    explicit Match(teamInfo homeInfo, teamInfo awayInfo, QWidget *parent = 0);
    explicit Match();
    void init();
    ~Match();

    double& getLastValue(int player, int stat){
        return latestStats[vectorPosition(player,stat)];
    }

    void endMatch(int length);

    QString toWikiFootballBox(QString current = QString());

    uint8_t totalPlayers;
    teamInfo home, away;
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
        qDebug("Function Entered");
        QString homename = home.name, awayname = away.name;
        qstr.replace("%HOME",homename.remove(QRegExp("[^\\w@]")));
        qstr.replace("%AWAY",awayname.remove(QRegExp("[^\\w@]")));
        std::wstringstream string;
        std::time_t time = (std::time_t)(timestamp/1000);
        string<< std::put_time(std::gmtime(&time), qstr.toStdWString().data());
        qDebug("Function returned");
        return QString::fromStdWString(string.str());
    }

    template<FileAction act>
    static bool file(Match *match, QString filename);
    //================Event handling==================//
public slots:
    void statsDisplayChanged(statsInfo &info);
    void showBenched(bool shown);

signals:
protected:
    //to handle copying the table via CTRL+C
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::Match *ui;
    static int darkenRate;

    //================Time-keeping==================//
    int halfTimeTick = -1, extraTimeTick = -1, extraHalfTimeTick = -1;
    uint8_t matchLength = 0;


    //================Record-keeping==================//
    qint64 matchCreationTime, matchStartTime, matchEndTime;
    //2D array, Column-major, 64 rows, StatTableReader::stat_count columns
    std::vector<double> latestStats;

    //List of times when stats were updated, DO NOT INSERT OR ERASE except from front/back
    std::deque<match_time> updateTimes;
    //Important events in the match, each referencing an updateTimes entry
    std::vector<match_event> events;
    //All stat changes, separated by player and each referencing an updateTimes entry
    std::vector<stat_change> playerStatHistory[64];


    //================Utility==================//
    static int vectorPosition(int player, int stat){
        return player * (int)statsInfo::count() + stat;
    }
};

inline QString match_event::player1Name() const{ return when->match->getPlayerName(player1); }
inline QString match_event::player2Name() const{ return when->match->getPlayerName(player2); }
inline quint32 match_event::player1Id() const{ return when->match->getPlayerId(player1); }
inline quint32 match_event::player2Id() const{ return when->match->getPlayerId(player2); }
extern template bool Match::file<load>(Match *match, QString filename);
extern template bool Match::file<save>(Match *match, QString filename);

#endif // MATCH_H
