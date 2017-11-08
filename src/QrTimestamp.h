#ifndef QRTIMESTAMP_H
#define QRTIMESTAMP_H

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QLabel>

class QrTimestamp : public QObject
{
    Q_OBJECT

    QImage img;
    QPixmap pxmp;
    QLabel *label;

public:
    explicit QrTimestamp(QLabel *_label, QObject *parent = 0);

signals:

public slots:
    void update();
};

#endif // QRTIMESTAMP_H
