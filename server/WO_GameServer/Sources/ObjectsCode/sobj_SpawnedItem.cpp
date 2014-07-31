#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "sobj_SpawnedItem.h"
#include "obj_ServerItemSpawnPoint.h"

IMPLEMENT_CLASS(obj_SpawnedItem, "obj_SpawnedItem", "Object");
AUTOREGISTER_CLASS(obj_SpawnedItem);

obj_SpawnedItem::obj_SpawnedItem()
{
	m_SpawnIdx = -1;
	m_DestroyIn = 0.0f;
}

obj_SpawnedItem::~obj_SpawnedItem()
{
}

BOOL obj_SpawnedItem::OnCreate()
{
	//r3dOutToLog("obj_SpawnedItem %p created. %dx%d\n", this, m_Item.itemID, m_Item.quantity);
	
	r3d_assert(NetworkLocal);
	r3d_assert(GetNetworkID());
	r3d_assert(m_SpawnObj.get() > 0);
	r3d_assert(m_SpawnIdx >= 0);
	r3d_assert(m_Item.itemID);

	// raycast down to earth in case world was changed
	r3dPoint3D pos = gServerLogic.AdjustPositionToFloor(GetPosition());
	SetPosition(pos);

	// overwrite object network visibility
	distToCreateSq = 1024 * 1024;
	distToDeleteSq = 1200 * 1200;
	
	gServerLogic.NetRegisterObjectToPeers(this);
	
	return parent::OnCreate();
}

BOOL obj_SpawnedItem::OnDestroy()
{
	//r3dOutToLog("obj_SpawnedItem %p destroyed\n", this);

	obj_ServerItemSpawnPoint* owner = (obj_ServerItemSpawnPoint*)GameWorld().GetObject(m_SpawnObj);
	r3d_assert(owner);
	owner->PickUpObject(m_SpawnIdx);
	
	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));

	return parent::OnDestroy();
}

BOOL obj_SpawnedItem::Update()
{
	if(m_DestroyIn > 0 && r3dGetTime() > m_DestroyIn)
		return FALSE; // force item de-spawn

	return parent::Update();
}

DefaultPacket* obj_SpawnedItem::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateDroppedItem_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	n.pos     = GetPosition();
	n.Item    = m_Item;

	*out_size = sizeof(n);
	return &n;
}