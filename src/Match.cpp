#include "Match.h"
#include "ui_match.h"
#include "CustomTableStyle.h"
#include <QScrollBar>
#include <QHash>
#include <utility>
#include <cmath>
int Match::matchLength = 10;
bool Match::benchShown = false;
int Match::   goal_s;
int Match:: assist_s;
int Match::owngoal_s;
int Match:: yellow_s;
int Match::    red_s;
int Match::   fmin_s;
int Match::   lmin_s;
int Match::  ticks_s;
QHash<int,Match::pEvent> Match::statEvents;

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

    //Save indicies of important stats, along with some properties
#define INDEX(IDX,NAME) (IDX = statsInfo::getStatIndex(NAME))
    statEvents = {
    #define PAIR(IDX,IS1,TYPE) std::pair<int,pEvent>(IDX, pEvent{IS1,TYPE})
        PAIR(INDEX(   goal_s,         "Goals Scored"), true,goal),
        PAIR(INDEX( assist_s,              "Assists"),false,goal),
        PAIR(INDEX(owngoal_s,     "Own Goals Scored"), true,ownGoal),
        PAIR(INDEX( yellow_s,         "Yellow Cards"), true,yellowCard),
        PAIR(INDEX(    red_s,            "Red Cards"), true,redCard),
        PAIR(INDEX(   fmin_s,"First Minute on Pitch"), true,subOn),
    #undef PAIR
    };
    INDEX( lmin_s,"Last Minute on Pitch");
    INDEX(ticks_s,"Game Ticks Played");
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

    updateTimes.push_back(match_time{this,matchStartTime,-1.0f,-1.0f,0});
    match_time *when = &updateTimes.back();

    for(int8_t p = 0; p < totalPlayers; ++p){
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
        for(int i = statsInfo::getStat(goal_s).getValue(currPlayer); i > 0; --i){
            events.append(match_event{when,p,-1,goal, p < home.nPlayers});
            (p < home.nPlayers ? homeScore : awayScore) += 1;
        }
        for(int i = statsInfo::getStat(owngoal_s).getValue(currPlayer); i > 0; --i){
            events.append(match_event{when,p,-1,ownGoal, p < home.nPlayers});
            (p < home.nPlayers ? awayScore : homeScore) += 1;
        }
        for(int i = statsInfo::getStat(yellow_s).getValue(currPlayer); i > 0; --i){
            events.append(match_event{when,p,-1,yellowCard, p < home.nPlayers});
        }
        for(int i = statsInfo::getStat(red_s).getValue(currPlayer); i > 0; --i){
            events.append(match_event{when,p,-1,redCard, p < home.nPlayers});
        }
    }
    when->gameMinute = lastMinute;
    when->gameTick = tickLastUpdate;

    //Update the table column widths
    for(int s = 0; s < statsInfo::count(); ++s){
        ui->statsTable->setColumnWidth(s, std::max(20,maxWidths[s]+8));
    }

    setLabels(lastMinute,lastMinute,-1);

    for(match_event &event: events){
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

void Match::update(uint8_t *currData, uint8_t *prevData){
    QFontMetrics fm(ui->statsTable->font());
    QVector<int> maxWidths = QVector<int>((int)statsInfo::count(),0);

    //The current segment of the match, gets set when a segmented stat change is detected, otherwise we don't need it anyway
    int segment = -1;//1st half: [0-2], 2nd half: [3-5], 1st ET: [6-7?], 2nd ET: [7?-8?]  //TODO figure out if segment 8 is used

    updateTimes.push_back(match_time{this,QDateTime::currentMSecsSinceEpoch(),(float)lastMinute,-1.0f,tickLastUpdate});
    match_time *when = &updateTimes.back();
    QVector<stat_change> changes;

    //==================Collect Stat Changes====================//
    //For every stat of every player
    for(int p = 0; p < totalPlayers; ++p){
        uint8_t *currPlayer = currData + (p * statsInfo::entrySize());
        uint8_t *prevPlayer = prevData + (p * statsInfo::entrySize());

        for(int s = 0; s < statsInfo::count(); ++s){
            const PESstat &stat = statsInfo::getStat(s);
            double curr = stat.getValue(currPlayer);
            double prev = stat.getValue(prevPlayer);

            //update text width
            QString str = QString::number(curr,'g',10);
            maxWidths[s] = std::max(maxWidths[s], fm.width(str));

            QTableWidgetItem *item = ui->statsTable->item(p,s);
            //Update the stat
            if(curr != prev){
                latestStats[vectorPosition(p,s)] = curr;
                //Update the Table item
                item->setText(str);
                item->setTextColor(Qt::red);

                //Figure out what segment(s) changed and record it
                for(uint8_t seg = 0; seg < stat.count; ++seg){
                    stat_change change{when,stat_value{0},(int16_t)s,seg,(int8_t)p};
                    if(stat.getSegmentChanged(currPlayer,prevPlayer,seg,&change.newVal))
                        changes.push_back(change);
                }

                //If we've yet to figure out what segment we're in
                if(segment < 0 && stat.count == 9){
                    //check each segment, starting with the last, and stop when the value is non-zero
                    for(segment = 8; segment >= 0 && stat.getSegmentDouble(currPlayer,segment) == 0; --segment);
                }
            } else { //Stat did not change
                //Darken the item text color so it slowly fades from red to black
                int h, s, v, a;
                item->textColor().getHsv(&h,&s,&v,&a);
                if(v > 0)
                    item->setTextColor(QColor::fromHsv(h, s, std::max(0,v-darkenRate), a));
            }//End If/Else stat was updated
        }//End For each stat
    }//End For each player

    //Update the table column widths
    for(int s = 0; s < statsInfo::count(); ++s){
        ui->statsTable->setColumnWidth(s, std::max(20,maxWidths[s]+8));
    }

    //=====================Process Stat Events=====================//
    bool hasSubbed = false;
    //Search through the changed stats to record important events
    QVector<match_event> eventsThisUpdate;
    for(stat_change change: changes){
        if(!statEvents.contains(change.statId)){
            continue;
        }
        const pEvent t = statEvents.value(change.statId);
        hasSubbed = hasSubbed || t.type == subOn;

        match_event newEvent{when, -1, -1, t.type, change.player < home.nPlayers};
        match_event *event = &newEvent;
        for(match_event &e : eventsThisUpdate){
            if(e.type == t.type && (t.is1 ? e.player1 : e.player2) < 0)
                event = &e;
        }
        (t.is1 ? event->player1 : event->player2) = change.player;
        if(event == &newEvent)
            eventsThisUpdate.append(newEvent);
    }

    //===================Calculate update time=================//
    for(int p = 0; p < totalPlayers; ++p){
        when->gameMinute = std::max(when->gameMinute,(float)latestStats[vectorPosition(p, lmin_s)]);
        when->gameTick   = std::max(when->gameTick,(int32_t)latestStats[vectorPosition(p,ticks_s)]);
    }
    //fix inaccurate times caused by subbing
    if(hasSubbed){
        minuteSubbed = (int)when->gameMinute;
        when->gameMinute -= 1;
    }
    //===================TODO=================//
    //TODO figure out what to do when gameMinute == minuteSubbed
    //Issues:
    //what happens when subbed in injury time?
    //what happens if the minute is incorrectly advanced to 45/90/105/120?

    //Calculate the current state of the match clock
    int gameMinute = (int)when->gameMinute, gameTick = when->gameTick;
    //bool minuteChanged = gameMinute-1 == lastMinute;


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

    //Update the match time
    when->gameMinute = (float)time;
    when->injuryMinute = injuryTime;


    //=========================Update UI==========================//
    //Update the event log textbox and tally the scoreline
    int val = ui->eventLog->verticalScrollBar()->value();
    for(match_event &event: eventsThisUpdate){
        //Tally Scoreline
        if(event.type == goal)
            (event.isHome ? homeScore : awayScore) += 1;
        else if(event.type == ownGoal)
            (event.isHome ? awayScore : homeScore) += 1;

        //TODO use actual team colors
        ui->eventLog->setTextBackgroundColor(QColor(event.isHome ? "#e0e0fc" : "#ffe0dd"));
        QString logStr = event.eventLogString();
        if(!logStr.isEmpty())
            ui->eventLog->append(logStr);
    }
    if(eventsThisUpdate.size() > 0)
        ui->eventLog->verticalScrollBar()->setValue(val);
    //Update score and time labels
    setLabels(gameMinute,time,injuryTime);
    showBenched(benchShown);


    //=======================Update Events=======================//
    //Check if the clock has stopped or started and add the appropriate event
    if(gameTick == tickLastUpdate){
        if(gameTick != tickLastStopped){
            tickLastStopped = gameTick;
            eventsThisUpdate.append(match_event{when,-1,-1,clockStopped,false});
            emit clock_stopped(when->timestamp,gameTick,time,injuryTime);
        }
    } else {
        if(tickLastUpdate == tickLastStopped){
            eventsThisUpdate.append(match_event{when,-1,-1,clockStopped,false});
            emit clock_started(when->timestamp,gameTick,time,injuryTime);
        }
        tickLastUpdate = gameTick;
    }

    //Add new events and stat changes to the match or if there were none, remove the latest update time
    if(eventsThisUpdate.isEmpty() && changes.isEmpty()){
        when = nullptr;
        updateTimes.pop_back();
    } else {
        events.append(eventsThisUpdate);
        for(stat_change change: changes)
            playerStatHistory[change.player].push_back(change);
    }

    //Emit events as signals
    for(match_event &event : eventsThisUpdate){
        emit newEvent(event,events);
    }
}


void Match::setLabels(int gameMinute, double time, double injuryTime){
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
    emit table_lost(QDateTime::currentMSecsSinceEpoch(),updateTimes.back().gameMinute,updateTimes.back().injuryMinute);
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

QString match_event::eventLogString(){
    QString result = QString("%1").arg(when->minuteString(),7);
    switch(type){
    case goal:
    case assist:
        result +=              "' GOAL: " + player1Name();
        if(player2 >= 0)
            result += "\n         assist: " + player2Name();
        break;
    case ownGoal:
        result +=              "' OWN GOAL: "+ player1Name();
        break;
    case yellowCard:
        result +=              "' YELLOW: " + player1Name();
        break;
    case redCard:
        result +=              "' RED: " + player1Name();
        break;
    case subOn:
    case subOff:
        result +=              "' IN: " + player1Name();
        if(player2 >= 0)
            result += "\n         OUT: " + player2Name();
        break;
    default: return QString();
    }
    return result;
}

QJsonDocument match_event::toJSON(){
    QJsonObject res = when->startEventObject();
    QJsonObject player1Obj{
        {"name",player1Name()},
        {"playerId",(qint64)player1Id()}
    };
    QJsonObject player2Obj{
        {"name",player2Name()},
        {"playerId",(qint64)player2Id()}
    };

    switch (type) {
    case goal:
    case assist:
        res["event"] = "Goal";
        res["team"] = isHome ? "Home" : "Away";
        res["scorer"] = player1Obj;
        if(player2 >= 0)
            res["assister"] = player2Obj;
        break;
    case ownGoal:
        res["event"] = "Own Goal";
        res["team"] = isHome ? "Home" : "Away";
        res["player"] = player1Obj;
        break;
    case yellowCard:
        res["event"] = "Card";
        res["team"] = isHome ? "Home" : "Away";
        res["card"] = "Yellow";
        res["player"] = player1Obj;
        break;
    case redCard:
        res["event"] = "Card";
        res["team"] = isHome ? "Home" : "Away";
        res["card"] = "Red";
        res["player"] = player1Obj;
        break;
    case subOn:
    case subOff:
        res["event"] = "Player Sub";
        res["team"] = isHome ? "Home" : "Away";
        res["playerIn"] = player1Obj;
        if(player2 >= 0)
            res["playerOut"] = player2Obj;
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

QString match_time::minuteString() const{
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

QJsonObject match_time::startEventObject() const{
    QJsonObject res{
        {"type", "Event"},
        {"timestamp", timestamp},
        {"gameMinute", gameMinute},
    };
    if(std::isnan(injuryMinute))
        res["injuryMinute"] = true;
    else if(injuryMinute >= 0)
        res["injuryMinute"] = injuryMinute;
    return res;
}
