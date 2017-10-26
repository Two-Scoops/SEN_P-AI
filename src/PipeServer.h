#ifndef PIPESERVER_H
#define PIPESERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSet>
#include <QString>
#include <QJsonDocument>
#include "Common.h"

class PipeClient : public QObject{
    Q_OBJECT
public:
    explicit PipeClient(QLocalSocket *s, QObject *parent = 0): QObject(parent), socket(s){
        connect(socket,&QLocalSocket::disconnected, this, &PipeClient::disconnected);
        //The Qt fuckwits decided to overload QLocalSocket::error and then make you do this casting bullshit
        connect(socket,static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
                this, &PipeClient::error);
        connect(socket,&QLocalSocket::readyRead, this, &PipeClient::readyRead);
    }

    void sendEvent(QJsonDocument event){
        if(valid())
            socket->write(event.toJson());
    }

    bool valid() const{ return socket != nullptr; }

public slots:
    void disconnected(){
        if(valid())
            socket->deleteLater();
        socket = nullptr;
    }
    void error(QLocalSocket::LocalSocketError){
        if(valid())
            socket->disconnectFromServer();
    }
    void readyRead(){
        if(valid())
            socket->readAll();
    }

private:
    QLocalSocket *socket;
};

class PipeServer : public QObject
{
    Q_OBJECT
public:
    explicit PipeServer(QObject *parent = 0);

signals:

public slots:
    void newConnection();
    void newEvent(matchEvent event, const QList<matchEvent> &);

    void table_lost(qint64 timestamp, float gameMinute, float injuryMinute);
    void table_found(qint64 timestamp, float gameMinute, float injuryMinute, int homeScore, int awayScore);
    void teamsChanged(qint64 timestamp, teamInfo home, teamInfo away);

private:
    QJsonDocument lastTeamsChanged;
    QJsonDocument lastStatsFound;
    QList<QJsonDocument> eventsThisMatch;
    bool areStatsFound = false;

    QList<PipeClient*> clients;
    QLocalServer *server;

    void broadcastEvent(QJsonDocument event);
};

#endif // PIPESERVER_H
