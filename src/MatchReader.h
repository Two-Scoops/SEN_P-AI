#ifndef MATCHREADER_H
#define MATCHREADER_H

#include <QWidget>
#include <QDebug>
#include <QString>
#include "Match.h"
#include "PipeServer.h"
#include "StatsInfo.h"
#include "StatTableReader.h"

namespace Ui {
class MatchReader;
}

class MatchReader : public QWidget
{
    Q_OBJECT

public:
    explicit MatchReader(QWidget *parent = 0);
    ~MatchReader();

    void endMatch();

    teamInfo Home, Away;
signals:
    void new_match(Match *match);
    void done_match(QString tabtext);
    void invalidate_match();

    void clock_stopped(quint64 timestamp, quint32 gameTick, float gameMinute, float injuryMinute);
    void clock_started(quint64 timestamp, quint32 gameTick, float gameMinute, float injuryMinute);
    void newEvent(match_event event);

    void table_lost_event(match_time *when);
    void table_found_event(match_time *when);

    void matchUpdated();

public slots:
    void settingsChanged(){
        updateTimer->setInterval(QSettings().value("UpdateRate",100).toInt());
    }
    void show_benced(bool shown){
        benchShown = shown;
    }

private slots:
    void stats_lost();
    void stats_found(uint8_t *data);
    void teams_changed(qint64 timestamp, teamInfo home, teamInfo away);
    void update(uint8_t *currData, uint8_t *prevData);
    void showTime();
private:
    Ui::MatchReader *ui;
    StatTableReader *reader = nullptr;
    PipeServer *server = nullptr;
    QTimer *updateTimer = nullptr;
    //Current Match
    Match *match = nullptr;
    bool benchShown = false;

    //================Time-keeping==================//
    enum gameHalf {
        unknownHalf =-1, firstHalf, firstHalfInjury, secondHalf, secondHalfInjury, firstET, firstETInjury, secondET, secondETInjury, penalties
    } currentHalf;
    int halfTimeTick, extraTimeTick, extraHalfTimeTick;
    int tickLastUpdate, tickLastStopped, lastMinute;
    int minuteTicks[121];
    int thisMatchLength;
    eventType lastStopReason;
    int matchLength = 10; //In (realtime) minutes (as specified in the match settings)
    void resetTime(){
        currentHalf = firstHalf;
        halfTimeTick = extraTimeTick = extraHalfTimeTick = -1;
        tickLastUpdate =  tickLastStopped = lastMinute = thisMatchLength = 0;
        lastStopReason = eventType::unknown;
        for(int &i: minuteTicks)
            i = -1;
        minuteTicks[0] = 0;
    }

    //==========Stat indexing=========//
    enum stat_index {
        goal_s, assist_s, owngoal_s, yellow_s, red_s, fmin_s, lmin_s, ticks_s,
        foul_s, offside_s, pass_s, gudPass_s, intrcpt_s, shot_s, gudShot_s, save_s,
        statIndexCount
    };
    int si[statIndexCount];
    struct pEvent { bool is1; eventType type; };
    std::unordered_map<int,pEvent> statEvents;

    eventType proccessStatEvents(std::vector<match_event> &eventsThisUpdate, const std::vector<stat_change> &changes, match_time *when);
    int collectStatChanges(uint8_t *currData, uint8_t *prevData, std::vector<stat_change> &changes, match_time *when);
    void calcUpdateTime(match_time *when, int segment);
    void newMatch();
};

#endif // MATCHREADER_H
