#ifndef STATSINFO_H
#define STATSINFO_H
#include <QString>
#include <QHash>
#include <stdint.h>
#include <math.h>

enum type:char {
    sint8, uint8, uint16, uint32, float32, uint32x9, uint32arr, validIndicator, cstring
};

struct PESstat {
    int visualIndex;
    ptrdiff_t offset;
    type statType;
    uint8_t count;
    QString name;
    bool shown;
    bool logged;
    //QString comment;

//    PESstat(int idx, ptrdiff_t _offset, type _type, uint8_t _count, const char *_name, bool _shown, bool _logged, const char *_comment = nullptr):
//        name(_name), comment(_comment), offset(_offset), visualIndex(idx), statType(_type), count(_count), shown(_shown), logged(_logged) {
//    }

    double getSegmentValue(uint8_t *data, uint8_t segment = 0) const{
        if(segment >= count) return std::nan("");
        switch (statType) {
#define CASE(VAL,TYPE) case VAL: return *(TYPE*)(data + offset + segment * sizeof(TYPE))
        CASE( sint8,  int8_t);
        CASE( uint8, uint8_t);
        CASE( uint16,uint16_t);
        CASE( uint32,uint32_t);
        CASE(float32,float);
        default: return std::nan("");
        }
    }

    double getValue(uint8_t *data) const{
        double result = 0;
        for(uint8_t i = 0; i < count; ++i)
            result += getSegmentValue(data,i);
        return result;
    }

    bool getChanged(uint8_t *curr, uint8_t *prev) const{
        return getValue(curr) != getValue(prev);
    }

    bool getSegmentChanged(uint8_t *curr, uint8_t *prev, uint8_t segment) const{
        return getSegmentValue(curr,segment) != getSegmentValue(prev,segment);
    }
};

class statsInfo
{
    static const size_t statCount;
    static const size_t playerEntrySize;
    static PESstat * const stats;
    static PESstat dummy;

    static QHash<QString,PESstat*> statMapping;
public:
    statsInfo();

    static void init(){
        statMapping = QHash<QString,PESstat*>();
        for(int s = 0; s < statCount; ++s){
            statMapping.insert(stats[s].name,stats+s);
        }
    }

    static PESstat& getStat(size_t i){
        if(i < statCount)
            return stats[i];
        else
            return dummy;
    }

    static PESstat& getStat(QString str){
        return *(statMapping.value(str,&dummy));
    }

    PESstat& operator[](size_t i){
        return getStat(i);
    }

    static size_t count() {
        return statCount;
    }

    static size_t entrySize(){
        return playerEntrySize;
    }


    enum KnownStats{
        playerID = 0, goalsScored = 1, assists = 5, yellowCards = 46, redCards = 47, saves = 112, firstMinuteOnPitch = 114, lastMinuteOnPitch = 115, currentPosition = 123,
        ticksAsGK = 124, ticksAsCF = 136,
        playerRating = 138, realTimeOnPitch = 140,
    };
};

#endif // STATSINFO_H
