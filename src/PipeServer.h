#ifndef PIPESERVER_H
#define PIPESERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <QJsonDocument>
#include <vector>
#include "Common.h"
#include "Match.h"

class PipeClient : public QObject{
    Q_OBJECT
public:
    explicit PipeClient(QLocalSocket *s, QObject *parent = 0): QObject(parent), socket(s){
        qDebug("Function Entered");
        connect(socket,&QLocalSocket::disconnected, this, &PipeClient::disconnected);
        //The Qt fuckwits decided to overload QLocalSocket::error and then make you do this casting bullshit
        connect(socket,static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
                this, &PipeClient::error);
        connect(socket,&QLocalSocket::readyRead, this, &PipeClient::readyRead);
        qDebug("Function returned");
    }

    void sendEvent(QJsonDocument event){
        qDebug("Function Entered");
        QByteArray data = event.toJson();
        qDebug().noquote() << data;
        quint32 size = data.size();
        const unsigned char sizeArr[] = {
            (size>> 0) & 255,
            (size>> 8) & 255,
            (size>>16) & 255,
            (size>>24) & 255
        };
        data.prepend((const char*)sizeArr,4);
        if(valid())
            socket->write(data);
        qDebug("Function returned");
    }

    bool valid() const{ return socket != nullptr; }
    quintptr descriptor() const{
        if(valid())
            return socket->socketDescriptor();
        else
            return -1;
    }

public slots:
    void disconnected(){
        qDebug("Function Entered");
        if(socket != nullptr){
            socket->abort();
            disconnect(socket,0,this,0);
        }
        disconnect(this,0,0,0);
        socket = nullptr;
        qDebug("Function returned");
    }
    void error(QLocalSocket::LocalSocketError){
        qDebug("Function Entered");
        if(socket != nullptr){
            socket->abort();
            disconnect(socket,0,this,0);
        }
        disconnect(this,0,0,0);
        socket = nullptr;
        qDebug("Function returned");
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
    void newEvent(match_event event);

    void table_lost(match_time *when);
    void table_found(match_time *when);
    void teamsChanged(Match *match);

private:
    QJsonDocument lastTeamsChanged;
    QJsonDocument lastStatsFound;
    QList<QJsonDocument> eventsThisMatch;
    bool areStatsFound = false;

    std::vector<PipeClient*> clients;
    QLocalServer *server;

    void broadcastEvent(QJsonDocument event, Match *match, qint64 timestamp);
    void broadcastEvent(QJsonDocument event, Match *match){ broadcastEvent(event,match,match->timestamp()); }
    void broadcastEvent(QJsonDocument event, match_time *when){ broadcastEvent(event,when->match,when->timestamp); }
};

#endif // PIPESERVER_H
