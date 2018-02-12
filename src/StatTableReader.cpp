#include "StatTableReader.h"

#include <stdio.h>
#include <tchar.h>
#include <stdint.h>
#include <ctype.h>
#include <algorithm>
#include <Windows.h>
#include <tlhelp32.h>
#include <QDebug>

#define STAT_MEM_STATE MEM_COMMIT
#define STAT_MEM_TYPE MEM_PRIVATE
#define STAT_MEM_PROTEC PAGE_READWRITE
#define STAT_MEM_SIZE 589824
#define STAT_ENTRY_SIZE 4784
#define HOME_OFFSET 0x35010
#define AWAY_OFFSET 0x5DE40

#define isPlayerId(NUM) (NUM > 70099 && NUM < 90300)
#define teamIdOf(NUM) (NUM/100)
#define isSameTeam(A,B) (teamIdOf(A) == teamIdOf(B))

#define MAX_PLAYERS 32
static uint8_t countTableEntries(HANDLE PES, uintptr_t addr, uint32_t) {
    uint32_t playerID = 0;
    uint32_t maybeID = 0;
    int entries = 0;
    while (ReadProcessMemory(PES, (void*)addr, &maybeID, 4, NULL) && entries <= MAX_PLAYERS) {
        //printf("Testing value: %d\n", maybeID);
        if(!isPlayerId(maybeID))
            break;
        if(playerID > 0 && !isSameTeam(maybeID,playerID))
            break;
        if(playerID == 0)
            playerID = maybeID;
        entries++;
        addr += STAT_ENTRY_SIZE;
    }
    return entries;
}



#define TEAM_MEM_STATE MEM_COMMIT
#define TEAM_MEM_TYPE MEM_PRIVATE
#define TEAM_MEM_PROTEC PAGE_READWRITE
#define TEAM_MEM_SIZE 393216
#define TEAM_MEM_OFFSET 0x538B4
#define TEAM_HOME_PLAYER_OFFSET (TEAM_MEM_OFFSET + 0x0F8)
#define TEAM_AWAY_PLAYER_OFFSET (TEAM_MEM_OFFSET + 0x618)

static uint8_t countTeamPlayerEntries(HANDLE PES, uintptr_t base_addr){
    uint8_t count = 0;
    uint32_t homePlayers[66] = {0};
    uint32_t awayPlayers[66] = {0};
    int homeRead = ReadProcessMemory(PES, (void*)(base_addr + TEAM_HOME_PLAYER_OFFSET), homePlayers, 264, NULL);
    int awayRead = ReadProcessMemory(PES, (void*)(base_addr + TEAM_AWAY_PLAYER_OFFSET), awayPlayers, 264, NULL);
    if(homeRead && awayRead){
        for(int i = 0; i < 32; ++i){
            if((homePlayers[i*2] & 0xFFFF) == 0xFFFD && teamIdOf(homePlayers[i*2 + 1]) == homePlayers[65])
                count++;
        }
        for(int i = 0; i < 32; ++i){
            if((awayPlayers[i*2] & 0xFFFF) == 0xFFFE && teamIdOf(awayPlayers[i*2 + 1]) == awayPlayers[65])
                count++;
        }
    }
    return count;
}

bool StatTableReader::updateProcess(){
    //Check if process is still opened
    DWORD exitCode = 0;
    if (GetExitCodeProcess(processHandle, &exitCode)) {
        if (exitCode != STILL_ACTIVE) {
            //Process was closed, close handle
            CloseHandle(processHandle);
            processHandle = NULL;
            basePtr = homePtr = awayPtr = teamDataPtr = nullptr;
            emit status_changed(QStringLiteral("SEN:P-AI noticed %1 close").arg(QString::fromWCharArray(proccessName)));
            emit table_lost();
            return false;
        } else {
            return true;
        }
    } else {
        emit status_changed(QStringLiteral("SEN:P-AI does not have access to Exit Code"));
        return false;
    }
}

bool StatTableReader::findProcess(){
    emit action_changed(QStringLiteral("Searching for %1...").arg(QString::fromWCharArray(proccessName)));
    //Try to find process by name
    DWORD pesID = 0;
    {
        PROCESSENTRY32 entry;
        entry.dwSize = sizeof(PROCESSENTRY32);

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
        //Loop through every available proccess and compare its name to the target name
        if (Process32First(snapshot, &entry) == TRUE)
        {
            while (Process32Next(snapshot, &entry) == TRUE)
            {
                if (_wcsicmp(entry.szExeFile, proccessName) == 0)
                {
                    pesID =  entry.th32ProcessID;
                    break;
                }
            }
        }
        CloseHandle(snapshot);
    }
    //If a process was found
    if (pesID != 0) {
        //open handle to process
        processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,FALSE,pesID);
        if (processHandle == NULL) {
            emit status_changed(QStringLiteral("SEN:P-AI failed to open %1 with debug access\n").arg(QString::fromWCharArray(proccessName)));
            return false;
        } else {
            emit status_changed(QStringLiteral("SEN:P-AI noticed %1!").arg(QString::fromWCharArray(proccessName)));
            return true;
        }
    } else {//no process was found
        return false;
    }
}

