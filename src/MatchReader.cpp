#include "src/MatchReader.h"
#include "ui_MatchReader.h"
#include "QrTimestamp.h"
#include <QDebug>

MatchReader::MatchReader(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MatchReader)
{
    ui->setupUi(this);
    QTimer *timer = new QTimer(this);
    QrTimestamp *qrcode = new QrTimestamp(ui->qrCode,this);
    connect(timer,&QTimer::timeout, qrcode,&QrTimestamp::update);
    connect(timer,&QTimer::timeout, this,&MatchReader::showTime);
    timer->start(33);

    QSettings settings;

    reader = new StatTableReader(L"PES2017.exe",ui->bruteForce,this);
    server = new PipeServer(this);

    updateTimer = new QTimer(this);
    connect(updateTimer,&QTimer::timeout,              reader,&StatTableReader::update);
    connect(reader,&StatTableReader::status_changed,   ui->done, &QLabel::setText);
    connect(reader,&StatTableReader::action_changed,   ui->doing,&QLabel::setText);
    connect(reader,&StatTableReader::table_lost,       this,&MatchReader::stats_lost);
    connect(reader,&StatTableReader::table_found,      this,&MatchReader::stats_found);
    connect(reader,&StatTableReader::teamsChanged,     this,&MatchReader::teams_changed);
    connect(reader,&StatTableReader::statTableUpdate,  this,&MatchReader::update);
    updateTimer->start(settings.value("UpdateRate",100).toInt());

    connect(this, &MatchReader::table_found_event, server,&PipeServer::table_found);
    connect(this, &MatchReader::table_lost_event,  server,&PipeServer::table_lost);
    connect(this, &MatchReader::newEvent,          server,&PipeServer::newEvent);


    //Save indicies of important stats, along with some properties
#define INDEX(IDX,NAME) (si[IDX] = statsInfo::getStatIndex(NAME))
#define PAIR(IDX,IS1,TYPE) std::pair<int,pEvent>(IDX, pEvent{IS1,TYPE})
    statEvents = {
        PAIR(INDEX(   goal_s,         "Goals Scored"), true,goal),
        PAIR(INDEX( assist_s,              "Assists"),false,goal),
        PAIR(INDEX(owngoal_s,     "Own Goals Scored"), true,ownGoal),
        PAIR(INDEX( yellow_s,         "Yellow Cards"), true,yellowCard),
        PAIR(INDEX(    red_s,            "Red Cards"), true,redCard),
        PAIR(INDEX(   fmin_s,"First Minute on Pitch"), true,subOn),
    };
    INDEX(   lmin_s,"Last Minute on Pitch");
    INDEX(  ticks_s,"Game Ticks Played");

    INDEX(   foul_s,"Fouls");
    INDEX(offside_s,"Fouls Offside");
    INDEX(   pass_s,"Passes");
    INDEX(gudPass_s,"Passes Made");
    INDEX(intrcpt_s,"Interceptions");
    INDEX(   shot_s,"Shots");
    INDEX(gudShot_s,"Shots on Target");
    INDEX(   save_s,"Saves");

#undef PAIR
}

MatchReader::~MatchReader()
{
    delete ui;
}
//===========UI Handling=============
void MatchReader::showTime(){
    ui->timeLabel->setText(QDateTime::currentDateTimeUtc().toString("MMM dd, hh:mm:ss.zzz t"));
}

void MatchReader::stats_lost()
{
    qDebug("Function Entered");
    if(match == nullptr)
        return;
    endMatch();
    dbgReturn();
}
//==========Match Tracking===========

void MatchReader::newMatch(){
    qDebug("Function Entered");
    if(match != nullptr)
        endMatch();
    match = new Match(Home,Away);
    resetTime();
    emit new_match(match);
    dbgReturn();
}

