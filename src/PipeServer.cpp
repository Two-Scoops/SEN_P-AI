#include "PipeServer.h"
#include <algorithm>
#include <QFile>


PipeServer::PipeServer(QObject *parent) : QObject(parent)
{
    server = new QLocalServer(this);
    connect(server,&QLocalServer::newConnection, this,&PipeServer::newConnection);
    server->listen("\\\\.\\pipe\\SEN_P-AI");
    if(!server->errorString().isEmpty())
        qDebug() << server->errorString();
}


void PipeServer::newConnection(){
    while(server->hasPendingConnections()){
        PipeClient *client = new PipeClient(server->nextPendingConnection(),this);
        client->sendEvent(lastTeamsChanged);
        client->sendEvent(lastStatsFound);
        clients.push_back(client);
        if(!server->errorString().isEmpty())
            qDebug() << server->errorString();
    }
    if(!server->errorString().isEmpty())
        qDebug() << server->errorString();
}
//TODO handle connections and incoming messages
void PipeServer::newEvent(matchEvent event, const QList<matchEvent> &){
    broadcastEvent(event.toJSON());
}


void PipeServer::table_lost(qint64 timestamp, float gameMinute, float injuryMinute){
    areStatsFound = false;
    QJsonObject event{
        {"type","Event"},
        {"event","Stats Lost"},
        {"timestamp",timestamp},
        {"gameMinute",gameMinute},
        {"injuryMinute",std::isnan(injuryMinute) ? QJsonValue(true) : QJsonValue(injuryMinute)}
    };
    if(injuryMinute < 0)
        event.remove("injuryMinute");
    broadcastEvent(QJsonDocument(event));
}

void PipeServer::table_found(qint64 timestamp, float gameMinute, float injuryMinute, int homeScore, int awayScore){
    areStatsFound = true;
    QJsonObject event{
        {"type","Event"},
        {"event","Stats Found"},
        {"timestamp",timestamp},
        {"gameMinute",gameMinute},
        {"injuryMinute",std::isnan(injuryMinute) ? QJsonValue(true) : QJsonValue(injuryMinute)},
        {"homeScore",homeScore},
        {"awayScore",awayScore}
    };
    if(injuryMinute < 0)
        event.remove("injuryMinute");
    lastStatsFound = QJsonDocument(event);
    broadcastEvent(lastStatsFound);
}

void PipeServer::teamsChanged(qint64 timestamp, teamInfo home, teamInfo away){
    QJsonArray homeArr, awayArr;
    for(int i = 0; i < home.nPlayers; ++i)
        homeArr.append(QJsonObject{ {"name",home.player[i].name}, {"playerId",(qint64)home.player[i].ID} });
    for(int i = 0; i < away.nPlayers; ++i)
        awayArr.append(QJsonObject{ {"name",away.player[i].name}, {"playerId",(qint64)away.player[i].ID} });

    QJsonObject event{
        {"type","Event"},
        {"event","Teams Changed"},
        {"timestamp",timestamp},
        {"home",
            QJsonObject {
                {"name",home.name},
                {"id",(qint64)home.ID},
                {"players",homeArr}
            }
        },
        {"away",
            QJsonObject{
                {"name",away.name},
                {"id",(qint64)away.ID},
                {"players",awayArr}
            }
        }
    };
    lastTeamsChanged = QJsonDocument(event);
    broadcastEvent(lastTeamsChanged);
}

void PipeServer::broadcastEvent(QJsonDocument event){
    QFile log(applyFilenameFormat(QSettings().value("EventLogFile", "SEN_P-AI_Events.log").toString()));
    log.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Append);
    log.write(event.toJson());
    log.close();
    std::remove_if(clients.begin(),clients.end(),[](PipeClient *client){ return !client->valid(); });
    for(PipeClient *client: clients){
        client->sendEvent(event);
    }
}
