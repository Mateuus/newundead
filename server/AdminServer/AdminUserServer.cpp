#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"
#include <shellapi.h>
#include "AdminUserServer.h"


using namespace NetPacketsAdminServer;

static	r3dNetwork	clientNet;
CAdminUserServer adminUserServer;

CAdminUserServer::CAdminUserServer()
{
	return;
}

CAdminUserServer::~CAdminUserServer()
{

}

void CAdminUserServer::Tick()
{
	//r3dOutToLog("Update!\n");
	clientNet.Update();
}
void CAdminUserServer::Start(int port, int in_maxPeerCount)
{
	clientNet.Initialize(this, "clientNet");
	if(!clientNet.CreateHost(port, in_maxPeerCount)) {
		r3dError("CreateHost failed\n");
	}

	r3dOutToLog("AdminUserServer started at port %d, %d CCU\n", port, in_maxPeerCount);
	return;
}

void		CAdminUserServer::OnNetPeerConnected(DWORD peerId)
{
	r3dOutToLog("Client %d Connected!\n",peerId);
}
void		CAdminUserServer::OnNetPeerDisconnected(DWORD peerId)
{
	r3dOutToLog("Client %d Disconnected!\n",peerId);
}

#define DEFINE_PACKET_HANDLER_MUS(xxx) \
	case xxx: \
{ \
	const xxx##_s& n = *(xxx##_s*)packetData; \
    \
	On##xxx(peerId, n); \
	break; \
}

void		CAdminUserServer::OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize)
{
	switch(packetData->EventID) 
	{
	DEFINE_PACKET_HANDLER_MUS(PKT_Chatmessage);
	}

	return;
}

void CAdminUserServer::OnPKT_Chatmessage(DWORD peerId, const PKT_Chatmessage_s& n)
{
	r3dOutToLog("received PKT_Chatmessage from %d '%s'\n",peerId,n.msg);
}