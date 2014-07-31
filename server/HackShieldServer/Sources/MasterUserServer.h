#pragma once

#include "MasterServer.h"
#include "MasterGameServer.h"

#include "../../ServerNetPackets/NetPacketsGameBrowser.h"
using namespace NetPacketsGameBrowser;

class CMasterUserServer : public r3dNetCallback
{
  public:
	// peer-status array for each peer
	enum EPeerStatus
	{
	  PEER_Free,
	  PEER_Connected,
	  PEER_Validated,
	};
	struct peer_s 
	{
	  EPeerStatus	status;
	  DWORD		peerUniqueId;	// index that is unique per each peer, 16bits: peerId, 16bits: curPeerUniqueId_

	  float		connectTime;
	  float		lastReqTime;

	  peer_s() 
	  {
	    status     = PEER_Free;
	  }
	};
	peer_s*		peers_;
	int		MAX_PEERS_COUNT;
	int		numConnectedPeers_;
	int		maxConnectedPeers_;
	DWORD		curPeerUniqueId_;	// counter for unique peer checking

	void		DisconnectIdlePeers();
	void		DisconnectCheatPeer(DWORD peerId, const char* message, ...);
	bool		DisconnectIfShutdown(DWORD peerId);
	bool		DoValidatePeer(DWORD peerId, const r3dNetPacketHeader* PacketData, int PacketSize);
	
	// callbacks from r3dNetwork
	void		OnNetPeerConnected(DWORD peerId);
	void		OnNetPeerDisconnected(DWORD peerId);
	void		OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize);

	bool		Validate(const GBPKT_C2M_RefreshList_s& n) { return true; }
	bool		Validate(const GBPKT_C2M_JoinGameReq_s& n);
	bool		Validate(const GBPKT_C2M_QuickGameReq_s& n) { return true; }
	bool		Validate(const GBPKT_C2M_RentGame_s& n) { return true; }
	bool		Validate(const GBPKT_C2M_PlayerListReq_s& n) { return true; }
	bool		Validate(const GBPKT_C2M_RentGameChangeSet_s& n) { return true; }
	bool		Validate(const GBPKT_C2S_RentGameKick_s& n) { return true; }
	
	void		OnGBPKT_C2M_RefreshList(DWORD peerId, const GBPKT_C2M_RefreshList_s& n);
	void		OnGBPKT_C2M_JoinGameReq(DWORD peerId, const GBPKT_C2M_JoinGameReq_s& n);
	void		OnGBPKT_C2M_QuickGameReq(DWORD peerId, const GBPKT_C2M_QuickGameReq_s& n);
	void OnGBPKT_C2S_RentGameKick(DWORD peerId, const GBPKT_C2S_RentGameKick_s& n);
void OnGBPKT_C2M_RentGame(DWORD peerId, const GBPKT_C2M_RentGame_s& n);
void OnGBPKT_C2M_RentGameChangeSet(DWORD peerId, const GBPKT_C2M_RentGameChangeSet_s& n);
void OnGBPKT_C2M_PlayerListReq(DWORD peerId, const GBPKT_C2M_PlayerListReq_s& n);
	void		DoJoinGame(CServerG* game, DWORD CustomerID, const char* pwd, GBPKT_M2C_JoinGameAns_s& ans);
	void		CreateNewGame(const CMSNewGameData& ngd, GBPKT_M2C_JoinGameAns_s& ans);
	void    SendPlayerList(DWORD peerId , int alive , const char* name , int rep , int xp);
void SendPlayerListEnd(DWORD peerId);
	void		PrintStats();
	void		Temp_Debug1();

  public:
	CMasterUserServer();
	~CMasterUserServer();

	void		Start(int port, int in_maxPeerCount);
	void		Tick();
	void		Stop();
};

extern	CMasterUserServer gMasterUserServer;
