#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"
#include <shellapi.h>

#pragma warning(disable: 4065)	// switch statement contains 'default' but no 'case' labels

#include "MasterUserServer.h"
#include "MasterGameServer.h"

using namespace NetPacketsGameBrowser;

static	r3dNetwork	clientNet;
CMasterUserServer gMasterUserServer;

static bool IsNullTerminated(const char* data, int size)
{
	for(int i=0; i<size; i++) {
		if(data[i] == 0)
			return true;
	}

	return false;
}

CMasterUserServer::CMasterUserServer()
{
	return;
}

CMasterUserServer::~CMasterUserServer()
{
	SAFE_DELETE_ARRAY(peers_);
}

void CMasterUserServer::Start(int port, int in_maxPeerCount)
{
	maxPeek = 0;
	r3d_assert(in_maxPeerCount);
	MAX_PEERS_COUNT = in_maxPeerCount;
	peers_          = new peer_s[MAX_PEERS_COUNT];

	numConnectedPeers_ = 0;
	maxConnectedPeers_ = 0;
	curPeerUniqueId_   = 0;

	clientNet.Initialize(this, "clientNet");
	if(!clientNet.CreateHost(port, MAX_PEERS_COUNT)) {
		r3dError("CreateHost failed\n");
	}

	r3dOutToLog("MasterUserServer started at port %d, %d CCU\n", port, MAX_PEERS_COUNT);

	return;
}

static bool SupervisorsSortByName(const CServerS* d1, const CServerS* d2)
{
	return strcmp(d1->GetName(), d2->GetName()) < 0;
}

