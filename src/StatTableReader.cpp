#include "StatTableReader.h"

#include <stdio.h>
#include <tchar.h>
#include <stdint.h>
#include <ctype.h>
#include <algorithm>
#include <Windows.h>
#include <tlhelp32.h>
#include <QDebug>


teamInfo currHome, currAway;

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
static uint8_t countTableEntries(HANDLE PES, uintptr_t addr, uint32_t *teamID) {
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
    if(entries >= 11 && teamID)
        *teamID = teamIdOf(playerID);
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


void StatTableReader::update(){
    //Check if process is still opened
    if (proccessHandle != NULL) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(proccessHandle, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                //Process was closed, close handle
                CloseHandle(proccessHandle);
                proccessHandle = NULL;
                basePtr = homePtr = awayPtr = teamDataPtr = nullptr;
                emit status_changed(QStringLiteral("%1 closed").arg(QString::fromWCharArray(proccessName)),0);
                emit table_lost();
                return;
            }
        } else {
            emit status_changed(QStringLiteral("Program does not have access to Exit Code"),0);
            return;
        }
    }

    if(proccessHandle){ //Procces is found
        if(teamDataPtr){ //Team Data is found
            emit status_changed(QStringLiteral("Updating data"),0);
            if(ReadProcessMemory(proccessHandle, teamDataPtr, prevTeamData, sizeof(teamDataTable), NULL)){ //Team Data read succeeds
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

                    emit status_changed(QStringLiteral("Teams changed"),0);
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


                if(basePtr){ //Stat Table is found
                    emit status_changed(QStringLiteral("Updating stats"),0);
                    int homeRead = ReadProcessMemory(proccessHandle, homePtr, data,                        _nHome*STAT_ENTRY_SIZE, NULL);
                    int awayRead = ReadProcessMemory(proccessHandle, awayPtr, data+_nHome*STAT_ENTRY_SIZE, _nAway*STAT_ENTRY_SIZE, NULL);
                    if(homeRead && awayRead){ //Table read succeeds
                        emit statTableUpdate(data);
                    } else { //Table read fails
                        basePtr = homePtr = awayPtr = nullptr;
                        emit status_changed(QStringLiteral("Stats table lost"),0);
                        emit table_lost();
                    }
                } else { //Table is lost
                    emit status_changed(QStringLiteral("Searching for stats table..."),0);
                    MEMORY_BASIC_INFORMATION info;

                    //Ask Windows for information on every block of memory in the proccess, starting at zero
                    for(uintptr_t mappingAddr = 0;
                        VirtualQueryEx(proccessHandle, (void*)mappingAddr, &info, sizeof(info)) > 0;
                        mappingAddr = mappingAddr + info.RegionSize) {
                        //If the properties of the memory block match what we're looking for
                        if ((info.State & STAT_MEM_STATE) && (info.Type & STAT_MEM_TYPE) && (info.Protect & STAT_MEM_PROTEC) && (info.RegionSize == STAT_MEM_SIZE)) {
                            //Check set memory offsets in the block and count how many Player IDs are found
                            uint8_t homeCount = countTableEntries(proccessHandle,(mappingAddr+HOME_OFFSET),&_homeID);
                            uint8_t awayCount = countTableEntries(proccessHandle,(mappingAddr+AWAY_OFFSET),&_awayID);

                            //If there are enough players to make two valid teams
                            if (homeCount >= 11 && awayCount >= 11) {
                                //Set target proccess pointers
                                basePtr = (uint8_t*) mappingAddr;
                                homePtr = (uint8_t*)(mappingAddr+HOME_OFFSET);
                                awayPtr = (uint8_t*)(mappingAddr+AWAY_OFFSET);
                                //If the number of valid players on each team is different than it was before, reallocate the player entries array and fix all the pointers
                                if(homeCount != _nHome || awayCount != _nAway){
                                    data = (uint8_t*)realloc(data,(homeCount+awayCount)*STAT_ENTRY_SIZE);
                                    //fix all the pointers and counts
                                    _nHome = homeCount;
                                    _nAway = awayCount;
                                    _nPlayers = homeCount + awayCount;
                                }
                                //Do an initial update of the stats table
                                ReadProcessMemory(proccessHandle, homePtr, data,                        _nHome*STAT_ENTRY_SIZE, NULL);
                                ReadProcessMemory(proccessHandle, awayPtr, data+_nHome*STAT_ENTRY_SIZE, _nAway*STAT_ENTRY_SIZE, NULL);
                                emit status_changed(QStringLiteral("Stats table found"),0);
                                emit table_found(data);
                                break;
                            }//End If enough valid players
                        }//End If matching memory block
                    }//End For every memory block in the proccess
                }//End If/Else stats table is found/lost
            } else { //Team data read fails
                teamDataPtr = nullptr;
                emit status_changed(QStringLiteral("Team Data table lost"),0);
                emit teamData_lost();
            }
        } else { //Team data is lost
            emit status_changed(QStringLiteral("Searching for Team Data table..."),0);
            MEMORY_BASIC_INFORMATION info;

            //Ask Windows for information on every block of memory in the proccess, starting at zero
            for(uintptr_t mappingAddr = 0;
                VirtualQueryEx(proccessHandle, (void*)mappingAddr, &info, sizeof(info)) > 0;
                mappingAddr = mappingAddr + info.RegionSize) {
                //If the properties of the memory block match what we're looking for
                if ((info.State & TEAM_MEM_STATE) && (info.Type & TEAM_MEM_TYPE) && (info.Protect & TEAM_MEM_PROTEC) && (info.RegionSize == TEAM_MEM_SIZE)) {
                    //Check set memory offsets in the block and count how many Player IDs are found
                    uint8_t playerCount = countTeamPlayerEntries(proccessHandle, mappingAddr);
                    //If there are enough players to make two valid teams
                    if(playerCount >= 22){
                        teamDataPtr = (teamDataTable*)(mappingAddr + TEAM_MEM_OFFSET);
                        //Do an initial read of the team data
                        ReadProcessMemory(proccessHandle, teamDataPtr, currTeamData, sizeof(teamDataTable), NULL);
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

                        currHome = home;
                        currAway = away;
                        emit status_changed(QStringLiteral("Team data found"),0);
                        emit teamsChanged(QDateTime::currentMSecsSinceEpoch(),home,away);
                        break;
                    } //End if enough valid players
                }//If matching memory block
            }//End For every memory block in the proccess
        }//End If/Else team data is found/lost

    } else { //Proccess is lost
        emit status_changed(QStringLiteral("Searching for %1...").arg(QString::fromWCharArray(proccessName)),0);
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
        //If a proccess was found
        if (pesID != 0) {
            //open handle to process
            proccessHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,FALSE,pesID);
            if (proccessHandle == NULL) {
                emit status_changed(QStringLiteral("Failed to open %1 with debug access\n").arg(QString::fromWCharArray(proccessName)),0);
            } else {
                emit status_changed(QStringLiteral("Found %1").arg(QString::fromWCharArray(proccessName)),0);
            }
        }
    }//End If/Else proccess found/lost
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
