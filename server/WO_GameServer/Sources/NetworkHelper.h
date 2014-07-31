#pragma once

#include "GameCommon.h"
#include "ServerGameLogic.h"

class INetworkHelper
{
  public:
	BYTE		PeerVisStatus[ServerGameLogic::MAX_PEERS_COUNT];

	float		distToCreateSq;
	float		distToDeleteSq;

  public:
	INetworkHelper();

	virtual DefaultPacket*	NetGetCreatePacket(int* out_size) = NULL;

	bool		GetVisibility(DWORD peerId) const
	{
		r3d_assert(peerId < ServerGameLogic::MAX_PEERS_COUNT);
		return PeerVisStatus[peerId] ? true : false;
	}
};