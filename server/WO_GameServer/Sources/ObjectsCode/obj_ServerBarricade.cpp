#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"

#include "obj_ServerBarricade.h"
#include "ServerGameLogic.h"
#include "../EclipseStudio/Sources/ObjectsCode/weapons/WeaponArmory.h"
#include "../../GameEngine/ai/AutodeskNav/AutodeskNavMesh.h"

IMPLEMENT_CLASS(obj_ServerBarricade, "obj_ServerBarricade", "Object");
AUTOREGISTER_CLASS(obj_ServerBarricade);

std::vector<obj_ServerBarricade*> obj_ServerBarricade::allBarricades;

obj_ServerBarricade::obj_ServerBarricade()
{
	allBarricades.push_back(this);
	
	m_ItemID = 0;
	ObjTypeFlags |= OBJTYPE_GameplayItem;
	requestKill = false;
	Health = 1;
	m_ObstacleId = -1;
}

obj_ServerBarricade::~obj_ServerBarricade()
{
	for(std::vector<obj_ServerBarricade*>::iterator it = allBarricades.begin(); it != allBarricades.end(); ++it)
	{
		if(*it == this)
		{
			allBarricades.erase(it);
			break;
		}
	}
}

BOOL obj_ServerBarricade::OnCreate()
{
	ObjFlags |= OBJFLAG_SkipCastRay;
	
	// set FileName based on itemid for ReadPhysicsConfig() in OnCreate() 
	r3dPoint3D bsize(1, 1, 1);
	if(m_ItemID == WeaponConfig::ITEMID_BarbWireBarricade)
	{
		FileName = "Data\\ObjectsDepot\\Weapons\\Item_Barricade_BarbWire_Built.sco";
		bsize    = r3dPoint3D(5.320016f, 1.842704f, 1.540970f);
	}
	else if(m_ItemID == WeaponConfig::ITEMID_WoodShieldBarricade)
	{
		FileName = "Data\\ObjectsDepot\\Weapons\\Item_Barricade_WoodShield_Built.sco";
		bsize    = r3dPoint3D(1.582400f, 2.083451f, 0.708091f);
	}
	else if(m_ItemID == WeaponConfig::ITEMID_RiotShieldBarricade)
	{
		FileName = "Data\\ObjectsDepot\\Weapons\\Item_Riot_Shield_01.sco";
		bsize    = r3dPoint3D(1.726829f, 2.136024f, 0.762201f);
	}
	else if(m_ItemID == WeaponConfig::ITEMID_SandbagBarricade)
	{
		FileName = "Data\\ObjectsDepot\\Weapons\\item_barricade_Sandbag_built.sco";
		bsize    = r3dPoint3D(1.513974f, 1.057301f, 1.111396f);
	}
	else
		r3dError("unknown barricade item\n");

	parent::OnCreate();
	
	// add navigational obstacle
	r3dBoundBox obb;
	obb.Size = bsize;
	obb.Org  = r3dPoint3D(GetPosition().x - obb.Size.x/2, GetPosition().y, GetPosition().z - obb.Size.z/2);
	m_ObstacleId = gAutodeskNavMesh.AddObstacle(obb, GetRotationVector().x);
	
	// calc 2d radius
	m_Radius = R3D_MAX(obb.Size.x, obb.Size.z) / 2;

	gServerLogic.NetRegisterObjectToPeers(this);
	return 1;
}

BOOL obj_ServerBarricade::OnDestroy()
{
	if(m_ObstacleId >= 0)
	{
		gAutodeskNavMesh.RemoveObstacle(m_ObstacleId);
	}
	
	return parent::OnDestroy();
}

BOOL obj_ServerBarricade::Update()
{
	float curTime = r3dGetTime();

	if(requestKill) 
	{
		PKT_S2C_DestroyNetObject_s n;
		n.spawnID = toP2pNetId(GetNetworkID());
		gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));

		return FALSE;
	}

	return parent::Update();
}

DefaultPacket* obj_ServerBarricade::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	n.itemID  = m_ItemID;
	n.pos     = GetPosition();
	n.var1    = GetRotationVector().x;

	*out_size = sizeof(n);
	return &n;
}

void obj_ServerBarricade::DoDamage(float dmg)
{
	if(Health > 0)
	{
		Health -= dmg;
		if(Health <= 0.0f)
		{
			requestKill = true;
		}
	}
}