void CMasterUserServer::PrintStats()
{
	const float curTime = r3dGetTime();

	static float nextShutLog_ = 0;
	if(gMasterGameServer.shuttingDown_ && curTime > nextShutLog_) {
		nextShutLog_ = curTime + 1.0f;
		r3dOutToLog("SHUTDOWN in %.0f\n", gMasterGameServer.shutdownLeft_);
	}

	// dump some useful statistics
	static float nextDebugLog_ = 0;
	if(curTime < nextDebugLog_) 
		return;

	nextDebugLog_ = curTime + 1.0f;

	int numGames = 0;
	int numCCU   = 0;
	int maxCCU   = 0;
	for(CMasterGameServer::TGamesList::const_iterator it = gMasterGameServer.games_.begin(); 
		it != gMasterGameServer.games_.end();
		++it)
	{
		const CServerG* game = it->second;

		numCCU += game->curPlayers_;
		maxCCU += game->getGameInfo().maxPlayers;
		numGames++;
	}

	static int peakCCU = 0;
	if(numCCU > peakCCU) peakCCU = numCCU;

	FILE* f = fopen("MasterServer_ccu.txt", "wt");

	char buf[1024];    
	size_t numSupervisors = gMasterGameServer.supers_.size();
	/* sprintf(buf, "MSINFO: %d (%d max) peers, %d CCU in %d games, PeakCCU: %d, MaxCCU:%d, Supervisors: %d\n",
	numConnectedPeers_,
	maxConnectedPeers_,
	numCCU,
	numGames,
	peakCCU,
	maxCCU,
	numSupervisors); 

	r3dOutToLog(buf); CLOG_INDENT;
	if(f) fprintf(f, buf);*/

	static std::vector<const CServerS*> supers;
	supers.clear();
	for(CMasterGameServer::TSupersList::const_iterator it = gMasterGameServer.supers_.begin(); it != gMasterGameServer.supers_.end(); ++it)
		supers.push_back(it->second);

	std::sort(supers.begin(), supers.end(), SupervisorsSortByName);

	// log supervisors status
	int totalplayer = 0;
	int totalgames = 0;
	int maxplayer = 0;
	int maxgames = 0;
	int peakPlr = 0;
	int peakPlrSId = 0;
	for(size_t i=0; i<supers.size(); ++i)
	{
		const CServerS* super = supers[i];
		sprintf(buf, "%s(%s), games:%d/%d, players:%d/%d\n", 
			super->GetName(),
			inet_ntoa(*(in_addr*)&super->ip_),
			super->GetExpectedGames(), super->maxGames_,
			super->GetExpectedPlayers(), super->maxPlayers_);

		r3dOutToLog(buf); CLOG_INDENT;
		if(f) fprintf(f, buf);

		totalplayer += super->GetExpectedPlayers();
		totalgames  += super->GetExpectedGames();
		maxgames += super->maxGames_;
		maxplayer += super->maxPlayers_;

		/*if (peakPlr < super->games_->game->curPlayers_)
		{
		peakPlr = super->games_->game->curPlayers_;
		peakPlrSId = super->games_->info.ginfo.gameServerId;
		}*/
	}

	r3dOutToLog("Total super:%d game:%d/%d player:%d/%d peer:%d (max %d) \n",supers.size(),totalgames,maxgames,totalplayer,maxplayer,numConnectedPeers_,gServerConfig->masterCCU_);

	for(CMasterGameServer::TGamesList::const_iterator it = gMasterGameServer.games_.begin(); 
		it != gMasterGameServer.games_.end();
		++it)
	{
		const CServerG* game = it->second;

		if (game && peakPlr < game->curPlayers_)
		{
			peakPlr = game->curPlayers_;
			peakPlrSId = game->getGameInfo().gameServerId;
		}
	}
	char name[64] = {0};
	int max = 0;
	for(CMasterGameServer::TGamesList::const_iterator it = gMasterGameServer.games_.begin(); 
		it != gMasterGameServer.games_.end();
		++it)
	{
		const CServerG* game = it->second;

		if (game->getGameInfo().gameServerId == peakPlrSId && peakPlrSId != 0)
		{
			sprintf(name,game->getGameInfo().name);
			max = game->getGameInfo().maxPlayers;
		}
	}

	if (totalplayer > maxPeek) maxPeek = totalplayer;

	char text[256] = {0};
	sprintf(text,"WO::Master Players:%d Peak:%d (id:%d %s [%d/%d])",totalplayer,maxPeek,peakPlrSId,name,peakPlr,max);
	SetConsoleTitle(text);

	if(f) fclose(f);

	return;
}

void CMasterUserServer::Temp_Debug1()
{
	r3dOutToLog("Temp_Debug1\n");

	int numGames = 0;
	int numCCU   = 0;
	for(CMasterGameServer::TGamesList::const_iterator it = gMasterGameServer.games_.begin(); 
		it != gMasterGameServer.games_.end();
		++it)
	{
		const CServerG* game = it->second;
		r3dOutToLog("game %s, %d players, %d joiners\n", game->GetName(), game->curPlayers_, game->GetJoiningPlayers());

		numCCU += game->curPlayers_;
		numGames++;
	}

	r3dOutToLog("MSINFO: %d (%d max) peers, %d CCU in %d games\n",
		numConnectedPeers_,
		maxConnectedPeers_,
		numCCU,
		numGames);

	// log supervisors status
	for(CMasterGameServer::TSupersList::const_iterator it = gMasterGameServer.supers_.begin();
		it != gMasterGameServer.supers_.end();
		++it)
	{
		const CServerS* super = it->second;
		r3dOutToLog("%s, games:%d/%d, players:%d/%d\n", 
			super->GetName(),
			super->GetExpectedGames(), super->maxGames_,
			super->GetExpectedPlayers(), super->maxPlayers_);
		CLOG_INDENT;

		int users = 0;
		for(int i=0; i<super->maxGames_; i++) 
		{
			const CServerG* game = super->games_[i].game;
			if(!game) 
				continue;

			r3dOutToLog("game %s, %d players, %d joiners\n", game->GetName(), game->curPlayers_, game->GetJoiningPlayers());
		}

	}

	return;
}

