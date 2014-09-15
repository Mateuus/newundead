#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"
#include <shellapi.h>
#include "process.h"

#include "CkHttpRequest.h"
#include "CkHttp.h"
#include "CkHttpResponse.h"
#include <Tlhelp32.h>
#pragma warning(disable: 4065)	// switch statement contains 'default' but no 'case' labels

#include "MasterGameServer.h"
#include "MasterUserServer.h"
#include "MasterServer.h"

#include "../../MasterServer/Sources/NetPacketsServerBrowser.h"
using namespace NetPacketsServerBrowser;

#include "ObjectsCode/weapons/WeaponConfig.h"
#include "ServerWeapons/MasterServerWeaponArmory.h"
#include "../../EclipseStudio/Sources/backend/WOBackendAPI.h"

#include "../EclipseStudio/Sources/GameLevel.h"
#include "../EclipseStudio/Sources/GameLevel.cpp"

CMasterGameServer gMasterGameServer;
r3dNetwork	serverNet;

static __int64 WarGuardRestartTime = _time64(&WarGuardRestartTime)+1800;
extern int		gDomainPort;
extern bool		gDomainUseSSL;

CMasterGameServer::CMasterGameServer()
{
	curSuperId_ = 1;

	itemsDbUpdateThread_ = NULL;
	itemsDbUpdateFlag_   = ITEMSDBUPDATE_Unactive;
	itemsDbLastUpdate_   = 0;
	newWeaponArmory_     = NULL;

	shuttingDown_        = false;

	return;
}

CMasterGameServer::~CMasterGameServer()
{
}
//void UpdateCmd()
//{
//	char n[512];
//	scanf("%s",&n);
//	r3dOutToLog("%s\n",n);
//}
void CMasterGameServer::SendBanPlayer(int gameserverid,int customerid , const char* gamertag)
{
	SBPKT_M2G_BanPlayer_s n;
	n.customerid = customerid;
	r3dscpy(n.gamertag,gamertag);
	net_->SendToPeer(&n,sizeof(n),gameserverid);
}
void CreateProcessByName(const char* name)
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	char param[512];
	sprintf(param,"\"%s\"",name);
	BOOL res = CreateProcess(
		NULL,
		param,
		NULL,
		NULL,
		FALSE,
		DETACHED_PROCESS, // no console by default, if needed game server will alloc it
		NULL,
		NULL,
		&si,
		&pi);
	if (res)
		r3dOutToLog("CreateProcess %s\n",param);
	else
		r3dOutToLog("CreateProcess %s failed\n",param);
}
void CMasterGameServer::Start(int port, int in_serverId)
{
	//CreateThread(NULL,NULL,(LPTHREAD_START_ROUTINE) UpdateCmd,NULL,0,0);
	SetConsoleTitle("WO::Master");

	masterServerId_  = in_serverId;
	r3d_assert(masterServerId_ > 0 && masterServerId_ < 255);

	// give time for supervisors to connect to us (except for dev server 2000)
	supersCooldown_  = r3dGetTime() + gServerConfig->supervisorCoolDownSeconds_;

#if ENABLED_SERVER_WEAPONARMORY
	DoFirstItemsDbUpdate();
#endif

	serverNet.Initialize(this, "serverNet");
	if(!serverNet.CreateHost(port, MAX_PEERS_COUNT)) {
		r3dError("CreateHost failed\n");
	}

	r3dOutToLog("MasterGameServer started at port %d\n", port);
	nextLogTime_ = r3dGetTime();
	// CreateProcessByName("WarGuard Server.exe");
	/*	r3dFile* f = r3d_open("tile.bin", "rb");
	if (!f)
	{
	r3dOutToLog("TERRAIN3: Could not open 'tile.bin'\n");
	}

	uint32_t hdr = 0;
	//r3dTL::fread_be(hdr, f);

	//const uint32_t COLL_ELEMS_HEADER_V1 = 'CEV1';
	int tileLayersCount;
	fread( &tileLayersCount, sizeof(tileLayersCount), 1, f );
	r3dOutToLog("tile %d\n",tileLayersCount);*/
	return;
}

