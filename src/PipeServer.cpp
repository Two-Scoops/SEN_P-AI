#include "PipeServer.h"
#include <algorithm>
#include <QFile>


PipeServer::PipeServer(QObject *parent) : QObject(parent)
{
    server = new QLocalServer(this);
    connect(server,&QLocalServer::newConnection, this,&PipeServer::newConnection);
    server->setSocketOptions(QLocalServer::WorldAccessOption);
    server->listen("\\\\.\\pipe\\SEN_P-AI");
    if(!server->errorString().isEmpty())
        qDebug() << server->errorString();
}


void PipeServer::newConnection(){
    qDebug("Function Entered");
    while(server->hasPendingConnections()){
        PipeClient *client = new PipeClient(server->nextPendingConnection(),this);
        if(!lastTeamsChanged.isEmpty()){
            client->sendEvent(lastTeamsChanged);
            if(!lastStatsFound.isEmpty())
                client->sendEvent(lastStatsFound);
        }
        clients.push_back(client);
        if(!server->errorString().isEmpty())
            qDebug() << server->errorString();
    }
    if(!server->errorString().isEmpty())
        qDebug() << server->errorString();
    dbgReturn();
}
//TODO handle connections and incoming messages
void PipeServer::newEvent(match_event event){
    qDebug("Function Entered");
    broadcastEvent(event.toJSON(),event.when->match);
    dbgReturn();
}


void PipeServer::table_lost(match_time *when){
    qDebug("Function Entered");
    areStatsFound = false;
    QJsonObject event{
        {"type","Event"},
        {"event","Stats Lost"},
        {"timestamp",when->timestamp},
        {"gameMinute",when->gameMinute},
        {"injuryMinute",std::isnan(when->injuryMinute) ? QJsonValue(true) : QJsonValue(when->injuryMinute)}
    };
    if(when->injuryMinute < 0)
        event.remove("injuryMinute");
    broadcastEvent(QJsonDocument(event),when->match);
    dbgReturn();
}

void PipeServer::table_found(match_time *when){
    qDebug("Function Entered");
    areStatsFound = true;
    QJsonObject event{
        {"type","Event"},
        {"event","Stats Found"},
        {"timestamp",when->timestamp},
        {"gameMinute",when->gameMinute},
        {"injuryMinute",std::isnan(when->injuryMinute) ? QJsonValue(true) : QJsonValue(when->injuryMinute)},
        {"homeScore",when->match->homeScore},
        {"awayScore",when->match->awayScore}
    };
    if(when->injuryMinute < 0)
        event.remove("injuryMinute");
    lastStatsFound = QJsonDocument(event);
    broadcastEvent(lastStatsFound, when->match);
    dbgReturn();
}

void PipeServer::teamsChanged(Match *match){
    qDebug("Function Entered");
    QJsonArray homeArr, awayArr;
    for(int i = 0; i < match->home.nPlayers; ++i)
        homeArr.append(QJsonObject{ {"name",match->home.player[i].name}, {"playerId",(qint64)match->home.player[i].ID} });
    for(int i = 0; i < match->away.nPlayers; ++i)
        awayArr.append(QJsonObject{ {"name",match->away.player[i].name}, {"playerId",(qint64)match->away.player[i].ID} });

    QJsonObject event{
        {"type","Event"},
        {"event","Teams Changed"},
        {"timestamp",match->timestamp()},
        {"home",
            QJsonObject {
                {"name",match->home.name},
                {"id",(qint64)match->home.ID},
                {"players",homeArr}
            }
        },
        {"away",
            QJsonObject{
                {"name",match->away.name},
                {"id",(qint64)match->away.ID},
                {"players",awayArr}
            }
        }
    };
    lastTeamsChanged = QJsonDocument(event);
    broadcastEvent(lastTeamsChanged, match);
    dbgReturn();
}

void PipeServer::broadcastEvent(QJsonDocument event, Match *match, qint64 timestamp){
    qDebug("Function Entered");
    QFile log(match->applyFilenameFormat(QSettings().value("EventLogFile", "SEN_P-AI_Events.log").toString(),timestamp));
    log.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Append);
    log.write(event.toJson());
    log.close();
    auto i = clients.begin();
    for(; i != clients.end(); ++i){
        if((*i)->valid())
            (*i)->sendEvent(event);
        else{
            (*i)->deleteLater();
            clients.erase(i);
        }
    }
    dbgReturn();
}
