#pragma once

#include "r3dNetwork.h"
#include "../../ServerNetPackets/NetPacketsGameBrowser.h"
using namespace NetPacketsGameBrowser;

class MasterServerLogic : public r3dNetCallback
{
  protected:
	CRITICAL_SECTION csNetwork;
	r3dNetwork	g_net;
	bool		isConnected_;

	// r3dNetCallback virtuals
virtual	void		OnNetPeerConnected(DWORD peerId);
virtual	void		OnNetPeerDisconnected(DWORD peerId);
virtual	void		OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize);

  public:
	volatile int	masterServerId_;
  
	volatile bool	gameListReceived_;
	std::vector<GBPKT_M2C_SupervisorData_s> supers_;
	std::vector<GBPKT_M2C_GameData_s> games_;
	
	volatile bool	gameJoinAnswered_;
	GBPKT_M2C_JoinGameAns_s gameJoinAnswer_;
	
	//char PlayerListData[128];
	struct listplayer_s
	{
	 char		data[128];
	  
	  listplayer_s()
	  {
	  }
	};
	int numlist;
	listplayer_s	ListPlayer[4096];
	bool rentans;
	int rentansnum;
	char rentmsg[128];
	void SendRentGameChangePwd(int id , const char* pwd);
	void SendKickReq(DWORD gameServerId , const char* name);
	void SendBanPlayer(int customerid,const char* gamertag);
	volatile bool PlayerListEnded;
	volatile bool	versionChecked_;
	volatile bool	shuttingDown_;
	// this should not be reinited on connect()
	// bad version should be fatal error
	bool		badClientVersion_;

  protected:
	typedef bool (MasterServerLogic::*fn_wait)();
	int		WaitFunc(fn_wait fn, float timeout, const char* msg);
	
	// wait functions
	bool		wait_GameListReceived() {
	  return gameListReceived_;
	}
	bool		wait_GameJoinAsnwer() {
	  return gameJoinAnswered_;
	}
	
  public:
	MasterServerLogic();
	virtual ~MasterServerLogic();

	int		StartConnect(const char* host, int port);
	void		Disconnect();

	bool		IsConnected() { return isConnected_; }
	
	void		RequestGameList();
	int		WaitForGameList();
    void    SendPlayerListReq(DWORD gameServerId);
	void		SendJoinGame(DWORD gameServerId, const char* pwd = "");
	void		SendJoinQuickGame(const NetPacketsGameBrowser::GBPKT_C2M_QuickGameReq_s& n);
	void         SendRentGame(const char* name,const char* pwd , int slot , int mapid , int period, int customerid);
	void         SendRentReq();
	int		WaitForGameJoin();
	void		GetJoinedGameServer(char* out_ip, int* out_port, __int64* out_sessionId);
	
	void		Tick();
};

extern	MasterServerLogic gMasterServerLogic;