bool CMasterGameServer::IsGameServerIdStarted(int gameServerId)
{
	const float curTime = r3dGetTime();

	// scan supervisors for active
	for(TSupersList::iterator it = supers_.begin(); it != supers_.end(); ++it) 
	{
		const CServerS* super = it->second;

		// for each game slot with correct permanent game index
		for(int gslot=0; gslot<super->maxGames_; gslot++)
		{
			if(super->games_[gslot].info.ginfo.gameServerId != gameServerId)
				continue;

			// closing games is not active and not free
			if(curTime < super->games_[gslot].closeTime)
				continue;

			// check if slot is not used
			if(super->games_[gslot].game == NULL &&
				(curTime > super->games_[gslot].createTime + CServerS::REGISTER_EXPIRE_TIME)) {
					continue;
			}

			const CServerG* game = super->games_[gslot].game;
			if(game == NULL) {
				// game not started yet (within registering time) - consider it as active
				return true;
			}

			// game is fully active
			return true;
		}
	}

	return false;
}
void killProcessByName(const char *filename)
{
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
	PROCESSENTRY32 pEntry;
	pEntry.dwSize = sizeof (pEntry);
	BOOL hRes = Process32First(hSnapShot, &pEntry);
	while (hRes)
	{
		if (strcmp(pEntry.szExeFile, filename) == 0)
		{
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0,
				(DWORD) pEntry.th32ProcessID);
			if (hProcess != NULL)
			{
				TerminateProcess(hProcess, 9);
				CloseHandle(hProcess);
			}
		}
		hRes = Process32Next(hSnapShot, &pEntry);
	}
	CloseHandle(hSnapShot);
}

void CMasterGameServer::UpdateWarGuardServer()
{
	__int64 secs1 = _time64(&secs1);
	if (secs1 > WarGuardRestartTime)
	{
		r3dOutToLog("WarGuard Server Expired. restarting\n");
		WarGuardRestartTime = _time64(&WarGuardRestartTime)+3600;
		killProcessByName("WarGuard Server.exe");
		CreateProcessByName("WarGuard Server.exe");
	}
}
void CMasterGameServer::UpdateRentGames()
{
	for(CMasterGameServer::TSupersList::const_iterator it = gMasterGameServer.supers_.begin();
		it != gMasterGameServer.supers_.end();
		++it)
	{
		const CServerS* super = it->second;
		for(int i=0; i<super->maxGames_; i++) 
		{
			CServerG* game = super->games_[i].game;
			if(!game) 
				continue;
			// find server and send req to server
			if (game->info_.ginfo.ispass && !game->info_.ginfo.isexited)
			{
				__int64 secs1 = _time64(&secs1);
				if (secs1 > game->info_.ginfo.expirein) // expired server
				{
					r3dOutToLog("RentServer %d expired. send shut down..\n",game->info_.ginfo.gameServerId);
					game->info_.ginfo.isexited = true;
					SendKillGame(game->info_.ginfo.gameServerId);
				}
			}
		}
	}
}
void CMasterGameServer::UpdatePermanentGames()
{
	static float nextCheck = -1;
	const float curTime = r3dGetTime();
	if(curTime < nextCheck)
		return;
	nextCheck = curTime + 0.5f;

	// no new games if we shutting down
	if(shuttingDown_)
		return;

	// don't do anything if there is no supervisors registered
	if(supers_.size() == 0)
		return;

	// scan each permanent game slot and spawn new game if there isn't enough of them
	for(int pgslot=0; pgslot<gServerConfig->numPermGames_; pgslot++)
	{
		const CMasterServerConfig::permGame_s& pg = gServerConfig->permGames_[pgslot];

		// check if we have supervisors with correct region, if not - skip it
		bool haveSupers = false;
		for(TSupersList::iterator it = supers_.begin(); it != supers_.end(); ++it) {
			const CServerS* super = it->second;
			if(super->region_ == pg.ginfo.region) {
				haveSupers = true;
				break;
			}
		}
		// silently continue, no need to spam log about that
		if(!haveSupers) {
			//r3dOutToLog("permgame: %d, no supervisors in region %d\n", pgslot, pg.ginfo.region);
			continue;
		}

		// there should be only ONE game server of that ID running
		if(IsGameServerIdStarted(pg.ginfo.gameServerId))
			continue;

		r3dOutToLog("gameServerId %d - spawning new\n", pg.ginfo.gameServerId);
		CLOG_INDENT;

		// spawn new game
		CMSNewGameData ngd(pg.ginfo, "", 0);

		DWORD ip;
		DWORD port;
		__int64 sessionId;
		if(!CreateNewGame(ngd, &ip, &port, &sessionId)) {
			continue;
		}

		/*
		r3dOutToLog("permgame: %d(%d out of %d) created at %s:%d\n", 
		pgslot,
		pg.curGames,
		pg.maxGames,
		inet_ntoa(*(in_addr*)&ip), 
		port);
		*/      

		// spawn 50 games per sec to avoid all servers spawning at once
		nextCheck = curTime + 0.02f;
		break;
	}

}
void CMasterGameServer::SendPlayerKick(const char* name,DWORD svpeerId)
{
	SBPKT_M2G_SendPlayerKick_s n;
	r3dscpy(n.name,name);
	net_->SendToPeer(&n, sizeof(n), svpeerId, true);
}
void CMasterGameServer::SendPlayerListReq(DWORD peerId,DWORD svpeerId)
{
	SBPKT_M2G_SendPlayerReq_s n;
	n.ClientPeerId = peerId;
	net_->SendToPeer(&n, sizeof(n), svpeerId, true);
}
void CMasterGameServer::SendKillGame(int gameServerId)
{
	r3dOutToLog("--------- SHUTDOWN requested to serverid %d\n",gameServerId);
	SBPKT_M2G_ShutdownNote_s n;
	n.reason   = 0;
	n.timeLeft = 300;
	for(CMasterGameServer::TSupersList::const_iterator it = gMasterGameServer.supers_.begin();
		it != gMasterGameServer.supers_.end();
		++it)
	{
		const CServerS* super = it->second;
		for(int i=0; i<super->maxGames_; i++) 
		{
			const CServerG* game = super->games_[i].game;
			if(!game) 
				continue;
			// find server and send req to server
			if (game->info_.ginfo.gameServerId == gameServerId)
			{
				net_->SendToPeer(&n, sizeof(n), game->peer_, true);
				r3dOutToLog("SHUTDOWN requested to serverid %d success\n",gameServerId);
				return;
			}
		}
	}
}
void CMasterGameServer::RequestShutdown()
{
	if(shuttingDown_)
		return;

	r3dOutToLog("--------- SHUTDOWN requested\n");

	shuttingDown_ = true;
	shutdownLeft_ = 300.0f;

	// notify each game about shutdown
	SBPKT_M2G_ShutdownNote_s n;
	n.reason   = 0;
	n.timeLeft = shutdownLeft_ - 5.0f; // make that game server shutdown 5 sec before us

	for(TGamesList::iterator it = games_.begin(); it != games_.end(); ++it) 
	{
		const CServerG* game = it->second;
		net_->SendToPeer(&n, sizeof(n), game->peer_, true);
	}

}


