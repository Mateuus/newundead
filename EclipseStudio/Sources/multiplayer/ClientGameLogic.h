#pragma once

#include "r3dProtect.h"
#include "r3dNetwork.h"
#include "multiplayer/P2PMessages.h"
#include "./../EclipseStudio/Sources/UI/UIAsync.h"

class GameObject;
class obj_Player;

#include "../../ServerNetPackets/NetPacketsGameInfo.h"

#define FULL_AREA_EXPLOSION  360.0f

class ClientGameLogic : public r3dNetCallback
{
  protected:
	r3dNetwork	g_net;
	// r3dNetCallback virtuals
virtual	void		OnNetPeerConnected(DWORD peerId);
virtual	void		OnNetPeerDisconnected(DWORD peerId);
virtual	void		OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize);

  public:
	enum { MAX_NUM_PLAYERS = 320 };

	// player struct for hiding actual player pointer
	struct {
		r3dSec_type<obj_Player*, 0xDABB01C3> ptr;
	}		players2_[MAX_NUM_PLAYERS];
	obj_Player*	GetPlayer(int idx) const;
	void		SetPlayerPtr(int idx, obj_Player* ptr);
	int		CurMaxPlayerIdx;	// current max used index in players2_
	
	// list of players on server
	struct PlayerName_s
	{
		char	Gamertag[32*2];
		int		plrRep;
		bool	isLegend;
			bool	isPunisher;
					bool	Flashlight;
					bool	isInvitePending;
					/*bool	isGroups1;
					bool	isGroups2;
					bool	isGroups3;
					bool	isGroups4;
					bool	isGroups5;*/
						int	ShowCustomerID;
							int	MeCustomerID;
		int GroupID;
		bool	isDev;
		bool isPremium;
		char ip[64];
		bool isMute;
	};
	PlayerName_s	playerNames[MAX_NUM_PLAYERS]; // playername by server peer index

	struct HelpCalls_s
	{
		char		rewardText[128*2];
	char		DistressText[128*2];
		gp2pnetid_t netID;
		r3dPoint3D pos;
	};
	HelpCalls_s HelpCalls[MAX_NUM_PLAYERS];

	bool isBuyAns;
	int BuyAnsCode;
	void SendBuyItemReq(int ItemID);
	bool isLoaded;
    bool LastRain;
	void *footStepsSnd;
	DWORD		net_lastFreeId; // !!!ONLY use it for assigning networkID during loading map!!!
	float		localPlayerConnectedTime; // used for calculating how much time player was playing

	bool		serverConnected_;
	r3dSec_type<obj_Player*, 0x15CFB1B9> localPlayer_;
	volatile LONG	serverVersionStatus_;
	volatile bool	gameJoinAnswered_;
	int		localPlayerIdx_;
	volatile bool	gameStartAnswered_;
	int		gameStartResult_; // as PKT_S2C_StartGameAns_s::EResult
	GBGameInfo	m_gameInfo;

	struct DataInfos_s
	{
		gp2pnetid_t	spawnID;
	    char		fname[512];
	    r3dPoint3D pos;
	    r3dVector angle;
	    bool col;
	};

	__int64		gameStartUtcTime_;
	float		lastShadowCacheReset_;
	float		gameStartTime_;
	__int64		GetServerGameTime() const;
	void		UpdateTimeOfDay();
	
	__int64		m_sessionId;
	__int64		GetGameSessionID() {
		if(serverConnected_)
			return m_sessionId;
		else
			return 0;
	}

	float	m_highPingTimer;

	int		disconnectStatus_;	// 0 - playing, 1 - requested, 2 - acked
	int		ischeat;
	int     ShowCustomerID;
		char     GroupGamertag[512];
	char cheatmsg[512];
	float		disconnectReqTime_;	// time when disconnect was requested
	
	bool		gameShuttedDown_;
	
	// cheat things
	bool		cheatAttemptReceived_;	// true if server detected cheating
	BYTE		cheatAttemptCheatId_;	// server reason
	DWORD		nextSecTimeReport_;	// time when we need send next PKT_C2S_SecurityRep_s
	DWORD		gppDataSeed_;		// seed for sending crc of game player parameters
	bool		d3dCheatSent_;		// if we sended d3d cheat screenshot yet
	bool		d3dCheatSent2_;

	float lastBuilding;
	//UIAsync<ClientGameLogic> async_;
	
	#define DEFINE_PACKET_FUNC(XX) \
	  void On##XX(const XX##_s& n, GameObject* fromObj, DWORD peerId, bool& needPassThru);
	#define IMPL_PACKET_FUNC(CLASS, XX) \
	  void CLASS::On##XX(const XX##_s& n, GameObject* fromObj, DWORD peerId, bool& needPassThru)
	//  
	int		ProcessWorldEvent(GameObject* fromObj, DWORD eventId, DWORD peerId, const void* packetData, int packetSize);
	 DEFINE_PACKET_FUNC(PKT_C2S_ValidateConnectingPeer);
	 DEFINE_PACKET_FUNC(PKT_C2C_PacketBarrier);
	 DEFINE_PACKET_FUNC(PKT_S2C_JoinGameAns);
	 DEFINE_PACKET_FUNC(PKT_S2C_ShutdownNote);
	 DEFINE_PACKET_FUNC(PKT_S2C_SetGamePlayParams);
	 DEFINE_PACKET_FUNC(PKT_S2C_StartGameAns);
	 DEFINE_PACKET_FUNC(PKT_S2C_PlayerNameJoined);
	 DEFINE_PACKET_FUNC(PKT_S2C_UpdateEnvironment);
	 DEFINE_PACKET_FUNC(PKT_S2C_PlayerNameLeft);
	 DEFINE_PACKET_FUNC(PKT_S2C_CreatePlayer);
	  DEFINE_PACKET_FUNC(PKT_C2S_PlayerSetObStatus);
	 DEFINE_PACKET_FUNC(PKT_S2C_Damage);
	 DEFINE_PACKET_FUNC(PKT_S2C_ZombieAttack);
	 DEFINE_PACKET_FUNC(PKT_S2C_KillPlayer);
	 DEFINE_PACKET_FUNC(PKT_C2S_DisconnectReq);
	  DEFINE_PACKET_FUNC(PKT_S2C_SetAlreadyLoadMaps);
	  DEFINE_PACKET_FUNC(PKT_C2S_BuyItemAns);
	  
	 DEFINE_PACKET_FUNC(PKT_C2C_ChatMessage);
	  DEFINE_PACKET_FUNC(PKT_S2C_CheatMsg);
		
		     
			   DEFINE_PACKET_FUNC(PKT_S2C_SendHelpCallData);
			    DEFINE_PACKET_FUNC(PKT_C2S_SendHelpCall);
	//  DEFINE_PACKET_FUNC(PKT_C2C_GroupJoin);
	 DEFINE_PACKET_FUNC(PKT_S2C_UpdateWeaponData);
	 DEFINE_PACKET_FUNC(PKT_S2C_UpdateGearData);
	 DEFINE_PACKET_FUNC(PKT_S2C_CreateNetObject);
	  DEFINE_PACKET_FUNC(PKT_S2C_CreateVehicles);
	 // if (!g_FastLoad->GetBool())
	  DEFINE_PACKET_FUNC(PKT_S2C_CreateBuilding);

	 DEFINE_PACKET_FUNC(PKT_S2C_DestroyNetObject);
	 DEFINE_PACKET_FUNC(PKT_S2C_CreateDroppedItem);
	 DEFINE_PACKET_FUNC(PKT_S2C_CreateNote);
	  DEFINE_PACKET_FUNC(PKT_S2C_CreateGrave);
	   DEFINE_PACKET_FUNC(PKT_S2C_SetGraveData);
	     DEFINE_PACKET_FUNC(PKT_S2C_SetSafeLockData);
	    DEFINE_PACKET_FUNC(PKT_S2C_CreateSafeLock);
	 DEFINE_PACKET_FUNC(PKT_S2C_SetNoteData);
	 DEFINE_PACKET_FUNC(PKT_S2C_CreateZombie);
	 DEFINE_PACKET_FUNC(PKT_S2C_CheatWarning);
	 DEFINE_PACKET_FUNC(PKT_S2C_CreateAnimals);
    // DEFINE_PACKET_FUNC(PKT_S2C_SendHelpCall);

	r3dPoint3D	AdjustSpawnPositionToGround(const r3dPoint3D& pos);

  protected:
	typedef bool (ClientGameLogic::*fn_wait)();
	int		WaitFunc(fn_wait fn, float timeout, const char* msg);
	
	// wait functions
	bool		wait_IsConnected() {
	  return net_->IsConnected();
	}
	bool		wait_ValidateVersion() {
          return serverVersionStatus_ != 0;
        }
	bool		wait_GameJoin() {
	  return gameJoinAnswered_;
	}
	bool		wait_Loaded() {
	  return isLoaded;
	}
	bool		wait_GameStart();

  private:	
	// make copy constructor and assignment operator inaccessible
	ClientGameLogic(const ClientGameLogic& rhs);
	ClientGameLogic& operator=(const ClientGameLogic& rhs);

  private: // this is singleton, can't create directly.
	ClientGameLogic();
	virtual ~ClientGameLogic();
	
  public:
	static void CreateInstance();
	 //bool AntiCheat();
	static void DeleteInstance();
	static ClientGameLogic* GetInstance();

	bool		isHavelocalPlayer() {
	  return localPlayer_ != NULL;
	}

    bool isNeedToLoop;
	bool isNeedToRender;
	void		Reset();

	bool		Connect(const char* host, int port);
	void		Disconnect();

	int		RequestToJoinGame();

	int		RequestToStartGame();
	int		ValidateServerVersion(__int64 sessionId);
	
	void		RequestToDisconnect();

	void		ApplyExplosionDamage(const r3dVector& pos, float radius, int wpnIdx, const r3dVector& forwVector = R3D_ZERO_VECTOR, float direction = FULL_AREA_EXPLOSION );
	obj_Player* ClientGameLogic::FindPlayer(char* Name);
	obj_Player* ClientGameLogic::FindPlayerCustom(int CustomerID);
	void		Tick();
	void		SendScreenshot(IDirect3DSurface9* texture);
	void		 SendScreenshotFailed(int code);
	//void CreateBuildingThread(/*const PKT_S2C_CreateBuilding_s& n*/);
};

__forceinline ClientGameLogic& gClientLogic() {
	return *ClientGameLogic::GetInstance();
}

