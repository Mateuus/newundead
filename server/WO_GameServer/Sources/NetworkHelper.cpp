#include "r3dPCH.h"
#include "r3d.h"

#include "NetworkHelper.h"

INetworkHelper::INetworkHelper()
{
	memset(PeerVisStatus, 0, sizeof(PeerVisStatus));

#if 0
	distToCreateSq = 20 * 20;
	distToDeleteSq = 30 * 30;
#else
	distToCreateSq = 500 * 500;
	distToDeleteSq = 600 * 600;
#endif
}