void MatchReader::stats_found(uint8_t *data){
    qDebug("Function Entered");
    if(match == nullptr){
        newMatch();
    }
    match_time *when = match->addUpdateTime();
    match->matchStartTime = when->timestamp;

    QFontMetrics fm = match->getFont();
    std::vector<int> maxWidths = std::vector<int>((int)statsInfo::count(),0);

    for(int8_t p = 0; p < match->totalPlayers; ++p){
        uint8_t *currPlayer = data + (p * statsInfo::entrySize());
        for(int s = 0; s < statsInfo::count(); ++s){
            const PESstat &stat = statsInfo::getStat(s);
            double val = stat.getValue(currPlayer);

            QString str = match->addStatCell(p,s,val);
            maxWidths[s] = std::max(maxWidths[s], fm.width(str));
        }
        auto statVal = [&](stat_index idx){
            return (int)statsInfo::getStat(si[idx]).getValue(currPlayer);
        };
        lastMinute = std::max(lastMinute,statVal(lmin_s));
        tickLastUpdate = std::max(tickLastUpdate,statVal(ticks_s));
        auto addAllEvents = [&](stat_index idx, eventType type){
            for(int i = statVal(idx); i > 0; --i){
                match->addEvent(when,type,p);
            }
        };
        addAllEvents(goal_s,goal);
        addAllEvents(owngoal_s,ownGoal);
        addAllEvents(yellow_s,yellowCard);
        addAllEvents(red_s,redCard);
    }
    when->gameMinute = lastMinute;
    when->gameTick = tickLastUpdate;

    match->setColumnWidths(maxWidths);
    match->setLabels(lastMinute,lastMinute,-1);

    emit table_found_event(when);
    dbgReturn();
}

void MatchReader::teams_changed(qint64 timestamp, teamInfo home, teamInfo away)
{
    qDebug("Function Entered");
    if(home.name[0] == '\0' || away.name[0] == '\0'){
        match = nullptr;
        return;
    }
    Home = home; Away = away;
    newMatch();
    match->matchCreationTime = timestamp;
    server->teamsChanged(match);
    dbgReturn();
}

void MatchReader::endMatch(){
    qDebug("Function Entered");
    if(match->updateTimes.empty()){
        emit invalidate_match();
    } else {
        match->endMatch(matchLength);
        emit done_match(Home.name + " vs " + Away.name);
        emit table_lost_event(&match->updateTimes.back());
    }
    match = nullptr;
    dbgReturn();
}

int MatchReader::collectStatChanges(uint8_t *currData, uint8_t *prevData, std::vector<stat_change>& changes, match_time *when){
    qDebug("Function Entered");

    QFontMetrics fm = match->getFont();
    std::vector<int> maxWidths = std::vector<int>((int)statsInfo::count(),0);

    //The current segment of the match, gets set when a segmented stat change is detected, otherwise we don't need it anyway
    int segment = -1;//1st half: [0-2], 2nd half: [3-5], 1st ET: [6-7?], 2nd ET: [7?-8?]  //TODO figure out if segment 8 is used

    //For every stat of every player
    for(int p = 0; p < match->totalPlayers; ++p){
        uint8_t *currPlayer = currData + (p * statsInfo::entrySize());
        uint8_t *prevPlayer = prevData + (p * statsInfo::entrySize());

        for(int s = 0; s < statsInfo::count(); ++s){
            const PESstat &stat = statsInfo::getStat(s);
            double curr = stat.getValue(currPlayer);
            double prev = stat.getValue(prevPlayer);

            //update text width
            QString str =  curr > 10000 ? QString::number(curr,'f',0) : QString::number(curr,'g',5);
            maxWidths[s] = std::max(maxWidths[s], fm.width(str));

            //Update the Table item
            bool changed = curr != prev;
            match->updateTableStat(p,s,curr,str,changed);

            //Update the stat
            if(changed){
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
            }//End If stat was updated
        }//End For each stat
    }//End For each player


    match->setColumnWidths(maxWidths);

    dbgReturn(return segment);
}

