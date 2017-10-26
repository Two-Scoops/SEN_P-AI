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

struct statChange {
    qint64 timestamp;
    double oldVal;
    qint16 statId;
    qint8 player, gameMinute;
    qint32 gameTick;
};

enum eventType: unsigned char {
    goal, assist, ownGoal, yellowCard, redCard, subOn, subOff, teamsChanged, statsFound, statsLost, clockStopped, clockStarted
};

struct matchEvent {
    qint64 realTime;
    QString player1Name, player2Name;
    quint32 player1Id, player2Id;
    float gameMinute, injuryMinute;
    qint32 gameTick;
    eventType type;
    bool isHome;

    QString minuteString(){
        double time = gameMinute, injuryTime = injuryMinute;
        QString minuteString = QString::number((int)gameMinute);
        if(std::isnan(injuryTime))
            minuteString += "+";
        else if(injuryTime >= 0)
            minuteString += "+" + QString::number((int)injuryTime) + ":" + QString("%1").arg((int)(std::fmod(injuryTime,1.0) * 60),2,10,QChar('0'));
        else
            minuteString += ":" + QString("%1").arg((int)(std::fmod(time,1.0) * 60),2,10,QChar('0'));
        return minuteString;
    }

    QString eventLogString(){
        QString result = QString("%1").arg(minuteString(),7);
        switch(type){
        case goal:
        case assist:
            result +=              "' GOAL: " + player1Name;
            if(player2Id > 0)
                result += "\n         assist: " + player2Name;
            break;
        case ownGoal:
            result +=              "' OWN GOAL: "+ player1Name;
            break;
        case yellowCard:
            result +=              "' YELLOW: " + player1Name;
            break;
        case redCard:
            result +=              "' RED: " + player1Name;
            break;
        case subOn:
        case subOff:
            result +=              "' IN: " + player1Name;
            if(player2Id > 0)
                result += "\n         OUT: " + player2Name;
            break;
        default: return QString();
        }
        return result;
    }

    QJsonDocument toJSON(){
        QJsonObject res{
            {"type", "Event"},
            {"timestamp", realTime},
            {"gameMinute", gameMinute},
        };
        if(std::isnan(injuryMinute))
            res["injuryMinute"] = true;
        else if(injuryMinute >= 0)
            res["injuryMinute"] = injuryMinute;
        QJsonObject player1{
            {"name",player1Name},
            {"playerId",(qint64)player1Id}
        };
        QJsonObject player2{
            {"name",player2Name},
            {"playerId",(qint64)player2Id}
        };

        switch (type) {
        case goal:
        case assist:
            res["event"] = "Goal";
            res["team"] = isHome ? "Home" : "Away";
            res["scorer"] = player1;
            if(player2Id > 0)
                res["assister"] = player2;
            break;
        case ownGoal:
            res["event"] = "Own Goal";
            res["team"] = isHome ? "Home" : "Away";
            res["player"] = player1;
            break;
        case yellowCard:
            res["event"] = "Card";
            res["team"] = isHome ? "Home" : "Away";
            res["card"] = "Yellow";
            res["player"] = player1;
            break;
        case redCard:
            res["event"] = "Card";
            res["team"] = isHome ? "Home" : "Away";
            res["card"] = "Red";
            res["player"] = player1;
            break;
        case subOn:
        case subOff:
            res["event"] = "Player Sub";
            res["team"] = isHome ? "Home" : "Away";
            res["playerIn"] = player1;
            if(player2Id > 0)
                res["playerOut"] = player2;
            break;
        case clockStarted:
            res["event"] = "Clock Started";
            break;
        case clockStopped:
            res["event"] = "Clock Stopped";
            break;
        default:
            break;
        }
        return QJsonDocument(res);
    }
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
public slots:
    void statsDisplayChanged(statsInfo &info);
    void showBenched(bool shown);
    void stats_found(uint8_t *data);

    void update(uint8_t *data);

signals:
    void clock_stopped(quint64 timestamp, quint32 gameTick, float gameMinute, float injuryMinute);
    void clock_started(quint64 timestamp, quint32 gameTick, float gameMinute, float injuryMinute);
    void newEvent(matchEvent event, const QList<matchEvent> &previousEvents);


    void table_lost(qint64 timestamp, float gameMinute, float injuryMinute);
    void table_found(qint64 timestamp, float gameMinute, float injuryMinute, int homeScore, int awayScore);

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    Ui::Match *ui;
    StatTableReader *reader;
    static int darkenRate;

    enum gameHalf {
        unknown =-1, firstHalf, firstHalfInjury, secondHalf, secondHalfInjury, firstET, firstETInjury, secondET, secondETInjury
    } currentHalf = unknown;


    int halfTimeTick = -1, extraTimeTick = -1, extraHalfTimeTick = -1;
    int tickLastUpdate = 0;
    int tickLastStopped = 0;//TODO calculate this
    int lastMinute = 0;
    int minuteTicks[121] = {-1};
    static int matchLength; //In (realtime) minutes (as specified in the match settings)
    static bool benchShown;

    qint64 matchStartTime, matchEndTime;
    //2D array, Column-major, 64 rows, StatTableReader::stat_count columns
    QVector<double> latestStats;
    //Unsorted list of all stat changes that have occurred
    QList<statChange> statHistory;
    QList<matchEvent> events;

    void setLabels(int homeScore, int awayScore, int gameMinute, double time, double injuryTime);

    void recalcTime(int gameTick, float &gameMinute, float &injuryMinute);

    static int vectorPosition(int player, int stat){
        return player * (int)statsInfo::count() + stat;
    }

    QString getPlayerName(int player){
        return player > home.nPlayers ? away.player[player - home.nPlayers].name : home.player[player].name;
    }

    quint32 getPlayerId(int player){
        return player > home.nPlayers ? away.player[player - home.nPlayers].ID : home.player[player].ID;
    }
};

#endif // MATCH_H
