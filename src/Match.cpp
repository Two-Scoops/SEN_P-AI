#include "Match.h"
#include "ui_match.h"
#include "CustomTableStyle.h"
#include <QScrollBar>
#include <QHash>
#include <utility>
#include <cmath>
int Match::matchLength = 10;
bool Match::benchShown = false;

Match::Match(StatTableReader *_reader, teamInfo homeInfo, teamInfo awayInfo, QWidget *parent) :
    QWidget(parent),
    home(homeInfo), away(awayInfo), totalPlayers(homeInfo.nPlayers + awayInfo.nPlayers), reader(_reader),
    ui(new Ui::Match)
{
    ui->setupUi(this);
    ui->homeLabel->setText(homeInfo.name);
    ui->awayLabel->setText(awayInfo.name);
    ui->statsTable->setColumnCount((int)statsInfo::count());
    ui->statsTable->setRowCount(totalPlayers);

    latestStats.resize(totalPlayers * (int)statsInfo::count());
    latestStats.fill(0,totalPlayers * (int)statsInfo::count());

    currentHalf = firstHalf;
    minuteTicks[0] = 0;

    for(int p = 0; p < totalPlayers; ++p){
        QString name = p < home.nPlayers ? home.player[p].name : away.player[p - home.nPlayers].name;
        ui->statsTable->setVerticalHeaderItem(p, new QTableWidgetItem(name));
    }
    for(int s = 0; s < statsInfo::count(); ++s){
        ui->statsTable->setHorizontalHeaderItem(s,new QTableWidgetItem(statsInfo::getStat(s).name));
    }

    QHeaderView *view = ui->statsTable->horizontalHeader();
    view->setSectionResizeMode(QHeaderView::Fixed);
    view->setStretchLastSection(true);

    ui->statsTable->setFocusPolicy(Qt::StrongFocus);
}


void Match::keyPressEvent(QKeyEvent *event){
    if(event->matches(QKeySequence::Copy) && ui->statsTable->hasFocus()){
        event->accept();
        QString text;
        QModelIndexList selected = ui->statsTable->selectionModel()->selectedIndexes();
        if(selected.empty())
            return;
        int row = selected[0].row(), col = selected[0].column();
        for(QModelIndex &index: selected){
            const int nrow = index.row(), ncol = index.column();
            if(ui->statsTable->isRowHidden(nrow) || ui->statsTable->isColumnHidden(ncol))
                continue;
            if(row != nrow)
                text += "\n";
            else if(col != ncol)
                text += "\t";
            row = nrow;
            col = ncol;
            text += index.data().toString();
        }

        QApplication::clipboard()->setText(text);
    } else {
        event->ignore();
    }
}

void Match::stats_found(uint8_t *data){
    matchStartTime = QDateTime::currentMSecsSinceEpoch();
    QFontMetrics fm(ui->statsTable->font());
    QVector<int> maxWidths = QVector<int>((int)statsInfo::count(),0);

    int homeScore = 0, awayScore = 0;
    for(int p = 0; p < totalPlayers; ++p){
        uint8_t *currPlayer = data + (p * statsInfo::entrySize());
        for(int s = 0; s < statsInfo::count(); ++s){
            const PESstat &stat = statsInfo::getStat(s);
            double val = stat.getValue(currPlayer);

            QString str = QString::number(val,'g',10);
            QTableWidgetItem *item = new QTableWidgetItem(str);
            //TODO use actual team colors
            item->setBackground(QBrush(QColor(p < home.nPlayers ? "#E5E5FC" : "#FFE9E8")));
            ui->statsTable->setItem(p,s,item);
            maxWidths[s] = std::max(maxWidths[s], fm.width(str));
            getLastValue(p,s) = val;
        }
        lastMinute = std::max(lastMinute,(int)statsInfo::getStat("Last Minute on Pitch").getValue(currPlayer));
        tickLastUpdate = std::max(tickLastUpdate,(int)statsInfo::getStat("Game Ticks Played").getValue(currPlayer));
        for(int i = statsInfo::getStat("Goals Scored").getValue(currPlayer); i > 0; --i){
            events.append(matchEvent{matchStartTime,getPlayerName(p),QString(),getPlayerId(p),0,(float)lastMinute,-1,tickLastUpdate,goal, p < home.nPlayers});
            (p < home.nPlayers ? homeScore : awayScore) += 1;
        }
        for(int i = statsInfo::getStat("Yellow Cards").getValue(currPlayer); i > 0; --i){
            events.append(matchEvent{matchStartTime,getPlayerName(p),QString(),getPlayerId(p),0,(float)lastMinute,-1,tickLastUpdate,yellowCard, p < home.nPlayers});
        }
        for(int i = statsInfo::getStat("Red Cards").getValue(currPlayer); i > 0; --i){
            events.append(matchEvent{matchStartTime,getPlayerName(p),QString(),getPlayerId(p),0,(float)lastMinute,-1,tickLastUpdate,redCard, p < home.nPlayers});
        }
    }

    //Update the table column widths
    for(int s = 0; s < statsInfo::count(); ++s){
        ui->statsTable->setColumnWidth(s, std::max(20,maxWidths[s]+8));
    }

    setLabels(homeScore,awayScore,lastMinute,lastMinute,-1);

    for(matchEvent &event: events){
        //TODO use actual team colors
        ui->eventLog->setTextBackgroundColor(QColor(event.isHome ? "#e0e0fc" : "#ffe0dd"));
        QString logStr = event.eventLogString();
        if(!logStr.isEmpty())
            ui->eventLog->append(logStr);
    }

    emit table_found(QDateTime::currentMSecsSinceEpoch(),lastMinute,-1,homeScore,awayScore);
}