void CMasterGameServer::Tick()
{
	const float curTime = r3dGetTime();

	net_->Update();

	DisconnectIdlePeers();
	//UpdateCmd();
	// do not start games if supervisor cooldown is active
	//UpdateWarGuardServer();
	if(r3dGetTime() > supersCooldown_)
	{
		//supersCooldown_ = r3dGetTime()+supersCooldown_
		UpdatePermanentGames();
		UpdateRentGames();
		r3dCloseCFG_Cur();
	}
	/*static float nextupdaterent = r3dGetTime();
	if (r3dGetTime() > nextupdaterent)
	{
	nextupdaterent = r3dGetTime() + 30;
	gServerConfig->LoadRentConfig();
	}*/

#if ENABLED_SERVER_WEAPONARMORY
	UpdateItemsDb();
#endif

	if(shuttingDown_)
		shutdownLeft_ -= r3dGetFrameTime();

	return;
}

void CMasterGameServer::Stop()
{
	if(net_)
		net_->Deinitialize();
}

void CMasterGameServer::DisconnectIdlePeers()
{
	const float IDLE_PEERS_CHECK = 0.2f;	// do checks every N seconds
	const float IDLE_PEERS_DELAY = 5.0f;	// peer have this N seconds to validate itself

	static float nextCheck = -1;
	const float curTime = r3dGetTime();
	if(curTime < nextCheck)
		return;
	nextCheck = curTime + IDLE_PEERS_CHECK;

	for(int i=0; i<MAX_PEERS_COUNT; i++) 
	{
		if(peers_[i].status != PEER_Connected)
			continue;

		if(curTime < peers_[i].connectTime + IDLE_PEERS_DELAY)
			continue;

		DisconnectCheatPeer(i, "validation time expired");
	}

	return;
}

void CMasterGameServer::DisconnectCheatPeer(DWORD peerId, const char* message)
{
	r3dOutToLog("cheat: master peer%d, reason: %s\n", peerId, message);
	net_->DisconnectPeer(peerId);

	// fire up disconnect event manually, enet might skip if if other peer disconnect as well
	OnNetPeerDisconnected(peerId);
}

void CMasterGameServer::OnNetPeerConnected(DWORD peerId)
{
	peer_s& peer = peers_[peerId];
	r3d_assert(peer.status == PEER_Free);

	peer.status = PEER_Connected;
	peer.connectTime = r3dGetTime();

	// report our version
	CREATE_PACKET(SBPKT_ValidateConnectingPeer, n);
	n.version = SBNET_VERSION;
	n.key1    = 0;
	net_->SendToPeer(&n, sizeof(n), true);

	return;
}

