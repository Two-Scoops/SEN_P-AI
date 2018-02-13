#include "Match.h"
#include "ui_match.h"
#include "CustomTableStyle.h"
#include <QScrollBar>
#include <QFile>
#include <QDataStream>
#include <utility>
#include <cmath>
#include <type_traits>
#include <memory>

#include "Common.h"

int Match::darkenRate = 19;

void Match::init(){
    qDebug("Function Entered");
    ui->homeLabel->setText(home.name);
    ui->awayLabel->setText(away.name);
    ui->statsTable->setColumnCount((int)statsInfo::count());
    ui->statsTable->setRowCount(totalPlayers);

    latestStats.resize(totalPlayers * (int)statsInfo::count(), 0);

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
    dbgReturn(return);
}

Match::Match(teamInfo homeInfo, teamInfo awayInfo, QWidget *parent) :
    QWidget(parent),
    home(homeInfo), away(awayInfo), totalPlayers(homeInfo.nPlayers + awayInfo.nPlayers),
    ui(new Ui::Match)
{
    qDebug("Function Entered");
    matchCreationTime = QDateTime::currentMSecsSinceEpoch();
    ui->setupUi(this);
    init();
    dbgReturn();
}

Match::Match(): ui(new Ui::Match){
    qDebug("Function Entered");
    ui->setupUi(this);
    dbgReturn();
}


void Match::keyPressEvent(QKeyEvent *event){
    qDebug("Function Entered");
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
    dbgReturn(return);
}

void Match::statsDisplayChanged(statsInfo &info){
    qDebug("Function Entered");
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
    dbgReturn(return);
}


QFontMetrics Match::getFont() const{
    return QFontMetrics(ui->statsTable->font());
}

void Match::addEvent(match_event event){
    events.push_back(event);
    //Tally Scoreline
    if(event.type == goal)
        (event.isHome ? homeScore : awayScore) += 1;
    else if(event.type == ownGoal)
        (event.isHome ? awayScore : homeScore) += 1;
    logEvent(event);
}
void Match::logEvent(match_event &event){
    //TODO use actual team colors
    ui->eventLog->setTextBackgroundColor(QColor(event.isHome ? "#e0e0fc" : "#ffe0dd"));
    QString logStr = event.eventLogString();
    if(!logStr.isEmpty())
        ui->eventLog->append(logStr);
    return;
}

QString Match::addStatCell(int r, int c, double val){
    QString str = QString::number(val,'g',10);
    QTableWidgetItem *item = new QTableWidgetItem(str);
    //TODO use actual team colors
    item->setBackground(QBrush(QColor(r < home.nPlayers ? "#E5E5FC" : "#FFE9E8")));
    ui->statsTable->setItem(r,c,item);
    getLastValue(r,c) = val;
    return str;
}