void CMasterUserServer::Tick()
{
	const float curTime = r3dGetTime();

	net_->Update();

	DisconnectIdlePeers();

	PrintStats();

	return;
}

void CMasterUserServer::Stop()
{
	if(net_)
		net_->Deinitialize();
}

void CMasterUserServer::DisconnectIdlePeers()
{
	const float IDLE_PEERS_CHECK = 0.2f;	// do checks every N seconds
	const float VALIDATE_DELAY   = 5.0f;	// peer have this N seconds to validate itself

	static float nextCheck = -1;
	const float curTime = r3dGetTime();
	if(curTime < nextCheck)
		return;
	nextCheck = curTime + IDLE_PEERS_CHECK;

	for(int i=0; i<MAX_PEERS_COUNT; i++) 
	{
		if(peers_[i].status == PEER_Connected && curTime > peers_[i].connectTime + VALIDATE_DELAY) {
			DisconnectCheatPeer(i, "validation time expired");
			continue;
		}
	}

	return;
}

void CMasterUserServer::DisconnectCheatPeer(DWORD peerId, const char* message, ...)
{
	char buf[2048] = {0};

	if(message)
	{
		va_list ap;
		va_start(ap, message);
		StringCbVPrintfA(buf, sizeof(buf), message, ap);
		va_end(ap);
	}

	DWORD ip = net_->GetPeerIp(peerId);
	if(message)
	{

		r3dOutToLog("!!! cheat: peer%d[%s], reason: %s\n", 
			peerId, 
			inet_ntoa(*(in_addr*)&ip),
			buf);
	}

	net_->DisconnectPeer(peerId);

	// fire up disconnect event manually, enet might skip if if other peer disconnect as well
	OnNetPeerDisconnected(peerId);
}

bool CMasterUserServer::DisconnectIfShutdown(DWORD peerId)
{
	if(!gMasterGameServer.shuttingDown_)
		return false;

	GBPKT_M2C_ShutdownNote_s n;
	n.reason = 0;
	net_->SendToPeer(&n, sizeof(n), peerId);

	DisconnectCheatPeer(peerId, NULL);
	return true;
}

bool CMasterUserServer::Validate(const GBPKT_C2M_JoinGameReq_s& n)
{
	if(!IsNullTerminated(n.pwd, sizeof(n.pwd)))
		return false;

	return true;    
}

void CMasterUserServer::OnNetPeerConnected(DWORD peerId)
{
	peer_s& peer = peers_[peerId];
	r3d_assert(peer.status == PEER_Free);

	curPeerUniqueId_++;
	peer.peerUniqueId = (peerId << 16) | (curPeerUniqueId_ & 0xFFFF);
	peer.status       = PEER_Connected;
	peer.connectTime  = r3dGetTime();
	peer.lastReqTime  = r3dGetTime() - 1.0f; // minor hack to avoid check for 'too many requests'
	peer.LastRentTime = 0;

	// send validate packet, so client can check version right now
	GBPKT_ValidateConnectingPeer_s n;
	n.version = GBNET_VERSION;
	n.key1    = 0;
	net_->SendToPeer(&n, sizeof(n), peerId, true);

	numConnectedPeers_++;
	maxConnectedPeers_ = R3D_MAX(maxConnectedPeers_, numConnectedPeers_);

	return;
}

void CMasterUserServer::OnNetPeerDisconnected(DWORD peerId)
{
	peer_s& peer = peers_[peerId];

	if(peer.status != PEER_Free)
		numConnectedPeers_--;

	//r3dOutToLog("master: client disconnected\n");
	peer.status       = PEER_Free;
	peer.peerUniqueId = 0;
	return;
}