bool StatTableReader::updateTeams(){
    emit action_changed(QStringLiteral("Updating data"));
    if(ReadProcessMemory(processHandle, teamDataPtr, prevTeamData, sizeof(teamDataTable), NULL)){ //Team Data read succeeds
        std::swap(currTeamData,prevTeamData);
        //If either team has changed
        if(currTeamData->homeData.teamID != prevTeamData->homeData.teamID || currTeamData->awayData.teamID != prevTeamData->awayData.teamID){
            teamInfo home, away;
            home.name = QString::fromUtf8(currTeamData->homeData.teamName);
            home.ID = currTeamData->homeData.teamID;
            home.nPlayers = 0;
            for(int i = 0; i < 32; ++i){
                if(currTeamData->homeData.players[i].playerValid.validIndicator != -1){
                    teamDataTable::playerEntry &player = currTeamData->homePlayers[i];
                    home.player[home.nPlayers++] = {QString::fromUtf8(player.playerName), player.playerID, player.playerCondition};
                }
            }

            away.name = QString::fromUtf8(currTeamData->awayData.teamName);
            away.ID = currTeamData->awayData.teamID;
            away.nPlayers = 0;
            for(int i = 0; i < 32; ++i){
                if(currTeamData->awayData.players[i].playerValid.validIndicator != -1){
                    teamDataTable::playerEntry &player = currTeamData->awayPlayers[i];
                    away.player[away.nPlayers++] = {QString::fromUtf8(player.playerName), player.playerID, player.playerCondition};
                }
            }

            emit status_changed(QStringLiteral("SEN:P-AI noticed new teams!"));
            emit teamsChanged(QDateTime::currentMSecsSinceEpoch(),home,away);
        } else { //If neither team has changed
            //go find what else changed
            for(size_t i = 0; i < sizeof(teamDataTable); ++i){
                uint8_t currByte = ((uint8_t*)currTeamData)[i];
                uint8_t prevByte = ((uint8_t*)prevTeamData)[i];
                if(currByte != prevByte)
                    emit teamDataChanged(i,prevByte,currByte);
            }
        }//End If/Else either team has/hasn't changed

        //TODO insert some team data shit here
        return true;
    } else { //Team data read fails
        teamDataPtr = nullptr;
        emit status_changed(QStringLiteral("SEN:P-AI lost track of teams"));
        emit teamData_lost();
        return false;
    }
}

bool StatTableReader::findTeams(){
    emit action_changed(QStringLiteral("Searching for Team Data table..."));
    MEMORY_BASIC_INFORMATION info;

    //Ask Windows for information on every block of memory in the proccess, starting at zero
    for(uintptr_t mappingAddr = 0;
        VirtualQueryEx(processHandle, (void*)mappingAddr, &info, sizeof(info)) > 0;
        mappingAddr = mappingAddr + info.RegionSize) {
        //If the properties of the memory block match what we're looking for
        if ((info.State & TEAM_MEM_STATE) && (info.Type & TEAM_MEM_TYPE) && (info.Protect & TEAM_MEM_PROTEC) && (info.RegionSize == TEAM_MEM_SIZE)) {
            //Check set memory offsets in the block and count how many Player IDs are found
            uint8_t playerCount = countTeamPlayerEntries(processHandle, mappingAddr);
            //If there are enough players to make two valid teams
            if(playerCount >= 22){
                teamDataPtr = (teamDataTable*)(mappingAddr + TEAM_MEM_OFFSET);
                //Do an initial read of the team data
                ReadProcessMemory(processHandle, teamDataPtr, currTeamData, sizeof(teamDataTable), NULL);
                teamInfo home, away;
                home.name = QString::fromUtf8(currTeamData->homeData.teamName);
                _homeID = home.ID = currTeamData->homeData.teamID;
                home.nPlayers = 0;
                for(int i = 0; i < 32; ++i){
                    if(currTeamData->homeData.players[i].playerValid.validIndicator != -1){
                        teamDataTable::playerEntry &player = currTeamData->homePlayers[i];
                        home.player[home.nPlayers++] = {QString::fromUtf8(player.playerName), player.playerID, player.playerCondition};
                    }
                }
                away.name = QString::fromUtf8(currTeamData->awayData.teamName);
                _awayID = away.ID = currTeamData->awayData.teamID;
                away.nPlayers = 0;
                for(int i = 0; i < 32; ++i){
                    if(currTeamData->awayData.players[i].playerValid.validIndicator != -1){
                        teamDataTable::playerEntry &player = currTeamData->awayPlayers[i];
                        away.player[away.nPlayers++] = {QString::fromUtf8(player.playerName), player.playerID, player.playerCondition};
                    }
                }
                emit status_changed(QStringLiteral("SEN:P-AI noticed the teams!"));
                emit teamsChanged(QDateTime::currentMSecsSinceEpoch(),home,away);
                return true;
            } //End if enough valid players
        }//End If matching memory block
    }//End For every memory block in the proccess
    return false;
}

