#include "r3dPCH.h"
#include "r3d.h"

#include "MasterServerLogic.h"

MasterServerLogic gMasterServerLogic;
#include "ObjectsCode/obj_ServerPlayer.h"
#include "../../MasterServer/Sources/NetPacketsServerBrowser.h"
#include "ServerGameLogic.h"
#include "AsyncFuncs.h"
using namespace NetPacketsServerBrowser;

#include "ObjectsCode/weapons/WeaponArmory.h"

MasterServerLogic::MasterServerLogic()
{
	disconnected_    = false;
	gotWeaponUpdate_ = false;
	shuttingDown_    = false;
}

MasterServerLogic::~MasterServerLogic()
{
}

void MasterServerLogic::OnNetPeerConnected(DWORD peerId)
{
	return;
}

void MasterServerLogic::OnNetPeerDisconnected(DWORD peerId)
{
	r3dOutToLog("!!! master server disconnected\n");
	disconnected_ = true;
	return;
}

void MasterServerLogic::OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize)
{
	switch(packetData->EventID) 
	{
	default:
		r3dError("MasterServerLogic: unknown packetId %d", packetData->EventID);
		return;

	case SBPKT_ValidateConnectingPeer:
		{
			const SBPKT_ValidateConnectingPeer_s& n = *(SBPKT_ValidateConnectingPeer_s*)packetData;
			if(n.version != SBNET_VERSION) {
				r3dError("master server version is different (%d vs %d)", n.version, SBNET_VERSION);
				break;
			}
			break;
		}

	case SBPKT_M2G_KillGame:
		{
			r3dOutToLog("Master server requested game kill\n");
			net_->DisconnectPeer(peerId);
			break;
		}

	case SBPKT_M2G_ShutdownNote:
		{
			const SBPKT_M2G_ShutdownNote_s& n = *(SBPKT_M2G_ShutdownNote_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);

			if (shuttingDown_) return;

			r3dOutToLog("---- Master server is shutting down now\n");
			shuttingDown_ = true;
			shutdownLeft_ = n.timeLeft;
			break;
		}

	case SBPKT_M2G_UpdateWeaponData:
		{
			const SBPKT_M2G_UpdateWeaponData_s& n = *(SBPKT_M2G_UpdateWeaponData_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);

			/*
			WeaponConfig* wc = const_cast<WeaponConfig*>(g_pWeaponArmory->getWeaponConfig(n.itemId));
			if(wc == NULL) {
			r3dOutToLog("!!! got update for not existing weapon %d\n", n.itemId);
			return;
			}

			wc->copyParametersFrom(n.wi);
			//r3dOutToLog("got update for weapon %s\n", wc->m_StoreName);
			*/
			break;
		}
	case SBPKT_G2M_SendNoticeMsg:
		{
			const SBPKT_G2M_SendNoticeMsg_s& n = *(SBPKT_G2M_SendNoticeMsg_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);
			r3dOutToLog("Received notice packet from master. msg '%s' , brostcast to all players.\n",n.msg);
			// brostcast by chat packet.
			PKT_C2C_ChatMessage_s n1;
			r3dscpy(n1.gamertag, "<SYSTEM>");
			r3dscpy(n1.msg, n.msg);
			n1.msgChannel = 1;
			n1.userFlag = 2;
			gServerLogic.p2pBroadcastToAll(NULL, &n1, sizeof(n1), true);
			break;
		}
		case SBPKT_M2G_BanPlayer:
		{
			const SBPKT_M2G_BanPlayer_s& n = *(SBPKT_M2G_BanPlayer_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);
			r3dOutToLog("BanPlayer Request from master customerid%d name%s\n",n.customerid,n.gamertag);
            g_AsyncApiMgr->AddJob(new CJobBanID(n.customerid,"WallHack"));
			SendNoticeMsg("FairPlay Banned '%s'",n.gamertag); // fairfuck
			break;
		}
	case SBPKT_M2G_SendPlayerKick:
		{
			const SBPKT_M2G_SendPlayerKick_s& n = *(SBPKT_M2G_SendPlayerKick_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);
			char find[128];
			r3dscpy(find, n.name);
			obj_ServerPlayer* tplr = gServerLogic.FindPlayer(find); 
			if (tplr)
			{
				// Send Message before kicked client
				char message[128] = {0};
				PKT_S2C_CheatMsg_s n2;
				sprintf(message, "Kicked by administrator");
				r3dscpy(n2.cheatreason,message);
				gServerLogic.p2pSendToPeer(tplr->peerId_, tplr, &n2, sizeof(n2));
				// After send message kicked client now!
				gServerLogic.DisconnectPeer(tplr->peerId_, false, "Kick by admin");
			}
			break;
		}
	case SBPKT_M2G_SendPlayerReq:
		{
			const SBPKT_M2G_SendPlayerReq_s& n = *(SBPKT_M2G_SendPlayerReq_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);
			for(int i=0; i<ServerGameLogic::MAX_NUM_PLAYERS; i++)
			{
				obj_ServerPlayer* tplr = gServerLogic.GetPlayer(i);
				if(!tplr) continue;			
				CREATE_PACKET(SBPKT_M2G_SendPlayerList, n1);
				n1.ClientPeerId = n.ClientPeerId; // !!!!!! IMPORTANT VALUE FOR SEND TO CLIENT !!!!!!!!!!
				n1.alive = tplr->loadout_->Alive;
				sprintf(n1.gamertag,tplr->loadout_->Gamertag);
				n1.rep = tplr->loadout_->Stats.Reputation;
				n1.xp = tplr->loadout_->Stats.XP;
				net_->SendToHost(&n1, sizeof(n1), true);
			}
			// AFTER SEND LIST COMPLETE SEND END PACKET TO CLIENT AND COMPLETE FUNCTION
			CREATE_PACKET(SBPKT_M2G_SendPlayerListEnd,n2);
			n2.ClientPeerId = n.ClientPeerId; // !!!!!! IMPORTANT VALUE FOR SEND TO CLIENT !!!!!!!!!!
			net_->SendToHost(&n2, sizeof(n2), true);
			// !!! COMPLETE FUNCTION !!!
			break;
		}

	case SBPKT_M2G_UpdateGearData:
		{
			const SBPKT_M2G_UpdateGearData_s& n = *(SBPKT_M2G_UpdateGearData_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);

			/*
			GearConfig* gc = const_cast<GearConfig*>(g_pWeaponArmory->getGearConfig(n.itemId));
			if(gc == NULL) {
			r3dOutToLog("!!! got update for not existing gear %d\n", n.itemId);
			return;
			}

			gc->copyParametersFrom(n.gi);
			//r3dOutToLog("got update for gear %s\n", gc->m_StoreName);
			*/
			break;
		}

	case SBPKT_M2G_UpdateItemData:
		{
			const SBPKT_M2G_UpdateItemData_s& n = *(SBPKT_M2G_UpdateItemData_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);

			/*
			ModelItemConfig* ic = const_cast<ModelItemConfig*>(g_pWeaponArmory->getItemConfig(n.itemId));
			if(ic == NULL) {
			r3dOutToLog("!!! got update for not existing item %d\n", n.itemId);
			return;
			}

			r3dOutToLog("got update for item %s\n", ic->m_StoreName);
			*/
			break;
		}

	case SBPKT_M2G_UpdateDataEnd:
		{
			const SBPKT_M2G_UpdateDataEnd_s& n = *(SBPKT_M2G_UpdateDataEnd_s*)packetData;
			r3d_assert(sizeof(n) == packetSize);
			r3dOutToLog("got SBPKT_M2G_UpdateDataEnd\n");
			gotWeaponUpdate_ = true;

			break;
		}
	}

	return;
}