bool CMasterUserServer::DoValidatePeer(DWORD peerId, const r3dNetPacketHeader* PacketData, int PacketSize)
{
	peer_s& peer = peers_[peerId];

	if(peer.status >= PEER_Validated)
		return true;

	// we still can receive packets after peer was force disconnected
	if(peer.status == PEER_Free)
		return false;

	r3d_assert(peer.status == PEER_Connected);
	if(PacketData->EventID != GBPKT_ValidateConnectingPeer) {
		DisconnectCheatPeer(peerId, "wrong validate packet id");
		return false;
	}

	const GBPKT_ValidateConnectingPeer_s& n = *(GBPKT_ValidateConnectingPeer_s*)PacketData;
	if(sizeof(n) != PacketSize) {
		DisconnectCheatPeer(peerId, "wrong validate packet size");
		return false;
	}
	if(n.version != GBNET_VERSION) {
		DisconnectCheatPeer(peerId, "wrong validate version");
		return false;
	}
	if(n.key1 != GBNET_KEY1) {
		DisconnectCheatPeer(peerId, "wrong validate key");
		return false;
	}

	// send server info to client
	GBPKT_M2C_ServerInfo_s n1;
	n1.serverId = BYTE(gMasterGameServer.masterServerId_);
	net_->SendToPeer(&n1, sizeof(n1), peerId, true);

	// set peer to validated
	peer.status = PEER_Validated;
	return false;
}