bool StatTableReader::updateStats(){
    emit action_changed(QStringLiteral("Updating stats"));
    int homeRead = ReadProcessMemory(processHandle, homePtr, prevData,                        _nHome*STAT_ENTRY_SIZE, NULL);
    int awayRead = ReadProcessMemory(processHandle, awayPtr, prevData+_nHome*STAT_ENTRY_SIZE, _nAway*STAT_ENTRY_SIZE, NULL);
    if(homeRead && awayRead){ //Table read succeeds
        bool stillValid = true;
        for(int i = 0; i < _nHome + _nAway && stillValid; ++i){
            stillValid = stillValid && currData[i*STAT_ENTRY_SIZE] == prevData[i*STAT_ENTRY_SIZE];
        }
        if(stillValid){
            std::swap(currData,prevData);
            emit statTableUpdate(currData,prevData);
            return true;
        }
    } //Table read fails or player ids changed
    basePtr = homePtr = awayPtr = nullptr;
    emit status_changed(QStringLiteral("SEN:P-AI lost track of stats"));
    emit table_lost();
    return false;
}

void StatTableReader::doStatsFound(uint8_t homeCount, uint8_t awayCount){
    //If the number of valid players on each team is different than it was before, reallocate the player entries array and fix all the pointers
    if(homeCount != _nHome || awayCount != _nAway){
        currData = data = (uint8_t*)realloc(data,(homeCount+awayCount)*STAT_ENTRY_SIZE*2);
        prevData = currData + ((homeCount+awayCount)*STAT_ENTRY_SIZE);
        //fix all the pointers and counts
        _nHome = homeCount;
        _nAway = awayCount;
        _nPlayers = homeCount + awayCount;
    }
    //Do an initial update of the stats table
    ReadProcessMemory(processHandle, homePtr, currData,                        _nHome*STAT_ENTRY_SIZE, NULL);
    ReadProcessMemory(processHandle, awayPtr, currData+_nHome*STAT_ENTRY_SIZE, _nAway*STAT_ENTRY_SIZE, NULL);
    emit status_changed(QStringLiteral("SEN:P-AI noticed the stats!"));
    emit table_found(currData);
}

bool StatTableReader::findStats(){
    emit action_changed(QStringLiteral("Searching for stats table..."));
    MEMORY_BASIC_INFORMATION info;

    //Ask Windows for information on every block of memory in the proccess, starting at zero
    for(uintptr_t mappingAddr = 0;
        VirtualQueryEx(processHandle, (void*)mappingAddr, &info, sizeof(info)) > 0;
        mappingAddr = mappingAddr + info.RegionSize) {
        //If the properties of the memory block match what we're looking for
        if ((info.State & STAT_MEM_STATE) && (info.Type & STAT_MEM_TYPE) && (info.Protect & STAT_MEM_PROTEC) && (info.RegionSize == STAT_MEM_SIZE)) {
            //Check set memory offsets in the block and count how many Player IDs are found
            uint8_t homeCount = countTableEntries(processHandle,(mappingAddr+HOME_OFFSET),_homeID);
            uint8_t awayCount = countTableEntries(processHandle,(mappingAddr+AWAY_OFFSET),_awayID);

            //If there are enough players to make two valid teams
            if (homeCount >= 11 && awayCount >= 11) {
                //Set target proccess pointers
                basePtr = (uint8_t*) mappingAddr;
                homePtr = (uint8_t*)(mappingAddr+HOME_OFFSET);
                awayPtr = (uint8_t*)(mappingAddr+AWAY_OFFSET);
                doStatsFound(homeCount, awayCount);
                return true;
            }//End If enough valid players
        }//End If matching memory block
    }//End For every memory block in the proccess
    return false;
}

