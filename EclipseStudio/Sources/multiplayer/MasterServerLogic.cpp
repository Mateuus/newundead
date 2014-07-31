#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "MasterServerLogic.h"

#include "ui/FrontEndWarZ.h"
#include "ui/m_LoadingScreen.h"

extern const char* getReputationString(int Reputation);
	MasterServerLogic gMasterServerLogic;
	FrontendWarZ* frontend;

MasterServerLogic::MasterServerLogic()
{
  InitializeCriticalSection(&csNetwork);
  isConnected_      = false;
  gameListReceived_ = false;
  gameJoinAnswered_ = false;
  badClientVersion_ = false;
  versionChecked_   = false;
  shuttingDown_     = false;
  masterServerId_   = 0;
}

MasterServerLogic::~MasterServerLogic()
{
  g_net.Deinitialize();
  
  DeleteCriticalSection(&csNetwork);
}

void MasterServerLogic::OnNetPeerConnected(DWORD peerId)
{
  r3dOutToLog("connected to master server peer%02d\n", peerId);
  
  // send validation packet immediately
  GBPKT_ValidateConnectingPeer_s n;
  n.version = GBNET_VERSION;
  n.key1    = GBNET_KEY1;
  net_->SendToHost(&n, sizeof(n), true);

  // set connected flag after sending validation packet.
  isConnected_ = true;
  
  return;
}

void MasterServerLogic::OnNetPeerDisconnected(DWORD peerId)
{
  isConnected_ = false;
  r3dOutToLog("disconnected from master server\n");
}

void MasterServerLogic::OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize)
{
  switch(packetData->EventID) 
  {
    default:
      r3dError("MasterServerLogic: invalid packetId %d", packetData->EventID);
      return;
      
    case GBPKT_M2C_ServerInfo:
    {
      const GBPKT_M2C_ServerInfo_s& n = *(GBPKT_M2C_ServerInfo_s*)packetData;
      
      masterServerId_ = n.serverId;
      r3d_assert(masterServerId_);

      break;
    }
      
    case GBPKT_ValidateConnectingPeer:
    {
      const GBPKT_ValidateConnectingPeer_s& n = *(GBPKT_ValidateConnectingPeer_s*)packetData;

      //check version
      versionChecked_ = true;
      if(n.version != GBNET_VERSION)
      {
        r3dOutToLog("Wrong client version. Please get update\n");
        badClientVersion_ = true;
      }

      break;
    }
    
    case GBPKT_M2C_ShutdownNote:
    {
      const GBPKT_M2C_ShutdownNote_s& n = *(GBPKT_M2C_ShutdownNote_s*)packetData;
      r3d_assert(sizeof(n) == packetSize);

      r3dOutToLog("Master server is shutting down...\n");
      shuttingDown_ = true;
      break;
    }
      
    case GBPKT_M2C_StartGamesList:
    {
      const GBPKT_M2C_StartGamesList_s& n = *(GBPKT_M2C_StartGamesList_s*)packetData;
      r3d_assert(sizeof(n) == packetSize);
      //r3dOutToLog("GBPKT_M2C_StartGamesList\n");

      r3d_assert(!gameListReceived_);
      break;
    }
    
    case GBPKT_M2C_SupervisorData:
    {
      const GBPKT_M2C_SupervisorData_s& n = *(GBPKT_M2C_SupervisorData_s*)packetData;
      r3d_assert(sizeof(n) == packetSize);
      //r3dOutToLog("GBPKT_M2C_GameData\n");
      
      supers_.push_back(n);
      break;
    }
    case GBPKT_C2M_RentGameAns:
    {
      const GBPKT_C2M_RentGameAns_s& n = *(GBPKT_C2M_RentGameAns_s*)packetData;
      r3d_assert(sizeof(n) == packetSize);
	  	  rentansnum = n.anscode;
	  sprintf(rentmsg,n.msg);
	  rentans = true;
	  break;
	}
    case GBPKT_M2C_GameData:
    {
      const GBPKT_M2C_GameData_s& n = *(GBPKT_M2C_GameData_s*)packetData;
      r3d_assert(sizeof(n) == packetSize);
      //r3dOutToLog("GBPKT_M2C_GameData\n");
      
      games_.push_back(n);
      break;
    }
    case GBPKT_M2C_EndGamesList:
    {
      const GBPKT_M2C_EndGamesList_s& n = *(GBPKT_M2C_EndGamesList_s*)packetData;
      r3d_assert(sizeof(n) == packetSize);
      r3dOutToLog("GBPKT_M2C_EndGamesList\n");
      gameListReceived_ = true;
      break;
    }
    
    case GBPKT_M2C_JoinGameAns:
    {
      const GBPKT_M2C_JoinGameAns_s& n = *(GBPKT_M2C_JoinGameAns_s*)packetData;
      r3d_assert(sizeof(n) == packetSize);
      //r3dOutToLog("GBPKT_M2C_JoinGameAns, res:%d\n", n.result);
      
      gameJoinAnswered_ = true;
      gameJoinAnswer_   = n;
      break;
    }
		        case GBPKT_C2M_PlayerListEnd:
    {
		const GBPKT_C2M_PlayerListEnd_s& n = *(GBPKT_C2M_PlayerListEnd_s*)packetData;
		  r3d_assert(sizeof(n) == packetSize);
		  PlayerListEnded = true;
		  break;
	}
        case GBPKT_C2M_PlayerListAns:
    {
		const GBPKT_C2M_PlayerListAns_s& n = *(GBPKT_C2M_PlayerListAns_s*)packetData;
		//  r3d_assert(sizeof(n) == packetSize);
		//frontend->addPlayerInfo(n.gamertag,n.alive,n.rep,n.xp);
		//sprintf(PlayerListData,"%s %d %d %d",n.gamertag,n.alive,n.rep,n.xp);
		/*char name[128];
		int alive;
		int rep;
		int xp;*/
		//sscanf(PlayerListData, "%s %d %d %d", &name, &alive, &rep, &xp);
		//frontend->addPlayerInfo(name,alive,rep,xp);
		//sprintf(gMasterServerLogic.PlayerListData,"");
			r3d_assert(numlist < R3D_ARRAYSIZE(ListPlayer));
	listplayer_s& pg = ListPlayer[numlist++];
	sprintf(pg.data,"%s %d %d %d",n.gamertag,n.alive,n.rep,n.xp);
		r3dOutToLog("GBPKT_C2M_PlayerListAns_s %s %d %d %d\n",n.gamertag,n.alive,n.rep,n.xp);
		break;
	}
  }

  return;
}

