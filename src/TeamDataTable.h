#ifndef TEAMDATATABLE_H
#define TEAMDATATABLE_H

#include <stdint.h>
struct validPair;
struct validPair{
    enum indicator_t: short {
        invalid = -1,
        home = -2,
        away = -3
    } validIndicator;
    int16_t indexThingy; //team dependant
};

struct teamDataTable{
    struct {
        validPair teamValid;
        char teamName[70];
        char scoreboardName[4];
        uint16_t unknown1; //0xFFFF
        uint8_t unknown2[97]; //0x00 X 97
        char banner1Text[16];
        char banner2Text[16];
        char banner3Text[16];
        char banner4Text[16];
        uint8_t bannerEditedFlags[4]; //same as EDIT0
        uint8_t unknown3[3]; //possibly padding
        struct {
            validPair playerValid;
            uint32_t playerID;
        } players[32];
        validPair teamValid2; //indexThingy set to 0x0000
        uint32_t teamID;
        uint8_t shirtNumbers[32];
        struct {
            uint8_t unknown[16]; //0xFF FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        } unknown4[15];
        uint8_t unknown5[528];//Got no fuckin clue
    } homeData, awayData;

    struct teamTactics{
        uint8_t unknown6[32];//0x00 X 32
        uint8_t unknown7[4];//0x32 X 4
        uint8_t unknown8[8];//02 02 00 02 00 02 00 00
        validPair managerValid;//indexThingy set to 0x0000
        uint32_t managerID;
        char managerName[46];
        uint8_t unknown9[6];//D7 00 28 14 FF FF
        //Tactics
        struct Preset{
            struct Formation {
                enum:char {
                    GK = 0, CB, LB, RB, DMF, CMF, LMF, RMF, AMF, LWF, RWF, SS, CF
                } positions[11];
                struct {
                    uint8_t Y, X;
                } coordinates[11];
            } formation[3];
            uint8_t attackingStyle;//0: Counter Attack, 1: Possession Game
            uint8_t buildUp;//0: Long-pass, 1: Short-pass
            uint8_t attackingArea;//0: Wide, 1: Center
            uint8_t positioning; //0: Maintain Formation, 1: Flexible
            uint8_t defenseiveStyles; //0: Frontline pressure, 1: All-out Defence
            uint8_t containmentArea; //0: Middle, 1: Wide
            uint8_t pressuring; //0: Aggressive, 1: Conservative
            uint8_t unknownA[2]; //Padding?
            struct AdvancedInstruction {
                enum: char {
                    OFF = 0,
                    hugTheTouchline,
                    falsNo9,
                    falseFullBacks,
                    attackingFullbacks,
                    wingRotation,
                    tikiTaka,
                    centeringTargets,
                    swarmTheBox,
                    deepDefensiveLine,
                    gegenpress,
                    tightMarking,
                    counterTarget
                } instruction;
                uint8_t unknownB [3];
                uint8_t targetedPlayer;
                uint8_t unknownC [3];
            } attack1, attack2, defence1, defence2;
            uint8_t supportRange; //1-10 slider
            uint8_t numbersInAttack; //Unused, set to 3
            uint8_t defensiveLine; //1-10 slider
            uint8_t compactness; //1-10 slider
            uint8_t numbersInDefence; //Unused, set to 3
            uint8_t manmarking[11];
            bool fluidFormation;
            uint8_t unknownD[3];
        } preset[3];
        uint8_t playerLineup[32];
        uint8_t longFKTaker;
        uint8_t shortFKTaker;
        uint8_t FKTaker2;
        uint8_t leftCKTaker;
        uint8_t rightCKTaker;
        uint8_t PKTaker;
        uint8_t playersToJoinAttack[3];
        uint8_t captian;
        enum:char {
            off, veryLate, flexible, veryEarly
        } autoSubstitution;
        bool autoOffsideTrap;
        bool autoChangePresetTactics;
        bool autoChangeAttackDefenceLevel;
        uint8_t autoLineupSelect;//0: Unedited, 2: By Ability
        uint8_t unknownE;
    } homeTactics, awayTactics;

    struct playerEntry{
        uint8_t height;//(cm)
        uint8_t weight;//(kg)
        uint8_t unknown10[28];//bytes from the Player Entry in EDIT0, potentially modified or partially out of order

        enum:unsigned char {
            purple = 0x00, //down arrow
            blue   = 0x20, //down-right arrow
            green  = 0x40, //right arrow
            orange = 0x60, //up-right arrow
            red    = 0x80,  //up arrow
            _bitmask = 0xE0
        } playerCondition; //bits[0-4]: unknown, bits[5-7]: conidtion
        uint8_t unknown12[13];
        validPair playerValid;
        uint32_t playerID;
        uint32_t commentaryID; //probably
        char playerName[46];
        char printName[18];
        uint32_t unknown13[26]; //0x00000000, 0xFFFF0000, or 0xFFFFFFFF
        uint32_t unknown14; //0x00000001
        uint32_t unknown15; //0x00000015
    } homePlayers[32], awayPlayers[32];
};
#endif // TEAMDATATABLE_H
