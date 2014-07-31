#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"
#include "../EclipseStudio/Sources/ObjectsCode/weapons/WeaponArmory.h"

#include "sobj_DroppedItem.h"

const float DROPPED_ITEM_EXPIRE_TIME = 60 * 60.0f; // 60 min

IMPLEMENT_CLASS(obj_DroppedItem, "obj_DroppedItem", "Object");
AUTOREGISTER_CLASS(obj_DroppedItem);

obj_DroppedItem::obj_DroppedItem()
{
}

obj_DroppedItem::~obj_DroppedItem()
{
}

BOOL obj_DroppedItem::OnCreate()
{
	//r3dOutToLog("obj_DroppedItem %p created. %d\n", this, m_Item.itemID);

	r3d_assert(NetworkLocal);
	r3d_assert(GetNetworkID());
	r3d_assert(m_Item.itemID);

	// check if we will be dropping up WHOLE clip
	const WeaponAttachmentConfig* cfg = g_pWeaponArmory->getAttachmentConfig(m_Item.itemID);
	if(cfg && cfg->category == storecat_FPSAttachment && cfg->m_type == WPN_ATTM_CLIP)
	{
		// if so, make it stackable
		if(m_Item.Var1 == cfg->m_Clipsize)
			m_Item.Var1 = -1;
	}

	// overwrite object network visibility
	distToCreateSq = 256 * 256;
	distToDeleteSq = 256 * 256;

	gServerLogic.NetRegisterObjectToPeers(this);

	m_expireTime = r3dGetTime() + DROPPED_ITEM_EXPIRE_TIME;
	return parent::OnCreate();
}

BOOL obj_DroppedItem::OnDestroy()
{
	//r3dOutToLog("obj_DroppedItem %p destroyed\n", this);

	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));
	
	return parent::OnDestroy();
}

BOOL obj_DroppedItem::Update()
{
	if(r3dGetTime() > m_expireTime)
	{
		setActiveFlag(0);
	}

	return parent::Update();
}

DefaultPacket* obj_DroppedItem::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateDroppedItem_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	n.pos     = GetPosition();
	n.Item    = m_Item;
	
	*out_size = sizeof(n);
	return &n;
}