void CMasterGameServer::OnNetPeerDisconnected(DWORD peerId)
{
	peer_s& peer = peers_[peerId];
	switch(peer.status)
	{
	case PEER_Free:
		break;
	case PEER_Connected:
		break;
	case PEER_Validated:
		break;

	case PEER_GameServer:
		{
			TGamesList::iterator it = games_.find(peerId);
			// see if game was already closed successfully
			if(it == games_.end())
				break;

			CServerG* game = it->second;

			r3dOutToLog("game %s closed unexpectedly\n", game->GetName());
			DeleteGame(game);

			games_.erase(it);
			break;
		}

	case PEER_SuperServer:
		{
			TSupersList::iterator it = supers_.find(peerId);
			if(it == supers_.end()) {
				break;
			}

			CServerS* super = it->second;

			r3dOutToLog("master: super disconnected '%s'[%d] \n", super->GetName(), super->id_);
			//TODO? disconnect all games from there.
			delete super;

			supers_.erase(it);

			// supervisor is disconnected, this might be network problem and other supervisors will be disconnecting as well
			// so we initiate cooldown time to wait for disconnected games, etc, etc
			if(r3dGetTime() > supersCooldown_) {
				r3dOutToLog("starting cooldown timer\n");
				supersCooldown_ = r3dGetTime() + 60.0f;
			}

			break;
		}
	}

	peer.status = PEER_Free;
	return;
}

bool CMasterGameServer::DoValidatePeer(DWORD peerId, const r3dNetPacketHeader* PacketData, int PacketSize)
{
	peer_s& peer = peers_[peerId];

	if(peer.status >= PEER_Validated)
		return true;

	// we still can receive packets after peer was force disconnected
	if(peer.status == PEER_Free)
		return false;

	r3d_assert(peer.status == PEER_Connected);
	if(PacketData->EventID != SBPKT_ValidateConnectingPeer) {
		DisconnectCheatPeer(peerId, "wrong validate packet id");
		return false;
	}

	const SBPKT_ValidateConnectingPeer_s& n = *(SBPKT_ValidateConnectingPeer_s*)PacketData;
	if(sizeof(n) != PacketSize) {
		DisconnectCheatPeer(peerId, "wrong validate packet size");
		return false;
	}
	if(n.version != SBNET_VERSION) {
		DisconnectCheatPeer(peerId, "wrong validate version");
		return false;
	}
	if(n.key1 != SBNET_KEY1) {
		DisconnectCheatPeer(peerId, "wrong validate key");
		return false;
	}

	// set peer to validated
	peer.status = PEER_Validated;
	return false;
}