void Match::setColumnWidths(std::vector<int> &maxWidths){
    qDebug("Function Entered");
    for(int s = 0; s < ui->statsTable->columnCount(); ++s){
        ui->statsTable->setColumnWidth(s, std::max(20,maxWidths[s]+8));
    }
    dbgReturn(return);
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

void Match::updateTableStat(int p, int s, double val, QString &str, bool changed){
    QTableWidgetItem *item = ui->statsTable->item(p,s);
    if(changed){
        latestStats[vectorPosition(p,s)] = val;
        item->setText(str);
        item->setTextColor(Qt::red);
    } else {
        //Darken the item text color so it slowly fades from red to black
        int h, s, v, a;
        item->textColor().getHsv(&h,&s,&v,&a);
        if(v > 0)
            item->setTextColor(QColor::fromHsv(h, s, std::max(0,v-darkenRate), a));
    }
}

int Match::getLogScroll(){ return ui->eventLog->verticalScrollBar()->value(); }

void Match::setLogScroll(int val){ ui->eventLog->verticalScrollBar()->setValue(val); }


void Match::endMatch(int length){
    qDebug("Function Entered");
    matchLength = length;
    updateTimes.push_back(updateTimes.back());
    updateTimes.back().timestamp = QDateTime::currentMSecsSinceEpoch();
    for(int row = 0; row < ui->statsTable->rowCount(); ++row){
        for(int col = 0; col < ui->statsTable->columnCount(); ++col){
            ui->statsTable->item(row,col)->setTextColor(Qt::black);
        }
    }
    dbgReturn(return);
}

void Match::showBenched(bool shown){
    for(int p = 0; p < totalPlayers; ++p)
        ui->statsTable->setRowHidden(p, !shown && 0 > getLastValue(p,statsInfo::firstMinuteOnPitch));
}

Match::~Match()
{
    delete ui;
}



QString match_event::eventLogString(){
    QString result = QString("%1").arg(when->minuteString(),7);
    switch(type){
    case goal:
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
        switch (reason) {
        case goal:
            res["reason"] = "Goal"; break;
        case ownGoal:
            res["reason"] = "Own Goal"; break;
        case yellowCard: case redCard: case foul:
            res["reason"] = "Foul"; break;
        case offside:
            res["reason"] = "Offside"; break;
        case goalkick:
            res["reason"] = "Goal kick"; break;
        case cornerkick:
            res["reason"] = "Corner kick"; break;
        case throwin:
            res["reason"] = "Out of bounds"; break;
        case halfend:
            res["reason"] = "Half End"; break;
        default:
            res["reason"] = "Unknown"; break;
        }
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


#include <functional>
struct NoCheck {
    template<typename T> bool operator ()(T,T){ return true; }
};

template<FileAction act>
struct BinFile: QFile {
    bool good;
    QString errorReason;
#define setGood good = good
    bool SetGood(bool alsoGood, QString whybad){
        if(good && !alsoGood){ errorReason = whybad; qDebug()<<whybad; }
        return good = good && alsoGood;
    }

    BinFile(QString filename): QFile(filename), good(true), errorReason("No error"){}
    void open(){
        if(act == load) good && SetGood(QFile::open(QIODevice::ReadOnly), "File failed to open");
        if(act == save) good && SetGood(QFile::open(QIODevice::WriteOnly | QIODevice::Truncate), "File failed to open");
    }

    template<typename T> void data(T* buff, size_t count){
        const size_t sz = sizeof(T)*count;
        size_t sizeOut;
        if(!good) return;
        if(act == load) sizeOut =  read((      char*)buff,sz);
        if(act == save) sizeOut = write((const char*)buff,sz);
        SetGood(sz == sizeOut, QString("File read/write failed with only %1/%2 bytes").arg(sizeOut).arg(sz));
    }
    template<typename T> void data(const T* buff, size_t count){
        const size_t sz = sizeof(T)*count;
        size_t sizeOut = 0;
        if(!good) return;
        if(act == save) sizeOut = write((const char*)buff,sz);
        SetGood(sz == sizeOut, QString("File read/write failed with only %1/%2 bytes").arg(sizeOut).arg(sz));
    }
    template<typename T> bool check(const T* buff, size_t count){
        bool same = true;
        if(act == save) data<T>(buff,count);
        if(act == load){
            std::vector<T> tmp(count);
            data<T>(tmp.data(),count);
            for(size_t i = 0; i < count; ++i)
                same = same && tmp[i] == buff[i];
        }
        return same;
    }
    template<typename T, typename Pred = NoCheck> void check(T val, Pred pred = Pred{}){
        T got = val;
        data<T>(&val,1);
        if(act == load) setGood && pred(got,val);
    }
    template<typename T> void fixed(const T* buff, size_t count){ setGood && check<T>(buff,count); }
    template<typename T> void  data(T& val){ BinFile:: data<T>(&val,1); }
    //template<typename T> void fixed(T val){ BinFile::fixed<T>(&val,1); }
    template<typename T> T get(){ T val; data(val); return val; }

    void string(QString &str, size_t len){
        QByteArray cstr = str.toUtf8().leftJustified((int)len,'\0',true);
        data(cstr.data(),len);
        if(act == load) str = QString::fromUtf8(cstr);
    }

#undef setGood
};


const char *sectionNames[] = {
    "matchInfo",
    "finalStats",
    "updateTimes",
    "matchEvents",
    "playerStatChanges"
};
struct Section {
    uint32_t size = 0, offset = 0;
    char name[24] = {'\0'};
    template<FileAction act> void rwOffset(BinFile<act> &f){
        if(act == save) offset = (uint32_t)f.pos();
        if(act == load) f.seek(offset);
    }
    template<FileAction act> void rwSize(BinFile<act> &f){
        uint32_t tmpSize = (uint32_t)f.pos() - offset;
        if(act == save) size = tmpSize;
        if(act == load) f.SetGood(size == tmpSize,QString("Stored(%1) and actual(%2) size mismatch").arg(size).arg(tmpSize));
    }
};

template<FileAction act>
bool Match::file(Match *match, QString filename)
{
    qDebug("Function Entered");
    Match &m = *match;
    if(act == save){
        filename = match->applyFilenameFormat(filename,match->matchCreationTime);
        if(!filename.endsWith(".sen",Qt::CaseInsensitive)) filename.append(".sen");
    }
    BinFile<act> f(filename);
#define setGood f.good = f.good
#define RET_IF_ERROR(MSG) if(!f.good) dbgReturn(return false) //;qDebug()<<MSG
    f.open();
    RET_IF_ERROR("File opened\nReading/Writing header");
    auto rwSize = [&](auto& container){
        uint32_t size = (uint32_t)container.size();
        f.data(size);
        if(act == load) container.resize(size);
        return size;
    };
    auto loopAll = [&](auto& container, auto size, auto fn){
        for(uint32_t i = 0; i < size; ++i)
            fn(container[i],i);
    };
    auto forAllIn = [&](auto& container, auto fn){
        loopAll(container,rwSize(container),fn);
    };
    //=======Header========//
    f.fixed("SEN:P-AI\0",9);
    f.check((uint8_t)MAJOR,std::equal_to<uint8_t>{}); //Major versions must be the same
    f.check((uint8_t)MINOR,std::greater_equal<uint8_t>{}); //Minor version can be greater
    f.check((uint8_t)PATCH,[](auto,auto){ return true; }); //Patches don't matter
    f.check((uint16_t)statsInfo::count());
    RET_IF_ERROR("Reading/Writing Section Table");
    f.data(m.totalPlayers);
    f.data(m.matchLength);
    //=======Section Table========//
    uint16_t sectionCount = 5;
    f.data(sectionCount);
    std::vector<Section> sections(sectionCount);
    if(act == save){
        for(uint16_t i = 0; i < sectionCount; ++i)
            std::strncpy(sections[i].name,sectionNames[i],23);
    }
    qint64 sectionsPos = f.pos();
    f.data(sections.data(),sections.size());//read/write the section table
    auto findSection = [&](const char *name)->Section*{
        for(Section &sec: sections) if(std::strncmp(name,sec.name,24) == 0) return &sec;
        return nullptr;
    };
    Section *sec = nullptr;
#define SECTION(NAME) setGood && (sec = findSection(NAME)); RET_IF_ERROR("Reading/Writing " NAME);
    /*========*/SECTION("matchInfo");/*========*/
    sec->rwOffset<act>(f);
    f.data(m.matchCreationTime);
    f.data(m.matchStartTime);
    f.data(m.matchEndTime);
    f.data(m.home.ID);
    f.data(m.away.ID);
    f.data(m.home.nPlayers);
    f.data(m.away.nPlayers);
    if(act == load) m.totalPlayers = m.home.nPlayers + m.away.nPlayers;
    f.data(m.homeScore);
    f.data(m.awayScore);
    f.string(m.home.name,70);
    f.string(m.away.name,70);
    //===Player entries===//
    for(uint8_t i = 0; i < m.home.nPlayers; ++i){
        f.data(m.home.player[i].ID);
        f.string(m.home.player[i].name,46);
    }
    for(uint8_t i = 0; i < m.away.nPlayers; ++i){
        f.data(m.away.player[i].ID);
        f.string(m.away.player[i].name,46);
    }
    if(act == load){
        m.init();
    }
    sec->rwSize(f);
    /*========*/SECTION("finalStats");/*========*/
    sec->rwOffset<act>(f);
    QFontMetrics fm = match->getFont();
    std::vector<int> maxWidths = std::vector<int>((int)statsInfo::count(),0);

    for(size_t s = 0; s < statsInfo::count(); ++s){
        const type statT = statsInfo::getStat(s).getStatValueType();
        for(uint8_t p = 0; p < m.totalPlayers; ++p){
            auto varData = [&](auto val){
                if(act == save) val = (decltype(val))m.getLastValue((int)p,(int)s);
                f.data(val);
                if(act == load) maxWidths[s] = std::max(maxWidths[s], fm.width(m.addStatCell((int)p,(int)s,(double)val)));
            };
            switch (statT) {
            case  sint32: varData( int32_t(0)); break;
            case  uint32: varData(uint32_t(0)); break;
            case float32: varData(   float(0)); break;
            }
        }
    }
    if(act == load) m.setColumnWidths(maxWidths);
    sec->rwSize(f);
    /*========*/SECTION("updateTimes");/*========*/
    sec->rwOffset<act>(f);
    f.data(m.halfTimeTick);
    f.data(m.extraTimeTick);
    f.data(m.extraHalfTimeTick);
    std::unordered_map<const match_time*,uint32_t> ptr2idx;
    rwSize(m.updateTimes);
    forAllIn(m.updateTimes,[&](match_time &t, uint32_t i){
        t.match = &m;
        f.data(t.timestamp);
        f.data(t.gameTick);
        f.data(t.gameMinute);
        f.data(t.injuryMinute);
        if(act == save) ptr2idx.insert({&t,i});
    });
    sec->rwSize(f);
    /*========*/SECTION("matchEvents");/*========*/
    sec->rwOffset<act>(f);
    forAllIn(m.events,[&](match_event &e, uint32_t){
        if(act == save) f.data(ptr2idx.at(e.when));
        if(act == load) e.when = &m.updateTimes[f.get<uint32_t>()];
        f.data(e.player1);
        f.data(e.player2);
        f.data(e.type);
        f.check(uint8_t(0),NoCheck{});
        if(act == load){
            e.isHome = m.isHome(e.player1);
            m.logEvent(e);
        }
    });
    sec->rwSize(f);
    /*========*/SECTION("playerStatChanges");/*========*/
    sec->rwOffset<act>(f);
    uint32_t nPlayerChanges[64] = {0};
    for(uint8_t p = 0; p < 64; ++p)
        nPlayerChanges[p] = rwSize(m.playerStatHistory[p]);
    for(uint8_t p = 0; p < m.totalPlayers; ++p){
        loopAll(m.playerStatHistory[p],nPlayerChanges[p],[&](stat_change &c, uint32_t){
            if(act == save) f.data(ptr2idx.at(c.when));
            if(act == load) c.when = &m.updateTimes[f.get<uint32_t>()];
            f.data(c.statId);
            switch(statsInfo::getStat(c.statId).getStatValueType()){
            case  sint32: f.data(c.newVal.i); break;
            case  uint32: f.data(c.newVal.u); break;
            case float32: f.data(c.newVal.f); break;
            }
            f.data(c.segment);
        });
    }
    sec->rwSize(f);
    RET_IF_ERROR("Writing sections table");
    //====Rewrite Section entries===//
    if(act == save){
        f.seek(sectionsPos);
        f.data(sections.data(),sections.size());//read/write the section table
    }
    f.close();
    if(act == load){
        match_time &lastTime = m.updateTimes.back();
        m.setLabels((int)lastTime.gameMinute,lastTime.gameMinute,lastTime.injuryMinute);
    }
    dbgReturn(return f.good);
}
template bool Match::file<load>(Match *match, QString filename);
template bool Match::file<save>(Match *match, QString filename);
