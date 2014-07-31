#pragma once


#include "../../ServerNetPackets/NetPacketsAdminServer.h"

#include "r3dNetwork.h"

using namespace NetPacketsAdminServer;

class CAdminClientLogic : public r3dNetCallback
{
  public:
		CAdminClientLogic();
	virtual ~CAdminClientLogic();

	r3dNetwork	g_net;

	void Tick();
	void		Start(int port, int in_maxPeerCount);

		// callbacks from r3dNetwork
 virtual	void		OnNetPeerConnected(DWORD peerId);
virtual	void		OnNetPeerDisconnected(DWORD peerId);
virtual	void		OnNetData(DWORD peerId, const r3dNetPacketHeader* packetData, int packetSize);
	void Connect(const char* host,int port);
};

extern	CAdminClientLogic adminUserLogic;