void Match::statsDisplayChanged(statsInfo &info){
    int minHeight = 0;
    QHeaderView *header = ui->statsTable->horizontalHeader();
    for(int i = 0; i < info.count(); ++i){
        header->moveSection(header->visualIndex(i),info[i].visualIndex);
        header->setSectionHidden(i,!info[i].shown);
        if(info[i].shown){
            minHeight = std::max(minHeight,QFontMetrics(ui->statsTable->font()).width(info[i].name));
        }
    }
    header->setMinimumHeight(minHeight+8);
}

void Match::update(uint8_t *data){

    //Delare a local struct to hold a bunch of variables so that they can be passed to non-capturing lambdas
    struct data_t{
        //collects important events to be processed
        QList<matchEvent> eventsThisUpdate;
        int gameMinute = 0;
        int gameTick = 0;
        qint64 timestamp;
        Match *match;

        void setPlayerEvent(int player, bool is1, eventType type){
            matchEvent newEvent{timestamp, QString(), QString(), 0, 0, -1, -1, -1, type, player < match->home.nPlayers};
            matchEvent *event = &newEvent;
            for(matchEvent &e : eventsThisUpdate){
                if(e.type == type && (is1 ? e.player1Id : e.player2Id) <= 0)
                    event = &e;
            }
            (is1 ? event->player1Name : event->player2Name) = match->getPlayerName(player);
            (is1 ? event->player1Id : event->player2Id) = match->getPlayerId(player);
            if(event == &newEvent)
                eventsThisUpdate.append(newEvent);
        }

        //void updateMinute(double val, int p){ gameMinute = std::max(gameMinute,(qint16)val); }
    } local;
    local.match = this;
    local.gameMinute = lastMinute;
    local.gameTick = tickLastUpdate;
    qint64 &timestamp = local.timestamp;
    timestamp = QDateTime::currentMSecsSinceEpoch();
    //qDebug() << "Update at " << QDateTime::currentDateTime().toString();

    typedef void (*eventFN)(data_t &local, double val, int p);
    #define eventLAMBDA(LOCAL,VAL,P)  [](data_t &LOCAL, double VAL, int P)

    //Holds special case code for some stats, think of it like a weird switch statement that actually gets used later in the function
    const QHash<QString,eventFN> statEvents = {
    #define PAIR(STR,FN) std::pair<QString,eventFN>(QStringLiteral(STR), FN )
        PAIR("Goals Scored",          eventLAMBDA(local,   ,p){ local.setPlayerEvent(p, true,goal);      }),
        PAIR("Assists",               eventLAMBDA(local,   ,p){ local.setPlayerEvent(p,false,goal);      }),
        PAIR("Yellow Cards",          eventLAMBDA(local,   ,p){ local.setPlayerEvent(p,true,yellowCard); }),
        PAIR("Red Cards",             eventLAMBDA(local,   ,p){ local.setPlayerEvent(p,true,redCard);    }),
        PAIR("First Minute on Pitch", eventLAMBDA(local,   ,p){ local.setPlayerEvent(p,true,subOn);      }),
        PAIR("Last Minute on Pitch",  eventLAMBDA(local,val, ){ local.gameMinute = std::max(local.gameMinute,(int)val); }),
        PAIR("Game Ticks Played",     eventLAMBDA(local,val, ){ local.gameTick = std::max(local.gameTick,(int)val); }),
    };
    //This is better than a switch since because stat indicies could change with the adding and removing of stats


    //The current segment of the match, gets set when a segmented stat change is detected, otherwise we don't need it anyway
    int segment = -1;//1st half: [0-2], 2nd half: [3-5], 1st ET: [6-7?], 2nd ET: [7?-8?]  //TODO figure out if segment 8 is used
    QList<statChange> changes;

    QFontMetrics fm(ui->statsTable->font());
    QVector<int> maxWidths = QVector<int>((int)statsInfo::count(),0);

    //For every stat of every player
    for(int p = 0; p < totalPlayers; ++p){
        uint8_t *currPlayer = data + (p * statsInfo::entrySize());

        for(int s = 0; s < statsInfo::count(); ++s){
            const PESstat &stat = statsInfo::getStat(s);
            double curr = stat.getValue(currPlayer);
            double prev = latestStats[vectorPosition(p,s)];

            //update text width
            QString str = QString::number(curr,'g',10);
            maxWidths[s] = std::max(maxWidths[s], fm.width(str));

            QTableWidgetItem *item = ui->statsTable->item(p,s);
            //Update the stat
            if(curr != prev){
                latestStats[vectorPosition(p,s)] = curr;
                changes.push_back(statChange{timestamp,prev,(qint16)s,(qint8)p,-1,-1});

                //Update the Table item
                item->setText(str);
                item->setTextColor(Qt::red);

                //If we've yet to figure out what segment we're in
                if(segment < 0 && stat.count == 9){
                    //check each segment, starting with the last, and stop when the value is non-zero
                    for(segment = 8; segment >= 0 && stat.getSegmentValue(currPlayer,segment) == 0; --segment);
                }

                //Check if the stat has a special case and if so, pass the local data, stat value, and player to it
                auto iter = statEvents.constFind(stat.name);
                if(iter != statEvents.constEnd())
                    iter.value()(local,curr,p);

            } else { //Stat did not change
                //Darken the item text color so it slowly fades from red to black
                int h, s, v, a;
                item->textColor().getHsv(&h,&s,&v,&a);
                if(v > 0)
                    item->setTextColor(QColor::fromHsv(h, s, std::max(0,v-darkenRate), a));
            }//End If/Else stat was updated
        }//End For each stat
    }//End For each player
    //qDebug() << changes.size() <<" Stat Changes read";

    //Update the table column widths
    for(int s = 0; s < statsInfo::count(); ++s){
        ui->statsTable->setColumnWidth(s, std::max(20,maxWidths[s]+8));
    }
    //qDebug() << "Widths updated";

    //Calculate the current state of the match clock
    int &gameMinute = local.gameMinute, &gameTick = local.gameTick;
    //bool minuteChanged = gameMinute-1 == lastMinute;
    //qDebug() << "gameMinute: " << gameMinute << ", gameTick: "<<gameTick;

    //Figure out what half we're in
    if(gameMinute-1 == lastMinute){
        minuteTicks[gameMinute] = gameTick;

        if(gameMinute  <   0) currentHalf = unknown;
        else if(gameMinute  <  45) currentHalf = firstHalf;
        else if(gameMinute ==  45) currentHalf = firstHalfInjury;
        else if(gameMinute  <  90) currentHalf = secondHalf;
        else if(gameMinute ==  90) currentHalf = secondHalfInjury;
        else if(gameMinute  < 105) currentHalf = firstET;
        else if(gameMinute == 105) currentHalf = firstETInjury;
        else if(gameMinute  < 120) currentHalf = secondET;
        else if(gameMinute == 120) currentHalf = secondETInjury;
        else currentHalf = unknown;
    }else{
        switch (currentHalf) {
        case  firstHalfInjury: if(     halfTimeTick < 0 && segment >= 3){      halfTimeTick = tickLastStopped; currentHalf = secondHalf; } break;
        case secondHalfInjury: if(    extraTimeTick < 0 && segment >= 6){     extraTimeTick = tickLastStopped; currentHalf = firstET; } break;
        case    firstETInjury: if(extraHalfTimeTick < 0 && segment >= 7){ extraHalfTimeTick = tickLastStopped; currentHalf = secondET; } break;
            //TODO figure out if the second ET half starts on segment 7 or in the middle of it
        default: break;
        }
    }
    lastMinute = gameMinute;
    //qDebug() <<"currentHalf: "<< currentHalf;

    //calculate the time
    double ticksPerMinute = matchLength * 32;//320 ticks per gameMinute with match length set to 10 minutes, so [ticks per game minute] = 32 * [match length setting]
    double time = -1, injuryTime = -1;

    //local function which will calculate the current minute based on game ticks and try to adjust its parameters so that it matches the current minute according to the stats table
    const auto calcTimeAndAdjustTPM = [this,&ticksPerMinute,&time,gameMinute,gameTick](int &baseTick, const int baseMinute){
        time = ((gameTick - baseTick) / ticksPerMinute) + baseMinute;
        if(gameMinute != (int)time){
            double avrgTPM = 0;
            int i;
            //calculate the average TPM based on the current half alone
            for(i = gameMinute; i > (baseMinute+1) && minuteTicks[i-1] >= 0; --i)
                avrgTPM += minuteTicks[i] - minuteTicks[i-1];
            if(gameMinute != i){
                avrgTPM /= gameMinute-i;
                //try to adjust the TPM to match the calculated average
                while(ticksPerMinute - avrgTPM >  32) ticksPerMinute -= 32;
                while(ticksPerMinute - avrgTPM < -32) ticksPerMinute += 32;
            }
            //try to adjust the halftimeTick so that the tick time is accurate
            while(gameMinute <(int)time) time = baseMinute + (gameTick - (++baseTick)) / ticksPerMinute;
            while(gameMinute >(int)time) time = baseMinute + (gameTick - (--baseTick)) / ticksPerMinute;
        }
    };

    //calculate the time and try adjusting parameters to make the seconds and injury time minutes more accurate
    switch (currentHalf) {
    case firstHalf:
        time = gameTick / ticksPerMinute;
        //try to adjust the TPM so that the tick time is accurate
        while(gameMinute < (int)time) time = gameTick / (ticksPerMinute += 32);
        while(gameMinute > (int)time) time = gameTick / (ticksPerMinute -= 32);
        break;
    case secondHalf: calcTimeAndAdjustTPM(halfTimeTick,45); break;
    case    firstET: calcTimeAndAdjustTPM(extraTimeTick,90); break;
    case   secondET: calcTimeAndAdjustTPM(extraHalfTimeTick,105); break;
#define CALC_INJURY_TIME(LEN,TICK,COND) (ticksPerMinute > 0 && TICK COND) ? ((gameTick - TICK) / ticksPerMinute) - LEN : NAN
    case  firstHalfInjury: time =  45; injuryTime = CALC_INJURY_TIME(45,                0,== 0); break;
    case secondHalfInjury: time =  90; injuryTime = CALC_INJURY_TIME(45,     halfTimeTick, > 0); break;
    case    firstETInjury: time = 105; injuryTime = CALC_INJURY_TIME(15,    extraTimeTick, > 0); break;
    case   secondETInjury: time = 120; injuryTime = CALC_INJURY_TIME(15,extraHalfTimeTick, > 0); break;
    default: break;
    }
    matchLength = std::nearbyint(ticksPerMinute/32);
    //qDebug() << "Time calculated: "<<time<<" (injury time: "<< injuryTime<<")";

    //Update the collected stats and events with the right time
    for(statChange &stat: changes){
        stat.gameMinute = gameMinute;
        stat.gameTick = gameTick;
    }
    statHistory.append(changes);
    for(matchEvent &event: local.eventsThisUpdate){
        event.gameMinute = time;
        event.injuryMinute = injuryTime;
        event.gameTick = gameTick;
    }
    //qDebug() <<"Stat changes and events adjusted";

    //Update the event log textbox and tally the scoreline
    QScrollBar *bar = ui->eventLog->verticalScrollBar();
    int range = bar->maximum() - bar->minimum();
    int val = bar->value();
    ui->eventLog->clear();
    int homeScore = 0, awayScore = 0;
    for(matchEvent &event: events){
        //Tally Scoreline
        if(event.type == goal)
            (event.isHome ? homeScore : awayScore) += 1;

        //recalculate event time based on (hopefully) more accurate parameters
        recalcTime(event.gameTick, event.gameMinute, event.injuryMinute);

        //TODO use actual team colors
        ui->eventLog->setTextBackgroundColor(QColor(event.isHome ? "#e0e0fc" : "#ffe0dd"));
        QString logStr = event.eventLogString();
        if(!logStr.isEmpty())
            ui->eventLog->append(logStr);
    }
    if(range == (bar->maximum() - bar->minimum()))
        bar->setValue(val);
    //qDebug() << "Log written to";

    //Update score and time labels
    setLabels(homeScore,awayScore,gameMinute,time,injuryTime);
    //qDebug() << "labels updated";

    //Check if the clock has stopped or started and add the appropriate event
    if(gameTick == tickLastUpdate){
        if(gameTick != tickLastStopped){
            tickLastStopped = gameTick;
            local.eventsThisUpdate.append(matchEvent{timestamp,QString(),QString(),0,0,(float)time,(float)injuryTime,gameTick,clockStopped,false});
            emit clock_stopped(timestamp,gameTick,time,injuryTime);
        }
    } else {
        if(tickLastUpdate == tickLastStopped){
            local.eventsThisUpdate.append(matchEvent{timestamp,QString(),QString(),0,0,(float)time,(float)injuryTime,gameTick,clockStarted,false});
            emit clock_started(timestamp,gameTick,time,injuryTime);
        }
        tickLastUpdate = gameTick;
    }
    events.append(local.eventsThisUpdate);

    for(matchEvent &event : local.eventsThisUpdate){
        emit newEvent(event,events);
    }

    showBenched(benchShown);
}