#define DEFINE_PACKET_HANDLER_MUS(xxx) \
	case xxx: \
{ \
	const xxx##_s& n = *(xxx##_s*)PacketData; \
	if(sizeof(n) != PacketSize) { \
	DisconnectCheatPeer(peerId, "wrong %s size %d vs %d", #xxx, sizeof(n), PacketSize); \
	break; \
	} \
	if(!Validate(n)) { \
	DisconnectCheatPeer(peerId, "invalid %s", #xxx); \
	break; \
	} \
	On##xxx(peerId, n); \
	break; \
}

void CMasterUserServer::OnNetData(DWORD peerId, const r3dNetPacketHeader* PacketData, int PacketSize)
{
	if(PacketSize < sizeof(r3dNetPacketHeader)) {
		DisconnectCheatPeer(peerId, "too small packet");
		return;
	}

	// wait for validating packet
	if(!DoValidatePeer(peerId, PacketData, PacketSize))
		return;

	if(DisconnectIfShutdown(peerId))
		return;

	peer_s& peer = peers_[peerId];
	r3d_assert(peer.status >= PEER_Validated);

	const float curTime = r3dGetTime();

	// check for valid request overflows
	if(curTime < peer.lastReqTime + 0.2f) {
		DisconnectCheatPeer(peerId, "too many requests per sec");
		return;
	}
	peer.lastReqTime = curTime;

	switch(PacketData->EventID) 
	{
	default:
		DisconnectCheatPeer(peerId, "invalid packet id");
		return;

		DEFINE_PACKET_HANDLER_MUS(GBPKT_C2M_RefreshList)
			DEFINE_PACKET_HANDLER_MUS(GBPKT_C2M_JoinGameReq);
		DEFINE_PACKET_HANDLER_MUS(GBPKT_C2M_PlayerListReq);
		DEFINE_PACKET_HANDLER_MUS(GBPKT_C2M_QuickGameReq);
		DEFINE_PACKET_HANDLER_MUS(GBPKT_C2M_RentGameReq);
		DEFINE_PACKET_HANDLER_MUS(GBPKT_C2M_RentGameChangeSet);
		DEFINE_PACKET_HANDLER_MUS(GBPKT_C2S_RentGameKick);
		DEFINE_PACKET_HANDLER_MUS(GBPKT_C2M_RentGameStr);
		DEFINE_PACKET_HANDLER_MUS(GBPKT_C2M_BanPlayer);
	}

	return;
}
void CMasterUserServer::OnGBPKT_C2M_BanPlayer(DWORD peerId, const GBPKT_C2M_BanPlayer_s& n)
{
	r3dOutToLog("BanPlayer Request customerid %d gamertag %s\n",n.customerid,n.name);
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
			gMasterGameServer.SendBanPlayer(game->peer_,n.customerid,(const char*)n.name);
			// no need to send notice at now. notice will send by server.
			r3dOutToLog("BanPlayer Request customerid %d gamertag %s send to server%d\n",n.customerid,n.name,game->info_.ginfo.gameServerId);
			return;
		}
	}
}
void CMasterUserServer::OnGBPKT_C2S_RentGameKick(DWORD peerId, const GBPKT_C2S_RentGameKick_s& n)
{
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
			if (game->info_.ginfo.gameServerId == n.serverid)
			{
				gMasterGameServer.SendPlayerKick(n.name,game->peer_);
				return;
			}
		}
	}
}
void CMasterUserServer::OnGBPKT_C2M_RentGameChangeSet(DWORD peerId, const GBPKT_C2M_RentGameChangeSet_s& n)
{
	r3dOutToLog("GBPKT_C2M_RentGameChangeSet %d , %s\n",n.gameserverid,n.pwd);
	gServerConfig->SetRentPwd(n.gameserverid,n.pwd);
}
void CMasterUserServer::OnGBPKT_C2M_PlayerListReq(DWORD peerId, const GBPKT_C2M_PlayerListReq_s& n)
{
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
			if (game->info_.ginfo.gameServerId == n.serverId)
			{
				gMasterGameServer.SendPlayerListReq(peerId,game->peer_);
				return;
			}
		}
	}
}
void CMasterUserServer::OnGBPKT_C2M_RentGameStr(DWORD peerId, const GBPKT_C2M_RentGameStr_s& n)
{
	// not check for over max rent games. add rent server at now!
	r3dOutToLog("StartRentGames Packet Received.\n");
	gServerConfig->AddRentGame(n.ownercustomerid,n.name,n.pwd,n.mapid,n.slot,n.period);
}
void CMasterUserServer::OnGBPKT_C2M_RentGameReq(DWORD peerId, const GBPKT_C2M_RentGameReq_s& n)
{
	r3dOutToLog("RentGameReq Packet Received.\n");
	int gamecount = 0;
	bool havesupers = false;

	const CServerS* superv = NULL;
	for(CMasterGameServer::TSupersList::const_iterator it = gMasterGameServer.supers_.begin();
		it != gMasterGameServer.supers_.end();
		++it)
	{
		const CServerS* super = it->second;
		if (super->region_ == GBNET_REGION_Europe)
		{
			//havesupers = true;
			//gamecount = super->GetExpectedGames();
			superv = super;
		}

		for (int i=0 ; i<super->maxGames_;i++)
		{
			const CServerG* game = super->games_[i].game;
			if (!game)
				continue;

			if (game->info_.ginfo.ispass)
				gamecount++;
		}

		/*if (super->region_ == GBNET_REGION_Europe2)
		{
		for (int i=0 ; i<superv->maxGames_;i++)
		{
		const CServerG* game = super->games_[i].game;
		if (game)
		continue;

		if (game->info_.ginfo.ispass)
		gamecount++;
		}
		}*/
	}

	if (superv)
		havesupers = true;
	else
		havesupers = false;


	CREATE_PACKET(GBPKT_C2M_RentGameAns,n1);
	r3dOutToLog("Gamecount %d\n",gamecount);
	if (gamecount >= gServerConfig->maxrent && havesupers && gServerConfig->maxrent != 0) // Games Is FULL
	{
		r3dOutToLog("Rent Server is FULL!\n");
		n1.anscode = 1;
		// send full message to peer. not forgot send free times.
		// NOT FINISHED.
		/*int expiredata[100];
		int lowestnum = 0;
		int numhavedata = 0;
		for (i=0 , i<superv->maxGames_,i++)
		{
		const CServerG* game = super->games_[i].game;
		if (game)
		continue;
		expiredata[numhavedata++] = game->info_.ginfo.expirein;
		}*/
		sprintf(n1.msg,"Rent Server is full. (%d/%d)",gamecount,gServerConfig->maxrent);
	}
	else if (gServerConfig->maxrent == 0) // rent games not used.
	{
		n1.anscode = 3;
		sprintf(n1.msg,"Rent Server not available. (%d/%d)",gamecount,gServerConfig->maxrent);
	}
	else if (havesupers) // this is ready to add rent server , not send any message.
	{
		r3dOutToLog("Rent Server is free slot.\n");
		n1.anscode = 0;
		//gServerConfig->AddRentGame(n.ownercustomerid,n.name,n.pwd,n.mapid,n.slot,n.period);
	}
	else // undefind problem.
	{
		r3dOutToLog("Rent Server is not found or have problem.\n");
		n1.anscode = 2;
		sprintf(n1.msg,"Rent Server Request Failed.");
	}

	net_->SendToPeer(&n1, sizeof(n1), peerId);
}
void CMasterUserServer::SendPlayerListEnd(DWORD peerId)
{
	CREATE_PACKET(GBPKT_C2M_PlayerListEnd, n);
	net_->SendToPeer(&n, sizeof(n), peerId);
}
void CMasterUserServer::SendPlayerList(DWORD peerId , int alive , const char* name , int rep , int xp)
{
	CREATE_PACKET(GBPKT_C2M_PlayerListAns, n);
	n.alive = alive;
	r3dscpy(n.gamertag,name);
	n.rep = rep;
	n.xp = xp;
	net_->SendToPeer(&n, sizeof(n), peerId);
}
void CMasterUserServer::OnGBPKT_C2M_RefreshList(DWORD peerId, const GBPKT_C2M_RefreshList_s& n)
{
	// r3dOutToLog("sending session list to client%d\n", peerId);

	{ // start list
		CREATE_PACKET(GBPKT_M2C_StartGamesList, n);
		net_->SendToPeer(&n, sizeof(n), peerId);
	}

	// send supervisors data
	for(CMasterGameServer::TSupersList::iterator it = gMasterGameServer.supers_.begin(); it != gMasterGameServer.supers_.end(); ++it)
	{
		const CServerS* super = it->second;

		GBPKT_M2C_SupervisorData_s n;
		n.ID     = WORD(super->id_);
		n.ip     = super->ip_;
		n.region = super->region_;
		net_->SendToPeer(&n, sizeof(n), peerId);
	}

	// send games

	for(CMasterGameServer::TGamesList::iterator it = gMasterGameServer.games_.begin(); it != gMasterGameServer.games_.end(); ++it) 
	{
		const CServerG* game = it->second;
		if(game->isValid() == false)
			continue;

		if(game->isFinished())
			continue;

		BYTE gameStatus = 0;
		gameStatus |= game->isFull()?2:0;
		gameStatus |= game->isPassworded()?8:0;

		CREATE_PACKET(GBPKT_M2C_GameData, n);
		n.superId    = (game->id_ >> 16);
		n.info       = game->info_.ginfo;
		if (game->info_.ginfo.pwdchar[0])
			sprintf(n.info.pwdchar,"0");

		n.status     = gameStatus;
		n.curPlayers = (BYTE)game->curPlayers_;

		net_->SendToPeer(&n, sizeof(n), peerId);
	}

	{ // end list
		CREATE_PACKET(GBPKT_M2C_EndGamesList, n);
		net_->SendToPeer(&n, sizeof(n), peerId);
	}


	return;
}