int MasterServerLogic::WaitFunc(fn_wait fn, float timeout, const char* msg)
{
  float endWait = r3dGetTime() + timeout;
  while(1) 
  {
    r3dEndFrame();
    r3dStartFrame();
    
    extern void tempDoMsgLoop();
    tempDoMsgLoop();
    
    if((this->*fn)())
      break;

    r3dRenderer->StartRender();
    r3dRenderer->StartFrame();
    r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA);
    Font_Label->PrintF(10, 10, r3dColor::white, "%s", msg);
    r3dRenderer->EndFrame();
    r3dRenderer->EndRender( true );
      
    if(r3dGetTime() > endWait) {
      return 0;
    }
  }
  
  return 1;
}

void MasterServerLogic::Tick()
{
  R3DPROFILE_FUNCTION("MasterServerLogic::Tick");
  EnterCriticalSection(&csNetwork);
  if(net_) {
    net_->Update();
  }
  LeaveCriticalSection(&csNetwork);

  return;
}

int MasterServerLogic::StartConnect(const char* host, int port)
{
  // hide inside critical section, because it can be called from other thread in FrontEndNew
  EnterCriticalSection(&csNetwork);

  versionChecked_ = false;
  shuttingDown_   = false;
  masterServerId_ = 0;
  
  g_net.Initialize(this, "master server");
  g_net.CreateClient();
  int res = g_net.Connect(host, port);
  
  LeaveCriticalSection(&csNetwork);

  return res;
}

void MasterServerLogic::Disconnect()
{
  // hide inside critical section, because it can be called from other thread in FrontEndNew
  EnterCriticalSection(&csNetwork);

  g_net.Deinitialize();
  isConnected_ = false;

  LeaveCriticalSection(&csNetwork);
}

void MasterServerLogic::RequestGameList()
{
  if(!net_ || !net_->IsConnected())
    r3dError("masterserver isn't connected\n");
  if(badClientVersion_) 
    r3dError("badClientVersion");
    
  gameListReceived_ = false;
  supers_.clear();
  supers_.reserve(64);
  games_.clear();
  games_.reserve(512);
  
  // send refresh command
  CREATE_PACKET(GBPKT_C2M_RefreshList, n);
  net_->SendToHost(&n, sizeof(n));
  
  return;
}

int MasterServerLogic::WaitForGameList()
{
  if(!WaitFunc(&MasterServerLogic::wait_GameListReceived, 10, "receiving game list"))
    r3dError("can't receive game list");
    
  /*
  r3dOutToLog("GameList: %d games\n", games_.size());
  for(size_t i=0; i<games_.size(); i++) {
    const GBPKT_M2C_GameData_s& g = games_[i];
    r3dOutToLog("game%d: players:%d\n", i, g.info.maxPlayers);
  }*/
    
  return 1;
}