void CMasterGameServer::OnNetData(DWORD peerId, const r3dNetPacketHeader* PacketData, int PacketSize)
{
	if(PacketSize < sizeof(r3dNetPacketHeader)) {
		DisconnectCheatPeer(peerId, "too small packet");
		return;
	}

	// wait for validating packet
	if(!DoValidatePeer(peerId, PacketData, PacketSize))
		return;

	peer_s& peer = peers_[peerId];
	r3d_assert(peer.status >= PEER_Validated);

	switch(PacketData->EventID) 
	{
	default:
		r3dOutToLog("CMasterGameServer: invalid packetId %d", PacketData->EventID);
		DisconnectCheatPeer(peerId, "wrong packet id");
		return;

	case SBPKT_S2M_RegisterMachine:
		{
			const SBPKT_S2M_RegisterMachine_s& n = *(SBPKT_S2M_RegisterMachine_s*)PacketData;
			r3d_assert(sizeof(n) == PacketSize);

			r3d_assert(supers_.find(peerId) == supers_.end());

			DWORD ip = net_->GetPeerIp(peerId);
			DWORD id = curSuperId_++;

			// check if that super is already connected or wasn't disconnected yet
			for(TSupersList::iterator it = supers_.begin(); it != supers_.end(); ++it) 
			{
				CServerS* super = it->second;
				if((super->ip_ == ip || super->ip_ == n.externalIpAddr) && strcmp(super->GetName(), n.serverName) == 0)
				{
					r3dOutToLog("master: DUPLICATE super registered '%s' ip:%s\n", n.serverName, inet_ntoa(*(in_addr*)&ip));
					net_->DisconnectPeer(peerId);
					return;
				}
			}

			CServerS* super = new CServerS(id, ip, peerId);
			super->Init(n);
			supers_.insert(TSupersList::value_type(peerId, super));

			r3d_assert(peer.status == PEER_Validated);
			peer.status = PEER_SuperServer;

			// send registration answer
			{
				SBPKT_M2S_RegisterAns_s n;
				n.id = id;
				net_->SendToPeer(&n, sizeof(n), peerId);
			}
			if (n.region == GBNET_REGION_US_West)
			{
			//r3dOutToLog("super private zone registered Create RentServer now...\n");
			gServerConfig->LoadRentConfig();
			}
			break;
		}
	case SBPKT_M2G_SendPlayerListEnd:
		{
			const SBPKT_M2G_SendPlayerListEnd_s& n = *(SBPKT_M2G_SendPlayerListEnd_s*)PacketData;
			r3d_assert(sizeof(n) == PacketSize);
			//r3dOutToLog("SBPKT_M2G_SendPlayerListEnd\n");
			gMasterUserServer.SendPlayerListEnd(n.ClientPeerId);
			break;
		}
	case SBPKT_M2G_SendPlayerList:
		{
			const SBPKT_M2G_SendPlayerList_s& n = *(SBPKT_M2G_SendPlayerList_s*)PacketData;
			//r3dOutToLog("SBPKT_M2G_SendPlayerList\n");
			//r3d_assert(sizeof(n) == PacketSize);
			gMasterUserServer.SendPlayerList(n.ClientPeerId,n.alive,n.gamertag,n.rep,n.xp);
			break;
		}
	case SBPKT_G2M_SendScreenShot:
		{
			break;
		}
	case SBPKT_G2M_ScreenShotData:
		{
			const SBPKT_G2M_ScreenShotData_s& n = *(SBPKT_G2M_ScreenShotData_s*)PacketData;
			//r3dOutToLog("Received SS %s\n",n.fname);
			extern bool isEnabledSS;
			if (!isEnabledSS) break;

			_mkdir("SSAH");
			FILE* f = fopen(n.fname, "wb");
			if(f == NULL) {
				r3dOutToLog( "SaveScreenshot unable to save fname:%s", n.fname);
				return;
			}
			fwrite((char*)PacketData + sizeof(n), 1, n.size, f);
			fclose(f);
			break;
		}
	case SBPKT_G2M_SendNoticeMsg:
		{
			const SBPKT_G2M_SendNoticeMsg_s& n = *(SBPKT_G2M_SendNoticeMsg_s*)PacketData;
			r3dOutToLog("Received notice message from peer%d '%s'\n",peerId,n.msg);
			//r3dOutToLog("Brostcast to all servers.\n");
			// scans servers.
			int numgames = 0;
			for(CMasterGameServer::TSupersList::const_iterator it = gMasterGameServer.supers_.begin();
				it != gMasterGameServer.supers_.end();
				++it)
			{
				const CServerS* super = it->second;
				numgames += super->GetExpectedGames();
				for(int i=0; i<super->maxGames_; i++) 
				{
					const CServerG* game = super->games_[i].game;
					if(!game) 
						continue;
					// resend this packet to all servers.
					net_->SendToPeer(&n, sizeof(n), game->peer_, true);
				}
			}
			r3dOutToLog("Send notice packet to %d servers\n",numgames);
			break;
		}
	case SBPKT_G2M_RegisterGame:
		{
			const SBPKT_G2M_RegisterGame_s& n = *(SBPKT_G2M_RegisterGame_s*)PacketData;
			r3d_assert(sizeof(n) == PacketSize);
			r3d_assert(peer.status == PEER_Validated);

			r3dOutToLog("game 0x%x connected\n", n.gameId);

			// register this game in supervisor
			CServerS* super = GetServerByGameId(n.gameId);
			if(super == NULL) {
				// this might happen when supervisor crashed between game start & registration
				r3dOutToLog("game 0x%x without supervisor\n", n.gameId);

				SBPKT_M2G_KillGame_s n1;
				net_->SendToPeer(&n1, sizeof(n1), peerId);
				net_->DisconnectPeer(peerId);
				break;
			}

			CServerG* game = super->CreateGame(n.gameId, peerId);
			r3d_assert(game);

			games_.insert(TGamesList::value_type(peerId, game));

			r3d_assert(peer.status == PEER_Validated);
			peer.status = PEER_GameServer;

#if ENABLED_SERVER_WEAPONARMORY
			SendArmoryInfoToGame(game);
#endif
			break;
		}

	case SBPKT_G2M_AddPlayer:
		{
			const SBPKT_G2M_AddPlayer_s& n = *(SBPKT_G2M_AddPlayer_s*)PacketData;
			r3d_assert(sizeof(n) == PacketSize);

			TGamesList::iterator it = games_.find(peerId);
			r3d_assert(it != games_.end());
			CServerG* game = it->second;
			game->AddPlayer(n.CustomerID);
			break;
		}

	case SBPKT_G2M_RemovePlayer:
		{
			const SBPKT_G2M_RemovePlayer_s& n = *(SBPKT_G2M_RemovePlayer_s*)PacketData;
			r3d_assert(sizeof(n) == PacketSize);

			TGamesList::iterator it = games_.find(peerId);
			r3d_assert(it != games_.end());
			CServerG* game = it->second;
			game->RemovePlayer(n.CustomerID);
			break;
		}

	case SBPKT_G2M_FinishGame:
		{
			const SBPKT_G2M_FinishGame_s& n = *(SBPKT_G2M_FinishGame_s*)PacketData;
			r3d_assert(sizeof(n) == PacketSize);

			TGamesList::iterator it = games_.find(peerId);
			r3d_assert(it != games_.end());
			CServerG* game = it->second;

			r3dOutToLog("game %s finished\n", game->GetName());
			game->finished_ = true;

			break;
		}

	case SBPKT_G2M_CloseGame:
		{
			const SBPKT_G2M_CloseGame_s& n = *(SBPKT_G2M_CloseGame_s*)PacketData;
			r3d_assert(sizeof(n) == PacketSize);

			TGamesList::iterator it = games_.find(peerId);
			r3d_assert(it != games_.end());
			CServerG* game = it->second;

			r3dOutToLog("game %s closed\n", game->GetName());
			DeleteGame(game);

			games_.erase(it);

			break;
		}

	case SBPKT_G2M_DataUpdateReq:
		{
			const SBPKT_G2M_DataUpdateReq_s& n = *(SBPKT_G2M_DataUpdateReq_s*)PacketData;
			r3d_assert(sizeof(n) == PacketSize);

			r3dOutToLog("got SBPKT_G2M_DataUpdateReq\n");
#if ENABLED_SERVER_WEAPONARMORY
			StartItemsDbUpdate(true);
#endif
			break;
		}

	}

	return;
}

