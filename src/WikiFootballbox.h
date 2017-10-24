#ifndef WIKIFOOTBALLBOX_H
#define WIKIFOOTBALLBOX_H
#include <QStringList>
#include <QMap>
#include <algorithm>
#include <QRegularExpression>
#include <QRegExp>

enum eventType {
    goal, penaltyGoal, ownGoal, yellow, red
};

struct playerEvents {
    struct playerEvent {
        int minute, injuryMinute;
        eventType type;

        QString toString(){
            QString result = "|" + QString::number(minute);
            if(injuryMinute >= 0)
                result = result + "+" + QString::number(injuryMinute);
            switch (type) {
            case goal:        result += "|";     break;
            case penaltyGoal: result += "|pen."; break;
            case ownGoal:     result += "|o.g."; break;
            default: break;
            }
            return result;
        }

        bool operator <(const playerEvent &other){
            return (minute == other.minute) ? (injuryMinute <= other.injuryMinute) : (minute < other.minute);
        }
    };

    QString playerName;
    QList<playerEvent> events;

    QString toWikiText(bool isHome){
        QList<playerEvent> goals;
        QList<playerEvent> cards;
        for(playerEvent e : events){
            switch (e.type) {
            case goal: goals.append(e); break;
            case yellow:
            case red: cards.append(e); break;
            default: break;
            }
        }
        std::sort(goals.begin(),goals.end());
        std::sort(cards.begin(),cards.end());
        QString goalStr;
        if(goals.size() > 0){
            QStringList goalStrs;
            for(playerEvent goal : goals)
                goalStrs.append(goal.toString());
            goalStr = "{{goal|" + goalStrs.join("");
            if(*goalStr.end() == QChar('|'))
                goalStr.chop(1);
            goalStr += "}}";
        }
        QString cardStr;
        if(cards.size() == 1){
            playerEvent card = cards.first();
            if(card.type == red)
                cardStr = "{{sent off|0" + card.toString() + "}}";
            else if(card.type == yellow)
                cardStr = "{{yel" + card.toString() + "}}";
        } else if (cards.size() == 2){
            cardStr = "{{sent off|1" + cards[0].toString() + cards[1].toString() + "}}";
        } else if (cards.size() == 3){
            cardStr = "{{sent off|2" + cards[0].toString() + cards[1].toString() + "}}";
        }
        QString name = playerName;
        if(name.startsWith(QChar('>'))){
            name = "{{greentext|" + name.remove(0,1) + "}}";
        }
        QStringList result;
        if(isHome)
            result << playerName << goalStr << cardStr;
        else
            result << cardStr << goalStr << playerName;
        return result.join(QChar(' '));
    }
};

struct groupStanding{
    QString team;
    int W,D,L,GF,GA;
    QChar Q;

    groupStanding(const QString &wikiText){
        QStringList val = wikiText.split(QChar('|'));
        team = val[1].trimmed();
        W  = val[2].trimmed().toInt();
        D  = val[3].trimmed().toInt();
        L  = val[4].trimmed().toInt();
        GF = val[5].trimmed().toInt();
        GA = val[6].trimmed().toInt();
        Q = QRegularExpression("\\s*Q\\s*=\\s*(\\w?)\\w*\\s*}}").match(val[7]).captured(1).at(0);
    }

    QString toWikiText(int position){
        return QString("{{Group/%1|%2|%3|%4|%5|%6|%7|Q=%8}}\n").arg(position).arg(team).arg(W).arg(D).arg(L).arg(GF).arg(GA).arg(Q);
    }

    bool operator <(const groupStanding &other){
        int points = W*3 + D, otherPoints = other.W*3 + other.D;
        if(points != otherPoints)
            return points < otherPoints;
        else
            return GF < other.GF;
    }
};

struct groupStandings {
    QList<groupStanding> standings;

    groupStandings(const QString &wikiText){
        QRegularExpressionMatchIterator i = QRegularExpression("{{\\s*Group\\/\\d.*?}}").globalMatch(wikiText);
        while(i.hasNext())
            standings.append(groupStanding(i.next().captured(0)));
    }

