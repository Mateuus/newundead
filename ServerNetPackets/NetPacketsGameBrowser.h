#pragma once

#include "r3dNetwork.h"
#include "NetPacketsGameInfo.h"

namespace NetPacketsGameBrowser
{
#pragma pack(push)
#pragma pack(1)

#define GBNET_VERSION		(0x00000004 + GBGAMEINFO_VERSION)
#define GBNET_KEY1		0x25e022aa

//
// Game Browser Packet IDs
// 
enum gbpktType_e
{
  GBPKT_ValidateConnectingPeer = r3dNetwork::FIRST_FREE_PACKET_ID,

  // client vs master server codes
  GBPKT_C2M_RefreshList,
  GBPKT_M2C_StartGamesList,
  GBPKT_M2C_SupervisorData,
  GBPKT_M2C_GameData,
  GBPKT_M2C_EndGamesList,

   GBPKT_C2S_RentGameKick,

   GBPKT_C2M_BanPlayer,
  GBPKT_C2M_RentGameChangeSet,
  GBPKT_C2M_PlayerListEnd,
  GBPKT_C2M_JoinGameReq,
  GBPKT_C2M_RentGameReq,
  GBPKT_C2M_RentGameAns,
  GBPKT_C2M_RentGameStr,
  GBPKT_C2M_PlayerListReq,
  GBPKT_C2M_PlayerListAns,
  GBPKT_C2M_QuickGameReq,
  GBPKT_M2C_JoinGameAns,

  GBPKT_M2C_ServerInfo,
  GBPKT_M2C_ShutdownNote,

  GBPKT_LAST_PACKET_ID,
};
#if GBPKT_LAST_PACKET_ID > 255
  #error Shit happens, more that 255 packet ids
#endif

#ifndef CREATE_PACKET
#define CREATE_PACKET(PKT_ID, VAR) PKT_ID##_s VAR
#endif

struct GBPKT_ValidateConnectingPeer_s : public r3dNetPacketMixin<GBPKT_ValidateConnectingPeer>
{
	DWORD		version;
	DWORD		key1;
};

struct GBPKT_C2M_RefreshList_s : public r3dNetPacketMixin<GBPKT_C2M_RefreshList>
{
};

struct GBPKT_M2C_StartGamesList_s : public r3dNetPacketMixin<GBPKT_M2C_StartGamesList>
{
};

struct GBPKT_M2C_SupervisorData_s : public r3dNetPacketMixin<GBPKT_M2C_SupervisorData>
{
	GBPKT_M2C_SupervisorData_s& operator=(const GBPKT_M2C_SupervisorData_s& rhs) {
          memcpy(this, &rhs, sizeof(*this));
          return *this;
	}

	BYTE		region;
	WORD		ID;
	DWORD		ip;
};

struct GBPKT_M2C_GameData_s : public r3dNetPacketMixin<GBPKT_M2C_GameData>
{
	GBPKT_M2C_GameData_s& operator=(const GBPKT_M2C_GameData_s& rhs) {
          memcpy(this, &rhs, sizeof(*this));
          return *this;
	}

	WORD		superId; // ID of supervisor
	GBGameInfo	info;
	BYTE		status;  // 0-good. 1-finished, 2-full, 4-too_late, 8-passworded,16-not avail for join
	int		curPlayers;
};

struct GBPKT_M2C_EndGamesList_s : public r3dNetPacketMixin<GBPKT_M2C_EndGamesList>
{
};

struct GBPKT_C2M_JoinGameReq_s : public r3dNetPacketMixin<GBPKT_C2M_JoinGameReq>
{
	GBPKT_C2M_JoinGameReq_s() {
	  pwd[0] = 0;
	}
	
	DWORD		CustomerID;
	DWORD		gameServerId;
	char		pwd[16];
};

struct GBPKT_C2M_QuickGameReq_s : public r3dNetPacketMixin<GBPKT_C2M_QuickGameReq>
{
	DWORD		CustomerID;
	BYTE		region;		// EGBGameRegion
	BYTE		gameMap;	// 0xFF for any map
};

struct GBPKT_M2C_JoinGameAns_s : public r3dNetPacketMixin<GBPKT_M2C_JoinGameAns>
{
	GBPKT_M2C_JoinGameAns_s& operator=(const GBPKT_M2C_JoinGameAns_s &rhs) {
          memcpy(this, &rhs, sizeof(*this));
          return *this;
	}

	enum {
	  rUnknown = 0,
	  rNoGames,
	  rGameFull,
	  rGameFinished,
	  rGameNotFound,
	  rWrongPassword,
	  rLevelTooLow,
	  rLevelTooHigh,
	  rJoinDelayActive,
	  rInputPassword,
	  rOk,
	};
	GBPKT_M2C_JoinGameAns_s() {
	  result = rUnknown;
	}

	BYTE		result;
	DWORD		ip;
	WORD		port;
	__int64		sessionId;
};

struct GBPKT_M2C_ServerInfo_s : public r3dNetPacketMixin<GBPKT_M2C_ServerInfo>
{
	BYTE		serverId;
};
struct GBPKT_C2S_RentGameKick_s : public r3dNetPacketMixin<GBPKT_C2S_RentGameKick>
{
	DWORD		serverid;
	char name[32*2];
};
struct GBPKT_M2C_ShutdownNote_s : public r3dNetPacketMixin<GBPKT_M2C_ShutdownNote>
{
	BYTE		reason;
};
struct GBPKT_C2M_PlayerListEnd_s : public r3dNetPacketMixin<GBPKT_C2M_PlayerListEnd>
{
};
struct GBPKT_C2M_PlayerListReq_s : public r3dNetPacketMixin<GBPKT_C2M_PlayerListReq>
{
DWORD serverId;
};
struct GBPKT_C2M_PlayerListAns_s : public r3dNetPacketMixin<GBPKT_C2M_PlayerListAns>
{
    char		gamertag[32*2];
	int alive;
	int rep;
	int xp;
};
struct GBPKT_C2M_RentGameChangeSet_s : public r3dNetPacketMixin<GBPKT_C2M_RentGameChangeSet>
{
	DWORD gameserverid;
    char pwd[512];
};
struct GBPKT_C2M_RentGameReq_s : public r3dNetPacketMixin<GBPKT_C2M_RentGameReq>
{
	/*char		name[16];
	char pwd[16];
	int mapid;
	int slot;
	int period;
	int ownercustomerid;*/
};
struct GBPKT_C2M_RentGameAns_s : public r3dNetPacketMixin<GBPKT_C2M_RentGameAns>
{
	int anscode;
	char msg[128];
};

struct GBPKT_C2M_RentGameStr_s : public r3dNetPacketMixin<GBPKT_C2M_RentGameStr>
{
	char		name[16];
	char pwd[16];
	int mapid;
	int slot;
	int period;
	int ownercustomerid;
};

struct GBPKT_C2M_BanPlayer_s : public r3dNetPacketMixin<GBPKT_C2M_BanPlayer>
{
	char name[16];
    int customerid;
};



#pragma pack(pop)

}; // namespace NetPacketsGameBrowser