eventType MatchReader::proccessStatEvents(std::vector<match_event>& eventsThisUpdate, const std::vector<stat_change>& changes, match_time *when){
    qDebug("Function Entered");
    //Search through the changed stats to record important events
    enum rState {
        unknown, shot, missedShot, savedShot, goalScored, ownGoalScored, foul, offside, pass, inboundPass
    } reasonState = unknown;

    for(stat_change change: changes){
        auto i = statEvents.find(change.statId);
        if(i != statEvents.end()){
            const pEvent t = i->second;

            match_event newEvent{when, -1, -1, t.type, eventType::unknown, match->isHome(change.player)};
            match_event *event = &newEvent;
            for(match_event &e : eventsThisUpdate){
                if(e.type == t.type && (t.is1 ? e.player1 : e.player2) < 0)
                    event = &e;
            }
            (t.is1 ? event->player1 : event->player2) = change.player;
            if(event == &newEvent){
                eventsThisUpdate.push_back(newEvent);
            }
        }
        auto statId = change.statId;
        if(statId == si[foul_s]){
            if(reasonState != offside)
                reasonState = foul;
        } else if(statId == si[offside_s]){
            reasonState = offside;
        } else if(statId == si[pass_s]){
            if(reasonState == unknown)
                reasonState = pass;
        } else if(statId == si[gudPass_s] || statId == si[intrcpt_s]){
            reasonState = inboundPass;
        } else if(statId == si[shot_s]){
            if(reasonState == unknown)
                reasonState = missedShot;
        } else if(statId == si[gudShot_s]){
            if(reasonState == unknown || reasonState == missedShot)
                reasonState = shot;
        } else if(statId == si[save_s]){
            reasonState = savedShot;
        } else if(statId == si[goal_s]){
            reasonState = goalScored;
        } else if(statId == si[owngoal_s]){
            reasonState = ownGoalScored;
        }
    }

    switch (reasonState) {
    default:
    case unknown:
    case shot:
    case inboundPass:   dbgReturn(return eventType::unknown);
    case missedShot:    dbgReturn(return eventType::goalkick);
    case savedShot:     dbgReturn(return eventType::cornerkick);
    case goalScored:    dbgReturn(return eventType::goal);
    case ownGoalScored: dbgReturn(return eventType::ownGoal);
    case foul:          dbgReturn(return eventType::foul);
    case offside:       dbgReturn(return eventType::offside);
    //case pass:          dbgReturn(return eventType::throwin);
    }
}