void CMasterGameServer::DeleteGame(CServerG* game)
{
	CServerS* super = GetServerByGameId(game->id_);

	if(super) {
		super->DeregisterGame(game);
	} else {
		r3dOutToLog("master: game %s from UNKNOWN[%d] supervisor disconnected\n", game->GetName(), game->id_ >> 16);
	}

	delete game;
}

CServerS* CMasterGameServer::GetLeastUsedServer(EGBGameRegion region)
{
	CServerS* selected = NULL;
	int       minGames = 999;

	// search for server with maximum available users
	for(TSupersList::iterator it = supers_.begin(); it != supers_.end(); ++it) 
	{
		CServerS* super = it->second;

		// filter our supervisors if region is specified
		if(region != GBNET_REGION_Unknown && super->region_ != region)
			continue;

		if(super->GetExpectedGames() >= super->maxGames_)
			continue;

		if(super->GetExpectedGames() < minGames) {
			selected = super;
			minGames = super->GetExpectedGames();
		}


	}

	return selected;
}


CServerS* CMasterGameServer::GetServerByGameId(DWORD gameId)
{
	DWORD sid = gameId >> 16;

	for(TSupersList::iterator it = supers_.begin(); it != supers_.end(); ++it) {
		CServerS* super = it->second;
		if(sid == super->id_)
			return super;
	}

	return NULL;
}

CServerG* CMasterGameServer::GetGameByServerId(DWORD gameServerId)
{
	for(TGamesList::iterator it = games_.begin(); it != games_.end(); ++it) 
	{
		CServerG* game = it->second;
		if(gameServerId == game->getGameInfo().gameServerId)
			return game;
	}

	return NULL;
}

CServerG* CMasterGameServer::GetGameBySessionId(__int64 sessionId)
{
	for(TGamesList::iterator it = games_.begin(); it != games_.end(); ++it) 
	{
		CServerG* game = it->second;
		if(sessionId == game->info_.sessionId)
			return game;
	}

	return NULL;
}

CServerG* CMasterGameServer::GetQuickJoinGame(int gameMap, EGBGameRegion region)
{
#ifdef _DEBUG
	r3dOutToLog("GetQuickJoinGame: %d region:%d\n", gameMap, region); CLOG_INDENT;
#endif  

	// find most populated game
	int       foundMax  = -1;
	CServerG* foundGame = NULL;

	for(TGamesList::iterator it = games_.begin(); it != games_.end(); ++it) 
	{
		CServerG* game = it->second;
		if(!game->canJoinGame())
			continue;
		if(game->isPassworded())
			continue;

		const GBGameInfo& gi = game->getGameInfo();

		// filter out region
		if(region != GBNET_REGION_Unknown && gi.region != region) {
			continue;
		}

		// filter out specified map/mode (0xFF mean any game/mode)
		if(gameMap < 0xFF && gameMap != gi.mapId)
			continue;

		int numPlayers = game->curPlayers_ + game->GetJoiningPlayers();
		if(numPlayers >= gi.maxPlayers)
			continue;

		if(numPlayers > foundMax) {
			foundMax  = numPlayers;
			foundGame = game;
		}
	}

	CServerG* game = foundGame;
	if(game == NULL) {
#ifdef _DEBUG  
		r3dOutToLog("no free games\n");
#endif    
		return NULL;
	}

#ifdef _DEBUG  
	r3dOutToLog("%s, %d(+%d) of %d players\n", 
		game->GetName(), 
		game->curPlayers_, 
		game->GetJoiningPlayers(), 
		game->getGameInfo().maxPlayers);
#endif    

	return game;
}

