#pragma once


#include "../../ServerNetPackets/NetPacketsAdminServer.h"

#include "r3dNetwork.h"

using namespace NetPacketsAdminServer;

class CAdminUserServer : public r3dNetCallback
{
  public:
	CAdminUserServer();
	~CAdminUserServer();

	void		Start(int port, int in_maxPeerCount);
	void Tick();

		// callbacks from r3dNetwork
	void		OnNetPeerConnected(DWORD peerId);
	void		OnNetPeerDisconnected(DWORD peerId);
	void		OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize);
	void OnPKT_Chatmessage(DWORD peerId, const PKT_Chatmessage_s& n);
};

extern	CAdminUserServer adminUserServer;