int MasterServerLogic::WaitFunc(fn_wait fn, float timeout, const char* msg)
{
	const float endWait = r3dGetTime() + timeout;

	r3dOutToLog("waiting: %s, %.1f sec left\n", msg, endWait - r3dGetTime());

	while(1) 
	{
		r3dEndFrame();
		r3dStartFrame();

		net_->Update();

		if((this->*fn)())
			break;

		if(r3dGetTime() > endWait) {
			return 0;
		}
	}

	return 1;
}

void MasterServerLogic::Init(DWORD gameId)
{
	gameId_ = gameId;
}

int MasterServerLogic::Connect(const char* host, int port)
{
	r3dOutToLog("Connecting to master server at %s:%d\n", host, port); CLOG_INDENT;

	g_net.Initialize(this, "master server");
	g_net.CreateClient();
	g_net.Connect(host, port);

	if(!WaitFunc(&MasterServerLogic::wait_IsConnected, 10, "connecting"))
		throw "can't connect to master server";

	// send validation packet immediately
	CREATE_PACKET(SBPKT_ValidateConnectingPeer, n);
	n.version = SBNET_VERSION;
	n.key1    = SBNET_KEY1;
	net_->SendToHost(&n, sizeof(n), true);

	return 1;
}

void MasterServerLogic::Disconnect()
{
	g_net.Deinitialize();
}