void StatTableReader::tryReadBlock(uintptr_t src, uint8_t *dest, size_t size){
    if(!ReadProcessMemory(processHandle,(void*)src,dest,size, NULL)){
        if(size == 1) *dest = 0;
        if(size <= 0) return;
        size_t half = size/2;
        tryReadBlock(src,dest,half);
        tryReadBlock(src+half,dest+half,size-half);
    }
}

#include "emmintrin.h"
bool StatTableReader::bruteForceStats(){
    MEMORY_BASIC_INFORMATION info;
#define AWAY_DIFF 0x28E30

    //Ask Windows for information on every block of memory in the proccess, starting at zero
    for(uintptr_t mappingAddr = 0;
        VirtualQueryEx(processHandle, (void*)mappingAddr, &info, sizeof(info)) > 0;
        mappingAddr = mappingAddr + info.RegionSize) {
        //If the properties of the memory block match what we're looking for
        if ((info.State & STAT_MEM_STATE) && (info.Type & STAT_MEM_TYPE) && (info.Protect & STAT_MEM_PROTEC) && (info.RegionSize > 0)){
            size_t regionSize = info.RegionSize;
            if(regionSize > searchSize){
                if(searchSize > 0)
                    _mm_free(searchSpace);
                searchSpace = (uint32_t*)_mm_malloc((searchSize = (uint32_t)regionSize),16);
                if(searchSpace == nullptr){
                    qDebug() << "Allocation failed";
                    return false;
                }
            }
//            tryReadBlock(mappingAddr,(uint8_t*)searchSpace,regionSize);
            if(!ReadProcessMemory(processHandle,(void*)(mappingAddr),searchSpace,regionSize, NULL))
                continue;
            const __m128i* ptr = (const __m128i*)searchSpace;
            const uint32_t player = (_homeID*100)+1;
            const __m128i comp = _mm_set1_epi32(player);//Compare to the player ID of home team player 1
            const __m128i mask = _mm_set1_epi8(~0);
            const size_t numVectors = (regionSize)/16;
            //For every 128bit vector in the valid search area
            for(size_t i = 0; i < numVectors; i++){
                __m128i eq = _mm_cmpeq_epi32(_mm_load_si128(ptr+i),comp);
                if(!_mm_test_all_zeros(eq,mask)){//If any elements matched
                    int j = -1;
                    while(ptr[i].m128i_u32[++j] != player);//check which element matched
                    uintptr_t homeAddr = mappingAddr + ((i*4)+j)*4;
                    uintptr_t awayAddr = homeAddr + AWAY_DIFF;

                    uint8_t homeCount = countTableEntries(processHandle,homeAddr,_homeID);
                    uint8_t awayCount = countTableEntries(processHandle,awayAddr,_awayID);
                    //If there are enough players to make two valid teams
                    if (homeCount >= 11 && awayCount >= 11) {
                        //Set target proccess pointers
                        basePtr = (uint8_t*) mappingAddr;
                        homePtr = (uint8_t*) homeAddr;
                        awayPtr = (uint8_t*) awayAddr;
                        doStatsFound(homeCount, awayCount);
                        return true;
                    }//End If enough valid players
                }//End If any elements matched
            }//End for every 128bit vector
        }//End If matching memory block
    }//End For every memory block in the proccess
    return false;
}


void StatTableReader::update(){
    switch (status) {
    default:
    case None:
        if(findProcess()) status = Process;
        break;
    case Process:
        if(!updateProcess()) status = None;
        else if(findTeams()) status = Teams;
        break;
    case Teams:
        if(!updateTeams()) status = Process;
        else if(findStats()) status = Stats;
        break;
    case Stats:
        if(!updateStats()) status = Teams;
        break;
    case BruteForce:
        if(bruteForceStats()) status = Stats;
        else status = Teams;
        break;
    }
    butt->setEnabled(status == Teams);
}


int StatTableReader::GetDebugPrivilege()
{
    HANDLE hProcess=GetCurrentProcess();
    HANDLE hToken;

    if (!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
        return FALSE;
    LUID luid;
    BOOL bRet=FALSE;

    if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
    {
        TOKEN_PRIVILEGES tp;

        tp.PrivilegeCount=1;
        tp.Privileges[0].Luid=luid;
        tp.Privileges[0].Attributes=(TRUE) ? SE_PRIVILEGE_ENABLED: 0;
        //
        //  Enable the privilege or disable all privileges.
        //
        if (AdjustTokenPrivileges(hToken, FALSE, &tp, NULL, (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL))
        {
            //
            //  Check to see if you have proper access.
            //  You may get "ERROR_NOT_ALL_ASSIGNED".
            //
            bRet=(GetLastError() == ERROR_SUCCESS);
        }
    }

    CloseHandle(hToken);
    return bRet;
}
