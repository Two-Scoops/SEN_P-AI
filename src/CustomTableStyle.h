#ifndef CUSTOMTABLESTYLE_H
#define CUSTOMTABLESTYLE_H

#include <QtGui>
#include <QProxyStyle>
#include <QHeaderView>
#include <QTableWidget>

class SidewaysTextHeader: public QHeaderView {
    Q_OBJECT
public:
    SidewaysTextHeader(QWidget *parent = Q_NULLPTR): QHeaderView(Qt::Horizontal,parent){}
protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    friend class QHeaderView;
};

class MyStyle : public QProxyStyle
{
    QWidget *target;
public:
    MyStyle(QStyle *style, QWidget *targetWidget) : QProxyStyle(style), target(targetWidget){}
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = 0) const
    {
        if (element == QStyle::CE_HeaderLabel && target->isAncestorOf(widget)) {
            const QHeaderView *hv = qobject_cast<const QHeaderView *>(widget);
            if (!hv || hv->orientation() != Qt::Horizontal)
                return QProxyStyle::drawControl(element, option, painter, widget);
            const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option);
            painter->save();
            painter->translate(header->rect.bottomLeft()+QPointF(10,0));
            painter->rotate(-90);
            QFont font(painter->font());
            QList<QTableWidgetSelectionRange> selected = ((QTableWidget*)widget)->selectedRanges();
            for(QTableWidgetSelectionRange range: selected){
                if(range.leftColumn() <= header->section && range.rightColumn() >= header->section)
                    font.setBold(true);
            }
            painter->setFont(font);
            painter->drawText(0,0,header->text);
            painter->restore();
            return;
        }
        return QProxyStyle::drawControl(element, option, painter, widget);
    }
};
#endif // CUSTOMTABLESTYLE_H