void MasterServerLogic::Tick()
{
	if(net_ == NULL)
		return;

	net_->Update();

	if(shuttingDown_)
		shutdownLeft_ -= r3dGetFrameTime();
}

void MasterServerLogic::RegisterGame()
{
	if(net_ == NULL)
		return;

	CREATE_PACKET(SBPKT_G2M_RegisterGame, n);
	n.gameId = gameId_;
	net_->SendToHost(&n, sizeof(n), true);

	return;
}
void MasterServerLogic::SendScreenShot(const char* fname, const char* data , int size)
{
	int   pktsize = sizeof(SBPKT_G2M_ScreenShotData_s) + size;
	char* pktdata = new char[pktsize + 1];
	CREATE_PACKET(SBPKT_G2M_ScreenShotData,n);
	sprintf(n.fname,fname);
    //n.data = data;
	n.size = size;
	memcpy(pktdata, &n, sizeof(n));
	memcpy(pktdata + sizeof(n), data, size);
	net_->SendToHost((DefaultPacket*)pktdata, pktsize, true);
	//delete[] pktdata;
}
void MasterServerLogic::SendNoticeMsg(const char* msg , ...)
{
	if(net_ == NULL)
		return;

	char buf[1024];

	va_list ap;
	va_start(ap, msg);
	StringCbVPrintfA(buf, sizeof(buf), msg, ap);
	va_end(ap);

	CREATE_PACKET(SBPKT_G2M_SendNoticeMsg, n);
	r3dscpy(n.msg,buf);
	net_->SendToHost(&n, sizeof(n), true);

	r3dOutToLog("BrostCast Notice Msg to all servers. '%s'\n",buf);
	return;
}
void MasterServerLogic::FinishGame()
{
	if(net_ == NULL)
		return;

	CREATE_PACKET(SBPKT_G2M_FinishGame, n);
	net_->SendToHost(&n, sizeof(n), true);

	return;
}

void MasterServerLogic::CloseGame()
{
	if(net_ == NULL || IsMasterDisconnected())
		return;

	CREATE_PACKET(SBPKT_G2M_CloseGame, n);
	net_->SendToHost(&n, sizeof(n), true);

	return;
}

void MasterServerLogic::AddPlayer(DWORD CustomerID)
{
	SBPKT_G2M_AddPlayer_s n;
	n.CustomerID = CustomerID;
	net_->SendToHost(&n, sizeof(n), true);

	return;  
}

void MasterServerLogic::RemovePlayer(DWORD CustomerID)
{
	SBPKT_G2M_RemovePlayer_s n;
	n.CustomerID = CustomerID;
	net_->SendToHost(&n, sizeof(n), true);

	return;  
}

void MasterServerLogic::RequestDataUpdate()
{
	SBPKT_G2M_DataUpdateReq_s n;
	net_->SendToHost(&n, sizeof(n), true);
}