void CMasterUserServer::OnGBPKT_C2M_JoinGameReq(DWORD peerId, const GBPKT_C2M_JoinGameReq_s& n)
{
	GBPKT_M2C_JoinGameAns_s ans;
	do 
	{
		CServerG* game = gMasterGameServer.GetGameByServerId(n.gameServerId);
		if(!game) {
			ans.result = GBPKT_M2C_JoinGameAns_s::rGameNotFound;
			break;
		}

		DoJoinGame(game, n.CustomerID, n.pwd, ans);
		break;
	} while (0);

	r3d_assert(ans.result != GBPKT_M2C_JoinGameAns_s::rUnknown);
	net_->SendToPeer(&ans, sizeof(ans), peerId, true);
	return;
}

void CMasterUserServer::OnGBPKT_C2M_QuickGameReq(DWORD peerId, const GBPKT_C2M_QuickGameReq_s& n)
{
	GBPKT_M2C_JoinGameAns_s ans;

	CServerG* game = gMasterGameServer.GetQuickJoinGame(n.gameMap, (EGBGameRegion)n.region);
	// in case some region game wasn't available, repeat search without specifying filter
	if(game == NULL && n.region != GBNET_REGION_Unknown)
	{
		CLOG_INDENT;
		game = gMasterGameServer.GetQuickJoinGame(n.gameMap, GBNET_REGION_Unknown);
	}

	if(!game) {
		ans.result = GBPKT_M2C_JoinGameAns_s::rGameNotFound;
		net_->SendToPeer(&ans, sizeof(ans), peerId, true);
		return;
	}

	game->AddJoiningPlayer(n.CustomerID);

	ans.result    = GBPKT_M2C_JoinGameAns_s::rOk;
	ans.ip        = game->ip_;
	ans.port      = game->info_.port;
	ans.sessionId = game->info_.sessionId;
	net_->SendToPeer(&ans, sizeof(ans), peerId, true);
	return;
}