int MasterServerLogic::WaitForGameJoin()
{	
  if( !DoConnectScreen( this, &MasterServerLogic::wait_GameJoinAsnwer, gLangMngr.getString("WaitingForJoinAnswer"), 10.f ) )
  {
    return 0;
  }
  
  if(gameJoinAnswer_.result != GBPKT_M2C_JoinGameAns_s::rOk)
  {
    return 0;
  }
  
  return 1;
}
void MasterServerLogic::SendKickReq(DWORD gameServerId , const char* name)
{
	CREATE_PACKET(GBPKT_C2S_RentGameKick, n);
    n.serverid = gameServerId;
	r3dscpy(n.name,name);
	net_->SendToHost(&n, sizeof(n));
}
void MasterServerLogic::SendRentReq()
{
	rentans = false;
	rentansnum = 0;
	sprintf(rentmsg,"");
    CREATE_PACKET(GBPKT_C2M_RentGameReq, n);
    net_->SendToHost(&n, sizeof(n));
}
void MasterServerLogic::SendPlayerListReq(DWORD gameServerId)
{
	CREATE_PACKET(GBPKT_C2M_PlayerListReq, n);
    n.serverId = gameServerId;
	net_->SendToHost(&n, sizeof(n));
}
void MasterServerLogic::SendJoinGame(DWORD gameServerId, const char* pwd)
{
  if(!net_ || !net_->IsConnected())
    r3dError("masterserver isn't connected\n");
  if(badClientVersion_) 
    r3dError("badClientVersion");

  r3d_assert(gameListReceived_);
  r3d_assert(gameServerId > 0);
  
  gameJoinAnswered_ = false;

  // send join game request
  CREATE_PACKET(GBPKT_C2M_JoinGameReq, n);
  n.CustomerID   = gUserProfile.CustomerID;
  n.gameServerId = gameServerId;
  r3dscpy(n.pwd, pwd);
  net_->SendToHost(&n, sizeof(n));
}
void MasterServerLogic::SendRentGameChangePwd(int id , const char* pwd)
{
		if(!net_ || !net_->IsConnected())
		r3dError("masterserver isn't connected\n");
	if(badClientVersion_) 
		r3dError("badClientVersion");

	CREATE_PACKET(GBPKT_C2M_RentGameChangeSet, n);
	r3dscpy(n.pwd,pwd);
	n.gameserverid = id;
	net_->SendToHost(&n, sizeof(n));
}
void MasterServerLogic::SendRentGame(const char* name,const char* pwd , int slot , int mapid , int period, int customerid)
{
	if(!net_ || !net_->IsConnected())
		r3dError("masterserver isn't connected Mateuus\n");
	if(badClientVersion_) 
		r3dError("badClientVersion");

	CREATE_PACKET(GBPKT_C2M_RentGameStr, n);
	r3dscpy(n.name, name);
	r3dscpy(n.pwd, pwd);
	n.mapid = mapid;
	n.ownercustomerid = customerid;
	n.slot = slot;
	n.period = period;
		net_->SendToHost(&n, sizeof(n));
}
void MasterServerLogic::SendBanPlayer(int customerid,const char* gamertag)
{
   CREATE_PACKET(GBPKT_C2M_BanPlayer, n);
	r3dscpy(n.name, gamertag);
	n.customerid = customerid;
    net_->SendToHost(&n, sizeof(n));
}
void MasterServerLogic::SendJoinQuickGame(const NetPacketsGameBrowser::GBPKT_C2M_QuickGameReq_s& n)
{
  if(!net_ || !net_->IsConnected())
    r3dError("masterserver isn't connected\n");
  if(badClientVersion_) 
    r3dError("badClientVersion");

  gameJoinAnswered_ = false;
  net_->SendToHost(&n, sizeof(n));
}

void MasterServerLogic::GetJoinedGameServer(char* out_ip, int* out_port, __int64* out_sessionId)
{
  r3d_assert(gameJoinAnswered_);
  r3d_assert(gMasterServerLogic.gameJoinAnswer_.result == GBPKT_M2C_JoinGameAns_s::rOk);
  
  DWORD ip = gameJoinAnswer_.ip;
  sprintf(out_ip, inet_ntoa(*(in_addr*)&ip));
  *out_port      = gameJoinAnswer_.port;
  *out_sessionId = gameJoinAnswer_.sessionId;
  
  return;
}