bool CMasterGameServer::CreateNewGame(const CMSNewGameData& ngd, DWORD* out_ip, DWORD* out_port, __int64* out_sessionId)
{
	CServerS* super = GetLeastUsedServer((EGBGameRegion)ngd.ginfo.region);
	if(super == NULL)
	{
		r3dOutToLog("there is no free game servers at region:%d\n", ngd.ginfo.region);
		return false;
	}

	CREATE_PACKET(SBPKT_M2S_StartGameReq, n);
	if(super->RegisterNewGameSlot(ngd, n) == false)
	{
		r3dOutToLog("request for new game failed at %s\n", super->GetName());
		return false;
	}

#if 1
	r3dOutToLog("request for new game send to %s, creator:%d, players:%d, id:%x, port:%d\n", super->GetName(), ngd.CustomerID, ngd.ginfo.maxPlayers, n.gameId, n.port);
	net_->SendToPeer(&n, sizeof(n), super->peer_, true);
#else
	char strginfo[256];
	ginfo.ToString(strginfo);

	char cmd[512];
	sprintf(cmd, "\"%u %u %u\" \"%s\"", n.gameId, n.port, ngd.CustomerID, strginfo);
	const char* exe = "WZ_GameServer.exe";
	int err;
	if(err = (int)ShellExecute(NULL, "open", exe, cmd, "", SW_SHOW) < 32) {
		r3dOutToLog("!!! unable to run %s: %d\n", exe, err);
	}
#endif

	*out_ip        = super->ip_;
	*out_port      = n.port;
	*out_sessionId = n.sessionId;

	return true;
}

#if ENABLED_SERVER_WEAPONARMORY
DWORD __stdcall CMasterGameServer::ItemsDbUpdateThread(LPVOID in)
{
	CMasterGameServer* This = (CMasterGameServer*)in;

	MSWeaponArmory* wa   = NULL;
	CkHttpResponse* resp = NULL;

	try
	{
		r3dOutToLog("ItemsDBUpdateThread: started\n");

		CkHttp http;
		int success = http.UnlockComponent("ARKTOSHttp_decCLPWFQXmU");
		if(success != 1) 
			r3dError("Internal error");

		CkHttpRequest req;
		req.UsePost();
		req.put_Path("/conexao/api/php/api_getItemsDB.php");
		req.AddParam("serverkey", "CfFkqQWjfgksYG56893GDhjfjZ20");

		resp = http.SynchronousRequest(g_api_ip->GetString(), gDomainPort, gDomainUseSSL, req);
		if(!resp)
			throw "no response";

		wa = new MSWeaponArmory();
		char* data = (char*)resp->bodyStr();
		if(!wa->loadItemsDB(data, strlen(data))) 
			throw "failed to load itemsdb";

		r3dOutToLog("ItemsDBUpdateThread: updated, %d weapons\n", wa->m_NumWeaponsLoaded);

		delete resp;

		This->newWeaponArmory_   = wa;
		This->itemsDbUpdateFlag_ = ITEMSDBUPDATE_Ok;
		return This->itemsDbUpdateFlag_;
	}
	catch(const char* msg)
	{
		r3dOutToLog("!!!! ItemsDBUpdateThread failed: %s\n", msg);
	}

	SAFE_DELETE(wa);
	SAFE_DELETE(resp);

	This->itemsDbUpdateFlag_ = ITEMSDBUPDATE_Error;
	return This->itemsDbUpdateFlag_;
}

void CMasterGameServer::StartItemsDbUpdate(bool forced)
{
	if(itemsDbUpdateFlag_ != ITEMSDBUPDATE_Unactive) 
	{
		r3dOutToLog("items db update already in process\n");
		return;
	}

	itemsDbUpdateFlag_   = ITEMSDBUPDATE_Processing;
	itemsDbUpdateForced_ = forced;

	// create items update thread thread
	itemsDbUpdateThread_ = CreateThread(NULL, 0, &ItemsDbUpdateThread, this, 0, NULL);
	itemsDbLastUpdate_   = r3dGetTime();

	return;
}