void Match::setLabels(int homeScore, int awayScore, int gameMinute, double time, double injuryTime){
    ui->hScoreLabel->setText(QString::number(homeScore));
    ui->aScoreLabel->setText(QString::number(awayScore));
    QString minuteString = QString::number(gameMinute);
    if(std::isnan(injuryTime))
        minuteString += "+";
    else if(injuryTime >= 0)
        minuteString += "+" + QString::number((int)injuryTime) + ":" + QString("%1").arg((int)(std::fmod(injuryTime,1.0) * 60),2,10,QChar('0'));
    else
        minuteString += ":" + QString("%1").arg((int)(std::fmod(time,1.0) * 60),2,10,QChar('0'));
    ui->clockLabel->setText(minuteString);
}


void Match::endMatch(){
    for(int row = 0; row < ui->statsTable->rowCount(); ++row){
        for(int col = 0; col < ui->statsTable->columnCount(); ++col){
            ui->statsTable->item(row,col)->setTextColor(Qt::black);
        }
    }
    emit table_lost(QDateTime::currentMSecsSinceEpoch(),events.back().gameMinute,events.back().injuryMinute);
}


void Match::recalcTime(int gameTick, float &gameMinute, float &injuryMinute){
    if(matchLength <= 0)
        return;

    int halftick = 0;
    double timeInHalf = 45;
    if(gameTick < halfTimeTick){
    } else if(gameTick < extraTimeTick){
        halftick = halfTimeTick;
        timeInHalf = 45;
    } else if(gameTick < extraHalfTimeTick){
        halftick = extraTimeTick;
        timeInHalf = 15;
    } else {
        halftick = extraHalfTimeTick;
        timeInHalf = 15;
    }

    //if the chosen halfTick wasn't negative
    if(halftick >= 0){
        double eventTime = (gameTick-halftick) / (matchLength * 32.0);
        gameMinute = std::trunc(gameMinute);
        if(eventTime < timeInHalf)
            gameMinute += std::fmod(eventTime,1.0);
        else
            injuryMinute = eventTime - timeInHalf;
    }
}

void Match::showBenched(bool shown){
    benchShown = shown;
    for(int p = 0; p < totalPlayers; ++p)
        ui->statsTable->setRowHidden(p, !shown && 0 > getLastValue(p,statsInfo::firstMinuteOnPitch));
}

Match::~Match()
{
    delete ui;
}

int Match::darkenRate = 19;
