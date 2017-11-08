#include "QrTimestamp.h"
#include <QDateTime>
#include <QDebug>
#include <cstdio>

#include "lib/QR-Code-generator/QrCode.hpp"
using qrcodegen::QrCode;
using qrcodegen::QrSegment;

QrTimestamp::QrTimestamp(QLabel *_label, QObject *parent) : QObject(parent),
    img(29,29,QImage::Format_Mono), pxmp(29,29), label(_label)
{
    img.setColor(1,0xFF000000);
    img.setColor(0,0xFFFFFFFF);
    img.fill(0);
    pxmp.convertFromImage(img,0);
    label->setPixmap(pxmp);
}

void QrTimestamp::update(){
    char time_str[21];
    std::snprintf(time_str,21,"%llu",(unsigned long long)QDateTime::currentMSecsSinceEpoch());
    try{
        QrCode qr = QrCode::encodeSegments({QrSegment::makeNumeric(time_str)},QrCode::Ecc::HIGH,1,1);
        for(int y = 0; y < 21; ++y)
            for(int x = 0; x < 21; ++x)
                img.setPixel(x+4,y+4,qr.getModule(x,y) ?  1 : 0);
    }catch(const char* str){
        qDebug() << "QrCode Error:" << str;
    }
    pxmp.convertFromImage(img,0);
    label->setPixmap(pxmp);
}
