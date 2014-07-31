#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"
#include <shellapi.h>
#include "AdminClientLogic.h"


using namespace NetPacketsAdminServer;

CAdminClientLogic adminUserLogic;

CAdminClientLogic::CAdminClientLogic()
{
	return;
}

CAdminClientLogic::~CAdminClientLogic()
{

}

void		CAdminClientLogic::Tick()
{
	//r3dOutToLog("Client Update!\n");
	g_net.Update();
}
void		CAdminClientLogic::OnNetPeerConnected(DWORD peerId)
{
	r3dOutToLog("Server %d Connected!\n",peerId);
	r3dOutToLog("Send Packet 'pongay'\n");
	CREATE_PACKET(PKT_Chatmessage,n);
	sprintf(n.msg,"pongay");
    g_net.SendToHost(&n, sizeof(n), true);
}
void		CAdminClientLogic::OnNetPeerDisconnected(DWORD peerId)
{
}
void		CAdminClientLogic::OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize)
{
}

void		CAdminClientLogic::Connect(const char* host,int port)
{
	g_net.Initialize(this, "admin server");
	g_net.CreateClient();
	int res = g_net.Connect(host, port);
}