void CMasterGameServer::DoFirstItemsDbUpdate()
{
	// minor hack: if we're running in local test mode, skip items updating
	extern int gDomainPort;
	if(gDomainPort == 55016 || stricmp(g_api_ip->GetString(), "localhost") == 0)
	{
		itemsDbUpdateFlag_ = ITEMSDBUPDATE_Processing; // put it in permanent wait state
		return;
	}

	r3dOutToLog("reading new items db\n");
	StartItemsDbUpdate(false);
	DWORD rr = ::WaitForSingleObject(itemsDbUpdateThread_, 30000);
	if(rr != WAIT_OBJECT_0) {
		r3dError("failed to download items db - timeout\n");
		return;
	}
	if(itemsDbUpdateFlag_ != ITEMSDBUPDATE_Ok) {
		r3dError("failed to download items db - error\n");
		return;
	}

	// swap current weapon armory with new one
	r3d_assert(gMSWeaponArmory == NULL);
	gMSWeaponArmory    = newWeaponArmory_;
	newWeaponArmory_   = NULL;
	itemsDbUpdateFlag_ = ITEMSDBUPDATE_Unactive;
	return;
}

void CMasterGameServer::UpdateItemsDb()
{
	if(itemsDbUpdateFlag_ == ITEMSDBUPDATE_Processing)
		return;

	if(itemsDbUpdateFlag_ == ITEMSDBUPDATE_Unactive)
	{
		// do update every 10 min
		if(r3dGetTime() > itemsDbLastUpdate_ + 600.0f)
		{
			r3dOutToLog("Starting periodic itemsdb update\n");
			StartItemsDbUpdate(false);
		}
		return;
	}

	if(itemsDbUpdateFlag_ == ITEMSDBUPDATE_Error)
	{
		r3dOutToLog("failed to get items db update\n");
		itemsDbUpdateFlag_ = ITEMSDBUPDATE_Unactive;
		return;
	}

	r3d_assert(itemsDbUpdateFlag_ == ITEMSDBUPDATE_Ok);
	r3dOutToLog("got new weapon info, sending to %d games\n", games_.size());

	// swap current weapon armory with new one
	SAFE_DELETE(gMSWeaponArmory);
	r3d_assert(gMSWeaponArmory == NULL);
	gMSWeaponArmory    = newWeaponArmory_;
	newWeaponArmory_   = NULL;
	itemsDbUpdateFlag_ = ITEMSDBUPDATE_Unactive;

	// send new data to games
	if(itemsDbUpdateForced_)
	{
		for(TGamesList::iterator it = games_.begin(); it != games_.end(); ++it) 
		{
			const CServerG* game = it->second;
			SendArmoryInfoToGame(game);
		}
	}

	return;
}

void CMasterGameServer::SendArmoryInfoToGame(const CServerG* game)
{
	if(gMSWeaponArmory == NULL) {
		r3dOutToLog("gMSWeaponArmory isn't loaded\n");
		return;
	}

	r3d_assert(gMSWeaponArmory);

	// send all weapons to game server
	for(uint32_t i=0; i<gMSWeaponArmory->m_NumWeaponsLoaded; i++)
	{
		const WeaponConfig& wc = *gMSWeaponArmory->m_WeaponArray[i];

		SBPKT_M2G_UpdateWeaponData_s n;
		n.itemId = wc.m_itemID;
		wc.copyParametersTo(n.wi);

		net_->SendToPeer(&n, sizeof(n), game->peer_, true);
	}

	// send all gears to game server
	for(uint32_t i=0; i<gMSWeaponArmory->m_NumGearLoaded; i++)
	{
		const GearConfig& gc = *gMSWeaponArmory->m_GearArray[i];

		SBPKT_M2G_UpdateGearData_s n;
		n.itemId = gc.m_itemID;
		gc.copyParametersTo(n.gi);

		net_->SendToPeer(&n, sizeof(n), game->peer_, true);
	}

	// send lootboxes to game server
	for(uint32_t i=0; i<gMSWeaponArmory->m_NumItemLoaded; i++)
	{
		const ItemConfig& ic = *gMSWeaponArmory->m_ItemArray[i];
		if(ic.category != storecat_LootBox)
			continue;

		SBPKT_M2G_UpdateItemData_s n;
		n.itemId = ic.m_itemID;
		n.LevelRequired = ic.m_LevelRequired;

		net_->SendToPeer(&n, sizeof(n), game->peer_, true);
	}

	// send end update event
	SBPKT_M2G_UpdateDataEnd_s n;
	net_->SendToPeer(&n, sizeof(n), game->peer_, true);

	return;
}
#endif