void MatchReader::calcUpdateTime(match_time *when, int segment){
    qDebug("Function Entered");
    {
        //Calculate the mode average of the "Last Minute on Pitch" stat to eliminate inaccuracies caused by subbing (thanks PES)
        std::unordered_map<int,int> minuteCount;
        for(int p = 0; p < match->totalPlayers; ++p){
            minuteCount[(int)match->getLastValue(p, si[lmin_s])]++;
            when->gameTick   = std::max(when->gameTick,(int32_t)match->getLastValue(p,si[ticks_s]));
        }
        minuteCount[-1] = -1;
        std::unordered_map<int,int>::iterator mean = std::max_element(minuteCount.begin(),minuteCount.end());
        when->gameMinute = (float)mean->first;
    }

    //Calculate the current state of the match clock
    int gameMinute = (int)when->gameMinute, gameTick = when->gameTick;
    //bool minuteChanged = gameMinute-1 == lastMinute;


    //Figure out what half we're in
    if(gameMinute-1 == lastMinute){
        minuteTicks[gameMinute] = gameTick;

        if(gameMinute  <   0) currentHalf = unknownHalf;
        else if(gameMinute  <  45) currentHalf = firstHalf;
        else if(gameMinute ==  45) currentHalf = firstHalfInjury;
        else if(gameMinute  <  90) currentHalf = secondHalf;
        else if(gameMinute ==  90) currentHalf = secondHalfInjury;
        else if(gameMinute  < 105) currentHalf = firstET;
        else if(gameMinute == 105) currentHalf = firstETInjury;
        else if(gameMinute  < 120) currentHalf = secondET;
        else if(gameMinute == 120) currentHalf = secondETInjury;
        else currentHalf = unknownHalf;
    }else{
        switch (currentHalf) {
        case  firstHalfInjury: if(     halfTimeTick < 0 && segment >= 3){      halfTimeTick = tickLastStopped; currentHalf = secondHalf; } break;
        case secondHalfInjury: if(    extraTimeTick < 0 && segment == 6){     extraTimeTick = tickLastStopped; currentHalf = firstET; } break;
        case    firstETInjury: if(extraHalfTimeTick < 0 && segment == 7){ extraHalfTimeTick = tickLastStopped; currentHalf = secondET; } break;
        case   secondETInjury: if(                         segment == 8){                                      currentHalf = penalties; } break;
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
    case        penalties: time = -2; injuryTime = NAN;
    default: break;
    }
    matchLength = std::nearbyint(ticksPerMinute/32);

    //Update the match time
    when->gameMinute = (float)time;
    when->injuryMinute = injuryTime;
    dbgReturn(return);
}


void MatchReader::update(uint8_t *currData, uint8_t *prevData){
    qDebug("Function Entered");
    match_time *when = match->addUpdateTime(lastMinute,tickLastUpdate);

    //==================Collect Stat Changes====================//
    std::vector<stat_change> changes;
    int segment = collectStatChanges(currData,prevData,changes,when);

    //=====================Process Stat Events=====================//
    std::vector<match_event> eventsThisUpdate;
    eventType stopReason = proccessStatEvents(eventsThisUpdate,changes,when);

    //===================Calculate update time=================//
    calcUpdateTime(when,segment);
    float time = when->gameMinute, injuryTime = when->injuryMinute;
    int gameMinute = (int)time, gameTick = when->gameTick;


    //=========================Update UI==========================//
    //Update the event log textbox and tally the scoreline
    int val = match->getLogScroll();
    for(match_event &event: eventsThisUpdate){
        match->addEvent(event);
    }
    if(eventsThisUpdate.size() > 0)
        match->setLogScroll(val);
    //Update score and time labels
    match->setLabels(gameMinute,time,injuryTime);
    match->showBenched(benchShown);


    //=======================Update Events=======================//
    //Check if the clock has stopped or started and add the appropriate event
    if(gameTick == tickLastUpdate){ //Is the clock stopped
        if(gameTick != tickLastStopped){//Did it just stop?
            tickLastStopped = gameTick;
            if(stopReason == unknown)
                stopReason = lastStopReason;
            eventsThisUpdate.push_back(match_event{when,-1,-1,clockStopped,stopReason,false});
            match->events.push_back(eventsThisUpdate.back());
            lastStopReason = unknown;
            emit clock_stopped(when->timestamp,gameTick,time,injuryTime);
        }
    } else {
        if(tickLastUpdate == tickLastStopped){//Was the clock stopped before?
            eventsThisUpdate.push_back(match_event{when,-1,-1,clockStarted,unknown,false});
            match->events.push_back(eventsThisUpdate.back());
            emit clock_started(when->timestamp,gameTick,time,injuryTime);
        }
        tickLastUpdate = gameTick;
        lastStopReason = stopReason;
    }

    //Add new events and stat changes to the match or if there were none, remove the latest update time
    if(eventsThisUpdate.empty() && changes.empty()){
        when = nullptr;
        match->updateTimes.pop_back();
    } else {
        for(stat_change change: changes)
            match->playerStatHistory[change.player].push_back(change);
        emit matchUpdated();
    }

    //Emit events as signals
    for(match_event &event : eventsThisUpdate){
        emit newEvent(event);
    }
    dbgReturn(return);
}