void CMasterUserServer::DoJoinGame(CServerG* game, DWORD CustomerID, const char* pwd, GBPKT_M2C_JoinGameAns_s& ans)
{
	r3d_assert(game);

	if(game->isFull()) {
		ans.result = GBPKT_M2C_JoinGameAns_s::rGameFull;
		return;
	}
	if(game->isFinished()) {
		ans.result = GBPKT_M2C_JoinGameAns_s::rGameFinished;
		return;
	}

	// bool isAdmin = false;
	// if(CustomerID == 1281646178 || // sagor
	// CustomerID == 1288125909 || // sousuke
	// CustomerID == 1288144549 || // cvance
	// CustomerID == 1288629751 || // wertyuiop
	// CustomerID == 1288188971 || // piki
	// CustomerID == 1288686686  // kewk
	// ) 
	// {
	// // do not check password for GM, we allow GMs to enter any game
	// isAdmin = true;
	// }

	if(game->isPassworded()) {
		if (pwd[0] == 0) 
		{
			ans.result = GBPKT_M2C_JoinGameAns_s::rInputPassword;
			return;
		}
		if(strcmp(game->info_.ginfo.pwdchar, pwd) != 0) {
			ans.result = GBPKT_M2C_JoinGameAns_s::rWrongPassword;
			return;
		}
	}

	game->AddJoiningPlayer(CustomerID);

	ans.result    = GBPKT_M2C_JoinGameAns_s::rOk;
	ans.ip        = game->ip_;
	ans.port      = game->info_.port;
	ans.sessionId = game->info_.sessionId;
	return;
}

void CMasterUserServer::CreateNewGame(const CMSNewGameData& ngd, GBPKT_M2C_JoinGameAns_s& ans)
{
	// request new game from master server
	DWORD ip, port;
	__int64 sessionId;
	if(gMasterGameServer.CreateNewGame(ngd, &ip, &port, &sessionId) == false)
	{
		ans.result = GBPKT_M2C_JoinGameAns_s::rNoGames;
		return;
	}

	// fill answer results
	ans.result    = GBPKT_M2C_JoinGameAns_s::rOk;
	ans.ip        = ip;
	ans.port      = (WORD)port;
	ans.sessionId = sessionId;
	return;
}