    QString toWikiText(){
        QString result = QStringLiteral("{{Group/top}}\n");
        for(int i = 0; i < standings.size(); ++i)
            result.append(standings[i].toWikiText(i+1));
        result.append(QStringLiteral("{{Group/end}}\n"));
        return result;
    }
};


QDateTime parseStartDateTemplate(const QString &str){
    const int nFormats = 6;
    const QString formats[nFormats] = {
        QStringLiteral("{{Start date|yyyy|M|d|H|m|s"),
        QStringLiteral("{{Start date|yyyy|M|d|H|m"),
        QStringLiteral("{{Start date|yyyy|M|d|H"),
        QStringLiteral("{{Start date|yyyy|M|d"),
        QStringLiteral("{{Start date|yyyy|M"),
        QStringLiteral("{{Start date|yyyy")
    };
    for(int i = 0; i < nFormats; ++i){
        QDateTime t = QDateTime::fromString(str,formats[i]);
        if(t.isValid())
            return t;
    }
    return QDateTime();
}

QTime parseTimeTemplate(const QString &str){
    return QTime::fromString(str,QStringLiteral("{{time|h|s}}"));
}

struct footballBox {
    QMap<QString,QString> params;
    QStringList paramNames;
    QString home, away;
    QDateTime time;

    footballBox(){
        paramNames
            << "date"
            << "event"
            << "round"
            << "time"
            << "team1"
            << "team2"
            << "score"
            << "goals1"
            << "goals2"
            << "stadium"
            << "attendance"
            << "video";
        for(QString &str : paramNames){
            params.insert(str,QString());
        }
    }

    footballBox(const QString &str){
        QStringList lines = str.split('\n');
        QRegExp paramMatcher("^\\|\\s*(\\w*)\\s*=\\s*(.*)");
        QRegExp teamMatcher("{{team (?:home|away)\\|\\s*(\\w+)}}");
        for(QString line :lines){
            if(paramMatcher.indexIn(line) >= 0){
                QString paramName = paramMatcher.cap(1);
                QString paramValue = paramMatcher.cap(2);
                params.insert(paramName,paramValue);
                paramNames.append(paramName);
                if(paramName == "team1"){
                    teamMatcher.indexIn(paramValue);
                    home = teamMatcher.cap(1);
                } else if(paramName == "team2"){
                    teamMatcher.indexIn(paramValue);
                    away = teamMatcher.cap(1);
                } else if(paramName == "date"){
                    QTime t = time.time();
                    time = parseStartDateTemplate(paramValue);
                    if(t.isValid())
                        time.setTime(t);
                } else if(paramName == "time"){
                    time.setTime(parseTimeTemplate(paramValue));
                }
            }
        }
    }

    footballBox(const QString &homeTeam, const QString &awayTeam, QDateTime matchTime){
        setHome(homeTeam);
        setAway(awayTeam);
        setTime(matchTime);
    }

    void setHome(const QString &homeTeam){
        params[QStringLiteral("team1")] = QString("{{team home|%1}}").arg(home = homeTeam);
    }
    void setAway(const QString &awayTeam){

        params[QStringLiteral("team2")] = QString("{{team away|%1}}").arg(away = awayTeam);
    }
    void setTime(QDateTime matchTime){
        time = matchTime;
        params[QStringLiteral("date")] = time.toString(QStringLiteral("{{Start date|yyyy|MM|dd|df=y}}"));
        params[QStringLiteral("time")] = time.toString(QStringLiteral("{{Time|HH|mm}}"));
    }
    void setScore(int homeScore, int awayScore){
        params[QStringLiteral("score")] = QString("%1&ndash;%2").arg(homeScore).arg(awayScore);
    }

    QString toWikiText(){
        QString result = QStringLiteral("{{Football box\n");
        for(QString &paramName : paramNames){
            result.append(QString("| %1 = %2\n").arg(paramName,12).arg(params[paramName]));
        }
        result.append(QStringLiteral("}}\n"));
        return result;
    }
};
#endif // WIKIFOOTBALLBOX_H
