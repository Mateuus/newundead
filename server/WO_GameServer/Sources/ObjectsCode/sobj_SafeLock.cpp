#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "sobj_SafeLock.h"

IMPLEMENT_CLASS(obj_SafeLock, "obj_SafeLock", "Object");
AUTOREGISTER_CLASS(obj_SafeLock);

obj_SafeLock::obj_SafeLock()
{
}

obj_SafeLock::~obj_SafeLock()
{
}

BOOL obj_SafeLock::OnCreate()
{
	r3dOutToLog("obj_SafeLock %p created.\n", this);
	
	r3d_assert(NetworkLocal);
	r3d_assert(GetNetworkID());
	//NoteID can be 0 for newly created notes
	
	//_time64(&m_Note.SpawnTime);
	
	// raycast down to earth in case world was changed
	//r3dPoint3D pos = gServerLogic.AdjustPositionToFloor(GetPosition());
	SetPosition(GetPosition());

	gServerLogic.NetRegisterObjectToPeers(this);
	
	return parent::OnCreate();
}

BOOL obj_SafeLock::OnDestroy()
{
	r3dOutToLog("obj_SafeLock %p destroyed\n", this);

	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));
	
	return parent::OnDestroy();
}

BOOL obj_SafeLock::Update()
{
	/*__int64 utcTime;
	if(_time64(&utcTime) > m_Note.SpawnTime + (m_Note.ExpireMins * 60))
	{
		r3dOutToLog("obj_Grave %p expired\n", this);
		setActiveFlag(0);
	}*/

	return parent::Update();
}

DefaultPacket* obj_SafeLock::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateSafeLock_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	n.pos     = GetPosition();
	
	*out_size = sizeof(n);
	return &n;
}

void obj_SafeLock::NetSendSafeLockData(DWORD peerId)
{
	PKT_S2C_SetSafeLockData_s n;
	n.pass = m_Note.pass;
	//r3dscpy(n.plr2, m_Note.plr2.c_str());
	
	gServerLogic.p2pSendToPeer(peerId, this, &n, sizeof(n));
}