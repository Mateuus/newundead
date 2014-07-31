#ifndef WO_SERVER
/*
* TeamSpeak 3 client sample
*
* Copyright (c) 2007-2012 TeamSpeak Systems GmbH
*/

/*#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#endif
#include <stdio.h>
#include <limits.h>*/

#include "r3dpch.h"
#include "r3d.h"
#include "GameLevel.h"
#include "APIScaleformGfx.h"
#include "AI_Player.h"
#include "AI_PlayerAnim.h"

#include "r3dPhysSkeleton.h"
#include "r3dAtmosphere.h"
#include "ObjectsCode\Effects\obj_ParticleSystem.h"

#include "Particle.h"

#include "ObjectsCode/world/DecalChief.h"
#include "ObjectsCode/world/MaterialTypes.h"

#include "ObjectsCode/weapons/Weapon.h"

#include "ObjectsCode/weapons/Gear.h"
#include "ObjectsCode/WEAPONS/Ammo.h"
#include "ObjectsCode/weapons/WeaponArmory.h"
#include "ObjectsCode/weapons/ExplosionVisualController.h"

#include "ObjectsCode/Gameplay/obj_PlayerSpawnPoint.h"
#include "ObjectsCode/Gameplay/obj_DamageArea.h"
#include "ObjectsCode/Gameplay/obj_DroppedItem.h"
#include "ObjectsCode/Gameplay/obj_Note.h"
#include "ObjectsCode/Gameplay/obj_Grave.h"
#include "ObjectsCode/Gameplay/obj_PostBox.h"
#include "ObjectsCode/Gameplay/obj_SafeLock.h"
#include "../Gameplay/obj_Animals.h"
#include "../../GameEngine/gameobjects/obj_Vehicle.h"
#include "../Gameplay/obj_Zombie.h"
#include "../Gameplay/obj_ZombieDummy.h"
#include "ObjectsCode/Gameplay/NPCObject.h"

#include "ObjectsCode/WORLD/obj_LightMesh.h"

#include "multiplayer/ClientGameLogic.h"
#include "GameCode/UserProfile.h"
#include "GameCode/UserRewards.h"

#include "Gameplay_Params.h"

#include "CamouflageDataManager.h"

#include "../SF/CmdProcessor/CmdProcessor.h"
#include "../SF/CmdProcessor/CmdConsole.h"

#include "..\..\ui\HUDDisplay.h"
#include "..\..\ui\HUD_EditorGame.h"
#include "..\..\ui\FrontEndShared.h"
#include "..\..\ui\HUDPause.h"
#include "..\..\ui\HUDTrade.h"
#include "..\..\ui\HUDActionUI.h"
#include "..\..\ui\HUDAttachments.h"
#include "..\..\ui\HUDGeneralStore.h"
#include "..\..\ui\HUDVault.h"
#include "..\..\ui\HUDRepair.h"

#include "../../VoIP/public_definitions.h"
#include "../../VoIP/public_errors.h"
#include "../../VoIP/clientlib_publicdefinitions.h"
#include "../../VoIP/clientlib.h"

extern HUDDisplay*	hudMain;
extern HUDPause*	hudPause;
extern HUDAttachments*	hudAttm;
extern HUDActionUI*	hudActionUI;
extern HUDGeneralStore*	hudGeneralStore;
extern HUDVault* hudVault;
extern HUDTrade* hudTrade;
extern HUDRepair* hudRepair;
//HUDTrade* hudTrade = NULL;
extern 	PlayerStateVars_s CurrentRig;
//float m_Stamina = m_Stamina1;
extern bool g_bEditMode;
//float recoil;
//float spread;
//float recoil2;
static float nextSendTimeUpdate = r3dGetTime();
extern void TPSGameHud_UnlockAchievement( int achievementID );

extern float getWaterDepthAtPos(const r3dPoint3D& pos);

IMPLEMENT_CLASS(obj_Player, "obj_Player", "Object");
AUTOREGISTER_CLASS(obj_Player);

float lastSendWpnLog;
float		_ai_fTurnSpeedIdle      = 360;
float		_ai_fTurnSpeedWalk	= 720;
float		_ai_NetPosUpdateDelta = 1.0f / 10;	// "30"
float		_zai_AttackDamage = 23.0f;

bool		g_CameraInsidePlayer = false;

#include "../../rendering/Deffered/PFX_ScopeEffect.h"
extern int g_RenderScopeEffect;
extern PFX_ScopeEffect gPFX_ScopeEffect;

r3dSec_type<CUberData*, 0x345fdFCa> AI_Player_UberData = NULL;

r3dTexture* obj_Player::s_laserPointerTex = NULL;
r3dTexture* obj_Player::s_flashlightFlashTex = NULL;
r3dTexture* obj_Player::s_laserFlashTex = NULL;
r3dTexture* obj_Player::s_lightHaloTex = NULL;

r3dTexture* gPlayer_AuraTexture;
r3dTexture* gPlayer_CamoTexture;

gobjid_t m_LocalPlayer_CurrentAimAt;

#define TIME_FOR_SECOND_WIND 5.0f
#define KILLSTREAK_FOR_SECOND_WIND 3
static const float gDist_To_Allow_Vehicle_Entry = 5;
bool gDisableShortcuts = false;
struct PhysSkeletonCacheEntry
{
	int Taken ;
	r3dPhysSkeleton* Skel ;
};

typedef r3dTL::TArray< PhysSkeletonCacheEntry > PhysSkeletonCache ;

int g_bPhysSkeletonCacheInitialized ;
PhysSkeletonCache g_PhysSkeletonCache ;

// per shot
float SpreadIncrease[] = 
{
	0.15f, // ASR
	1.0f, // SNP
	0.5f, // SHTG
	0.45f, // MG
	0.0f, // SUPPORT
	0.5f, // HG
	0.1f, // SMG
	0.0f, // GRENADES
};
// per second
float SpreadDecrease[] = 
{
	4.0f, // ASR
	1.0f, // SNP
	4.0f, // SHTG
	2.0f, // MG
	4.0f, // SUPPORT
	1.0f, // HG
	4.0f, // SMG
	4.0f, // GRENADES
};
// percentage of weapon's spread
float SpreadMin[] = 
{
	.2f, // ASR
	1.0f, // SNP
	.25f, // SHTG
	.2f, // MG
	.1f, // SUPPORT
	.1f, // HG
	.15f, // SMG
	.1f, // GRENADES
};
void obj_Player::CheckVeCam()
{
	//if (this != gClientLogic().localPlayer_)
	//return;
	if (g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle())
	{
		r3dPoint3D scrCoord;
		r3dProjectToScreen(g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->GetPosition() + r3dPoint3D(0, 1.8f, 0), &scrCoord);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y,    r3dColor(255,255,255,100), "Speed %.2f km/h",g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->vd->vehicle->computeForwardSpeed()*2);	
	}
}
void AddCachedPhysSkeleton()
{
	PhysSkeletonCacheEntry entry ;

	entry.Skel	= new r3dPhysSkeleton( "data/ObjectsDepot/Characters/RagDoll.RepX" ) ;
	entry.Taken	= 0 ;

	g_PhysSkeletonCache.PushBack( entry ) ;
}

void InitializePhysSkeletonCache( int count, float progressStart, float progressEnd )
{
	r3d_assert( !g_bPhysSkeletonCacheInitialized ) ;

	R3D_LOG_TIMESPAN_START(InitializeSkeletonCache);
	for( int i = 0, e = count ; i < e ; i ++ )
	{
		AddCachedPhysSkeleton() ;

		void SetLoadingProgress( float progress ) ;
		SetLoadingProgress( progressStart + ( progressEnd - progressStart ) * i / ( count - 1 ) ) ;
	}
	R3D_LOG_TIMESPAN_END(InitializeSkeletonCache);	

	g_bPhysSkeletonCacheInitialized = 1 ;
}

void DestroyPhysSkeletonCache()
{
	r3d_assert( g_bPhysSkeletonCacheInitialized ) ;

	for( int i = 0, e = g_PhysSkeletonCache.Count() ; i < e ; i ++ )
	{
		PhysSkeletonCacheEntry& entry = g_PhysSkeletonCache[ i ] ;
		delete entry.Skel ;
	}

	g_PhysSkeletonCache.Clear() ;

	g_bPhysSkeletonCacheInitialized = 0 ;
}

void ReleaseCachedPhysSkeleton( r3dPhysSkeleton* skel )
{
	if( skel )
	{
		for( int i = 0, e = g_PhysSkeletonCache.Count() ; i < e ; i ++ )
		{
			PhysSkeletonCacheEntry& entry = g_PhysSkeletonCache[ i ] ;
			if( entry.Skel == skel )
			{
				entry.Skel->unlink() ;
				entry.Skel->SwitchToRagdoll(false);
				entry.Taken = 0 ;
				return ;
			}
		}

		r3dError( "ReleaseCacheSkeleton: skeleton not found!" ) ;
	}
}

r3dPhysSkeleton* AquireCacheSkeleton()
{
	for( int i = 0, e = g_PhysSkeletonCache.Count() ; i < e ; i ++ )
	{
		PhysSkeletonCacheEntry& entry = g_PhysSkeletonCache[ i ] ;

		if( !entry.Taken )
		{
			entry.Taken = 1 ;
			return entry.Skel ;
		}
	}

	AddCachedPhysSkeleton() ;

	PhysSkeletonCacheEntry& entry = g_PhysSkeletonCache.GetLast() ;

	entry.Taken = 1 ;

	return entry.Skel ;
}


struct PhysObstacleCacheEntry
{
	int Taken;
	ObstacleHandle Handle;
};
PhysObstacleCacheEntry*	g_PhysObstacleCache = NULL;
int		g_PhysObstacleSize  = 0;

void InitializePhysObstacleCache(int count)
{
	r3d_assert(g_PhysObstacleCache == NULL) ;

	g_PhysObstacleCache = new PhysObstacleCacheEntry[count];
	g_PhysObstacleSize  = count;

	for(int i=0; i<count; i++)
	{
		PxBoxObstacle obstacle;
		obstacle.mHalfExtents = PxVec3(0.25f, 1.0f, 0.25f);
		obstacle.mPos = PxExtendedVec3(0, 9000 + (float)i * 10, 0); // teleport way up
		g_PhysObstacleCache[i].Handle = g_pPhysicsWorld->m_PlayerObstaclesManager->addObstacle(obstacle);
		g_PhysObstacleCache[i].Taken  = 0;
	}
}

void DestroyPhysObstacleCache()
{
	r3d_assert(g_PhysObstacleCache);

	// destroy in reverse order 
	for(int i=g_PhysObstacleSize-1; i>=0; i--)
	{
		g_pPhysicsWorld->m_PlayerObstaclesManager->removeObstacle(g_PhysObstacleCache[i].Handle);
		g_PhysObstacleCache[i].Handle = 0;
		g_PhysObstacleCache[i].Taken  = 0;
	}

	SAFE_DELETE_ARRAY(g_PhysObstacleCache);
	g_PhysObstacleSize = 0;
}

int AcquirePlayerObstacle(const r3dPoint3D& pos)
{
	r3d_assert(g_PhysObstacleCache);

	for(int i=0; i<g_PhysObstacleSize; i++)
	{
		if(g_PhysObstacleCache[i].Taken)
			continue;

		g_PhysObstacleCache[i].Taken = 1;
		UpdatePlayerObstacle(i, pos);
		return i;
	}

	return -1;
}

void UpdateVehicleObstacle(int idx, const r3dPoint3D& pos)
{
	if(idx == -1)
		return;

	R3DPROFILE_FUNCTION("UpdateAnimalsObstacle");

	r3d_assert(idx >= 0 && idx < g_PhysObstacleSize);
	r3d_assert(g_PhysObstacleCache[idx].Taken);

	PxBoxObstacle obstacle;
	obstacle.mHalfExtents = PxVec3(1.25f, 1, 1.25f);
	obstacle.mPos = PxExtendedVec3(pos.x, pos.y + 1.0f, pos.z);
	g_pPhysicsWorld->m_PlayerObstaclesManager->updateObstacle(g_PhysObstacleCache[idx].Handle, obstacle);

	return;
}

void ReleasePlayerObstacle(int* out_idx)
{
	int idx = *out_idx;
	if(idx == -1)
		return;

	r3d_assert(idx >= 0 && idx < g_PhysObstacleSize);
	r3d_assert(g_PhysObstacleCache[idx].Taken);

	UpdatePlayerObstacle(idx, r3dPoint3D(0, 9000 + (float)idx * 10, 0)); // teleport way up
	g_PhysObstacleCache[idx].Taken = 0;
	*out_idx = -1;
}
void UpdateAnimalsObstacle(int idx, const r3dPoint3D& pos)
{
	if(idx == -1)
		return;

	R3DPROFILE_FUNCTION("UpdateAnimalsObstacle");

	r3d_assert(idx >= 0 && idx < g_PhysObstacleSize);
	r3d_assert(g_PhysObstacleCache[idx].Taken);

	PxBoxObstacle obstacle;
	obstacle.mHalfExtents = PxVec3(0.70f, 0.75f, 0.70f);
	obstacle.mPos = PxExtendedVec3(pos.x, pos.y + 1.0f, pos.z);
	g_pPhysicsWorld->m_PlayerObstaclesManager->updateObstacle(g_PhysObstacleCache[idx].Handle, obstacle);

	return;
}
void UpdatePlayerObstacle(int idx, const r3dPoint3D& pos)
{
	if(idx == -1)
		return;

	R3DPROFILE_FUNCTION("UpdateObstacle");

	r3d_assert(idx >= 0 && idx < g_PhysObstacleSize);
	r3d_assert(g_PhysObstacleCache[idx].Taken);

	PxBoxObstacle obstacle;
	obstacle.mHalfExtents = PxVec3(0.25f, 1.0f, 0.25f);
	obstacle.mPos = PxExtendedVec3(pos.x, pos.y + 1.0f, pos.z);
	g_pPhysicsWorld->m_PlayerObstaclesManager->updateObstacle(g_PhysObstacleCache[idx].Handle, obstacle);

	return;
}

static void AdjustHitPosition(DecalParams& params, GameObject* target);

void AI_Player_FreeStuff()
{
	SAFE_DELETE(AI_Player_UberData);

	if(gPlayer_AuraTexture)
	{
		r3dRenderer->DeleteTexture(gPlayer_AuraTexture);

		gPlayer_AuraTexture = 0;
	}

	if(gPlayer_CamoTexture)
	{
		r3dRenderer->DeleteTexture(gPlayer_CamoTexture);

		gPlayer_CamoTexture = 0;
	}
}

#ifndef FINAL_BUILD
DECLARE_CMD(binv)
{
	if(ev.NumArgs() != 1)
	{
		ConPrint("binv");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	ConPrint("Backpack id:%d, size:%d\n", slot.BackpackID, slot.BackpackSize);
	ConPrint("Inventory List:\n");
	for(int i=0; i<slot.BackpackSize; i++)
	{
		wiInventoryItem& wi = slot.Items[i];
		if(wi.itemID == 0)
			continue;
		const BaseItemConfig* cfg = g_pWeaponArmory->getConfig(wi.itemID);
		ConPrint("%02d: ItemID:%d, q:%d, Var1:%d Var2:%d %s", i, wi.itemID, wi.quantity, wi.Var1, wi.Var2, cfg->m_StoreName);
	}

	return;
}

DECLARE_CMD(pickup)
{
	obj_Player* plr = gClientLogic().localPlayer_;

	GameObject* closest = NULL;
	float       minDist = 99999999.0f;
	for(GameObject* obj = GameWorld().GetFirstObject(); obj; obj = GameWorld().GetNextObject(obj))
	{
		if(obj->Class->Name == "obj_DroppedItem" || obj->Class->Name == "obj_Note")
		{
			float dist = (plr->GetPosition() - obj->GetPosition()).Length();
			if(dist < minDist) 
			{
				closest = obj;
				minDist = dist;
			}
		}
	}

	if(!closest) {
		ConPrint("no closest items");
		return;
	}

	ConPrint("using network obj %d\n", closest->GetNetworkID());

	PKT_C2S_UseNetObject_s n;
	n.spawnID = toP2pNetId(closest->GetNetworkID());
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
}

DECLARE_CMD(giveitem)
{
	if(ev.NumArgs() != 3)
	{
		ConPrint("giveitem {itemid} {num}");
		return;
	}

	int num = ev.GetInteger(2);
	for(int i=0; i<num; i++)
	{
		PKT_C2S_Admin_GiveItem_s n;
		n.ItemID = ev.GetInteger(1);
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
	}
}


DECLARE_CMD(changebp)
{
	if(ev.NumArgs() != 2)
	{
		ConPrint("changebp {slot}");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	int slotFrom = ev.GetInteger(1);

	const BackpackConfig* cfg = g_pWeaponArmory->getBackpackConfig(slot.Items[slotFrom].itemID);
	if(!cfg) {
		ConPrint("bad backpack itemid");
		return;
	}

	plr->ChangeBackpack(slotFrom);
}

DECLARE_CMD(alist)
{
	if(ev.NumArgs() != 1)
	{
		ConPrint("alist {slot}");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	int wid      = 0; // test - by default modify weapon 0
	Weapon* wpn = plr->m_Weapons[wid];
	if(!wpn) {
		ConPrint("no weapon");
		return;
	}

	ConPrint("Weapon: %s\n", wpn->getConfig()->m_StoreName);
	for(int i=0; i<WPN_ATTM_MAX; i++)
	{
		ConPrint(" Slot: %d, cur %d, def %d\n", i, slot.Attachment[wid].attachments[i], wpn->getConfig()->FPSDefaultID[i]);
		if(!wpn->getConfig()->FPSSpecID[i])
			continue;

		char buf[2084] = "  ";
		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const WeaponAttachmentConfig* config = g_pWeaponArmory->getAttachmentConfig(itemID);
			if(!config) continue;
			if(config->m_specID == wpn->getConfig()->FPSSpecID[i])
				sprintf(buf + strlen(buf), ", %d %s", config->m_itemID, config->m_StoreName);
			if(config->m_itemID == wpn->getConfig()->FPSDefaultID[i])
				sprintf(buf + strlen(buf), ", (def) %d %s", config->m_itemID, config->m_StoreName);
		}
		ConPrint(buf);
	}
}

DECLARE_CMD(aequip)
{
	if(ev.NumArgs() != 2)
	{
		ConPrint("attmequip {slot}");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	int wid      = 0; // test - by default modify weapon 0
	int slotFrom = ev.GetInteger(1);

	const WeaponAttachmentConfig* cfg = g_pWeaponArmory->getAttachmentConfig(slot.Items[slotFrom].itemID);
	if(!cfg) {
		ConPrint("bad attachment itemid");
		return;
	}

	Weapon* wpn = plr->m_Weapons[wid];
	if(!wpn) {
		ConPrint("no weapon");
		return;
	}

	plr->EquipWpnAttm(wid, slotFrom);
}

DECLARE_CMD(aremove)
{
	if(ev.NumArgs() != 2)
	{
		ConPrint("aremove {weapon attm type}");
		return;
	}


	int wid      = 0; // test - by default modify weapon 0
	int wpnAttmType = ev.GetInteger(1);
	if(wpnAttmType >= WPN_ATTM_MAX)  {
		ConPrint("bad wpnAttmType");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	plr->RemoveWpnAttm(0, wpnAttmType);
}
#endif

bool obj_Player::ChangeBackpack(int slotFrom)
{
	wiCharDataFull& slot = CurLoadout;

	const BackpackConfig* cfg = g_pWeaponArmory->getBackpackConfig(slot.Items[slotFrom].itemID);
	if(!cfg) {
		return false;
	}

	PKT_C2S_PlayerChangeBackpack_s n;
	n.SlotFrom     = slotFrom;
	n.BackpackSize = cfg->m_maxSlots;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	// replace backpack in used slot with current one (SERVER CODE SYNC POINT)
	slot.Items[slotFrom].itemID = slot.BackpackID;

	// change it
	slot.BackpackID   = cfg->m_itemID;
	slot.BackpackSize = cfg->m_maxSlots;
	UpdateLoadoutSlot(slot);

	return true;
}

void obj_Player::DropItem(int slotFrom)
{
	PKT_C2S_BackpackDrop_s n;
	n.SlotFrom = slotFrom;
	n.pos      = GetPosition() + GetvForw()*0.1f;
	p2pSendToHost(this, &n, sizeof(n));

	int itemID = CurLoadout.Items[slotFrom].itemID;

	//local logic
	CurLoadout.Items[slotFrom].quantity--;
	if(CurLoadout.Items[slotFrom].quantity <= 0) {
		CurLoadout.Items[slotFrom].Reset();
	}

	// fucked up attachment design!
	const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(itemID);
	if(wac)
	{
		int quantity = CurLoadout.Items[slotFrom].quantity; // if attm is droppen, then here will be zero
		for(int i=0; i<CurLoadout.BackpackSize; ++i)
		{
			if(CurLoadout.Items[i].itemID == itemID)
				quantity += CurLoadout.Items[i].quantity;
		}
		if(quantity == 0) // check if that attm is equipped and if yes, remove it from weapon
		{
			uint32_t curAttm[WPN_ATTM_MAX] = {0};
			if(m_Weapons[0])
			{
				m_Weapons[0]->getCurrentAttachmentIDs(curAttm);
				if(curAttm[wac->m_type] == itemID)
					RemoveWpnAttm(0, wac->m_type);
			}
			if(m_Weapons[1])
			{
				m_Weapons[1]->getCurrentAttachmentIDs(curAttm);
				if(curAttm[wac->m_type] == itemID)
					RemoveWpnAttm(1, wac->m_type);
			}			
		}
	}

	OnBackpackChanged(slotFrom);

	if(NetworkLocal)
	{
		const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(itemID);
		if(wc)
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_DROPGUN"), GetPosition());
		else
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_DROPITEM"), GetPosition());
	}
}

void obj_Player::RemoveWpnAttm(int wid, int attmType)
{
	wiCharDataFull& slot = CurLoadout;

	if(attmType >= WPN_ATTM_MAX)  {
		return;
	}

	Weapon* wpn = m_Weapons[wid];
	if(!wpn) {
		return;
	}

	PKT_C2S_PlayerRemoveAttachment_s n;
	n.wid         = wid;
	n.WpnAttmType = attmType;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	// set wpn attachment
	slot.Attachment[wid].attachments[attmType] = 0;
	wpn->setWeaponAttachmentsByIDs(slot.Attachment[wid].attachments);
}

void obj_Player::EquipWpnAttm(int wid, int backpackSlot)
{
	wiCharDataFull& slot = CurLoadout;

	int slotFrom = backpackSlot;

	const WeaponAttachmentConfig* cfg = g_pWeaponArmory->getAttachmentConfig(slot.Items[slotFrom].itemID);
	if(!cfg) {
		return;
	}

	Weapon* wpn = m_Weapons[wid];
	if(!wpn) {
		return;
	}

	PKT_C2S_PlayerEquipAttachment_s n;
	n.wid          = wid;
	n.AttmSlot     = slotFrom;
	n.dbg_AttmID   = cfg->m_itemID;
	n.dbg_WeaponID = wpn->getConfig()->m_itemID;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	// set wpn attachment
	slot.Attachment[wid].attachments[cfg->m_type] = cfg->m_itemID;
	wpn->setWeaponAttachmentsByIDs(slot.Attachment[wid].attachments);

	// we're changing clip, reload weapon
	if(cfg->m_type == WPN_ATTM_CLIP)
	{
		wpn->Reload();
	}
}


void _player_AdjustBoneCallback(DWORD dwData, int boneId, D3DXMATRIX &mp, D3DXMATRIX &anim)
{
	obj_Player* npc = (obj_Player*)dwData;
	if (!npc)
		return;

	if(npc->m_PhysSkeleton && npc->m_PhysSkeleton->IsRagdollMode())
		return;

	if(g_camera_mode->GetInt()==2 && npc->NetworkLocal) // FPS bone adjustments
	{
	}
	else // TPS bone adjustments
	{
		if(boneId == npc->boneId_Bip01_Spine1 || boneId == npc->boneId_Bip01_Neck) 
		{
			float xRot = npc->bodyAdjust_x;
			float yRot = npc->bodyAdjust_y[0];

			// add recoil for spine1
			if(boneId == npc->boneId_Bip01_Spine1)
			{
				if(!(npc->NetworkLocal && npc->hasScopeMode() && CurrentRig.allowScope)) // don't add recoil in scope mode to a weapon, as it messes up muzzle flash rendering
				{
					float recoil = npc->RecoilViewMod.y*0.5f;
					recoil = R3D_MIN(recoil, 0.0f);

					yRot += recoil;
				}
			}

			D3DXMATRIX r1, r2;

			// rotate previous matrix, with saved position
			r3dPoint3D vv = r3dPoint3D(mp._41, mp._42, mp._43);
			D3DXMatrixRotationY(&r2, xRot/2);
			D3DXMatrixRotationX(&r1, -yRot/2);
			mp = mp * (r1 * r2);
			mp._41 = vv.x;
			mp._42 = vv.y;
			mp._43 = vv.z;
		}
		else if(boneId == npc->boneId_Bip01_LUpperArm || boneId == npc->boneId_Bip01_RUpperArm)
		{
			float xRot = npc->RecoilViewMod.x*0.1f;
			float yRot = npc->RecoilViewMod.y*0.5f;

			xRot = 0;//-R3D_MIN(xRot, 0.5f); 
			yRot = R3D_MIN(yRot, 0.1f);

			D3DXMATRIX r1, r2;

			// rotate previous matrix, with saved position
			r3dPoint3D vv = r3dPoint3D(mp._41, mp._42, mp._43);
			D3DXMatrixRotationY(&r2, xRot/2);
			D3DXMatrixRotationAxis(&r1, npc->GetvRightDraw().d3dx(), -yRot/2);
			mp = mp * (r1 * r2);
			mp._41 = vv.x;
			mp._42 = vv.y;
			mp._43 = vv.z;
		}
	}

	return;    
}

void obj_Player::UpdateLoadoutSlot(const wiCharDataFull& slot)
{
	R3DPROFILE_FUNCTION("obj_Player::UpdateLoadoutSlot");

	CurLoadout = slot; // save loadout
	m_GearWeight = 0;
	m_isFemaleHero = CurLoadout.HeroItemID==20184||CurLoadout.HeroItemID==20194||CurLoadout.HeroItemID==20195;

	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; ++i)
		SAFE_DELETE(m_Weapons[i]);

	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;

	// create weapons & items
	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; i++)
	{
		int ItemID = slot.Items[i].itemID;
		if(ItemID == 0)
			continue;

		const WeaponConfig* wcfg = g_pWeaponArmory->getWeaponConfig(ItemID);
		if(wcfg == NULL) {
			//r3dOutToLog("!!!! there is no weapon id %d\n", ItemID);
			continue;
		}

		// r3dOutToLog("UpdateLoadoutSlot id %d\n", ItemID);

		m_Weapons[i] = new Weapon(this, i, wcfg, isFirstPerson, true, i <= wiCharDataFull::CHAR_LOADOUT_WEAPON2 ? &CurLoadout.Attachment[i] : NULL);
		r3d_assert(m_Weapons[i]); // to check for out of memory
	}

	if(NetworkLocal)
	{
		updateWeaponUI();
	}

	uberEquip_->SetSlot(SLOT_Weapon, m_Weapons[m_SelectedWeapon]);
	uberEquip_->SetSlot(SLOT_WeaponBackRight, (Weapon*)NULL);
	uberEquip_->SetSlot(SLOT_WeaponSide, (Weapon*)NULL);

	if(m_Weapons[m_SelectedWeapon] && !forcedEmptyHands)
		m_Weapons[m_SelectedWeapon]->OnEquip();

	if(m_Weapons[1] && m_SelectedWeapon != 1)
		uberEquip_->SetSlot(SLOT_WeaponSide, m_Weapons[1]);

	const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(CurLoadout.HeroItemID);
	loadHeroMeshes(heroConfig);
	if(heroConfig)
		m_GearWeight += heroConfig->m_Weight;

	const BackpackConfig* backpack = g_pWeaponArmory->getBackpackConfig(CurLoadout.BackpackID);
	if(backpack)
		uberEquip_->SetSlot(SLOT_Backpack, backpack->getMesh());
	else
		uberEquip_->SetSlot(SLOT_Backpack, (r3dMesh*)NULL);

	uint32_t SlotArmorID    = slot.Items[wiCharDataFull::CHAR_LOADOUT_ARMOR].itemID;
	uint32_t SlotHeadGearID = slot.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID;
	if(SlotArmorID>0)
	{
		uberEquip_->SetSlot(SLOT_Armor, g_pWeaponArmory->createGear(SlotArmorID));

		if(uberEquip_->slots_[SLOT_Armor].gear)
			m_GearWeight += uberEquip_->slots_[SLOT_Armor].gear->GetWeight();
	}
	else
		uberEquip_->SetSlot(SLOT_Armor, (Gear*)NULL);

	if(SlotHeadGearID>0)
	{
		uberEquip_->SetSlot(SLOT_Helmet, g_pWeaponArmory->createGear(SlotHeadGearID));

		if(uberEquip_->slots_[SLOT_Helmet].gear)
			m_GearWeight += uberEquip_->slots_[SLOT_Helmet].gear->GetWeight();
	}
	else
		uberEquip_->SetSlot(SLOT_Helmet, (Gear*)NULL);

	// start loading meshes
	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; ++i)
	{
		if(m_Weapons[i])
			m_Weapons[i]->getModel(true, isFirstPerson);
	}
	for(int i=0; i<SLOT_Max; ++i)
	{
		if(uberEquip_->slots_[i].gear)
			uberEquip_->slots_[i].gear->getModel(isFirstPerson);
	}

	uberAnim_->CurrentWeapon = NULL;
	SyncAnimation(true);
}

void obj_Player::loadHeroMeshes(const HeroConfig* heroConfig)
{
	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	if(heroConfig)
	{
		uberEquip_->SetSlot(SLOT_UpperBody, heroConfig->getBodyMesh(CurLoadout.BodyIdx, isFirstPerson));
		uberEquip_->SetSlot(SLOT_LowerBody, heroConfig->getLegMesh(CurLoadout.LegsIdx));
		uberEquip_->SetSlot(SLOT_Head, heroConfig->getHeadMesh(CurLoadout.HeadIdx));
	}
	else
	{
		uberEquip_->SetSlot(SLOT_UpperBody, (r3dMesh*)NULL);
		uberEquip_->SetSlot(SLOT_LowerBody, (r3dMesh*)NULL);
		uberEquip_->SetSlot(SLOT_Head, (r3dMesh*)NULL);
	}
}

obj_Player::obj_Player()
: footStepsSnd(0)
, currentFootStepsSoundID(-1)
, boneId_Bip01_Spine1(-1)
, boneId_Bip01_Neck(-1)
, boneId_Bip01_LUpperArm(-1)
, boneId_Bip01_RUpperArm(-1)
, boneId_Bip01_Head(-1)
, boneId_Bip01_R_Hand(-1)
, boneId_Bip01_L_Hand(-1)
, m_AuraType(AT_NONE)
, netMover(this, _ai_NetPosUpdateDelta, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
, m_enableRendering(true)
, lifeProperties(this)
, m_ItemSimulateLeftClick(false)
, showSlotsInfoTimer(0.0f)
, forcedEmptyHands(false)
, m_sndBreathSprint(0)
, m_bswim(0)
, m_bswimf(0)
, m_sndInPain(0)
, m_sndClothMovementSprint(0)
, m_sndBreathBaseVolume(1)
, m_BarricadeMeshPlacement(NULL)
, m_isFemaleHero(false)
, m_sndSniperBreath(0)
, m_sndSniperHeartSlow(0)
, m_sndSniperExhale(0)
{
	uberEquip_ = NULL;
	m_PhysSkeleton = NULL;

	m_disablePhysSkeleton = false;

	PhysXObstacleIndex = -1;

	lastTimeHit = 0;
	bOnGround =1;
	fHeightAboveGround = 0.0f;
	StartFallingHeight = 0;
	StartFallingTime = 0;
	bCrouch = 0;
	bProne = 0;
	TimeOfDeath = 0;
	PosOfDeath.Assign(0,0,0);
	DisableKillerView = false;
	bSwim = false;
	bSwimShift = false;
	Dead_KillerID = 0;
	m_BloodTimer = 0;
	m_GearWeight = 0;
	CustomerID     = 0;
	ClanID         = 0;
	GroupID         = 0;
	ClanTagColor   = 0xFFFFFFFF;
	r3dscpy(ClanTag, "CLAN");

	//CoolDown = 0.0f;

	viewTargetPos		= r3dPoint3D(0, 0, 0);
	viewTargetNorm		= r3dPoint3D(0, 1, 0);

	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; ++i)
		m_Weapons[i] = NULL;
	m_SelectedWeapon = 0;
	m_PrevSelectedWeapon = -1;

	rewards_.reserve(32);
	lastRewardShowTime_ = 0;


#if VEHICLES_ENABLED
	vehicleViewActive_ = VehicleView_None; 
#endif

#ifndef FINAL_BUILD
	static bool cmd_registered = false;
	if(!cmd_registered)
	{
		cmd_registered = true;
		REG_CCOMMAND(binv, 0, "show backpack");
		REG_CCOMMAND(pickup, 0, "use closest network object");
		REG_CCOMMAND(giveitem, 0, "give item to player");
		REG_CCOMMAND(changebp, 0, "change backpack");
		REG_CCOMMAND(alist, 0, "list attachments");
		REG_CCOMMAND(aequip, 0, "equip attachment");
		REG_CCOMMAND(aremove, 0, "remove attachment");
	}
#endif

	if(g_pDecalChief && g_pDecalChief->GetTypeCount() > 254)
	{
		r3dError("more that 254 decals"); 
	}
	if(ImpactParticleEntry::NumRegistered > 254)
	{
		r3dError("more that 254 impact particles");
	}

	// create animation data first time player is created
	if(!AI_Player_UberData)
	{
		AI_Player_UberData = new CUberData();
	}

	if(!gPlayer_AuraTexture)
	{
		gPlayer_AuraTexture = r3dRenderer->LoadTexture("Data/Shaders/TEXTURE/Aura.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
	}

	if(!gPlayer_CamoTexture)
	{
		gPlayer_CamoTexture = r3dRenderer->LoadTexture("Data/Shaders/TEXTURE/plastic_nm.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
	}
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_Player::Load(const char *fname)
{
	if(!parent::Load(fname)) return FALSE;

	CurLoadout.isVisible = true;
	bPersistent 	= 0;

	ObjTypeFlags |= OBJTYPE_Human;

	Height 	 	= 1.8f;
	m_EncryptedUserName.set("---");

	lastDamageRcvd = 0;
	lastTimeHitForce  = r3dPoint3D(1, 0, 0);
	lastTimeHitBone = 0;
	bDead                 = 0;
	BloodEffect = 0;
	m_ReticleCurrentScale = 1.0f;
	m_ReticleTargetScale = 1.0f;
	m_isAiming = false;
	m_isChangedAiming1 = false;
	m_isPressedFireTrigger = false;
	m_isFinishedAiming = false;
	m_isInScope = false;

	grenadeReadyTime = 0;

	m_OrigBBox.Org.Assign( -0.5, 0, -0.5 );
	m_OrigBBox.Size.Assign( 1, 2, 1 );

	SetBBoxLocal( m_OrigBBox ) ;

	m_vVision.Assign(1, 0, 0);
	ViewAngle.Assign(0, 0, 0);
	RecoilViewMod.Assign(0,0,0);
	RecoilViewModTarget.Assign(0,0,0);
	RecoilViewModTarget2.Assign(0,0,0);
	SniperViewMod.Assign(0,0,0);
	RecoilCooldown = 0;
	m_fPlayerRotation      = 0;
	m_fPlayerRotationTarget     = 0;
	//UpdateRotation();

	D3DXMatrixIdentity(&DrawRotMatrix);
	D3DXMatrixIdentity(&DrawFullMatrix);
	D3DXMatrixIdentity(&DrawFullMatrix_Localized);
	D3DXMatrixIdentity(&MoveMatrix);

	JumpVelocity = 0;
	m_StaminaPenaltyTime = 0;
	//m_BreathPenaltyTime = 0;

	m_LastKillTime = 0;
	m_RemainingSecondWindTime = 0.0f;
	m_isHoldingBreath = false;

	bodyAdjust_x    = 0;
	bodyAdjust_y[0] = 0;
	bodyAdjust_y[1] = 0;

	RealAcceleration    = r3dPoint3D(0, 0, 0);
	InputAcceleration    = r3dPoint3D(0, 0, 0);
	SetVelocity(r3dPoint3D(0, 0, 0));

	PlayerState	= PLAYER_IDLE;
	PlayerMoveDir   = 0;
	// we pack both vars into single byte in PKT_C2C_MoveRel.state, so check for overflow
	//COMPILE_ASSERT(PLAYER_NUM_STATES <= 0xF);
	COMPILE_ASSERT(CUberData::ANIMDIR_COUNT <= 0xF);

	m_HitMarkerFadeout = 0;
	m_HitMarkerTex = NULL;

	UpdateTransform();
	return TRUE;
}

BOOL obj_Player::OnCreate()
{
	R3DPROFILE_FUNCTION("obj_Player::OnCreate");

	R3D_LOG_TIMESPAN_START(Ai_Player_OnCreate);

	parent::OnCreate();

	r3d_assert(PhysXObstacleIndex == -1);
	if(!NetworkLocal)
	{
		// create obstacle for network players, so we can't bump to them.
		PhysXObstacleIndex = AcquirePlayerObstacle(GetPosition());
	}

#ifdef CHEATS_ENABLED
	Cheats = true;
#else
	Cheats = true;
#endif
	updatehudtime = 0;
	proneTime = 0.0f;
	bAllowToUseWeapons	= 1;
	afterRespawnTimer = 0;
	lastTimeWeaponSwitch = 0;
	TimeOfLastRespawn = r3dGetTime();
	m_AuraType = AT_NONE;
	footsteps_timeSinceLastMissingHitResult = 0;

	m_needToDrawBarricadePlacement = 0;
	m_BarricadeMeshPlacement = NULL;
	m_canPlaceConsumableItem = false;

	CurLoadout.isVisible = true;
	m_SpeedBoost = 0;
	m_SpeedBoostTime = 0;
	m_MorhineShotTime =0;
	m_BandagesEffectTime = 0;
	bPhys = true;
	CurrentSpread = 0;

	lastSendWpnLog = r3dGetTime();
	lastSendStateLog = r3dGetTime();
	if(NetworkLocal)
	{
		// otherwise when you are in scope mode you might not see your own player, and so scope will not render. Anyway, we should see our player always
		setSkipOcclusionCheck(true);
		ObjFlags |=	OBJFLAG_ForceSceneBoxBBox | OBJFLAG_AlwaysDraw; 
		PhysicsConfig.group = PHYSCOLL_CHARACTERCONTROLLER;
		PhysicsConfig.type = PHYSICS_TYPE_CONTROLLER;
		PhysicsConfig.mass = 100.0f;
		PhysicsConfig.ready = true;
		PhysicsObject = BasePhysicsObject::CreateCharacterController(PhysicsConfig, this);

		if(s_laserPointerTex == 0)
		{
			s_laserPointerTex = r3dRenderer->LoadTexture("data/shaders/texture/laserPointerDot.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem );
			r3d_assert(s_laserPointerTex!=NULL);
		}

		if(s_flashlightFlashTex == 0)
		{
			s_flashlightFlashTex = r3dRenderer->LoadTexture("data/shaders/texture/Light_Flashlight.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
			r3d_assert(s_flashlightFlashTex!=NULL);
		}

		if(s_laserFlashTex == 0)
		{
			s_laserFlashTex = r3dRenderer->LoadTexture("data/shaders/texture/Light_Laser.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
			r3d_assert(s_laserFlashTex!=NULL);
		}

		if(s_lightHaloTex == 0)
		{
			s_lightHaloTex = r3dRenderer->LoadTexture("data/shaders/texture/Light_Halo_01.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
			r3d_assert(s_lightHaloTex!=NULL);
		}

		m_HitMarkerTex = r3dRenderer->LoadTexture("data/weapons/hitmarker.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem );
		r3d_assert(m_HitMarkerTex != NULL);
	}
	else
	{
		PhysicsConfig.group = PHYSCOLL_CHARACTERCONTROLLER;
		PhysicsConfig.type = PHYSICS_TYPE_RAYCAST_BOX;
		PhysicsConfig.mass = 100.0f;
		// NOTE : if you change this, sync with PHYSICS_TYPE_CONTROLLER offset...
		PhysicsConfig.offset = r3dPoint3D( 0, 0.9f, 0 );
		PhysicsObject = BasePhysicsObject::CreateDynamicObject(PhysicsConfig, this);
		//wchar_t tempStr[64];
		//char plrUserName[64]; GetUserName(plrUserName);
		//swprintf(tempStr, 64, gLangMngr.getString("$HUD_Msg_JoinedGame"), utf8ToWide(plrUserName));
		//if(hudMain)
		//	hudMain->showMessage(tempStr);
	}

	m_Stamina = getMaxStamina();
	m_Breath = getMaxBreath();

	// init animation
	uberEquip_ = new CUberEquip(this);
	r3d_assert(AI_Player_UberData);
	uberAnim_ = new CUberAnim(this, AI_Player_UberData);

	boneId_Bip01_Spine1    = uberAnim_->GetBoneID("Bip01_Spine1");
	boneId_Bip01_Spine2    = uberAnim_->GetBoneID("Bip01_Spine2");
	boneId_Bip01_Neck      = uberAnim_->GetBoneID("Bip01_Neck");
	boneId_Bip01_LUpperArm = uberAnim_->GetBoneID("Bip01_L_UpperArm");
	boneId_Bip01_RUpperArm = uberAnim_->GetBoneID("Bip01_R_UpperArm");
	boneId_Bip01_Head      = uberAnim_->GetBoneID("Bip01_Head");
	boneId_Bip01_R_Hand	   = uberAnim_->GetBoneID("Bip01_R_Hand");
	boneId_Bip01_L_Hand	   = uberAnim_->GetBoneID("Bip01_L_Hand");

	UpdateLoadoutSlot(CurLoadout);

	PlayerState = PLAYER_IDLE;
	SyncAnimation(true);

	// force animation update here, so that phys skeleton can be updated, otherwise creating too many of them at 0,0,0 causing physX to crash (too many pairs created)
	{
		D3DXMATRIX mr;
		D3DXMatrixRotationY(&mr, R3D_PI);
		D3DXMATRIX CharDrawMatrix = mr;

		UpdateAnimations(CharDrawMatrix);
		UpdateRotation();
	}

	// create phys skeleton after we have animation data
	if(!m_disablePhysSkeleton)
	{
		m_PhysSkeleton = AquireCacheSkeleton() ;
		m_PhysSkeleton->linkParent(GetSkeleton(), DrawFullMatrix, this, NetworkLocal ? PHYSCOLL_LOCALPLAYER : PHYSCOLL_NETWORKPLAYER) ;
		m_PhysSkeleton->SwitchToRagdoll(false);
	}

	// if we was dead on spawn
	if(bDead) 
	{
		r3d_assert(!NetworkLocal);

		PlayerState = PLAYER_DIE;
		uberAnim_->AnimPlayerState = PLAYER_DIE;
		uberAnim_->StartDeathAnim();
		DoRagdoll(true);

		// fucking physx
		SetPosition(GetPosition());
	}

	// load local player sounds
	if(NetworkLocal)
	{
		m_sndBreathSprint = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_BREATHE_F_SLOW":"Sounds/WarZ/PlayerSounds/PLAYER_BREATHE_M_SLOW"), GetPosition(), true);
		SoundSys.Stop(m_sndBreathSprint);
		r3d_assert(m_sndBreathSprint);
		SoundSys.GetProperty(m_sndBreathSprint, FMOD_EVENTPROPERTY_VOLUME, &m_sndBreathBaseVolume);

		m_sndInPain = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_in_pain_F":"Sounds/WarZ/PlayerSounds/PLAYER_in_pain_M"), GetPosition(), true);
		SoundSys.Stop(m_sndInPain);
		r3d_assert(m_sndInPain);

		m_sndClothMovementSprint = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_PANTS_MOVE"), GetPosition(), true);
		SoundSys.Stop(m_sndClothMovementSprint);
		r3d_assert(m_sndClothMovementSprint);

		m_sndSniperBreath = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_BREATHE_F":"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_BREATHE_M"), GetPosition(), true);
		SoundSys.Stop(m_sndSniperBreath);
		r3d_assert(m_sndSniperBreath);
		m_sndSniperHeartSlow = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_HEART_SLOW"), GetPosition(), true);
		SoundSys.Stop(m_sndSniperHeartSlow);
		r3d_assert(m_sndSniperHeartSlow);
		m_sndSniperExhale = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_EXHALE_F":"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_EXHALE_M"), GetPosition(), true);
		SoundSys.Stop(m_sndSniperExhale);
		r3d_assert(m_sndSniperExhale);
		m_sndVehicleDrive = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggyEngineLoop"), GetPosition(), true);
		SoundSys.Stop(m_sndVehicleDrive);
		r3d_assert(m_sndVehicleDrive);
	}

	lastWeapDataRep = r3dGetTime();

	TeleportPlayer(GetPosition(), "OnCreate");

	prevPos = GetPosition();

	// for fucking physx glich
	/*PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
	// if player is fucking flying on objects get him down to terrain.
	if(!g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 1.00f, PxSceneQueryFlags(PxSceneQueryFlag::eDISTANCE), hit, filter) && !bSwim)
	{
	PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
	bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 500.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);

	if( hitResult )
	SetPosition(r3dPoint3D(hit.impact.x,hit.impact.y,hit.impact.z));
	else // holy shits he get fucking through world? okay get him to terrain
	{
	if (Terrain)
	{
	SetPosition(r3dPoint3D(GetPosition().x,Terrain->GetHeight(GetPosition()),GetPosition().z));
	}
	}
	}*/

	R3D_LOG_TIMESPAN_END(Ai_Player_OnCreate);

	return TRUE;
}
BOOL obj_Player::OnDestroy()
{
	//wchar_t tempStr[64];
	//char plrUserName[64]; GetUserName(plrUserName);
	//swprintf(tempStr, 64, gLangMngr.getString("$HUD_Msg_LeftGame"), utf8ToWide(plrUserName));
	//if(hudMain)
	//	hudMain->showMessage(tempStr);

	m_BarricadeMeshPlacement = NULL;

	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; ++i)
	{
		SAFE_DELETE(m_Weapons[i]);
	}

	ReleasePlayerObstacle(&PhysXObstacleIndex);	// player is gone, clear obstacle

	if(NetworkLocal)
	{
		if(gClientLogic().localPlayer_ == this)
			gClientLogic().localPlayer_ = 0;

		r3dRenderer->DeleteTexture(s_laserPointerTex);
		s_laserPointerTex = NULL;

		r3dRenderer->DeleteTexture(s_flashlightFlashTex);
		s_flashlightFlashTex = NULL;

		r3dRenderer->DeleteTexture(s_laserFlashTex);
		s_laserFlashTex = NULL;

		r3dRenderer->DeleteTexture(s_lightHaloTex);
		s_lightHaloTex = NULL;
	}

	SAFE_DELETE(uberAnim_);
	SAFE_DELETE(uberEquip_);

	ReleaseCachedPhysSkeleton( m_PhysSkeleton ) ;
	m_PhysSkeleton = 0 ;

	for(uint32_t i=0; i<footStepsArr.Count(); ++i) 
	{
		SoundSys.Stop(footStepsArr[i].snd);
		SoundSys.Release(footStepsArr[i].snd);
	}
	footStepsArr.Clear();

	if(NetworkLocal)
	{
		SoundSys.Stop(m_sndBreathSprint); SoundSys.Release(m_sndBreathSprint);
		SoundSys.Stop(m_sndInPain); SoundSys.Release(m_sndInPain);
		SoundSys.Stop(m_sndClothMovementSprint); SoundSys.Release(m_sndClothMovementSprint);

		SoundSys.Stop(m_sndSniperBreath); SoundSys.Release(m_sndSniperBreath);
		SoundSys.Stop(m_sndSniperHeartSlow); SoundSys.Release(m_sndSniperHeartSlow);
		SoundSys.Stop(m_sndSniperExhale); SoundSys.Release(m_sndSniperExhale);
		SoundSys.Stop(m_sndVehicleDrive); SoundSys.Release(m_sndVehicleDrive);
	}

	if (hudTrade)
	{
		if (hudTrade->plr == this)
		{
			hudTrade->plr = NULL;
			hudTrade->Deactivate();
		}
	}

	if(hudMain)
	{
		if(!m_CharIcon.IsUndefined())
			hudMain->removeUserIcon(m_CharIcon);
		{
			/*if (gClientLogic().localPlayer_->CurLoadout.GroupID == CurLoadout.GroupID && CurLoadout.GroupID != 0)
			{
			hudMain->showMessage(gLangMngr.getString("Player in group left"));
			char plrUserName[64]; GetUserName(plrUserName);
			hudMain->removeplayerfromgroup(plrUserName,false);
			//gClientLogic().ShowCustomerID = 0;
			}*/
		}
	}

	return parent::OnDestroy();
}

float getMinimumAngleDistance(float from, float to)
{
	//make sure the angles are in range [0..360)
	const float da = 1.0f/360.0f;
	from = from - floorf(from * da)*360.0f;
	to = to - floorf(to * da)*360.0f;

	float d = to - from;
	if(d <-180.0f)	d += 360.0f;
	if(d > 180.0f)	d -= 360.0f;
	return d;
}

void Fix_Angle(float& deg)
{
	if(deg < -R3D_PI)
		deg += R3D_PI * 2;
	else if(deg >= R3D_PI)
		deg -= R3D_PI * 2;
}

void obj_Player::UpdateRotationToTarget(float fY, float fTurnSpeed)
{
	// if don't need to rotate
	if(R3D_ABS(fY) < 0.2f|| R3D_ABS(fY) > 120)
	{
		m_fPlayerRotation = m_fPlayerRotationTarget;
		uberAnim_->StopTurnInPlaceAnim();
		return;
	}

	float fTimePassed = r3dGetFrameTime();

	// turn character to the needed angle
	float step = fTurnSpeed * fTimePassed;

	m_fPlayerRotation += fY < 0.0f ? -step : +step;

	// check if we finished rotation, overrotated
	float fY2 = getMinimumAngleDistance(m_fPlayerRotation, m_fPlayerRotationTarget);
	if((fY > 0 && fY2 <= 0) || (fY < 0 && fY2 >= 0) 

		|| 
		// don't overreact - can get to any angle with this sort of step anyway (happens on low fps)
		step >= 180.f )
	{
		m_fPlayerRotation = m_fPlayerRotationTarget;
		uberAnim_->StopTurnInPlaceAnim();
	}
}

R3D_FORCEINLINE void UpdateBodyAdjustX( float* ioAdjust, float newadjust, float dt )
{
	float bodyAdjust_x = *ioAdjust ;
	bool was_less = bodyAdjust_x < newadjust ;

	bodyAdjust_x += (newadjust - bodyAdjust_x) * dt * 4.0f;

	if( was_less )
	{
		if( bodyAdjust_x > newadjust )
			bodyAdjust_x = newadjust ;
	}
	else
	{
		if( bodyAdjust_x < newadjust )
			bodyAdjust_x = newadjust ;
	}

	*ioAdjust = bodyAdjust_x ;
}

void obj_Player::UpdateUpperBodyAngLegs()
{
	float fTimePassed = r3dGetFrameTime();
	const float fMaxBodyAngle = R3D_DEG2RAD(45.0f);

	// turn firstly upper body only and only then legs
	float fY = getMinimumAngleDistance(m_fPlayerRotation, m_fPlayerRotationTarget);
	float newadjust = R3D_DEG2RAD(fY);

	UpdateBodyAdjustX( &bodyAdjust_x, newadjust, fTimePassed ) ;

	if(NetworkLocal && g_camera_mode->GetInt()==2)
	{
		UpdateRotationToTarget(fY, _ai_fTurnSpeedIdle);
	}
	else
	{
		if(uberAnim_->turnInPlaceTrackID == CUberAnim::INVALID_TRACK_ID)
		{
			if(R3D_ABS(bodyAdjust_x) > fMaxBodyAngle)
			{
				uberAnim_->StartTurnInPlaceAnim();
				bodyAdjust_x = R3D_CLAMP(bodyAdjust_x, -fMaxBodyAngle, fMaxBodyAngle);
			}
		}
		else
		{
			UpdateRotationToTarget(fY, _ai_fTurnSpeedIdle);
			// set new body adjust after turning body
			float fY = getMinimumAngleDistance(m_fPlayerRotation, m_fPlayerRotationTarget);
			float newadjust = R3D_DEG2RAD(fY);
			bodyAdjust_x = newadjust;
		}
	}
}

void obj_Player::UpdateRotation()
{
	R3DPROFILE_FUNCTION("UpdateRotation");

	uberAnim_->UpdateTurnInPlaceAnim();

	float fPrevRotation = m_fPlayerRotation;
	float fTimePassed = r3dGetFrameTime();
	if(PlayerState == PLAYER_IDLE || (PlayerState == PLAYER_MOVE_CROUCH && PlayerMoveDir == CUberData::ANIMDIR_Stand))
	{
		UpdateUpperBodyAngLegs();
	}
	else
	{
		uberAnim_->StopTurnInPlaceAnim();

		UpdateBodyAdjustX(&bodyAdjust_x, 0.f, fTimePassed * 4.0f);

		float fY = getMinimumAngleDistance(m_fPlayerRotation, m_fPlayerRotationTarget);
		UpdateRotationToTarget(fY, _ai_fTurnSpeedWalk);
	}

	// lerp body bending to target
	if(bodyAdjust_y[0] < bodyAdjust_y[1]) {
		bodyAdjust_y[0] = R3D_MIN(bodyAdjust_y[0] + fTimePassed * 3.2f, bodyAdjust_y[1]);
	} else if(bodyAdjust_y[0] > bodyAdjust_y[1]) {
		bodyAdjust_y[0] = R3D_MAX(bodyAdjust_y[0] - fTimePassed * 3.2f, bodyAdjust_y[1]);
	} else
		bodyAdjust_y[0] = bodyAdjust_y[1];

	// create rotation matrix
	D3DXMatrixRotationYawPitchRoll(&MoveMatrix, R3D_DEG2RAD(m_fPlayerRotation) + bodyAdjust_x, 0, 0);
	D3DXMatrixRotationYawPitchRoll(&DrawRotMatrix, R3D_DEG2RAD(m_fPlayerRotation), 0, 0);

	r3dPoint3D pos = GetPosition();

	extern int CurHUDID;
	if(g_camera_mode->GetInt()==2 && NetworkLocal && ((g_bEditMode && (CurHUDID==1 || CurHUDID==6) )||!g_bEditMode) && !bDead)
	{
		float hands_height = 0.0f;
		{
			D3DXMATRIX tmp, res;
			D3DXMatrixIdentity(&tmp);
			uberAnim_->anim.GetCurrentSkeleton()->GetBoneWorldTM(boneId_Bip01_Head, &res, tmp);
			hands_height = res._42;
		}
		// weapon movement when turning
		D3DXMATRIX m3, m4;
		float limit = 2.0f;
		if(m_isAiming)
			limit = 0.5f;
		// when firing, don't delay movement
		if(m_Weapons[m_SelectedWeapon] && (r3dGetTime() < m_Weapons[m_SelectedWeapon]->getLastTimeFired() + 0.5f))
			limit = 0.0f;
		float fRot = -R3D_CLAMP(getMinimumAngleDistance(fPrevRotation, m_fPlayerRotation)*10.0f, -limit, limit);
		static float EMAPrev = 0.0f;

		float EMACur = EMAPrev + 0.20f * (fRot - EMAPrev); // exponential moving average of about 15 prev.values
		EMAPrev = EMACur;

		D3DXMatrixRotationYawPitchRoll(&m3, R3D_DEG2RAD(EMACur), 0, 0);

		D3DXMatrixTranslation(&m4, 0, -hands_height, 0.1f);
		D3DXMatrixMultiply(&DrawFullMatrix, &m3, &m4);

		D3DXMatrixMultiply(&DrawFullMatrix_Localized, &DrawFullMatrix, &r3dRenderer->InvViewMatrix_Localized);		

		D3DXMatrixMultiply(&DrawFullMatrix, &DrawFullMatrix, &r3dRenderer->InvViewMatrix);
	}
	else
	{
		DrawFullMatrix = DrawRotMatrix;

		DrawFullMatrix._41 += pos.x;
		DrawFullMatrix._42 += pos.y;
		DrawFullMatrix._43 += pos.z;

		DrawFullMatrix_Localized = DrawRotMatrix;

		DrawFullMatrix_Localized._41 += pos.x - r3dRenderer->LocalizationPos.x;
		DrawFullMatrix_Localized._42 += pos.y - r3dRenderer->LocalizationPos.y;
		DrawFullMatrix_Localized._43 += pos.z - r3dRenderer->LocalizationPos.z;
	}

	// set physx rotation vectors
	SetRotationVector(r3dVector(m_fPlayerRotation, 0, 0));
}

static bool updateShootingTarget(obj_Player* pl, const r3dPoint3D& shootFrom, const r3dPoint3D& muzzlePos, r3dPoint3D& sPos, r3dPoint3D& sNorm, r3dMaterial** sMat, GameObject** gameObj, const char** actorName, float spread, bool recheckCollisionAgainsMuzzler)
{
	r3d_assert(pl);
	r3d_assert(sMat);
	r3d_assert(gameObj);

	const float MAX_CASTING_DISTANCE = 20000.0f;
	r3dPoint3D dir;
	if(pl->m_isInScope || g_camera_mode->GetInt() != 1)
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);
	else
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH*0.32f, &dir);

	*sMat = NULL;
	*gameObj = NULL;

	// convert spread from diameter at 50 meter range to radius at 1meter range
	spread = (spread*0.5f)/50.0f; // 0.5 - because spread initially set as diameter, and we need radius. 50-because spread is set at 50 meter range. formula is: tanA=opp/adj = so that is angle that we need to feed into raycast

	// limit spread to prevent ppl shooting behind, etc.
	spread = R3D_CLAMP(spread, -0.5f, 0.5f); 
	//r3dOutToLog("updateShootingTarget Spread %.2f\n",spread);
	D3DXMATRIX rotMat;
	D3DXMatrixRotationYawPitchRoll(&rotMat, u_GetRandom(-spread, spread), u_GetRandom(-spread, spread), u_GetRandom(-spread, spread));
	r3dPoint3D dirWithSpread;
	D3DXVec3TransformNormal((D3DXVECTOR3*)&dirWithSpread, (D3DXVECTOR3*)&dir, &rotMat);
	{
		// shoot from camera :)
		PxRaycastHit hit;
		PhysicsCallbackObject* target = NULL;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
		g_pPhysicsWorld->raycastSingle(PxVec3(shootFrom.x, shootFrom.y, shootFrom.z), PxVec3(dirWithSpread.x, dirWithSpread.y, dirWithSpread.z), MAX_CASTING_DISTANCE, 
			PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eNORMAL, hit, filter);
		// check if target position is still visible from muzzler (prevent against shooting from wall edges
		if( recheckCollisionAgainsMuzzler && hit.shape )
		{
			PxVec3 dir2(hit.impact.x - muzzlePos.x, hit.impact.y - muzzlePos.y, hit.impact.z - muzzlePos.z);
			dir2.normalize();

			PxRaycastHit hit2;
			g_pPhysicsWorld->raycastSingle(PxVec3(muzzlePos.x, muzzlePos.y, muzzlePos.z), dir2, MAX_CASTING_DISTANCE, PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eNORMAL, hit2, filter);
			if(hit2.shape != hit.shape)
			{
				// replace old collision data with new one
				hit   = hit2;
			}
		}

		if( hit.shape && (target = static_cast<PhysicsCallbackObject*>(hit.shape->getActor().userData)))
		{
			if(actorName)
				*actorName = hit.shape->getActor().getName();

			*gameObj = target->isGameObject();
			if(*gameObj)
			{
				if( (*gameObj)->isObjType( OBJTYPE_Mesh ) )
					*sMat = static_cast< MeshGameObject* > ( target )->MeshLOD[ 0 ]->GetFaceMaterial( hit.faceIndex );
			}
			else if(target->hasMesh())
			{
				*sMat = target->hasMesh()->GetFaceMaterial( hit.faceIndex );
			}
			if (!*sMat)
				*sMat = target->GetMaterial(hit.faceIndex);

			sPos.x = hit.impact.x;
			sPos.y = hit.impact.y;
			sPos.z = hit.impact.z;

			sNorm.x = hit.normal.x;
			sNorm.y = hit.normal.y;
			sNorm.z = hit.normal.z;

			return true;
		}
	}

	sPos = muzzlePos + dir * 500.0f;
	sNorm = -dir;

	// do not allow to shoot behind you, like when there a pole between your character and camera
	{
		r3dPoint3D dir2 = (sPos - muzzlePos).Normalize();
		if(dir.Dot(dir2) < 0.0f)
		{
			*sMat = NULL;
			*gameObj = NULL;
		}
	}

	return false;
}

static bool updateMeleeTarget(obj_Player* pl, const r3dPoint3D& shootFrom, const r3dPoint3D& muzzlePos, r3dPoint3D& sPos, r3dPoint3D& sNorm, r3dMaterial** sMat, GameObject** gameObj, const char** actorName, float spread, bool recheckCollisionAgainsMuzzler)
{
	r3d_assert(pl);
	r3d_assert(sMat);
	r3d_assert(gameObj);

	*sMat = NULL;
	*gameObj = NULL;

	// for melee we need to check if we hit anyone in front of us. PhysX doesn't allow to do this check easily (overlaps do not return hit information, just shapes), so instead 
	// I'm going to do 9 ray casts in front of player. They are super short, so it shouldn't cause any performance issues 
	// and we going to hit first player, if any
	// TODO: we need to handle a case when we hit static object only and do not hit any player, but for melee this is minor
	const float MAX_CASTING_DISTANCE = 2.0f;
	float rayDegrees[9] = {-30.0f, -20.0f, -10.0f, -5.0f, 0.0f, 5.0f, 10.0f, 20.0f, 30.0f};
	for(int iRay = 0; iRay <9; ++iRay)
	{
		D3DXMATRIX rotMat;
		D3DXMatrixRotationYawPitchRoll(&rotMat, R3D_DEG2RAD(rayDegrees[iRay]), 0, 0);
		r3dPoint3D dirWithSpread;
		D3DXVec3TransformNormal((D3DXVECTOR3*)&dirWithSpread, (D3DXVECTOR3*)&gCam.vPointTo/*(D3DXVECTOR3*)&pl->GetvForw()*/, &rotMat);

		{
			PxRaycastHit hit;
			PhysicsCallbackObject* target = NULL;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
			g_pPhysicsWorld->raycastSingle(PxVec3(shootFrom.x, shootFrom.y, shootFrom.z), PxVec3(dirWithSpread.x, dirWithSpread.y, dirWithSpread.z), MAX_CASTING_DISTANCE, 
				PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eNORMAL, hit, filter);

			if( hit.shape && (target = static_cast<PhysicsCallbackObject*>(hit.shape->getActor().userData)))
			{
				*gameObj = target->isGameObject();
				if(!*gameObj)
					continue;
				if(!((*gameObj)->isObjType(OBJTYPE_Human) || (*gameObj)->isObjType(OBJTYPE_Zombie) || (*gameObj)->Class->Name == "obj_ZombieDummy" || (*gameObj)->Class->Name == "obj_Animals" /*|| (*gameObj)->Class->Name == "obj_Building"*/))
					continue;

				*sMat = NULL;

				if(actorName)
					*actorName = hit.shape->getActor().getName();

				sPos.x = hit.impact.x;
				sPos.y = hit.impact.y;
				sPos.z = hit.impact.z;

				sNorm.x = hit.normal.x;
				sNorm.y = hit.normal.y;
				sNorm.z = hit.normal.z;

				return true;
			}
		}
	}

	r3dPoint3D dir;
	if(pl->m_isInScope || g_camera_mode->GetInt() != 1)
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);
	else
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH*0.32f, &dir);

	sPos = muzzlePos + dir * 500.0f;
	sNorm = -dir;

	// do not allow to shoot behind you, like when there a pole between your character and camera
	{
		r3dPoint3D dir2 = (sPos - muzzlePos).Normalize();
		if(dir.Dot(dir2) < 0.0f)
		{
			*sMat = NULL;
			*gameObj = NULL;
		}
	}

	return false;
}

static bool isMuzzlerBlocked(obj_Player* pl, const r3dPoint3D& muzzlePos)
{
	// check if muzzler is visible, and if not (inside of wall) then we do not shoot anything
	bool muzzlerBlocked = false;
	{
		r3dVector temp = muzzlePos-gCam;
		float len = temp.Length();
		temp.Normalize();
		if(len > 0)
		{
			PxRaycastHit hit;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eDYNAMIC|PxSceneQueryFilterFlag::eSTATIC));
			muzzlerBlocked = g_pPhysicsWorld->raycastSingle( PxVec3(gCam.x,gCam.y,gCam.z), PxVec3(temp.x, temp.y, temp.z), len, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
		}
	}
	if(muzzlerBlocked)
		return true;

	const float MAX_CASTING_DISTANCE = 20000.f;
	r3dPoint3D dir;
	if(pl->m_isInScope || g_camera_mode->GetInt() != 1)
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);
	else
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH*0.32f, &dir);

	// check if camera->shoot_target position agains camera->muzzler (disable shooting when close to wall, but looking at step edge)
	float distToMuzzler = (gCam-muzzlePos).Length();
	{
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eDYNAMIC|PxSceneQueryFilterFlag::eSTATIC));
		if(g_pPhysicsWorld->raycastSingle(PxVec3(gCam.x, gCam.y, gCam.z), PxVec3(dir.x, dir.y, dir.z), MAX_CASTING_DISTANCE, PxSceneQueryFlags(PxSceneQueryFlag::eDISTANCE), hit, filter))
		{
			if(hit.distance < distToMuzzler)
			{
				// shooting blocked before muzzler
				return true;
			}
		}
	}

	// for check some crap shit player shoot in colision but muzzler and gcam is outside of colision.

	// this game writen by idiot
	{
		r3dVector temp = pl->GetPosition()-muzzlePos;
		float len = temp.Length();
		temp.Normalize();
		if(len > 0)
		{
			PxRaycastHit hit;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eDYNAMIC|PxSceneQueryFilterFlag::eSTATIC));
			muzzlerBlocked = g_pPhysicsWorld->raycastSingle( PxVec3(muzzlePos.x,muzzlePos.y,muzzlePos.z), PxVec3(temp.x, temp.y, temp.z), len, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
		}
	}
	if(muzzlerBlocked)
		return true;

	return false;
}

void obj_Player::UpdateTargetting()
{
	R3DPROFILE_FUNCTION("UpdateTargetting");
	r3d_assert(NetworkLocal);

	m_LocalPlayer_CurrentAimAt = invalidGameObjectID;

	r3dMaterial* temp;
	GameObject* tempObj;
	r3dPoint3D muzzlerPos = GetMuzzlerPosition();

	{
		r3dPoint3D tmpPos, tmpNorm;
		// we always accept this position now, sometimes this jiggles, if that happens we'll have to look at the angle of the aiming and maybe factor out parts of it. 
		updateShootingTarget(this, gCam, muzzlerPos, tmpPos, tmpNorm, &temp, &tempObj, NULL, 0.0f, false);

		{
			viewTargetPos = tmpPos;
			viewTargetNorm = tmpNorm;
		}
	}

	const char* reticleColor = "white";
	if(tempObj)
	{
		m_LocalPlayer_CurrentAimAt = tempObj->GetSafeID();
		if(tempObj->isObjType(OBJTYPE_Human))
		{
			obj_Player* target = (obj_Player*)tempObj;
			if(!target->bDead)
			{
				reticleColor = "red";
			}
		}
	}

	r3dPoint3D bendTarget = GetPosition();
	bendTarget.Y += Height * 0.9f;
	bendTarget   += GetvForw() * 0.2f;

	r3dPoint3D v = (viewTargetPos - bendTarget);
	float dist = v.Length();
	float bendUp = asinf(v.y / v.Length());
	if(dist<1.8f)
		bendUp = R3D_LERP(0.0f, bendUp, dist/1.8f);

	// do not bend body in prone position
	if(PlayerState == PLAYER_PRONE_DOWN || PlayerState == PLAYER_PRONE_UP || PlayerState == PLAYER_MOVE_PRONE || PlayerState == PLAYER_PRONE_IDLE || PlayerState == PLAYER_PRONE_AIM)
		bendUp = 0;

	bodyAdjust_y[1] = bendUp;

#ifndef FINAL_BUILD
	if( g_char_spect->GetInt() )
	{
		bodyAdjust_y[1] = 0 ;
	}
#endif
}

r3dPoint3D obj_Player::GetMuzzlerPosition()
{
	if(GetSkeleton() && m_Weapons[m_SelectedWeapon])
	{
		return m_Weapons[m_SelectedWeapon]->getMuzzlePos(uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix));
	}

	return GetPosition() + GetvForw() * 1.0f + r3dPoint3D(0, 0.5f, 0);
}

// 0 - body
// 1 - head
// 2 - hands
// 3 - legs
void obj_Player::UpdateHud()
{
	if (hudMain)
	{
		ObjectManager& GW = GameWorld();

		obj_Player* plr = NULL;
		obj_Zombie* z = NULL;
		float dist = 100;
		for (GameObject *targetObj = GW.GetFirstObject(); targetObj; targetObj = GW.GetNextObject(targetObj))
		{
			if(targetObj->isObjType(OBJTYPE_Human) && (targetObj->GetPosition() - GetPosition()).Length() <= dist && ((obj_Player*)targetObj) != this)
			{
				obj_Player* p = (obj_Player*)targetObj;

				// friends filters

				if (p->ClanID == ClanID && ClanID != 0) continue;

				if (p->CurLoadout.GroupID == CurLoadout.GroupID && CurLoadout.GroupID != 0) continue;

				plr = (obj_Player*)targetObj;
				dist = (targetObj->GetPosition() - GetPosition()).Length();
			}

			if(targetObj->isObjType(OBJTYPE_Zombie) && (targetObj->GetPosition() - GetPosition()).Length() <= dist && !((obj_Zombie*)targetObj)->bDead)
			{
				z = (obj_Zombie*)targetObj;
				dist = (targetObj->GetPosition() - GetPosition()).Length();
			}
		}
		if (plr || z)
			hudMain->setThreatValue((100 - (int)dist));
		else
			hudMain->setThreatValue(0);
	}
}
BYTE obj_Player::getBodyNameFromBoneName(const char* boneName)
{
	struct boneToBodyPart_s {
		BYTE part;
		const char* bone;
	};
	const static boneToBodyPart_s map[] = {
		{1, "Bip01_Head"},
		{2, "Bip01_L_UpperArm"},
		{2, "Bip01_L_Forearm"},
		{2, "Bip01_L_Hand"},
		{2, "Bip01_R_UpperArm"},
		{2, "Bip01_R_Forearm"},
		{2, "Bip01_R_Hand"},
		{3, "Bip01_L_Thigh"},
		{3, "Bip01_L_Calf"},
		{3, "Bip01_L_Foot"},
		{3, "Bip01_L_Toe0"},
		{3, "Bip01_R_Thigh"},
		{3, "Bip01_R_Calf"},
		{3, "Bip01_R_Foot"},
		{3, "Bip01_R_Toe0"},
	};

	// not optimal, but numbers CAN change, because of skeleton modification
	for(int i=0; i<R3D_ARRAYSIZE(map); i++)
	{
		if(strcmp(map[i].bone, boneName) == 0) {
#ifndef FINAL_BUILD
			r3dOutToLog("%s mapped to %d\n", boneName, map[i].part);
#endif
			return map[i].part;
		}
	}

	return 0;
}

void obj_Player::CheckFireWeapon()
{

	r3d_assert(NetworkLocal);

	if (bSwim || bSwimShift || isInVehicle())
		return;

	VMPROTECT_BeginMutation("obj_Player::CheckFireWeapon");

	bool needExit = false;
#ifndef FINAL_BUILD
	if(d_video_spectator_mode->GetBool()) needExit = true;
	if(d_observer_mode->GetBool()) needExit = true;
#endif
	if(hudPause && hudPause->isActive()) needExit = true;
	if(hudAttm && hudAttm->isActive()) needExit = true;
	if(hudGeneralStore && hudGeneralStore->isActive()) needExit = true;
	if(hudVault && hudVault->isActive()) needExit = true;
	//if(hudTrade && hudTrade->isActive()) needExit = true;
	if(Mouse->GetMouseVisibility()) needExit = true;
	if(hudMain && hudMain->isChatInputActive()) needExit = true;
	if(hudMain && hudMain->isPlayersListVisible()) needExit = true;

	static bool wasReloading = false; // static is okay, this code is running for local player only
	lifeProperties.weaponFired = false;

	Weapon* wpn = m_Weapons[m_SelectedWeapon];
	/*	if(wpn != NULL && wpn->getItemID() != WeaponConfig::ITEMID_Flashlight && wpn->m_FlashlightParticle && wpn->m_Flashlight.IsOn())
	{
	wpn->m_FlashlightParticle->ObjFlags |= OBJFLAG_SkipDraw;
	wpn->m_Flashlight.TurnOff();
	}*/
	if((wpn == NULL || forcedEmptyHands))
	{
		/*if (InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_PRIMARY_FIRE) && bAllowToUseWeapons)
		{
		uberAnim_->StartHands();
		/*int aid = 0;
		aid = CUberData().AddAnimation("Stand_Shooting_MEL_Hands_R_Punch");
		uberAnim_->anim.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.2f);
		}
		if (!needExit)
		{
		if(g_toggle_aim->GetBool())
		{
		if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_AIM))
		m_isAiming = !m_isAiming;
		}
		else
		{
		m_isAiming = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_AIM);
		}
		}*/
		return;
	}

	if (gClientLogic().m_gameInfo.ispre && wpn->getCategory()==storecat_SNP)
		return;

	if((PlayerState == PLAYER_MOVE_SPRINT && wpn->getCategory()!=storecat_MELEE) || bDead)  
	{
		if(wasReloading)
		{
			wasReloading = false;
		}
		needExit = true;
	}
	if(PlayerState == PLAYER_PRONE_DOWN || PlayerState == PLAYER_PRONE_UP)
		needExit = true;

	float gamepadLeftTrigger, gamepadRightTrigger;
	Gamepad->GetTrigger(gamepadLeftTrigger, gamepadRightTrigger);

	bool wpnIsNoLongerAvailable = false; // workaround for the fast that when you use last item, it will call OnBackpackChanged and will DELETE current weapon that you are using
	// not sure what is the proper way to fix this. Use smart pointers maybe?? 
	if(wpn->isUsableItem() && !needExit)
	{
		// special case items
		if(wpn->getItemID() == WeaponConfig::ITEMID_Binoculars || wpn->getItemID() == WeaponConfig::ITEMID_RangeFinder) // binoculars, range finder, etc
		{
			bool oldAiming = m_isAiming;
			if(g_toggle_aim->GetBool())
			{
				if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_AIM))
					m_isAiming = !m_isAiming;
			}
			else
			{
				m_isAiming = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_AIM) || gamepadLeftTrigger > 0;
			}

			extern 	PlayerStateVars_s CurrentRig;
			m_isInScope = hasScopeMode() && CurrentRig.allowScope;

			// remember that aiming was changed
			if(m_isAiming != oldAiming)
				m_isChangedAiming1 = true;

			extern float LerpValue;
			m_isFinishedAiming = m_isAiming && LerpValue==1.0f;

			if(wpn->getItemID() == 101319 && hudMain)
				hudMain->showRangeFinderUI(m_isFinishedAiming && m_isInScope);
		}
		else if(wpn->getItemID() == WeaponConfig::ITEMID_PieceOfPaper)
		{
			m_isAiming = false;
			m_isFinishedAiming = false;
			if(InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_PRIMARY_FIRE) || m_ItemSimulateLeftClick)
			{
				m_ItemSimulateLeftClick = false;
				if(hudMain && hudMain->canShowWriteNote())
					hudMain->showWriteNote(wpn->m_BackpackIdx);
			}
		}
		else if(wpn->getItemID() == WeaponConfig::ITEMID_AirHorn)
		{
			// todo
		}
		else
		{
			m_isAiming = false;
			m_isFinishedAiming = false;
			if(m_needToDrawBarricadePlacement && InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_AIM))
			{
				m_needToDrawBarricadePlacement = 0;
				m_BarricadeMeshPlacement = NULL;
			}

			// consumables
			if(InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_PRIMARY_FIRE) || m_ItemSimulateLeftClick)
			{
				m_ItemSimulateLeftClick = false;

				wiInventoryItem wi = wpn->getPlayerItem(); // COPY of item, not reference. because it can be reset in Fire()
				if(wi.quantity > 0 && wpn->isReadyToFire(true, false) && wi.itemID != 301321) // consumables have rate of fire (cooldown)
				{
					if(wpn->getItemID() == WeaponConfig::ITEMID_BarbWireBarricade || 
						wpn->getItemID() == WeaponConfig::ITEMID_WoodShieldBarricade ||
						wpn->getItemID() == WeaponConfig::ITEMID_RiotShieldBarricade ||
						wpn->getItemID() == WeaponConfig::ITEMID_SandbagBarricade)
					{
						if(m_needToDrawBarricadePlacement == 0)
						{
							m_needToDrawBarricadePlacement = wpn->getItemID();
							return;
						}
						else
						{
							if(m_canPlaceConsumableItem)
							{
								m_needToDrawBarricadePlacement = 0; // and let user place riot shield by sending packet to server
								m_BarricadeMeshPlacement = NULL;
							}
							else
								return;
						}
					}
					if (wpn->getItemID() == WeaponConfig::ITEMID_PersonalLocker)
					{

					}
					if((wpn->getItemID() == WeaponConfig::ITEMID_Bandages || wpn->getItemID() == WeaponConfig::ITEMID_Bandages2 || wpn->getItemID() == WeaponConfig::ITEMID_Antibiotics ||
						wpn->getItemID() == WeaponConfig::ITEMID_Painkillers || wpn->getItemID() == WeaponConfig::ITEMID_Medkit) 
						&& CurLoadout.Health > 99 && !CurLoadout.legfall && !CurLoadout.bleeding)
					{
						if(hudMain) hudMain->showMessage(gLangMngr.getString("InfoMsg_MaxHealthAlready"));
						return;
					}
					if((wpn->getItemID() == WeaponConfig::ITEMID_C01Vaccine || wpn->getItemID() == WeaponConfig::ITEMID_C04Vaccine) 
						&& CurLoadout.Toxic < 1.0f)
					{
						if(hudMain) hudMain->showMessage(gLangMngr.getString("InfoMsg_NoTocixInBlood"));
						return;
					}

					//const FoodConfig* fc = g_pWeaponArmory->getFoodConfig(wi.itemID);

					/*if((wpn->getItemID() == WeaponConfig::ITEMID_Bandages || wpn->getItemID() == WeaponConfig::ITEMID_Bandages2 || wpn->getItemID() == WeaponConfig::ITEMID_Antibiotics ||
					wpn->getItemID() == WeaponConfig::ITEMID_Painkillers || wpn->getItemID() == WeaponConfig::ITEMID_Medkit) && hudMain->Cooldown > 0 )
					{
					if(hudMain) hudMain->showMessage(gLangMngr.getString("Currenty Cooldown"));
					return;
					}*/
					if(hudMain->Cooldown > 0 )
					{
						if(hudMain) hudMain->showMessage(gLangMngr.getString("Currenty Cooldown"));
						return;
					}

					uberAnim_->StartShootAnim();
					wpn->Fire(r3dPoint3D(0,0,0), D3DXMATRIX(), true); // to update last fire time




					showSlotsInfoTimer = r3dGetTime() + 10.0f; // show weapon info for X seconds
					if(hudMain)
						hudMain->showSlots(true);

					// todo: we need to sync this crap over network, so that network players will see us using item
					// todo: also, we need to sync when we sent this packet, so that it is in sync with animation
					float var1 = 0.0f;
					r3dPoint3D pos       = GetPosition() + GetvForw()*1.0f + r3dPoint3D(0,1,0);
					bool needCorrectPos = false; // those objects require precise position, otherwise there will be desync between client\server (mainly spawn beacon problem)
					if(wi.itemID == WeaponConfig::ITEMID_BarbWireBarricade ||
						wi.itemID == WeaponConfig::ITEMID_WoodShieldBarricade ||
						wi.itemID == WeaponConfig::ITEMID_RiotShieldBarricade || 
						wi.itemID == WeaponConfig::ITEMID_SandbagBarricade)
					{
						var1 = (m_fPlayerRotation) + R3D_RAD2DEG(bodyAdjust_x) + 180;
						needCorrectPos = true; 
					}

					if(needCorrectPos)
					{
						PxRaycastHit hit;
						PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC));
						if(g_pPhysicsWorld->raycastSingle(PxVec3(pos.x, pos.y, pos.z), PxVec3(0,-1,0), 3.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter))
							pos = r3dPoint3D(hit.impact.x, hit.impact.y+0.1f, hit.impact.z);
					}

					if((wpn->getItemID() == WeaponConfig::ITEMID_Bandages || wpn->getItemID() == WeaponConfig::ITEMID_Bandages2 || wpn->getItemID() == WeaponConfig::ITEMID_Antibiotics ||
						wpn->getItemID() == WeaponConfig::ITEMID_Painkillers || wpn->getItemID() == WeaponConfig::ITEMID_Medkit || wpn->getItemID() == 101284) )
					{
						hudMain->currentslot = wpn->m_BackpackIdx;
						hudMain->Cooldown = r3dGetTime() + 10.0f;
					}

					localPlayer_UseItem(wpn->m_BackpackIdx, wi.itemID, pos, var1);

					m_BandagesEffectTime = r3dGetTime() + 0.5f;

					// after Fire() check new quantity
					{
						if(wpn->getConfig()->m_isConsumable)
							wi.quantity--; // simulate what fire() does
						if(wi.quantity <= 0) {
							wpnIsNoLongerAvailable = true;
							OnBackpackChanged(m_SelectedWeapon);
						}
						else
						{
							if(hudMain)
								hudMain->updateSlotInfo(m_SelectedWeapon, wi.quantity);
						}
					}

				}
			}
		}
		needExit = true;
	}

	if(!bAllowToUseWeapons)
		needExit = true;

	if(needExit)
	{
		if(!wpnIsNoLongerAvailable)
			wpn->isReadyToFire(false, false); // very important to call ready to fire with false to reset internal counter of how long trigger was pressed
		return;
	}

	/*	if (bSwim)
	{
	wpn->isReadyToFire(false, false);
	}*/

	if(wpn->getItemID() == WeaponConfig::ITEMID_Flashlight)
	{
		if (InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_TOGGLE_FLASHLIGHT))
		{
			wpn->toggleFlashlight();
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/NewWeapons/Melee/flashlight"), GetPosition());
		}
	}

	m_isPressedFireTrigger = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_PRIMARY_FIRE) || gamepadRightTrigger > 0;
	// check muzzle block except when using scope
	if(!m_isInScope && wpn->getCategory()!=storecat_MELEE && isMuzzlerBlocked(this, GetMuzzlerPosition()))
		m_isPressedFireTrigger = false;

	bool oldAiming = m_isAiming;
	if(g_toggle_aim->GetBool())
	{
		if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_AIM))
			m_isAiming = !m_isAiming;
	}
	else
	{
		m_isAiming = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_AIM) || gamepadLeftTrigger > 0;
	}

	// check if we just switched weapon and do not allow to fire
	if((r3dGetTime() - lastTimeWeaponSwitch) < 0.4f)
	{
		m_isPressedFireTrigger = false;
		m_isAiming = false;
	}

	extern 	PlayerStateVars_s CurrentRig;
	m_isInScope = hasScopeMode() && CurrentRig.allowScope;

	if(wpn->isReloading())
		m_isAiming = false;

	//if(wpn->getAnimType()== WPN_ANIM_MELEE) // no aiming in melee
	//	m_isAiming = false;

	// remember that aiming was changed
	if(m_isAiming != oldAiming)
		m_isChangedAiming1 = true;

	extern float LerpValue;
	if (wpn->getCategory() != storecat_SNP)
	{
		m_isFinishedAiming = m_isAiming && LerpValue==1.0f;
	}
	else 
	{
		m_isFinishedAiming = m_isAiming && LerpValue==1.0f && m_isInScope;
	}

	bool playerCrouching = PlayerState==PLAYER_MOVE_CROUCH || PlayerState==PLAYER_MOVE_CROUCH_AIM; 
	bool playerProning = PlayerState==PLAYER_MOVE_PRONE || PlayerState==PLAYER_PRONE_AIM || PlayerState == PLAYER_PRONE_IDLE;
	bool playerMoving  = InputAcceleration.Length() > 0.01f;
	bool playerRunning = PlayerState==PLAYER_MOVE_RUN;
	int triggerPressedCounter = wpn->getTriggerPressedCounter();

	float recoilCoolDownMod = 1.0f;
	if(triggerPressedCounter < 3)
		recoilCoolDownMod = 2.50f;
	else
		recoilCoolDownMod = 3.2f;

	// BEGIN SPREAD\RECOIL LOGIC

	if(!m_isPressedFireTrigger) // if user let go of trigger, start return recoil back to normal right away, so that it doesn't linger at the top for a second or two after stopping firing
	{
		if(wpn->getCategory() != storecat_SNP)
		{
			RecoilCooldown = 0;
		}
	}

	//[ST] trying to get higher recoil without changing database. hack :)
	recoil = wpn->getRecoil()*1.1f;
	recoil2 = recoil;
	PKT_C2S_WpnLog_s n;
	n.itemid = 0;
	if (NetworkLocal && wpn && r3dGetTime() > lastSendWpnLog)
	{
		const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(wpn->getItemID());
		lastSendWpnLog = r3dGetTime() + 2.0f;

		if (wc)
		{
			n.itemid = wpn->getItemID();
			n.m_AmmoDamage = wc->m_AmmoDamage;
			n.m_AmmoMass = wpn->getAmmoMass();
			n.m_AmmoSpeed = wpn->getAmmoSpeed();
			n.m_recoil = wpn->getRecoil();
			n.m_spread = wpn->getSpread();
			n.slot = m_SelectedWeapon;
			n.m_rateoffire = wpn->getRateOfFire();
			n.m_recoilattm = wpn->m_WeaponAttachmentStats[wpn->WPN_ATTM_STAT_RECOIL];
			n.m_spreadattm = wpn->m_WeaponAttachmentStats[wpn->WPN_ATTM_STAT_SPREAD];
			n.m_rateattm = wpn->m_WeaponAttachmentStats[wpn->WPN_ATTM_STAT_FIRERATE];
			n.m_recoil2 = recoil;
			n.m_spread2 = spread;
			//r3dOutToLog("recoil %.2f\n",recoil2);
		}
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
	}
	//r3dOutToLog("recoil %.2f\n",recoil);

	spread = CurrentSpread; // spread will be affected now by player state and other modificators

	//[ST] - just in case - not sure why we needed it
	//spread *= GPP->c_fGlobalSpreadMod;

	// Okay by default in NON ADS mode we're increasing spread
	spread *= 1.1f;
	//r3dOutToLog("spread %.2f CurrentSpread %.2f\n",spread,CurrentSpread);

	if (wpn->getCategory()==storecat_SHTG) spread *= 1.2f;


	if(playerCrouching)
	{
		//recoil *= GPP->c_fSpreadRecoilMod_Crouch;
		if(wpn->getCategory()==storecat_SHTG)
			spread *= GPP->c_fSpreadModShotgun_Crouch;
		else
			spread *= GPP->c_fSpreadRecoilMod_Crouch;
	}
	if(m_isFinishedAiming)
	{
		recoil *= GPP->c_fSpreadRecoilMod_Aim*2.0f;
		if(wpn->getCategory()==storecat_SHTG)
			spread *= GPP->c_fSpreadModShotgun_Aim;
		else
			spread *= GPP->c_fSpreadRecoilMod_Aim;
	}
	if(playerRunning)
	{
		recoil *= GPP->c_fSpreadRecoilMod_Running;
		spread *= GPP->c_fSpreadRecoilMod_Running;
	}
	//if(playerMoving)
	//{
	//	recoil *= GPP->c_fSpreadRecoilMod_Moving;
	//	spread *= GPP->c_fSpreadRecoilMod_Moving;
	//}

	/*if(wpn->getCategory() == storecat_SNP && !m_isAiming)
	{
	m_ReticleTargetScale = 25.0f;
	}*/

	// first bullet logic
	if(!playerRunning && m_isFinishedAiming && wpn->getCategory() != storecat_SHTG && bOnGround)
	{
		// spr_mod - to prevent player from shooting straight at target by just clicking on a mouse, rather than holding fire
		float spr_mod = 1.0f - R3D_CLAMP(((r3dGetTime() - wpn->getLastTimeFired())/GPP->c_fZeroSpreadDelayBetweenShots), 0.0f, 0.9f); // 0.9 - first bullet 10% spread
		// if shooting for 10 bullets straight you spread will slowly increase
		//[ST] - Removed this code
		//spread = R3D_LERP(spread*spr_mod, spread, R3D_CLAMP(float(triggerPressedCounter)/10.0f, 0.0f, 1.0f)); 

		// special case for sniper rifle
		if(wpn->getCategory() == storecat_SNP && m_isInScope && triggerPressedCounter==0)
			spread = 0;
	}

	// if in air
	if(!bOnGround)
	{
		if(wpn->getCategory() == storecat_HG)
			spread *= 2.0f;
		else
			spread *= 4.0f; // hard coded, if jumping = spread increased by X times
	}

	// special case for sniper rifle
	if(wpn->getCategory() == storecat_SNP && !m_isInScope)
		spread = 11; // fixed spread at 11 for sniper rifles if not aiming - per sergey's request


	// FINISHED SPREAD\RECOIL LOGIC

	{
		bool isReloading = wpn->isReloading();
		if(isReloading && !wasReloading)
		{
			wasReloading = true;
		}
		if(!isReloading && wasReloading)
		{
			wasReloading = false;
		}
		if(isReloading)
		{
			float progress = R3D_CLAMP((wpn->getReloadProgress()/wpn->getReloadTime()), 0.0f, 1.0f);
		}
	}

	if((InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_RELOAD)||Gamepad->WasReleased(gpX)) && !wpn->isReloading())
		wpn->Reload();

	if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_CHANGE_RATE_FIRE) && !wpn->isReloading())
	{
		wpn->switchFiremode();
		//		if(hudMain)
		//			hudMain->setFireMode(wpn->getFiremode());
	}

	// check for grenade throw, if successful it will go to normal fire code
	if(wpn->getCategory() == storecat_GRENADE)
	{
		const wiInventoryItem& wi = wpn->getPlayerItem(); 
		if(!uberAnim_->IsGrenadePinPullActive() && wi.quantity > 0)
		{
			//if(wpn->isReadyToFire(m_isPressedFireTrigger, m_isFinishedAiming)) 
			//{
            if(wpn->isReadyToFire(m_isPressedFireTrigger, m_isFinishedAiming)) 
            {
                uberAnim_->StartGrenadePinPullAnimation();


                PKT_C2C_PlayerReadyGrenade_s n;
                n.wid = m_SelectedWeapon;
                p2pSendToHost(this, &n, sizeof(n));
                return;
            }
		}
	}


	bool isReadyToFire = wpn->isReadyToFire(m_isPressedFireTrigger, m_isFinishedAiming);
	float holdingDelay = 0;

	// for grenades, override regular logic a little bit
	if(wpn->getCategory()==storecat_GRENADE && uberAnim_->IsGrenadePinPullActive())
	{
	}

	// sniper aiming and holding breath logic
	{
		// reset flag that we're holding breath only when key is released. used for prevent sprinting (same key)
		if(m_isHoldingBreath && !InputMappingMngr->isPressed(r3dInputMappingMngr::KS_HOLD_BREATH))
			m_isHoldingBreath = false;

		static float last_timer = 0;
		if(wpn->getCategory() == storecat_SNP && m_isAiming) // sniper rifle and in scope mode
		{
			r3d_assert(NetworkLocal); // just to make sure, as this code should run only for local player (using statics here)

			if(InputMappingMngr->isPressed(r3dInputMappingMngr::KS_HOLD_BREATH) && m_Breath>0/* && m_BreathPenaltyTime<=0*/)
			{
				m_isHoldingBreath = true;

				//ViewAngle += SniperViewMod;
				//SniperViewMod.Assign(0,0,0);
				m_Breath = m_Breath - r3dGetFrameTime();
				if(m_Breath < 0)
					m_Breath = 0;
				//if(m_Breath <= 0)
				//	m_BreathPenaltyTime = GPP->c_fSniperRifleAfterHoldBounceTime;
			}
			else
			{
				float swayDecrease = 0.75f;

				SniperViewMod.Assign((r3dSin(last_timer/3)*3)*swayDecrease, (r3dCos(last_timer))*swayDecrease, 0);
				last_timer += r3dGetFrameTime();
			}
		}
		else
		{
			SniperViewMod.Assign(0,0,0);
			last_timer = 0;
		}
	}

	if(isReadyToFire) 
	{
		if(wpn->getCategory() == storecat_GRENADE)
		{
			uberAnim_->StartGrenadeThrowAnimation();
		}

		int numActualShots = wpn->getNumShotsRequired();
		int numVirtualShots = 0; // for shotgun
		if(numActualShots && wpn->getCategory() == storecat_SHTG)
			numVirtualShots = 7;

		while(numActualShots>0)
		{
			executeWeaponFireLocalPlayer(gCam, recoil, spread, recoilCoolDownMod, holdingDelay, true, !m_isInScope, 0);	
			--numActualShots;

			// update spread after shooting
			CurrentSpread += SpreadIncrease[wpn->getCategory()-storecat_ASR]*wpn->getSpread();
			CurrentSpread = R3D_MIN(CurrentSpread, wpn->getSpread());
			lifeProperties.weaponFired = true;
		}
		while(numVirtualShots>0)
		{
			executeWeaponFireLocalPlayer(gCam, recoil, spread, recoilCoolDownMod, holdingDelay, false, !m_isInScope, 0);	
			--numVirtualShots;
			lifeProperties.weaponFired = true;
		}

		uberAnim_->StartRecoilAnim();
	}

	m_ReticleTargetScale = R3D_MIN(1.0f + spread, 12.0f);

	// update reload anim
	if(wpn->isReloading())
	{
		uberAnim_->StartReloadAnim();
	}
	else
	{
		uberAnim_->StopReloadAnim();
	}

	VMPROTECT_End();
}

// grenades are thrown at specific animation frame, so we cannot remove weapon (grenade) before it executed. So we have to wait for grenade to be thrown before updating our inventory
// this callback handles this
void obj_Player::GrenadeCallbackFromWeapon(const wiInventoryItem& wi)
{
	// simulate what fire() will do
	if(hudMain)
		hudMain->showSlots(true);

	if(wi.quantity <= 0) {
		OnBackpackChanged(m_SelectedWeapon);
	}
	else
	{
		if(hudMain)
			hudMain->updateSlotInfo(m_SelectedWeapon, wi.quantity);
	}
}

float obj_Player::GetGrenadeLaunchDelay()
{
	return uberAnim_->GetGrenadeLaunchFrame() / 25.0f;
}


// make sure this function is the same on server: bool ServerGameLogic::CanDamageThisObject(const GameObject* targetObj)
bool canDamageTarget(const GameObject* obj)
{
	if(obj->isObjType(OBJTYPE_Human))
		return true;
	if(obj->Class->Name == "obj_Animals")
		return true;
	if(obj->isObjType(OBJTYPE_Zombie))
		return true;
	if(obj->Class->Name == VMPROTECT_DecryptStringA("obj_LightMesh"))
	{
		obj_LightMesh* lmesh = (obj_LightMesh*)obj;
		if(lmesh->isLightOn())
			return true;
		return false;
	}
	if(obj->Class->Name == VMPROTECT_DecryptStringA("obj_Barricade"))
	{
		return true;
	}
	if(obj->Class->Name == VMPROTECT_DecryptStringA("obj_GunTurret"))
	{
		return true;
	}

	return false;
}

float g_ScopeFeedbackTimer = 0.0f;

void obj_Player::executeWeaponFireLocalPlayer(const r3dPoint3D& from, float recoil, float spread, float recoilCoolDownMod, float holdingDelay, bool executeWeaponFireCode, bool checkAgainsMuzzler, int damagePercFromPiercing)
{
	AHNHS_PROTECT_FUNCTION
		r3d_assert(NetworkLocal);

	Weapon* wpn = m_Weapons[m_SelectedWeapon];
	r3d_assert(wpn);
	r3d_assert(!wpn->isUsableItem());

	r3dPoint3D	shootTargetPos;
	r3dPoint3D	shootTargetNorm;
	GameObject*	shootTarget    = NULL;
	r3dMaterial*	shootMaterial  = NULL;
	const char*	shootActorName = "";

	// reset rotation if we're firing from idle mode
	if(PlayerState == PLAYER_IDLE)
	{
		bodyAdjust_x = 0;
		m_fPlayerRotation = m_fPlayerRotationTarget;
	}

	if(wpn->getCategory() == storecat_SNP)
	{
		// increase recoil for sniper rifle artificially
		recoil *= 10.0f;
		//if(!m_isFinishedAiming && spread>0) // if not aiming, make spread really huge
		//	spread = 50.0f;
	}

	//r3dOutToLog("executeWeaponFireLocalPlayer Spread %.2f\n",spread);
	r3dPoint3D muzzlerPos = GetMuzzlerPosition();
	{
		if(wpn->getCategory() == storecat_MELEE)
			updateMeleeTarget(this, GetPosition()+r3dPoint3D(0, bProne?0.2f:(bCrouch?0.6f:1.5f), 0.0f), muzzlerPos, shootTargetPos, shootTargetNorm, &shootMaterial, &shootTarget, &shootActorName, spread, checkAgainsMuzzler);
		else
			updateShootingTarget(this, from, muzzlerPos, shootTargetPos, shootTargetNorm, &shootMaterial, &shootTarget, &shootActorName, spread, checkAgainsMuzzler);
	}

	RecoilViewModTarget.y += recoil * u_GetRandom(0.85f, 1.75f);
	if(RecoilViewModTarget.y > GPP->c_MaxVerticalRecoil) // limit how much gun goes up
	{
		RecoilViewModTarget.y = u_GetRandom(GPP->c_MaxVerticalRecoil*0.95f, GPP->c_MaxVerticalRecoil*1.1f);
	}
	RecoilViewModTarget.x += recoil*u_GetRandom(-0.15f, 0.3f); // + to a side

	if(executeWeaponFireCode) // set cooldown only for actual fire
	{
		RecoilCooldown = R3D_MAX(wpn->getRateOfFire()*recoilCoolDownMod, r3dGetFrameTime()*2.50f);
	}

	if(wpn->getCategory()==storecat_SNP || wpn->getCategory()==storecat_SHTG)
		if(RecoilCooldown > r3dGetFrameTime()*8.0f)
			RecoilCooldown = r3dGetFrameTime()*8.0f;



	// send weapon fired event
	if(wpn->getCategory() != storecat_GRENADE)
	{
		PKT_C2C_PlayerFired_s n; 
		n.fire_from = GetPosition();
		n.fire_to = shootTargetPos;
		n.holding_delay = holdingDelay;
		n.debug_wid = m_SelectedWeapon;
		n.wasAiming = m_isAiming;
		n.execWeaponFire = executeWeaponFireCode;
		p2pSendToHost(this, &n, sizeof(n), true);
	}
	// iKasu Crossbow Arrow
	/*	if(wpn->getItemID() == 101322) // Not Working on Server
	{
	PKT_C2S_PlayerDropArrow_s n;
	n.fire_to = shootTargetPos;
	p2pSendToHost(this, &n, sizeof(n), true);
	}
	*/
	if(wpn->isImmediateShooting())
	{
		if ( ProcessBulletHit(damagePercFromPiercing, this, shootTargetPos, shootTargetNorm, shootTarget, shootMaterial, shootActorName,  wpn->getConfig(), GetMuzzlerPosition(), GetPosition())  == false ) 
		{	
			r3dPoint3D dir = shootTargetPos - from;
			dir.Normalize();
			executeWeaponFireLocalPlayer(shootTargetPos + dir*0.1f, 0.0f, 0.0f, 0.0f, holdingDelay, false, false, damagePercFromPiercing);
		}
	}

	wpn->Fire(shootTargetPos, uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix), executeWeaponFireCode, holdingDelay);

	if(executeWeaponFireCode)
	{
		extern float g_shootCameraShakeTimer;
		g_shootCameraShakeTimer = 1.0f;
		if(m_isAiming && hasScopeMode())
		{
			g_ScopeFeedbackTimer = 1.0f;
		}
	}

	//	Change player noise value
	lifeProperties.weaponFired = true;
}

void obj_Player::UpdateAnimations(const D3DXMATRIX& CharDrawMatrix)
{
	R3DPROFILE_FUNCTION("UpdateAnimations");
	const float TimePassed = r3dGetFrameTime();
#if ENABLE_RAGDOLL
	bool ragdoll = m_PhysSkeleton && m_PhysSkeleton->IsRagdollMode();
	if (!ragdoll)
#endif
	{
		uberAnim_->anim.Update(TimePassed, r3dPoint3D(0,0,0), CharDrawMatrix);
		uberAnim_->anim.Recalc();
	}

	if(m_PhysSkeleton)
		m_PhysSkeleton->syncAnimation(GetSkeleton(), DrawFullMatrix, GetAnimation());

#if ENABLE_RAGDOLL
	if (ragdoll)
	{
		r3dBoundBox bbox = m_PhysSkeleton->getWorldBBox();
		bbox.Org -= GetPosition();
		SetBBoxLocal(bbox);
	}
#endif
}

r3dPoint3D obj_Player::GetHeadPosition()
{
	D3DXMATRIX m;
	uberAnim_->anim.GetCurrentSkeleton()->GetBoneWorldTM(boneId_Bip01_Head, &m, DrawFullMatrix);

	return r3dPoint3D(m._41, m._42, m._43);
}

void obj_Player::DoDeath(int networdIDKiller, bool forced_by_server, STORE_CATEGORIES damageSource)
{
	r3dOutToLog("%s %s: DoDeath\n",NetworkLocal ? "LocalPlayer" : "", Name.c_str());
	/*{
	GameObject* killedByObj = GameWorld().GetNetworkObject(networdIDKiller);
	if(killedByObj)
	{
	char killerUserName[64]; ((obj_Player*)killedByObj)->GetUserName(killerUserName);
	char deathUserName[64]; GetUserName(deathUserName);
	r3dOutToConsole("DoDeath() %s killed by %s",deathUserName,killerUserName);
	}
	}*/
	r3d_assert(!bDead);

	if(NetworkLocal)
	{
		CurLoadout.Health = 0; // to make sure that UI will show zero health
	}

	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/BulletHits/Hit_Death"), GetPosition());		

	CurrentSpread = 0;

	ReleasePlayerObstacle(&PhysXObstacleIndex);	// player is dead, allow passage thru him

	m_needToDrawBarricadePlacement = 0;
	m_BarricadeMeshPlacement = NULL;
	m_canPlaceConsumableItem = false;

	m_ItemSimulateLeftClick = false;

	m_SpeedBoost = 0;
	m_SpeedBoostTime = 0;
	m_MorhineShotTime = 0;
	m_BandagesEffectTime = 0;

	bCrouch = 0;
	bProne = 0;
	m_isAiming = 0;
	m_isPressedFireTrigger = false;
	lastDamageRcvd = 0;
	BloodEffect = 0;
	bDead = 1;
	TimeOfDeath = r3dGetTime();
	DeathDamageSource = damageSource;
	PosOfDeath = GetPosition();
	Dead_KillerID = networdIDKiller;
	DisableKillerView = false;

	m_AuraType = AT_NONE;
	grenadeReadyTime = 0;

	// if we have not default backpack - remove it as it will be dropped as item on server
	if (!hudMain->IsNoDrop)
	{
		if(CurLoadout.BackpackID != 20176) {
			uberEquip_->SetSlot(SLOT_Backpack, (r3dMesh*)NULL);
		}
	}
	else
	{
		hudMain->addChatMessage(0, "<Client>", "No Item Dropped", 2);
	}


	PlayerState = PLAYER_DIE;
	uberAnim_->AnimPlayerState = PLAYER_DIE;
	uberAnim_->StartDeathAnim();
	DoRagdoll(true, lastTimeHitBone, &lastTimeHitForce);

	// clear rotation so it won't affect ragdoll bbox
	SetRotationVector(r3dPoint3D(0, 0, 0));

	RealAcceleration.Assign(0,0,0);
	InputAcceleration.Assign(0,0,0);

	if(!NetworkLocal)
	{
		const ClientGameLogic& CGL = gClientLogic();
		if(CGL.localPlayer_ && CGL.localPlayer_->GetNetworkID() == Dead_KillerID)
		{
			if(!CGL.localPlayer_->bDead)
			{
				CGL.localPlayer_->m_LastKillTime = r3dGetTime();
			}
		}
	}
	else
	{

		//		hudMain->setHealth(0);

		r_hud_filter_mode->SetInt(0); // turn off NVG
	}

	if(NetworkLocal)
	{
		if(hudAttm && hudAttm->isActive())
			hudAttm->Deactivate();
		if(hudPause && hudPause->isActive())
			hudPause->Deactivate();
		if(hudGeneralStore && hudGeneralStore->isActive())
			hudGeneralStore->Deactivate();
		if(hudVault && hudVault->isActive())
			hudVault->Deactivate();	

		if(hudTrade && hudTrade->isActive())
			hudTrade->Deactivate();	

		g_camera_mode->SetInt(0);
		UpdateLoadoutSlot(CurLoadout); // reload meshes
		SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/MainMenu GUI/UI_player_death"), GetPosition());

#if VEHICLES_ENABLED
		if( isInVehicle() )
		{
			exitVehicle();
		}
#endif
		if(hudMain)
		{
			char deadMsg[128] = {0};
			GameObject* killedByObj = GameWorld().GetNetworkObject(networdIDKiller);
			if(killedByObj)
			{
				if(killedByObj == this)
				{
					r3dscpy(deadMsg, "KILLED BY ENVIRONMENT");
				}
				else if(killedByObj->isObjType(OBJTYPE_Human))
				{
					obj_Player* killedByPlr = (obj_Player*)killedByObj;
					char tmpName[64] = {0};
					killedByPlr->GetUserName(tmpName);
					sprintf(deadMsg, "KILLED BY %s", tmpName);
				}
				else if(killedByObj->isObjType(OBJTYPE_Zombie))
					r3dscpy(deadMsg, "EATEN BY ZOMBIE");
			}
			hudMain->showYouAreDead(deadMsg);
		}
	}

	return;
}

void obj_Player::Undie()
{
	afterRespawnTimer = 2.0f;

	lastWeapDataRep = r3dGetTime();

	bDead = 0;

	// clear death anim
	uberAnim_->anim.AnimTracks.clear();

	DoRagdoll(false);

	PlayerState = PLAYER_IDLE;
	SyncAnimation(true);

}

static void dumpAnim(r3dAnimation& anim)
{
	for(size_t i=0; i<anim.AnimTracks.size(); i++) 
	{
		const r3dAnimation::r3dAnimInfo& ai = anim.AnimTracks[i];

		char st[256] = "";
		if(ai.GetStatus() & ANIMSTATUS_Playing) strcat(st, "Play ");
		if(ai.GetStatus() & ANIMSTATUS_Paused) strcat(st, "Pause ");
		if(ai.GetStatus() & ANIMSTATUS_Finished) strcat(st, "Finish ");
		if(ai.GetStatus() & ANIMSTATUS_Fading) strcat(st, "Fade ");
		if(ai.GetStatus() & ANIMSTATUS_Expiring) strcat(st, "Expire ");

		r3dOutToLog("%d: %s %f, %X %s\n", ai.GetTrackID(), ai.GetAnim()->GetAnimName(), ai.GetInfluence(), ai.GetStatus(), st);
	}
}

r3dSkeleton* obj_Player::GetSkeleton()
{
	return uberAnim_->anim.pSkeleton;
}

r3dAnimation& obj_Player::GetAnimation()
{
	return uberAnim_->anim;
}

void obj_Player::SyncAnimation(bool force)
{
	R3DPROFILE_FUNCTION("obj_Player::SyncAnimation");
	if(bDead)
		return;

	const Weapon* wpn = m_Weapons[m_SelectedWeapon];
	if(forcedEmptyHands)
		wpn = NULL;
	bool isInAttmMenu = false;
	if(wpn && hudAttm && hudAttm->isActive() && wpn->getCategory()>=storecat_ASR && wpn->getCategory()<=storecat_SMG)
		isInAttmMenu = true;

	uberAnim_->SyncAnimation(PlayerState, PlayerMoveDir, force, wpn, isInAttmMenu); // firstly sync to update weapon pointer and only then update jump
	uberAnim_->UpdateJump(bOnGround);
}

static float accumLastDamageRcvd = 0;
	void obj_Player::ApplyDamage(const r3dPoint3D& dmgPos, float damage, GameObject* fromObj, int bodyBone, int dmgType)
	{
		lastTimeHit = r3dGetTime();
		lastTimeHitBone = bodyBone;
		lastDamageRcvd = damage;
		accumLastDamageRcvd += damage;

		if (fromObj == gClientLogic().localPlayer_)
		{
		if(bodyBone == uberAnim_->GetBoneID("Bip01_Head"))
		SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/BulletHits/Hit_Headshot"), GetPosition());
		/*else if(dmgType==storecat_MELEE)
		SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/Impacts/Impacts-KnifeStab"), GetPosition());		
		else
		SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/Impacts/Impacts-Body"), GetPosition());*/		
		}

		// apply ragdoll force from object position, or up if too close
		r3dPoint3D ragHitPos = GetPosition();
		lastTimeHitForce = r3dPoint3D(0, 1, 0);
		if(bodyBone < GetSkeleton()->NumBones)
			ragHitPos = uberEquip_->getBonePos(bodyBone, GetSkeleton(), DrawFullMatrix);
		if((ragHitPos - fromObj->GetPosition()).LengthSq() > 0.001f)
			lastTimeHitForce = (ragHitPos - fromObj->GetPosition()).NormalizeTo();

		float dmgForce = 10.0f;
		switch(dmgType)
		{
		case storecat_ASR:
			dmgForce = 30.0f;
			break;
		case storecat_SNP:
			dmgForce = 60.0f;
			break;
		case storecat_SHTG:
			dmgForce = 60.0f;
			break;
		case storecat_MG:
			dmgForce = 40.0f;
			break;
		case storecat_HG:
			dmgForce = 10.0f;
			break;
		case storecat_SMG:
			dmgForce = 20.0f;
			break;
		}
		lastTimeHitForce *= dmgForce;

	#ifndef FINAL_BUILD
		if(NetworkLocal && m_isAiming)
		{
			float recoil = 3.5f;
			RecoilViewModTarget2.x = recoil * (random(2)==1?u_GetRandom(-1.0f, -0.5f):u_GetRandom(0.5f, 1.0f));
			RecoilViewModTarget2.y = recoil * (random(2)==1?u_GetRandom(-1.0f, -0.5f):u_GetRandom(0.5f, 1.0f));
		}
	#endif
	}

	bool obj_Player::ChangeWeaponByIndex(int index)
	{
		r3d_assert(index>=0 && index < NUM_WEAPONS_ON_PLAYER);
		if(m_SelectedWeapon == index && !forcedEmptyHands)
			return false;

		m_needToDrawBarricadePlacement = 0;
		m_BarricadeMeshPlacement = NULL;
		m_canPlaceConsumableItem = false;

		forcedEmptyHands = false;
		m_PrevSelectedWeapon = m_SelectedWeapon;
		m_SelectedWeapon = index;

		uberAnim_->StopReloadAnim();

		if(m_PrevSelectedWeapon >= 0 && m_Weapons[m_PrevSelectedWeapon])
		{
			m_Weapons[m_PrevSelectedWeapon]->OnUnequip();
		}

		if(m_Weapons[m_SelectedWeapon])
		{
			m_Weapons[m_SelectedWeapon]->OnEquip();
		}

		if(hudMain && NetworkLocal)
		{
			showSlotsInfoTimer = r3dGetTime() + 5.0f;
			hudMain->showSlots(true);

			updateWeaponUI();
		}


		UpdateCharWeaponMeshes();
		return true;
	}

void obj_Player::UpdateCharWeaponMeshes()
{
	//r3d_assert(!m_Weapons[m_SelectedWeapon].Empty());

	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	uberEquip_->SetSlot(SLOT_Weapon, m_Weapons[m_SelectedWeapon]);
	if(m_SelectedWeapon==0)
	{
		uberEquip_->SetSlot(SLOT_WeaponBackRight, (Weapon*)NULL);
	}
	else
	{
		if(m_Weapons[0])
		{
			uberEquip_->SetSlot(SLOT_WeaponBackRight, m_Weapons[0]);
		}
	}
	if(m_SelectedWeapon==1)
		uberEquip_->SetSlot(SLOT_WeaponSide, (Weapon*)NULL);
	else
	{
		if(m_Weapons[1])
			uberEquip_->SetSlot(SLOT_WeaponSide, m_Weapons[1]);
	}
}

void obj_Player::ReloadWeaponFromSlot(int wpnSlot, int ammoSlot, int amount)
{


	r3d_assert(NetworkLocal);



	// remove clip (SERVER CODE SYNC POINT)
	wiInventoryItem& wi = CurLoadout.Items[ammoSlot];
	r3d_assert(wi.quantity > 0);
	wi.quantity--;
	if(wi.quantity <= 0)
		wi.Reset();

	//OnBackpackChanged(ammoSlot);

	updateWeaponUI();

	PKT_C2C_PlayerReload_s n;
	n.WeaponSlot = (BYTE)wpnSlot;
	n.AmmoSlot   = (BYTE)ammoSlot;
	n.dbg_Amount = (BYTE)amount;
	p2pSendToHost(this, &n, sizeof(n));

	return;
}

float obj_Player::getMaxStamina()
{
	float maxStamina = GPP->c_fSprintMaxEnergy;
#ifdef FINAL_BUILD
	if(CurLoadout.Stats.skillid1 == 1){
		maxStamina += 25.5f;
		if(CurLoadout.Stats.skillid4 == 1)
			maxStamina += 30.5f;

		if (gUserProfile.ProfileData.isPunisher)
		{
			maxStamina += 33.5f;
		}
	}
	else
	{
		if (gUserProfile.ProfileData.isPunisher)
		{
			maxStamina += 30.5f;
		}
	}
#endif
	// Skillsystem
	return maxStamina;
}

float obj_Player::getMaxBreath()
{
	float maxBreath = GPP->c_fSniperRifleHoldBreathTime;
#ifdef FINAL_BUILD
	if(CurLoadout.Stats.skillid12 == 1){
		maxBreath += 1.5f;
		if(CurLoadout.Stats.skillid14 == 1)
			maxBreath += 2.5f;

		if (gUserProfile.ProfileData.isPunisher)
		{
			maxBreath += 3.5f;
		}
	}
	else
	{
		if (gUserProfile.ProfileData.isPunisher)
		{
			maxBreath += 3.0f;
		}
	}
#endif
	// Skillsystem
	return maxBreath;
}

void obj_Player::StartJump()
{
	r3d_assert(NetworkLocal);

	//check if we are joing to jump into water, cancel jump in that case
	{
		r3dPoint3D vel    = r3dPoint3D(0,0,0);
		vel    += (GetvForw()  * InputAcceleration.Z);
		vel    += (GetvRight() * InputAcceleration.X);
		vel.Y  += InputAcceleration.Y;

		r3dPoint3D newPos = vel + GetPosition();
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
		bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(newPos.x, newPos.y + 0.5f, newPos.z), PxVec3(0, -1, 0), 10.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
		r3dPoint3D posForWater = newPos;
		if( hitResult )
			posForWater = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);			

		float waterDepth = getWaterDepthAtPos(posForWater);
		if(waterDepth > 1.0f) // too deep
		{
			return; // cancel jump
		}
	}

	uberAnim_->StartJump();
	JumpVelocity  = 18;

	JumpStartTime = r3dGetTime() + uberAnim_->jumpStartTime;

	PKT_C2C_PlayerJump_s n;
	p2pSendToHost(this, &n, sizeof(n));

	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_JUMPING_F":"Sounds/WarZ/PlayerSounds/PLAYER_JUMPING_M"), GetPosition());

}

bool obj_Player::IsJumpActive()
{
	// going up on jump, or in air.
	return (JumpVelocity > 0 || !bOnGround);
}

void obj_Player::DetectIfOnGround()
{
	//static bool prevOnGround = bOnGround;

	PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
	if(g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 1.00f, PxSceneQueryFlags(PxSceneQueryFlag::eDISTANCE), hit, filter) && !bSwim)
	{
		bOnGround = (hit.distance < 0.6f);
		fHeightAboveGround = hit.distance - 0.5f;
		bOnGround = true;
	}
	else
	{
		bOnGround = (hit.distance < 0.6f);
		fHeightAboveGround = hit.distance - 0.5f;
		bOnGround = false;
	}

	// hack to check if player got glued to some geometry while jumping. Happen if he jumped on something that has some bumps and character controller thinks that he can stay on that tiny bump
	/*if(!bOnGround && R3D_ABS(oldstate.Position.y - GetPosition().y) > 0.01f)
	{
	bOnGround = true;
	fHeightAboveGround = 0.0f;
	}
	else*/ if (R3D_ABS(oldstate.Position.y - GetPosition().y) < 0.01f && !bOnGround)
	{ 
		bOnGround = true;
		fHeightAboveGround = 0.0f;
	}

	if (bSwim)
	{
		//bOnGround = true;
		fHeightAboveGround = 0.0f;
	}

	//if(prevOnGround != bOnGround)
	//{
	//	prevOnGround = bOnGround;
	//	r3dOutToLog("onGround=%d\n", bOnGround);
	//}
}

void obj_Player::UpdateLocalPlayerMovement()
{
	r3d_assert(NetworkLocal);

	VMPROTECT_BeginMutation("obj_Player::UpdateLocalPlayerMovement");

	// important: limit movement to max for 1 sec. so server can check for teleports
	const float TimePassed = R3D_MIN(r3dGetFrameTime(), 0.1f);
	const float curTime    = r3dGetTime();

	AHNHS_PROTECT_FUNCTION
		RealAcceleration = InputAcceleration;

	// apply weight to moving speed before gravity and jumping
	float weightAdj = 1.0f + ((m_GearWeight/100.0f)); // each 100kg half your speed
	RealAcceleration = RealAcceleration/weightAdj;

	const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(CurLoadout.BackpackID);

	float Gravity = 9.81f;
	float PlrMaxWeight = bc->m_maxWeight;
	float PlrTotalWeight = CurLoadout.getTotalWeight();
#ifdef FINAL_BUILD
	if(CurLoadout.Stats.skillid2 == 1){
		PlrMaxWeight *= 1.10f;
		PlrTotalWeight *= 1.0f;
		if(CurLoadout.Stats.skillid6 == 1){
			PlrMaxWeight *= 1.25f;
			PlrTotalWeight *= 1.0f;
		}
		if(gUserProfile.ProfileData.isPunisher){
			PlrMaxWeight *= 1.50f;
			PlrTotalWeight *= 1.0f;
		}


	}
#endif


	if(PlrMaxWeight > PlrTotalWeight)
		weightAdj = 1.0f + 0.0f;
	else{
		if((PlrTotalWeight - PlrMaxWeight) >= 30.0f)
		{
			weightAdj = 1.0f + 100.0f; // each 100kg half your speed
			Gravity = Gravity + (30.0f / 10.0f);
		}else{
			weightAdj = 1.0f + (PlrTotalWeight - PlrMaxWeight) / 10.0f; // each 100kg half your speed
			Gravity = Gravity + (PlrTotalWeight - PlrMaxWeight) / 10.0f;
		}
	}
	RealAcceleration = RealAcceleration/weightAdj;

	// locally add gravity to acceleration
	/*if(JumpVelocity=0)
	uberAnim_->UpdateJump(true);*/

	PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
	bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 500.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
	r3dPoint3D posForWater = GetPosition();
	if( hitResult )
		posForWater = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);			

	float waterDepth = getWaterDepthAtPos(GetPosition());

	if (!isInVehicle() && !bSwim)
	{
		float deltaTimeInAir =(curTime-StartFallingTime);
		if(!bOnGround && JumpVelocity<=0) // don't accelerate falling while jumping, only when actually falling
		{
			Gravity += (Gravity * deltaTimeInAir)*1.2f;
		}

		if (waterDepth < 1.4f)
		{
			RealAcceleration.Y -= Gravity;
		}
		if(JumpVelocity > 0 && curTime >= JumpStartTime && waterDepth < 0.4f)
		{
			JumpVelocity -= Gravity * TimePassed * 2.5f;
			RealAcceleration.Y += JumpVelocity;
			// reset falling time
			if(JumpVelocity<=0)
				StartFallingTime = curTime;
		}

		if(bOnGround)
			StartFallingTime = curTime; // keep reseting timer, otherwise when jump it will not be null
	}

	r3dPoint3D vel    = r3dPoint3D(0,0,0);
	/*		if (!bOnGround && JumpVelocity > -3 && JumpVelocity < 2)
	{
	RealAcceleration.X += 0.50f;
	RealAcceleration.Z += 0.50f;
	RealAcceleration.Y -= 10.0f;
	vel    += RealAcceleration.Z;
	vel    += RealAcceleration.X;
	vel.Y  += RealAcceleration.Y;
	}
	else
	{*/
	vel    += (GetvForw()  * RealAcceleration.Z);
	vel    += (GetvRight() * RealAcceleration.X);
	vel.Y  += RealAcceleration.Y;
	//}
	// send some info to server about fast movement
	{
		static float lastUpd = -999;
		r3dPoint3D velXZ(vel.x, 0, vel.z);
		if(velXZ.Length() > 8.0f && curTime > lastUpd + 60.0f)
		{
			lastUpd = curTime;
			char buf[1024];
			sprintf(buf, "%.2f %.2f %.1f/%.1f %.2f %.1f", 
				velXZ.x, velXZ.z, GPP->AI_SPRINT_SPEED, GPP->AI_RUN_SPEED, TimePassed, weightAdj);

			/*PKT_C2S_DBG_LogMessage_s n;
			r3dscpy(n.msg, buf);
			p2pSendToHost(this, &n, sizeof(n));*/
		}
	}

	ControllerPhysObj* controller = (ControllerPhysObj*)PhysicsObject;
	// IF PLAYER IS FAST LOAD ON PLAYER CANT MOVE 12sec because fastload problem.
	// wait 12 secs for fastload load building around player.
	// prevPos2 = GetPosition();
	if ((GetPosition() - prevPosLeng).LengthSq() > 5)
	{
		prevPosLeng = GetPosition();
	}

	/*if (g_FastLoad->GetBool() && r3dGetTime() > gClientLogic().localPlayerConnectedTime + 10 && gClientLogic().localPlayer_ == this)
	controller->Move(vel * TimePassed, 0.5f);

	if (!g_FastLoad->GetBool())*/
	if (controller)
		controller->Move(vel * TimePassed, 0.5f);

	/*PKT_C2S_SetVelocity_s n;
	n.pos = GetPosition();
	n.vel = vel;
	p2pSendToHost(this,&n,sizeof(n));*/


	if (GetPosition().Length() > 10 && prevPos.Length() > 10)
	{
		// check if he under terrain
		if (Terrain)
		{
			float height = Terrain->GetHeight(GetPosition());

			if (GetPosition().y+0.1f < height)
			{
				SetPosition(r3dPoint3D(GetPosition().x,height+0.5f,GetPosition().z));
				SetVelocity(r3dPoint3D(0,0,0));
			}
		}
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
		//PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK,0,0,0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);

		ControllerPhysObj* controller = (ControllerPhysObj*)PhysicsObject;

		//r3dPoint3D pos = (prevPos + r3dPoint3D(0,controller->GetControlerHeight() >= 0.5f ? 0.5f : 0.4f,0) - GetPosition()).NormalizeTo();
		//if(g_pPhysicsWorld->raycastSingle(PxVec3(prevPos.x, prevPos.y + controller->GetControlerHeight() >= 0.5f ? 0.5f : 0.4f,prevPos.z), PxVec3(pos.x,pos.y,pos.z), pos.Length(), PxSceneQueryFlag::eIMPACT, hit, filter))
		r3dVector pos = prevPos;
		r3dVector dir = GetPosition() - prevPos;
		float dirl = dir.Length(); 
		dir.Normalize();
		if(g_pPhysicsWorld->raycastSingle(PxVec3(pos.x, pos.y, pos.z), PxVec3(dir.x,dir.y,dir.z), dirl, PxSceneQueryFlag::eIMPACT, hit, filter))
		{
			//r3dOutToLog("raycastSingle\n");
			obj_Player* plr = this;
			if (plr)
			{
				PxRaycastHit hit;
				PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
				bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(plr->prevPos.x, (plr->prevPos.y + 0.5f) + controller->GetControlerHeight() >= 0.5f ? 0.5f : 0.4f, plr->prevPos.z), PxVec3(0, -1, 0), 500.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
				r3dPoint3D pos = plr->GetPosition();
				if( hitResult )
				{
					pos = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);
					SetPosition(pos);
					SetVelocity(r3dPoint3D(0,0,0));
					prevPos = GetPosition();
				}
				else
				{
					SetPosition(prevPos);
					SetVelocity(r3dPoint3D(0,0,0));
				}

				//SetPosition(prevPosLeng);
			}
		}
		else
		{
			float heighOffset = 0.8f;
			if(controller->GetControlerHeight() <0.5f)
				heighOffset = 0.4f;

			PxBoxGeometry bbox(0.05f, heighOffset, 0.05f);
			PxTransform pose(PxVec3(prevPos.x, prevPos.y + controller->m_HeightOffset, prevPos.z), PxQuat(0,0,0,1));
			PxShape* hit = NULL;
			if(g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, hit, filter))
			{
				obj_Player* plr = this;
				PxRaycastHit hit;
				PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
				bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(plr->prevPos.x, (plr->prevPos.y + 0.5f) + controller->GetControlerHeight() >= 0.5f ? 0.5f : 0.4f, plr->prevPos.z), PxVec3(0, -1, 0), 500.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
				r3dPoint3D pos = plr->prevPosLeng;
				if( hitResult )
				{
					pos = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);
					SetPosition(pos);
					SetVelocity(r3dPoint3D(0,0,0));
					prevPos = GetPosition();
				}
				else
				{
					SetPosition(prevPos);
					SetVelocity(r3dPoint3D(0,0,0));
				}

				//SetPosition(prevPosLeng);
			}
			else
			{
				PxTransform pose(PxVec3(GetPosition().x, GetPosition().y + controller->m_HeightOffset, GetPosition().z), PxQuat(0,0,0,1));
				if(g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, hit, filter))
				{
					obj_Player* plr = this;
					PxRaycastHit hit;
					PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
					bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(plr->prevPos.x, (plr->prevPos.y + 0.5f) + controller->GetControlerHeight() >= 0.5f ? 0.5f : 0.4f, plr->prevPos.z), PxVec3(0, -1, 0), 500.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
					r3dPoint3D pos = plr->prevPos;
					if( hitResult )
					{
						pos = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);
						SetPosition(pos);
						SetVelocity(r3dPoint3D(0,0,0));
						prevPos = GetPosition();
					}
					else
					{
						SetPosition(prevPos);
						SetVelocity(r3dPoint3D(0,0,0));
					}

					//SetPosition(prevPosLeng);
				}
				/*else
				{
				PxBoxGeometry bbox(0.1f, heighOffset, 0.1f);
				if(g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, hit, filter))
				{
				obj_Player* plr = this;
				PxRaycastHit hit;
				PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
				bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(plr->prevPos.x, (plr->prevPos.y + 0.5f) + controller->GetControlerHeight() >= 0.5f ? 0.5f : 0.4f, plr->prevPos.z), PxVec3(0, -1, 0), 500.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
				r3dPoint3D pos = plr->prevPos;
				if( hitResult )
				{
				pos = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);
				SetPosition(pos);
				prevPos = GetPosition();
				}
				else
				SetPosition(prevPos);

				//SetPosition(prevPosLeng);
				}
				}*/
			}
		}
	}

	prevPos = GetPosition();
	parent::Update(); // manually update position after updating physics. Needed for camera, otherwise it will be lagging one frame behind

	// check if player is trying to get into water
	// use raycast to fix bug when player able to walk on water by jumping
	{
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
		bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 500.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
		r3dPoint3D posForWater = GetPosition();
		if( hitResult )
			posForWater = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);			

		float waterDepth = getWaterDepthAtPos(GetPosition());
		if(waterDepth > 1.5f) // too deep
		{
			SetPosition(r3dPoint3D(GetPosition().x,GetPosition().y + 0.1f,GetPosition().z)); // Up Player to a water
			//parent::Update();
			//SetVelocity(r3dPoint3D(0,0,10));
		}
		else
			SetVelocity(vel);
	}

	/*ObjectManager& GW = GameWorld();
	for (GameObject *targetObj = GW.GetFirstObject(); targetObj; targetObj = GW.GetNextObject(targetObj))
	{
	if (targetObj->Class->Name == "obj_Vehicle")
	{
	//r3dOutToLog("obj_Vehicle Length %.2f",(GetPosition() - targetObj->GetPosition()).LengthSq());
	if ((GetPosition() - targetObj->GetPosition()).LengthSq() < 2.0f)
	{
	SetPosition(prevPos);
	parent::Update();
	SetVelocity(r3dPoint3D(0,0,0));
	}
	}
	}*/
	if (prevPos.y-GetPosition().y < -0.75f)
	{
		//parent::Update();
		// SetVelocity(r3dPoint3D(0,0,0));
		SetPosition(prevPos);
		r3dOutToLog("Position Y OVERLOAD!\n");
	}

	float ll = (prevPos-GetPosition()).LengthSq();
	if(ll > 0.0001f)
	{
		extern void WaterSplash(const r3dPoint3D& waterSplashPos, float height, float size, float amount, int texIdx);
		WaterSplash(GetPosition(), 2.0f, 5.0f, 0.1f*ll, 3);
	}

	bool bWasOnGround = bOnGround;
	DetectIfOnGround();

	if(bWasOnGround && !bOnGround)
	{
		StartFallingHeight = GetPosition().y;
		StartFallingTime = r3dGetTime();
	}
	if(!bWasOnGround && bOnGround)
	{
		// calculate damage from falling if any
		float newHeight = GetPosition().y;
		if((StartFallingHeight - newHeight) > 8.0f && !bSwim) // more than 8 meters, we gonna break something
		{
			float damageAmount = ((StartFallingHeight - newHeight))*2.0f; // if falling more than 25 meters - dies immediatelly (damage >=100)
			damageAmount = R3D_CLAMP(damageAmount, 0.0f, 255.0f);

			// send damage to server
			PKT_C2S_FallingDamage_s n;
			n.damage = (BYTE)damageAmount;
#ifndef CHEATS_ENABLED
			p2pSendToHost(this, &n, sizeof(n));
#endif

			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_GRUNTING_F":"Sounds/WarZ/PlayerSounds/PLAYER_GRUNTING_M"), GetPosition());
		}
	}

	// STAMINA LOGIC SHOULD BE SYNCED WITH SERVER CODE!
	if(PlayerState == PLAYER_MOVE_SPRINT || bSwimShift)
	{
		AHNHS_PROTECT_FUNCTION
			m_Stamina = m_Stamina - TimePassed;
		if(m_Stamina <= 0)
		{
			AHNHS_PROTECT_FUNCTION
				m_StaminaPenaltyTime = 3.0f;
		}
	}
	else if(bOnGround || bSwim) // regen only if standing on ground and swim
	{
		float regen_rate = CurLoadout.Health<50?GPP->c_fSprintRegenRateLowHealth:GPP->c_fSprintRegenRateNormal;
#ifdef FINAL_BUILD
		if(CurLoadout.Stats.skillid3 == 1){
			regen_rate *= 2.0f;
			if(CurLoadout.Stats.skillid5 == 1)
				regen_rate *= 3.0f;

			if (gUserProfile.ProfileData.isPunisher)
			{
				regen_rate *= 3.5f;
			}
		}
		else
		{
			if (gUserProfile.ProfileData.isPunisher)
			{
				regen_rate *= 3.0f;
			}
		}
#endif

		// Skillsystem iKasu

		AHNHS_PROTECT_FUNCTION
			m_Stamina = m_Stamina + TimePassed*regen_rate; // regeneration rate
		if(m_StaminaPenaltyTime > 0.0f)
			m_StaminaPenaltyTime = m_StaminaPenaltyTime - TimePassed;
	}
	m_Stamina = R3D_CLAMP((float)m_Stamina, 0.0f, getMaxStamina());

	bool scopeRendering = hasScopeMode() && CurrentRig.allowScope;
	if(m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getCategory() == storecat_SNP && m_isFinishedAiming) 
	{
		if(hudMain)
		{
			//			hudMain->SetEnergyValue(1.0f); // hide energy bar
			//			hudMain->SetBreathValue(R3D_CLAMP((m_Breath/getMaxBreath()), 0.0f, 1.0f));
		}
	}
	else
	{
		if(hudMain)
		{
			//			hudMain->SetBreathValue(2.0f); // hide breath
			//			hudMain->SetEnergyValue(m_Stamina/getMaxStamina());
		}
	}

	VMPROTECT_End();
}

void obj_Player::localPlayer_UseItem(int slotID, uint32_t itemID, const r3dPoint3D& pos, float var1, float var2, float var3, DWORD var4)
{
	/*	if (hudMain->Cooldown > 0)
	{
	if(hudMain) hudMain->showMessage(gLangMngr.getString("Currenty Cooldown"));
	return;
	}*/

	r3d_assert(NetworkLocal);

	PKT_C2C_PlayerUseItem_s n;
	n.SlotFrom  = (BYTE)slotID;
	n.dbg_ItemID= itemID;
	n.pos       = pos;
	n.var1		= var1;
	n.var2		= var2;
	n.var3		= var3;
	n.var4		= var4;
	p2pSendToHost(this, &n, sizeof(n), true);

	const FoodConfig* fc = g_pWeaponArmory->getFoodConfig(itemID);
	if(fc)
	{
		hudMain->currentslot = slotID;
		hudMain->Cooldown = r3dGetTime() + 10.0f;
		if(fc->category == storecat_Food)
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_EAT"), GetPosition());
		else
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_DRINK"), GetPosition());

		m_Stamina = m_Stamina + GPP->c_fSprintMaxEnergy*fc->Stamina;
	}
	if (itemID==WeaponConfig::ITEMID_Antibiotics||itemID==WeaponConfig::ITEMID_Painkillers||itemID==WeaponConfig::ITEMID_Medkit)   
	{        
		if(var4!=0)        
		{            
			GameObject* obj = GameWorld().GetNetworkObject(var4);            
			if(obj && obj->isObjType(OBJTYPE_Human))           
			{                
				obj_Player* plr = (obj_Player*)obj;                
				SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(plr->m_isFemaleHero?"Sounds/WarZ/PlayerSounds/HEAL_pills":"Sounds/WarZ/PlayerSounds/HEAL_pills"), plr->GetPosition());            
			}        
		}        
		else            
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/HEAL_pills":"Sounds/WarZ/PlayerSounds/HEAL_pills"), GetPosition());    
	}    
	if (itemID==WeaponConfig::ITEMID_Bandages||itemID==WeaponConfig::ITEMID_Bandages2)   
	{        
		if(var4!=0)       
		{            
			GameObject* obj = GameWorld().GetNetworkObject(var4);            
			if(obj && obj->isObjType(OBJTYPE_Human))           
			{                
				obj_Player* plr = (obj_Player*)obj;                
				SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(plr->m_isFemaleHero?"Sounds/WarZ/PlayerSounds/HEAL_bandages":"Sounds/WarZ/PlayerSounds/HEAL_bandages"), plr->GetPosition());            
			}        
		}       
		else            SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/HEAL_bandages":"Sounds/WarZ/PlayerSounds/HEAL_bandages"), GetPosition());    
	}    
	if (itemID==WeaponConfig::ITEMID_C01Vaccine||itemID==WeaponConfig::ITEMID_C04Vaccine)    
	{        
		if(var4!=0)        
		{            
			GameObject* obj = GameWorld().GetNetworkObject(var4);            
			if(obj && obj->isObjType(OBJTYPE_Human))           
			{                
				obj_Player* plr = (obj_Player*)obj;                
				SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(plr->m_isFemaleHero?"Sounds/WarZ/PlayerSounds/HEAL_inject":"Sounds/WarZ/PlayerSounds/HEAL_inject"), plr->GetPosition());            
			}        
		}        
		else            SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/HEAL_inject":"Sounds/WarZ/PlayerSounds/HEAL_inject"), GetPosition());    
	}
	//STOP COPY HERE!
}

BOOL obj_Player::Update()
{
	R3DPROFILE_FUNCTION("obj_Player::Update");
	if(!NetworkLocal) // for local already done in UpdateLocalPlayerMovement
		parent::Update();

	if(CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected)
		m_AuraType = AT_SPAWNPROTECTION;
	else
		m_AuraType = AT_NONE;

	//m_AuraType = AT_SPAWNPROTECTION;
	/*if (gClientLogic().localPlayer_)
	{
	if (gClientLogic().localPlayer_->isInVehicle())
	TogglePhysicsSimulation(false);
	}*/

	//float Stride[4] = {155.5f, 155.5f, 155.5f, 155.5f};

	//D3DXVECTOR4 Stride( 0.0f, 1.0f, 0.0f, 1.0f );

	/*for (int i = 0; i < 2000; ++i)
	{
	r3dRenderer->pd3ddev->SetPixelShaderConstantF(i,Stride,i);
	r3dRenderer->pd3ddev->SetPixelShaderConstantF(i,Stride,i);
	}*/


	//r3dRenderer->pd3ddev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
	//r3dOutToLog("GetPixelShaderConstantF %.2f\n",Stride);

	camoTimeLine.Update();


	// show pending rewards, even if game finished.
	if(NetworkLocal)
	{
		ProcessRewardQueue();
		if(updatehudtime+0.1f < r3dGetTime())
		{
			UpdateHud();
			updatehudtime = r3dGetTime();
		}
	}

	const ClientGameLogic& CGL = gClientLogic();
	if(gClientLogic().disconnectStatus_)
		return TRUE;

	if (NetworkLocal)
	{
		// update timeplayed in local

		// i dont have any idea to add real time , the fucking hammerpoint use int to record this.
		//CurLoadout.Stats.TimePlayed += (int)(r3dGetFrameTime()*60);

#ifdef CHEATS_ENABLED 
#ifdef HK_ENABLED
		if (Keyboard->WasPressed(kbsF7))
		{
			if (gClientLogic().localPlayer_->ESPZombies)
				gClientLogic().localPlayer_->ESPZombies = false;
			else
				gClientLogic().localPlayer_->ESPZombies = true;
		}

		if (Keyboard->WasPressed(kbsF8))
		{
			if (gClientLogic().localPlayer_->ESPHackKill)
				gClientLogic().localPlayer_->ESPHackKill = false;
			else
				gClientLogic().localPlayer_->ESPHackKill = true;
		}
#endif
		if (Keyboard->WasPressed(kbsF10))
		{
			if (gClientLogic().localPlayer_->Cheats)
				gClientLogic().localPlayer_->Cheats = false;
			else
				gClientLogic().localPlayer_->Cheats = true;
		}
#endif
		if(r3dGetTime() > nextSendTimeUpdate)
		{
			nextSendTimeUpdate = r3dGetTime()+60.0f;
			PKT_C2S_ValidateEnvironment_s n;
			n.CurTime = r3dGameLevel::Environment.__CurTime;
			p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
		}
		if (r3dGetTime() > lastSendStateLog)
		{
			lastSendStateLog = r3dGetTime()+2.0f;
			PKT_C2S_PlayerState_s n;
			n.accel = RealAcceleration;
			n.state = PlayerState;
			p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
			{
				PKT_C2S_ValidateBackpack_s n;
				for (int i=0;i<72;i++)
				{
					n.Items[i] = CurLoadout.Items[i];
					//r3dOutToLog("Slot %d itemid %d\n",i,CurLoadout.Items[i].itemID);
				}
				p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
			}
		}
		/*if (r3dGetTime() > nextSendD3D)
		{
		nextSendD3D = r3dGetTime()+2.0f;
		extern unsigned long* pInterface;

		extern DWORD EndSceneAddress;
		extern DWORD DrawIndexedPrimitiveAddress;
		extern DWORD PresentAddress;
		PKT_C2S_SendD3DLog_s n;
		n.DrawIndexedPrimitiveAddress = DrawIndexedPrimitiveAddress;
		n.EndSceneAddress = EndSceneAddress;
		n.pInterface = pInterface;
		n.PresentAddress = PresentAddress;
		p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
		}*/
	}
	const float curTime = r3dGetTime();

#ifndef FINAL_BUILD
	if(NetworkLocal && gDisableShortcuts == false )
	{
		if( Keyboard->IsPressed( kbsLeftAlt ) && Keyboard->WasPressed( kbsK ) )
		{
			g_char_spect->SetInt( !g_char_spect->GetInt() ) ;
		}

		// 		if(Keyboard->WasPressed(kbsF4))
		// 		{
		// 			// send disconnect request to server
		// 			PKT_C2S_DisconnectReq_s n;
		// 			p2pSendToHost(this, &n, sizeof(n), true);
		// 		}

		if(Keyboard->IsPressed(kbsLeftAlt) && hudMain)
		{
			if(Keyboard->WasPressed(kbs8))
			{
				hudMain->showChat(true);
				hudMain->addChatMessage(0, "blabla", "rtjdkfhdkjfhdf dfd f", 1);
			}
			if(Keyboard->WasPressed(kbsF5))
			{
				// send data update request to server
				PKT_C2S_DataUpdateReq_s n;
				p2pSendToHost(this, &n, sizeof(n), true);
				hudMain->showMessage(L"Data update request sent");
			}

			if(Keyboard->WasPressed(kbs8))
			{
				/*PKT_C2S_TEST_SpawnDummyReq_s n;
				n.pos = GetPosition() + GetvForw() * 4.0f;
				PxRaycastHit hit;
				PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
				if(g_pPhysicsWorld->raycastSingle(PxVec3(n.pos.x, n.pos.y+2, n.pos.z), PxVec3(0, -1, 0), 4, PxSceneQueryFlag::eIMPACT, hit, filter))
				{
				n.pos.x = hit.impact.x;
				n.pos.y = hit.impact.y;
				n.pos.z = hit.impact.z;
				}

				p2pSendToHost(this, &n, sizeof(n));*/
			}
		}
	}
#endif

	if(m_Weapons[m_SelectedWeapon] && bSwim)
	{
		switchToEmptyHands();
	}
	/*{
	// check that last known position is good (player didn't log out while in air, or underground, etc) to prevent being stuck
	{
	PxSweepHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
	PxBoxGeometry boxg(0.5f, 0.1f, 0.5f);
	PxTransform pose(PxVec3(GetPosition().x, GetPosition().y+0.5f, GetPosition().z));
	if(g_pPhysicsWorld->PhysXScene->sweepSingle(boxg, pose, PxVec3(0,-1,0), 2000.0f, PxSceneQueryFlag::eIMPACT, hit, filter))
	{
	r3dOutToLog("@@@@ %.2f, %.2f, %.2f\n", hit.impact.x, hit.impact.y, hit.impact.z);
	}
	}
	}*/

	// CHECK CAMERA THROUGH COLLISTION CODE
	/*if (NetworkLocal && !bDead)
	{

	PxRaycastHit hit;

	r3dPoint3D dir;
	r3dPoint3D hitPos;
	r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);

	PhysicsCallbackObject* target = NULL;
	//PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
	// get a pos of camera aiming
	if (g_pPhysicsWorld->raycastSingle(PxVec3(gCam.x, gCam.y, gCam.z), PxVec3(dir.x, dir.y, dir.z), 2000.0f, PxSceneQueryFlag::eIMPACT, hit, filter))
	{

	dir = gCam - r3dPoint3D(hit.impact.x,hit.impact.y,hit.impact.z);
	float dirl = dir.Length(); 
	dir.Normalize();

	// check if he camera through building

	if(g_pPhysicsWorld->raycastSingle(hit.impact, PxVec3(dir.x,dir.y,dir.z), 2000.0f, PxSceneQueryFlag::eIMPACT, hit, filter))
	{
	// get him to sky and go to a death

	SetPosition(GetPosition() + r3dPoint3D(0,512,0));

	}

	}

	}*/

	// CLAN TAG
	if (gClientLogic().localPlayer_)
	{
		if (gClientLogic().localPlayer_ && hudMain && CurLoadout.GroupID == gClientLogic().localPlayer_->CurLoadout.GroupID && !CurLoadout.isGroupShow && CurLoadout.GroupID != 0)
		{
			char plrUserName[64]; GetUserName(plrUserName);
			CurLoadout.isGroupShow = true;
			hudMain->addplayertogroup(plrUserName,CurLoadout.GroupID2 == CurLoadout.GroupID);
		}
		else if (gClientLogic().localPlayer_ && hudMain && CurLoadout.GroupID == 0 && CurLoadout.isGroupShow)
		{
			CurLoadout.isGroupShow = false;
			char plrUserName[64]; GetUserName(plrUserName);
			hudMain->removeplayerfromgroup(plrUserName,false);
		}
		else if (gClientLogic().localPlayer_ && hudMain && gClientLogic().localPlayer_->CurLoadout.GroupID != CurLoadout.GroupID && CurLoadout.GroupID != 0 && CurLoadout.isGroupShow || gClientLogic().localPlayer_->CurLoadout.GroupID == 0 && CurLoadout.GroupID == 0 && CurLoadout.isGroupShow || gClientLogic().localPlayer_->CurLoadout.GroupID != 0 && CurLoadout.GroupID == 0 && CurLoadout.isGroupShow)
		{
			CurLoadout.isGroupShow = false;
			char plrUserName[64]; GetUserName(plrUserName);
			hudMain->removeplayerfromgroup(plrUserName,false);
		}
	}
	if(!NetworkLocal && hudMain && CGL.localPlayer_)
	{
		bool showName1 = (this->GetSafeID() == m_LocalPlayer_CurrentAimAt)?true:false;
		bool isShowGroup = (GetPosition() - CGL.localPlayer_->GetPosition()).Length() <= 512;
		if((m_CharIcon.IsUndefined() && CGL.localPlayer_ && showName1 || ClanID == CGL.localPlayer_->ClanID && ClanID != 0 && m_CharIcon.IsUndefined() || m_CharIcon.IsUndefined() && CurLoadout.GroupID == gClientLogic().localPlayer_->CurLoadout.GroupID && CurLoadout.GroupID != 0) /*&& isShowGroup*/)
		{
			char plrUserName[256]; GetUserNameAndClanTag(plrUserName);

			if (ClanID == CGL.localPlayer_->ClanID && ClanID != 0/* && isShowGroup*/)
			{
				hudMain->addCharTag1(plrUserName, CurLoadout.Stats.Reputation, true, m_CharIcon);
			}
			else
			{
				hudMain->addCharTag1(plrUserName, CurLoadout.Stats.Reputation, false, m_CharIcon);
			}
		}

		//if (!(m_CharIcon.IsUndefined() && CGL.localPlayer_ && showName1 || ClanID == CGL.localPlayer_->ClanID && ClanID != 0 && m_CharIcon.IsUndefined() || m_CharIcon.IsUndefined() && CurLoadout.GroupID == gClientLogic().localPlayer_->CurLoadout.GroupID && CurLoadout.GroupID != 0) && !isShowGroup)
 //                hudMain->removeUserIcon(m_CharIcon);

		if (!showName1 && !m_CharIcon.IsUndefined())
		{
			//bool isShowGroup = (GetPosition() - CGL.localPlayer_->GetPosition()).Length() <= 512;
			// hey not add this! the nameplate cheat will work!
			if (ClanID == CGL.localPlayer_->ClanID && ClanID != 0 || CurLoadout.GroupID == gClientLogic().localPlayer_->CurLoadout.GroupID && CurLoadout.GroupID != 0/* || isShowGroup*/)
			{
			}
			else
				hudMain->removeUserIcon(m_CharIcon);
		}


		if(!m_CharIcon.IsUndefined())
		{
			obj_Player* plr = gClientLogic().localPlayer_;
			bool showTag = true;
			bool alwaysShow = true;
			bool showBool = true;

			if(bDead)
				showTag = false;

			showBool = false;

			char plrUserName[64]; GetUserName(plrUserName);

			bool isShowGroup = (GetPosition() - CGL.localPlayer_->GetPosition()).Length() <= 512;
			if	(CurLoadout.GroupID == CGL.localPlayer_->CurLoadout.GroupID && CurLoadout.GroupID != 0 && isShowGroup)
			{
				hudMain->setCharTagTextVisible1(m_CharIcon, showName1, true);
				hudMain->moveUserIcon(m_CharIcon, GetPosition()+r3dPoint3D(0,2.0f,0), alwaysShow);
			}
			else if (ClanID == CGL.localPlayer_->ClanID && ClanID != 0 && CurLoadout.GroupID == CGL.localPlayer_->CurLoadout.GroupID && CurLoadout.GroupID != 0 && isShowGroup)
			{
				hudMain->setCharTagTextVisible1(m_CharIcon, showName1, true);
				hudMain->moveUserIcon(m_CharIcon, GetPosition()+r3dPoint3D(0,2.0f,0), alwaysShow);
			}
			else if (ClanID == CGL.localPlayer_->ClanID && ClanID != 0 /*&& isShowGroup*/)
			{
				hudMain->setCharTagTextVisible1(m_CharIcon, showName1, false);
				hudMain->moveUserIcon(m_CharIcon, GetPosition()+r3dPoint3D(0,2.0f,0), alwaysShow);
			}
			else
			{
				hudMain->setCharTagTextVisible1(m_CharIcon, showName1, false);
				//hudMain->moveUserIcon(m_CharIcon, GetPosition()+r3dPoint3D(0,2.0f,0), alwaysShow);
				hudMain->moveUserIcon(m_CharIcon, GetPosition()+r3dPoint3D(0,2.0f,0), alwaysShow);
			}
		}
	}



	float timePassed = r3dGetFrameTime();

	// SPREAD/RECOIL LOGIC
	if(NetworkLocal && m_Weapons[m_SelectedWeapon])
	{
		if(CurrentSpread > 0 && !m_isPressedFireTrigger)
		{
			CurrentSpread -= timePassed*SpreadDecrease[m_Weapons[m_SelectedWeapon]->getCategory()-storecat_ASR];
		}
		CurrentSpread = R3D_MAX(CurrentSpread, SpreadMin[m_Weapons[m_SelectedWeapon]->getCategory()-storecat_ASR]*m_Weapons[m_SelectedWeapon]->getSpread());

		//r3dOutToLog("1) Recoil=%.2f, cooldown=%.2f\n", RecoilViewModTarget.y, RecoilCooldown.get());
		// update recoil
		if(!RecoilViewMod.AlmostEqual((RecoilViewModTarget), 0.01f))
		{
			RecoilViewMod = R3D_LERP(RecoilViewMod, RecoilViewModTarget, R3D_MIN(timePassed*GPP->c_fRecoilCooldownSpeed*4.0f * u_GetRandom(0.75f, 1.25f), 1.0f)); // lerp to make it smooth, less jerky
		}
		RecoilViewMod = R3D_LERP(RecoilViewMod, (RecoilViewMod+RecoilViewModTarget2), timePassed*2.0f);

		if(RecoilCooldown>0)
			RecoilCooldown = RecoilCooldown-timePassed;

		if(RecoilCooldown<=0)
		{
			// slowly make recoil go back to zero
			RecoilViewModTarget = R3D_LERP(RecoilViewModTarget, r3dVector(0,0,0), R3D_MIN(timePassed*GPP->c_fRecoilCooldownSpeed*8.0f, 1.0f));
		}
		// recoil effect when you are hit
		RecoilViewModTarget2 = R3D_LERP(RecoilViewModTarget2, r3dVector(0,0,0), R3D_MIN(timePassed*GPP->c_fRecoilCooldownSpeed*2.0f, 1.0f));

		//r3dOutToLog("2) Recoil=%.2f, cooldown=%.2f\n", RecoilViewModTarget.y, RecoilCooldown.get());
	}
	// FINISHED SPREAD/RECOIL LOGIC
	//if (r_hud_filter_mode->GetInt() == 1)
	//hudMain->showMessage(gLangMngr.getString("Turn on NVG"));
	//r_hud_filter_mode->SetInt(0); // turn off NVG
	//hudMain->showMessage(gLangMngr.getString("Turn on NVG"));
	// if mouse is visible - then we are in control menu mode, so DO NOT update player weapon (shooting) or player movement
	// update weapon selection
	if(NetworkLocal && !Mouse->GetMouseVisibility() && !bDead && !(g_pDefaultConsole && g_pDefaultConsole->IsVisible()) && !(hudMain && hudMain->isChatInputActive()))
	{
		// Allright NVG Status Check
		if(CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20188 ||
			CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20187 ||
			CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20067
			)
		{
			//r_hud_filter_mode->SetInt(0); // turn on NVG
			//
		}
		else
		{
			if (r_hud_filter_mode->GetInt() == 1)
			{
				r_hud_filter_mode->SetInt(0);
				hudMain->showMessage(gLangMngr.getString("Turn off NVG"));
			}
		}
		R3DPROFILE_FUNCTION("WeaponSelection");
		int prevSelectedItem = m_SelectedWeapon;
		bool prevEmptyHandsState = forcedEmptyHands;
		{
			int buttonPressed = 0;
			if(PlayerState != PLAYER_MOVE_SPRINT )
			{
				if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_PRIMARY_WEAPON))
					buttonPressed = 1;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_HANDGUN_WEAPON))
					buttonPressed = 2;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_ITEM1))
					buttonPressed = 3;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_ITEM2))
					buttonPressed = 4;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_ITEM3))
					buttonPressed = 5;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_ITEM4))
					buttonPressed = 6;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_QUICK_SWITCH_WEAPON))
				{
					if(m_PrevSelectedWeapon == -1)
						buttonPressed = 1;	// switch to second weapon if no previous weapon was selected
					else
						buttonPressed = m_PrevSelectedWeapon+1; // +1 to map from index to button pressed
				}
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_NEXTITEM))
				{
					// scan for 4 item slots and see if we can switch to next item
					int selWeapon = R3D_MAX(m_SelectedWeapon, 1);
					for(int i=0; i<4; i++)
					{
						if(++selWeapon >= NUM_WEAPONS_ON_PLAYER)
							selWeapon = 2;
						if(m_Weapons[selWeapon] != NULL)
						{
							buttonPressed = selWeapon + 1;
							break;
						}
					}
				}
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_TOGGLE_NIGHTVISION))
				{
					// check if player has night vision goggles
					if(CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20188 ||
						CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20187 ||
						CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20067
						)
						r_hud_filter_mode->SetInt(r_hud_filter_mode->GetInt() == HUDFilter_NightVision ? HUDFilter_Default : HUDFilter_NightVision);
				}
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_FREE_HANDS))
				{
					switchToEmptyHands();
					UpdateLoadoutSlot(CurLoadout); // for this shit will clear a fucking flash light meshes.
				}
				else
				{
					// query mouse scroll
					int mouseScroll = 0;
					Mouse->QueryWheelMotion(&mouseScroll);
					if(Gamepad->WasPressed(gpY))
						mouseScroll = 1;

					if(mouseScroll!=0)
					{
						int selWeapon = m_SelectedWeapon;
						for(int i=0; i<NUM_WEAPONS_ON_PLAYER; i++)
						{
							if(mouseScroll>0) {
								if(++selWeapon >= NUM_WEAPONS_ON_PLAYER)
									selWeapon = 0;
							} else {
								if(--selWeapon < 0)
									selWeapon = NUM_WEAPONS_ON_PLAYER - 1;
							}

							if(m_Weapons[selWeapon] != NULL) {
								buttonPressed = selWeapon + 1;
								break;
							}
						}
					}
				}
			}
			else if ( PlayerState == PLAYER_MOVE_SPRINT ) 
			{
				// clear the mouse wheel. 
				int mouseScroll = 0;
				Mouse->QueryWheelMotion(&mouseScroll);
			}


			r3d_assert(buttonPressed>=0 && buttonPressed <=NUM_WEAPONS_ON_PLAYER);
			const FoodConfig* foodItem = NULL;
			if((buttonPressed-1)>=wiCharDataFull::CHAR_LOADOUT_ITEM1 && (buttonPressed-1)<=wiCharDataFull::CHAR_LOADOUT_ITEM4)
				foodItem = g_pWeaponArmory->getFoodConfig(CurLoadout.Items[buttonPressed-1].itemID);
			if(foodItem)
			{
				wiInventoryItem& wi = CurLoadout.Items[buttonPressed-1];
				if(wi.quantity > 0 && wi.itemID != 301321)
				{
					showSlotsInfoTimer = r3dGetTime() + 5.0f; // show weapon info for X seconds
					if(hudMain)
					{
						hudMain->showSlots(true);
						hudMain->setActivatedSlot(buttonPressed-1);
					}

					if(hudMain->Cooldown > 0 )
					{
						if(hudMain) hudMain->showMessage(gLangMngr.getString("Currenty Cooldown"));
						//return;
					}
					else
					{
						localPlayer_UseItem(buttonPressed-1, foodItem->m_itemID, GetPosition() + GetvForw()*1.0f + r3dPoint3D(0,1,0));

						if(--wi.quantity <= 0) {
							wi.Reset();
							OnBackpackChanged(buttonPressed-1);
						}
						else
						{
							if(hudMain)
								hudMain->updateSlotInfo(buttonPressed-1, wi.quantity);
						}
					}
				}
			}
			else if(buttonPressed && m_Weapons[buttonPressed-1])
			{
				if(buttonPressed>2 && !(m_Weapons[buttonPressed-1]->isUsableItem() || m_Weapons[buttonPressed-1]->getCategory()==storecat_GRENADE))
				{
					// do not allow to switch to quick slots if they are not usable items or grenades
				}
				else
				{
					if((m_Weapons[buttonPressed-1]->isUsableItem() || m_Weapons[buttonPressed-1]->getCategory()==storecat_GRENADE) && InputMappingMngr->isPressed(r3dInputMappingMngr::KS_PRIMARY_FIRE))
					{
						// do not allow switching to usable item while fire key is pressed
					}
					else
					{
						if(ChangeWeaponByIndex(buttonPressed-1))
						{
							lastTimeWeaponSwitch = curTime;
						}
						else if(buttonPressed >= 3 && buttonPressed <=6) // sergey's request: already was pressed and this is an item, simulate left click!
						{
							if(m_Weapons[buttonPressed-1]->isUsableItem())
								m_ItemSimulateLeftClick = true;
						}
					}
				}
			}
		}

		if(prevSelectedItem != m_SelectedWeapon || prevEmptyHandsState != forcedEmptyHands)
		{
			SyncAnimation(true);

			PKT_C2C_PlayerSwitchWeapon_s n;
			n.wid       = forcedEmptyHands?255:m_SelectedWeapon;
			p2pSendToHost(this, &n, sizeof(n), true);
		}
	}

	// WARNING: current model is rotated 180 around y
	D3DXMATRIX mr;
	D3DXMatrixRotationY(&mr, R3D_PI);
	D3DXMATRIX CharDrawMatrix = mr;

	if(NetworkLocal && !Mouse->GetMouseVisibility()) 
	{
		if(m_HitMarkerFadeout > 0)
			m_HitMarkerFadeout -= timePassed * GPP->c_fHitMarkerFadeoutSpeed;

		UpdateTargetting();
	}

	plr_local_moving_speed = (GetPosition() - oldstate.Position).Length();
	if(NetworkLocal) 
	{
		// moved to separate function and called from UpdatePlayerMovement, to make sure that player position and camera position are in sync
	} 
	else if(GetVelocity().LengthSq() > 0.0001f)
	{
		R3DPROFILE_FUNCTION("NetPlayer_Move");
		r3dPoint3D prevPos = GetPosition();
		SetPosition(GetPosition() + GetVelocity() * timePassed);
		DetectIfOnGround();

		// check if we overmoved to target position
		r3dPoint3D v = netMover.GetNetPos() - GetPosition();
		float d = GetVelocity().Dot(v);
		if(d < 0) {
			SetPosition(netMover.GetNetPos());
			SetVelocity(r3dPoint3D(0, 0, 0));
		}

		float ll = (prevPos-GetPosition()).LengthSq();
		if(ll > 0.0001f)
		{
			extern void WaterSplash(const r3dPoint3D& waterSplashPos, float height, float size, float amount, int texIdx);
			WaterSplash(GetPosition(), 2.0f, 5.0f, 0.1f*ll, 3);
		}
	}
	if(afterRespawnTimer>0)
		afterRespawnTimer -= timePassed;
	//r3dOutToLog("Player Update: %s to %.2f, %.2f, %.2f\n", UserName, GetPosition().x, GetPosition().y, GetPosition().z);

	// update footsteps after moving player
	UpdateFootsteps();
	/*
	// update camo only if player has special items
	gCamouflageDataManager.UpdateCamouflageData(*this);
	*/

	UpdateSounds();


	// even if dead, still need to send pos, otherwise if died in the air - you will still be in the air.
	if(NetworkLocal) 
	{
		CNetCellMover::moveData_s md;
		md.pos       = GetPosition();
		md.turnAngle = m_fPlayerRotationTarget;
		md.bendAngle = bodyAdjust_y[1];
		md.state     = (PlayerState&0xF) | ((PlayerMoveDir&0xF) << 4);

		PKT_C2C_MoveSetCell_s n1;
		PKT_C2C_MoveRel_s     n2;
		DWORD pktFlags = netMover.SendPosUpdate(md, &n1, &n2);
		if(pktFlags & 0x1)
			p2pSendToHost(this, &n1, sizeof(n1), true);
		if(pktFlags & 0x2)
			p2pSendToHost(this, &n2, sizeof(n2), false);
	}

	//
	// send hash of currently equipped weapon configs
	//
	if(NetworkLocal && !bDead && curTime >= (lastWeapDataRep + PKT_C2S_PlayerWeapDataRep_s::REPORT_PERIOD))
	{
		r3d_assert(!bDead);
		lastWeapDataRep = curTime;

		PKT_C2S_PlayerWeapDataRep_s n;
		r3d_assert(2 == R3D_ARRAYSIZE(n.weaponsDataHash));
		for(int i=0; i<2; i++)
		{
			n.weaponsDataHash[i] = 0;
			n.debug_wid[i] = 0;
			if(m_Weapons[i] == NULL)
				continue;
			n.weaponsDataHash[i] = m_Weapons[i]->getConfig()->GetClientParametersHash();

			n.debug_wid[i] = m_Weapons[i]->getItemID();
			m_Weapons[i]->getConfig()->copyParametersTo(n.debug_winfo[i]);
		}

		p2pSendToHost(this, &n, sizeof(n), true);
	}

	if(NetworkLocal && hudMain)
	{
		R3DPROFILE_FUNCTION("update hud");
		// update HUD
		{
			{
				float bloodAlpha = 1.0f-R3D_MIN(r3dGetTime()-lastTimeHit, 1.0f);
				hudMain->setBloodAlpha(bloodAlpha);
			}

			bAllowToUseWeapons = true;
			if((CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox) || (CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected))

				bAllowToUseWeapons = false;
			hudMain->showSafeZoneWarning(!bAllowToUseWeapons);

			//hudMain->setVisibility(lifeProperties.getPlayerVisibility());
			//hudMain->setHearing(lifeProperties.getPlayerHearRadius());

			/*int value = 0;


			switch (PlayerState)
			{
			case PLAYER_MOVE_RUN:
			value = 75;
			break;
			case PLAYER_MOVE_PRONE:
			value = 25;
			break;
			case PLAYER_MOVE_SPRINT:
			value = 80;
			break;
			}

			if (lifeProperties.weaponFired)
			{
			value = 100;
			}

			hudMain->setThreatValue(value);*/

			/*	if (d_drive_vehicles)
			{
			bAllowToUseWeapons = false;
			}
			else if (!d_drive_vehicles && !(CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox) || (CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected))
			{
			bAllowToUseWeapons = true;
			}*/

			//hudMain->setTime(CGL.GetServerGameTime());
			if(hudPause->isActive())
				hudPause->setTime(CGL.GetServerGameTime());

			if (CurLoadout.Stats.skillid29 == 1)
			{
				_zai_AttackDamage = 15.0f;
			}

			/*float healthskill; // convert back to rate of fire

			if (CurLoadout.Stats.skillid7 == 1)
			{
			healthskill = (2.0f / (int)CurLoadout.Health); // convert back to rate of fire
			}
			else if (CurLoadout.Stats.skillid0 == 1)
			{
			healthskill = (1.5f / (int)CurLoadout.Health); // convert back to rate of fire
			}
			else
			{
			healthskill = CurLoadout.Health;
			}

			r3dOutToLog("Health : %d ,%d\n",healthskill,(int)CurLoadout.Health);*/
			//hudMain->setConditionIcon("IconBleed",CurLoadout.bleeding);
			hudMain->setConditionIcon("IconInfection",CurLoadout.legfall);
			hudMain->setLifeParams((int)CurLoadout.Hunger, (int)CurLoadout.Thirst, (int)CurLoadout.Health, (int)CurLoadout.Toxic, int(((getMaxStamina()-m_Stamina)/getMaxStamina())*100.0f));
			if(m_Weapons[m_SelectedWeapon])
			{
				if(m_Weapons[m_SelectedWeapon]->getCategory() == storecat_GRENADE) // treated as items
				{
					const wiInventoryItem& wi = m_Weapons[m_SelectedWeapon]->getPlayerItem();
					hudMain->setWeaponInfo(R3D_MIN(1, wi.quantity), R3D_MIN(0, wi.quantity-1), 1);
				}
				else
					hudMain->setWeaponInfo(m_Weapons[m_SelectedWeapon]->getNumBulletsLeft(), m_Weapons[m_SelectedWeapon]->getNumClipsLeft(), (int)m_Weapons[m_SelectedWeapon]->getFiremode());
			}

			if(r3dGetTime() > showSlotsInfoTimer && showSlotsInfoTimer>0)
			{
				hudMain->showSlots(false);
				showSlotsInfoTimer = 0;
			}
		}
		{
			// update items
			for(int i=wiCharDataFull::CHAR_LOADOUT_WEAPON1; i<=wiCharDataFull::CHAR_LOADOUT_ITEM4; ++i)
			{
				const Weapon* wpn = m_Weapons[i];
				if(!wpn) continue;

				if(wpn->isUsableItem())
				{
					hudMain->updateSlotInfo(i, CurLoadout.Items[i].quantity);
				}
			}

			float bloodLevel = 0;
			if(CurLoadout.Health < 25.0f)
				bloodLevel = 100.0f - ((CurLoadout.Health/25.0f)*100.0f); // show blood effect when health is between 0 and 25
			PFX_3DLUTColorCorrection::s_damageHitColorCorrectionVal = (bloodLevel/100.0f);
			//hudMain->SetBloodLevel(bloodLevel);
		}
	}

	// check for dead after moving player, otherwise might die in the air and be stuck in the air (dead body)
	if(bDead)
	{
		if(NetworkLocal)
			PFX_3DLUTColorCorrection::s_damageHitColorCorrectionVal = 0;

		UpdateRotation();

		UpdateAnimations(CharDrawMatrix);

		if(m_Weapons[m_SelectedWeapon])
			m_Weapons[m_SelectedWeapon]->Update(uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix));

		return TRUE;
	}

	if(!NetworkLocal)
	{
		UpdatePlayerObstacle(PhysXObstacleIndex, GetPosition());
	}

	SyncAnimation();

	UpdateRotation();

	UpdateAnimations(CharDrawMatrix);

	if(m_Weapons[m_SelectedWeapon])
	{
		m_Weapons[m_SelectedWeapon]->Update(uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix));
	}

	if(NetworkLocal)
	{
		// breath timer
		/*if(m_BreathPenaltyTime > 0)
		{
		m_BreathPenaltyTime = m_BreathPenaltyTime - r3dGetFrameTime();
		if(m_BreathPenaltyTime < 0)
		m_BreathPenaltyTime = 0;
		}
		else*/
		{
			if(!InputMappingMngr->isPressed(r3dInputMappingMngr::KS_HOLD_BREATH))
			{
				if(m_Breath < getMaxBreath())
					m_Breath = m_Breath + timePassed;
			}
		}
	}

	{
		//if(r3dGetTime() < m_MorhineShotTime && m_AuraType == AT_NONE)
		//	m_AuraType = AT_MORHINE;
		//if(m_AuraType == AT_MORHINE && r3dGetTime() > m_MorhineShotTime)
		//	m_AuraType = AT_NONE;
		/*if(CurLoadout.GameFlags && wiCharDataFull::GAMEFLAG_isSpawnProtected)
		{
		m_AuraType = AT_SPAWNPROTECTION;
		PKT_C2C_Aura_s n;
		p2pSendToHost(this, &n, sizeof(n));
		}*/
		//if(!(CurLoadout.GameFlags && wiCharDataFull::GAMEFLAG_isSpawnProtected) &&  m_AuraType == AT_SPAWNPROTECTION)
		//	m_AuraType = AT_NONE;
	}

	if(NetworkLocal)
	{
		if(BloodEffect > 0)
		{
			BloodEffect -= timePassed;
			if(BloodEffect < 0) BloodEffect = 0;
		}
	}

	// update fire
	if(NetworkLocal && !Mouse->GetMouseVisibility())
	{
		if(fabs(m_ReticleCurrentScale - m_ReticleTargetScale)>0.01f)
			m_ReticleCurrentScale = R3D_LERP(m_ReticleCurrentScale, m_ReticleTargetScale, timePassed * 5.0f);

		bool scopeRendering = (hasScopeMode() && CurrentRig.allowScope && m_isAiming) || (g_camera_mode->GetInt()==2 && m_isAiming);
	}

	// if low health - start dropping blood every second
	m_BloodTimer += timePassed;
	if(CurLoadout.Health < 30 && m_BloodTimer > 1.0f)
	{
		r3dPoint3D pos = GetPosition(); pos.y += 2.0f;
		AddBloodOnGround(pos);
		m_BloodTimer = 0;
	}

	if(NetworkLocal)
		UpdateActionUI();

	return TRUE;
}

float dist_Point_to_Line( const r3dPoint3D& p, const r3dPoint3D& l1, const r3dPoint3D& l2, bool clampToSegment=false)
{
	r3dVector v = l2 - l1;
	r3dVector w = p - l1;

	float c1 = w.Dot(v);
	float c2 = v.Dot(v);
	float b = c1 / c2;
	if(clampToSegment)
		b = R3D_CLAMP(b, 0.0f, 1.0f);

	r3dPoint3D Pb = l1 + b * v;
	return (p-Pb).Length();
}

void obj_Player::UpdateActionUI()
{
	R3DPROFILE_FUNCTION("UpdateActionUI");
	r3d_assert(NetworkLocal);
	if(hudMain==NULL || hudActionUI == NULL || hudPause == NULL ||
		hudAttm == NULL || hudGeneralStore == NULL || hudVault == NULL || hudTrade == NULL)
		return;

	if(hudMain->isChatInputActive())
		return;
	if(hudPause->isActive())
		return;
	if(hudAttm->isActive())
		return;
	if(hudGeneralStore->isActive())
		return;
	if(hudVault->isActive())
		return;
	if(hudTrade->isActive())
		return;

	if(bDead && hudActionUI->isActive())
		hudActionUI->Deactivate();
	if(bDead)
		return;

	float maxDist = g_camera_mode->GetInt()==2?3.0f:4.0f;

	r3dVector forw = gCam.vPointTo;//GetvForw();
	r3dVector searchStartLine = gCam;
	r3dVector searchEndLine = searchStartLine + forw*maxDist;

	// slow, need to rewrite
	GameObject* dropObj = 0;
	float distance = 999999999.0f;

	float curTime = r3dGetTime();

	bool selectHumans = false;
	{
		uint32_t wpnItemID = 0;
		if(m_Weapons[m_SelectedWeapon])
			wpnItemID = m_Weapons[m_SelectedWeapon]->getItemID();
		selectHumans = wpnItemID==WeaponConfig::ITEMID_Antibiotics||wpnItemID==WeaponConfig::ITEMID_Bandages||wpnItemID==WeaponConfig::ITEMID_Bandages2||
			wpnItemID==WeaponConfig::ITEMID_Painkillers||wpnItemID==WeaponConfig::ITEMID_ZombieRepellent||wpnItemID==WeaponConfig::ITEMID_C01Vaccine||
			wpnItemID==WeaponConfig::ITEMID_C04Vaccine||wpnItemID==WeaponConfig::ITEMID_Medkit;
		if(m_Weapons[m_SelectedWeapon])
		{
			wiInventoryItem wi = CurLoadout.Items[m_Weapons[m_SelectedWeapon]->m_BackpackIdx];
			if(wi.quantity == 0)
				selectHumans = false;
		}
	}

	/*	ObjectManager& GW = GameWorld();
	for(GameObject *obj = GW.GetFirstObject(); obj; obj = GW.GetNextObject(obj))
	{
	if(obj->isObjType(OBJTYPE_SharedUsableItem) || (obj->isObjType(OBJTYPE_Human)&&selectHumans&&obj!=this))
	{
	float d = dist_Point_to_Line(obj->GetBBoxWorld().Center(), searchStartLine, searchEndLine, true); 
	if(distance > d)
	{
	if(obj->isObjType(OBJTYPE_SharedUsableItem))
	{
	if(((SharedUsableItem*)obj)->m_lastPickupAttempt < curTime)
	{
	dropObj = obj;
	distance = d;
	}
	}
	else // human
	{
	dropObj = obj;
	distance = d;
	}
	}
	}
	}*/
	{
		std::list<SharedUsableItem*>::iterator it;
		for(it=m_SharedUsableItemList.begin(); it!=m_SharedUsableItemList.end(); ++it)
		{
			SharedUsableItem* obj = *it;
			r3dPoint3D objCenter = obj->GetBBoxWorld().Center();
			float d = dist_Point_to_Line(objCenter, searchStartLine, searchEndLine, true); 
			if(distance > d)
			{
				// check dist to player
				if((objCenter - GetPosition()).LengthSq() > (maxDist*maxDist))
					continue;
				// check if both player and camera are facing object
				if((GetPosition()-objCenter).Dot(gCam-objCenter) < 0.0f) 
					continue;
				if(obj->m_lastPickupAttempt < curTime)
				{
					dropObj = obj;
					distance = d;
				}
			}
		}
		//	if(selectHumans)
		//	{
		ClientGameLogic& CGL = gClientLogic();
		for(int i=0; i<ClientGameLogic::MAX_NUM_PLAYERS; ++i)
		{
			obj_Player* plr = CGL.GetPlayer(i);
			if(plr && plr != this)
			{
				float d = dist_Point_to_Line(plr->GetBBoxWorld().Center(), searchStartLine, searchEndLine, true); 
				if(distance > d)
				{
					dropObj = plr;
					distance = d;
				}
			}

		}
	}

	//}


	static float pressedKeyTimer = 0.0f;

	/*if (d_drive_vehicles->GetBool())
	{
	if(!hudActionUI->isActive() )
	{
	hudActionUI->Activate();
	hudActionUI->setText(L"Vehicles", L"Hold E To Exit Vehicles", InputMappingMngr->getKeyName(r3dInputMappingMngr::KS_INTERACT));
	hudActionUI->enableRegularBlock();
	}
	if(hudActionUI->isActive())
	{
	r3dPoint3D scrPos;
	r3dProjectToScreen(dropObj->GetPosition(), &scrPos);
	hudActionUI->setScreenPos((int)scrPos.x, (int)scrPos.y);
	}
	}*/
	if(distance < 1.0f && !d_drive_vehicles->GetBool()) // distance to ray. if more than 1, then it is outside of ray
	{
		// in case if you are standing right next to pile of objects, we need to re-init ui each time we pick different item from pile
		static GameObject* prevHUDItem = NULL;
		if(prevHUDItem != dropObj && hudActionUI->isActive())
			hudActionUI->Deactivate();
		prevHUDItem = dropObj;


		if(!hudActionUI->isActive())
		{
			hudActionUI->Activate();

			if(dropObj->isObjType(OBJTYPE_SharedUsableItem))
			{
				//	hudActionUI->setCarInfo(0,0,0,0,0,false);
				hudActionUI->setText(((SharedUsableItem*)dropObj)->m_ActionUI_Title, ((SharedUsableItem*)dropObj)->m_ActionUI_Msg, strcmp(dropObj->Class->Name.c_str(),"obj_Grave")?InputMappingMngr->getKeyName(r3dInputMappingMngr::KS_INTERACT) : "Y");
			}
			else if(dropObj->Class->Name == "obj_Vehicle")
			{
				obj_Vehicle* vehicle = (obj_Vehicle*)dropObj;
				hudActionUI->setText(L"Vehicles", L"Hold E To Enter Vehicles", InputMappingMngr->getKeyName(r3dInputMappingMngr::KS_INTERACT));
				//hudActionUI->setCarInfo(5,10,20,10,50,true);
			}
			else if (selectHumans)// human have weapons
			{
				//hudActionUI->setCarInfo(0,0,0,0,0,false);
				hudActionUI->setText(L"USE ITEM", m_Weapons[m_SelectedWeapon]->getConfig()->m_StoreNameW, InputMappingMngr->getKeyName(r3dInputMappingMngr::KS_INTERACT));
			}
			else if (dropObj->Class->Name == "obj_Player")
			{
				obj_Player* plr = (obj_Player*)dropObj;
				hudActionUI->setCarInfo(0,0,0,0,0,false);
				hudActionUI->setText(L"Trade", L"Hold 'E' To Trade", InputMappingMngr->getKeyName(r3dInputMappingMngr::KS_INTERACT));
			}
			hudActionUI->enableRegularBlock();
		}
		if(hudActionUI->isActive())
		{
			hudActionUI->showEBlock(true,true);
			/*if(dropObj->Class->Name == "obj_Vehicle")
			{
			obj_Vehicle* vehicle = (obj_Vehicle*)dropObj;
			hudActionUI->setCarInfo((int)vehicle->dura/20,85,40,80,(int)vehicle->fuel,true);
			}
			else
			hudActionUI->setCarInfo(0,0,0,0,0,false);*/
			r3dPoint3D scrPos;
			r3dProjectToScreen(dropObj->GetBBoxWorld().Center(), &scrPos);
			hudActionUI->setScreenPos((int)scrPos.x, (int)scrPos.y);
		}

		bool isPress = strcmp(dropObj->Class->Name.c_str(),"obj_Grave")?InputMappingMngr->isPressed(r3dInputMappingMngr::KS_INTERACT):Keyboard->IsPressed(kbsY);
		if(isPress)
		{
			hudActionUI->enableProgressBlock();
			hudActionUI->setProgress(int(pressedKeyTimer*100));
			pressedKeyTimer += r3dGetFrameTime();
			// send request to pickup weapon
			if(pressedKeyTimer > 1.0f) 
			{
				pressedKeyTimer = 0.0f;
				if(dropObj->isObjType(OBJTYPE_SharedUsableItem))
					((SharedUsableItem*)dropObj)->m_lastPickupAttempt = curTime+1.0f; // do not attempt to pick this item for 1 sec (wait for server)
				// hack for notes
				if(dropObj->Class->Name == "obj_Note" || dropObj->Class->Name == "obj_PermanentNote")
				{
					obj_Note* note = (obj_Note*)dropObj;
					if(note->m_GotData) // do not send packet to server again
					{
						hudMain->showReadNote(note->m_Text.c_str());
						hudActionUI->Deactivate();
						return;
					}
				}
				if(dropObj->Class->Name == "obj_Vehicle")
				{
					obj_Vehicle* target_Vehicle = (obj_Vehicle*)dropObj;
					/*if ( target_Vehicle  ) // now we're going to try to use vehicles (otherwise UAV characters can't use vehicles).
					{
					if ( target_Vehicle->status )
					{
					//hudMain->showMessage(L"Cannot enter vehicle");
					curcar = target_Vehicle;
					vehicleViewActive_ = VehicleView_Passenger;

					TogglePhysicsSimulation(false);
					PKT_C2C_CarPass_s n;
					n.NetID = toP2pNetId(target_Vehicle->GetNetworkID());
					p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
					hudActionUI->Deactivate();
					return;
					}

					hudActionUI->showEBlock(false,false);
					hudActionUI->Deactivate();

					d_drive_vehiclescon->SetBool(true);
					//d_drive_vehicles->SetBool(true);
					g_pPhysicsWorld->m_VehicleManager->DriveCar(target_Vehicle->vd);
					curcar = target_Vehicle;
					enterVehicle( target_Vehicle );
					/*TogglePhysicsSimulation(false);
					target_Vehicle->SwitchToDrivable( true );
					vehicleViewActive_ = VehicleView_Driver;
					target_Vehicle->NetworkLocal = true;
					curcar = target_Vehicle;
					PKT_C2C_CarStatus_s n;
					n.CarID = toP2pNetId(target_Vehicle->GetNetworkID());
					p2pSendToHost(this,&n,sizeof(n));
					}*/
					SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggyEngine_Start"),GetPosition());
					return;
				}
				if(dropObj->Class->Name == "obj_SafeLock")
				{
					hudMain->showSL(true,true);
				}
				if(dropObj->Class->Name == "obj_Grave")
				{
					obj_Grave* Grave = (obj_Grave*)dropObj;
					if (Grave)
					{
						if(Grave->m_GotData) // do not send packet to server again
						{
							hudMain->showGraveNote(Grave->m_plr1.c_str(),Grave->m_plr2.c_str());
							hudActionUI->Deactivate();
							return;
						}
					}
					//hudMain->showGraveNote("imFaya","Killed By Batman1");
					hudActionUI->Deactivate();
				}
				if(dropObj->isObjType(OBJTYPE_NPC))
				{
					((NPCObject*)dropObj)->OnAction();
				}
				else if(dropObj->isObjType(OBJTYPE_SharedUsableItem))
				{
					PKT_C2S_UseNetObject_s n;
					n.spawnID = toP2pNetId(dropObj->GetNetworkID());
					p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
					// Mission Lets play - PICK UP ITEM
					if (gClientLogic().localPlayer_->CurLoadout.Mission1 == 1)
					{
						PKT_C2s_PlayerSetMissionStatus_s n1;
						n1.id = 1;
						n1.status = 2;
						p2pSendToHost(gClientLogic().localPlayer_, &n1, sizeof(n1));
						hudMain->setMissionObjectiveCompleted(0,0);
						gClientLogic().localPlayer_->CurLoadout.Mission1 = 2;
					}
				}
				else if (selectHumans) // Heal human
				{
					DWORD netID = dropObj->GetNetworkID();
					wiInventoryItem& wi = CurLoadout.Items[m_Weapons[m_SelectedWeapon]->m_BackpackIdx];
					localPlayer_UseItem(m_Weapons[m_SelectedWeapon]->m_BackpackIdx, m_Weapons[m_SelectedWeapon]->getItemID(), dropObj->GetPosition(), 0.0f, 0.0f, 0.0f, netID);
					if(--wi.quantity <= 0) {
						wi.Reset();
						OnBackpackChanged(m_Weapons[m_SelectedWeapon]->m_BackpackIdx);
					}
					else
					{
						if(hudMain)
							hudMain->updateSlotInfo(m_Weapons[m_SelectedWeapon]->m_BackpackIdx, wi.quantity);
					}
				}
				else if (dropObj->Class->Name == "obj_Player")
				{
					obj_Player* plr = (obj_Player*)dropObj;
					if (plr->CurLoadout.isTradeReq)
					{
						PKT_C2S_TradeAccept_s n;
						n.netId = plr->GetNetworkID();
						p2pSendToHost(this,&n,sizeof(n));

						plr->CurLoadout.isTradeReq = false;
						hudTrade->plr = plr;
						if (hudTrade->plr)
							hudTrade->Activate();
					}
					else
					{
						PKT_C2S_TradeRequest_s n;
						n.netId = (int)plr->GetNetworkID();
						p2pSendToHost(this,&n,sizeof(n));
						char msg[128];
						char name[128]; plr->GetUserName(name);
						sprintf(msg,"Sent trade request to %s",name);
						hudMain->addChatMessage(0,"<trade>",msg,0);
					}
				}

				hudActionUI->Deactivate();
			}
		}
		else
		{
			hudActionUI->enableRegularBlock();
			pressedKeyTimer = 0.0f;
		}
	}
	else
	{
		if(hudActionUI->isActive())
		{
			hudActionUI->Deactivate();
		}
		pressedKeyTimer = 0.0f;
	}
}

void obj_Player::AddBloodOnGround(const r3dPoint3D& posOfShot)
{
	{
		r3dPoint3D blood_pos;
		r3dPoint3D blood_norm;
		bool has_collision = false;
		{
			PxRaycastHit hit;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
			if(g_pPhysicsWorld->raycastSingle(PxVec3(posOfShot.x, posOfShot.y, posOfShot.z), PxVec3(0, -1, 0), 10.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eNORMAL), hit, filter))
			{
				blood_pos = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);
				blood_norm = r3dPoint3D(hit.normal.x, hit.normal.y, hit.normal.z);
				has_collision = true;
			}
			else
			{
				//WTF??? How no collision? Is player up in the sky??
			}
		}
		if(has_collision)
		{
			DecalParams params;
			params.Pos		= blood_pos;
			params.Dir		= blood_norm;
			params.TypeID	= GetDecalID( r3dHash::MakeHash("default"), r3dHash::MakeHash("blood_ground") );
			if( params.TypeID != INVALID_DECAL_ID )
				g_pDecalChief->Add( params );
		}
	}
}
void obj_Player::OnNetPacket(const PKT_C2S_TradeAccept_s& n)
{
	GameObject* obj = GameWorld().GetNetworkObject(n.netId);
	if (obj && obj->Class->Name == "obj_Player")
	{
		obj_Player* plr = (obj_Player*)obj;
		hudTrade->plr = plr;
		hudTrade->Activate();
	}
}
void obj_Player::OnNetPacket(const PKT_C2S_TradeOptoBack_s& n)
{
	hudTrade->OnNetPacket(n);
}
void obj_Player::OnNetPacket(const PKT_C2S_TradeSuccess_s& n)
{
	hudTrade->Deactivate();
}
void obj_Player::OnNetPacket(const PKT_C2S_TradeAccept2_s& n)
{
	hudTrade->setOppositeTradeIndicator(true);
}
void obj_Player::OnNetPacket(const PKT_C2S_TradeBacktoOp_s& n)
{
	hudTrade->OnNetPacket(n);
}
void obj_Player::OnNetPacket(const PKT_C2S_TradeCancel_s& n)
{
	hudTrade->Deactivate();
	hudMain->addChatMessage(0,"<trade>","Other player cancelled trade",0);
}
void obj_Player::OnNetPacket(const PKT_S2C_MoveTeleport_s& n)
{
	TeleportPlayer(n.teleport_pos, "PlayerMoveTeleport");
}

void obj_Player::OnNetPacket(const PKT_C2C_MoveSetCell_s& n)
{
	netMover.SetCell(n);
}

void obj_Player::OnNetPacket(const PKT_C2C_MoveRel_s& n)
{
	const CNetCellMover::moveData_s& md = netMover.DecodeMove(n);

	if(afterRespawnTimer>0) // check for old player moves and discard them
	{
		if((md.pos - GetPosition()).Length() > 10.0f)
			return;
	}

	m_fPlayerRotationTarget = md.turnAngle;
	bodyAdjust_y[1]         = md.bendAngle;
	PlayerState             = (md.state & 0xF);
	PlayerMoveDir           = (md.state >> 4) & 0xF;

	if(PlayerState == PLAYER_IDLE) {
		// limit idle bend angle to some reasonable value in IDLE mode - by some reason animation look like crap when bending there.
		bodyAdjust_y[1] = R3D_CLAMP(bodyAdjust_y[1], -0.4f, 0.4f);
	}

	// calc velocity to reach position on time for next update
	r3dPoint3D vel = netMover.GetVelocityToNetTarget(
		GetPosition(),
		GPP->AI_SPRINT_SPEED * 1.5f,
		1.0f);

	SetVelocity(vel);
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerJump_s& n)
{
	uberAnim_->StartJump();
	return;
}


void obj_Player::ProcessRewardQueue()
{
	r3d_assert(NetworkLocal);

	if(rewards_.size() == 0)
		return;

	// one reward show per sec
	const float curTime = r3dGetTime();
	if(curTime < lastRewardShowTime_ + 1.0f)
		return;
	lastRewardShowTime_ = curTime;

	PKT_S2C_AddScore_s n = *rewards_.begin();
	rewards_.erase(rewards_.begin());

	if((n.ID & 0x8000) == 0)
	{
		GameStats.XP += n.XP;
		GameStats.GD += n.GD;
	}
	else
	{
		// reward without adding score/gd
		n.ID &= ~0x8000;
	}

	if(!g_GameRewards) // can happen in server quickjoin from Main_Network
		return;

	const CGameRewards::rwd_s& rwd = g_GameRewards->GetRewardData(n.ID);

	char rewardName[128] = "";	// localized reward name
	char iconName[128] = "";
	if(rwd.IsSet)
	{
		sprintf(rewardName, "Reward_%s", rwd.Name.c_str());
		sprintf(iconName, "$Data/Menu/Rewards/RWD_%s.png", rwd.Name.c_str());
	}
	else
	{
		sprintf(rewardName, "RWD_%d", n.ID);
	}

	// and always show some crap about that reward
	const wchar_t* rewardString=gLangMngr.getString(rewardName);
	//	hudMain->ShowAchievementCustom(rewardString, "", iconName, "");

	//	hudMain->ShowScore(n.XP, 0, n.GD, GameStats.GD, GameStats.XP);

	wchar_t tmpStr[64];
	swprintf(tmpStr, 64, gLangMngr.getString("InfoMsg_XPAdded"), n.XP);
	hudMain->showMessage(tmpStr);
}

void obj_Player::OnNetPacket(const PKT_S2C_AddScore_s& n)
{
	if(NetworkLocal)
	{
		rewards_.push_back(n);
	}
	else
	{
		GameStats.XP += n.XP;
		GameStats.GD += n.GD;
	}
}
void obj_Player::OnNetPacket(const PKT_C2S_GroupKick_s& n)
{
	//wchar_t msg[64];
	//swprintf(msg,64,L"%s will be kicked from group",gClientLogic().playerNames[n.peerId].Gamertag);
	//hudMain->showMessage(msg);
	char msg[128];
	sprintf(msg,"%s will be kicked from group",gClientLogic().playerNames[n.peerId].Gamertag);
	hudMain->addChatMessage(3,"<group>",msg,0);
	hudMain->aboutToLeavePlayerFromGroup(gClientLogic().playerNames[n.peerId].Gamertag);
}
void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerGroupID_s& n)
{
	/*	if (NetworkLocal)
	{
	return;
	}*/
	r3dOutToLog("%d %d\n",n.GroupID,CustomerID);
	CurLoadout.GroupID = n.GroupID;
}
void obj_Player::OnNetPacket(const PKT_C2S_GroupNoAccept_s& n)
{
	char msg[64];
	sprintf(msg,"No group invite from player %s",gClientLogic().playerNames[n.peerId].Gamertag);
	hudMain->addChatMessage(3,"<group>",msg,0);
}
void obj_Player::OnNetPacket(const PKT_C2S_GroupReInvite_s& n)
{
	gClientLogic().playerNames[n.peerId].isInvitePending = true;
	hudMain->showMessage(gLangMngr.getString("GroupRcvdInviteFrom"));
}
void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerVitals_s& n)
{
	CurLoadout.Health = (float)n.Health;
	CurLoadout.Thirst = (float)n.Thirst;
	CurLoadout.Hunger = (float)n.Hunger;
	CurLoadout.Toxic  = (float)n.Toxic;
	CurLoadout.GroupID = n.GroupID;
	CurLoadout.bleeding = n.bleeding;
	CurLoadout.legfall = n.legfall;
	CurLoadout.IsCallForHelp = n.IsCallForHelp;
	CurLoadout.isVisible = n.isVisible;
	CurLoadout.GroupID2 = n.GroupID2;

	char username[64]; GetUserName(username);
	for(int i=0; i<R3D_ARRAYSIZE(gClientLogic().playerNames); i++)
	{
		if (!strcmp(username,gClientLogic().playerNames[i].Gamertag))
		{
			gClientLogic().playerNames[i].GroupID = n.GroupID;
			break;
		}
	}
}

void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerLoadout_s& n)
{
	// only remote players can change loadout this way
	if(NetworkLocal)
		return;

	// create new loadout
	wiCharDataFull slot = CurLoadout;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1 ].itemID = n.WeaponID0;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2 ].itemID = n.WeaponID1;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM1   ].itemID = n.QuickSlot1;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM2   ].itemID = n.QuickSlot2;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM3   ].itemID = n.QuickSlot3;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM4   ].itemID = n.QuickSlot4;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ARMOR   ].itemID = n.ArmorID;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID = n.HeadGearID;
	slot.BackpackID = n.BackpackID;

	UpdateLoadoutSlot(slot);
}

void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerAttachments_s& n)
{
	// only remote players can change loadout this way
	if(NetworkLocal)
		return;

	r3d_assert(n.wid == wiCharDataFull::CHAR_LOADOUT_WEAPON1 || n.wid == wiCharDataFull::CHAR_LOADOUT_WEAPON2);
	Weapon* wpn = m_Weapons[n.wid];
	if(!wpn) {
		return;
	}

	// set wpn attachment
	CurLoadout.Attachment[n.wid].attachments[WPN_ATTM_MUZZLE]    = n.Attm.MuzzleID;
	CurLoadout.Attachment[n.wid].attachments[WPN_ATTM_LEFT_RAIL] = n.Attm.LeftRailID;
	wpn->setWeaponAttachmentsByIDs(CurLoadout.Attachment[n.wid].attachments);

	// workaround to enable flashlight particle (OnEquip will enable flashlight light)!
	// also we assume that remote player can change attachment only for currently equipped weapon
	wpn->OnUnequip();
	wpn->OnEquip();
}
void obj_Player::OnNetPacket(const PKT_C2S_GroupSwapLeader_s& n)
{
	hudMain->removegrouplist(gClientLogic().playerNames[n.peerId].Gamertag);
	hudMain->addplayertogroup(gClientLogic().playerNames[n.peerId].Gamertag,true);

	// fucking bugs we need to set leader is already added if not, this will add as member of groups.

	ClientGameLogic& CGL = gClientLogic();
	for(int i=0; i<ClientGameLogic::MAX_NUM_PLAYERS; ++i)
	{
		obj_Player* plr = CGL.GetPlayer(i);
		if (plr)
		{
			// i think the client already have leader on world. the server will adding member of group when client joining group.
			// if not , not worry , the server will send new group id again and Update() will add this as leader.
			char username[64]; plr->GetUserName(username);
			if (!strcmp(username,gClientLogic().playerNames[n.peerId].Gamertag))
			{
				plr->CurLoadout.isGroupShow = true;
				break;
			}
		}
	}

	//char msg[64];
	//sprintf(msg,"Swap Leader to %s",gClientLogic().playerNames[n.peerId].Gamertag);
	//hudMain->addChatMessage(3,"<group>",msg,0);
}
void obj_Player::OnNetPacket(const PKT_C2S_GroupID_s& n)
{
	gClientLogic().playerNames[n.peerId].GroupID = n.GroupID;
}
void obj_Player::OnNetPacket(const PKT_C2C_CarPass_s& n)
{
	if (NetworkLocal) return;

	//r3dOutToLog("CarPass\n");

	if (n.NetID == 0)
	{
		TogglePhysicsSimulation(true);
		curcar = NULL;
	}

	GameObject* obj = GameWorld().GetNetworkObject(n.NetID);
	if (obj) {
		obj_Vehicle* vehicle = (obj_Vehicle*)obj;
		//if (vehicle == gClientLogic().localPlayer_->curcar){
		//r3dOutToLog("TogglePhysicsSimulation False\n");
		curcar = vehicle;
		TogglePhysicsSimulation(false);
		//}
	}
}
void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerWorldFlags_s& n)
{
	CurLoadout.GameFlags = n.flags;
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerSwitchWeapon_s& n)
{
	r3d_assert(!NetworkLocal);

	if(n.wid == 255)
	{
		switchToEmptyHands();
		UpdateLoadoutSlot(CurLoadout); // for this shit will clear a fucking flash light meshes.
		return;
	}

	r3d_assert(m_Weapons[n.wid]);

	if(m_Weapons[n.wid] == NULL)
	{
#ifndef FINAL_BUILD
		// do not crash in public build
		char plrUserName[64]; GetUserName(plrUserName);
		r3dOutToLog("NET_DEBUG: %s tried to change to empty weapon slot %d", plrUserName, n.wid);
#endif
	}
	else
	{
		uberAnim_->StopGrenadeAnimations();	// we now can switch weapon while grenade animation active, so need to abort

		ChangeWeaponByIndex(n.wid);
		SyncAnimation(true);
	}
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerReadyGrenade_s& n)
{
	//r3dOutToLog("PKT_C2C_PlayerReadyGrenade\n");
	m_SelectedWeapon = n.wid;
	r3d_assert(m_SelectedWeapon>=0 && m_SelectedWeapon < NUM_WEAPONS_ON_PLAYER);
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	uberAnim_->StartGrenadePinPullAnimation();
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerThrewGrenade_s& n)
{
	m_SelectedWeapon = n.debug_wid;
	r3d_assert(m_SelectedWeapon>=0 && m_SelectedWeapon < NUM_WEAPONS_ON_PLAYER);
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	D3DXMATRIX fireFrom ;
	fireFrom = DrawRotMatrix ;

	fireFrom._41 = n.fire_from.x ;
	fireFrom._42 = n.fire_from.y ;
	fireFrom._43 = n.fire_from.z ;

	// get the weapon bone.  Fire requires it. 
	D3DXMATRIX weaponBone = uberEquip_->getWeaponBone(GetSkeleton(),fireFrom);

	// remove translation, the rotation is all we wanted for grenades.
	weaponBone._41 = n.fire_from.x ;
	weaponBone._42 = n.fire_from.y ;
	weaponBone._43 = n.fire_from.z ;

	m_Weapons[m_SelectedWeapon]->Fire(n.fire_to, weaponBone, true, n.holding_delay, n.fire_from);

	uberAnim_->StartGrenadeThrowAnimation();
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerReload_s& n)
{
	r3d_assert(!NetworkLocal);

	Weapon* wpn = m_Weapons[n.WeaponSlot];
	if(!wpn) return;

	wpn->StartReloadSequence();
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerFired_s& n)
{
	m_SelectedWeapon = n.debug_wid;
	//r3d_assert(m_SelectedWeapon>=0 && m_SelectedWeapon < NUM_WEAPONS_ON_PLAYER);
	//r3d_assert(m_Weapons[m_SelectedWeapon]);

	D3DXMATRIX fireFrom ;
	fireFrom = DrawRotMatrix ;

	fireFrom._41 = n.fire_from.x ;
	fireFrom._42 = n.fire_from.y ;
	fireFrom._43 = n.fire_from.z ;

	// get the weapon bone.  Fire requires it. 
	D3DXMATRIX weaponBone = uberEquip_->getWeaponBone(GetSkeleton(),fireFrom);

	if( m_Weapons[m_SelectedWeapon]->getCategory() == storecat_GRENADE ) {
		// remove translation, the rotation is all we wanted for grenades.
		weaponBone._41 = n.fire_from.x ;
		weaponBone._42 = n.fire_from.y ;
		weaponBone._43 = n.fire_from.z ;
	}

	m_Weapons[m_SelectedWeapon]->Fire(n.fire_to, weaponBone, n.execWeaponFire?true:false, n.holding_delay, n.fire_from);

	uberAnim_->StartRecoilAnim();
}
void obj_Player::OnNetPacket(const PKT_C2S_TradeRequest_s& n)
{
	GameObject* obj = GameWorld().GetNetworkObject((DWORD)n.netId);
	if (obj && obj->Class->Name == "obj_Player")
	{
		obj_Player* plr = (obj_Player*)obj;
		char msg[64];
		char username[64];
		plr->GetUserName(username);
		sprintf(msg,"Trade requested by %s",username);
		plr->CurLoadout.isTradeReq = true;
		hudMain->addChatMessage(0,"<trade>",msg,0);
	}
}
void obj_Player::OnNetPacket(const PKT_C2C_PlayerHitNothing_s& n)
{
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerHitStatic_s& n)
{
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	if(!m_Weapons[m_SelectedWeapon]) return;

	const ClientGameLogic& CGL = gClientLogic();

	// create decal
	if(n.decalIdx != 0 )
	{
		DecalParams params;
		params.Dir	= n.hit_norm;
		params.Pos	= n.hit_pos;
		params.TypeID	= (uint32_t)(n.decalIdx) - 1;
		if(params.TypeID >= 0 && params.TypeID < (int)g_pDecalChief->GetTypeCount())
		{
			g_pDecalChief->Add( params );
		}
		else
		{
			// some cheat or invalid data
		}
	}

	// impact particle
	if(n.particleIdx != 0)
	{
		uint32_t particleIdx = (uint32_t)(n.particleIdx) - 1;
		if(particleIdx < ImpactParticleEntry::NumRegistered)
		{
			SpawnImpactParticle(particleIdx, n.hit_pos, n.hit_norm);
		}
		else
		{
			// some cheat or invalid data
		}
	}

	if(GameObject* from = GameWorld().GetNetworkObject(n.FromID))
	{
		r3dPoint3D waterSplashPos;
		extern bool TraceWater(const r3dPoint3D& start, const r3dPoint3D& finish, r3dPoint3D& waterSplashPos);
		if( TraceWater( from->GetPosition(), n.hit_pos, waterSplashPos))
		{	
			extern void WaterSplash(const r3dPoint3D& waterSplashPos, float height, float size, float amount, int texIdx);
			waterSplashPos.y -= 0.1f;
			WaterSplash(waterSplashPos, 0.1f, 30.0f, 0.04f, 2);
		}
	}

	// add impulse
	GameObject* target = GameWorld().GetObjectByHash(n.hash_obj);
	if(m_Weapons[m_SelectedWeapon]->isImmediateShooting() && target)
		if(target->PhysicsObject)
			target->PhysicsObject->AddImpulseAtPos(-n.hit_norm*m_Weapons[m_SelectedWeapon]->getAmmoSpeed()*m_Weapons[m_SelectedWeapon]->getAmmoMass()/150.0f, n.hit_pos);

	// play near miss sound
	if(m_Weapons[m_SelectedWeapon]->isImmediateShooting())
	{
		if(CGL.localPlayer_)
		{
			float dist = dist_Point_to_Line(CGL.localPlayer_->GetPosition(), GetPosition(), n.hit_pos);
			if(dist < 5.0f && rand()%5==0)
			{
				static int bulletHitMissSoundID = -1;
				if(bulletHitMissSoundID==-1)
					bulletHitMissSoundID = SoundSys.GetEventIDByPath("Sounds/BulletHits/Miss");
				SoundSys.PlayAndForget(bulletHitMissSoundID, CGL.localPlayer_->GetPosition());
			}
		}
	}
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerHitStaticPierced_s& n)
{
	PKT_C2C_PlayerHitStatic_s n2;
	r3d_assert(sizeof(n) == sizeof(n2));
	memcpy(&n2, &n, sizeof(n));
	OnNetPacket(n2);
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerHitDynamic_s& n)
{
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	r3dPoint3D dir = (GetPosition()-n.hit_pos).NormalizeTo();

	GameObject* target = GameWorld().GetNetworkObject(n.targetId);
	if(!target) // current player doesn't see target
		return;

	const ClientGameLogic& CGL = gClientLogic();

	// if we hit local player, display hit effect
	if(target->isObjType(OBJTYPE_Human))
	{
		AddBloodOnGround(n.hit_pos);
		SpawnImpactParticle(r3dHash::MakeHash("player"), r3dHash::MakeHash(m_Weapons[m_SelectedWeapon]->getDecalSource()), n.hit_pos, dir);

		if(target->NetworkLocal == false || CGL.localPlayer_==NULL) 
			return;

		// if hit local player
		extern void TPSGameHUD_AddHitEffect(GameObject* from);
		TPSGameHUD_AddHitEffect(this);
	}
	else if(target->Class->Name == "obj_Animals")
	{
		AddBloodOnGround(n.hit_pos);
		SpawnImpactParticle(r3dHash::MakeHash("player"), r3dHash::MakeHash(m_Weapons[m_SelectedWeapon]->getDecalSource()), n.hit_pos, dir);
	}
	else if(target->isObjType(OBJTYPE_Zombie))
	{
		// hit zombie
		AddBloodOnGround(n.hit_pos);
		SpawnImpactParticle(r3dHash::MakeHash("zombie"), r3dHash::MakeHash(m_Weapons[m_SelectedWeapon]->getDecalSource()), n.hit_pos, dir);

		obj_Zombie* z = (obj_Zombie*)target;

		// apply ragdoll force from object position, or up if too close
		r3dPoint3D zragHitPos = z->GetPosition();
		r3dPoint3D zlastTimeHitForce = r3dPoint3D(0, 1, 0);
		if(n.hit_body_bone < GetSkeleton()->NumBones) // zombies use same skeleton as player
			zragHitPos = uberEquip_->getBonePos(n.hit_body_bone, GetSkeleton(), z->GetTransformMatrix());
		if((zragHitPos - this->GetPosition()).LengthSq() > 0.001f)
			zlastTimeHitForce = (zragHitPos - this->GetPosition()).NormalizeTo();

		z->lastTimeHitPos  = zlastTimeHitForce;
		z->lastTimeDmgType = m_Weapons[m_SelectedWeapon]->getCategory();
		z->lastTimeHitBoneID = n.hit_body_bone;
		z->PlayHurtSound();
	}
	else // hit something else, for now that is UAV only
	{
		SpawnImpactParticle(r3dHash::MakeHash("Metal"),r3dHash::MakeHash(m_Weapons[m_SelectedWeapon]->getDecalSource()), n.hit_pos, dir);
	}
}

void obj_Player::OnNetPacket(const PKT_S2C_SpawnExplosion_s& n)
{
	GameObject* from = GameWorld().GetNetworkObject(n.FromID);
	if(from)
	{
		// add decal
		DecalParams params;
		params.Dir		= r3dPoint3D(0,1,0);
		params.Pos		= from->GetPosition();
		params.TypeID	= GetDecalID( r3dHash::MakeHash(""), r3dHash::MakeHash("grenade") );
		if( params.TypeID != INVALID_DECAL_ID )
			g_pDecalChief->Add( params );
		SpawnImpactParticle(r3dHash::MakeHash(""), r3dHash::MakeHash("grenade"), from->GetPosition(), r3dPoint3D(0,1,0));

		//	Start radial blur effect
		gExplosionVisualController.AddExplosion(from->GetPosition(), n.radius);
	}
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerUseItem_s& n)
{
	// assumption: current weapon is set to whatever player is about to use so just play it's animation.
	if(m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->isUsableItem())
		uberAnim_->StartShootAnim();
}

void obj_Player::OnNetPacket(const PKT_S2C_PlayerUsedItemAns_s& n)
{
	// ptumik: server isn't sending this packet atm
	/*if(n.itemId == WeaponConfig::ITEMID_Bandages || n.itemId == WeaponConfig::ITEMID_Bandages2 || n.itemId == WeaponConfig::ITEMID_MedKit)
	{
	m_BandagesEffectTime = r3dGetTime() + 0.5f;
	}*/
}

void obj_Player::OnNetPacket(const PKT_S2C_BackpackAddNew_s& n)
{
	// NOTE: only for local players
	r3d_assert(NetworkLocal);

	if(n.Item.itemID == 'GOLD')
	{
		int gd = n.Item.quantity;
		if (gUserProfile.ProfileData.isPremium)
			gd *= 2;

		gUserProfile.ProfileData.GameDollars += gd;
		wchar_t tmpStr[64];
		swprintf(tmpStr, 64, gLangMngr.getString("InfoMsg_DollarsPickedUp"), gd);
		SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/MainMenu GUI/UI_menu_open"), GetPosition()); // pickup sound
		hudMain->showMessage(tmpStr);
		return;
	}

	r3d_assert(CurLoadout.Items[n.SlotTo].itemID == 0);
	CurLoadout.Items[n.SlotTo] = n.Item;

	OnBackpackChanged(n.SlotTo);

	hudMain->showMessage(gLangMngr.getString("InfoMsg_NewItemAdded"));

	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/MainMenu GUI/UI_menu_open"), GetPosition()); // pickup sound
}
/*void obj_Player::OnNetPacket(const PKT_C2S_SetVelocity_s& n)
{
float heighOffset = 0.8f;
//if(((PxCapsuleController*)Controller)->getHeight()<0.5f)
//	heighOffset = 0.4f;

PxBoxGeometry bbox(0.05f, heighOffset, 0.05f);
PxTransform pose(PxVec3(n.pos.x, n.pos.y, n.pos.z), PxQuat(0,0,0,1));
PxShape* hit = NULL;
PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK,0,0,0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
if(g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, hit, filter)) 
{
r3dOutToLog("overlapAny\n");
SetPosition(prevPosLeng);
PKT_C2S_SetVelocity_s n1;
//n.vel = r3dPoint3D(0,0,0);
p2pSendToHost(this,&n1,sizeof(n1));
}
}*/
void obj_Player::OnNetPacket(const PKT_S2C_BackpackModify_s& n)
{
	// NOTE: only for local players
	r3d_assert(NetworkLocal);

	if(n.SlotTo == 0xFF)
	{
		hudMain->showMessage(gLangMngr.getString("InfoMsg_NoFreeBackpackSlots"));
		return;
	}

	if(n.SlotTo == 0xFE)
	{
		hudMain->showMessage(gLangMngr.getString("InfoMsg_TooMuchWeight"));
		return;
	}

	r3d_assert(n.SlotTo < wiCharDataFull::CHAR_MAX_BACKPACK_SIZE);
	wiInventoryItem& wi = CurLoadout.Items[n.SlotTo];
	r3d_assert(wi.itemID == n.dbg_ItemID);

	if(wi.quantity < n.Quantity)
		hudMain->showMessage(gLangMngr.getString("InfoMsg_ItemAdded"));
	else
		hudMain->showMessage(gLangMngr.getString("InfoMsg_ItemRemoved"));

	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/MainMenu GUI/UI_menu_open"), GetPosition());

	wi.quantity = n.Quantity;
	if(wi.quantity <= 0)
		wi.Reset();

	OnBackpackChanged(n.SlotTo);
}

void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerReputation_s& n)
{
	if (NetworkLocal && hudMain)
	{
		const int diff = n.Reputation - CurLoadout.Stats.Reputation;
		if (diff != 0)
		{
			wchar_t tmpStr[64];
			if (diff > 0)
				swprintf(tmpStr, 64, gLangMngr.getString("InfoMsg_ReputationAdded"), diff);
			else
				swprintf(tmpStr, 64, gLangMngr.getString("InfoMsg_ReputationRemoved"), diff);

			hudMain->showMessage(tmpStr);
		}
	}
	CurLoadout.Stats.Reputation = n.Reputation;
}

BOOL obj_Player::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	R3DPROFILE_FUNCTION("obj_Player::OnNetReceive");
	r3d_assert(!(ObjFlags & OBJFLAG_JustCreated)); // make sure that object was actually created before processing net commands

#undef DEFINE_GAMEOBJ_PACKET_HANDLER
#define DEFINE_GAMEOBJ_PACKET_HANDLER(xxx) \
case xxx: { \
	const xxx##_s&n = *(xxx##_s*)packetData; \
	r3d_assert(packetSize == sizeof(n)); \
	OnNetPacket(n); \
	return TRUE; \
	}

	switch(EventID)
	{
	default: return FALSE;
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_MoveTeleport);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeCancel);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveSetCell);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveRel);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeAccept);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeBacktoOp);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeAccept2);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeOptoBack);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeSuccess);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerJump);

		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_AddScore);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerVitals);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerLoadout);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerGroupID);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerAttachments);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_CarPass);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupSwapLeader);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupID);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupReInvite);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupNoAccept);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupKick);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerWorldFlags);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerSwitchWeapon);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerReadyGrenade);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerThrewGrenade);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerReload);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerFired);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitNothing);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitStatic);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeRequest);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitStaticPierced);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitDynamic);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SpawnExplosion);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerUseItem);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_PlayerUsedItemAns);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_BackpackAddNew);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_BackpackModify);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerReputation);
		//DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_SetVelocity);
	}

	return TRUE;
}

bool obj_Player::hasScopeMode() const 
{
	if(bDead)
		return false;
	if(forcedEmptyHands)
		return false;

	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	return (m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->hasScopeMode(isFirstPerson)); 
}

bool obj_Player::hasScopeMode_hidePlayerModel() const 
{
	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	if(m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getScopeConfig(isFirstPerson))
		return m_Weapons[m_SelectedWeapon]->getScopeConfig(isFirstPerson)->hide_player_model;

	return false;
}

void obj_Player::DrawReticle()
{
	if(!NetworkLocal)
		return;

	g_RenderScopeEffect = 0;

	if(bDead || gClientLogic().disconnectStatus_ || (m_Weapons[m_SelectedWeapon] == NULL && !(r_hud_filter_mode->GetInt()==HUDFilter_NightVision)))
	{
		CurrentRig.allowScope = false; // to make sure that scope will not be rendered when you are dead
		g_ScopeFeedbackTimer = 0.0f;
		return;
	}

	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	r3dRenderer->SetRenderingMode(R3D_BLEND_PUSH|R3D_BLEND_NZ|R3D_BLEND_ALPHA);

	float cx = r3dRenderer->ScreenW/2;
	float cy = r3dRenderer->ScreenH/2;
	float cy2 = r3dRenderer->ScreenH*0.32f;

	// draw laser pointer if any
	if(m_Weapons[m_SelectedWeapon] && !forcedEmptyHands)
	{
		if(PlayerState != PLAYER_MOVE_SPRINT && !m_Weapons[m_SelectedWeapon]->isReloading() && g_camera_mode->GetInt() == 2 && !(hudAttm && hudAttm->isActive()))
		{
			r3dPoint3D laserPos;
			if(m_Weapons[m_SelectedWeapon]->hasLaserPointer(laserPos))
			{
				r3dRenderer->SetRenderingMode( R3D_BLEND_PUSH | R3D_BLEND_ADD | R3D_BLEND_ZC );

				float size  = R3D_MIN(r3dRenderer->ScreenW*0.02f, r3dRenderer->ScreenH*0.02f);

				// gradually decrease size over distance
				const float maxDecDist = 20.0f;
				float dist = (gCam - laserPos).Length();
				if(dist >= maxDecDist) dist = maxDecDist;
				size = R3D_LERP(size, size*0.25f, dist / maxDecDist);

				r3dPoint3D laserPosScreen;
				r3dProjectToScreen(laserPos, &laserPosScreen);
				if(m_isAiming)
				{
					laserPosScreen.x = cx;
					laserPosScreen.y = cy;
				}
				r3dColor laserColor = r3dColor::red;
				laserColor.A = (int)R3D_LERP(196.0f, 0.0f, dist/400.0f);
				r3dDrawBox2D(float(laserPosScreen.x-(size/2)), float(laserPosScreen.y-(size/2)), size, size, laserColor, s_laserPointerTex);

				r3dRenderer->SetRenderingMode( R3D_BLEND_POP );
			}
		}
	}

	// check if blinded by flash light or laser
	{
		// check all other players, see if they are close enough, have attachment and pointing it at you
		ClientGameLogic& CGL = gClientLogic();
		for(int i=0; i<CGL.MAX_NUM_PLAYERS; ++i)
		{
			obj_Player* plr = CGL.GetPlayer(i);
			if(g_bEditMode)
			{
				plr = this;
				i = CGL.MAX_NUM_PLAYERS;
			}
			if(!plr)
				continue;
			if(plr == this && !g_bEditMode)
				continue;
			if( !(plr->m_SelectedWeapon >= 0 && plr->m_SelectedWeapon <=NUM_WEAPONS_ON_PLAYER) )
				continue;
			if(plr->m_Weapons[plr->m_SelectedWeapon] == NULL)
				continue;
			if(plr->forcedEmptyHands)
				continue;
			int attmID = plr->m_Weapons[plr->m_SelectedWeapon]->getWeaponAttachmentID(WPN_ATTM_LEFT_RAIL);
			bool hasFlashlight = attmID == 400018 || attmID == 400022 || plr->m_Weapons[plr->m_SelectedWeapon]->getItemID()==101306;
			bool hasLaser = attmID == 400004 || attmID == 400021;

			if( hasFlashlight || hasLaser ) // laser, flashlight
			{
				float dist = (plr->GetPosition() - gCam).Length();

				// other players do not have FPS model, so just use muzzle pos
				D3DXMATRIX weaponBone = plr->uberEquip_->getWeaponBone(plr->GetSkeleton(), plr->DrawFullMatrix);
				r3dPoint3D attmPos = plr->m_Weapons[plr->m_SelectedWeapon]->getMuzzlePos(weaponBone);
				{
					// check if attachment is visible
					r3dPoint3D attmScreenPos;
					if(!r3dProjectToScreen(attmPos, &attmScreenPos))
						continue;

					r3dPoint3D attmDir = r3dPoint3D(weaponBone._11, weaponBone._12, weaponBone._13);
					if(attmDir.Dot(gCam.vPointTo)>0.0f)
						continue;
					float distToCamera = (attmPos - gCam).Length();

					// check that attachment is visible and not blocked
					PxRaycastHit hit;
					PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER),0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
					r3dPoint3D dirToAttm = (attmPos - gCam).NormalizeTo();
					bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(gCam.x, gCam.y, gCam.z), PxVec3(dirToAttm.x, dirToAttm.y, dirToAttm.z), distToCamera, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
					if(!hitResult)
					{
						/*{
						float size = r3dRenderer->ScreenH*0.1f;
						r3dDrawBox2DRotate(attmScreenPos.x, attmScreenPos.y, size, size, 0.0f, r3dColor::white, s_lightHaloTex);
						}*/

						float viewDot = -attmDir.Dot(dirToAttm);
						if(hasFlashlight) // flashlight
						{
							if(viewDot < 0.75f)
								viewDot = 0;
							else
								viewDot = viewDot*4-3.0f;
						}
						else // laser
						{
							if(viewDot < 0.997f)
								viewDot = 0;
							else
								viewDot = ((viewDot*100.0f)-99.7f); // convert from [0.99, 1.0] to [0, 1]
						}

						{
							r3dTexture* tex = (hasLaser)?s_laserFlashTex:s_flashlightFlashTex;
							float size = R3D_MIN(r3dRenderer->ScreenW, r3dRenderer->ScreenH);
							float scale = viewDot*0.5f;
							if(distToCamera < 10.0f)
								scale = scale * (1.0f+2.0f*R3D_CLAMP((distToCamera/10.0f), 0.0f, 1.0f));
							else if(attmID == 400018 || attmID == 400022) // flashlight, max range 20m
								scale = scale * (3.0f-(3*(R3D_CLAMP((distToCamera-10.0f)/10.0f, 0.0f, 0.95f))));
							else // laser, max range 40m
								scale = scale * (3.0f-(3*(R3D_CLAMP((distToCamera-10.0f)/30.0f, 0.0f, 1.0f))));
							size = size * scale;
							r3dDrawBox2DRotate(attmScreenPos.x, attmScreenPos.y, size, size, r3dSin(r3dGetTime()*0.01f)*360.0f, r3dColor::white, tex);
							r3dDrawBox2DRotate(attmScreenPos.x, attmScreenPos.y, size, size, r3dCos(r3dGetTime()*0.012f)*360.0f, r3dColor::white, tex);
						}
					}
				}
			}
		}
	}
	bool scopeRendering = hasScopeMode() && CurrentRig.allowScope;

	const ScopeConfig* scopeConfig = NULL;
	if(m_Weapons[m_SelectedWeapon])
		scopeConfig = m_Weapons[m_SelectedWeapon]->getScopeConfig(isFirstPerson);

	if(r_hud_filter_mode->GetInt()==HUDFilter_NightVision && !scopeRendering)// nightvision
	{
		scopeConfig = g_pWeaponArmory->getScopeConfig("nvgframe");
		scopeRendering = true;
	}

	r3d_assert(scopeConfig);

	if(scopeRendering) 	// draw sniper scope
	{
		g_RenderScopeEffect = 1;
		gPFX_ScopeEffect.SetScopeReticle(scopeConfig->scope_reticle);
		gPFX_ScopeEffect.SetScopeNormal(scopeConfig->scope_normal);
		gPFX_ScopeEffect.SetScopeMask(scopeConfig->scope_mask);
		gPFX_ScopeEffect.SetScopeBlurMask(scopeConfig->scopeBlur_mask);

		ScopeMaterialParams params = gPFX_ScopeEffect.GetMaterialParams() ;
		params.Lighting = scopeConfig->lighting ;
		gPFX_ScopeEffect.SetMaterialParams( params ) ;

		float scaleScope = scopeConfig->scale;
		float scopeScaleMod = 0.20f;
		if(g_camera_mode->GetInt() == 2)
			scopeScaleMod = 0.10f;

		scaleScope += g_ScopeFeedbackTimer*scopeScaleMod;
		if(g_ScopeFeedbackTimer>0)
		{
			g_ScopeFeedbackTimer = R3D_MAX(g_ScopeFeedbackTimer-r3dGetFrameTime()*8.0f, 0.0f);
		}
		gPFX_ScopeEffect.SetScopeScale(scaleScope);
	}
#ifndef FINAL_BUILD
	else if(g_bEditMode && r_show_player_debug_data->GetBool() && scopeConfig->reticule) // draw reticule only in editor, in game reticule is in flash
	{
		float rw = (float)scopeConfig->reticule->GetWidth();
		float rh = (float)scopeConfig->reticule->GetHeight();
		float yy = g_camera_mode->GetInt() == 1?cy2:cy;
		r3dDrawBox2D(float(cx-(rw/2)), float(yy-(rh/2)), rw, rh, r3dColor::white, scopeConfig->reticule);
	}
#endif

	if(!scopeRendering)
		g_ScopeFeedbackTimer = 0.0f;

	if(m_HitMarkerFadeout>0)
	{
		float rw = (float)m_HitMarkerTex->GetWidth();
		float rh = (float)m_HitMarkerTex->GetHeight();
		r3dColor clr = r3dColor::white; clr.A = BYTE(m_HitMarkerFadeout*255.0f);
		if(m_isInScope)
			r3dDrawBox2D(float(cx-(rw/2)), float(cy-(rh/2)), rw, rh, r3dColor::white, m_HitMarkerTex);
		else
		{
			float yy = g_camera_mode->GetInt() == 1?cy2:cy;
			r3dDrawBox2D(float(cx-(rw/2)), float(yy-(rh/2)), rw, rh, r3dColor::white, m_HitMarkerTex);
		}
	}

	if(r3dGetTime() < m_BandagesEffectTime)
	{
		// !! hardcoded for 0.5 duration
		float time = m_BandagesEffectTime - r3dGetTime(); // values: 0.5...0.0
		float infl = 0;
		time -= 0.25f;  // 0.25...-0.25
		time *= 4.0f; // 1.0...-1.0
		time = R3D_ABS(time); // 1.0...0.0...1.0
		time = 1.0f-time; // 0.0...1.0...0.0
		time = pow(time, 2.0f); // curve
		infl = R3D_CLAMP(time, 0.0f, 1.0f);

		r3dColor24 effectColor(153, 217, 255, 196);
		effectColor = effectColor * infl;
		r3dDrawBox2D(0, 0, r3dRenderer->ScreenW, r3dRenderer->ScreenH, effectColor);
	}

	r3dRenderer->SetRenderingMode(R3D_BLEND_POP);
}
void obj_Player::DrawLabel()
{
	r3dRenderer->SetMaterial(NULL);

	struct PushRestoreStates
	{
		PushRestoreStates()
		{
			r3dRenderer->SetRenderingMode( R3D_BLEND_ALPHA | R3D_BLEND_NZ | R3D_BLEND_PUSH );
		}

		~PushRestoreStates()
		{
			r3dRenderer->SetRenderingMode( R3D_BLEND_POP );
		}
	} pushRestoreStates; (void)pushRestoreStates;

	if(NetworkLocal && gClientLogic().localPlayer_)
	{
		// show a warning if FPS or PING is low
		float fps = r3dGetAvgFPS();
		if(fps < 20.0f)
			Font_Label->PrintF(r3dRenderer->ScreenW-80, 0, r3dColor(243,43,37), "FPS: %.1f", fps);

#ifdef CHEATS_ENABLED
		Font_Label->PrintF(r3dRenderer->ScreenW-120, 0, r3dColor(243,43,37), "Ping: %d", gClientLogic().net_->lastPing_);
#endif
	}

	// CHEATS CODE(FOR ADMINS)
#ifdef CHEATS_ENABLED
	//MenuFont_Editor->PrintF(r3dRenderer->ScreenW-200, 15, r3dColor(243,43,37), "F8 : stun zombie around 500m");
	//MenuFont_Editor->PrintF(r3dRenderer->ScreenW-200, 30, r3dColor(243,43,37), "F7 : kill player around 500m");
	//MenuFont_Editor->PrintF(r3dRenderer->ScreenW-200, 45, r3dColor(243,43,37), "F6 : kill zombie around 500m");


	if (gClientLogic().localPlayer_->Cheats)
	{
		// CHEATS OPTIONS CODE
		if (this == gClientLogic().localPlayer_)
		{
			//if (Keyboard->WasPressed(kbsF5))
			//	ESPItems = !ESPItems;
#ifdef HK_ENABLED
			MenuFont_Editor->PrintF(r3dRenderer->ScreenW-210, 45, r3dColor(243,43,37), "F7 : ESPZombies %s",ESPZombies ? "On" : "Off");
			MenuFont_Editor->PrintF(r3dRenderer->ScreenW-210, 60, r3dColor(243,43,37), "F8 : AimHK %s",ESPHackKill ? "On" : "Off");
#endif


			// FUCKING BULLET DROP POINT ESP CODE

			Weapon* wpn = /*m_Weapons[m_SelectedWeapon]*/ NULL;

			// disabled
			if (wpn) // dist calc
			{
				r3dPoint3D dir;
				r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);

				r3dPoint3D muzzlePos = wpn->getMuzzlePos(uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix));
				PxRaycastHit hit;
				PhysicsCallbackObject* target = NULL;
				PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
				g_pPhysicsWorld->raycastSingle(PxVec3(muzzlePos.x, muzzlePos.y, muzzlePos.z), PxVec3(dir.x, dir.y, dir.z), 2000.0f, PxSceneQueryFlag::eDISTANCE, hit, filter);

				float distance = -1;
				if(hit.shape)
				{
					r3dPoint3D scrCoord;
					const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(wpn->getItemID());
					r3dPoint3D point = r3dPoint3D(hit.impact.x,hit.impact.y,hit.impact.z) + ((r3dVector(0, -9.81f, 0) * (float)wc->m_AmmoMass) * hit.distance);
					r3dDrawCircle3D(point, 2, gCam, 0.1f, r3dColor(0, 0, 255));
					//r3dProjectToScreen(point, &scrCoord);
					//MenuFont_Editor->PrintF(scrCoord.x, scrCoord.y,    r3dColor(255,255,51),"Bullet Drop Point");
				}
			}

			ObjectManager& GW = GameWorld();
			for (GameObject *targetObj = GW.GetFirstObject(); targetObj; targetObj = GW.GetNextObject(targetObj))
			{
				// ESP CODES
				float dist = (gCam - targetObj->GetPosition()).Length();
				r3dPoint3D scrCoord;
				float x, y;
				int isVisible = 1;

				//isVisible = r3dProjectToScreenAlways(targetObj->GetPosition()+r3dPoint3D(0,2.0f,0), &scrCoord, 20, 20);

				isVisible = r3dProjectToScreen(targetObj->GetPosition(), &scrCoord);


				// convert screens into UI space
				float mulX = 1920.0f/r3dRenderer->ScreenW;
				float mulY = 1080.0f/r3dRenderer->ScreenH;
				x = scrCoord.x/* * mulX*/;
				y = scrCoord.y/* * mulY*/;
				/*if(targetObj->isObjType(OBJTYPE_SharedUsableItem))
				{

				if (dist < 120)
				{
				MenuFont_Editor->PrintF(x, y,    r3dColor(255,222,0),((SharedUsableItem*)targetObj)->Name.c_str());
				MenuFont_Editor->PrintF(x, y+12,    r3dColor(255,222,0), "dist : %.2f", dist);
				}
				}
				else*/ 


				if(targetObj->isObjType(OBJTYPE_Human)) // check new player spawn in close range
				{
					obj_Player* plr = (obj_Player*)targetObj;
#ifdef HK_ENABLED
					if (!r3dProjectToScreen(targetObj->GetPosition(),NULL) && plr != this && plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected && (plr->GetPosition() - GetPosition()).Length() < 150)
					{
						MenuFont_Editor->PrintF(r3dRenderer->ScreenW / 2, 100,    r3dColor(243,43,37),"Player spawn in close range!");
					}
#endif

					/*if (plr->GetPosition() - GetPosition()).Length() < 50 && && plr != this)
					{
					if (CurLoadout.GroupID != 0 && plr->CurLoadout.GroupID != CurLoadout.GroupID)
					MenuFont_Editor->PrintF(r3dRenderer->ScreenW / 2, 120,    r3dColor(243,43,37),"Player in near you! dist %.2f",>GetPosition() - GetPosition()).Length());
					else if (ClanID != 0 && plr->CClanID != ClanID)
					MenuFont_Editor->PrintF(r3dRenderer->ScreenW / 2, 120,    r3dColor(243,43,37),"Player in near you! dist %.2f",>GetPosition() - GetPosition()).Length());
					else if (plr->ClanID == 0 || plr->CurLoadout.GroupID != 0)
					MenuFont_Editor->PrintF(r3dRenderer->ScreenW / 2, 120,    r3dColor(243,43,37),"Player in near you! dist %.2f",>GetPosition() - GetPosition()).Length());
					}*/
				}
				if (isVisible)
				{
#ifdef HK_ENABLED
					if (targetObj->isObjType(OBJTYPE_Zombie) && gClientLogic().localPlayer_->ESPZombies)
					{
						if (dist < 400)
						{
							MenuFont_Editor->PrintF(x, y,    r3dColor(243,43,37),"Zombie");
							MenuFont_Editor->PrintF(x, y+12,    r3dColor(243,43,37), "dist : %.2f", dist);
						}
					}
					else 
#endif

						if (targetObj->isObjType(OBJTYPE_Human))
						{
							if (dist < 750)
							{
								char plrUserName[64]; ((obj_Player*)targetObj)->GetUserName(plrUserName);
								obj_Player* plr = (obj_Player*)targetObj;
								Weapon* wpn = plr->m_Weapons[plr->m_SelectedWeapon];

								if (wpn)
								{
									const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(wpn->getItemID());
									if (wc)
									{
										MenuFont_Editor->PrintF(x, y-48,    r3dColor(255,255,51),"%s",wc->m_StoreName);
									}
								}

								// HACKKILL 

#ifdef HK_ENABLED
								if (plr->GetSafeID() == m_LocalPlayer_CurrentAimAt && ESPHackKill && this != plr && Mouse->IsPressed(r3dMouse::mLeftButton)
									&& !hudMain->isPlayersListVisible() && !hudPause->isActive() && !hudTrade->isActive()
									&& !hudRepair->isActive())
								{
									PKT_C2S_Temp_Damage_s n;
									n.targetId = toP2pNetId(plr->GetNetworkID());
									n.wpnIdx = (BYTE)0;
									n.damagePercentage = (BYTE)100; 
									n.explosion_pos = plr->GetPosition();
									p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
								}
#endif

								if (gClientLogic().localPlayer_)
								{
									if (plr->GetSafeID() == m_LocalPlayer_CurrentAimAt && this != plr)
									{
#ifdef HK_ENABLED
										MenuFont_Editor->PrintF(x, y-12,    r3dColor(255,255,51),"Aiming %s",ESPHackKill ? "Click To Kill" : "");
#endif

										if (plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox)
											MenuFont_Editor->PrintF(x, y-24,    r3dColor(255,255,51),"Safe Zone");
										else if (plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected)
											MenuFont_Editor->PrintF(x, y-24,    r3dColor(255,255,51),"Spawn Protected");

										if (plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected && plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox)
										{
											MenuFont_Editor->PrintF(x, y-24,    r3dColor(255,255,51),"Safe Zone");
											MenuFont_Editor->PrintF(x, y-36,    r3dColor(255,255,51),"Spawn Protected");
										}
									}
									else
									{
										if (plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox)
											MenuFont_Editor->PrintF(x, y-12,    r3dColor(255,255,51),"Safe Zone");
										else if (plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected)
											MenuFont_Editor->PrintF(x, y-12,    r3dColor(255,255,51),"Spawn Protected");

										if (plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected && plr->CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox)
										{
											MenuFont_Editor->PrintF(x, y-12,    r3dColor(255,255,51),"Safe Zone");
											MenuFont_Editor->PrintF(x, y-24,    r3dColor(255,255,51),"Spawn Protected");
										}
									}
								}


								MenuFont_Editor->PrintF(x, y,    r3dColor(0,255,0),"Player %s",plrUserName);
								MenuFont_Editor->PrintF(x, y+12,    r3dColor(0,255,0), "dist : %.2f", dist);
								MenuFont_Editor->PrintF(x, y+24,    r3dColor(0,255,0), "Health : %.2f", ((obj_Player*)targetObj)->CurLoadout.Health);
							}
						}
				}
			}
		}
	}
#endif
#ifdef DEV_ENABLED
	// draw player CustomerID/gamertag for admins
	if(gUserProfile.ProfileData.isDevAccount)
	{
		float dist = (gCam - GetPosition()).Length();
		float distx = (gCam - GetPosition()).x;
		float disty = (gCam - GetPosition()).y;
		float distz = (gCam - GetPosition()).z;
		//int a    = int(R3D_MAX(1.0f - (dist / 300.0f), 0.1f)*255.0f);
		r3dPoint3D scrCoord;
		r3dProjectToScreen(GetPosition() + r3dPoint3D(0, 1.8f, 0), &scrCoord);

		char plrUserName[64]; GetUserName(plrUserName);
		//obj_Player* tplr = FindPlayer(plrUserName);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y+36,r3dColor(255,255,255,100), "Health=%.1f", CurLoadout.Health);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y+24, r3dColor(255,255,255,100), "pos=%.2f, %.2f, %.2f", GetPosition().x, GetPosition().y, GetPosition().z);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y+12, r3dColor(255,255,255,100), "ID:%d", CustomerID);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y,    r3dColor(255,255,255,100), "%s", plrUserName);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y-12, r3dColor(255,255,255,100), "dist=%.2f", dist);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y-24, r3dColor(255,255,255,100), "pos3D=%.2f, %.2f, %.2f", distx,disty,distz);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y-36, r3dColor(255,255,255,100), "GroupID=%d",CurLoadout.GroupID);

	}
#endif

	if (NetworkLocal)
	{
		// Font_Label->PrintF( 10, r3dRenderer->ScreenH-200,    r3dColor(255,255,255), "Time %.2f",r3dGameLevel::Environment.__CurTime );
		//Font_Label->PrintF( 10, r3dRenderer->ScreenH-200,    r3dColor(255,255,255), "Speed %.2f",RealAcceleration.z);
		//Weapon* wpn = m_Weapons[m_SelectedWeapon];
		//if (wpn)
		//Font_Label->PrintF( 10, r3dRenderer->ScreenH-220,    r3dColor(255,255,255), "m_WeaponAttachmentStats %.2f getRegetSpreadcoil %.2f",wpn->m_WeaponAttachmentStats[wpn->WPN_ATTM_STAT_SPREAD],(float)wpn->getSpread());
		//const r3dGPUStats& s = r3dRenderer->Stats;
		//extern int	n_draw;
		//Font_Label->PrintF( 10, r3dRenderer->ScreenH-200,    r3dColor(255,255,255), "NumDraws %d, n_draw %d",s.GetNumDraws(),n_draw);

		/*PKT_C2S_WpnLog_s n;
		n.itemid = 0;
		if (NetworkLocal && wpn && r3dGetTime() > lastSendWpnLog)
		{
		const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(wpn->getItemID());
		lastSendWpnLog = r3dGetTime() + 2.0f;

		if (wc)
		{
		Font_Label->PrintF( 10, r3dRenderer->ScreenH-200,    r3dColor(255,255,255), "m_Spread %.2f getSpread %.2f",(float)wc->m_spread,wpn->getSpread());
		//Font_Label->PrintF( 10, r3dRenderer->ScreenH-220,    r3dColor(255,255,255), "m_WeaponAttachmentStats %.2f getRecoil %.2f",wpn->m_WeaponAttachmentStats[wpn->WPN_ATTM_STAT_RECOIL],wpn->getRecoil());
		n.itemid = wpn->getItemID();
		n.m_AmmoDamage = wc->m_AmmoDamage;
		n.m_AmmoMass = wpn->getAmmoMass();
		n.m_AmmoSpeed = wpn->getAmmoSpeed();
		n.m_recoil = wpn->getRecoil();
		n.m_spread = wpn->getSpread();
		n.slot = m_SelectedWeapon;
		n.m_rateoffire = wpn->getRateOfFire();
		n.m_recoilattm = wpn->m_WeaponAttachmentStats[wpn->WPN_ATTM_STAT_RECOIL];
		n.m_spreadattm = wpn->m_WeaponAttachmentStats[wpn->WPN_ATTM_STAT_SPREAD];
		n.m_rateattm = wpn->m_WeaponAttachmentStats[wpn->WPN_ATTM_STAT_FIRERATE];
		n.m_recoil2 = recoil2;
		n.m_spread2 = spread;
		//r3dOutToLog("recoil %.2f\n",recoil2);
		}
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
		}*/
	}
	//Font_Label->PrintF( 10, r3dRenderer->ScreenH-200,    r3dColor(255,255,255), "RealAcceleration %f.1f %f.1f %f.1f",RealAcceleration.x,RealAcceleration.y,RealAcceleration.z);
	//if (NetworkLocal)
	//Font_Label->PrintF( 10, r3dRenderer->ScreenH-220,    r3dColor(255,255,255), "VelocityY %.2f",prevPos.y-GetPosition().y);	

	/*{
	if (curcar != NULL)
	{
	//r3dProjectToScreenAlways(g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->GetBBoxWorld().Center() + r3dPoint3D(0, 1.8f, 0), &scrCoord,20,20);
	//float xSpeed = g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->vd->vehicle->computeForwardSpeed()*2-g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->oldSpeed;
	if (curcar)
	{
	if (isDriving())
	{
	Font_Label->PrintF( 10, r3dRenderer->ScreenH-220,    r3dColor(255,255,255), "Speed %.1f km/h",curcar->vd->vehicle->computeForwardSpeed()*2);	
	Font_Label->PrintF( 10, r3dRenderer->ScreenH-200,    r3dColor(255,255,255), "Durability %f.1f %",curcar->dura);	
	}
	else if (!d_drive_vehicles->GetBool() && isInVehicle())
	{
	Font_Label->PrintF( 10, r3dRenderer->ScreenH-220,    r3dColor(255,255,255), "Speed %.1f km/h",curcar->RemoteSpeed);	
	Font_Label->PrintF( 10, r3dRenderer->ScreenH-200,    r3dColor(255,255,255), "RPM %.1f",curcar->rpm);	
	}

	}
	//Font_Label->PrintF( 10, r3dRenderer->ScreenH-240,    r3dColor(255,255,255), "xSpeed %.2f km/n",xSpeed);	
	}
	}*/
	//#endif

	/*
	{
	// muzzler position debug
	D3DXMATRIX mm = uberEquip_->getWeaponBone(GetSkeleton(),GetPosition());
	r3dPoint3D pos = m_Weapons[m_SelectedWeapon]->getMuzzlePos(mm);
	r3dDrawLine3D(pos, pos + r3dPoint3D(mm._11, mm._12, mm._13), gCam, 0.005f, r3dColor(255, 255, 255));
	r3dDrawLine3D(pos, pos + r3dPoint3D(0, 1, 0), gCam, 0.005f, r3dColor(255, 255, 255));

	// r3dDrawLine3D(gTempTargetPos, gTempTargetPos + r3dPoint3D(mm._11, mm._12, mm._13), gCam, 0.005f, r3dColor(255, 0, 0));
	// r3dDrawLine3D(gTempTargetPos, gTempTargetPos + r3dPoint3D(0, 1, 0), gCam, 0.005f, r3dColor(255, 0, 0));

	// r3dDrawLine3D(gTempTargetPos, gTempMuzzlePos, gCam, 0.005f, r3dColor(0,255,0));
	r3dRenderer->Flush();
	}
	*/

#ifndef FINAL_BUILD
	if(NetworkLocal && r_show_player_debug_data->GetBool() && m_Weapons[m_SelectedWeapon])
	{
		Font_Label->PrintF(10, r3dRenderer->ScreenH-240,r3dColor(255,255,255), "m_Health %.2f", CurLoadout.Health);
		Font_Label->PrintF(10, r3dRenderer->ScreenH-220,r3dColor(255,255,255), "DmgRcvd %.2f", lastDamageRcvd);
		Font_Label->PrintF(10, r3dRenderer->ScreenH-200,r3dColor(255,255,255), "AccumDmgRcvd %.2f", accumLastDamageRcvd);
		Font_Label->PrintF(10, r3dRenderer->ScreenH-180,r3dColor(255,255,255), "Recoil %.2f, %.2f", RecoilViewModTarget.x, RecoilViewModTarget.y);

		if(Keyboard->WasPressed(kbsO))
			accumLastDamageRcvd = 0;

		/*Font_Label->PrintF(10, r3dRenderer->ScreenH-280,r3dColor(255,255,255), "%s %d %d (%dx%d)", 
		m_Weapons[m_SelectedWeapon]->getConfig()->m_StoreName,
		m_Weapons[m_SelectedWeapon]->m_numBulletsLeftInClip,
		m_Weapons[m_SelectedWeapon]->m_numBulletsLeft,
		m_Weapons[m_SelectedWeapon]->m_pConfig->m_clipSize,
		m_Weapons[m_SelectedWeapon]->m_ModifiedNumClips);*/
	}
#endif

#ifndef FINAL_BUILD
	// editing rig animation speed
	if(0)
	{
		int astate = uberAnim_->AnimPlayerState;
		Font_Label->PrintF(10, r3dRenderer->ScreenH-300, r3dColor(255,255,255), "AnimState:%d, Speed:%f", astate, uberAnim_->AnimSpeedStates[astate]);

		//if(Keyboard->IsPressed(kbsT)) uberAnim_->AnimSpeedStates[astate] -= r3dGetFrameTime() * 0.2f;
		if(Keyboard->IsPressed(kbsY)) uberAnim_->AnimSpeedStates[astate] += r3dGetFrameTime() * 0.2f;
	}
#endif

#ifndef FINAL_BUILD
	char plrUserName[64]; GetUserName(plrUserName);
	if(strncmp(plrUserName, "@@TestDummy", 10) == 0)
	{
		r3dPoint3D scrCoord;
		if(r3dProjectToScreen(GetPosition() + r3dPoint3D(0, 1, 0), &scrCoord))
		{
			float dist = gClientLogic().localPlayer_ ? (gClientLogic().localPlayer_->GetPosition() - GetPosition()).Length() : 0;
			Font_Label->PrintF(scrCoord.x+20, scrCoord.y,   r3dColor(255,255,255), "LastDmg %.2f", lastDamageRcvd);
			Font_Label->PrintF(scrCoord.x+20, scrCoord.y+25,r3dColor(255,255,255), "Distance %.2f", dist);
		}
	}
#endif

#ifndef FINAL_BUILD
	if(r_show_player_debug_data->GetBool())
	{
		float x = 10;
		float y = 100;
		for(size_t i=0; i<uberAnim_->anim.AnimTracks.size(); i++) 
		{
			const r3dAnimation::r3dAnimInfo& ai = uberAnim_->anim.AnimTracks[i];

			char st[256] = "";
			if(ai.dwStatus & ANIMSTATUS_Playing) strcat(st, "Play ");
			if(ai.dwStatus & ANIMSTATUS_Paused) strcat(st, "Pause ");
			if(ai.dwStatus & ANIMSTATUS_Finished) strcat(st, "Finish ");
			if(ai.dwStatus & ANIMSTATUS_Fading) strcat(st, "Fade ");
			if(ai.dwStatus & ANIMSTATUS_Expiring) strcat(st, "Expire ");

			_r3dSystemFont->PrintF(x, y, r3dColor(255, 255, 255), "%s: %02.1f, %X %s", 
				ai.pAnim->pAnimName, ai.fInfluence, ai.dwStatus, st);

			y += 20;
		}
	}
#endif
#ifndef FINAL_BUILD
	if(r_show_player_health->GetBool())
	{
		r3dPoint3D scrCoord;
		if(r3dProjectToScreen(GetPosition() + r3dPoint3D(0, 1, 0), &scrCoord))
		{
			Font_Label->PrintF(scrCoord.x, scrCoord.y,   r3dColor(255,255,255), "%.1f", CurLoadout.Health);
		}
	}
	if(r_show_zombie_stats->GetBool())
	{
		if(NetworkLocal)
			lifeProperties.DebugVisualizeRanges();
	}
#endif

}

//------------------------------------------------------------------------
struct PlayerRenderable : Renderable
{
	void Init( obj_Player::AuraType at, int Transparent )
	{
		DrawFunc = Draw;
		PlayerAuraType = at ;

		this->Transparent = Transparent ;
		matFlagsModifier = 0;
		FirstPerson = 0 ;
	}

	void ApplyCamouflageSettings()
	{
		if (!Parent)
			return;

		gCamouflageDataManager.SetCurrentData(*Parent);
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlayerRenderable* This = static_cast<PlayerRenderable*>( RThis );

		bool draw_weapon = true;

		/*D3DXVECTOR4 Stride( 0.0f, 1.0f, 0.0f, 1.0f );
		r3dRenderer->pd3ddev->SetRenderState( D3DRS_ZENABLE, FALSE );
		r3dRenderer->pd3ddev->SetPixelShaderConstantF( 1, Stride, 1 ); // outdoorLightTopColor
		r3dRenderer->pd3ddev->SetPixelShaderConstantF( 3, Stride, 1 ); // outdoorLightBottomColor
		r3dRenderer->pd3ddev->SetRenderState( D3DRS_ZENABLE, TRUE );*/
		//r3dRenderer->pd3ddev->SetPixelShaderConstantF( 6, Stride, 1 ); // outdoorLightKeyColor  
		struct RenderEvents
		{
			RenderEvents()
			{
				D3DPERF_BeginEvent( 0, L"PlayerRenderable::Draw" ) ;
			}

			~RenderEvents()
			{
				D3DPERF_EndEvent() ;
			}

		} renderEvents; renderEvents ;

		CUberEquip::DrawType dt;


		extern int PS_TRANSPARENT_ID		;
		extern int PS_TRANSPARENT_AURA_ID	;

		struct RestoreCamOnExit
		{
			RestoreCamOnExit()
			{
				needRestore = false ;
			}

			~RestoreCamOnExit()
			{
				if( needRestore )
				{
					r3dRenderer->SetCamera( gCam, true ) ;
					if( r_particles_quality->GetInt() > 3 )
					{
						D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_ZENABLE, TRUE ) ) ;
					}
				}
			}

			bool needRestore ;
		} restoreCamOnExit ;

		if( ( This->Transparent || This->PlayerAuraType != obj_Player::AT_NONE )
			&&
			This->FirstPerson			
			)
		{
			r3dCamera firstPersonCam = gCam ;
			firstPersonCam.NearClip = r_first_person_render_z_start->GetFloat();
			firstPersonCam.FarClip = r_first_person_render_z_end->GetFloat();
			firstPersonCam.FOV = r_first_person_fov->GetFloat();

			restoreCamOnExit.needRestore = true ;

			r3dRenderer->SetCamera( firstPersonCam, true ) ;
		}

		if( This->PlayerAuraType != obj_Player::AT_NONE )
		{
			draw_weapon = false ;
			dt = CUberEquip::DT_AURA ;
			r3dMaterial::SetupTransparentStates( 1 );

			void SetParticleDepth();
			SetParticleDepth();

			float r(1.0f), g(1.0), b(1.0f);

			if( This->PlayerAuraType == obj_Player::AT_HEALING )
			{
				r = 0.f ;
				g = 1.f ;
				b = 0.25f ;
			}
			else if(This->PlayerAuraType == obj_Player::AT_MORHINE)
			{
				r = 0.f ;
				g = 0.f ;
				b = 1.0f ;
			}
			else if(This->PlayerAuraType == obj_Player::AT_BERSERK)
			{
				r = 1.f ;
				g = 0.f ;
				b = 0.f ;
			}
			else if(This->PlayerAuraType == obj_Player::AT_SPAWNPROTECTION) // golden color
			{
				r = 1.0f ;
				g = 0.62f ;
				b = 0.15f ;
			}

			r3dRenderer->SetTex( gPlayer_AuraTexture ) ;

			{
				float vConst[ 4 ] = { r, g, b, r3dGetTime()  } ;
				D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( MC_AURA, vConst, 1 ) );
			}

			void SetupFilmToneConstants( int reg ) ;
			SetupFilmToneConstants( 24 ) ;

			r3dRenderer->SetPixelShader( PS_TRANSPARENT_AURA_ID ) ;
			r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_ZC);
			r3dRenderer->pd3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
			r3dRenderer->pd3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		}
		else
		{
			const Weapon* wpn = This->Parent->m_Weapons[This->Parent->m_SelectedWeapon];
			if(wpn && wpn->getCategory() == storecat_GRENADE)
			{
				if(This->Parent->uberAnim_->IsGrenadeLaunched())
					draw_weapon = false; // throwing grenade, so do not render grenade in hand
			}

			if( This->Transparent )
			{
				const CamoTimeLine &ctl = This->Parent->camoTimeLine;
				float s = ctl.transparencyOnShock;

				float vConst[ 4 ][ 4 ] =
				{
					// float4 Aura				: register(c14); // (c65);
					{ 1.0f,	s * (1 - s) * 4, ctl.transparentCamoAlpha, 0},
					// float4 TCamoExtra		: register(c15);
					{ 0, 0, 0, 0.f },
					// float4 TCamoColor0			: register(c16);
					{ 0, 0, 0, 0.f },
					// float4 TCamoColor1			: register(c17);
					{ 0, 0, 0, 0 }
				};
				D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( MC_AURA, (float*)vConst, 4 ) );

				vConst[0][3] = 0.01f;
				D3D_V( r3dRenderer->pd3ddev->SetVertexShaderConstantF( 23, (float*)vConst, 1 ) ) ;

				for(int i=0; i<=SLOT_Helmet; i++)
				{
					if(This->Parent->uberEquip_->slots_[i].gear)
					{
						bool isFirstPerson = g_camera_mode->GetInt()==2 && This->Parent->NetworkLocal;
						if( r3dMesh* mesh = This->Parent->uberEquip_->slots_[i].gear->getModel(isFirstPerson) )
						{
							mesh->ExtrudeAmmount = 0.01f ;
						}
					}
				}

				r3dRenderer->SetTex( gPlayer_CamoTexture, 9 ) ;

				r3dRenderer->pd3ddev->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
				r3dRenderer->pd3ddev->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

			}

			if(This->Parent->bDead)
				draw_weapon = false;

			This->ApplyCamouflageSettings();
			dt = CUberEquip::DT_DEFERRED;
		}

		float vCData[ 4 ] = { 1, 1, 1, 0 } ;
#if 0
		if(gClientLogic().localPlayer_ && This->Parent->TeamID != gClientLogic().localPlayer_->TeamID)
		{
			vCData[ 0 ] = 1 ; vCData[ 1 ] = 0 ; vCData[ 2 ] = 0 ; 
		}
		else
		{
			vCData[ 0 ] = 0 ; vCData[ 1 ] = 0 ; vCData[ 2 ] = 1 ; 
		}
#endif
		D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( MC_MASKCOLOR, vCData, 1 ) );

		if (This->matFlagsModifier)
		{
			for (int i = 0; i < SLOT_Max; ++i)
			{
				r3dMesh *m = This->Parent->uberEquip_->getSlotMesh((ESlot)i);
				if(!m) continue;
				if(!m->IsDrawable()) continue;

				for (int j = 0, e = m->GetNumMaterials(); j < e; ++j)
				{
					r3dTriBatch &tb = m->MatChunks[j];
					r3dMaterial *mat = tb.Mat;
					if (!mat) continue;

					This->matFlagsModifier(mat);
				}
			}
		}

		if( This->Transparent )
		{
			void SetVolumeFogParams() ;
			SetVolumeFogParams() ;

			// if it's local, draw depth prepass to disable backfaces for camo so that everything is not uggly.
			// TODO : if we spectate we should do this on spectate target!
			if( This->Parent->NetworkLocal && r3dRenderer->SupportsR32FBlending && !This->FirstPerson )
			{
				void ActivateTransparentBuffer();
				void ActivateParticleDepth();
				void DeactivateTransparentBuffer();
				void DeactivateParticleDepth();
				void SetParticleDepth();
				void RemoveParticleDepth();

				RemoveParticleDepth();
				r3dTransparentDistortFlush() ;
				DeactivateTransparentBuffer();
				ActivateParticleDepth();

				r3dRenderer->SetPixelShader( PS_DEPTH_ID ) ;

				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_ALPHABLENDENABLE,  TRUE ) );

				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ONE ) );
				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE ) );

				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_MIN ) );

				This->Parent->uberEquip_->Draw( This->Parent->GetSkeleton(), This->Parent->DrawFullMatrix_Localized, draw_weapon, CUberEquip::DT_DEPTH, This->FirstPerson ? true : false );

				DeactivateParticleDepth();
				ActivateTransparentBuffer();
				r3dTransparentSetDistort( 1 ) ;
				SetParticleDepth();

				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD ) );
			}

			{
				D3DXMATRIX sm0, sm1 ;

				D3DXMatrixTranspose( &sm0, &This->Parent->DrawFullMatrix_Localized ) ;

				sm1 = This->Parent->DrawFullMatrix_Localized * r3dRenderer->ViewMatrix_Localized ;

				D3DXMatrixTranspose( &sm1, &sm1 ) ;

				float consts[ 6 ][ 4 ] =
				{
					// float4x3 mWorld : register( c32 ) ;
					{ sm0._11, sm0._12, sm0._13, sm0._14 },
					{ sm0._21, sm0._22, sm0._23, sm0._24 },
					{ sm0._31, sm0._32, sm0._33, sm0._34 },
					// float4x3 mWorldView : register( c35 ) ;
					{ sm1._11, sm1._12, sm1._13, sm1._14 },
					{ sm1._21, sm1._22, sm1._23, sm1._24 },
					{ sm1._31, sm1._32, sm1._33, sm1._34 },
				} ;

				D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 32, (float*)consts, 6 ) ) ;
			}

			r3dRenderer->SetRenderingMode( R3D_BLEND_ALPHA | R3D_BLEND_ZC );
			D3D_V( r3dRenderer->pd3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE ) );
			D3D_V( r3dRenderer->pd3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA ) );

			r3dTransparentSetDistort( 1 ) ;

			if( This->FirstPerson && r_particles_quality->GetInt() > 3 )
			{
				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_ZENABLE, FALSE ) ) ;
			}

		}

		if(This->Parent->forcedEmptyHands)
			draw_weapon = false;

		This->Parent->uberEquip_->Draw( This->Parent->GetSkeleton(), This->Parent->DrawFullMatrix_Localized, draw_weapon, dt, This->FirstPerson ? true : false );

		if( This->PlayerAuraType != obj_Player::AT_NONE )
		{
			r3dRenderer->SetPixelShader( PS_TRANSPARENT_ID ) ;
		}
	}

	obj_Player* Parent ;
	typedef void (*MaterialModifyFlags)(r3dMaterial *mat);
	MaterialModifyFlags matFlagsModifier;

	char PlayerAuraType ;
	char Transparent ;
	char FirstPerson ;
};

//------------------------------------------------------------------------
struct PlayerShadowRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlayerShadowRenderable* This = static_cast<PlayerShadowRenderable*>(RThis);

		bool draw_weapon = true;
		const Weapon* wpn = This->Parent->m_Weapons[This->Parent->m_SelectedWeapon];
		if(wpn && wpn->getCategory() == storecat_GRENADE)
		{
			if(This->Parent->uberAnim_->IsGrenadeLaunched())
				draw_weapon = false; // throwing grenade, so do not render grenade in hand
		}
		if(This->Parent->forcedEmptyHands)
			draw_weapon = false;
		This->Parent->uberEquip_->Draw(This->Parent->GetSkeleton(), This->Parent->DrawFullMatrix_Localized, draw_weapon, CUberEquip::DT_SHADOWS, false );
	}

	obj_Player* Parent;
};

struct PlayerFlashRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlayerFlashRenderable* This = static_cast<PlayerFlashRenderable*>( RThis );

		This->Parent->DrawLabel();
		This->Parent->DrawReticle();
	}

	obj_Player* Parent;
};

struct PlayerDebugRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlayerDebugRenderable* This = static_cast<PlayerDebugRenderable*>( RThis );
		r3dRenderer->SetTex(NULL);
		r3dRenderer->SetMaterial(NULL);
		r3dRenderer->SetRenderingMode(R3D_BLEND_NOALPHA | R3D_BLEND_NZ);

		This->Parent->GetSkeleton()->DrawSkeleton(Cam, This->Parent->GetPosition());

		/*if(This->Parent->explosionTime > 0)
		{
		This->Parent->explosionTime-=r3dGetFrameTime();
		r3dDrawSphereSolid(This->Parent->explosionCenter, This->Parent->explosionRadius, Cam, r3dColor::red);
		}*/
		//r3dDrawBoundBox(This->Parent->GetBBoxWorld(), Cam, r3dColor24::green/*debugColorLevels[m_Level]*/, 0.05f);
	}

	obj_Player* Parent;
};

#define	RENDERABLE_AI_PLAYER_SORT_VALUE (2*RENDERABLE_USER_SORT_VALUE)

extern RenderArray	g_render_arrays[ rsCount ];

void
obj_Player::AppendShadowRenderables( RenderArray & rarr, const r3dCamera& Cam )
{
	r3d_assert( &rarr - g_render_arrays - rsCreateSM < NumShadowSlices ) ;

	// HACK: For the moment. 
#if VEHICLES_ENABLED
	if ( isInVehicle() ) 
	{
		return; 
	}
#endif
	if (!isntvi)
		if (!CurLoadout.isVisible) return;

	int firstPerson = g_camera_mode->GetInt() == 2 && NetworkLocal && !bDead;

	if(firstPerson) // do not render our own shadow in FPS mode (same as in BF3)
		return;

	if( !gDisableDynamicObjectShadows && &rarr - g_render_arrays - rsCreateSM != r_active_shadow_slices->GetInt() - 1 )
	{
		PlayerShadowRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_AI_PLAYER_SORT_VALUE;

		rarr.PushBack( rend );
	}
}

//////////////////////////////////////////////////////////////////////////

void SetTransparentCamoMatFlags(r3dMaterial *mat )
{
	mat->Flags |= R3D_MAT_TRANSPARENT | R3D_MAT_TRANSPARENT_CAMOUFLAGE ;
	if (strstr(mat->Name, "nocamo"))
		mat->Flags |= R3D_MAT_SKIP_DRAW;
}

void SetTransparentCamoMatFlags_FP(r3dMaterial *mat )
{
	mat->Flags |= R3D_MAT_TRANSPARENT | R3D_MAT_TRANSPARENT_CAMO_FP ;
	if (strstr(mat->Name, "nocamo"))
		mat->Flags |= R3D_MAT_SKIP_DRAW;
}

//////////////////////////////////////////////////////////////////////////

void UnsetTransparentCamoMatFlags(r3dMaterial *mat)
{
	mat->Flags &= ~(R3D_MAT_TRANSPARENT | R3D_MAT_TRANSPARENT_CAMOUFLAGE | R3D_MAT_TRANSPARENT_CAMO_FP |  R3D_MAT_SKIP_DRAW);
}

struct PlaceableConsumableItemRenderable : MeshDeferredRenderable
{
	void Init( int a_transparent )
	{
		DrawFunc = Draw;
		Transparent = a_transparent ;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlaceableConsumableItemRenderable* This = static_cast< PlaceableConsumableItemRenderable* >( RThis );
		r3dApplyPreparedMeshVSConsts( This->Parent->m_ConsumableItemPlacementMeshVSConsts );

		int wasTransparent = 0 ;

		float oldTranspMultiplier = 1.0f ;

		bool hasMatForTrasnparency = This && This->Transparent && This->Mesh && This->Mesh->MatChunks[This->BatchIdx].Mat ;

		if ( hasMatForTrasnparency )
		{
			// because previous material may be the same
			// and thus it will filter our transparency hacks that follow
			r3dMaterial::ResetMaterialFilter() ;

			r3dMaterial* mat = This->Mesh->MatChunks[This->BatchIdx].Mat ;

			wasTransparent = mat->Flags & R3D_MAT_TRANSPARENT ;
			oldTranspMultiplier = mat->TransparencyMultiplier ;

			if( !wasTransparent )
				mat->TransparencyMultiplier = 0.75f ;

			mat->Flags |= R3D_MAT_TRANSPARENT ;

			SetLightsIfTransparent( mat, This->Parent->GetBBoxWorld() ) ;
		}

		if(This->Mesh->IsSkeletal())
			r3dSkeleton::SetDefaultShaderConstants();

		MeshDeferredRenderable::Draw( RThis, Cam );

		if ( hasMatForTrasnparency )
		{
			r3dMaterial* mat = This->Mesh->MatChunks[This->BatchIdx].Mat ;

			int& flags = mat->Flags ;
			flags &= ~R3D_MAT_TRANSPARENT ;
			flags |= wasTransparent ;

			mat->TransparencyMultiplier = oldTranspMultiplier ;
		}
	}

	obj_Player* Parent ;
	int Transparent ;
};

//------------------------------------------------------------------------
// special callback to ignore terrain for beacon placement, as otherwise it is almost impossible to place beacon on uneven terrain
class beaconFilterCallback : public PxSceneQueryFilterCallback
{
public:
	beaconFilterCallback(){};

	virtual PxSceneQueryHitType::Enum preFilter(const PxFilterData& filterData, PxShape* shape, PxSceneQueryFilterFlags& filterFlags)
	{
		if(shape)
		{
			PxRigidActor& actor = shape->getActor();
			PhysicsCallbackObject* target = static_cast<PhysicsCallbackObject*>(actor.userData);
			if(target)
			{
				GameObject *gameObj = target->isGameObject();
				if(gameObj)
				{
					if(gameObj->isObjType(OBJTYPE_Terrain))
						return PxSceneQueryHitType::eNONE;
				}
			}
		}
		return PxSceneQueryHitType::eBLOCK;
	}

	virtual PxSceneQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxSceneQueryHit& hit)
	{
		// shouldn't be called!
		return PxSceneQueryHitType::eBLOCK;
	}
};


void
obj_Player::AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam )
{
	if(!m_enableRendering)
		return;

	// HACK: For the moment. 
#if VEHICLES_ENABLED
	if ( isInVehicle() ) 
	{
		return; 
	}
#endif

	if (!isntvi)
		if (!CurLoadout.isVisible) return;

	int firstPerson = g_camera_mode->GetInt() == 2 && NetworkLocal && !bDead;

	if(firstPerson)
	{
		if(hasScopeMode() && CurrentRig.allowScope)
		{
			r_first_person_render_z_start->SetFloat(0.01f);
			r_first_person_render_z_end->SetFloat(0.7f);
		}
		else
		{
			r_first_person_render_z_start->SetFloat(0.1f);
			r_first_person_render_z_end->SetFloat(1.5f);
		}
	}

	// deferred
	if( !NetworkLocal || (NetworkLocal && (!g_CameraInsidePlayer || g_camera_mode->GetInt()==2)) ) // do not render player if camera is inside of player
	{
		if(!(NetworkLocal && hasScopeMode() && hasScopeMode_hidePlayerModel() && CurrentRig.allowScope)) // do not render yourself in scope mode
		{
			bool transparentCamo = camoTimeLine.camoMasterTransparency < 1.0f;
			if (camoTimeLine.camoMasterTransparency > 0)
			{
				PlayerRenderable rend;

				rend.Init( AT_NONE, 0 );
				rend.Parent		= this;
				rend.SortValue	= RENDERABLE_AI_PLAYER_SORT_VALUE;
				rend.matFlagsModifier = &UnsetTransparentCamoMatFlags;
				rend.FirstPerson = firstPerson ;

				render_arrays[ firstPerson ? rsFillGBufferFirstPerson : rsFillGBuffer ].PushBack( rend );
			}

			if ( transparentCamo )
			{
				PlayerRenderable rend ;

				rend.Init( AT_NONE, 1 ) ;
				rend.Parent		= this ;
				rend.FirstPerson = firstPerson ;

				rend.matFlagsModifier = firstPerson ? 
					&SetTransparentCamoMatFlags_FP
					:
				&SetTransparentCamoMatFlags ;

				float len = ( Cam - GetPosition() ).Length() ;
				rend.SortValue	= RENDERABLE_EMITTER_USER_SORT_VALUE | GetRevIDist( len );

				render_arrays[ rsDrawTransparents ].PushBack( rend ) ;			
			}

			if ( m_AuraType != AT_NONE )
			{
				PlayerRenderable rend ;

				rend.Init( m_AuraType, 0 ) ;
				//rend.Init( AT_HEALING , 0 ) ;
				rend.Parent		= this ;
				rend.FirstPerson = firstPerson ;

				rend.matFlagsModifier = &UnsetTransparentCamoMatFlags;

				float len = ( Cam - GetPosition() ).Length() ;

				// render after tcamo hecne +1
				rend.SortValue	= RENDERABLE_EMITTER_USER_SORT_VALUE | ( GetRevIDist( len ) + 1 );

				render_arrays[ rsDrawTransparents ].PushBack( rend ) ;			
			}

			if(m_needToDrawBarricadePlacement)
			{
				r3dMesh* riotMesh = NULL;
				if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_BarbWireBarricade)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Barricade_BarbWire_Built.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				else if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_WoodShieldBarricade)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Barricade_WoodShield_Built.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				else if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_RiotShieldBarricade)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Riot_Shield_01.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				else if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_SandbagBarricade)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\item_barricade_Sandbag_built.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				else if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_PersonalLocker)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Lockbox_01_Crate.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				if(riotMesh->IsDrawable())
				{
					r3dPoint3D *scale ;
					r3dPoint2D *texcScale ;
					scale = riotMesh->IsSkeletal() ? 0 : &riotMesh->unpackScale;
					texcScale = &riotMesh->texcUnpackScale ;

					D3DXMATRIX m1, m2, m3, world;
					r3dPoint3D pos = GetPosition() + GetvForw() * 1.0f;
					D3DXMatrixTranslation(&m1, pos.x, pos.y, pos.z);
					D3DXMatrixRotationY(&m2, R3D_PI);
					D3DXMatrixMultiply(&m3, &DrawRotMatrix, &m2);
					D3DXMatrixMultiply(&world, &m3, &m1);
					r3dPrepareMeshVSConsts(m_ConsumableItemPlacementMeshVSConsts, world, scale, texcScale, r3dRenderer->ViewMatrix, r3dRenderer->ViewProjMatrix );

					// check if we can place our riot shield. I don't want to do this in update, as anyway this function is called every frame, so should be fine
					{
						PxBoxGeometry bbox(riotMesh->localBBox.Size.x*0.5f, riotMesh->localBBox.Size.y*0.5f, riotMesh->localBBox.Size.z*0.5f);
						D3DXMATRIX rot = GetRotationMatrix();
						PxMat33 meshRot(PxVec3(rot._11, rot._21, rot._31), PxVec3(rot._12, rot._22, rot._32), PxVec3(rot._13, rot._23, rot._33));
						PxTransform pose(PxVec3(pos.x, pos.y+(riotMesh->localBBox.Size.y*0.5f)+0.5f, pos.z), PxQuat(meshRot));

						PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
						PxShape* shape;
						m_canPlaceConsumableItem = !g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, shape, filter) && bOnGround;
						if(m_canPlaceConsumableItem) // check if we are trying to place it on top of usable item
						{
							bbox.halfExtents.y *= 4.0f;
							PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
							PxShape* results[64] = {0};
							int numRes = g_pPhysicsWorld->PhysXScene->overlapMultiple(bbox, pose, results, 64, filter);
							for(int i=0; i<numRes; ++i)
							{
								PhysicsCallbackObject* clbObj = NULL;
								if( results[i] && (clbObj = static_cast<PhysicsCallbackObject*>(results[i]->getActor().userData)))
								{
									GameObject* gameObj = clbObj->isGameObject();
									if(gameObj && gameObj->isObjType(OBJTYPE_GameplayItem))
									{
										m_canPlaceConsumableItem = false;
										break;
									}
								}
							}
						}
					}

					{
						float dist = (Cam - GetPosition()).Length();

						uint32_t prevTranspCount = render_arrays[rsDrawTransparents].Count();

						riotMesh->AppendTransparentRenderables( render_arrays[rsDrawTransparents], m_canPlaceConsumableItem?r3dColor::white:r3dColor::red, dist, 1 );

						for( uint32_t i = prevTranspCount, e = render_arrays[rsDrawTransparents].Count(); i < e; i ++ )
						{
							PlaceableConsumableItemRenderable& rend = static_cast<PlaceableConsumableItemRenderable&>( render_arrays[rsDrawTransparents][ i ] ) ;
							rend.Init( 1 ) ;
							rend.Parent = this;
						}
					}
				}
			}
		}
	}

	// debug
	{
		PlayerDebugRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_AI_PLAYER_SORT_VALUE;

		render_arrays[ rsDrawBoundBox ].PushBack( rend );
	}

	// Flash UI
#ifndef FINAL_BUILD
	if((!d_video_spectator_mode->GetBool() && !d_disable_game_hud->GetBool() && !d_observer_mode->GetBool()) || d_force_scope_rendering->GetBool())
#endif
	{
		PlayerFlashRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_AI_PLAYER_SORT_VALUE;

		render_arrays[ rsDrawFlashUI ].PushBack( rend );
	}
}

void obj_Player::UpdateTransform()
{
	GameObject::UpdateTransform();
}

//////////////////////////////////////////////////////////////////////////
void obj_Player::stopFootstepsSound()
{
	if (SoundSys.IsHandleValid(footStepsSnd))
	{
		SoundSys.Stop(footStepsSnd);
	}
}

void obj_Player::UpdateSounds()
{
	R3DPROFILE_FUNCTION("UpdateSounds");

	if(NetworkLocal) // local sounds
	{
		/*	m_bswim = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_NORMAL"), GetPosition(), true);
		SoundSys.Stop(m_bswim);
		r3d_assert(m_bswim);

		m_bswimf = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_FAST"), GetPosition(), true);
		SoundSys.Stop(m_bswimf);
		r3d_assert(m_bswimf);*/

		if(!SoundSys.isPlaying(m_sndClothMovementSprint) && PlayerState == PLAYER_MOVE_SPRINT) // started sprinting
		{
			SoundSys.Start(m_sndClothMovementSprint);
		}
		else if(SoundSys.isPlaying(m_sndClothMovementSprint) && PlayerState != PLAYER_MOVE_SPRINT) // stopped sprinting
		{
			SoundSys.Stop(m_sndClothMovementSprint);
		}

		float breathVolume = 0.0f;
		float curStaminaR = m_Stamina/getMaxStamina();
		if(curStaminaR < 0.15f && curStaminaR > 0.1f)
			breathVolume = 1.0f-((curStaminaR-0.1f)*20.0f);
		else if(curStaminaR <= 0.1f)
			breathVolume = 1.0f;
		else if(curStaminaR > 0.15f)
			breathVolume = 0.0f;

		/*if (SoundSys.isPlaying(m_bswim) && bSwimShift)
		{
		SoundSys.Stop(m_bswim);
		}

		if (SoundSys.isPlaying(m_bswim) && !bSwim)
		{
		SoundSys.Stop(m_bswim);
		}
		else if (!SoundSys.isPlaying(m_bswim) && bSwim && !bSwimShift)
		{
		SoundSys.Start(m_bswim);
		}

		if (SoundSys.isPlaying(m_bswimf) && !bSwimShift)
		{
		SoundSys.Stop(m_bswimf);
		}
		else if (!SoundSys.isPlaying(m_bswimf) && bSwimShift)
		{
		SoundSys.Start(m_bswimf);
		}*/

		if(SoundSys.isPlaying(m_sndBreathSprint) && breathVolume <= 0.0f)
			SoundSys.Stop(m_sndBreathSprint);
		else if(!SoundSys.isPlaying(m_sndBreathSprint) && breathVolume > 0.0f)
			SoundSys.Start(m_sndBreathSprint);

		if(SoundSys.isPlaying(m_sndBreathSprint))
		{
			breathVolume *= m_sndBreathBaseVolume;
			SoundSys.SetProperty(m_sndBreathSprint, FMOD_EVENTPROPERTY_VOLUME, &breathVolume);
		}

		if(CurLoadout.Health < 15.0f && !SoundSys.isPlaying(m_sndInPain) && !bDead)
			SoundSys.Start(m_sndInPain);
		else if((CurLoadout.Health >= 15.0f || bDead) && SoundSys.isPlaying(m_sndInPain))
			SoundSys.Stop(m_sndInPain);


		bool scopeRendering = hasScopeMode() && CurrentRig.allowScope && m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getCategory() == storecat_SNP;
		if(scopeRendering)
		{
			if(m_isHoldingBreath && m_Breath > 0)
			{
				if(!SoundSys.isPlaying(m_sndSniperHeartSlow))
				{
					SoundSys.Stop(m_sndSniperBreath);
					SoundSys.Start(m_sndSniperHeartSlow);
					SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_INHALE_F":"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_INHALE_M"), GetPosition());
				}
			}
			else
			{
				if(SoundSys.isPlaying(m_sndSniperHeartSlow))
				{
					SoundSys.Stop(m_sndSniperHeartSlow);
					SoundSys.Start(m_sndSniperExhale);
				}
				if(!SoundSys.isPlaying(m_sndSniperExhale))
					if(!SoundSys.isPlaying(m_sndSniperBreath))
						SoundSys.Start(m_sndSniperBreath);
			}
		}
		else
		{
			if(SoundSys.isPlaying(m_sndSniperBreath))
				SoundSys.Stop(m_sndSniperBreath);
			if(SoundSys.isPlaying(m_sndSniperHeartSlow))
				SoundSys.Stop(m_sndSniperHeartSlow);
		}
	}
	else // for all players, 3D sounds
	{

	}
}

void obj_Player::UpdateFootsteps()
{
	R3DPROFILE_FUNCTION("UpdateFootsteps");
	R3DPROFILE_START("RayCast");
	// cast ray to see on what material player is standing
	PhysicsCallbackObject* target = NULL;
	const MaterialType *mt = 0;

	PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));

	// did we hit anything?
	bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 1.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
	if( hitResult )
	{
		if( hit.shape && (target = static_cast<PhysicsCallbackObject*>(hit.shape->getActor().userData)))
		{
			r3dMaterial* material = 0;
			GameObject *gameObj = target->isGameObject();
			if(gameObj)
			{
				if( gameObj->isObjType( OBJTYPE_Mesh ) )
					material = static_cast< MeshGameObject* > ( target )->MeshLOD[ 0 ]->GetFaceMaterial( hit.faceIndex );
				if(gameObj->isObjType(OBJTYPE_Terrain))
				{
					//mt = Terrain->GetMaterialType(GetPosition());
					//mt = Terrain2->GetMaterialType(GetPosition());
					mt = g_pMaterialTypes->Get(r3dHash::MakeHash("Sand"));
				}
			}
			else if(target->hasMesh())
			{
				material = target->hasMesh()->GetFaceMaterial( hit.faceIndex );
			}
			if (!material)
				material = target->GetMaterial(hit.faceIndex);

			if(material && !mt) // get material type from mesh material
			{
				mt = g_pMaterialTypes->Get(r3dHash::MakeHash(material->TypeName));
			}

			float waterDepth = getWaterDepthAtPos(GetPosition());
			if(waterDepth > 0.1f) 
			{
				mt = g_pMaterialTypes->Get(r3dHash::MakeHash("Water"));
			}

		}
	}
	R3DPROFILE_END("RayCast");

	{
		R3DPROFILE_FUNCTION("play sound");
		// hack: right now we have a problem with faceID in raycast not matching faceID of the actual mesh (collision mesh vs regular mesh). 
		// So, in case if we failed to find a proper material because of that, let's just use old foot step sound and that's it
		int fts = currentFootStepsSoundID; 
		if(mt && !bSwim)
		{
			if(bCrouch && mt->crouchFootstepsSound>=0)
				fts = mt->crouchFootstepsSound;
			else if(bProne && mt->proneFootstepsSound>=0)
				fts = mt->proneFootstepsSound;
			/*else if (bSwim && !bSwimShift)
			fts = mt->SwimSound;
			else if (bSwimShift)
			fts = mt->SwimFSound;*/
			else
				fts = mt->footStepsSound;
		}

		if (bSwim && PlayerState == PLAYER_SWIM)
			fts = SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_SLOW");
		else if (bSwim && !bSwimShift)
			fts = SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_NORMAL");
		else if (bSwimShift)
			fts = SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_FAST");

		if (bSwim && PlayerMoveDir == CUberData::ANIMDIR_Stand) // swim idie
		{
			fts = SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_IDIE");
		}

		if (isDriving() && d_drive_vehiclescon->GetBool())
			fts = SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggy_Engine");

		//footStepsSnd = NULL;

		if (!SoundSys.IsHandleValid(footStepsSnd) || fts != currentFootStepsSoundID)
		{
			SoundSys.Stop(footStepsSnd);
			footStepsSnd = NULL;

			for(uint32_t i=0; i<footStepsArr.Count(); ++i) 
			{
				if(footStepsArr[i].id == fts)
				{
					footStepsSnd = footStepsArr[i].snd;
					// check if handle is valid, sometimes it becomes invalid for some reason
					if(!SoundSys.IsHandleValid(footStepsSnd))
					{
						// handle is broken, remove it from the list, and remove it from FMOD
						SoundSys.Stop(footStepsArr[i].snd);
						SoundSys.Release(footStepsArr[i].snd);
						footStepsArr.Erase(i);	
						footStepsSnd = NULL;
					}
					break;
				}
			}

			if(footStepsSnd == NULL)
			{
				footStepsSnd = SoundSys.Play(fts, GetPosition());
				if (!SoundSys.IsHandleValid(footStepsSnd))
					return;
				footStepsArr.PushBack(footStepEntry_s(fts, footStepsSnd));
			}

			currentFootStepsSoundID = fts;
		}

		SoundSys.Set3DAttributes(footStepsSnd, &GetPosition(), 0, 0);

		static const float SpawnRatesByState[] = {
			0,	//PLAYER_IDLE,
			0,	//PLAYER_IDLEAIM,
			0.6f,	//PLAYER_MOVE_CROUCH,
			0.54f,	//PLAYER_MOVE_CROUCH_AIM,
			0.49f,	//PLAYER_MOVE_WALK_AIM,
			0.95f,	//PLAYER_MOVE_RUN,
			1.0f,	//PLAYER_MOVE_SPRINT,
			0.95f,	//PLAYER_MOVE_SWIM,
			0.95f,	//PLAYER_MOVE_SWIM,
			0.95f,	//PLAYER_MOVE_SWIM,
			1.0f,	//PRONE
			1.0f,	//PRONE_AIM
			1.0f,  //PRONE_UP
			0,	//PRONE_DOWN
			0,	//PRONE_IDLE
			0,	//PLAYER_DIE,
		};
		COMPILE_ASSERT( R3D_ARRAYSIZE(SpawnRatesByState) == PLAYER_NUM_STATES ) ;
		float spawnRate = SpawnRatesByState[PlayerState];
		if(hitResult == false && !bSwim)
		{
			spawnRate = 0.0f;
		}
		if (PlayerMoveDir == CUberData::ANIMDIR_Stand && !bSwim)
		{
			spawnRate = 0.0f;
		}

		if(spawnRate > 0)
		{
			footsteps_timeSinceLastMissingHitResult = r3dGetTime();
			if (SoundSys.GetState(footStepsSnd) != FMOD_EVENT_STATE_PLAYING)
			{
				SoundSys.Start(footStepsSnd);
			}

			SoundSys.SetProperty(footStepsSnd, FMOD_EVENTPROPERTY_SPAWNINTENSITY, &spawnRate);
		}
		else
		{
			if((r3dGetTime() - footsteps_timeSinceLastMissingHitResult)>0.25f && !bSwim) // workaround when you are running across wooden planks and sounds restart each time you step on new wooden plank
				SoundSys.Stop(footStepsSnd);
			else if (bSwim)
				SoundSys.Stop(footStepsSnd);
		}
	}
}

void obj_Player::updateWeaponUI()
{
	if(!hudMain)
		return;
	if(!NetworkLocal)
		return;

	for(int i=0; i<6; ++i)
	{
		const BaseItemConfig* baseItem = g_pWeaponArmory->getConfig(CurLoadout.Items[i].itemID);
		int numCharges = CurLoadout.Items[i].quantity;
		if(baseItem)
		{
			if (baseItem->m_itemID != 101398)
				hudMain->setSlotInfo(i, baseItem->m_StoreName, numCharges, baseItem->m_StoreIcon);
		}
		else
			hudMain->setSlotInfo(i, "", 0, "");
	}
	if(!forcedEmptyHands && m_Weapons[m_SelectedWeapon])
		hudMain->setActiveSlot(m_SelectedWeapon);
	else
		hudMain->setActiveSlot(100);

	{
		bool isMelee = false;
		if(m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getCategory() == storecat_MELEE)
			isMelee = true;
		if(!forcedEmptyHands && ((m_SelectedWeapon < 2 && !isMelee && m_Weapons[m_SelectedWeapon]) || (m_SelectedWeapon>=2 && m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getCategory() == storecat_GRENADE)))
		{
			hudMain->showWeaponInfo(true);
		}
		else
		{
			hudMain->showWeaponInfo(false);
		}
	}
}

void obj_Player::switchFPS_TPS()
{
	r3d_assert(NetworkLocal);

	g_camera_mode->SetInt(g_camera_mode->GetInt()==2?0:2);

	// we only need to reload body meshes
	const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(CurLoadout.HeroItemID);
	loadHeroMeshes(heroConfig);
	// reload weapon meshes (to load skeleton for FPS mode)
	// create weapons & items
	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; i++)
	{
		if(m_Weapons[i])
		{
			m_Weapons[i]->setFirstPersonMeshFlag(g_camera_mode->GetInt()==2);
			m_Weapons[i]->ResetMeshLoadedFlag();
			uint32_t attmIDs[WPN_ATTM_MAX];
			m_Weapons[i]->getCurrentAttachmentIDs(attmIDs);
			uint32_t zeroID[WPN_ATTM_MAX] = {0};
			m_Weapons[i]->setWeaponAttachmentsByIDs(zeroID); // reset attachments if any
			m_Weapons[i]->setWeaponAttachmentsByIDs(attmIDs); // reload attachments
		}
	}

	uberAnim_->anim.StopAll();	// prevent animation blending on loadout switch
	uberAnim_->AnimPlayerState = -1;
	SyncAnimation(true);
}

void obj_Player::switchToEmptyHands()
{
	if(forcedEmptyHands)
		return;

	forcedEmptyHands = true;
	if(m_Weapons[m_SelectedWeapon])
		m_Weapons[m_SelectedWeapon]->OnUnequip();
	SyncAnimation(true);

	if(NetworkLocal)
		updateWeaponUI();
}

float obj_Player::getPlayerHeightForCamera() const
{
	return Height;
}

//////////////////////////////////////////////////////////////////////////

void obj_Player::DoRagdoll(bool toRagdoll, int hitBone, const r3dPoint3D* impulse)
{
#if ENABLE_RAGDOLL
	if (m_PhysSkeleton)
	{
		if (impulse)
		{
			m_PhysSkeleton->SwitchToRagdollWithForce(toRagdoll, hitBone, impulse);
		}
		else
		{
			m_PhysSkeleton->SwitchToRagdoll(toRagdoll);
		}

		if (!m_PhysSkeleton->IsRagdollMode())
		{
			SetBBoxLocal(m_OrigBBox);
		}
	}
#endif
}

int obj_Player::GetInventoryIdByItemId(int ItemId,const wiCharDataFull& Loadout)
{
	for (int i=0;i<Loadout.BackpackSize;i++)
	{
		if (Loadout.Items[i].itemID == ItemId) return i;
	}

	return -1;
}

void obj_Player::OnGameEnded()
{
	if(m_SelectedWeapon >= 0 && m_Weapons[m_SelectedWeapon] )
	{
		// currently we only need to do this to the equipped weapon, it can be expanded as needed.
		m_Weapons[m_SelectedWeapon]->OnGameEnded();
	}
}

void obj_Player::OnBackpackChanged(int idx)
{
	r3d_assert(NetworkLocal);

	if(idx == wiCharDataFull::CHAR_LOADOUT_WEAPON1 || idx == wiCharDataFull::CHAR_LOADOUT_WEAPON2)
	{
		// attachments are reset on item change (SERVER CODE SYNC POINT)
		CurLoadout.Attachment[idx].Reset();
		if(CurLoadout.Items[idx].Var2 > 0)
			CurLoadout.Attachment[idx].attachments[WPN_ATTM_CLIP] = CurLoadout.Items[idx].Var2;
	}

	// check if we need to modify loadout
	if(idx >= wiCharDataFull::CHAR_LOADOUT_WEAPON1 && idx <= wiCharDataFull::CHAR_LOADOUT_HEADGEAR /*wiCharDataFull::CHAR_LOADOUT_WEAPON2*/)
	{
		UpdateLoadoutSlot(CurLoadout);

		updateWeaponUI();
	}

	// if trying to put weapon into quick slot that is active, then force empty hands
	if(idx >= wiCharDataFull::CHAR_LOADOUT_ITEM1 && idx <= wiCharDataFull::CHAR_LOADOUT_ITEM4 && 
		m_SelectedWeapon == idx && m_Weapons[m_SelectedWeapon] && !m_Weapons[m_SelectedWeapon]->isUsableItem())
	{	
		switchToEmptyHands();
		{
			PKT_C2C_PlayerSwitchWeapon_s n;
			n.wid       = forcedEmptyHands?255:m_SelectedWeapon;
			p2pSendToHost(this, &n, sizeof(n), true);
		}
	}


	if(hudPause && hudPause->isActive()) hudPause->reloadBackpackInfo();

	if(NetworkLocal && (idx == wiCharDataFull::CHAR_LOADOUT_ARMOR || idx == wiCharDataFull::CHAR_LOADOUT_HEADGEAR))
		SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_CHANGECLOTHES"), GetPosition());
}

#if VEHICLES_ENABLED
obj_Vehicle* obj_Player::canEnterVehicle()
{
	// Fast version from Frank
	for( GameObject* obj = GameWorld().GetFirstObject(); obj; obj = GameWorld().GetNextObject(obj) )
	{
		if( obj->ObjTypeFlags && OBJTYPE_Vehicle )
		{
			obj_Vehicle* vehicle= static_cast< obj_Vehicle* > ( obj );

			float distSq = (GetPosition() - vehicle->GetPosition()).LengthSq();
			if( distSq < gDist_To_Allow_Vehicle_Entry )
			{
				return vehicle;
			}
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void obj_Player::TogglePhysicsSimulation(bool on)
{
	if (bPhys == on) return;

	bPhys = on;

	if (PhysicsObject)
	{
		PxActor *a = PhysicsObject->getPhysicsActor();
		if (a)
		{
			on ? g_pPhysicsWorld->AddActor(*a) : g_pPhysicsWorld->RemoveActor(*a);
		}
		if (m_PhysSkeleton)
		{
			m_PhysSkeleton->TogglePhysicsSimulation(on);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void obj_Player::enterVehicle( obj_Vehicle* target_Vehicle )
{
	//SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggyEngine_Start"), GetPosition(), true); //SetPosition(ActualVehicle->GetPosition());
	//SoundSys.Start(m_sndVehicleDrive);
	//ActualVehicle=target_Vehicle;
	d_drive_vehiclescon->SetBool(true);
	TogglePhysicsSimulation(false);

	target_Vehicle->SwitchToDrivable( true );
	vehicleViewActive_ = VehicleView_Driver;
	d_drive_vehicles->SetBool( true );
	//target_Vehicle->owner = this;
	target_Vehicle->NetworkLocal = true;
	PKT_C2C_CarStatus_s n;
	n.CarID = toP2pNetId(target_Vehicle->GetNetworkID());
	p2pSendToHost(this,&n,sizeof(n));
	//target_Vehicle->vd->owner=target_Vehicle;
}

void obj_Player::exitVehicle()
{
	hudMain->setCarInfo(0,0,0,0,0,false);
	//SoundSys.Stop(m_sndVehicleDrive);
	SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/DuneBuggyEngine_Stop"), curcar->GetPosition(), true);
	r3dVector targetPosition = curcar->GetPosition() + r3dPoint3D( 4, 3, 0 ); // Server Vehicles
	TeleportPlayer( targetPosition, "Exiting vehicle");
	curcar->SwitchToDrivable( false );
	vehicleViewActive_ = VehicleView_None;
	d_drive_vehiclescon->SetBool(false);
	d_drive_vehicles->SetBool( false );
	curcar->SwitchToDrivable( false );
	PKT_C2C_CarStatus_s n;
	//n.status = false;
	//n.fuel = curcar->fuel;
	n.CarID = toP2pNetId(curcar->GetNetworkID());
	p2pSendToHost(this,&n,sizeof(n));
	//	g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->owner = NULL;
	curcar->NetworkLocal = false;
	//g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle()->vd->owner=0;
	TogglePhysicsSimulation(true);
}
bool obj_Player::isDriving()
{
	return vehicleViewActive_ == VehicleView_Driver;
}

bool obj_Player::isInVehicle() 
{
	return vehicleViewActive_ != VehicleView_None;
}
bool obj_Player::TestExitPosition( r3dVector& exitPosition, bool ignoreGround)
{

	// snap to ground
	if ( ignoreGround == false ) {
		// first find out if there's ground. 
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
		if(g_pPhysicsWorld->raycastSingle(PxVec3(exitPosition.x, exitPosition.y+1, exitPosition.z), PxVec3(0, -1, 0), 2, PxSceneQueryFlag::eIMPACT, hit, filter))
		{
			exitPosition.x = hit.impact.x;
			exitPosition.y = hit.impact.y;
			exitPosition.z = hit.impact.z;
		} 
		else 
		{
			return false;
		}
	}

	// check spawn area. 
	PxShape* overlapHit = NULL;
	PxU32 collisionFlag = COLLIDABLE_STATIC_MASK ;
	PxBoxGeometry playerBox( 0.2f, 0.9f, 0.2f) ;
	PxTransform pose(PxVec3(exitPosition.x, exitPosition.y +1.1f, exitPosition.z), PxQuat(0,0,0,1));
	PxSceneQueryFilterData capsulefilter(PxFilterData(collisionFlag, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
	if ( g_pPhysicsWorld->PhysXScene->overlapAny( playerBox, pose, overlapHit, capsulefilter))
	{
		return false;
	}

	return true;
}

#endif


//////////////////////////////////////////////////////////////////////////

void ProcessCharacterEditor(obj_Player* pl, float left, float top, float height);
void ProcessCharacterEditor()
{
	obj_Player* pl = (obj_Player *)GameWorld().GetObject(EditorGameHUD::editorPlayerId);
	if (!pl)
	{
		imgui_Static(r3dRenderer->ScreenW-375, 85, "Spawn player firstly by pressing F8");
		return;
	}

	ProcessCharacterEditor(pl, 100, 100, 150);
}

void ProcessCharacterEditor(obj_Player* pl, float left, float top, float height)
{
	stringlist_t weaponDB_nameList;
	uint32_t* weaponDB_idList = 0;

	stringlist_t attachmentDB_muzzle_nameList;
	uint32_t* attachmentDB_muzzle_idList = 0;
	stringlist_t attachmentDB_upperRail_nameList;
	uint32_t* attachmentDB_upperRail_idList = 0;
	stringlist_t attachmentDB_leftRail_nameList;
	uint32_t* attachmentDB_leftRail_idList = 0;
	stringlist_t attachmentDB_bottomRail_nameList;
	uint32_t* attachmentDB_bottomRail_idList = 0;
	stringlist_t attachmentDB_clip_nameList;
	uint32_t* attachmentDB_clip_idList = 0;

	static int selectedPrimaryWeaponIndex = -1;
	static int selectedSecondaryWeaponIndex = -1;
	static int selectedBodyIndex = 0;
	static int selectedHeroIndex = 0;
	static int selectedLegsIndex = 0;
	static int selectedArmorIndex = 0;
	static int selectedHeadIndex = 0;
	static int selectedHeadArmorIndex = 0;

	stringlist_t armorDBnames, bodyDBnames, heroDBnames, legsDBnames, headDBnames, headarmorBDnames;
	uint32_t *armorDBids = 0, *bodyDBids = 0, *heroDBids = 0, *legsDBids = 0, *headDBids = 0, *headarmorDBids = 0;
	if(weaponDB_nameList.empty()) // fill in list
	{
		weaponDB_nameList.push_back("empty");
		attachmentDB_muzzle_nameList.push_back("empty");
		attachmentDB_upperRail_nameList.push_back("empty");
		attachmentDB_leftRail_nameList.push_back("empty");
		attachmentDB_bottomRail_nameList.push_back("empty");
		attachmentDB_clip_nameList.push_back("empty");

		// sort item names there.
		struct CWpnNameSort {
			static int sort(const void* P1, const void* P2)	{
				const WeaponConfig* g1 = *(const WeaponConfig**)P1;
				const WeaponConfig* g2 = *(const WeaponConfig**)P2;
				return strcmp(g1->m_StoreIcon, g2->m_StoreIcon);
			}
		};
		struct CWpnAttachmentNameSort {
			static int sort(const void* P1, const void* P2)	{
				const WeaponAttachmentConfig* g1 = *(const WeaponAttachmentConfig**)P1;
				const WeaponAttachmentConfig* g2 = *(const WeaponAttachmentConfig**)P2;
				return strcmp(g1->m_StoreIcon, g2->m_StoreIcon);
			}
		};
		std::vector<const WeaponConfig*> allWpns;
		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const WeaponConfig* config = g_pWeaponArmory->getWeaponConfig(itemID);
			if(config)
			{
				allWpns.push_back(config);
			}
		}
		qsort(&allWpns[0], allWpns.size(), sizeof(WeaponConfig*), CWpnNameSort::sort);

		int numItemsInWeaponList = g_pWeaponArmory->getNumWeapons() + 1;
		weaponDB_idList = new uint32_t[numItemsInWeaponList];
		weaponDB_idList[0] = -1;
		for(int i=1; i<numItemsInWeaponList; ++i)
		{
			const WeaponConfig* config = allWpns[i-1];

			// get item FNAME without .dds
			char wname[256];
			sprintf(wname, strrchr(config->m_StoreIcon, '/') + 1);
			wname[strlen(wname)-4] = 0;

			weaponDB_nameList.push_back(wname);
			weaponDB_idList[i] = config->m_itemID;

			if(selectedPrimaryWeaponIndex==-1 && config->m_itemID == pl->CurLoadout.Items[0].itemID)
				selectedPrimaryWeaponIndex = i;
			if(selectedSecondaryWeaponIndex==-1 && config->m_itemID == pl->CurLoadout.Items[1].itemID)
				selectedSecondaryWeaponIndex = i;
		}

		if(selectedPrimaryWeaponIndex==-1)
			selectedPrimaryWeaponIndex = 0;
		if(selectedSecondaryWeaponIndex==-1)
			selectedSecondaryWeaponIndex = 0;

		int numItemsInAttachmentList = g_pWeaponArmory->getNumAttachments() + 1;
		attachmentDB_muzzle_idList = new uint32_t[numItemsInAttachmentList]; int numMuzzles = 0;
		attachmentDB_muzzle_idList[0] = 0;
		attachmentDB_upperRail_idList = new uint32_t[numItemsInAttachmentList]; int numUpperRails = 0;
		attachmentDB_upperRail_idList[0] = 0;
		attachmentDB_leftRail_idList = new uint32_t[numItemsInAttachmentList]; int numLeftRails = 0;
		attachmentDB_leftRail_idList[0] = 0;
		attachmentDB_bottomRail_idList = new uint32_t[numItemsInAttachmentList]; int numBottomRails = 0;
		attachmentDB_bottomRail_idList[0] = 0;
		attachmentDB_clip_idList = new uint32_t[numItemsInAttachmentList]; int numClips = 0;
		attachmentDB_clip_idList[0] = 0;

		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const WeaponAttachmentConfig* config = g_pWeaponArmory->getAttachmentConfig(itemID);
			if(config)
			{
				switch(config->m_type)
				{
				case WPN_ATTM_MUZZLE:
					attachmentDB_muzzle_nameList.push_back(config->m_StoreName);
					attachmentDB_muzzle_idList[++numMuzzles] = config->m_itemID;
					break;
				case WPN_ATTM_UPPER_RAIL:
					attachmentDB_upperRail_nameList.push_back(config->m_StoreName);
					attachmentDB_upperRail_idList[++numUpperRails] = config->m_itemID;
					break;
				case WPN_ATTM_LEFT_RAIL:
					attachmentDB_leftRail_nameList.push_back(config->m_StoreName);
					attachmentDB_leftRail_idList[++numLeftRails] = config->m_itemID;
					break;
				case WPN_ATTM_BOTTOM_RAIL:
					attachmentDB_bottomRail_nameList.push_back(config->m_StoreName);
					attachmentDB_bottomRail_idList[++numBottomRails] = config->m_itemID;
					break;
				case WPN_ATTM_CLIP:
					attachmentDB_clip_nameList.push_back(config->m_StoreName);
					attachmentDB_clip_idList[++numClips] = config->m_itemID;
					break;
				default:
					break;
				}
			}
		}
		int numItemsInGearList = g_pWeaponArmory->getNumGears() + 1;
		armorDBids = new uint32_t[numItemsInGearList]; int numArmor = 0;
		headarmorDBids = new uint32_t[numItemsInGearList]; int numHeadarmor = 0;

		headarmorBDnames.push_back("empty");
		armorDBnames.push_back("empty");
		armorDBids[0] = 0;
		headarmorDBids[0] = 0;

		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const GearConfig* config = g_pWeaponArmory->getGearConfig(itemID);
			if(config)
			{
				switch(config->category)
				{
				case storecat_Armor:
					armorDBnames.push_back(config->m_StoreName);
					armorDBids[++numArmor] = config->m_itemID;
					if(selectedArmorIndex==-1 && config->m_itemID == pl->CurLoadout.Items[6].itemID)
						selectedArmorIndex = numArmor;
					break;
				case storecat_Helmet:
					headarmorBDnames.push_back(config->m_StoreName);
					headarmorDBids[++numHeadarmor] = config->m_itemID;
					if(selectedHeadArmorIndex==-1 && config->m_itemID == pl->CurLoadout.Items[7].itemID)
						selectedHeadArmorIndex = numHeadarmor;
					break;
				default:
					break;
				}
			}
		}
		int numItemsInHeroList = g_pWeaponArmory->getNumHeroes();
		heroDBids = new uint32_t[numItemsInHeroList]; int numHero = 0;

		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const HeroConfig* config = g_pWeaponArmory->getHeroConfig(itemID);
			if(config)
			{
				heroDBnames.push_back(config->m_StoreName);
				heroDBids[numHero] = config->m_itemID;
				if(selectedHeroIndex==-1 && config->m_itemID == pl->CurLoadout.HeroItemID)
					selectedHeroIndex = numHero;
				numHero++;
			}
		}
	}

	static const int width = 150;
	//	static const int height = 150;
	static const int shift = 25;

	static int selectedPrimaryWeapon_Muzzle = 0;
	static int selectedPrimaryWeapon_UpperRail = 0;
	static int selectedPrimaryWeapon_LeftRail = 0;
	static int selectedPrimaryWeapon_BottomRail= 0;
	static int selectedPrimaryWeapon_Clip = 0;

	static int selectedSecondaryWeapon_Muzzle = 0;
	static int selectedSecondaryWeapon_UpperRail = 0;
	static int selectedSecondaryWeapon_LeftRail = 0;
	static int selectedSecondaryWeapon_BottomRail= 0;
	static int selectedSecondaryWeapon_Clip = 0;

	float offset = top + shift + height - 20;
	{
		static float primWeaponOffset = 0;
		imgui_Static(left + width * 7, top, "Primary Weapon", width);
		imgui_DrawList(left + width * 7, top + shift, (float)width, (float)height, weaponDB_nameList, &primWeaponOffset, &selectedPrimaryWeaponIndex);

		if(g_camera_mode->GetInt() == 2)
		{
			static float primWeaponAttachmentMuzzleOffset = 0;
			imgui_Static(left + width * 7, offset, "Muzzle", width);
			imgui_DrawList(left + width * 7, offset + 20, (float)width, (float)100.0f, attachmentDB_muzzle_nameList, &primWeaponAttachmentMuzzleOffset, &selectedPrimaryWeapon_Muzzle);

			static float primWeaponAttachmentUpperRailOffset = 0;
			imgui_Static(left + width * 7, offset + 100, "Upper Rail", width);
			imgui_DrawList(left + width * 7, offset + 20 + 100, (float)width, (float)100.0f, attachmentDB_upperRail_nameList, &primWeaponAttachmentUpperRailOffset, &selectedPrimaryWeapon_UpperRail);

			static float primWeaponAttachmentLeftRailOffset = 0;
			imgui_Static(left + width * 7, offset + 200, "Left Rail", width);
			imgui_DrawList(left + width * 7, offset + 20 + 200, (float)width, (float)100.0f, attachmentDB_leftRail_nameList, &primWeaponAttachmentLeftRailOffset, &selectedPrimaryWeapon_LeftRail);

			static float primWeaponAttachmentBottomRailOffset = 0;
			imgui_Static(left + width * 7, offset + 300, "Bottom Rail", width);
			imgui_DrawList(left + width * 7, offset + 20 + 300, (float)width, (float)100.0f, attachmentDB_bottomRail_nameList, &primWeaponAttachmentBottomRailOffset, &selectedPrimaryWeapon_BottomRail);

			static float primWeaponAttachmentClipOffset = 0;
			imgui_Static(left + width * 7, offset + 400, "Clip", width);
			imgui_DrawList(left + width * 7, offset + 20 + 400, (float)width, (float)100.0f, attachmentDB_clip_nameList, &primWeaponAttachmentClipOffset, &selectedPrimaryWeapon_Clip);
		}
	}

	{
		static float secWeaponOffset = 0;
		imgui_Static(left + width * 6, top, "Secondary Weapon", width);
		imgui_DrawList(left + width * 6, top + shift, (float)width, (float)height, weaponDB_nameList, &secWeaponOffset, &selectedSecondaryWeaponIndex);

		if(g_camera_mode->GetInt() == 2)
		{
			static float primWeaponAttachmentMuzzleOffset = 0;
			imgui_Static(left + width * 6, offset, "Muzzle", width);
			imgui_DrawList(left + width * 6, offset + 20, (float)width, (float)100.0f, attachmentDB_muzzle_nameList, &primWeaponAttachmentMuzzleOffset, &selectedSecondaryWeapon_Muzzle);

			static float primWeaponAttachmentUpperRailOffset = 0;
			imgui_Static(left + width * 6, offset + 100, "Upper Rail", width);
			imgui_DrawList(left + width * 6, offset + 20 + 100, (float)width, (float)100.0f, attachmentDB_upperRail_nameList, &primWeaponAttachmentUpperRailOffset, &selectedSecondaryWeapon_UpperRail);

			static float primWeaponAttachmentLeftRailOffset = 0;
			imgui_Static(left + width * 6, offset + 200, "Left Rail", width);
			imgui_DrawList(left + width * 6, offset + 20 + 200, (float)width, (float)100.0f, attachmentDB_leftRail_nameList, &primWeaponAttachmentLeftRailOffset, &selectedSecondaryWeapon_LeftRail);

			static float primWeaponAttachmentBottomRailOffset = 0;
			imgui_Static(left + width * 6, offset + 300, "Bottom Rail", width);
			imgui_DrawList(left + width * 6, offset + 20 + 300, (float)width, (float)100.0f, attachmentDB_bottomRail_nameList, &primWeaponAttachmentBottomRailOffset, &selectedSecondaryWeapon_BottomRail);

			static float primWeaponAttachmentClipOffset = 0;
			imgui_Static(left + width * 6, offset + 400, "Clip", width);
			imgui_DrawList(left + width * 6, offset + 20 + 400, (float)width, (float)100.0f, attachmentDB_clip_nameList, &primWeaponAttachmentClipOffset, &selectedSecondaryWeapon_Clip);
		}
	}

	static float heroOffset = 0;
	imgui_Static(left + width * 5, top, "Heroes", width);
	int oldIndex = selectedHeroIndex;
	imgui_DrawList(left + width * 5, top + shift, (float)width, (float)height, heroDBnames, &heroOffset, &selectedHeroIndex);
	if(oldIndex != selectedHeroIndex)
	{
		selectedBodyIndex = 0;
		selectedLegsIndex = 0;
		selectedHeadIndex = 0;
	}

	{
		int heroItemID = heroDBids[selectedHeroIndex];
		const HeroConfig* heroConf = g_pWeaponArmory->getHeroConfig(heroItemID);
		if(heroConf)
		{
			bodyDBids = new uint32_t[heroConf->getNumBodys()]; int numBody = 0;
			legsDBids = new uint32_t[heroConf->getNumLegs()]; int numLegs = 0;
			headDBids = new uint32_t[heroConf->getNumHeads()]; int numHead = 0;

			char tmpStr[16];
			for(size_t i=0; i<heroConf->getNumBodys(); ++i)
			{
				sprintf(tmpStr, "Body %d", i+1);
				bodyDBnames.push_back(tmpStr);
				bodyDBids[numBody] = i;
				if(pl->CurLoadout.BodyIdx == i)
					selectedBodyIndex = numBody;
				numBody++;
			}
			for(size_t i=0; i<heroConf->getNumLegs(); ++i)
			{
				sprintf(tmpStr, "Legs %d", i+1);
				legsDBnames.push_back(tmpStr);
				legsDBids[numLegs] = i;
				if(pl->CurLoadout.LegsIdx == i)
					selectedLegsIndex = numLegs;
				numLegs++;
			}
			for(size_t i=0; i<heroConf->getNumHeads(); ++i)
			{
				sprintf(tmpStr, "Head %d", i+1);
				headDBnames.push_back(tmpStr);
				headDBids[numHead] = i;
				if(pl->CurLoadout.HeadIdx == i)
					selectedHeadIndex = numHead;
				numHead++;
			}
		}
	}

	static float legsOffset = 0;
	imgui_Static(left + width * 4, top, "Legs", width);
	imgui_DrawList(left + width * 4, top + shift, (float)width, (float)height, legsDBnames, &legsOffset, &selectedLegsIndex);

	static float bodyOffset = 0;
	imgui_Static(left + width * 3, top, "Body", width);
	imgui_DrawList(left + width * 3, top + shift, (float)width, (float)height, bodyDBnames, &bodyOffset, &selectedBodyIndex);

	static float headOffset = 0;
	imgui_Static(left + width * 2, top, "Head", width);
	imgui_DrawList(left + width * 2, top + shift, (float)width, (float)height, headDBnames, &headOffset, &selectedHeadIndex);

	static float armorOffset = 0;
	imgui_Static(left + width * 1, top, "Armor", width);
	imgui_DrawList(left + width * 1, top + shift, (float)width, (float)height, armorDBnames, &armorOffset, &selectedArmorIndex);

	static float headArmorOffset = 0;
	imgui_Static(left + width * 0, top, "Head Armor", 150);
	imgui_DrawList(left + width * 0, top + shift, (float)width, (float)height, headarmorBDnames, &headArmorOffset, &selectedHeadArmorIndex);


	//if(imgui_Button(500, 550, 300, 30, "Apply Changes"))
	{
		wiCharDataFull loadout;
		loadout.Health = 100;
		loadout.Items[0].itemID = weaponDB_idList[selectedPrimaryWeaponIndex];
		loadout.Items[1].itemID = weaponDB_idList[selectedSecondaryWeaponIndex];
		loadout.Items[6].itemID = armorDBids[selectedArmorIndex];
		loadout.Items[7].itemID = headarmorDBids[selectedHeadArmorIndex];

		loadout.HeroItemID = heroDBids[selectedHeroIndex];
		loadout.HeadIdx = headDBids[selectedHeadIndex];
		loadout.BodyIdx = bodyDBids[selectedBodyIndex];
		loadout.LegsIdx = legsDBids[selectedLegsIndex];

		loadout.Attachment[0].attachments[WPN_ATTM_MUZZLE] = attachmentDB_muzzle_idList[selectedPrimaryWeapon_Muzzle];
		loadout.Attachment[0].attachments[WPN_ATTM_UPPER_RAIL] = attachmentDB_upperRail_idList[selectedPrimaryWeapon_UpperRail];
		loadout.Attachment[0].attachments[WPN_ATTM_LEFT_RAIL] = attachmentDB_leftRail_idList[selectedPrimaryWeapon_LeftRail];
		loadout.Attachment[0].attachments[WPN_ATTM_BOTTOM_RAIL] = attachmentDB_bottomRail_idList[selectedPrimaryWeapon_BottomRail];
		loadout.Attachment[0].attachments[WPN_ATTM_CLIP] = attachmentDB_clip_idList[selectedPrimaryWeapon_Clip];

		loadout.Attachment[1].attachments[WPN_ATTM_MUZZLE] = attachmentDB_muzzle_idList[selectedSecondaryWeapon_Muzzle];
		loadout.Attachment[1].attachments[WPN_ATTM_UPPER_RAIL] = attachmentDB_upperRail_idList[selectedSecondaryWeapon_UpperRail];
		loadout.Attachment[1].attachments[WPN_ATTM_LEFT_RAIL] = attachmentDB_leftRail_idList[selectedSecondaryWeapon_LeftRail];
		loadout.Attachment[1].attachments[WPN_ATTM_BOTTOM_RAIL] = attachmentDB_bottomRail_idList[selectedSecondaryWeapon_BottomRail];
		loadout.Attachment[1].attachments[WPN_ATTM_CLIP] = attachmentDB_clip_idList[selectedSecondaryWeapon_Clip];

		if(pl->CurLoadout.Items[0].itemID != loadout.Items[0].itemID ||
			pl->CurLoadout.Items[1].itemID != loadout.Items[1].itemID ||
			pl->CurLoadout.Items[6].itemID != loadout.Items[6].itemID ||
			pl->CurLoadout.Items[7].itemID != loadout.Items[7].itemID ||
			pl->CurLoadout.HeroItemID != loadout.HeroItemID ||
			pl->CurLoadout.HeadIdx != loadout.HeadIdx ||
			pl->CurLoadout.BodyIdx != loadout.BodyIdx ||
			pl->CurLoadout.LegsIdx != loadout.LegsIdx ||
			pl->CurLoadout.Attachment[0] != loadout.Attachment[0] || 
			pl->CurLoadout.Attachment[1] != loadout.Attachment[1]
		)
		{
			pl->UpdateLoadoutSlot(loadout);
		}
	}

	delete [] weaponDB_idList;
	delete [] attachmentDB_muzzle_idList;
	delete [] attachmentDB_upperRail_idList;
	delete [] attachmentDB_leftRail_idList;
	delete [] attachmentDB_bottomRail_idList;
	delete [] attachmentDB_clip_idList;
	delete [] armorDBids;
	delete [] bodyDBids;
	delete [] legsDBids;
	delete [] headDBids;
	delete [] headarmorDBids;
}

//------------------------------------------------------------------------

static void AdjustHitPosition( DecalParams& params, GameObject* target )
{
	R3DPROFILE_FUNCTION( "AdjustHitPosition" );
	if( target && target->isObjType( OBJTYPE_Mesh ) )
	{
		MeshGameObject* mobj = static_cast< MeshGameObject* >(target);

		float interSearchLength = mobj->GetObjectsRadius() * 0.33f;

		float dist;

		r3dMaterial* mtl;
		int minFace;

		r3dPoint3D start = params.Pos + params.Dir * interSearchLength * 0.5f;

		if( mobj->MeshLOD[ 0 ]->ContainsRay( start, -params.Dir, interSearchLength, &dist, &mtl, mobj->GetPosition(), mobj->GetRotationMatrix(), &minFace ) )
		{
			params.Pos = start - params.Dir * dist;
		}
	}
}

//------------------------------------------------------------------------

bool ProcessBulletHit( int &damageFromPiercable, GameObject* owner, const r3dPoint3D &hitPoint, const r3dPoint3D &hitNormal, GameObject* shootTarget, const r3dMaterial* shootMaterial, const char* hitActorName, const WeaponConfig *weaponInfo, const r3dPoint3D& muzzlerPosAtFireStart , const r3dPoint3D& FirePos) 
{	
	r3d_assert(owner);
	r3d_assert(owner->NetworkLocal==true);

	obj_Player* ownerPlayer = NULL;
	if(owner->isObjType(OBJTYPE_Human))
		ownerPlayer = (obj_Player*)owner;

	if( shootTarget == NULL && shootMaterial == NULL) // hit nothing
	{
		PKT_C2C_PlayerHitNothing_s n;
		p2pSendToHost(owner, &n, sizeof(n), true);
	} 
	else if (shootTarget && shootTarget->Class->Name == "obj_Animals")
	{
		SpawnImpactParticle(r3dHash::MakeHash("player"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
		if (ownerPlayer)
			ownerPlayer->AddBloodOnGround(hitPoint);

		//r3dOutToLog("Hit Animals\n");
		//obj_Animals* trgt = (obj_Animals*)shootTarget;

		//if(!trgt->bDead)
		//{
		PKT_C2C_PlayerHitDynamic_s n;
		n.muzzler_pos = muzzlerPosAtFireStart;
		n.hit_pos = hitPoint;
		n.targetId = toP2pNetId(shootTarget->GetNetworkID());
		n.hit_body_part = 0;
		n.hit_body_bone = 0;
		n.damageFromPiercing = damageFromPiercable;
		n.FirePos = FirePos;
		// NOTE: This can be broken if the player jumps after firing a sniper rifle, but shouldn't happen as it's too fast.
		n.state = 0;
		p2pSendToHost( owner, &n, sizeof(n), true);

		if(ownerPlayer) 
		{
			ownerPlayer->m_HitMarkerFadeout = 1.0f;
		}
		//}
	}
	else if(shootTarget && shootTarget->isObjType(OBJTYPE_Human)) // hit other player
	{
		r3d_assert(shootTarget->GetNetworkID());
		obj_Player* trgt = (obj_Player*)shootTarget;

		if(!trgt->bDead)
		{
			PKT_C2C_PlayerHitDynamic_s n;
			n.muzzler_pos = muzzlerPosAtFireStart;
			n.hit_pos = hitPoint;
			n.targetId = toP2pNetId(shootTarget->GetNetworkID());
			n.hit_body_part = trgt->getBodyNameFromBoneName(hitActorName);
			n.hit_body_bone = trgt->uberAnim_->GetBoneID(hitActorName);
			n.damageFromPiercing = damageFromPiercable;
			// NOTE: This can be broken if the player jumps after firing a sniper rifle, but shouldn't happen as it's too fast.
			n.state = 0;
			if(ownerPlayer)
				n.state |= (!ownerPlayer->bOnGround)?(0x1): 0 ; // [0]-in air 
			n.state |= (!trgt->bOnGround)?0x2:0 ; // [1]-in air
			n.FirePos = FirePos;
			p2pSendToHost( owner, &n, sizeof(n), true);

			if(!trgt->bDead && ownerPlayer) 
			{
				ownerPlayer->m_HitMarkerFadeout = 1.0f;
			}
		}
		else
		{
			PKT_C2C_PlayerHitNothing_s n;
			p2pSendToHost(owner, &n, sizeof(n), true);
		}

		if(ownerPlayer)
		{
			{
				ownerPlayer->AddBloodOnGround(hitPoint);
				SpawnImpactParticle(r3dHash::MakeHash("player"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
			}
		}
	}
	else if(shootTarget && shootTarget->isObjType(OBJTYPE_Zombie)) // hit zombie
	{
		r3d_assert(shootTarget->GetNetworkID());

		PKT_C2C_PlayerHitDynamic_s n;
		n.muzzler_pos = muzzlerPosAtFireStart;
		n.hit_pos  = hitPoint;
		n.targetId = toP2pNetId(shootTarget->GetNetworkID());
		n.hit_body_part = strcmp("Bip01_Head", hitActorName) == 0 ? 1 : 0;
		if(ownerPlayer)
			n.hit_body_bone = ownerPlayer->uberAnim_->GetBoneID(hitActorName); // zombies use same skeleton as player
		else
			n.hit_body_bone = 0xFF;

		n.damageFromPiercing = damageFromPiercable;
		n.state = 0;

		n.FirePos = FirePos;

		p2pSendToHost(owner, &n, sizeof(n), true);

		if(owner)
		{
			obj_Zombie* z = (obj_Zombie*)shootTarget;

			// apply ragdoll force from object position, or up if too close
			r3dPoint3D zragHitPos = z->GetPosition();
			r3dPoint3D zlastTimeHitForce = r3dPoint3D(0, 1, 0);
			if(ownerPlayer)
			{
				if(n.hit_body_bone < ownerPlayer->GetSkeleton()->NumBones) // zombies use same skeleton as player
					zragHitPos = ownerPlayer->uberEquip_->getBonePos(n.hit_body_bone, ownerPlayer->GetSkeleton(), z->GetTransformMatrix());
				if((zragHitPos - ownerPlayer->GetPosition()).LengthSq() > 0.001f)
					zlastTimeHitForce = (zragHitPos - ownerPlayer->GetPosition()).NormalizeTo();
			}

			z->lastTimeHitPos  = zlastTimeHitForce;//ownerPlayer->GetPosition();
			z->lastTimeDmgType = weaponInfo->category;
			z->lastTimeHitBoneID = n.hit_body_bone;
			z->PlayHurtSound();
		}
		if(ownerPlayer)
		{
			{
				ownerPlayer->AddBloodOnGround(hitPoint);
				SpawnImpactParticle(r3dHash::MakeHash("zombie"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
			}
		}
	}
#ifndef FINAL_BUILD
	else if(shootTarget && shootTarget->Class->Name == "obj_ZombieDummy")
	{
		obj_ZombieDummy* trgt = (obj_ZombieDummy*)shootTarget;

		r3dPoint3D hitForce = (hitPoint - ownerPlayer->GetPosition()).NormalizeTo();
		int hit_body_bone = trgt->anim_.GetCurrentSkeleton()->GetBoneID(hitActorName);

		if(strcmp("Bip01_Head", hitActorName) == 0)
		{
			r3dPoint3D hitForce_amp = hitForce * 30;
			trgt->physSkeleton->SwitchToRagdollWithForce(true, hit_body_bone, &hitForce_amp);
		}
		else
		{
			trgt->DoHit();
		}

		if(ownerPlayer)
		{
			ownerPlayer->AddBloodOnGround(hitPoint);
			SpawnImpactParticle(r3dHash::MakeHash("player"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
		}
	}
#endif	
	else if(shootTarget && shootTarget->GetNetworkID() && canDamageTarget(shootTarget))
	{
		// hit something that have network Id
		r3d_assert(shootTarget->GetNetworkID());
		PKT_C2C_PlayerHitDynamic_s n;
		n.muzzler_pos = muzzlerPosAtFireStart;		
		n.hit_pos = hitPoint;
		n.targetId = toP2pNetId(shootTarget->GetNetworkID());
		n.hit_body_bone = 0;
		n.hit_body_part = 0;
		n.state = 0;
		n.damageFromPiercing = damageFromPiercable; 
		n.FirePos = FirePos;
		p2pSendToHost( owner, &n, sizeof(n), true);

		//TODO: add decals
		SpawnImpactParticle(r3dHash::MakeHash("Metal"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
	} 
	else 
	{
		DecalParams params;
		params.Dir	= hitNormal;
		params.Pos	= hitPoint;
		params.TypeID	= INVALID_DECAL_ID;

		AdjustHitPosition( params, shootTarget );

		uint32_t sourceHash = r3dHash::MakeHash( weaponInfo->m_PrimaryAmmo->getDecalSource());
		uint32_t matHash    = 0;
		int      particleIdx= -1;

		r3dPoint3D waterSplashPos;
		extern bool TraceWater(const r3dPoint3D& start, const r3dPoint3D& finish, r3dPoint3D& waterSplashPos);
		if( TraceWater( r3dPoint3D(gCam.x, gCam.y, gCam.z), hitPoint, waterSplashPos))
		{	
			matHash = r3dHash::MakeHash("Thirst");
			particleIdx = SpawnImpactParticle(matHash, sourceHash, waterSplashPos, r3dPoint3D(0,1,0));
			extern void WaterSplash(const r3dPoint3D& waterSplashPos, float height, float size, float amount, int texIdx);
			waterSplashPos.y -= 0.1f;
			WaterSplash(waterSplashPos, 0.1f, 30.0f, 0.04f, 0);
		}
		else
		{
			{
				if( shootTarget && shootTarget->isObjType(OBJTYPE_Terrain) )
				{
					params.TypeID	= Terrain->GetDecalID(params.Pos, sourceHash);
					particleIdx = SpawnImpactParticle(matHash, sourceHash, hitPoint, hitNormal);
				}
				else
				{
					if( shootMaterial )
					{
						matHash = r3dHash::MakeHash(shootMaterial->TypeName);
						params.TypeID	= GetDecalID(matHash, sourceHash);
						particleIdx = SpawnImpactParticle(matHash, sourceHash, hitPoint, hitNormal);
					}
				}

				if( params.TypeID != INVALID_DECAL_ID )
				{
					g_pDecalChief->Add( params );
				}

				// add impulse
				if(shootTarget && shootTarget->PhysicsObject)
				{
					shootTarget->PhysicsObject->AddImpulseAtPos(-hitNormal*weaponInfo->m_AmmoSpeed*weaponInfo->m_AmmoMass/150.0f, hitPoint);
				}
			}
		}
		int hitPierceableObject = 0;
		if(shootTarget && shootTarget->m_BulletPierceable > 0  && damageFromPiercable == 0 && weaponInfo->category!=storecat_MELEE ) // melee cannot pierce objects
		{
			hitPierceableObject = shootTarget->m_BulletPierceable;
		}

		if(hitPierceableObject)
		{
			PKT_C2C_PlayerHitStaticPierced_s n; 
			n.hit_pos  = hitPoint;
			n.hit_norm = hitNormal;
			n.decalIdx = (BYTE)(params.TypeID + 1);  // -1 is invalid decal, bring to 0-xx range
			n.particleIdx = (BYTE)(particleIdx + 1); // same here
			n.hash_obj = shootTarget?shootTarget->GetHashID():0;
			p2pSendToHost( owner, &n, sizeof(n));

			damageFromPiercable = shootTarget->m_BulletPierceable;

			return false;

		}
		else
		{
			PKT_C2C_PlayerHitStatic_s n;
			n.hit_pos  = hitPoint;
			n.hit_norm = hitNormal;
			n.decalIdx = (BYTE)(params.TypeID + 1);  // -1 is invalid decal, bring to 0-xx range
			n.particleIdx = (BYTE)(particleIdx + 1); // same here
			n.hash_obj = shootTarget?shootTarget->GetHashID():0;
			p2pSendToHost( owner, &n, sizeof(n));
		}
	}

	return true;
}
#endif

#ifdef WO_SERVER
#include "r3dpch.h"
#include "r3d.h"

#include "APIScaleformGfx.h"
#include "AI_Player.h"
#include "AI_PlayerAnim.h"

#include "r3dPhysSkeleton.h"

#include "ObjectsCode\Effects\obj_ParticleSystem.h"

#include "Particle.h"

#include "ObjectsCode/world/DecalChief.h"
#include "ObjectsCode/world/MaterialTypes.h"

#include "ObjectsCode/weapons/Weapon.h"

#include "ObjectsCode/weapons/Gear.h"
#include "ObjectsCode/WEAPONS/Ammo.h"
#include "ObjectsCode/weapons/WeaponArmory.h"
#include "ObjectsCode/weapons/ExplosionVisualController.h"

#include "ObjectsCode/Gameplay/obj_PlayerSpawnPoint.h"
#include "ObjectsCode/Gameplay/obj_DamageArea.h"
#include "ObjectsCode/Gameplay/obj_DroppedItem.h"
#include "ObjectsCode/Gameplay/obj_Note.h"
#include "ObjectsCode/Gameplay/obj_PostBox.h"
#include "../../GameEngine/gameobjects/obj_Vehicle.h"
#include "../Gameplay/obj_Zombie.h"
#include "../Gameplay/obj_ZombieDummy.h"
#include "ObjectsCode/Gameplay/NPCObject.h"

#include "ObjectsCode/WORLD/obj_LightMesh.h"

#include "multiplayer/ClientGameLogic.h"
#include "GameCode/UserProfile.h"
#include "GameCode/UserRewards.h"

#include "Gameplay_Params.h"

#include "CamouflageDataManager.h"

#include "../SF/CmdProcessor/CmdProcessor.h"
#include "../SF/CmdProcessor/CmdConsole.h"

#include "..\..\ui\HUDDisplay.h"
#include "..\..\ui\HUD_EditorGame.h"
#include "..\..\ui\FrontEndShared.h"
#include "..\..\ui\HUDPause.h"
#include "..\..\ui\HUDActionUI.h"
#include "..\..\ui\HUDAttachments.h"
#include "..\..\ui\HUDGeneralStore.h"
#include "..\..\ui\HUDVault.h"

extern HUDDisplay*	hudMain;
extern HUDPause*	hudPause;
extern HUDAttachments*	hudAttm;
extern HUDActionUI*	hudActionUI;
extern HUDGeneralStore*	hudGeneralStore;
extern HUDVault* hudVault;

extern 	PlayerStateVars_s CurrentRig;

extern bool g_bEditMode;

extern void TPSGameHud_UnlockAchievement( int achievementID );

extern float getWaterDepthAtPos(const r3dPoint3D& pos);

IMPLEMENT_CLASS(obj_Player, "obj_Player", "Object");
AUTOREGISTER_CLASS(obj_Player);

float		_ai_fTurnSpeedIdle      = 360;
float		_ai_fTurnSpeedWalk	= 720;
float		_ai_NetPosUpdateDelta = 1.0f / 10;	// "30"

bool		g_CameraInsidePlayer = false;

#include "../../rendering/Deffered/PFX_ScopeEffect.h"
extern int g_RenderScopeEffect;
extern PFX_ScopeEffect gPFX_ScopeEffect;

r3dSec_type<CUberData*, 0x345fdFCa> AI_Player_UberData = NULL;

r3dTexture* obj_Player::s_laserPointerTex = NULL;
r3dTexture* obj_Player::s_flashlightFlashTex = NULL;
r3dTexture* obj_Player::s_laserFlashTex = NULL;
r3dTexture* obj_Player::s_lightHaloTex = NULL;

r3dTexture* gPlayer_AuraTexture;
r3dTexture* gPlayer_CamoTexture;

gobjid_t m_LocalPlayer_CurrentAimAt;

#define TIME_FOR_SECOND_WIND 5.0f
#define KILLSTREAK_FOR_SECOND_WIND 3
static const float gDist_To_Allow_Vehicle_Entry = 5;
bool gDisableShortcuts = false;
struct PhysSkeletonCacheEntry
{
	int Taken ;
	r3dPhysSkeleton* Skel ;
};

typedef r3dTL::TArray< PhysSkeletonCacheEntry > PhysSkeletonCache ;

int g_bPhysSkeletonCacheInitialized ;
PhysSkeletonCache g_PhysSkeletonCache ;

// per shot
float SpreadIncrease[] = 
{
	0.15f, // ASR
	1.0f, // SNP
	0.5f, // SHTG
	0.45f, // MG
	0.0f, // SUPPORT
	0.5f, // HG
	0.1f, // SMG
	0.0f, // GRENADES
};
// per second
float SpreadDecrease[] = 
{
	4.0f, // ASR
	1.0f, // SNP
	4.0f, // SHTG
	2.0f, // MG
	4.0f, // SUPPORT
	1.0f, // HG
	4.0f, // SMG
	4.0f, // GRENADES
};
// percentage of weapon's spread
float SpreadMin[] = 
{
	.2f, // ASR
	1.0f, // SNP
	.25f, // SHTG
	.2f, // MG
	.1f, // SUPPORT
	.1f, // HG
	.15f, // SMG
	.1f, // GRENADES
};

void AddCachedPhysSkeleton()
{
	PhysSkeletonCacheEntry entry ;

	entry.Skel	= new r3dPhysSkeleton( "data/ObjectsDepot/Characters/RagDoll.RepX" ) ;
	entry.Taken	= 0 ;

	g_PhysSkeletonCache.PushBack( entry ) ;
}

void InitializePhysSkeletonCache( int count, float progressStart, float progressEnd )
{
	r3d_assert( !g_bPhysSkeletonCacheInitialized ) ;

	R3D_LOG_TIMESPAN_START(InitializeSkeletonCache);
	for( int i = 0, e = count ; i < e ; i ++ )
	{
		AddCachedPhysSkeleton() ;

		void SetLoadingProgress( float progress ) ;
		SetLoadingProgress( progressStart + ( progressEnd - progressStart ) * i / ( count - 1 ) ) ;
	}
	R3D_LOG_TIMESPAN_END(InitializeSkeletonCache);	

	g_bPhysSkeletonCacheInitialized = 1 ;
}

void DestroyPhysSkeletonCache()
{
	r3d_assert( g_bPhysSkeletonCacheInitialized ) ;

	for( int i = 0, e = g_PhysSkeletonCache.Count() ; i < e ; i ++ )
	{
		PhysSkeletonCacheEntry& entry = g_PhysSkeletonCache[ i ] ;
		delete entry.Skel ;
	}

	g_PhysSkeletonCache.Clear() ;

	g_bPhysSkeletonCacheInitialized = 0 ;
}

void ReleaseCachedPhysSkeleton( r3dPhysSkeleton* skel )
{
	if( skel )
	{
		for( int i = 0, e = g_PhysSkeletonCache.Count() ; i < e ; i ++ )
		{
			PhysSkeletonCacheEntry& entry = g_PhysSkeletonCache[ i ] ;
			if( entry.Skel == skel )
			{
				entry.Skel->unlink() ;
				entry.Skel->SwitchToRagdoll(false);
				entry.Taken = 0 ;
				return ;
			}
		}

		r3dError( "ReleaseCacheSkeleton: skeleton not found!" ) ;
	}
}

r3dPhysSkeleton* AquireCacheSkeleton()
{
	for( int i = 0, e = g_PhysSkeletonCache.Count() ; i < e ; i ++ )
	{
		PhysSkeletonCacheEntry& entry = g_PhysSkeletonCache[ i ] ;

		if( !entry.Taken )
		{
			entry.Taken = 1 ;
			return entry.Skel ;
		}
	}

	AddCachedPhysSkeleton() ;

	PhysSkeletonCacheEntry& entry = g_PhysSkeletonCache.GetLast() ;

	entry.Taken = 1 ;

	return entry.Skel ;
}


struct PhysObstacleCacheEntry
{
	int Taken;
	ObstacleHandle Handle;
};
PhysObstacleCacheEntry*	g_PhysObstacleCache = NULL;
int		g_PhysObstacleSize  = 0;

void InitializePhysObstacleCache(int count)
{
	r3d_assert(g_PhysObstacleCache == NULL) ;

	g_PhysObstacleCache = new PhysObstacleCacheEntry[count];
	g_PhysObstacleSize  = count;

	for(int i=0; i<count; i++)
	{
		PxBoxObstacle obstacle;
		obstacle.mHalfExtents = PxVec3(0.25f, 1.0f, 0.25f);
		obstacle.mPos = PxExtendedVec3(0, 9000 + (float)i * 10, 0); // teleport way up
		g_PhysObstacleCache[i].Handle = g_pPhysicsWorld->m_PlayerObstaclesManager->addObstacle(obstacle);
		g_PhysObstacleCache[i].Taken  = 0;
	}
}

void DestroyPhysObstacleCache()
{
	r3d_assert(g_PhysObstacleCache);

	// destroy in reverse order 
	for(int i=g_PhysObstacleSize-1; i>=0; i--)
	{
		g_pPhysicsWorld->m_PlayerObstaclesManager->removeObstacle(g_PhysObstacleCache[i].Handle);
		g_PhysObstacleCache[i].Handle = 0;
		g_PhysObstacleCache[i].Taken  = 0;
	}

	SAFE_DELETE_ARRAY(g_PhysObstacleCache);
	g_PhysObstacleSize = 0;
}

int AcquirePlayerObstacle(const r3dPoint3D& pos)
{
	r3d_assert(g_PhysObstacleCache);

	for(int i=0; i<g_PhysObstacleSize; i++)
	{
		if(g_PhysObstacleCache[i].Taken)
			continue;

		g_PhysObstacleCache[i].Taken = 1;
		UpdatePlayerObstacle(i, pos);
		return i;
	}

	return -1;
}

void ReleasePlayerObstacle(int* out_idx)
{
	int idx = *out_idx;
	if(idx == -1)
		return;

	r3d_assert(idx >= 0 && idx < g_PhysObstacleSize);
	r3d_assert(g_PhysObstacleCache[idx].Taken);

	UpdatePlayerObstacle(idx, r3dPoint3D(0, 9000 + (float)idx * 10, 0)); // teleport way up
	g_PhysObstacleCache[idx].Taken = 0;
	*out_idx = -1;
}

void UpdatePlayerObstacle(int idx, const r3dPoint3D& pos)
{
	if(idx == -1)
		return;

	R3DPROFILE_FUNCTION("UpdateObstacle");

	r3d_assert(idx >= 0 && idx < g_PhysObstacleSize);
	r3d_assert(g_PhysObstacleCache[idx].Taken);

	PxBoxObstacle obstacle;
	obstacle.mHalfExtents = PxVec3(0.25f, 1.0f, 0.25f);
	obstacle.mPos = PxExtendedVec3(pos.x, pos.y + 1.0f, pos.z);
	g_pPhysicsWorld->m_PlayerObstaclesManager->updateObstacle(g_PhysObstacleCache[idx].Handle, obstacle);

	return;
}

static void AdjustHitPosition(DecalParams& params, GameObject* target);

void AI_Player_FreeStuff()
{
	SAFE_DELETE(AI_Player_UberData);

	if(gPlayer_AuraTexture)
	{
		r3dRenderer->DeleteTexture(gPlayer_AuraTexture);

		gPlayer_AuraTexture = 0;
	}

	if(gPlayer_CamoTexture)
	{
		r3dRenderer->DeleteTexture(gPlayer_CamoTexture);

		gPlayer_CamoTexture = 0;
	}
}

#ifndef FINAL_BUILD
DECLARE_CMD(binv)
{
	if(ev.NumArgs() != 1)
	{
		ConPrint("binv");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	ConPrint("Backpack id:%d, size:%d\n", slot.BackpackID, slot.BackpackSize);
	ConPrint("Inventory List:\n");
	for(int i=0; i<slot.BackpackSize; i++)
	{
		wiInventoryItem& wi = slot.Items[i];
		if(wi.itemID == 0)
			continue;
		const BaseItemConfig* cfg = g_pWeaponArmory->getConfig(wi.itemID);
		ConPrint("%02d: ItemID:%d, q:%d, Var1:%d Var2:%d %s", i, wi.itemID, wi.quantity, wi.Var1, wi.Var2, cfg->m_StoreName);
	}

	return;
}

DECLARE_CMD(pickup)
{
	obj_Player* plr = gClientLogic().localPlayer_;

	GameObject* closest = NULL;
	float       minDist = 99999999.0f;
	for(GameObject* obj = GameWorld().GetFirstObject(); obj; obj = GameWorld().GetNextObject(obj))
	{
		if(obj->Class->Name == "obj_DroppedItem" || obj->Class->Name == "obj_Note")
		{
			float dist = (plr->GetPosition() - obj->GetPosition()).Length();
			if(dist < minDist) 
			{
				closest = obj;
				minDist = dist;
			}
		}
	}

	if(!closest) {
		ConPrint("no closest items");
		return;
	}

	ConPrint("using network obj %d\n", closest->GetNetworkID());

	PKT_C2S_UseNetObject_s n;
	n.spawnID = toP2pNetId(closest->GetNetworkID());
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
}

DECLARE_CMD(giveitem)
{
	if(ev.NumArgs() != 3)
	{
		ConPrint("giveitem {itemid} {num}");
		return;
	}

	int num = ev.GetInteger(2);
	for(int i=0; i<num; i++)
	{
		PKT_C2S_Admin_GiveItem_s n;
		n.ItemID = ev.GetInteger(1);
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
	}
}


DECLARE_CMD(changebp)
{
	if(ev.NumArgs() != 2)
	{
		ConPrint("changebp {slot}");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	int slotFrom = ev.GetInteger(1);

	const BackpackConfig* cfg = g_pWeaponArmory->getBackpackConfig(slot.Items[slotFrom].itemID);
	if(!cfg) {
		ConPrint("bad backpack itemid");
		return;
	}

	plr->ChangeBackpack(slotFrom);
}

DECLARE_CMD(alist)
{
	if(ev.NumArgs() != 1)
	{
		ConPrint("alist {slot}");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	int wid      = 0; // test - by default modify weapon 0
	Weapon* wpn = plr->m_Weapons[wid];
	if(!wpn) {
		ConPrint("no weapon");
		return;
	}

	ConPrint("Weapon: %s\n", wpn->getConfig()->m_StoreName);
	for(int i=0; i<WPN_ATTM_MAX; i++)
	{
		ConPrint(" Slot: %d, cur %d, def %d\n", i, slot.Attachment[wid].attachments[i], wpn->getConfig()->FPSDefaultID[i]);
		if(!wpn->getConfig()->FPSSpecID[i])
			continue;

		char buf[2084] = "  ";
		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const WeaponAttachmentConfig* config = g_pWeaponArmory->getAttachmentConfig(itemID);
			if(!config) continue;
			if(config->m_specID == wpn->getConfig()->FPSSpecID[i])
				sprintf(buf + strlen(buf), ", %d %s", config->m_itemID, config->m_StoreName);
			if(config->m_itemID == wpn->getConfig()->FPSDefaultID[i])
				sprintf(buf + strlen(buf), ", (def) %d %s", config->m_itemID, config->m_StoreName);
		}
		ConPrint(buf);
	}
}

DECLARE_CMD(aequip)
{
	if(ev.NumArgs() != 2)
	{
		ConPrint("attmequip {slot}");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	int wid      = 0; // test - by default modify weapon 0
	int slotFrom = ev.GetInteger(1);

	const WeaponAttachmentConfig* cfg = g_pWeaponArmory->getAttachmentConfig(slot.Items[slotFrom].itemID);
	if(!cfg) {
		ConPrint("bad attachment itemid");
		return;
	}

	Weapon* wpn = plr->m_Weapons[wid];
	if(!wpn) {
		ConPrint("no weapon");
		return;
	}

	plr->EquipWpnAttm(wid, slotFrom);
}

DECLARE_CMD(aremove)
{
	if(ev.NumArgs() != 2)
	{
		ConPrint("aremove {weapon attm type}");
		return;
	}


	int wid      = 0; // test - by default modify weapon 0
	int wpnAttmType = ev.GetInteger(1);
	if(wpnAttmType >= WPN_ATTM_MAX)  {
		ConPrint("bad wpnAttmType");
		return;
	}

	obj_Player* plr = gClientLogic().localPlayer_;
	plr->RemoveWpnAttm(0, wpnAttmType);
}
#endif

bool obj_Player::ChangeBackpack(int slotFrom)
{
	wiCharDataFull& slot = CurLoadout;

	const BackpackConfig* cfg = g_pWeaponArmory->getBackpackConfig(slot.Items[slotFrom].itemID);
	if(!cfg) {
		return false;
	}

	PKT_C2S_PlayerChangeBackpack_s n;
	n.SlotFrom     = slotFrom;
	n.BackpackSize = cfg->m_maxSlots;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	// replace backpack in used slot with current one (SERVER CODE SYNC POINT)
	slot.Items[slotFrom].itemID = slot.BackpackID;

	// change it
	slot.BackpackID   = cfg->m_itemID;
	slot.BackpackSize = cfg->m_maxSlots;
	UpdateLoadoutSlot(slot);

	return true;
}

void obj_Player::DropItem(int slotFrom)
{
	PKT_C2S_BackpackDrop_s n;
	n.SlotFrom = slotFrom;
	n.pos      = GetPosition() + GetvForw()*0.1f;
	p2pSendToHost(this, &n, sizeof(n));

	int itemID = CurLoadout.Items[slotFrom].itemID;

	//local logic
	CurLoadout.Items[slotFrom].quantity--;
	if(CurLoadout.Items[slotFrom].quantity <= 0) {
		CurLoadout.Items[slotFrom].Reset();
	}

	// fucked up attachment design!
	const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(itemID);
	if(wac)
	{
		int quantity = CurLoadout.Items[slotFrom].quantity; // if attm is droppen, then here will be zero
		for(int i=0; i<CurLoadout.BackpackSize; ++i)
		{
			if(CurLoadout.Items[i].itemID == itemID)
				quantity += CurLoadout.Items[i].quantity;
		}
		if(quantity == 0) // check if that attm is equipped and if yes, remove it from weapon
		{
			uint32_t curAttm[WPN_ATTM_MAX] = {0};
			if(m_Weapons[0])
			{
				m_Weapons[0]->getCurrentAttachmentIDs(curAttm);
				if(curAttm[wac->m_type] == itemID)
					RemoveWpnAttm(0, wac->m_type);
			}
			if(m_Weapons[1])
			{
				m_Weapons[1]->getCurrentAttachmentIDs(curAttm);
				if(curAttm[wac->m_type] == itemID)
					RemoveWpnAttm(1, wac->m_type);
			}			
		}
	}

	OnBackpackChanged(slotFrom);

	if(NetworkLocal)
	{
		const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(itemID);
		if(wc)
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_DROPGUN"), GetPosition());
		else
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_DROPITEM"), GetPosition());
	}
}

void obj_Player::RemoveWpnAttm(int wid, int attmType)
{
	wiCharDataFull& slot = CurLoadout;

	if(attmType >= WPN_ATTM_MAX)  {
		return;
	}

	Weapon* wpn = m_Weapons[wid];
	if(!wpn) {
		return;
	}

	PKT_C2S_PlayerRemoveAttachment_s n;
	n.wid         = wid;
	n.WpnAttmType = attmType;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	// set wpn attachment
	slot.Attachment[wid].attachments[attmType] = 0;
	wpn->setWeaponAttachmentsByIDs(slot.Attachment[wid].attachments);
}

void obj_Player::EquipWpnAttm(int wid, int backpackSlot)
{
	wiCharDataFull& slot = CurLoadout;

	int slotFrom = backpackSlot;

	const WeaponAttachmentConfig* cfg = g_pWeaponArmory->getAttachmentConfig(slot.Items[slotFrom].itemID);
	if(!cfg) {
		return;
	}

	Weapon* wpn = m_Weapons[wid];
	if(!wpn) {
		return;
	}

	PKT_C2S_PlayerEquipAttachment_s n;
	n.wid          = wid;
	n.AttmSlot     = slotFrom;
	n.dbg_AttmID   = cfg->m_itemID;
	n.dbg_WeaponID = wpn->getConfig()->m_itemID;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	// set wpn attachment
	slot.Attachment[wid].attachments[cfg->m_type] = cfg->m_itemID;
	wpn->setWeaponAttachmentsByIDs(slot.Attachment[wid].attachments);

	// we're changing clip, reload weapon
	if(cfg->m_type == WPN_ATTM_CLIP)
	{
		wpn->Reload();
	}
}


void _player_AdjustBoneCallback(DWORD dwData, int boneId, D3DXMATRIX &mp, D3DXMATRIX &anim)
{
	obj_Player* npc = (obj_Player*)dwData;
	if (!npc)
		return;

	if(npc->m_PhysSkeleton && npc->m_PhysSkeleton->IsRagdollMode())
		return;

	if(g_camera_mode->GetInt()==2 && npc->NetworkLocal) // FPS bone adjustments
	{
	}
	else // TPS bone adjustments
	{
		if(boneId == npc->boneId_Bip01_Spine1 || boneId == npc->boneId_Bip01_Neck) 
		{
			float xRot = npc->bodyAdjust_x;
			float yRot = npc->bodyAdjust_y[0];

			// add recoil for spine1
			if(boneId == npc->boneId_Bip01_Spine1)
			{
				if(!(npc->NetworkLocal && npc->hasScopeMode() && CurrentRig.allowScope)) // don't add recoil in scope mode to a weapon, as it messes up muzzle flash rendering
				{
					float recoil = npc->RecoilViewMod.y*0.5f;
					recoil = R3D_MIN(recoil, 0.0f);

					yRot += recoil;
				}
			}

			D3DXMATRIX r1, r2;

			// rotate previous matrix, with saved position
			r3dPoint3D vv = r3dPoint3D(mp._41, mp._42, mp._43);
			D3DXMatrixRotationY(&r2, xRot/2);
			D3DXMatrixRotationX(&r1, -yRot/2);
			mp = mp * (r1 * r2);
			mp._41 = vv.x;
			mp._42 = vv.y;
			mp._43 = vv.z;
		}
		else if(boneId == npc->boneId_Bip01_LUpperArm || boneId == npc->boneId_Bip01_RUpperArm)
		{
			float xRot = npc->RecoilViewMod.x*0.1f;
			float yRot = npc->RecoilViewMod.y*0.5f;

			xRot = 0;//-R3D_MIN(xRot, 0.5f); 
			yRot = R3D_MIN(yRot, 0.1f);

			D3DXMATRIX r1, r2;

			// rotate previous matrix, with saved position
			r3dPoint3D vv = r3dPoint3D(mp._41, mp._42, mp._43);
			D3DXMatrixRotationY(&r2, xRot/2);
			D3DXMatrixRotationAxis(&r1, npc->GetvRightDraw().d3dx(), -yRot/2);
			mp = mp * (r1 * r2);
			mp._41 = vv.x;
			mp._42 = vv.y;
			mp._43 = vv.z;
		}
	}

	return;    
}

void obj_Player::UpdateLoadoutSlot(const wiCharDataFull& slot)
{
	R3DPROFILE_FUNCTION("obj_Player::UpdateLoadoutSlot");

	CurLoadout = slot; // save loadout
	m_GearWeight = 0;
	m_isFemaleHero = CurLoadout.HeroItemID==20184;

	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; ++i)
		SAFE_DELETE(m_Weapons[i]);

	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;

	// create weapons & items
	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; i++)
	{
		int ItemID = slot.Items[i].itemID;
		if(ItemID == 0)
			continue;

		const WeaponConfig* wcfg = g_pWeaponArmory->getWeaponConfig(ItemID);
		if(wcfg == NULL) {
			//r3dOutToLog("!!!! there is no weapon id %d\n", ItemID);
			continue;
		}

		m_Weapons[i] = new Weapon(this, i, wcfg, isFirstPerson, true, i <= wiCharDataFull::CHAR_LOADOUT_WEAPON2 ? &CurLoadout.Attachment[i] : NULL);
		r3d_assert(m_Weapons[i]); // to check for out of memory
	}

	if(NetworkLocal)
	{
		updateWeaponUI();
	}

	uberEquip_->SetSlot(SLOT_Weapon, m_Weapons[m_SelectedWeapon]);
	uberEquip_->SetSlot(SLOT_WeaponBackRight, (Weapon*)NULL);
	uberEquip_->SetSlot(SLOT_WeaponSide, (Weapon*)NULL);

	if(m_Weapons[m_SelectedWeapon] && !forcedEmptyHands)
		m_Weapons[m_SelectedWeapon]->OnEquip();

	if(m_Weapons[1] && m_SelectedWeapon != 1)
		uberEquip_->SetSlot(SLOT_WeaponSide, m_Weapons[1]);

	const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(CurLoadout.HeroItemID);
	loadHeroMeshes(heroConfig);
	if(heroConfig)
		m_GearWeight += heroConfig->m_Weight;

	const BackpackConfig* backpack = g_pWeaponArmory->getBackpackConfig(CurLoadout.BackpackID);
	if(backpack)
		uberEquip_->SetSlot(SLOT_Backpack, backpack->getMesh());
	else
		uberEquip_->SetSlot(SLOT_Backpack, (r3dMesh*)NULL);

	uint32_t SlotArmorID    = slot.Items[wiCharDataFull::CHAR_LOADOUT_ARMOR].itemID;
	uint32_t SlotHeadGearID = slot.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID;
	if(SlotArmorID>0)
	{
		uberEquip_->SetSlot(SLOT_Armor, g_pWeaponArmory->createGear(SlotArmorID));

		if(uberEquip_->slots_[SLOT_Armor].gear)
			m_GearWeight += uberEquip_->slots_[SLOT_Armor].gear->GetWeight();
	}
	else
		uberEquip_->SetSlot(SLOT_Armor, (Gear*)NULL);

	if(SlotHeadGearID>0)
	{
		uberEquip_->SetSlot(SLOT_Helmet, g_pWeaponArmory->createGear(SlotHeadGearID));

		if(uberEquip_->slots_[SLOT_Helmet].gear)
			m_GearWeight += uberEquip_->slots_[SLOT_Helmet].gear->GetWeight();
	}
	else
		uberEquip_->SetSlot(SLOT_Helmet, (Gear*)NULL);

	// start loading meshes
	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; ++i)
	{
		if(m_Weapons[i])
			m_Weapons[i]->getModel(true, isFirstPerson);
	}
	for(int i=0; i<SLOT_Max; ++i)
	{
		if(uberEquip_->slots_[i].gear)
			uberEquip_->slots_[i].gear->getModel(isFirstPerson);
	}

	uberAnim_->CurrentWeapon = NULL;
	SyncAnimation(true);
}

void obj_Player::loadHeroMeshes(const HeroConfig* heroConfig)
{
	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	if(heroConfig)
	{
		uberEquip_->SetSlot(SLOT_UpperBody, heroConfig->getBodyMesh(CurLoadout.BodyIdx, isFirstPerson));
		uberEquip_->SetSlot(SLOT_LowerBody, heroConfig->getLegMesh(CurLoadout.LegsIdx));
		uberEquip_->SetSlot(SLOT_Head, heroConfig->getHeadMesh(CurLoadout.HeadIdx));
	}
	else
	{
		uberEquip_->SetSlot(SLOT_UpperBody, (r3dMesh*)NULL);
		uberEquip_->SetSlot(SLOT_LowerBody, (r3dMesh*)NULL);
		uberEquip_->SetSlot(SLOT_Head, (r3dMesh*)NULL);
	}
}

obj_Player::obj_Player()
: footStepsSnd(0)
, currentFootStepsSoundID(-1)
, boneId_Bip01_Spine1(-1)
, boneId_Bip01_Neck(-1)
, boneId_Bip01_LUpperArm(-1)
, boneId_Bip01_RUpperArm(-1)
, boneId_Bip01_Head(-1)
, boneId_Bip01_R_Hand(-1)
, boneId_Bip01_L_Hand(-1)
, m_AuraType(AT_NONE)
, netMover(this, _ai_NetPosUpdateDelta, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
, m_enableRendering(true)
, lifeProperties(this)
, m_ItemSimulateLeftClick(false)
, showSlotsInfoTimer(0.0f)
, forcedEmptyHands(false)
, m_sndBreathSprint(0)
, m_sndInPain(0)
, m_sndClothMovementSprint(0)
, m_sndBreathBaseVolume(1)
, m_BarricadeMeshPlacement(NULL)
, m_isFemaleHero(false)
, m_sndSniperBreath(0)
, m_sndSniperHeartSlow(0)
, m_sndSniperExhale(0)
{
	uberEquip_ = NULL;
	m_PhysSkeleton = NULL;

	m_disablePhysSkeleton = false;

	PhysXObstacleIndex = -1;

	lastTimeHit = 0;
	bOnGround =1;
	fHeightAboveGround = 0.0f;
	StartFallingHeight = 0;
	StartFallingTime = 0;
	bCrouch = 0;
	bProne = 0;
	TimeOfDeath = 0;
	PosOfDeath.Assign(0,0,0);
	DisableKillerView = false;
	Dead_KillerID = 0;
	m_BloodTimer = 0;
	m_GearWeight = 0;
	CustomerID     = 0;
	ClanID         = 0;
	ClanTagColor   = 0xFFFFFFFF;
	r3dscpy(ClanTag, "CLAN");

	viewTargetPos		= r3dPoint3D(0, 0, 0);
	viewTargetNorm		= r3dPoint3D(0, 1, 0);

	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; ++i)
		m_Weapons[i] = NULL;
	m_SelectedWeapon = 0;
	m_PrevSelectedWeapon = -1;

	rewards_.reserve(32);
	lastRewardShowTime_ = 0;


#if VEHICLES_ENABLED
	vehicleViewActive_ = VehicleView_None; 
#endif

#ifndef FINAL_BUILD
	static bool cmd_registered = false;
	if(!cmd_registered)
	{
		cmd_registered = true;
		REG_CCOMMAND(binv, 0, "show backpack");
		REG_CCOMMAND(pickup, 0, "use closest network object");
		REG_CCOMMAND(giveitem, 0, "give item to player");
		REG_CCOMMAND(changebp, 0, "change backpack");
		REG_CCOMMAND(alist, 0, "list attachments");
		REG_CCOMMAND(aequip, 0, "equip attachment");
		REG_CCOMMAND(aremove, 0, "remove attachment");
	}
#endif

	if(g_pDecalChief && g_pDecalChief->GetTypeCount() > 254)
	{
		r3dError("more that 254 decals"); 
	}
	if(ImpactParticleEntry::NumRegistered > 254)
	{
		r3dError("more that 254 impact particles");
	}

	// create animation data first time player is created
	if(!AI_Player_UberData)
	{
		AI_Player_UberData = new CUberData();
	}

	if(!gPlayer_AuraTexture)
	{
		gPlayer_AuraTexture = r3dRenderer->LoadTexture("Data/Shaders/TEXTURE/Aura.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
	}

	if(!gPlayer_CamoTexture)
	{
		gPlayer_CamoTexture = r3dRenderer->LoadTexture("Data/Shaders/TEXTURE/plastic_nm.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
	}
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_Player::Load(const char *fname)
{
	if(!parent::Load(fname)) return FALSE;

	bPersistent 	= 0;

	ObjTypeFlags |= OBJTYPE_Human;

	Height 	 	= 1.8f;
	m_EncryptedUserName.set("---");

	lastDamageRcvd = 0;
	lastTimeHitForce  = r3dPoint3D(1, 0, 0);
	lastTimeHitBone = 0;
	bDead                 = 0;
	BloodEffect = 0;
	m_ReticleCurrentScale = 1.0f;
	m_ReticleTargetScale = 1.0f;
	m_isAiming = false;
	m_isChangedAiming1 = false;
	m_isPressedFireTrigger = false;
	m_isFinishedAiming = false;
	m_isInScope = false;

	grenadeReadyTime = 0;

	m_OrigBBox.Org.Assign( -0.5, 0, -0.5 );
	m_OrigBBox.Size.Assign( 1, 2, 1 );

	SetBBoxLocal( m_OrigBBox ) ;

	m_vVision.Assign(1, 0, 0);
	ViewAngle.Assign(0, 0, 0);
	RecoilViewMod.Assign(0,0,0);
	RecoilViewModTarget.Assign(0,0,0);
	RecoilViewModTarget2.Assign(0,0,0);
	SniperViewMod.Assign(0,0,0);
	RecoilCooldown = 0;
	m_fPlayerRotation      = 0;
	m_fPlayerRotationTarget     = 0;
	//UpdateRotation();

	D3DXMatrixIdentity(&DrawRotMatrix);
	D3DXMatrixIdentity(&DrawFullMatrix);
	D3DXMatrixIdentity(&DrawFullMatrix_Localized);
	D3DXMatrixIdentity(&MoveMatrix);

	JumpVelocity = 0;
	m_StaminaPenaltyTime = 0;
	//m_BreathPenaltyTime = 0;

	m_LastKillTime = 0;
	m_RemainingSecondWindTime = 0.0f;
	m_isHoldingBreath = false;

	bodyAdjust_x    = 0;
	bodyAdjust_y[0] = 0;
	bodyAdjust_y[1] = 0;

	RealAcceleration    = r3dPoint3D(0, 0, 0);
	InputAcceleration    = r3dPoint3D(0, 0, 0);
	SetVelocity(r3dPoint3D(0, 0, 0));

	PlayerState	= PLAYER_IDLE;
	PlayerMoveDir   = 0;
	// we pack both vars into single byte in PKT_C2C_MoveRel.state, so check for overflow
	//COMPILE_ASSERT(PLAYER_NUM_STATES <= 0xF);
	COMPILE_ASSERT(CUberData::ANIMDIR_COUNT <= 0xF);

	m_HitMarkerFadeout = 0;
	m_HitMarkerTex = NULL;

	UpdateTransform();
	return TRUE;
}

BOOL obj_Player::OnCreate()
{
	R3DPROFILE_FUNCTION("obj_Player::OnCreate");

	R3D_LOG_TIMESPAN_START(Ai_Player_OnCreate);

	parent::OnCreate();

	r3d_assert(PhysXObstacleIndex == -1);
	if(!NetworkLocal)
	{
		// create obstacle for network players, so we can't bump to them.
		PhysXObstacleIndex = AcquirePlayerObstacle(GetPosition());
	}

	bAllowToUseWeapons	= 1;
	afterRespawnTimer = 0;
	lastTimeWeaponSwitch = 0;
	TimeOfLastRespawn = r3dGetTime();
	m_AuraType = AT_NONE;
	footsteps_timeSinceLastMissingHitResult = 0;

	m_needToDrawBarricadePlacement = 0;
	m_BarricadeMeshPlacement = NULL;
	m_canPlaceConsumableItem = false;

	m_SpeedBoost = 0;
	m_SpeedBoostTime = 0;
	m_MorhineShotTime =0;
	m_BandagesEffectTime = 0;

	CurrentSpread = 0;

	if(NetworkLocal)
	{
		// otherwise when you are in scope mode you might not see your own player, and so scope will not render. Anyway, we should see our player always
		setSkipOcclusionCheck(true);
		ObjFlags |=	OBJFLAG_ForceSceneBoxBBox | OBJFLAG_AlwaysDraw; 
		PhysicsConfig.group = PHYSCOLL_CHARACTERCONTROLLER;
		PhysicsConfig.type = PHYSICS_TYPE_CONTROLLER;
		PhysicsConfig.mass = 100.0f;
		PhysicsConfig.ready = true;
		PhysicsObject = BasePhysicsObject::CreateCharacterController(PhysicsConfig, this);

		if(s_laserPointerTex == 0)
		{
			s_laserPointerTex = r3dRenderer->LoadTexture("data/shaders/texture/laserPointerDot.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem );
			r3d_assert(s_laserPointerTex!=NULL);
		}

		if(s_flashlightFlashTex == 0)
		{
			s_flashlightFlashTex = r3dRenderer->LoadTexture("data/shaders/texture/Light_Flashlight.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
			r3d_assert(s_flashlightFlashTex!=NULL);
		}

		if(s_laserFlashTex == 0)
		{
			s_laserFlashTex = r3dRenderer->LoadTexture("data/shaders/texture/Light_Laser.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
			r3d_assert(s_laserFlashTex!=NULL);
		}

		if(s_lightHaloTex == 0)
		{
			s_lightHaloTex = r3dRenderer->LoadTexture("data/shaders/texture/Light_Halo_01.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem);
			r3d_assert(s_lightHaloTex!=NULL);
		}

		m_HitMarkerTex = r3dRenderer->LoadTexture("data/weapons/hitmarker.dds", D3DFMT_UNKNOWN, false, 1, 1, D3DPOOL_MANAGED, PlayerTexMem );
		r3d_assert(m_HitMarkerTex != NULL);
	}
	else
	{
		PhysicsConfig.group = PHYSCOLL_CHARACTERCONTROLLER;
		PhysicsConfig.type = PHYSICS_TYPE_RAYCAST_BOX;
		PhysicsConfig.mass = 100.0f;
		// NOTE : if you change this, sync with PHYSICS_TYPE_CONTROLLER offset...
		PhysicsConfig.offset = r3dPoint3D( 0, 0.9f, 0 );
		PhysicsObject = BasePhysicsObject::CreateDynamicObject(PhysicsConfig, this);
		//wchar_t tempStr[64];
		//char plrUserName[64]; GetUserName(plrUserName);
		//swprintf(tempStr, 64, gLangMngr.getString("$HUD_Msg_JoinedGame"), utf8ToWide(plrUserName));
		//if(hudMain)
		//	hudMain->showMessage(tempStr);
	}

	m_Stamina = getMaxStamina();
	m_Breath = getMaxBreath();

	// init animation
	uberEquip_ = new CUberEquip(this);
	r3d_assert(AI_Player_UberData);
	uberAnim_ = new CUberAnim(this, AI_Player_UberData);

	boneId_Bip01_Spine1    = uberAnim_->GetBoneID("Bip01_Spine1");
	boneId_Bip01_Spine2    = uberAnim_->GetBoneID("Bip01_Spine2");
	boneId_Bip01_Neck      = uberAnim_->GetBoneID("Bip01_Neck");
	boneId_Bip01_LUpperArm = uberAnim_->GetBoneID("Bip01_L_UpperArm");
	boneId_Bip01_RUpperArm = uberAnim_->GetBoneID("Bip01_R_UpperArm");
	boneId_Bip01_Head      = uberAnim_->GetBoneID("Bip01_Head");
	boneId_Bip01_R_Hand	   = uberAnim_->GetBoneID("Bip01_R_Hand");
	boneId_Bip01_L_Hand	   = uberAnim_->GetBoneID("Bip01_L_Hand");

	UpdateLoadoutSlot(CurLoadout);

	PlayerState = PLAYER_IDLE;
	SyncAnimation(true);

	// force animation update here, so that phys skeleton can be updated, otherwise creating too many of them at 0,0,0 causing physX to crash (too many pairs created)
	{
		D3DXMATRIX mr;
		D3DXMatrixRotationY(&mr, R3D_PI);
		D3DXMATRIX CharDrawMatrix = mr;

		UpdateAnimations(CharDrawMatrix);
		UpdateRotation();
	}

	// create phys skeleton after we have animation data
	if(!m_disablePhysSkeleton)
	{
		m_PhysSkeleton = AquireCacheSkeleton() ;
		m_PhysSkeleton->linkParent(GetSkeleton(), DrawFullMatrix, this, NetworkLocal ? PHYSCOLL_LOCALPLAYER : PHYSCOLL_NETWORKPLAYER) ;
		m_PhysSkeleton->SwitchToRagdoll(false);
	}

	// if we was dead on spawn
	if(bDead) 
	{
		r3d_assert(!NetworkLocal);

		PlayerState = PLAYER_DIE;
		uberAnim_->AnimPlayerState = PLAYER_DIE;
		uberAnim_->StartDeathAnim();
		DoRagdoll(true);

		// fucking physx
		SetPosition(GetPosition());
	}

	// load local player sounds
	if(NetworkLocal)
	{
		m_sndBreathSprint = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_BREATHE_F_SLOW":"Sounds/WarZ/PlayerSounds/PLAYER_BREATHE_M_SLOW"), GetPosition(), true);
		SoundSys.Stop(m_sndBreathSprint);
		r3d_assert(m_sndBreathSprint);
		SoundSys.GetProperty(m_sndBreathSprint, FMOD_EVENTPROPERTY_VOLUME, &m_sndBreathBaseVolume);

		m_sndInPain = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_in_pain_F":"Sounds/WarZ/PlayerSounds/PLAYER_in_pain_M"), GetPosition(), true);
		SoundSys.Stop(m_sndInPain);
		r3d_assert(m_sndInPain);

		m_sndClothMovementSprint = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_PANTS_MOVE"), GetPosition(), true);
		SoundSys.Stop(m_sndClothMovementSprint);
		r3d_assert(m_sndClothMovementSprint);

		m_sndSniperBreath = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_BREATHE_F":"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_BREATHE_M"), GetPosition(), true);
		SoundSys.Stop(m_sndSniperBreath);
		r3d_assert(m_sndSniperBreath);
		m_sndSniperHeartSlow = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_HEART_SLOW"), GetPosition(), true);
		SoundSys.Stop(m_sndSniperHeartSlow);
		r3d_assert(m_sndSniperHeartSlow);
		m_sndSniperExhale = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_EXHALE_F":"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_EXHALE_M"), GetPosition(), true);
		SoundSys.Stop(m_sndSniperExhale);
		r3d_assert(m_sndSniperExhale);
	}

	lastWeapDataRep = r3dGetTime();

	TeleportPlayer(GetPosition(), "OnCreate");

	R3D_LOG_TIMESPAN_END(Ai_Player_OnCreate);

	return TRUE;
}

BOOL obj_Player::OnDestroy()
{
	//wchar_t tempStr[64];
	//char plrUserName[64]; GetUserName(plrUserName);
	//swprintf(tempStr, 64, gLangMngr.getString("$HUD_Msg_LeftGame"), utf8ToWide(plrUserName));
	//if(hudMain)
	//	hudMain->showMessage(tempStr);

	m_BarricadeMeshPlacement = NULL;

	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; ++i)
	{
		SAFE_DELETE(m_Weapons[i]);
	}

	ReleasePlayerObstacle(&PhysXObstacleIndex);	// player is gone, clear obstacle

	if(NetworkLocal)
	{
		if(gClientLogic().localPlayer_ == this)
			gClientLogic().localPlayer_ = 0;

		r3dRenderer->DeleteTexture(s_laserPointerTex);
		s_laserPointerTex = NULL;

		r3dRenderer->DeleteTexture(s_flashlightFlashTex);
		s_flashlightFlashTex = NULL;

		r3dRenderer->DeleteTexture(s_laserFlashTex);
		s_laserFlashTex = NULL;

		r3dRenderer->DeleteTexture(s_lightHaloTex);
		s_lightHaloTex = NULL;
	}

	SAFE_DELETE(uberAnim_);
	SAFE_DELETE(uberEquip_);

	ReleaseCachedPhysSkeleton( m_PhysSkeleton ) ;
	m_PhysSkeleton = 0 ;

	for(uint32_t i=0; i<footStepsArr.Count(); ++i) 
	{
		SoundSys.Stop(footStepsArr[i].snd);
		SoundSys.Release(footStepsArr[i].snd);
	}
	footStepsArr.Clear();

	if(NetworkLocal)
	{
		SoundSys.Stop(m_sndBreathSprint); SoundSys.Release(m_sndBreathSprint);
		SoundSys.Stop(m_sndInPain); SoundSys.Release(m_sndInPain);
		SoundSys.Stop(m_sndClothMovementSprint); SoundSys.Release(m_sndClothMovementSprint);

		SoundSys.Stop(m_sndSniperBreath); SoundSys.Release(m_sndSniperBreath);
		SoundSys.Stop(m_sndSniperHeartSlow); SoundSys.Release(m_sndSniperHeartSlow);
		SoundSys.Stop(m_sndSniperExhale); SoundSys.Release(m_sndSniperExhale);
	}

	if(hudMain)
	{
		if(!m_CharIcon.IsUndefined())
			hudMain->removeUserIcon(m_CharIcon);

	}

	return parent::OnDestroy();
}

float getMinimumAngleDistance(float from, float to)
{
	//make sure the angles are in range [0..360)
	const float da = 1.0f/360.0f;
	from = from - floorf(from * da)*360.0f;
	to = to - floorf(to * da)*360.0f;

	float d = to - from;
	if(d <-180.0f)	d += 360.0f;
	if(d > 180.0f)	d -= 360.0f;
	return d;
}

void Fix_Angle(float& deg)
{
	if(deg < -R3D_PI)
		deg += R3D_PI * 2;
	else if(deg >= R3D_PI)
		deg -= R3D_PI * 2;
}

void obj_Player::UpdateRotationToTarget(float fY, float fTurnSpeed)
{
	// if don't need to rotate
	if(R3D_ABS(fY) < 0.2f|| R3D_ABS(fY) > 120)
	{
		m_fPlayerRotation = m_fPlayerRotationTarget;
		uberAnim_->StopTurnInPlaceAnim();
		return;
	}

	float fTimePassed = r3dGetFrameTime();

	// turn character to the needed angle
	float step = fTurnSpeed * fTimePassed;

	m_fPlayerRotation += fY < 0.0f ? -step : +step;

	// check if we finished rotation, overrotated
	float fY2 = getMinimumAngleDistance(m_fPlayerRotation, m_fPlayerRotationTarget);
	if((fY > 0 && fY2 <= 0) || (fY < 0 && fY2 >= 0) 

		|| 
		// don't overreact - can get to any angle with this sort of step anyway (happens on low fps)
		step >= 180.f )
	{
		m_fPlayerRotation = m_fPlayerRotationTarget;
		uberAnim_->StopTurnInPlaceAnim();
	}
}

R3D_FORCEINLINE void UpdateBodyAdjustX( float* ioAdjust, float newadjust, float dt )
{
	float bodyAdjust_x = *ioAdjust ;
	bool was_less = bodyAdjust_x < newadjust ;

	bodyAdjust_x += (newadjust - bodyAdjust_x) * dt * 4.0f;

	if( was_less )
	{
		if( bodyAdjust_x > newadjust )
			bodyAdjust_x = newadjust ;
	}
	else
	{
		if( bodyAdjust_x < newadjust )
			bodyAdjust_x = newadjust ;
	}

	*ioAdjust = bodyAdjust_x ;
}

void obj_Player::UpdateUpperBodyAngLegs()
{
	float fTimePassed = r3dGetFrameTime();
	const float fMaxBodyAngle = R3D_DEG2RAD(45.0f);

	// turn firstly upper body only and only then legs
	float fY = getMinimumAngleDistance(m_fPlayerRotation, m_fPlayerRotationTarget);
	float newadjust = R3D_DEG2RAD(fY);

	UpdateBodyAdjustX( &bodyAdjust_x, newadjust, fTimePassed ) ;

	if(NetworkLocal && g_camera_mode->GetInt()==2)
	{
		UpdateRotationToTarget(fY, _ai_fTurnSpeedIdle);
	}
	else
	{
		if(uberAnim_->turnInPlaceTrackID == CUberAnim::INVALID_TRACK_ID)
		{
			if(R3D_ABS(bodyAdjust_x) > fMaxBodyAngle)
			{
				uberAnim_->StartTurnInPlaceAnim();
				bodyAdjust_x = R3D_CLAMP(bodyAdjust_x, -fMaxBodyAngle, fMaxBodyAngle);
			}
		}
		else
		{
			UpdateRotationToTarget(fY, _ai_fTurnSpeedIdle);
			// set new body adjust after turning body
			float fY = getMinimumAngleDistance(m_fPlayerRotation, m_fPlayerRotationTarget);
			float newadjust = R3D_DEG2RAD(fY);
			bodyAdjust_x = newadjust;
		}
	}
}

void obj_Player::UpdateRotation()
{
	R3DPROFILE_FUNCTION("UpdateRotation");

	uberAnim_->UpdateTurnInPlaceAnim();

	float fPrevRotation = m_fPlayerRotation;
	float fTimePassed = r3dGetFrameTime();
	if(PlayerState == PLAYER_IDLE || (PlayerState == PLAYER_MOVE_CROUCH && PlayerMoveDir == CUberData::ANIMDIR_Stand))
	{
		UpdateUpperBodyAngLegs();
	}
	else
	{
		uberAnim_->StopTurnInPlaceAnim();

		UpdateBodyAdjustX(&bodyAdjust_x, 0.f, fTimePassed * 4.0f);

		float fY = getMinimumAngleDistance(m_fPlayerRotation, m_fPlayerRotationTarget);
		UpdateRotationToTarget(fY, _ai_fTurnSpeedWalk);
	}

	// lerp body bending to target
	if(bodyAdjust_y[0] < bodyAdjust_y[1]) {
		bodyAdjust_y[0] = R3D_MIN(bodyAdjust_y[0] + fTimePassed * 3.2f, bodyAdjust_y[1]);
	} else if(bodyAdjust_y[0] > bodyAdjust_y[1]) {
		bodyAdjust_y[0] = R3D_MAX(bodyAdjust_y[0] - fTimePassed * 3.2f, bodyAdjust_y[1]);
	} else
		bodyAdjust_y[0] = bodyAdjust_y[1];

	// create rotation matrix
	D3DXMatrixRotationYawPitchRoll(&MoveMatrix, R3D_DEG2RAD(m_fPlayerRotation) + bodyAdjust_x, 0, 0);
	D3DXMatrixRotationYawPitchRoll(&DrawRotMatrix, R3D_DEG2RAD(m_fPlayerRotation), 0, 0);

	r3dPoint3D pos = GetPosition();

	extern int CurHUDID;
	if(g_camera_mode->GetInt()==2 && NetworkLocal && ((g_bEditMode && (CurHUDID==1 || CurHUDID==6) )||!g_bEditMode) && !bDead)
	{
		float hands_height = 0.0f;
		{
			D3DXMATRIX tmp, res;
			D3DXMatrixIdentity(&tmp);
			uberAnim_->anim.GetCurrentSkeleton()->GetBoneWorldTM(boneId_Bip01_Head, &res, tmp);
			hands_height = res._42;
		}
		// weapon movement when turning
		D3DXMATRIX m3, m4;
		float limit = 2.0f;
		if(m_isAiming)
			limit = 0.5f;
		// when firing, don't delay movement
		if(m_Weapons[m_SelectedWeapon] && (r3dGetTime() < m_Weapons[m_SelectedWeapon]->getLastTimeFired() + 0.5f))
			limit = 0.0f;
		float fRot = -R3D_CLAMP(getMinimumAngleDistance(fPrevRotation, m_fPlayerRotation)*10.0f, -limit, limit);
		static float EMAPrev = 0.0f;

		float EMACur = EMAPrev + 0.20f * (fRot - EMAPrev); // exponential moving average of about 15 prev.values
		EMAPrev = EMACur;

		D3DXMatrixRotationYawPitchRoll(&m3, R3D_DEG2RAD(EMACur), 0, 0);

		D3DXMatrixTranslation(&m4, 0, -hands_height, 0.1f);
		D3DXMatrixMultiply(&DrawFullMatrix, &m3, &m4);

		D3DXMatrixMultiply(&DrawFullMatrix_Localized, &DrawFullMatrix, &r3dRenderer->InvViewMatrix_Localized);		

		D3DXMatrixMultiply(&DrawFullMatrix, &DrawFullMatrix, &r3dRenderer->InvViewMatrix);
	}
	else
	{
		DrawFullMatrix = DrawRotMatrix;

		DrawFullMatrix._41 += pos.x;
		DrawFullMatrix._42 += pos.y;
		DrawFullMatrix._43 += pos.z;

		DrawFullMatrix_Localized = DrawRotMatrix;

		DrawFullMatrix_Localized._41 += pos.x - r3dRenderer->LocalizationPos.x;
		DrawFullMatrix_Localized._42 += pos.y - r3dRenderer->LocalizationPos.y;
		DrawFullMatrix_Localized._43 += pos.z - r3dRenderer->LocalizationPos.z;
	}

	// set physx rotation vectors
	SetRotationVector(r3dVector(m_fPlayerRotation, 0, 0));
}

static bool updateShootingTarget(obj_Player* pl, const r3dPoint3D& shootFrom, const r3dPoint3D& muzzlePos, r3dPoint3D& sPos, r3dPoint3D& sNorm, r3dMaterial** sMat, GameObject** gameObj, const char** actorName, float spread, bool recheckCollisionAgainsMuzzler)
{
	r3d_assert(pl);
	r3d_assert(sMat);
	r3d_assert(gameObj);

	const float MAX_CASTING_DISTANCE = 20000.0f;
	r3dPoint3D dir;
	if(pl->m_isInScope || g_camera_mode->GetInt() != 1)
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);
	else
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH*0.32f, &dir);

	*sMat = NULL;
	*gameObj = NULL;

	// convert spread from diameter at 50 meter range to radius at 1meter range
	spread = (spread*0.5f)/50.0f; // 0.5 - because spread initially set as diameter, and we need radius. 50-because spread is set at 50 meter range. formula is: tanA=opp/adj = so that is angle that we need to feed into raycast

	// limit spread to prevent ppl shooting behind, etc.
	spread = R3D_CLAMP(spread, -0.5f, 0.5f); 
	D3DXMATRIX rotMat;
	D3DXMatrixRotationYawPitchRoll(&rotMat, u_GetRandom(-spread, spread), u_GetRandom(-spread, spread), u_GetRandom(-spread, spread));
	r3dPoint3D dirWithSpread;
	D3DXVec3TransformNormal((D3DXVECTOR3*)&dirWithSpread, (D3DXVECTOR3*)&dir, &rotMat);
	{
		// shoot from camera :)
		PxRaycastHit hit;
		PhysicsCallbackObject* target = NULL;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
		g_pPhysicsWorld->raycastSingle(PxVec3(shootFrom.x, shootFrom.y, shootFrom.z), PxVec3(dirWithSpread.x, dirWithSpread.y, dirWithSpread.z), MAX_CASTING_DISTANCE, 
			PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eNORMAL, hit, filter);
		// check if target position is still visible from muzzler (prevent against shooting from wall edges
		if( recheckCollisionAgainsMuzzler && hit.shape )
		{
			PxVec3 dir2(hit.impact.x - muzzlePos.x, hit.impact.y - muzzlePos.y, hit.impact.z - muzzlePos.z);
			dir2.normalize();

			PxRaycastHit hit2;
			g_pPhysicsWorld->raycastSingle(PxVec3(muzzlePos.x, muzzlePos.y, muzzlePos.z), dir2, MAX_CASTING_DISTANCE, PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eNORMAL, hit2, filter);
			if(hit2.shape != hit.shape)
			{
				// replace old collision data with new one
				hit   = hit2;
			}
		}

		if( hit.shape && (target = static_cast<PhysicsCallbackObject*>(hit.shape->getActor().userData)))
		{
			if(actorName)
				*actorName = hit.shape->getActor().getName();

			*gameObj = target->isGameObject();
			if(*gameObj)
			{
				if( (*gameObj)->isObjType( OBJTYPE_Mesh ) )
					*sMat = static_cast< MeshGameObject* > ( target )->MeshLOD[ 0 ]->GetFaceMaterial( hit.faceIndex );
			}
			else if(target->hasMesh())
			{
				*sMat = target->hasMesh()->GetFaceMaterial( hit.faceIndex );
			}
			if (!*sMat)
				*sMat = target->GetMaterial(hit.faceIndex);

			sPos.x = hit.impact.x;
			sPos.y = hit.impact.y;
			sPos.z = hit.impact.z;

			sNorm.x = hit.normal.x;
			sNorm.y = hit.normal.y;
			sNorm.z = hit.normal.z;

			return true;
		}
	}

	sPos = muzzlePos + dir * 500.0f;
	sNorm = -dir;

	// do not allow to shoot behind you, like when there a pole between your character and camera
	{
		r3dPoint3D dir2 = (sPos - muzzlePos).Normalize();
		if(dir.Dot(dir2) < 0.0f)
		{
			*sMat = NULL;
			*gameObj = NULL;
		}
	}

	return false;
}

static bool updateMeleeTarget(obj_Player* pl, const r3dPoint3D& shootFrom, const r3dPoint3D& muzzlePos, r3dPoint3D& sPos, r3dPoint3D& sNorm, r3dMaterial** sMat, GameObject** gameObj, const char** actorName, float spread, bool recheckCollisionAgainsMuzzler)
{
	r3d_assert(pl);
	r3d_assert(sMat);
	r3d_assert(gameObj);

	*sMat = NULL;
	*gameObj = NULL;

	// for melee we need to check if we hit anyone in front of us. PhysX doesn't allow to do this check easily (overlaps do not return hit information, just shapes), so instead 
	// I'm going to do 9 ray casts in front of player. They are super short, so it shouldn't cause any performance issues 
	// and we going to hit first player, if any
	// TODO: we need to handle a case when we hit static object only and do not hit any player, but for melee this is minor
	const float MAX_CASTING_DISTANCE = 2.0f;
	float rayDegrees[9] = {-30.0f, -20.0f, -10.0f, -5.0f, 0.0f, 5.0f, 10.0f, 20.0f, 30.0f};
	for(int iRay = 0; iRay <9; ++iRay)
	{
		D3DXMATRIX rotMat;
		D3DXMatrixRotationYawPitchRoll(&rotMat, R3D_DEG2RAD(rayDegrees[iRay]), 0, 0);
		r3dPoint3D dirWithSpread;
		D3DXVec3TransformNormal((D3DXVECTOR3*)&dirWithSpread, (D3DXVECTOR3*)&gCam.vPointTo/*(D3DXVECTOR3*)&pl->GetvForw()*/, &rotMat);

		{
			PxRaycastHit hit;
			PhysicsCallbackObject* target = NULL;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
			g_pPhysicsWorld->raycastSingle(PxVec3(shootFrom.x, shootFrom.y, shootFrom.z), PxVec3(dirWithSpread.x, dirWithSpread.y, dirWithSpread.z), MAX_CASTING_DISTANCE, 
				PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eNORMAL, hit, filter);

			if( hit.shape && (target = static_cast<PhysicsCallbackObject*>(hit.shape->getActor().userData)))
			{
				*gameObj = target->isGameObject();
				if(!*gameObj)
					continue;
				if(!((*gameObj)->isObjType(OBJTYPE_Human) || (*gameObj)->isObjType(OBJTYPE_Zombie) || (*gameObj)->Class->Name == "obj_ZombieDummy"))
					continue;

				*sMat = NULL;

				if(actorName)
					*actorName = hit.shape->getActor().getName();

				sPos.x = hit.impact.x;
				sPos.y = hit.impact.y;
				sPos.z = hit.impact.z;

				sNorm.x = hit.normal.x;
				sNorm.y = hit.normal.y;
				sNorm.z = hit.normal.z;

				return true;
			}
		}
	}

	r3dPoint3D dir;
	if(pl->m_isInScope || g_camera_mode->GetInt() != 1)
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);
	else
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH*0.32f, &dir);

	sPos = muzzlePos + dir * 500.0f;
	sNorm = -dir;

	// do not allow to shoot behind you, like when there a pole between your character and camera
	{
		r3dPoint3D dir2 = (sPos - muzzlePos).Normalize();
		if(dir.Dot(dir2) < 0.0f)
		{
			*sMat = NULL;
			*gameObj = NULL;
		}
	}

	return false;
}

static bool isMuzzlerBlocked(obj_Player* pl, const r3dPoint3D& muzzlePos)
{
	// check if muzzler is visible, and if not (inside of wall) then we do not shoot anything
	bool muzzlerBlocked = false;
	{
		r3dVector temp = muzzlePos-gCam;
		float len = temp.Length();
		temp.Normalize();
		if(len > 0)
		{
			PxRaycastHit hit;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eDYNAMIC|PxSceneQueryFilterFlag::eSTATIC));
			muzzlerBlocked = g_pPhysicsWorld->raycastSingle( PxVec3(gCam.x,gCam.y,gCam.z), PxVec3(temp.x, temp.y, temp.z), len, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
		}
	}
	if(muzzlerBlocked)
		return true;

	const float MAX_CASTING_DISTANCE = 20000.f;
	r3dPoint3D dir;
	if(pl->m_isInScope || g_camera_mode->GetInt() != 1)
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);
	else
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH*0.32f, &dir);

	// check if camera->shoot_target position agains camera->muzzler (disable shooting when close to wall, but looking at step edge)
	float distToMuzzler = (gCam-muzzlePos).Length();
	{
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eDYNAMIC|PxSceneQueryFilterFlag::eSTATIC));
		if(g_pPhysicsWorld->raycastSingle(PxVec3(gCam.x, gCam.y, gCam.z), PxVec3(dir.x, dir.y, dir.z), MAX_CASTING_DISTANCE, PxSceneQueryFlags(PxSceneQueryFlag::eDISTANCE), hit, filter))
		{
			if(hit.distance < distToMuzzler)
			{
				// shooting blocked before muzzler
				return true;
			}
		}
	}

	return false;
}

void obj_Player::UpdateTargetting()
{
	R3DPROFILE_FUNCTION("UpdateTargetting");
	r3d_assert(NetworkLocal);

	m_LocalPlayer_CurrentAimAt = invalidGameObjectID;

	r3dMaterial* temp;
	GameObject* tempObj;
	r3dPoint3D muzzlerPos = GetMuzzlerPosition();

	{
		r3dPoint3D tmpPos, tmpNorm;
		// we always accept this position now, sometimes this jiggles, if that happens we'll have to look at the angle of the aiming and maybe factor out parts of it. 
		updateShootingTarget(this, gCam, muzzlerPos, tmpPos, tmpNorm, &temp, &tempObj, NULL, 0.0f, false);

		{
			viewTargetPos = tmpPos;
			viewTargetNorm = tmpNorm;
		}
	}

	const char* reticleColor = "white";
	if(tempObj)
	{
		m_LocalPlayer_CurrentAimAt = tempObj->GetSafeID();
		if(tempObj->isObjType(OBJTYPE_Human))
		{
			obj_Player* target = (obj_Player*)tempObj;
			if(!target->bDead)
			{
				reticleColor = "red";
			}
		}
	}

	r3dPoint3D bendTarget = GetPosition();
	bendTarget.Y += Height * 0.9f;
	bendTarget   += GetvForw() * 0.2f;

	r3dPoint3D v = (viewTargetPos - bendTarget);
	float dist = v.Length();
	float bendUp = asinf(v.y / v.Length());
	if(dist<1.8f)
		bendUp = R3D_LERP(0.0f, bendUp, dist/1.8f);

	// do not bend body in prone position
	if(PlayerState == PLAYER_PRONE_DOWN || PlayerState == PLAYER_PRONE_UP || PlayerState == PLAYER_MOVE_PRONE || PlayerState == PLAYER_PRONE_IDLE || PlayerState == PLAYER_PRONE_AIM)
		bendUp = 0;

	bodyAdjust_y[1] = bendUp;

#ifndef FINAL_BUILD
	if( g_char_spect->GetInt() )
	{
		bodyAdjust_y[1] = 0 ;
	}
#endif
}

r3dPoint3D obj_Player::GetMuzzlerPosition()
{
	if(GetSkeleton() && m_Weapons[m_SelectedWeapon])
	{
		return m_Weapons[m_SelectedWeapon]->getMuzzlePos(uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix));
	}

	return GetPosition() + GetvForw() * 1.0f + r3dPoint3D(0, 0.5f, 0);
}

// 0 - body
// 1 - head
// 2 - hands
// 3 - legs
BYTE obj_Player::getBodyNameFromBoneName(const char* boneName)
{
	struct boneToBodyPart_s {
		BYTE part;
		const char* bone;
	};
	const static boneToBodyPart_s map[] = {
		{1, "Bip01_Head"},
		{2, "Bip01_L_UpperArm"},
		{2, "Bip01_L_Forearm"},
		{2, "Bip01_L_Hand"},
		{2, "Bip01_R_UpperArm"},
		{2, "Bip01_R_Forearm"},
		{2, "Bip01_R_Hand"},
		{3, "Bip01_L_Thigh"},
		{3, "Bip01_L_Calf"},
		{3, "Bip01_L_Foot"},
		{3, "Bip01_L_Toe0"},
		{3, "Bip01_R_Thigh"},
		{3, "Bip01_R_Calf"},
		{3, "Bip01_R_Foot"},
		{3, "Bip01_R_Toe0"},
	};

	// not optimal, but numbers CAN change, because of skeleton modification
	for(int i=0; i<R3D_ARRAYSIZE(map); i++)
	{
		if(strcmp(map[i].bone, boneName) == 0) {
#ifndef FINAL_BUILD
			r3dOutToLog("%s mapped to %d\n", boneName, map[i].part);
#endif
			return map[i].part;
		}
	}

	return 0;
}

void obj_Player::CheckFireWeapon()
{
	r3d_assert(NetworkLocal);

	VMPROTECT_BeginMutation("obj_Player::CheckFireWeapon");

	bool needExit = false;
#ifndef FINAL_BUILD
	if(d_video_spectator_mode->GetBool()) needExit = true;
	if(d_observer_mode->GetBool()) needExit = true;
#endif
	if(hudPause && hudPause->isActive()) needExit = true;
	if(hudAttm && hudAttm->isActive()) needExit = true;
	if(hudGeneralStore && hudGeneralStore->isActive()) needExit = true;
	if(hudVault && hudVault->isActive()) needExit = true;
	if(Mouse->GetMouseVisibility()) needExit = true;
	if(hudMain && hudMain->isChatInputActive()) needExit = true;
	if(hudMain && hudMain->isPlayersListVisible()) needExit = true;

	static bool wasReloading = false; // static is okay, this code is running for local player only
	lifeProperties.weaponFired = false;

	Weapon* wpn = m_Weapons[m_SelectedWeapon];
	if((wpn == NULL || forcedEmptyHands))
	{
		if (!needExit)
		{
			if(g_toggle_aim->GetBool())
			{
				if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_AIM))
					m_isAiming = !m_isAiming;
			}
			else
			{
				m_isAiming = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_AIM);
			}
		}
		return;
	}

	if((PlayerState == PLAYER_MOVE_SPRINT && wpn->getCategory()!=storecat_MELEE) || bDead)  
	{
		if(wasReloading)
		{
			wasReloading = false;
		}
		needExit = true;
	}
	if(PlayerState == PLAYER_PRONE_DOWN || PlayerState == PLAYER_PRONE_UP)
		needExit = true;

	float gamepadLeftTrigger, gamepadRightTrigger;
	Gamepad->GetTrigger(gamepadLeftTrigger, gamepadRightTrigger);

	bool wpnIsNoLongerAvailable = false; // workaround for the fast that when you use last item, it will call OnBackpackChanged and will DELETE current weapon that you are using
	// not sure what is the proper way to fix this. Use smart pointers maybe?? 
	if(wpn->isUsableItem() && !needExit)
	{
		// special case items
		if(wpn->getItemID() == WeaponConfig::ITEMID_Binoculars || wpn->getItemID() == WeaponConfig::ITEMID_RangeFinder) // binoculars, range finder, etc
		{
			bool oldAiming = m_isAiming;
			if(g_toggle_aim->GetBool())
			{
				if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_AIM))
					m_isAiming = !m_isAiming;
			}
			else
			{
				m_isAiming = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_AIM) || gamepadLeftTrigger > 0;
			}

			extern 	PlayerStateVars_s CurrentRig;
			m_isInScope = hasScopeMode() && CurrentRig.allowScope;

			// remember that aiming was changed
			if(m_isAiming != oldAiming)
				m_isChangedAiming1 = true;

			extern float LerpValue;
			m_isFinishedAiming = m_isAiming && LerpValue==1.0f;

			if(wpn->getItemID() == 101319 && hudMain)
				hudMain->showRangeFinderUI(m_isFinishedAiming && m_isInScope);
		}
		else if(wpn->getItemID() == WeaponConfig::ITEMID_PieceOfPaper)
		{
			m_isAiming = false;
			m_isFinishedAiming = false;
			if(InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_PRIMARY_FIRE) || m_ItemSimulateLeftClick)
			{
				m_ItemSimulateLeftClick = false;
				if(hudMain && hudMain->canShowWriteNote())
					hudMain->showWriteNote(wpn->m_BackpackIdx);
			}
		}
		else if(wpn->getItemID() == WeaponConfig::ITEMID_AirHorn)
		{
			// todo
		}
		else
		{
			m_isAiming = false;
			m_isFinishedAiming = false;
			if(m_needToDrawBarricadePlacement && InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_AIM))
			{
				m_needToDrawBarricadePlacement = 0;
				m_BarricadeMeshPlacement = NULL;
			}

			// consumables
			if(InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_PRIMARY_FIRE) || m_ItemSimulateLeftClick)
			{
				m_ItemSimulateLeftClick = false;

				wiInventoryItem wi = wpn->getPlayerItem(); // COPY of item, not reference. because it can be reset in Fire()
				if(wi.quantity > 0 && wpn->isReadyToFire(true, false)) // consumables have rate of fire (cooldown)
				{
					if(wpn->getItemID() == WeaponConfig::ITEMID_BarbWireBarricade || 
						wpn->getItemID() == WeaponConfig::ITEMID_WoodShieldBarricade ||
						wpn->getItemID() == WeaponConfig::ITEMID_RiotShieldBarricade ||
						wpn->getItemID() == WeaponConfig::ITEMID_SandbagBarricade )
					{
						if(m_needToDrawBarricadePlacement == 0)
						{
							m_needToDrawBarricadePlacement = wpn->getItemID();
							return;
						}
						else
						{
							if(m_canPlaceConsumableItem)
							{
								m_needToDrawBarricadePlacement = 0; // and let user place riot shield by sending packet to server
								m_BarricadeMeshPlacement = NULL;
							}
							else
								return;
						}
					}

					if((wpn->getItemID() == WeaponConfig::ITEMID_Bandages || wpn->getItemID() == WeaponConfig::ITEMID_Bandages2 || wpn->getItemID() == WeaponConfig::ITEMID_Antibiotics ||
						wpn->getItemID() == WeaponConfig::ITEMID_Painkillers || wpn->getItemID() == WeaponConfig::ITEMID_Medkit) 
						&& CurLoadout.Health > 99)
					{
						if(hudMain) hudMain->showMessage(gLangMngr.getString("InfoMsg_MaxHealthAlready"));
						return;
					}
					if((wpn->getItemID() == WeaponConfig::ITEMID_C01Vaccine || wpn->getItemID() == WeaponConfig::ITEMID_C04Vaccine) 
						&& CurLoadout.Toxic < 1.0f)
					{
						if(hudMain) hudMain->showMessage(gLangMngr.getString("InfoMsg_NoTocixInBlood"));
						return;
					}

					uberAnim_->StartShootAnim();
					wpn->Fire(r3dPoint3D(0,0,0), D3DXMATRIX(), true); // to update last fire time

					showSlotsInfoTimer = r3dGetTime() + 5.0f; // show weapon info for X seconds
					if(hudMain)
						hudMain->showSlots(true);

					// todo: we need to sync this crap over network, so that network players will see us using item
					// todo: also, we need to sync when we sent this packet, so that it is in sync with animation
					float var1 = 0.0f;
					r3dPoint3D pos       = GetPosition() + GetvForw()*1.0f + r3dPoint3D(0,1,0);
					bool needCorrectPos = false; // those objects require precise position, otherwise there will be desync between client\server (mainly spawn beacon problem)
					if(wi.itemID == WeaponConfig::ITEMID_BarbWireBarricade ||
						wi.itemID == WeaponConfig::ITEMID_WoodShieldBarricade ||
						wi.itemID == WeaponConfig::ITEMID_RiotShieldBarricade || 
						wi.itemID == WeaponConfig::ITEMID_SandbagBarricade)
					{
						var1 = (m_fPlayerRotation) + R3D_RAD2DEG(bodyAdjust_x) + 180;
						needCorrectPos = true; 
					}

					if(needCorrectPos)
					{
						PxRaycastHit hit;
						PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC));
						if(g_pPhysicsWorld->raycastSingle(PxVec3(pos.x, pos.y, pos.z), PxVec3(0,-1,0), 3.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter))
							pos = r3dPoint3D(hit.impact.x, hit.impact.y+0.1f, hit.impact.z);
					}
					localPlayer_UseItem(wpn->m_BackpackIdx, wi.itemID, pos, var1);

					m_BandagesEffectTime = r3dGetTime() + 0.5f;

					// after Fire() check new quantity
					{
						if(wpn->getConfig()->m_isConsumable)
							wi.quantity--; // simulate what fire() does
						if(wi.quantity <= 0) {
							wpnIsNoLongerAvailable = true;
							OnBackpackChanged(m_SelectedWeapon);
						}
						else
						{
							if(hudMain)
								hudMain->updateSlotInfo(m_SelectedWeapon, wi.quantity);
						}
					}

				}
			}
		}
		needExit = true;
	}

	if(!bAllowToUseWeapons)
		needExit = true;

	if(needExit)
	{
		if(!wpnIsNoLongerAvailable)
			wpn->isReadyToFire(false, false); // very important to call ready to fire with false to reset internal counter of how long trigger was pressed
		return;
	}

	if(wpn->getItemID() == WeaponConfig::ITEMID_Flashlight)
	{
		if (InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_TOGGLE_FLASHLIGHT))
		{
			wpn->toggleFlashlight();
		}
	}

	m_isPressedFireTrigger = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_PRIMARY_FIRE) || gamepadRightTrigger > 0;
	// check muzzle block except when using scope
	if(!m_isInScope && wpn->getCategory()!=storecat_MELEE && isMuzzlerBlocked(this, GetMuzzlerPosition()))
		m_isPressedFireTrigger = false;

	bool oldAiming = m_isAiming;
	if(g_toggle_aim->GetBool())
	{
		if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_AIM))
			m_isAiming = !m_isAiming;
	}
	else
	{
		m_isAiming = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_AIM) || gamepadLeftTrigger > 0;
	}

	// check if we just switched weapon and do not allow to fire
	if((r3dGetTime() - lastTimeWeaponSwitch) < 0.4f)
	{
		m_isPressedFireTrigger = false;
		m_isAiming = false;
	}

	extern 	PlayerStateVars_s CurrentRig;
	m_isInScope = hasScopeMode() && CurrentRig.allowScope;

	if(wpn->isReloading())
		m_isAiming = false;

	//if(wpn->getAnimType()== WPN_ANIM_MELEE) // no aiming in melee
	//	m_isAiming = false;

	// remember that aiming was changed
	if(m_isAiming != oldAiming)
		m_isChangedAiming1 = true;

	extern float LerpValue;
	if (wpn->getCategory() != storecat_SNP)
	{
		m_isFinishedAiming = m_isAiming && LerpValue==1.0f;
	}
	else 
	{
		m_isFinishedAiming = m_isAiming && LerpValue==1.0f && m_isInScope;
	}

	bool playerCrouching = PlayerState==PLAYER_MOVE_CROUCH || PlayerState==PLAYER_MOVE_CROUCH_AIM; 
	bool playerProning = PlayerState==PLAYER_MOVE_PRONE || PlayerState==PLAYER_PRONE_AIM || PlayerState == PLAYER_PRONE_IDLE;
	bool playerMoving  = InputAcceleration.Length() > 0.01f;
	bool playerRunning = PlayerState==PLAYER_MOVE_RUN;
	int triggerPressedCounter = wpn->getTriggerPressedCounter();

	float recoilCoolDownMod = 1.0f;
	if(triggerPressedCounter < 3)
		recoilCoolDownMod = 2.50f;
	else
		recoilCoolDownMod = 3.2f;

	// BEGIN SPREAD\RECOIL LOGIC

	if(!m_isPressedFireTrigger) // if user let go of trigger, start return recoil back to normal right away, so that it doesn't linger at the top for a second or two after stopping firing
	{
		if(wpn->getCategory() != storecat_SNP)
		{
			RecoilCooldown = 0;
		}
	}

	//[ST] trying to get higher recoil without changing database. hack :)
	float recoil = wpn->getRecoil()*1.1f;

	float spread = CurrentSpread; // spread will be affected now by player state and other modificators

	//[ST] - just in case - not sure why we needed it
	//spread *= GPP->c_fGlobalSpreadMod;

	// Okay by default in NON ADS mode we're increasing spread
	spread *= 1.1f;

	if (wpn->getCategory()==storecat_SHTG) spread *= 1.2f;


	if(playerCrouching)
	{
		//recoil *= GPP->c_fSpreadRecoilMod_Crouch;
		if(wpn->getCategory()==storecat_SHTG)
			spread *= GPP->c_fSpreadModShotgun_Crouch;
		else
			spread *= GPP->c_fSpreadRecoilMod_Crouch;
	}
	if(m_isFinishedAiming)
	{
		recoil *= GPP->c_fSpreadRecoilMod_Aim*2.0f;
		if(wpn->getCategory()==storecat_SHTG)
			spread *= GPP->c_fSpreadModShotgun_Aim;
		else
			spread *= GPP->c_fSpreadRecoilMod_Aim;
	}
	if(playerRunning)
	{
		recoil *= GPP->c_fSpreadRecoilMod_Running;
		spread *= GPP->c_fSpreadRecoilMod_Running;
	}
	//if(playerMoving)
	//{
	//	recoil *= GPP->c_fSpreadRecoilMod_Moving;
	//	spread *= GPP->c_fSpreadRecoilMod_Moving;
	//}

	/*if(wpn->getCategory() == storecat_SNP && !m_isAiming)
	{
	m_ReticleTargetScale = 25.0f;
	}*/

	// first bullet logic
	if(!playerRunning && m_isFinishedAiming && wpn->getCategory() != storecat_SHTG && bOnGround)
	{
		// spr_mod - to prevent player from shooting straight at target by just clicking on a mouse, rather than holding fire
		float spr_mod = 1.0f - R3D_CLAMP(((r3dGetTime() - wpn->getLastTimeFired())/GPP->c_fZeroSpreadDelayBetweenShots), 0.0f, 0.9f); // 0.9 - first bullet 10% spread
		// if shooting for 10 bullets straight you spread will slowly increase
		//[ST] - Removed this code
		//spread = R3D_LERP(spread*spr_mod, spread, R3D_CLAMP(float(triggerPressedCounter)/10.0f, 0.0f, 1.0f)); 

		// special case for sniper rifle
		if(wpn->getCategory() == storecat_SNP && m_isInScope && triggerPressedCounter==0)
			spread = 0;
	}

	// if in air
	if(!bOnGround)
	{
		if(wpn->getCategory() == storecat_HG)
			spread *= 2.0f;
		else
			spread *= 4.0f; // hard coded, if jumping = spread increased by X times
	}

	// special case for sniper rifle
	if(wpn->getCategory() == storecat_SNP && !m_isInScope)
		spread = 11; // fixed spread at 11 for sniper rifles if not aiming - per sergey's request


	// FINISHED SPREAD\RECOIL LOGIC

	{
		bool isReloading = wpn->isReloading();
		if(isReloading && !wasReloading)
		{
			wasReloading = true;
		}
		if(!isReloading && wasReloading)
		{
			wasReloading = false;
		}
		if(isReloading)
		{
			float progress = R3D_CLAMP((wpn->getReloadProgress()/wpn->getReloadTime()), 0.0f, 1.0f);
		}
	}

	if((InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_RELOAD)||Gamepad->WasReleased(gpX)) && !wpn->isReloading())
		wpn->Reload();

	if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_CHANGE_RATE_FIRE) && !wpn->isReloading())
	{
		wpn->switchFiremode();
		//		if(hudMain)
		//			hudMain->setFireMode(wpn->getFiremode());
	}

	// check for grenade throw, if successful it will go to normal fire code
	if(wpn->getCategory() == storecat_GRENADE)
	{
		const wiInventoryItem& wi = wpn->getPlayerItem(); 
		if(!uberAnim_->IsGrenadePinPullActive() && wi.quantity > 0)
		{
			if(wpn->isReadyToFire(m_isPressedFireTrigger, m_isFinishedAiming)) 
			{
				uberAnim_->StartGrenadePinPullAnimation();

				PKT_C2C_PlayerReadyGrenade_s n;
				n.wid = m_SelectedWeapon;
				p2pSendToHost(this, &n, sizeof(n));
				return;
			}
		}
	}


	bool isReadyToFire = wpn->isReadyToFire(m_isPressedFireTrigger, m_isFinishedAiming);
	float holdingDelay = 0;

	// for grenades, override regular logic a little bit
	if(wpn->getCategory()==storecat_GRENADE && uberAnim_->IsGrenadePinPullActive())
	{
		// override previous readytofire, grenade will be ready only after animation is finished
		isReadyToFire = false;
		if(uberAnim_->IsGrenadePinPullFinished()) // ready to throw
		{
			if(grenadeReadyTime == 0)
				grenadeReadyTime = r3dGetTime(); // first frame when we finished playing pin pull animation, store time, as after that grenade timer is start to tick :)
			if(!m_isPressedFireTrigger || (r3dGetTime()-grenadeReadyTime)>wpn->getConfig()->m_AmmoDelay) // released trigger -> throw
			{
				holdingDelay = r3dGetTime()-grenadeReadyTime;
				grenadeReadyTime = 0;
				isReadyToFire = true;
			}
		}
	}

	// sniper aiming and holding breath logic
	{
		// reset flag that we're holding breath only when key is released. used for prevent sprinting (same key)
		if(m_isHoldingBreath && !InputMappingMngr->isPressed(r3dInputMappingMngr::KS_HOLD_BREATH))
			m_isHoldingBreath = false;

		static float last_timer = 0;
		if(wpn->getCategory() == storecat_SNP && m_isAiming) // sniper rifle and in scope mode
		{
			r3d_assert(NetworkLocal); // just to make sure, as this code should run only for local player (using statics here)

			if(InputMappingMngr->isPressed(r3dInputMappingMngr::KS_HOLD_BREATH) && m_Breath>0/* && m_BreathPenaltyTime<=0*/)
			{
				m_isHoldingBreath = true;

				//ViewAngle += SniperViewMod;
				//SniperViewMod.Assign(0,0,0);
				m_Breath = m_Breath - r3dGetFrameTime();
				if(m_Breath < 0)
					m_Breath = 0;
				//if(m_Breath <= 0)
				//	m_BreathPenaltyTime = GPP->c_fSniperRifleAfterHoldBounceTime;
			}
			else
			{
				float swayDecrease = 0.75f;

				SniperViewMod.Assign((r3dSin(last_timer/3)*3)*swayDecrease, (r3dCos(last_timer))*swayDecrease, 0);
				last_timer += r3dGetFrameTime();
			}
		}
		else
		{
			SniperViewMod.Assign(0,0,0);
			last_timer = 0;
		}
	}

	if(isReadyToFire) 
	{
		if(wpn->getCategory() == storecat_GRENADE)
		{
			uberAnim_->StartGrenadeThrowAnimation();
		}

		int numActualShots = wpn->getNumShotsRequired();
		int numVirtualShots = 0; // for shotgun
		if(numActualShots && wpn->getCategory() == storecat_SHTG)
			numVirtualShots = 7;

		while(numActualShots>0)
		{
			executeWeaponFireLocalPlayer(gCam, recoil, spread, recoilCoolDownMod, holdingDelay, true, !m_isInScope, 0);	
			--numActualShots;

			// update spread after shooting
			CurrentSpread += SpreadIncrease[wpn->getCategory()-storecat_ASR]*wpn->getSpread();
			CurrentSpread = R3D_MIN(CurrentSpread, wpn->getSpread());
			lifeProperties.weaponFired = true;
		}
		while(numVirtualShots>0)
		{
			executeWeaponFireLocalPlayer(gCam, recoil, spread, recoilCoolDownMod, holdingDelay, false, !m_isInScope, 0);	
			--numVirtualShots;
			lifeProperties.weaponFired = true;
		}

		uberAnim_->StartRecoilAnim();
	}

	m_ReticleTargetScale = R3D_MIN(1.0f + spread, 12.0f);

	// update reload anim
	if(wpn->isReloading())
	{
		uberAnim_->StartReloadAnim();
	}
	else
	{
		uberAnim_->StopReloadAnim();
	}

	VMPROTECT_End();
}

// grenades are thrown at specific animation frame, so we cannot remove weapon (grenade) before it executed. So we have to wait for grenade to be thrown before updating our inventory
// this callback handles this
void obj_Player::GrenadeCallbackFromWeapon(const wiInventoryItem& wi)
{
	// simulate what fire() will do
	if(hudMain)
		hudMain->showSlots(true);

	if(wi.quantity <= 0) {
		OnBackpackChanged(m_SelectedWeapon);
	}
	else
	{
		if(hudMain)
			hudMain->updateSlotInfo(m_SelectedWeapon, wi.quantity);
	}
}

float obj_Player::GetGrenadeLaunchDelay()
{
	return uberAnim_->GetGrenadeLaunchFrame() / 25.0f;
}


// make sure this function is the same on server: bool ServerGameLogic::CanDamageThisObject(const GameObject* targetObj)
bool canDamageTarget(const GameObject* obj)
{
	if(obj->isObjType(OBJTYPE_Human))
		return true;
	if(obj->isObjType(OBJTYPE_Zombie))
		return true;
	if(obj->Class->Name == VMPROTECT_DecryptStringA("obj_LightMesh"))
	{
		obj_LightMesh* lmesh = (obj_LightMesh*)obj;
		if(lmesh->isLightOn())
			return true;
		return false;
	}
	if(obj->Class->Name == VMPROTECT_DecryptStringA("obj_Barricade"))
	{
		return true;
	}

	return false;
}

float g_ScopeFeedbackTimer = 0.0f;

void obj_Player::executeWeaponFireLocalPlayer(const r3dPoint3D& from, float recoil, float spread, float recoilCoolDownMod, float holdingDelay, bool executeWeaponFireCode, bool checkAgainsMuzzler, int damagePercFromPiercing)
{
	r3d_assert(NetworkLocal);

	Weapon* wpn = m_Weapons[m_SelectedWeapon];
	r3d_assert(wpn);
	r3d_assert(!wpn->isUsableItem());

	r3dPoint3D	shootTargetPos;
	r3dPoint3D	shootTargetNorm;
	GameObject*	shootTarget    = NULL;
	r3dMaterial*	shootMaterial  = NULL;
	const char*	shootActorName = "";

	// reset rotation if we're firing from idle mode
	if(PlayerState == PLAYER_IDLE)
	{
		bodyAdjust_x = 0;
		m_fPlayerRotation = m_fPlayerRotationTarget;
	}

	if(wpn->getCategory() == storecat_SNP)
	{
		// increase recoil for sniper rifle artificially
		recoil *= 10.0f;
		//if(!m_isFinishedAiming && spread>0) // if not aiming, make spread really huge
		//	spread = 50.0f;
	}

	r3dPoint3D muzzlerPos = GetMuzzlerPosition();
	{
		if(wpn->getCategory() == storecat_MELEE)
			updateMeleeTarget(this, GetPosition()+r3dPoint3D(0, bProne?0.2f:(bCrouch?0.6f:1.5f), 0.0f), muzzlerPos, shootTargetPos, shootTargetNorm, &shootMaterial, &shootTarget, &shootActorName, spread, checkAgainsMuzzler);
		else
			updateShootingTarget(this, from, muzzlerPos, shootTargetPos, shootTargetNorm, &shootMaterial, &shootTarget, &shootActorName, spread, checkAgainsMuzzler);
	}

	RecoilViewModTarget.y += recoil * u_GetRandom(0.85f, 1.75f);
	if(RecoilViewModTarget.y > GPP->c_MaxVerticalRecoil) // limit how much gun goes up
	{
		RecoilViewModTarget.y = u_GetRandom(GPP->c_MaxVerticalRecoil*0.95f, GPP->c_MaxVerticalRecoil*1.1f);
	}
	RecoilViewModTarget.x += recoil*u_GetRandom(-0.15f, 0.3f); // + to a side

	if(executeWeaponFireCode) // set cooldown only for actual fire
	{
		RecoilCooldown = R3D_MAX(wpn->getRateOfFire()*recoilCoolDownMod, r3dGetFrameTime()*2.50f);
	}

	if(wpn->getCategory()==storecat_SNP || wpn->getCategory()==storecat_SHTG)
		if(RecoilCooldown > r3dGetFrameTime()*8.0f)
			RecoilCooldown = r3dGetFrameTime()*8.0f;



	// send weapon fired event
	if(wpn->getCategory() != storecat_GRENADE)
	{
		PKT_C2C_PlayerFired_s n; 
		n.fire_from = GetPosition();
		n.fire_to = shootTargetPos;
		n.holding_delay = holdingDelay;
		n.debug_wid = m_SelectedWeapon;
		n.wasAiming = m_isAiming;
		n.execWeaponFire = executeWeaponFireCode;
		p2pSendToHost(this, &n, sizeof(n), true);
	}

	if(wpn->isImmediateShooting())
	{
		if ( ProcessBulletHit(damagePercFromPiercing, this, shootTargetPos, shootTargetNorm, shootTarget, shootMaterial, shootActorName,  wpn->getConfig(), GetMuzzlerPosition() )  == false ) 
		{	
			r3dPoint3D dir = shootTargetPos - from;
			dir.Normalize();
			executeWeaponFireLocalPlayer(shootTargetPos + dir*0.1f, 0.0f, 0.0f, 0.0f, holdingDelay, false, false, damagePercFromPiercing);
		}
	}

	wpn->Fire(shootTargetPos, uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix), executeWeaponFireCode, holdingDelay);

	if(executeWeaponFireCode)
	{
		extern float g_shootCameraShakeTimer;
		g_shootCameraShakeTimer = 1.0f;
		if(m_isAiming && hasScopeMode())
		{
			g_ScopeFeedbackTimer = 1.0f;
		}
	}

	//	Change player noise value
	lifeProperties.weaponFired = true;
}

void obj_Player::UpdateAnimations(const D3DXMATRIX& CharDrawMatrix)
{
	R3DPROFILE_FUNCTION("UpdateAnimations");
	const float TimePassed = r3dGetFrameTime();
#if ENABLE_RAGDOLL
	bool ragdoll = m_PhysSkeleton && m_PhysSkeleton->IsRagdollMode();
	if (!ragdoll)
#endif
	{
		uberAnim_->anim.Update(TimePassed, r3dPoint3D(0,0,0), CharDrawMatrix);
		uberAnim_->anim.Recalc();
	}

	if(m_PhysSkeleton)
		m_PhysSkeleton->syncAnimation(GetSkeleton(), DrawFullMatrix, GetAnimation());

#if ENABLE_RAGDOLL
	if (ragdoll)
	{
		r3dBoundBox bbox = m_PhysSkeleton->getWorldBBox();
		bbox.Org -= GetPosition();
		SetBBoxLocal(bbox);
	}
#endif
}

r3dPoint3D obj_Player::GetHeadPosition()
{
	D3DXMATRIX m;
	uberAnim_->anim.GetCurrentSkeleton()->GetBoneWorldTM(boneId_Bip01_Head, &m, DrawFullMatrix);

	return r3dPoint3D(m._41, m._42, m._43);
}

void obj_Player::DoDeath(int networdIDKiller, bool forced_by_server, STORE_CATEGORIES damageSource)
{
	//r3dOutToLog("%s: DoDeath\n", Name.c_str());
	r3d_assert(!bDead);

	if(NetworkLocal)
	{
		CurLoadout.Health = 0; // to make sure that UI will show zero health
	}

	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/BulletHits/Hit_Death"), GetPosition());		

	CurrentSpread = 0;

	ReleasePlayerObstacle(&PhysXObstacleIndex);	// player is dead, allow passage thru him

	m_needToDrawBarricadePlacement = 0;
	m_BarricadeMeshPlacement = NULL;
	m_canPlaceConsumableItem = false;

	m_ItemSimulateLeftClick = false;

	m_SpeedBoost = 0;
	m_SpeedBoostTime = 0;
	m_MorhineShotTime = 0;
	m_BandagesEffectTime = 0;

	bCrouch = 0;
	bProne = 0;
	m_isAiming = 0;
	m_isPressedFireTrigger = false;
	lastDamageRcvd = 0;
	BloodEffect = 0;
	bDead = 1;
	TimeOfDeath = r3dGetTime();
	DeathDamageSource = damageSource;
	PosOfDeath = GetPosition();
	Dead_KillerID = networdIDKiller;
	DisableKillerView = false;

	m_AuraType = AT_NONE;
	grenadeReadyTime = 0;

	// if we have not default backpack - remove it as it will be dropped as item on server
	if(CurLoadout.BackpackID != 20176) {
		uberEquip_->SetSlot(SLOT_Backpack, (r3dMesh*)NULL);
	}

	PlayerState = PLAYER_DIE;
	uberAnim_->AnimPlayerState = PLAYER_DIE;
	uberAnim_->StartDeathAnim();
	DoRagdoll(true, lastTimeHitBone, &lastTimeHitForce);

	// clear rotation so it won't affect ragdoll bbox
	SetRotationVector(r3dPoint3D(0, 0, 0));

	RealAcceleration.Assign(0,0,0);
	InputAcceleration.Assign(0,0,0);

	if(!NetworkLocal)
	{
		const ClientGameLogic& CGL = gClientLogic();
		if(CGL.localPlayer_ && CGL.localPlayer_->GetNetworkID() == Dead_KillerID)
		{
			if(!CGL.localPlayer_->bDead)
			{
				CGL.localPlayer_->m_LastKillTime = r3dGetTime();
			}
		}
	}
	else
	{

		//		hudMain->setHealth(0);

		r_hud_filter_mode->SetInt(0); // turn off NVG
	}

	if(NetworkLocal)
	{
		if(hudAttm && hudAttm->isActive())
			hudAttm->Deactivate();
		if(hudPause && hudPause->isActive())
			hudPause->Deactivate();
		if(hudGeneralStore && hudGeneralStore->isActive())
			hudGeneralStore->Deactivate();
		if(hudVault && hudVault->isActive())
			hudVault->Deactivate();	

		g_camera_mode->SetInt(0);
		UpdateLoadoutSlot(CurLoadout); // reload meshes
		SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/MainMenu GUI/UI_player_death"), GetPosition());

		if(hudMain)
		{
			char deadMsg[128] = {0};
			GameObject* killedByObj = GameWorld().GetNetworkObject(networdIDKiller);
			if(killedByObj)
			{
				if(killedByObj == this)
				{
					r3dscpy(deadMsg, "KILLED BY ENVIRONMENT");
				}
				else if(killedByObj->isObjType(OBJTYPE_Human))
				{
					obj_Player* killedByPlr = (obj_Player*)killedByObj;
					char tmpName[64] = {0};
					killedByPlr->GetUserName(tmpName);
					sprintf(deadMsg, "KILLED BY %s", tmpName);
				}
				else if(killedByObj->isObjType(OBJTYPE_Zombie))
					r3dscpy(deadMsg, "EATEN BY ZOMBIE");
			}
			hudMain->showYouAreDead(deadMsg);
		}
	}

	return;
}

void obj_Player::Undie()
{
	afterRespawnTimer = 2.0f;

	lastWeapDataRep = r3dGetTime();

	bDead = 0;

	// clear death anim
	uberAnim_->anim.AnimTracks.clear();

	DoRagdoll(false);

	PlayerState = PLAYER_IDLE;
	SyncAnimation(true);

}

static void dumpAnim(r3dAnimation& anim)
{
	for(size_t i=0; i<anim.AnimTracks.size(); i++) 
	{
		const r3dAnimation::r3dAnimInfo& ai = anim.AnimTracks[i];

		char st[256] = "";
		if(ai.GetStatus() & ANIMSTATUS_Playing) strcat(st, "Play ");
		if(ai.GetStatus() & ANIMSTATUS_Paused) strcat(st, "Pause ");
		if(ai.GetStatus() & ANIMSTATUS_Finished) strcat(st, "Finish ");
		if(ai.GetStatus() & ANIMSTATUS_Fading) strcat(st, "Fade ");
		if(ai.GetStatus() & ANIMSTATUS_Expiring) strcat(st, "Expire ");

		r3dOutToLog("%d: %s %f, %X %s\n", ai.GetTrackID(), ai.GetAnim()->GetAnimName(), ai.GetInfluence(), ai.GetStatus(), st);
	}
}

r3dSkeleton* obj_Player::GetSkeleton()
{
	return uberAnim_->anim.pSkeleton;
}

r3dAnimation& obj_Player::GetAnimation()
{
	return uberAnim_->anim;
}

void obj_Player::SyncAnimation(bool force)
{
	R3DPROFILE_FUNCTION("obj_Player::SyncAnimation");
	if(bDead)
		return;

	const Weapon* wpn = m_Weapons[m_SelectedWeapon];
	if(forcedEmptyHands)
		wpn = NULL;
	bool isInAttmMenu = false;
	if(wpn && hudAttm && hudAttm->isActive() && wpn->getCategory()>=storecat_ASR && wpn->getCategory()<=storecat_SMG)
		isInAttmMenu = true;

	uberAnim_->SyncAnimation(PlayerState, PlayerMoveDir, force, wpn, isInAttmMenu); // firstly sync to update weapon pointer and only then update jump
	uberAnim_->UpdateJump(bOnGround);
}

static float accumLastDamageRcvd = 0;
void obj_Player::ApplyDamage(const r3dPoint3D& dmgPos, float damage, GameObject* fromObj, int bodyBone, int dmgType)
{
	lastTimeHit = r3dGetTime();
	lastTimeHitBone = bodyBone;
	lastDamageRcvd = damage;
	accumLastDamageRcvd += damage;

	/*if (fromObj == gClientLogic().localPlayer_)
	{
	if(bodyBone == uberAnim_->GetBoneID("Bip01_Head"))
	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/BulletHits/Hit_Headshot"), GetPosition());
	else if(dmgType==storecat_MELEE)
	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/Impacts/Impacts-KnifeStab"), GetPosition());		
	else
	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/Impacts/Impacts-Body"), GetPosition());		
	}*/

	// apply ragdoll force from object position, or up if too close
	r3dPoint3D ragHitPos = GetPosition();
	lastTimeHitForce = r3dPoint3D(0, 1, 0);
	if(bodyBone < GetSkeleton()->NumBones)
		ragHitPos = uberEquip_->getBonePos(bodyBone, GetSkeleton(), DrawFullMatrix);
	if((ragHitPos - fromObj->GetPosition()).LengthSq() > 0.001f)
		lastTimeHitForce = (ragHitPos - fromObj->GetPosition()).NormalizeTo();

	float dmgForce = 10.0f;
	switch(dmgType)
	{
	case storecat_ASR:
		dmgForce = 30.0f;
		break;
	case storecat_SNP:
		dmgForce = 60.0f;
		break;
	case storecat_SHTG:
		dmgForce = 60.0f;
		break;
	case storecat_MG:
		dmgForce = 40.0f;
		break;
	case storecat_HG:
		dmgForce = 10.0f;
		break;
	case storecat_SMG:
		dmgForce = 20.0f;
		break;
	}
	lastTimeHitForce *= dmgForce;

#ifndef FINAL_BUILD
	if(NetworkLocal && m_isAiming)
	{
		float recoil = 3.5f;
		RecoilViewModTarget2.x = recoil * (random(2)==1?u_GetRandom(-1.0f, -0.5f):u_GetRandom(0.5f, 1.0f));
		RecoilViewModTarget2.y = recoil * (random(2)==1?u_GetRandom(-1.0f, -0.5f):u_GetRandom(0.5f, 1.0f));
	}
#endif
}

bool obj_Player::ChangeWeaponByIndex(int index)
{
	r3d_assert(index>=0 && index < NUM_WEAPONS_ON_PLAYER);
	if(m_SelectedWeapon == index && !forcedEmptyHands)
		return false;

	m_needToDrawBarricadePlacement = 0;
	m_BarricadeMeshPlacement = NULL;
	m_canPlaceConsumableItem = false;

	forcedEmptyHands = false;
	m_PrevSelectedWeapon = m_SelectedWeapon;
	m_SelectedWeapon = index;

	uberAnim_->StopReloadAnim();

	if(m_PrevSelectedWeapon >= 0 && m_Weapons[m_PrevSelectedWeapon])
	{
		m_Weapons[m_PrevSelectedWeapon]->OnUnequip();
	}

	if(m_Weapons[m_SelectedWeapon])
	{
		m_Weapons[m_SelectedWeapon]->OnEquip();
	}

	if(hudMain && NetworkLocal)
	{
		showSlotsInfoTimer = r3dGetTime() + 5.0f;
		hudMain->showSlots(true);

		updateWeaponUI();
	}


	UpdateCharWeaponMeshes();
	return true;
}

void obj_Player::UpdateCharWeaponMeshes()
{
	//r3d_assert(!m_Weapons[m_SelectedWeapon].Empty());

	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	uberEquip_->SetSlot(SLOT_Weapon, m_Weapons[m_SelectedWeapon]);
	if(m_SelectedWeapon==0)
	{
		uberEquip_->SetSlot(SLOT_WeaponBackRight, (Weapon*)NULL);
	}
	else
	{
		if(m_Weapons[0])
		{
			uberEquip_->SetSlot(SLOT_WeaponBackRight, m_Weapons[0]);
		}
	}
	if(m_SelectedWeapon==1)
		uberEquip_->SetSlot(SLOT_WeaponSide, (Weapon*)NULL);
	else
	{
		if(m_Weapons[1])
			uberEquip_->SetSlot(SLOT_WeaponSide, m_Weapons[1]);
	}
}

void obj_Player::ReloadWeaponFromSlot(int wpnSlot, int ammoSlot, int amount)
{
	r3d_assert(NetworkLocal);

	// remove clip (SERVER CODE SYNC POINT)
	wiInventoryItem& wi = CurLoadout.Items[ammoSlot];
	r3d_assert(wi.quantity > 0);
	wi.quantity--;
	if(wi.quantity <= 0)
		wi.Reset();

	OnBackpackChanged(ammoSlot);

	PKT_C2C_PlayerReload_s n;
	n.WeaponSlot = (BYTE)wpnSlot;
	n.AmmoSlot   = (BYTE)ammoSlot;
	n.dbg_Amount = (BYTE)amount;
	p2pSendToHost(this, &n, sizeof(n));

	return;
}

float obj_Player::getMaxStamina()
{
	float maxStamina = GPP->c_fSprintMaxEnergy;
	return maxStamina;
}

float obj_Player::getMaxBreath()
{
	float maxBreath = GPP->c_fSniperRifleHoldBreathTime;
	return maxBreath;
}

void obj_Player::StartJump()
{
	r3d_assert(NetworkLocal);

	//check if we are joing to jump into water, cancel jump in that case
	{
		r3dPoint3D vel    = r3dPoint3D(0,0,0);
		vel    += (GetvForw()  * InputAcceleration.Z);
		vel    += (GetvRight() * InputAcceleration.X);
		vel.Y  += InputAcceleration.Y;

		r3dPoint3D newPos = vel + GetPosition();
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
		bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(newPos.x, newPos.y + 0.5f, newPos.z), PxVec3(0, -1, 0), 10.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
		r3dPoint3D posForWater = newPos;
		if( hitResult )
			posForWater = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);			

		float waterDepth = getWaterDepthAtPos(GetPosition());
		if(waterDepth > 1.0f) // too deep
		{
			return; // cancle jump
		}
	}

	uberAnim_->StartJump();
	JumpVelocity  = 17;
	JumpStartTime = r3dGetTime() + uberAnim_->jumpStartTime;

	PKT_C2C_PlayerJump_s n;
	p2pSendToHost(this, &n, sizeof(n));

	SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_JUMPING_F":"Sounds/WarZ/PlayerSounds/PLAYER_JUMPING_M"), GetPosition());
}

bool obj_Player::IsJumpActive()
{
	// going up on jump, or in air.
	return (JumpVelocity > 0 || !bOnGround);
}

void obj_Player::DetectIfOnGround()
{
	//static bool prevOnGround = bOnGround;

	PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
	if(g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 1.0f, PxSceneQueryFlags(PxSceneQueryFlag::eDISTANCE), hit, filter))
	{
		bOnGround = (hit.distance < 0.6f);
		fHeightAboveGround = hit.distance - 0.5f;
	}
	else
	{
		bOnGround = false;
		fHeightAboveGround = 10.0f;
	}

	// hack to check if player got glued to some geometry while jumping. Happen if he jumped on something that has some bumps and character controller thinks that he can stay on that tiny bump
	if(!bOnGround && R3D_ABS(oldstate.Position.y - GetPosition().y) < 0.01f)
	{
		bOnGround = true;
		fHeightAboveGround = 0.0f;
	}

	//if(prevOnGround != bOnGround)
	//{
	//	prevOnGround = bOnGround;
	//	r3dOutToLog("onGround=%d\n", bOnGround);
	//}
}

void obj_Player::UpdateLocalPlayerMovement()
{
	r3d_assert(NetworkLocal);

	VMPROTECT_BeginMutation("obj_Player::UpdateLocalPlayerMovement");

	// important: limit movement to max for 1 sec. so server can check for teleports
	const float TimePassed = R3D_MIN(r3dGetFrameTime(), 0.1f);
	const float curTime    = r3dGetTime();

	RealAcceleration = InputAcceleration;

	// apply weight to moving speed before gravity and jumping
	float weightAdj = 1.0f + ((m_GearWeight/100.0f)); // each 100kg half your speed
	RealAcceleration = RealAcceleration/weightAdj;

	// locally add gravity to acceleration
	float Gravity = 9.81f;
	float deltaTimeInAir =(curTime-StartFallingTime);
	if(!bOnGround && JumpVelocity<=0) // don't accelerate falling while jumping, only when actually falling
	{
		Gravity += Gravity * deltaTimeInAir;
	}
	float waterDepth = getWaterDepthAtPos(GetPosition());

	if (waterDepth < 1.4f)
	{
		RealAcceleration.Y -= Gravity;
	}
	if(JumpVelocity > 0 && curTime >= JumpStartTime && waterDepth < 0.4f)
	{
		JumpVelocity -= Gravity * TimePassed * 2.5f;
		RealAcceleration.Y += JumpVelocity;
		// reset falling time
		if(JumpVelocity<=0)
			StartFallingTime = curTime;
	}
	if(bOnGround)
		StartFallingTime = curTime; // keep reseting timer, otherwise when jump it will not be null

	r3dPoint3D vel    = r3dPoint3D(0,0,0);
	vel    += (GetvForw()  * RealAcceleration.Z);
	vel    += (GetvRight() * RealAcceleration.X);
	vel.Y  += RealAcceleration.Y;

	// send some info to server about fast movement
	{
		static float lastUpd = -999;
		r3dPoint3D velXZ(vel.x, 0, vel.z);
		if(velXZ.Length() > 8.0f && curTime > lastUpd + 60.0f)
		{
			lastUpd = curTime;
			char buf[1024];
			sprintf(buf, "%.2f %.2f %.1f/%.1f %.2f %.1f", 
				velXZ.x, velXZ.z, GPP->AI_SPRINT_SPEED, GPP->AI_RUN_SPEED, TimePassed, weightAdj);

			PKT_C2S_DBG_LogMessage_s n;
			r3dscpy(n.msg, buf);
			p2pSendToHost(this, &n, sizeof(n));
		}
	}

	ControllerPhysObj* controller = (ControllerPhysObj*)PhysicsObject;

	controller->Move(vel * TimePassed, 50000);
	r3dPoint3D prevPos = GetPosition();
	parent::Update(); // manually update position after updating physics. Needed for camera, otherwise it will be lagging one frame behind

	// check if player is trying to get into water
	// use raycast to fix bug when player able to walk on water by jumping
	{
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
		bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 500.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
		r3dPoint3D posForWater = GetPosition();
		if( hitResult )
			posForWater = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);			

		float waterDepth = getWaterDepthAtPos(GetPosition());
		if(waterDepth > 1.5f) // too deep
		{
			SetPosition(r3dPoint3D(GetPosition().x,GetPosition().y + 0.1f,GetPosition().z)); // Up Player to a water
		}
		else
			SetVelocity(vel);
	}

	float ll = (prevPos-GetPosition()).LengthSq();
	if(ll > 0.0001f)
	{
		extern void WaterSplash(const r3dPoint3D& waterSplashPos, float height, float size, float amount, int texIdx);
		WaterSplash(GetPosition(), 2.0f, 5.0f, 0.1f*ll, 3);
	}

	bool bWasOnGround = bOnGround;
	DetectIfOnGround();

	if(bWasOnGround && !bOnGround)
	{
		StartFallingHeight = GetPosition().y;
		StartFallingTime = r3dGetTime();
	}
	if(!bWasOnGround && bOnGround)
	{
		// calculate damage from falling if any
		float newHeight = GetPosition().y;
		if((StartFallingHeight - newHeight) > 5.0f) // more than 5 meters, we gonna break something
		{
			float damageAmount = ((StartFallingHeight - newHeight))*4.0f; // if falling more than 25 meters - dies immediatelly (damage >=100)
			damageAmount = R3D_CLAMP(damageAmount, 0.0f, 255.0f);

			// send damage to server
			PKT_C2S_FallingDamage_s n;
			n.damage = (BYTE)damageAmount;
			p2pSendToHost(this, &n, sizeof(n));

			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_GRUNTING_M"), GetPosition());
		}
	}

	// STAMINA LOGIC SHOULD BE SYNCED WITH SERVER CODE!
	if(PlayerState == PLAYER_MOVE_SPRINT || bSwimShift)
	{
		m_Stamina = m_Stamina - TimePassed;
		if(m_Stamina <= 0)
		{
			m_StaminaPenaltyTime = 3.0f;
		}
	}
	else if(bOnGround || bSwim) // regen only if standing on ground and swim
	{
		float regen_rate = CurLoadout.Health<50?GPP->c_fSprintRegenRateLowHealth:GPP->c_fSprintRegenRateNormal;

		m_Stamina = m_Stamina + TimePassed*regen_rate; // regeneration rate
		if(m_StaminaPenaltyTime > 0.0f)
			m_StaminaPenaltyTime = m_StaminaPenaltyTime - TimePassed;
	}
	m_Stamina = R3D_CLAMP((float)m_Stamina, 0.0f, getMaxStamina());

	bool scopeRendering = hasScopeMode() && CurrentRig.allowScope;
	if(m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getCategory() == storecat_SNP && m_isFinishedAiming) 
	{
		if(hudMain)
		{
			//			hudMain->SetEnergyValue(1.0f); // hide energy bar
			//			hudMain->SetBreathValue(R3D_CLAMP((m_Breath/getMaxBreath()), 0.0f, 1.0f));
		}
	}
	else
	{
		if(hudMain)
		{
			//			hudMain->SetBreathValue(2.0f); // hide breath
			//			hudMain->SetEnergyValue(m_Stamina/getMaxStamina());
		}
	}

	VMPROTECT_End();
}

void obj_Player::localPlayer_UseItem(int slotID, uint32_t itemID, const r3dPoint3D& pos, float var1, float var2, float var3, DWORD var4)
{
	r3d_assert(NetworkLocal);

	PKT_C2C_PlayerUseItem_s n;
	n.SlotFrom  = (BYTE)slotID;
	n.dbg_ItemID= itemID;
	n.pos       = pos;
	n.var1		= var1;
	n.var2		= var2;
	n.var3		= var3;
	n.var4		= var4;
	p2pSendToHost(this, &n, sizeof(n), true);

	const FoodConfig* fc = g_pWeaponArmory->getFoodConfig(itemID);
	if(fc)
	{
		if(fc->category == storecat_Food)
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_EAT"), GetPosition());
		else
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_DRINK"), GetPosition());

		m_Stamina += GPP->c_fSprintMaxEnergy*fc->Stamina;
	}
	if (itemID==WeaponConfig::ITEMID_Antibiotics||itemID==WeaponConfig::ITEMID_Bandages||itemID==WeaponConfig::ITEMID_Bandages2||
		itemID==WeaponConfig::ITEMID_Painkillers||itemID==WeaponConfig::ITEMID_Medkit)
	{
		if(var4!=0)
		{
			GameObject* obj = GameWorld().GetNetworkObject(var4);
			if(obj && obj->isObjType(OBJTYPE_Human))
			{
				obj_Player* plr = (obj_Player*)obj;
				SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(plr->m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_HEAL_F":"Sounds/WarZ/PlayerSounds/PLAYER_HEAL_M"), plr->GetPosition());
			}
		}
		else
			SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_HEAL_F":"Sounds/WarZ/PlayerSounds/PLAYER_HEAL_M"), GetPosition());
	}


}

BOOL obj_Player::Update()
{
	R3DPROFILE_FUNCTION("obj_Player::Update");
	if(!NetworkLocal) // for local already done in UpdateLocalPlayerMovement
		parent::Update();

	camoTimeLine.Update();

	// show pending rewards, even if game finished.
	if(NetworkLocal)
		ProcessRewardQueue();

	const ClientGameLogic& CGL = gClientLogic();
	if(gClientLogic().disconnectStatus_)
		return TRUE;

	const float curTime = r3dGetTime();

#ifndef FINAL_BUILD
	if(NetworkLocal && gDisableShortcuts == false )
	{
		if( Keyboard->IsPressed( kbsLeftAlt ) && Keyboard->WasPressed( kbsK ) )
		{
			g_char_spect->SetInt( !g_char_spect->GetInt() ) ;
		}

		// 		if(Keyboard->WasPressed(kbsF4))
		// 		{
		// 			// send disconnect request to server
		// 			PKT_C2S_DisconnectReq_s n;
		// 			p2pSendToHost(this, &n, sizeof(n), true);
		// 		}

		if(Keyboard->IsPressed(kbsLeftAlt) && hudMain)
		{
			if(Keyboard->WasPressed(kbs8))
			{
				hudMain->showChat(true);
				hudMain->addChatMessage(0, "blabla", "rtjdkfhdkjfhdf dfd f", 1);
			}
			if(Keyboard->WasPressed(kbsF5))
			{
				// send data update request to server
				PKT_C2S_DataUpdateReq_s n;
				p2pSendToHost(this, &n, sizeof(n), true);
				hudMain->showMessage(L"Data update request sent");
			}

			if(Keyboard->WasPressed(kbs8))
			{
				PKT_C2S_TEST_SpawnDummyReq_s n;
				n.pos = GetPosition() + GetvForw() * 4.0f;
				PxRaycastHit hit;
				PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
				if(g_pPhysicsWorld->raycastSingle(PxVec3(n.pos.x, n.pos.y+2, n.pos.z), PxVec3(0, -1, 0), 4, PxSceneQueryFlag::eIMPACT, hit, filter))
				{
					n.pos.x = hit.impact.x;
					n.pos.y = hit.impact.y;
					n.pos.z = hit.impact.z;
				}

				p2pSendToHost(this, &n, sizeof(n));
			}
		}
	}
#endif

	/*{
	// check that last known position is good (player didn't log out while in air, or underground, etc) to prevent being stuck
	{
	PxSweepHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
	PxBoxGeometry boxg(0.5f, 0.1f, 0.5f);
	PxTransform pose(PxVec3(GetPosition().x, GetPosition().y+0.5f, GetPosition().z));
	if(g_pPhysicsWorld->PhysXScene->sweepSingle(boxg, pose, PxVec3(0,-1,0), 2000.0f, PxSceneQueryFlag::eIMPACT, hit, filter))
	{
	r3dOutToLog("@@@@ %.2f, %.2f, %.2f\n", hit.impact.x, hit.impact.y, hit.impact.z);
	}
	}
	}*/

	float timePassed = r3dGetFrameTime();

	// SPREAD/RECOIL LOGIC
	if(NetworkLocal && m_Weapons[m_SelectedWeapon])
	{
		if(CurrentSpread > 0 && !m_isPressedFireTrigger)
		{
			CurrentSpread -= timePassed*SpreadDecrease[m_Weapons[m_SelectedWeapon]->getCategory()-storecat_ASR];
		}
		CurrentSpread = R3D_MAX(CurrentSpread, SpreadMin[m_Weapons[m_SelectedWeapon]->getCategory()-storecat_ASR]*m_Weapons[m_SelectedWeapon]->getSpread());

		//r3dOutToLog("1) Recoil=%.2f, cooldown=%.2f\n", RecoilViewModTarget.y, RecoilCooldown.get());
		// update recoil
		if(!RecoilViewMod.AlmostEqual((RecoilViewModTarget), 0.01f))
		{
			RecoilViewMod = R3D_LERP(RecoilViewMod, RecoilViewModTarget, R3D_MIN(timePassed*GPP->c_fRecoilCooldownSpeed*4.0f * u_GetRandom(0.75f, 1.25f), 1.0f)); // lerp to make it smooth, less jerky
		}
		RecoilViewMod = R3D_LERP(RecoilViewMod, (RecoilViewMod+RecoilViewModTarget2), timePassed*2.0f);

		if(RecoilCooldown>0)
			RecoilCooldown = RecoilCooldown-timePassed;

		if(RecoilCooldown<=0)
		{
			// slowly make recoil go back to zero
			RecoilViewModTarget = R3D_LERP(RecoilViewModTarget, r3dVector(0,0,0), R3D_MIN(timePassed*GPP->c_fRecoilCooldownSpeed*8.0f, 1.0f));
		}
		// recoil effect when you are hit
		RecoilViewModTarget2 = R3D_LERP(RecoilViewModTarget2, r3dVector(0,0,0), R3D_MIN(timePassed*GPP->c_fRecoilCooldownSpeed*2.0f, 1.0f));

		//r3dOutToLog("2) Recoil=%.2f, cooldown=%.2f\n", RecoilViewModTarget.y, RecoilCooldown.get());
	}
	// FINISHED SPREAD/RECOIL LOGIC

	// if mouse is visible - then we are in control menu mode, so DO NOT update player weapon (shooting) or player movement
	// update weapon selection
	if(NetworkLocal && !Mouse->GetMouseVisibility() && !bDead && !(g_pDefaultConsole && g_pDefaultConsole->IsVisible()) && !(hudMain && hudMain->isChatInputActive()))
	{
		R3DPROFILE_FUNCTION("WeaponSelection");
		int prevSelectedItem = m_SelectedWeapon;
		bool prevEmptyHandsState = forcedEmptyHands;
		{
			int buttonPressed = 0;
			if(PlayerState != PLAYER_MOVE_SPRINT )
			{
				if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_PRIMARY_WEAPON))
					buttonPressed = 1;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_HANDGUN_WEAPON))
					buttonPressed = 2;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_ITEM1))
					buttonPressed = 3;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_ITEM2))
					buttonPressed = 4;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_ITEM3))
					buttonPressed = 5;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_ITEM4))
					buttonPressed = 6;
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_QUICK_SWITCH_WEAPON))
				{
					if(m_PrevSelectedWeapon == -1)
						buttonPressed = 1;	// switch to second weapon if no previous weapon was selected
					else
						buttonPressed = m_PrevSelectedWeapon+1; // +1 to map from index to button pressed
				}
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_NEXTITEM))
				{
					// scan for 4 item slots and see if we can switch to next item
					int selWeapon = R3D_MAX(m_SelectedWeapon, 1);
					for(int i=0; i<4; i++)
					{
						if(++selWeapon >= NUM_WEAPONS_ON_PLAYER)
							selWeapon = 2;
						if(m_Weapons[selWeapon] != NULL)
						{
							buttonPressed = selWeapon + 1;
							break;
						}
					}
				}
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_TOGGLE_NIGHTVISION))
				{
					// check if player has night vision goggles
					if(CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20188 ||
						CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20187 ||
						CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20067
						)
						r_hud_filter_mode->SetInt(r_hud_filter_mode->GetInt() == HUDFilter_NightVision ? HUDFilter_Default : HUDFilter_NightVision);
				}
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_FREE_HANDS))
				{
					switchToEmptyHands();
				}
				else
				{
					// query mouse scroll
					int mouseScroll = 0;
					Mouse->QueryWheelMotion(&mouseScroll);
					if(Gamepad->WasPressed(gpY))
						mouseScroll = 1;

					if(mouseScroll!=0)
					{
						int selWeapon = m_SelectedWeapon;
						for(int i=0; i<NUM_WEAPONS_ON_PLAYER; i++)
						{
							if(mouseScroll>0) {
								if(++selWeapon >= NUM_WEAPONS_ON_PLAYER)
									selWeapon = 0;
							} else {
								if(--selWeapon < 0)
									selWeapon = NUM_WEAPONS_ON_PLAYER - 1;
							}

							if(m_Weapons[selWeapon] != NULL) {
								buttonPressed = selWeapon + 1;
								break;
							}
						}
					}
				}
			} 
			else if ( PlayerState == PLAYER_MOVE_SPRINT ) 
			{
				// clear the mouse wheel. 
				int mouseScroll = 0;
				Mouse->QueryWheelMotion(&mouseScroll);
			}


			r3d_assert(buttonPressed>=0 && buttonPressed <=NUM_WEAPONS_ON_PLAYER);
			const FoodConfig* foodItem = NULL;
			if((buttonPressed-1)>=wiCharDataFull::CHAR_LOADOUT_ITEM1 && (buttonPressed-1)<=wiCharDataFull::CHAR_LOADOUT_ITEM4)
				foodItem = g_pWeaponArmory->getFoodConfig(CurLoadout.Items[buttonPressed-1].itemID);
			if(foodItem)
			{
				wiInventoryItem& wi = CurLoadout.Items[buttonPressed-1];
				if(wi.quantity > 0)
				{
					showSlotsInfoTimer = r3dGetTime() + 5.0f; // show weapon info for X seconds
					if(hudMain)
					{
						hudMain->showSlots(true);
						hudMain->setActivatedSlot(buttonPressed-1);
					}

					localPlayer_UseItem(buttonPressed-1, foodItem->m_itemID, GetPosition() + GetvForw()*1.0f + r3dPoint3D(0,1,0));

					if(--wi.quantity <= 0) {
						wi.Reset();
						OnBackpackChanged(buttonPressed-1);
					}
					else
					{
						if(hudMain)
							hudMain->updateSlotInfo(buttonPressed-1, wi.quantity);
					}
				}
			}
			else if(buttonPressed && m_Weapons[buttonPressed-1])
			{
				if(buttonPressed>2 && !(m_Weapons[buttonPressed-1]->isUsableItem() || m_Weapons[buttonPressed-1]->getCategory()==storecat_GRENADE))
				{
					// do not allow to switch to quick slots if they are not usable items or grenades
				}
				else
				{
					if((m_Weapons[buttonPressed-1]->isUsableItem() || m_Weapons[buttonPressed-1]->getCategory()==storecat_GRENADE) && InputMappingMngr->isPressed(r3dInputMappingMngr::KS_PRIMARY_FIRE))
					{
						// do not allow switching to usable item while fire key is pressed
					}
					else
					{
						if(ChangeWeaponByIndex(buttonPressed-1))
						{
							lastTimeWeaponSwitch = curTime;
						}
						else if(buttonPressed >= 3 && buttonPressed <=6) // sergey's request: already was pressed and this is an item, simulate left click!
						{
							if(m_Weapons[buttonPressed-1]->isUsableItem())
								m_ItemSimulateLeftClick = true;
						}
					}
				}
			}
		}

		if(prevSelectedItem != m_SelectedWeapon || prevEmptyHandsState != forcedEmptyHands)
		{
			SyncAnimation(true);

			PKT_C2C_PlayerSwitchWeapon_s n;
			n.wid       = forcedEmptyHands?255:m_SelectedWeapon;
			p2pSendToHost(this, &n, sizeof(n), true);
		}
	}

	// WARNING: current model is rotated 180 around y
	D3DXMATRIX mr;
	D3DXMatrixRotationY(&mr, R3D_PI);
	D3DXMATRIX CharDrawMatrix = mr;

	if(NetworkLocal && !Mouse->GetMouseVisibility()) 
	{
		if(m_HitMarkerFadeout > 0)
			m_HitMarkerFadeout -= timePassed * GPP->c_fHitMarkerFadeoutSpeed;

		UpdateTargetting();
	}

	plr_local_moving_speed = (GetPosition() - oldstate.Position).Length();
	if(NetworkLocal) 
	{
		// moved to separate function and called from UpdatePlayerMovement, to make sure that player position and camera position are in sync
	} 
	else if(GetVelocity().LengthSq() > 0.0001f)
	{
		R3DPROFILE_FUNCTION("NetPlayer_Move");
		r3dPoint3D prevPos = GetPosition();
		SetPosition(GetPosition() + GetVelocity() * timePassed);

		DetectIfOnGround();

		// check if we overmoved to target position
		r3dPoint3D v = netMover.GetNetPos() - GetPosition();
		float d = GetVelocity().Dot(v);
		if(d < 0) {
			SetPosition(netMover.GetNetPos());
			SetVelocity(r3dPoint3D(0, 0, 0));
		}

		float ll = (prevPos-GetPosition()).LengthSq();
		if(ll > 0.0001f)
		{
			extern void WaterSplash(const r3dPoint3D& waterSplashPos, float height, float size, float amount, int texIdx);
			WaterSplash(GetPosition(), 2.0f, 5.0f, 0.1f*ll, 3);
		}
	}
	if(afterRespawnTimer>0)
		afterRespawnTimer -= timePassed;
	//r3dOutToLog("Player Update: %s to %.2f, %.2f, %.2f\n", UserName, GetPosition().x, GetPosition().y, GetPosition().z);

	// update footsteps after moving player
	UpdateFootsteps();
	/*
	// update camo only if player has special items
	gCamouflageDataManager.UpdateCamouflageData(*this);
	*/

	UpdateSounds();


	// even if dead, still need to send pos, otherwise if died in the air - you will still be in the air.
	if(NetworkLocal) 
	{
		CNetCellMover::moveData_s md;
		md.pos       = GetPosition();
		md.turnAngle = m_fPlayerRotationTarget;
		md.bendAngle = bodyAdjust_y[1];
		md.state     = (PlayerState&0xF) | ((PlayerMoveDir&0xF) << 4);

		PKT_C2C_MoveSetCell_s n1;
		PKT_C2C_MoveRel_s     n2;
		DWORD pktFlags = netMover.SendPosUpdate(md, &n1, &n2);
		if(pktFlags & 0x1)
			p2pSendToHost(this, &n1, sizeof(n1), true);
		if(pktFlags & 0x2)
			p2pSendToHost(this, &n2, sizeof(n2), false);
	}

	//
	// send hash of currently equipped weapon configs
	//
	if(NetworkLocal && !bDead && curTime >= (lastWeapDataRep + PKT_C2S_PlayerWeapDataRep_s::REPORT_PERIOD))
	{
		r3d_assert(!bDead);
		lastWeapDataRep = curTime;

		PKT_C2S_PlayerWeapDataRep_s n;
		r3d_assert(2 == R3D_ARRAYSIZE(n.weaponsDataHash));
		for(int i=0; i<2; i++)
		{
			n.weaponsDataHash[i] = 0;
			n.debug_wid[i] = 0;
			if(m_Weapons[i] == NULL)
				continue;
			n.weaponsDataHash[i] = m_Weapons[i]->getConfig()->GetClientParametersHash();

			n.debug_wid[i] = m_Weapons[i]->getItemID();
			m_Weapons[i]->getConfig()->copyParametersTo(n.debug_winfo[i]);
		}

		p2pSendToHost(this, &n, sizeof(n), true);
	}

	if(NetworkLocal && hudMain)
	{
		R3DPROFILE_FUNCTION("update hud");
		// update HUD
		{
			{
				float bloodAlpha = 1.0f-R3D_MIN(r3dGetTime()-lastTimeHit, 1.0f);
				hudMain->setBloodAlpha(bloodAlpha);
			}

			bAllowToUseWeapons = true;
			if(CurLoadout.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox)
				bAllowToUseWeapons = false;
			hudMain->showSafeZoneWarning(!bAllowToUseWeapons);

			hudMain->setVisibility(lifeProperties.getPlayerVisibility());
			hudMain->setHearing(lifeProperties.getPlayerHearRadius());

			//hudMain->setTime(CGL.GetServerGameTime());
			if(hudPause->isActive())
				hudPause->setTime(CGL.GetServerGameTime());

			hudMain->setLifeParams((int)CurLoadout.Hunger, (int)CurLoadout.Thirst, (int)CurLoadout.Health, (int)CurLoadout.Toxic, int(((getMaxStamina()-m_Stamina)/getMaxStamina())*100.0f));
			if(m_Weapons[m_SelectedWeapon])
			{
				if(m_Weapons[m_SelectedWeapon]->getCategory() == storecat_GRENADE) // treated as items
				{
					const wiInventoryItem& wi = m_Weapons[m_SelectedWeapon]->getPlayerItem();
					hudMain->setWeaponInfo(R3D_MIN(1, wi.quantity), R3D_MIN(0, wi.quantity-1), 1);
				}
				else
					hudMain->setWeaponInfo(m_Weapons[m_SelectedWeapon]->getNumBulletsLeft(), m_Weapons[m_SelectedWeapon]->getNumClipsLeft(), (int)m_Weapons[m_SelectedWeapon]->getFiremode());
			}

			if(r3dGetTime() > showSlotsInfoTimer && showSlotsInfoTimer>0)
			{
				hudMain->showSlots(false);
				showSlotsInfoTimer = 0;
			}
		}
		{
			// update items
			for(int i=wiCharDataFull::CHAR_LOADOUT_WEAPON1; i<=wiCharDataFull::CHAR_LOADOUT_ITEM4; ++i)
			{
				const Weapon* wpn = m_Weapons[i];
				if(!wpn) continue;

				if(wpn->isUsableItem())
				{
					hudMain->updateSlotInfo(i, CurLoadout.Items[i].quantity);
				}
			}

			float bloodLevel = 0;
			if(CurLoadout.Health < 25.0f)
				bloodLevel = 100.0f - ((CurLoadout.Health/25.0f)*100.0f); // show blood effect when health is between 0 and 25
			PFX_3DLUTColorCorrection::s_damageHitColorCorrectionVal = (bloodLevel/100.0f);
			//hudMain->SetBloodLevel(bloodLevel);
		}
	}
	// CLAN TAG
	if(!NetworkLocal && hudMain)
	{
		if(m_CharIcon.IsUndefined() && CGL.localPlayer_ && ClanID == CGL.localPlayer_->ClanID)
		{
			char plrUserName[256]; GetUserNameAndClanTag(plrUserName);
			hudMain->addCharTag1(plrUserName, CurLoadout.Stats.Reputation, false, m_CharIcon);
		}

		if(!m_CharIcon.IsUndefined())
		{
			bool showTag = true;
			bool showName = (this->GetSafeID() == m_LocalPlayer_CurrentAimAt)?true:false;
			bool alwaysShow = true;

			if(bDead)
				showTag = false;

			hudMain->setCharTagTextVisible1(m_CharIcon, showName, false);
			hudMain->moveUserIcon(m_CharIcon, GetPosition()+r3dPoint3D(0,2.0f,0), alwaysShow);
		}
	}

	// check for dead after moving player, otherwise might die in the air and be stuck in the air (dead body)
	if(bDead)
	{
		if(NetworkLocal)
			PFX_3DLUTColorCorrection::s_damageHitColorCorrectionVal = 0;

		UpdateRotation();

		UpdateAnimations(CharDrawMatrix);

		if(m_Weapons[m_SelectedWeapon])
			m_Weapons[m_SelectedWeapon]->Update(uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix));

		return TRUE;
	}

	if(!NetworkLocal)
	{
		UpdatePlayerObstacle(PhysXObstacleIndex, GetPosition());
	}

	SyncAnimation();

	UpdateRotation();

	UpdateAnimations(CharDrawMatrix);

	if(m_Weapons[m_SelectedWeapon])
	{
		m_Weapons[m_SelectedWeapon]->Update(uberEquip_->getWeaponBone(GetSkeleton(), DrawFullMatrix));
	}

	if(NetworkLocal)
	{
		// breath timer
		/*if(m_BreathPenaltyTime > 0)
		{
		m_BreathPenaltyTime = m_BreathPenaltyTime - r3dGetFrameTime();
		if(m_BreathPenaltyTime < 0)
		m_BreathPenaltyTime = 0;
		}
		else*/
		{
			if(!InputMappingMngr->isPressed(r3dInputMappingMngr::KS_HOLD_BREATH))
			{
				if(m_Breath < getMaxBreath())
					m_Breath = m_Breath + timePassed;
			}
		}
	}

	{
		if(r3dGetTime() < m_MorhineShotTime && m_AuraType == AT_NONE)
			m_AuraType = AT_MORHINE;
		if(m_AuraType == AT_MORHINE && r3dGetTime() > m_MorhineShotTime)
			m_AuraType = AT_NONE;
	}

	if(NetworkLocal)
	{
		if(BloodEffect > 0)
		{
			BloodEffect -= timePassed;
			if(BloodEffect < 0) BloodEffect = 0;
		}
	}

	// update fire
	if(NetworkLocal && !Mouse->GetMouseVisibility())
	{
		if(fabs(m_ReticleCurrentScale - m_ReticleTargetScale)>0.01f)
			m_ReticleCurrentScale = R3D_LERP(m_ReticleCurrentScale, m_ReticleTargetScale, timePassed * 5.0f);

		bool scopeRendering = (hasScopeMode() && CurrentRig.allowScope && m_isAiming) || (g_camera_mode->GetInt()==2 && m_isAiming);
	}

	// if low health - start dropping blood every second
	m_BloodTimer += timePassed;
	if(CurLoadout.Health < 30 && m_BloodTimer > 1.0f)
	{
		r3dPoint3D pos = GetPosition(); pos.y += 2.0f;
		AddBloodOnGround(pos);
		m_BloodTimer = 0;
	}

	if(NetworkLocal)
		UpdateActionUI();

	return TRUE;
}

float dist_Point_to_Line( const r3dPoint3D& p, const r3dPoint3D& l1, const r3dPoint3D& l2, bool clampToSegment=false)
{
	r3dVector v = l2 - l1;
	r3dVector w = p - l1;

	float c1 = w.Dot(v);
	float c2 = v.Dot(v);
	float b = c1 / c2;
	if(clampToSegment)
		b = R3D_CLAMP(b, 0.0f, 1.0f);

	r3dPoint3D Pb = l1 + b * v;
	return (p-Pb).Length();
}

void obj_Player::UpdateActionUI()
{
	R3DPROFILE_FUNCTION("UpdateActionUI");
	r3d_assert(NetworkLocal);
	if(hudMain==NULL || hudActionUI == NULL || hudPause == NULL ||
		hudAttm == NULL || hudGeneralStore == NULL || hudVault == NULL)
		return;

	if(hudMain->isChatInputActive())
		return;
	if(hudPause->isActive())
		return;
	if(hudAttm->isActive())
		return;
	if(hudGeneralStore->isActive())
		return;
	if(hudVault->isActive())
		return;

	if(bDead && hudActionUI->isActive())
		hudActionUI->Deactivate();
	if(bDead)
		return;

	float maxDist = g_camera_mode->GetInt()==2?3.0f:4.0f;

	r3dVector forw = gCam.vPointTo;//GetvForw();
	r3dVector searchStartLine = gCam;
	r3dVector searchEndLine = searchStartLine + forw*maxDist;

	// slow, need to rewrite
	GameObject* dropObj = 0;
	float distance = 999999999.0f;

	float curTime = r3dGetTime();

	bool selectHumans = false;
	{
		uint32_t wpnItemID = 0;
		if(m_Weapons[m_SelectedWeapon])
			wpnItemID = m_Weapons[m_SelectedWeapon]->getItemID();
		selectHumans = wpnItemID==WeaponConfig::ITEMID_Antibiotics||wpnItemID==WeaponConfig::ITEMID_Bandages||wpnItemID==WeaponConfig::ITEMID_Bandages2||
			wpnItemID==WeaponConfig::ITEMID_Painkillers||wpnItemID==WeaponConfig::ITEMID_ZombieRepellent||wpnItemID==WeaponConfig::ITEMID_C01Vaccine||
			wpnItemID==WeaponConfig::ITEMID_C04Vaccine||wpnItemID==WeaponConfig::ITEMID_Medkit;
		if(m_Weapons[m_SelectedWeapon])
		{
			wiInventoryItem wi = CurLoadout.Items[m_Weapons[m_SelectedWeapon]->m_BackpackIdx];
			if(wi.quantity == 0)
				selectHumans = false;
		}
	}

	/*	ObjectManager& GW = GameWorld();
	for(GameObject *obj = GW.GetFirstObject(); obj; obj = GW.GetNextObject(obj))
	{
	if(obj->isObjType(OBJTYPE_SharedUsableItem) || (obj->isObjType(OBJTYPE_Human)&&selectHumans&&obj!=this))
	{
	float d = dist_Point_to_Line(obj->GetBBoxWorld().Center(), searchStartLine, searchEndLine, true); 
	if(distance > d)
	{
	if(obj->isObjType(OBJTYPE_SharedUsableItem))
	{
	if(((SharedUsableItem*)obj)->m_lastPickupAttempt < curTime)
	{
	dropObj = obj;
	distance = d;
	}
	}
	else // human
	{
	dropObj = obj;
	distance = d;
	}
	}
	}
	}*/
	{
		std::list<SharedUsableItem*>::iterator it;
		for(it=m_SharedUsableItemList.begin(); it!=m_SharedUsableItemList.end(); ++it)
		{
			SharedUsableItem* obj = *it;
			r3dPoint3D objCenter = obj->GetBBoxWorld().Center();
			float d = dist_Point_to_Line(objCenter, searchStartLine, searchEndLine, true); 
			if(distance > d)
			{
				// check dist to player
				if((objCenter - GetPosition()).LengthSq() > (maxDist*maxDist))
					continue;
				// check if both player and camera are facing object
				if((GetPosition()-objCenter).Dot(gCam-objCenter) < 0.0f) 
					continue;
				if(obj->m_lastPickupAttempt < curTime)
				{
					dropObj = obj;
					distance = d;
				}
			}
		}
		if(selectHumans)
		{
			ClientGameLogic& CGL = gClientLogic();
			for(int i=0; i<ClientGameLogic::MAX_NUM_PLAYERS; ++i)
			{
				obj_Player* plr = CGL.GetPlayer(i);
				if(plr && plr != this)
				{
					float d = dist_Point_to_Line(plr->GetBBoxWorld().Center(), searchStartLine, searchEndLine, true); 
					if(distance > d)
					{
						dropObj = plr;
						distance = d;
					}
				}
			}
		}
	}

	static float pressedKeyTimer = 0.0f;
	if(distance < 1.0f) // distance to ray. if more than 1, then it is outside of ray
	{
		// in case if you are standing right next to pile of objects, we need to re-init ui each time we pick different item from pile
		static GameObject* prevHUDItem = NULL;
		if(prevHUDItem != dropObj && hudActionUI->isActive())
			hudActionUI->Deactivate();
		prevHUDItem = dropObj;

		if(!hudActionUI->isActive() )
		{
			hudActionUI->Activate();

			if(dropObj->isObjType(OBJTYPE_SharedUsableItem))
				hudActionUI->setText(((SharedUsableItem*)dropObj)->m_ActionUI_Title, ((SharedUsableItem*)dropObj)->m_ActionUI_Msg, InputMappingMngr->getKeyName(r3dInputMappingMngr::KS_INTERACT));
			else // human
				hudActionUI->setText(L"USE ITEM", m_Weapons[m_SelectedWeapon]->getConfig()->m_StoreNameW, InputMappingMngr->getKeyName(r3dInputMappingMngr::KS_INTERACT));
			hudActionUI->enableRegularBlock();
		}
		if(hudActionUI->isActive())
		{
			r3dPoint3D scrPos;
			r3dProjectToScreen(dropObj->GetBBoxWorld().Center(), &scrPos);
			hudActionUI->setScreenPos((int)scrPos.x, (int)scrPos.y);
		}

		if(InputMappingMngr->isPressed(r3dInputMappingMngr::KS_INTERACT))
		{
			hudActionUI->enableProgressBlock();
			hudActionUI->setProgress(int(pressedKeyTimer*100));
			pressedKeyTimer += r3dGetFrameTime();
			// send request to pickup weapon
			if(pressedKeyTimer > 1.0f) 
			{
				pressedKeyTimer = 0.0f;
				if(dropObj->isObjType(OBJTYPE_SharedUsableItem))
					((SharedUsableItem*)dropObj)->m_lastPickupAttempt = curTime+1.0f; // do not attempt to pick this item for 1 sec (wait for server)
				// hack for notes
				if(dropObj->Class->Name == "obj_Note" || dropObj->Class->Name == "obj_PermanentNote")
				{
					obj_Note* note = (obj_Note*)dropObj;
					if(note->m_GotData) // do not send packet to server again
					{
						hudMain->showReadNote(note->m_Text.c_str());
						hudActionUI->Deactivate();
						return;
					}
				}

				if(dropObj->isObjType(OBJTYPE_NPC))
				{
					((NPCObject*)dropObj)->OnAction();
				}
				else if(dropObj->isObjType(OBJTYPE_SharedUsableItem))
				{
					PKT_C2S_UseNetObject_s n;
					n.spawnID = toP2pNetId(dropObj->GetNetworkID());
					p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
				}
				else // human
				{
					DWORD netID = dropObj->GetNetworkID();
					wiInventoryItem& wi = CurLoadout.Items[m_Weapons[m_SelectedWeapon]->m_BackpackIdx];
					localPlayer_UseItem(m_Weapons[m_SelectedWeapon]->m_BackpackIdx, m_Weapons[m_SelectedWeapon]->getItemID(), dropObj->GetPosition(), 0.0f, 0.0f, 0.0f, netID);
					if(--wi.quantity <= 0) {
						wi.Reset();
						OnBackpackChanged(m_Weapons[m_SelectedWeapon]->m_BackpackIdx);
					}
					else
					{
						if(hudMain)
							hudMain->updateSlotInfo(m_Weapons[m_SelectedWeapon]->m_BackpackIdx, wi.quantity);
					}
				}

				hudActionUI->Deactivate();
			}
		}
		else
		{
			hudActionUI->enableRegularBlock();
			pressedKeyTimer = 0.0f;
		}
	}
	else
	{
		if(hudActionUI->isActive())
		{
			hudActionUI->Deactivate();
		}
		pressedKeyTimer = 0.0f;
	}
}

void obj_Player::AddBloodOnGround(const r3dPoint3D& posOfShot)
{
	{
		r3dPoint3D blood_pos;
		r3dPoint3D blood_norm;
		bool has_collision = false;
		{
			PxRaycastHit hit;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
			if(g_pPhysicsWorld->raycastSingle(PxVec3(posOfShot.x, posOfShot.y, posOfShot.z), PxVec3(0, -1, 0), 10.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eNORMAL), hit, filter))
			{
				blood_pos = r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z);
				blood_norm = r3dPoint3D(hit.normal.x, hit.normal.y, hit.normal.z);
				has_collision = true;
			}
			else
			{
				//WTF??? How no collision? Is player up in the sky??
			}
		}
		if(has_collision)
		{
			DecalParams params;
			params.Pos		= blood_pos;
			params.Dir		= blood_norm;
			params.TypeID	= GetDecalID( r3dHash::MakeHash("default"), r3dHash::MakeHash("blood_ground") );
			if( params.TypeID != INVALID_DECAL_ID )
				g_pDecalChief->Add( params );
		}
	}
}

void obj_Player::OnNetPacket(const PKT_S2C_MoveTeleport_s& n)
{
	TeleportPlayer(n.teleport_pos, "PlayerMoveTeleport");
}

void obj_Player::OnNetPacket(const PKT_C2C_MoveSetCell_s& n)
{
	netMover.SetCell(n);
}

void obj_Player::OnNetPacket(const PKT_C2C_MoveRel_s& n)
{
	const CNetCellMover::moveData_s& md = netMover.DecodeMove(n);

	if(afterRespawnTimer>0) // check for old player moves and discard them
	{
		if((md.pos - GetPosition()).Length() > 10.0f)
			return;
	}

	m_fPlayerRotationTarget = md.turnAngle;
	bodyAdjust_y[1]         = md.bendAngle;
	PlayerState             = (md.state & 0xF);
	PlayerMoveDir           = (md.state >> 4) & 0xF;

	if(PlayerState == PLAYER_IDLE) {
		// limit idle bend angle to some reasonable value in IDLE mode - by some reason animation look like crap when bending there.
		bodyAdjust_y[1] = R3D_CLAMP(bodyAdjust_y[1], -0.4f, 0.4f);
	}

	// calc velocity to reach position on time for next update
	r3dPoint3D vel = netMover.GetVelocityToNetTarget(
		GetPosition(),
		GPP->AI_SPRINT_SPEED * 1.5f,
		1.0f);

	SetVelocity(vel);
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerJump_s& n)
{
	uberAnim_->StartJump();
	return;
}

void obj_Player::ProcessRewardQueue()
{
	r3d_assert(NetworkLocal);

	if(rewards_.size() == 0)
		return;

	// one reward show per sec
	const float curTime = r3dGetTime();
	if(curTime < lastRewardShowTime_ + 1.0f)
		return;
	lastRewardShowTime_ = curTime;

	PKT_S2C_AddScore_s n = *rewards_.begin();
	rewards_.erase(rewards_.begin());

	if((n.ID & 0x8000) == 0)
	{
		GameStats.XP += n.XP;
		GameStats.GD += n.GD;
	}
	else
	{
		// reward without adding score/gd
		n.ID &= ~0x8000;
	}

	if(!g_GameRewards) // can happen in server quickjoin from Main_Network
		return;

	const CGameRewards::rwd_s& rwd = g_GameRewards->GetRewardData(n.ID);

	char rewardName[128] = "";	// localized reward name
	char iconName[128] = "";
	if(rwd.IsSet)
	{
		sprintf(rewardName, "Reward_%s", rwd.Name.c_str());
		sprintf(iconName, "$Data/Menu/Rewards/RWD_%s.png", rwd.Name.c_str());
	}
	else
	{
		sprintf(rewardName, "RWD_%d", n.ID);
	}

	// and always show some crap about that reward
	const wchar_t* rewardString=gLangMngr.getString(rewardName);
	//	hudMain->ShowAchievementCustom(rewardString, "", iconName, "");

	//	hudMain->ShowScore(n.XP, 0, n.GD, GameStats.GD, GameStats.XP);

	wchar_t tmpStr[64];
	swprintf(tmpStr, 64, gLangMngr.getString("InfoMsg_XPAdded"), n.XP);
	hudMain->showMessage(tmpStr);
}

void obj_Player::OnNetPacket(const PKT_S2C_AddScore_s& n)
{
	if(NetworkLocal)
	{
		rewards_.push_back(n);
	}
	else
	{
		GameStats.XP += n.XP;
		GameStats.GD += n.GD;
	}
}

void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerVitals_s& n)
{
	CurLoadout.Health = (float)n.Health;
	CurLoadout.Thirst = (float)n.Thirst;
	CurLoadout.Hunger = (float)n.Hunger;
	CurLoadout.Toxic  = (float)n.Toxic;

	//if(NetworkLocal)
	//	r3dOutToLog("My params: %.1f, %.1f, %.1f, %.1f\n", CurLoadout.Health, CurLoadout.Toxic, CurLoadout.Hunger, CurLoadout.Thirst);
}

void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerLoadout_s& n)
{
	// only remote players can change loadout this way
	if(NetworkLocal)
		return;

	// create new loadout
	wiCharDataFull slot = CurLoadout;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1 ].itemID = n.WeaponID0;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2 ].itemID = n.WeaponID1;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM1   ].itemID = n.QuickSlot1;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM2   ].itemID = n.QuickSlot2;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM3   ].itemID = n.QuickSlot3;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM4   ].itemID = n.QuickSlot4;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_ARMOR   ].itemID = n.ArmorID;
	slot.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID = n.HeadGearID;
	slot.BackpackID = n.BackpackID;

	UpdateLoadoutSlot(slot);
}

void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerAttachments_s& n)
{
	// only remote players can change loadout this way
	if(NetworkLocal)
		return;

	r3d_assert(n.wid == wiCharDataFull::CHAR_LOADOUT_WEAPON1 || n.wid == wiCharDataFull::CHAR_LOADOUT_WEAPON2);
	Weapon* wpn = m_Weapons[n.wid];
	if(!wpn) {
		return;
	}

	// set wpn attachment
	CurLoadout.Attachment[n.wid].attachments[WPN_ATTM_MUZZLE]    = n.Attm.MuzzleID;
	CurLoadout.Attachment[n.wid].attachments[WPN_ATTM_LEFT_RAIL] = n.Attm.LeftRailID;
	wpn->setWeaponAttachmentsByIDs(CurLoadout.Attachment[n.wid].attachments);

	// workaround to enable flashlight particle (OnEquip will enable flashlight light)!
	// also we assume that remote player can change attachment only for currently equipped weapon
	wpn->OnUnequip();
	wpn->OnEquip();
}

void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerWorldFlags_s& n)
{
	CurLoadout.GameFlags = n.flags;
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerSwitchWeapon_s& n)
{
	r3d_assert(!NetworkLocal);

	if(n.wid == 255)
	{
		switchToEmptyHands();
		return;
	}

	r3d_assert(m_Weapons[n.wid]);

	if(m_Weapons[n.wid] == NULL)
	{
#ifndef FINAL_BUILD
		// do not crash in public build
		char plrUserName[64]; GetUserName(plrUserName);
		r3dOutToLog("NET_DEBUG: %s tried to change to empty weapon slot %d", plrUserName, n.wid);
#endif
	}
	else
	{
		uberAnim_->StopGrenadeAnimations();	// we now can switch weapon while grenade animation active, so need to abort

		ChangeWeaponByIndex(n.wid);
		SyncAnimation(true);
	}
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerReadyGrenade_s& n)
{
	//r3dOutToLog("PKT_C2C_PlayerReadyGrenade\n");
	m_SelectedWeapon = n.wid;
	r3d_assert(m_SelectedWeapon>=0 && m_SelectedWeapon < NUM_WEAPONS_ON_PLAYER);
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	uberAnim_->StartGrenadePinPullAnimation();
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerThrewGrenade_s& n)
{
	m_SelectedWeapon = n.debug_wid;
	r3d_assert(m_SelectedWeapon>=0 && m_SelectedWeapon < NUM_WEAPONS_ON_PLAYER);
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	D3DXMATRIX fireFrom ;
	fireFrom = DrawRotMatrix ;

	fireFrom._41 = n.fire_from.x ;
	fireFrom._42 = n.fire_from.y ;
	fireFrom._43 = n.fire_from.z ;

	// get the weapon bone.  Fire requires it. 
	D3DXMATRIX weaponBone = uberEquip_->getWeaponBone(GetSkeleton(),fireFrom);

	// remove translation, the rotation is all we wanted for grenades.
	weaponBone._41 = n.fire_from.x ;
	weaponBone._42 = n.fire_from.y ;
	weaponBone._43 = n.fire_from.z ;

	m_Weapons[m_SelectedWeapon]->Fire(n.fire_to, weaponBone, true, n.holding_delay, n.fire_from);

	uberAnim_->StartGrenadeThrowAnimation();
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerReload_s& n)
{
	r3d_assert(!NetworkLocal);

	Weapon* wpn = m_Weapons[n.WeaponSlot];
	if(!wpn) return;

	wpn->StartReloadSequence();
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerFired_s& n)
{
	m_SelectedWeapon = n.debug_wid;
	r3d_assert(m_SelectedWeapon>=0 && m_SelectedWeapon < NUM_WEAPONS_ON_PLAYER);
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	D3DXMATRIX fireFrom ;
	fireFrom = DrawRotMatrix ;

	fireFrom._41 = n.fire_from.x ;
	fireFrom._42 = n.fire_from.y ;
	fireFrom._43 = n.fire_from.z ;

	// get the weapon bone.  Fire requires it. 
	D3DXMATRIX weaponBone = uberEquip_->getWeaponBone(GetSkeleton(),fireFrom);

	if( m_Weapons[m_SelectedWeapon]->getCategory() == storecat_GRENADE ) {
		// remove translation, the rotation is all we wanted for grenades.
		weaponBone._41 = n.fire_from.x ;
		weaponBone._42 = n.fire_from.y ;
		weaponBone._43 = n.fire_from.z ;
	}

	m_Weapons[m_SelectedWeapon]->Fire(n.fire_to, weaponBone, n.execWeaponFire?true:false, n.holding_delay, n.fire_from);

	uberAnim_->StartRecoilAnim();
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerHitNothing_s& n)
{
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerHitStatic_s& n)
{
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	const ClientGameLogic& CGL = gClientLogic();

	// create decal
	if(n.decalIdx != 0 )
	{
		DecalParams params;
		params.Dir	= n.hit_norm;
		params.Pos	= n.hit_pos;
		params.TypeID	= (uint32_t)(n.decalIdx) - 1;
		if(params.TypeID >= 0 && params.TypeID < (int)g_pDecalChief->GetTypeCount())
		{
			g_pDecalChief->Add( params );
		}
		else
		{
			// some cheat or invalid data
		}
	}

	// impact particle
	if(n.particleIdx != 0)
	{
		uint32_t particleIdx = (uint32_t)(n.particleIdx) - 1;
		if(particleIdx < ImpactParticleEntry::NumRegistered)
		{
			SpawnImpactParticle(particleIdx, n.hit_pos, n.hit_norm);
		}
		else
		{
			// some cheat or invalid data
		}
	}

	if(GameObject* from = GameWorld().GetNetworkObject(n.FromID))
	{
		r3dPoint3D waterSplashPos;
		extern bool TraceWater(const r3dPoint3D& start, const r3dPoint3D& finish, r3dPoint3D& waterSplashPos);
		if( TraceWater( from->GetPosition(), n.hit_pos, waterSplashPos))
		{	
			extern void WaterSplash(const r3dPoint3D& waterSplashPos, float height, float size, float amount, int texIdx);
			waterSplashPos.y -= 0.1f;
			WaterSplash(waterSplashPos, 0.1f, 30.0f, 0.04f, 2);
		}
	}

	// add impulse
	GameObject* target = GameWorld().GetObjectByHash(n.hash_obj);
	if(m_Weapons[m_SelectedWeapon]->isImmediateShooting() && target)
		if(target->PhysicsObject)
			target->PhysicsObject->AddImpulseAtPos(-n.hit_norm*m_Weapons[m_SelectedWeapon]->getAmmoSpeed()*m_Weapons[m_SelectedWeapon]->getAmmoMass()/150.0f, n.hit_pos);

	// play near miss sound
	if(m_Weapons[m_SelectedWeapon]->isImmediateShooting())
	{
		if(CGL.localPlayer_)
		{
			float dist = dist_Point_to_Line(CGL.localPlayer_->GetPosition(), GetPosition(), n.hit_pos);
			if(dist < 5.0f && rand()%5==0)
			{
				static int bulletHitMissSoundID = -1;
				if(bulletHitMissSoundID==-1)
					bulletHitMissSoundID = SoundSys.GetEventIDByPath("Sounds/BulletHits/Miss");
				SoundSys.PlayAndForget(bulletHitMissSoundID, CGL.localPlayer_->GetPosition());
			}
		}
	}
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerHitStaticPierced_s& n)
{
	PKT_C2C_PlayerHitStatic_s n2;
	r3d_assert(sizeof(n) == sizeof(n2));
	memcpy(&n2, &n, sizeof(n));
	OnNetPacket(n2);
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerHitDynamic_s& n)
{
	r3d_assert(m_Weapons[m_SelectedWeapon]);

	r3dPoint3D dir = (GetPosition()-n.hit_pos).NormalizeTo();

	GameObject* target = GameWorld().GetNetworkObject(n.targetId);
	if(!target) // current player doesn't see target
		return;

	const ClientGameLogic& CGL = gClientLogic();

	// if we hit local player, display hit effect
	if(target->isObjType(OBJTYPE_Human))
	{
		AddBloodOnGround(n.hit_pos);
		SpawnImpactParticle(r3dHash::MakeHash("player"), r3dHash::MakeHash(m_Weapons[m_SelectedWeapon]->getDecalSource()), n.hit_pos, dir);

		if(target->NetworkLocal == false || CGL.localPlayer_==NULL) 
			return;

		// if hit local player
		extern void TPSGameHUD_AddHitEffect(GameObject* from);
		TPSGameHUD_AddHitEffect(this);
	}
	else if(target->isObjType(OBJTYPE_Zombie))
	{
		// hit zombie
		AddBloodOnGround(n.hit_pos);
		SpawnImpactParticle(r3dHash::MakeHash("zombie"), r3dHash::MakeHash(m_Weapons[m_SelectedWeapon]->getDecalSource()), n.hit_pos, dir);

		obj_Zombie* z = (obj_Zombie*)target;

		// apply ragdoll force from object position, or up if too close
		r3dPoint3D zragHitPos = z->GetPosition();
		r3dPoint3D zlastTimeHitForce = r3dPoint3D(0, 1, 0);
		if(n.hit_body_bone < GetSkeleton()->NumBones) // zombies use same skeleton as player
			zragHitPos = uberEquip_->getBonePos(n.hit_body_bone, GetSkeleton(), z->GetTransformMatrix());
		if((zragHitPos - this->GetPosition()).LengthSq() > 0.001f)
			zlastTimeHitForce = (zragHitPos - this->GetPosition()).NormalizeTo();

		z->lastTimeHitPos  = zlastTimeHitForce;
		z->lastTimeDmgType = m_Weapons[m_SelectedWeapon]->getCategory();
		z->lastTimeHitBoneID = n.hit_body_bone;
		z->PlayHurtSound();
	}
	else // hit something else, for now that is UAV only
	{
		SpawnImpactParticle(r3dHash::MakeHash("Metal"),r3dHash::MakeHash(m_Weapons[m_SelectedWeapon]->getDecalSource()), n.hit_pos, dir);
	}
}

void obj_Player::OnNetPacket(const PKT_S2C_SpawnExplosion_s& n)
{
	GameObject* from = GameWorld().GetNetworkObject(n.FromID);
	if(from)
	{
		// add decal
		DecalParams params;
		params.Dir		= r3dPoint3D(0,1,0);
		params.Pos		= from->GetPosition();
		params.TypeID	= GetDecalID( r3dHash::MakeHash(""), r3dHash::MakeHash("grenade") );
		if( params.TypeID != INVALID_DECAL_ID )
			g_pDecalChief->Add( params );
		SpawnImpactParticle(r3dHash::MakeHash(""), r3dHash::MakeHash("grenade"), from->GetPosition(), r3dPoint3D(0,1,0));

		//	Start radial blur effect
		gExplosionVisualController.AddExplosion(from->GetPosition(), n.radius);
	}
}

void obj_Player::OnNetPacket(const PKT_C2C_PlayerUseItem_s& n)
{
	// assumption: current weapon is set to whatever player is about to use so just play it's animation.
	if(m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->isUsableItem())
		uberAnim_->StartShootAnim();
}

void obj_Player::OnNetPacket(const PKT_S2C_PlayerUsedItemAns_s& n)
{
	// ptumik: server isn't sending this packet atm
	/*if(n.itemId == WeaponConfig::ITEMID_Bandages || n.itemId == WeaponConfig::ITEMID_Bandages2 || n.itemId == WeaponConfig::ITEMID_MedKit)
	{
	m_BandagesEffectTime = r3dGetTime() + 0.5f;
	}*/
}

void obj_Player::OnNetPacket(const PKT_S2C_BackpackAddNew_s& n)
{
	// NOTE: only for local players
	r3d_assert(NetworkLocal);

	if(n.Item.itemID == 'GOLD')
	{
		gUserProfile.ProfileData.GameDollars += n.Item.quantity;
		wchar_t tmpStr[64];
		swprintf(tmpStr, 64, gLangMngr.getString("InfoMsg_DollarsPickedUp"), n.Item.quantity);
		hudMain->showMessage(tmpStr);
		return;
	}

	r3d_assert(CurLoadout.Items[n.SlotTo].itemID == 0);
	CurLoadout.Items[n.SlotTo] = n.Item;

	OnBackpackChanged(n.SlotTo);

	hudMain->showMessage(gLangMngr.getString("InfoMsg_NewItemAdded"));
}

void obj_Player::OnNetPacket(const PKT_S2C_BackpackModify_s& n)
{
	// NOTE: only for local players
	r3d_assert(NetworkLocal);

	if(n.SlotTo == 0xFF)
	{
		hudMain->showMessage(gLangMngr.getString("InfoMsg_NoFreeBackpackSlots"));
		return;
	}

	if(n.SlotTo == 0xFE)
	{
		hudMain->showMessage(gLangMngr.getString("InfoMsg_TooMuchWeight"));
		return;
	}

	r3d_assert(n.SlotTo < wiCharDataFull::CHAR_MAX_BACKPACK_SIZE);
	wiInventoryItem& wi = CurLoadout.Items[n.SlotTo];
	r3d_assert(wi.itemID == n.dbg_ItemID);

	if(wi.quantity < n.Quantity)
		hudMain->showMessage(gLangMngr.getString("InfoMsg_ItemAdded"));
	else
		hudMain->showMessage(gLangMngr.getString("InfoMsg_ItemRemoved"));

	wi.quantity = n.Quantity;
	if(wi.quantity <= 0)
		wi.Reset();

	OnBackpackChanged(n.SlotTo);
}

void obj_Player::OnNetPacket(const PKT_S2C_SetPlayerReputation_s& n)
{
	if (NetworkLocal && hudMain)
	{
		const int diff = n.Reputation - CurLoadout.Stats.Reputation;
		if (diff != 0)
		{
			wchar_t tmpStr[64];
			swprintf(tmpStr, 64, gLangMngr.getString("InfoMsg_ReputationAdded"), diff);
			hudMain->showMessage(tmpStr);
		}
	}
	CurLoadout.Stats.Reputation = n.Reputation;
}

BOOL obj_Player::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	R3DPROFILE_FUNCTION("obj_Player::OnNetReceive");
	r3d_assert(!(ObjFlags & OBJFLAG_JustCreated)); // make sure that object was actually created before processing net commands

#undef DEFINE_GAMEOBJ_PACKET_HANDLER
#define DEFINE_GAMEOBJ_PACKET_HANDLER(xxx) \
case xxx: { \
	const xxx##_s&n = *(xxx##_s*)packetData; \
	r3d_assert(packetSize == sizeof(n)); \
	OnNetPacket(n); \
	return TRUE; \
	}

	switch(EventID)
	{
	default: return FALSE;
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_MoveTeleport);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveSetCell);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveRel);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerJump);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_AddScore);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerVitals);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerLoadout);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerAttachments);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerWorldFlags);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerSwitchWeapon);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerReadyGrenade);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerThrewGrenade);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerReload);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerFired);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitNothing);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitStatic);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitStaticPierced);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitDynamic);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SpawnExplosion);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerUseItem);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_PlayerUsedItemAns);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_BackpackAddNew);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_BackpackModify);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_SetPlayerReputation);
	}

	return TRUE;
}

bool obj_Player::hasScopeMode() const 
{
	if(bDead)
		return false;
	if(forcedEmptyHands)
		return false;

	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	return (m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->hasScopeMode(isFirstPerson)); 
}

bool obj_Player::hasScopeMode_hidePlayerModel() const 
{
	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	if(m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getScopeConfig(isFirstPerson))
		return m_Weapons[m_SelectedWeapon]->getScopeConfig(isFirstPerson)->hide_player_model;

	return false;
}

void obj_Player::DrawReticle()
{
	if(!NetworkLocal)
		return;

	g_RenderScopeEffect = 0;

	if(bDead || gClientLogic().disconnectStatus_ || (m_Weapons[m_SelectedWeapon] == NULL && !(r_hud_filter_mode->GetInt()==HUDFilter_NightVision)))
	{
		CurrentRig.allowScope = false; // to make sure that scope will not be rendered when you are dead
		g_ScopeFeedbackTimer = 0.0f;
		return;
	}

	bool isFirstPerson = g_camera_mode->GetInt()==2 && NetworkLocal;
	r3dRenderer->SetRenderingMode(R3D_BLEND_PUSH|R3D_BLEND_NZ|R3D_BLEND_ALPHA);

	float cx = r3dRenderer->ScreenW/2;
	float cy = r3dRenderer->ScreenH/2;
	float cy2 = r3dRenderer->ScreenH*0.32f;

	// draw laser pointer if any
	if(m_Weapons[m_SelectedWeapon] && !forcedEmptyHands)
	{
		if(PlayerState != PLAYER_MOVE_SPRINT && !m_Weapons[m_SelectedWeapon]->isReloading() && g_camera_mode->GetInt() == 2 && !(hudAttm && hudAttm->isActive()))
		{
			r3dPoint3D laserPos;
			if(m_Weapons[m_SelectedWeapon]->hasLaserPointer(laserPos))
			{
				r3dRenderer->SetRenderingMode( R3D_BLEND_PUSH | R3D_BLEND_ADD | R3D_BLEND_ZC );

				float size  = R3D_MIN(r3dRenderer->ScreenW*0.02f, r3dRenderer->ScreenH*0.02f);

				// gradually decrease size over distance
				const float maxDecDist = 20.0f;
				float dist = (gCam - laserPos).Length();
				if(dist >= maxDecDist) dist = maxDecDist;
				size = R3D_LERP(size, size*0.25f, dist / maxDecDist);

				r3dPoint3D laserPosScreen;
				r3dProjectToScreen(laserPos, &laserPosScreen);
				if(m_isAiming)
				{
					laserPosScreen.x = cx;
					laserPosScreen.y = cy;
				}
				r3dColor laserColor = r3dColor::red;
				laserColor.A = (int)R3D_LERP(196.0f, 0.0f, dist/400.0f);
				r3dDrawBox2D(float(laserPosScreen.x-(size/2)), float(laserPosScreen.y-(size/2)), size, size, laserColor, s_laserPointerTex);

				r3dRenderer->SetRenderingMode( R3D_BLEND_POP );
			}
		}
	}

	// check if blinded by flash light or laser
	{
		// check all other players, see if they are close enough, have attachment and pointing it at you
		ClientGameLogic& CGL = gClientLogic();
		for(int i=0; i<CGL.MAX_NUM_PLAYERS; ++i)
		{
			obj_Player* plr = CGL.GetPlayer(i);
			if(g_bEditMode)
			{
				plr = this;
				i = CGL.MAX_NUM_PLAYERS;
			}
			if(!plr)
				continue;
			if(plr == this && !g_bEditMode)
				continue;
			if( !(plr->m_SelectedWeapon >= 0 && plr->m_SelectedWeapon <=NUM_WEAPONS_ON_PLAYER) )
				continue;
			if(plr->m_Weapons[plr->m_SelectedWeapon] == NULL)
				continue;
			if(plr->forcedEmptyHands)
				continue;
			int attmID = plr->m_Weapons[plr->m_SelectedWeapon]->getWeaponAttachmentID(WPN_ATTM_LEFT_RAIL);
			bool hasFlashlight = attmID == 400018 || attmID == 400022 || plr->m_Weapons[plr->m_SelectedWeapon]->getItemID()==101306;
			bool hasLaser = attmID == 400004 || attmID == 400021;

			if( hasFlashlight || hasLaser ) // laser, flashlight
			{
				float dist = (plr->GetPosition() - gCam).Length();

				// other players do not have FPS model, so just use muzzle pos
				D3DXMATRIX weaponBone = plr->uberEquip_->getWeaponBone(plr->GetSkeleton(), plr->DrawFullMatrix);
				r3dPoint3D attmPos = plr->m_Weapons[plr->m_SelectedWeapon]->getMuzzlePos(weaponBone);
				{
					// check if attachment is visible
					r3dPoint3D attmScreenPos;
					if(!r3dProjectToScreen(attmPos, &attmScreenPos))
						continue;

					r3dPoint3D attmDir = r3dPoint3D(weaponBone._11, weaponBone._12, weaponBone._13);
					if(attmDir.Dot(gCam.vPointTo)>0.0f)
						continue;
					float distToCamera = (attmPos - gCam).Length();

					// check that attachment is visible and not blocked
					PxRaycastHit hit;
					PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER),0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));
					r3dPoint3D dirToAttm = (attmPos - gCam).NormalizeTo();
					bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(gCam.x, gCam.y, gCam.z), PxVec3(dirToAttm.x, dirToAttm.y, dirToAttm.z), distToCamera, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
					if(!hitResult)
					{
						/*{
						float size = r3dRenderer->ScreenH*0.1f;
						r3dDrawBox2DRotate(attmScreenPos.x, attmScreenPos.y, size, size, 0.0f, r3dColor::white, s_lightHaloTex);
						}*/

						float viewDot = -attmDir.Dot(dirToAttm);
						if(hasFlashlight) // flashlight
						{
							if(viewDot < 0.75f)
								viewDot = 0;
							else
								viewDot = viewDot*4-3.0f;
						}
						else // laser
						{
							if(viewDot < 0.997f)
								viewDot = 0;
							else
								viewDot = ((viewDot*100.0f)-99.7f); // convert from [0.99, 1.0] to [0, 1]
						}

						{
							r3dTexture* tex = (hasLaser)?s_laserFlashTex:s_flashlightFlashTex;
							float size = R3D_MIN(r3dRenderer->ScreenW, r3dRenderer->ScreenH);
							float scale = viewDot*0.5f;
							if(distToCamera < 10.0f)
								scale = scale * (1.0f+2.0f*R3D_CLAMP((distToCamera/10.0f), 0.0f, 1.0f));
							else if(attmID == 400018 || attmID == 400022) // flashlight, max range 20m
								scale = scale * (3.0f-(3*(R3D_CLAMP((distToCamera-10.0f)/10.0f, 0.0f, 0.95f))));
							else // laser, max range 40m
								scale = scale * (3.0f-(3*(R3D_CLAMP((distToCamera-10.0f)/30.0f, 0.0f, 1.0f))));
							size = size * scale;
							r3dDrawBox2DRotate(attmScreenPos.x, attmScreenPos.y, size, size, r3dSin(r3dGetTime()*0.01f)*360.0f, r3dColor::white, tex);
							r3dDrawBox2DRotate(attmScreenPos.x, attmScreenPos.y, size, size, r3dCos(r3dGetTime()*0.012f)*360.0f, r3dColor::white, tex);
						}
					}
				}
			}
		}
	}
	bool scopeRendering = hasScopeMode() && CurrentRig.allowScope;

	const ScopeConfig* scopeConfig = NULL;
	if(m_Weapons[m_SelectedWeapon])
		scopeConfig = m_Weapons[m_SelectedWeapon]->getScopeConfig(isFirstPerson);

	if(r_hud_filter_mode->GetInt()==HUDFilter_NightVision && !scopeRendering)// nightvision
	{
		scopeConfig = g_pWeaponArmory->getScopeConfig("nvgframe");
		scopeRendering = true;
	}

	r3d_assert(scopeConfig);

	if(scopeRendering) 	// draw sniper scope
	{
		g_RenderScopeEffect = 1;
		gPFX_ScopeEffect.SetScopeReticle(scopeConfig->scope_reticle);
		gPFX_ScopeEffect.SetScopeNormal(scopeConfig->scope_normal);
		gPFX_ScopeEffect.SetScopeMask(scopeConfig->scope_mask);
		gPFX_ScopeEffect.SetScopeBlurMask(scopeConfig->scopeBlur_mask);

		ScopeMaterialParams params = gPFX_ScopeEffect.GetMaterialParams() ;
		params.Lighting = scopeConfig->lighting ;
		gPFX_ScopeEffect.SetMaterialParams( params ) ;

		float scaleScope = scopeConfig->scale;
		float scopeScaleMod = 0.20f;
		if(g_camera_mode->GetInt() == 2)
			scopeScaleMod = 0.10f;

		scaleScope += g_ScopeFeedbackTimer*scopeScaleMod;
		if(g_ScopeFeedbackTimer>0)
		{
			g_ScopeFeedbackTimer = R3D_MAX(g_ScopeFeedbackTimer-r3dGetFrameTime()*8.0f, 0.0f);
		}
		gPFX_ScopeEffect.SetScopeScale(scaleScope);
	}
#ifndef FINAL_BUILD
	else if(g_bEditMode && r_show_player_debug_data->GetBool() && scopeConfig->reticule) // draw reticule only in editor, in game reticule is in flash
	{
		float rw = (float)scopeConfig->reticule->GetWidth();
		float rh = (float)scopeConfig->reticule->GetHeight();
		float yy = g_camera_mode->GetInt() == 1?cy2:cy;
		r3dDrawBox2D(float(cx-(rw/2)), float(yy-(rh/2)), rw, rh, r3dColor::white, scopeConfig->reticule);
	}
#endif

	if(!scopeRendering)
		g_ScopeFeedbackTimer = 0.0f;

	if(m_HitMarkerFadeout>0)
	{
		float rw = (float)m_HitMarkerTex->GetWidth();
		float rh = (float)m_HitMarkerTex->GetHeight();
		r3dColor clr = r3dColor::white; clr.A = BYTE(m_HitMarkerFadeout*255.0f);
		if(m_isInScope)
			r3dDrawBox2D(float(cx-(rw/2)), float(cy-(rh/2)), rw, rh, r3dColor::white, m_HitMarkerTex);
		else
		{
			float yy = g_camera_mode->GetInt() == 1?cy2:cy;
			r3dDrawBox2D(float(cx-(rw/2)), float(yy-(rh/2)), rw, rh, r3dColor::white, m_HitMarkerTex);
		}
	}

	if(r3dGetTime() < m_BandagesEffectTime)
	{
		// !! hardcoded for 0.5 duration
		float time = m_BandagesEffectTime - r3dGetTime(); // values: 0.5...0.0
		float infl = 0;
		time -= 0.25f;  // 0.25...-0.25
		time *= 4.0f; // 1.0...-1.0
		time = R3D_ABS(time); // 1.0...0.0...1.0
		time = 1.0f-time; // 0.0...1.0...0.0
		time = pow(time, 2.0f); // curve
		infl = R3D_CLAMP(time, 0.0f, 1.0f);

		r3dColor24 effectColor(153, 217, 255, 196);
		effectColor = effectColor * infl;
		r3dDrawBox2D(0, 0, r3dRenderer->ScreenW, r3dRenderer->ScreenH, effectColor);
	}

	r3dRenderer->SetRenderingMode(R3D_BLEND_POP);
}

void obj_Player::DrawLabel()
{
	r3dRenderer->SetMaterial(NULL);

	struct PushRestoreStates
	{
		PushRestoreStates()
		{
			r3dRenderer->SetRenderingMode( R3D_BLEND_ALPHA | R3D_BLEND_NZ | R3D_BLEND_PUSH );
		}

		~PushRestoreStates()
		{
			r3dRenderer->SetRenderingMode( R3D_BLEND_POP );
		}
	} pushRestoreStates; (void)pushRestoreStates;

	if(NetworkLocal && gClientLogic().localPlayer_)
	{
		// show a warning if FPS or PING is low
		float fps = r3dGetAvgFPS();
		if(fps < 20.0f)
			Font_Label->PrintF(r3dRenderer->ScreenW-80, 0, r3dColor(243,43,37), "FPS: %.1f", fps);
	}

#ifndef FINAL_BUILD
	// draw player CustomerID/gamertag for admins
	if(!NetworkLocal && gUserProfile.ProfileData.isDevAccount && Keyboard->IsPressed(kbsLeftAlt))
	{
		float dist = (gCam - GetPosition()).Length();
		int a    = int(R3D_MAX(1.0f - (dist / 300.0f), 0.1f)*255.0f);
		r3dPoint3D scrCoord;
		if(r3dProjectToScreen(GetPosition() + r3dPoint3D(0, 1.8f, 0), &scrCoord))
		{
			char plrUserName[64]; GetUserName(plrUserName);
			Font_Editor->PrintF(scrCoord.x, scrCoord.y+12, r3dColor(255,255,255,a), "ID:%d", CustomerID);
			Font_Editor->PrintF(scrCoord.x, scrCoord.y,    r3dColor(255,255,255,a), "%s", plrUserName);
		}
	}
#endif

	/*
	{
	// muzzler position debug
	D3DXMATRIX mm = uberEquip_->getWeaponBone(GetSkeleton(),GetPosition());
	r3dPoint3D pos = m_Weapons[m_SelectedWeapon]->getMuzzlePos(mm);
	r3dDrawLine3D(pos, pos + r3dPoint3D(mm._11, mm._12, mm._13), gCam, 0.005f, r3dColor(255, 255, 255));
	r3dDrawLine3D(pos, pos + r3dPoint3D(0, 1, 0), gCam, 0.005f, r3dColor(255, 255, 255));

	// r3dDrawLine3D(gTempTargetPos, gTempTargetPos + r3dPoint3D(mm._11, mm._12, mm._13), gCam, 0.005f, r3dColor(255, 0, 0));
	// r3dDrawLine3D(gTempTargetPos, gTempTargetPos + r3dPoint3D(0, 1, 0), gCam, 0.005f, r3dColor(255, 0, 0));

	// r3dDrawLine3D(gTempTargetPos, gTempMuzzlePos, gCam, 0.005f, r3dColor(0,255,0));
	r3dRenderer->Flush();
	}
	*/

#ifndef FINAL_BUILD
	if(NetworkLocal && r_show_player_debug_data->GetBool() && m_Weapons[m_SelectedWeapon])
	{
		Font_Label->PrintF(10, r3dRenderer->ScreenH-240,r3dColor(255,255,255), "m_Health %.2f", CurLoadout.Health);
		Font_Label->PrintF(10, r3dRenderer->ScreenH-220,r3dColor(255,255,255), "DmgRcvd %.2f", lastDamageRcvd);
		Font_Label->PrintF(10, r3dRenderer->ScreenH-200,r3dColor(255,255,255), "AccumDmgRcvd %.2f", accumLastDamageRcvd);
		Font_Label->PrintF(10, r3dRenderer->ScreenH-180,r3dColor(255,255,255), "Recoil %.2f, %.2f", RecoilViewModTarget.x, RecoilViewModTarget.y);

		if(Keyboard->WasPressed(kbsO))
			accumLastDamageRcvd = 0;

		/*Font_Label->PrintF(10, r3dRenderer->ScreenH-280,r3dColor(255,255,255), "%s %d %d (%dx%d)", 
		m_Weapons[m_SelectedWeapon]->getConfig()->m_StoreName,
		m_Weapons[m_SelectedWeapon]->m_numBulletsLeftInClip,
		m_Weapons[m_SelectedWeapon]->m_numBulletsLeft,
		m_Weapons[m_SelectedWeapon]->m_pConfig->m_clipSize,
		m_Weapons[m_SelectedWeapon]->m_ModifiedNumClips);*/
	}
#endif

#ifndef FINAL_BUILD
	// editing rig animation speed
	if(0)
	{
		int astate = uberAnim_->AnimPlayerState;
		Font_Label->PrintF(10, r3dRenderer->ScreenH-300, r3dColor(255,255,255), "AnimState:%d, Speed:%f", astate, uberAnim_->AnimSpeedStates[astate]);

		if(Keyboard->IsPressed(kbsT)) uberAnim_->AnimSpeedStates[astate] -= r3dGetFrameTime() * 0.2f;
		if(Keyboard->IsPressed(kbsY)) uberAnim_->AnimSpeedStates[astate] += r3dGetFrameTime() * 0.2f;
	}
#endif

#ifndef FINAL_BUILD
	char plrUserName[64]; GetUserName(plrUserName);
	if(strncmp(plrUserName, "@@TestDummy", 10) == 0)
	{
		r3dPoint3D scrCoord;
		if(r3dProjectToScreen(GetPosition() + r3dPoint3D(0, 1, 0), &scrCoord))
		{
			float dist = gClientLogic().localPlayer_ ? (gClientLogic().localPlayer_->GetPosition() - GetPosition()).Length() : 0;
			Font_Label->PrintF(scrCoord.x+20, scrCoord.y,   r3dColor(255,255,255), "LastDmg %.2f", lastDamageRcvd);
			Font_Label->PrintF(scrCoord.x+20, scrCoord.y+25,r3dColor(255,255,255), "Distance %.2f", dist);
		}
	}
#endif

#ifndef FINAL_BUILD
	if(r_show_player_debug_data->GetBool())
	{
		float x = 10;
		float y = 100;
		for(size_t i=0; i<uberAnim_->anim.AnimTracks.size(); i++) 
		{
			const r3dAnimation::r3dAnimInfo& ai = uberAnim_->anim.AnimTracks[i];

			char st[256] = "";
			if(ai.dwStatus & ANIMSTATUS_Playing) strcat(st, "Play ");
			if(ai.dwStatus & ANIMSTATUS_Paused) strcat(st, "Pause ");
			if(ai.dwStatus & ANIMSTATUS_Finished) strcat(st, "Finish ");
			if(ai.dwStatus & ANIMSTATUS_Fading) strcat(st, "Fade ");
			if(ai.dwStatus & ANIMSTATUS_Expiring) strcat(st, "Expire ");

			_r3dSystemFont->PrintF(x, y, r3dColor(255, 255, 255), "%s: %02.1f, %X %s", 
				ai.pAnim->pAnimName, ai.fInfluence, ai.dwStatus, st);

			y += 20;
		}
	}
#endif
#ifndef FINAL_BUILD
	if(r_show_player_health->GetBool())
	{
		r3dPoint3D scrCoord;
		if(r3dProjectToScreen(GetPosition() + r3dPoint3D(0, 1, 0), &scrCoord))
		{
			Font_Label->PrintF(scrCoord.x, scrCoord.y,   r3dColor(255,255,255), "%.1f", CurLoadout.Health);
		}
	}
	if(r_show_zombie_stats->GetBool())
	{
		if(NetworkLocal)
			lifeProperties.DebugVisualizeRanges();
	}
#endif

}

//------------------------------------------------------------------------
struct PlayerRenderable : Renderable
{
	void Init( obj_Player::AuraType at, int Transparent )
	{
		DrawFunc = Draw;
		PlayerAuraType = at ;

		this->Transparent = Transparent ;
		matFlagsModifier = 0;
		FirstPerson = 0 ;
	}

	void ApplyCamouflageSettings()
	{
		if (!Parent)
			return;

		gCamouflageDataManager.SetCurrentData(*Parent);
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlayerRenderable* This = static_cast<PlayerRenderable*>( RThis );

		bool draw_weapon = true;

		struct RenderEvents
		{
			RenderEvents()
			{
				D3DPERF_BeginEvent( 0, L"PlayerRenderable::Draw" ) ;
			}

			~RenderEvents()
			{
				D3DPERF_EndEvent() ;
			}

		} renderEvents; renderEvents ;

		CUberEquip::DrawType dt;

		extern int PS_TRANSPARENT_ID		;
		extern int PS_TRANSPARENT_AURA_ID	;

		struct RestoreCamOnExit
		{
			RestoreCamOnExit()
			{
				needRestore = false ;
			}

			~RestoreCamOnExit()
			{
				if( needRestore )
				{
					r3dRenderer->SetCamera( gCam, true ) ;
					if( r_particles_quality->GetInt() > 3 )
					{
						D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_ZENABLE, TRUE ) ) ;
					}
				}
			}

			bool needRestore ;
		} restoreCamOnExit ;

		if( ( This->Transparent || This->PlayerAuraType != obj_Player::AT_NONE )
			&&
			This->FirstPerson			
			)
		{
			r3dCamera firstPersonCam = gCam ;
			firstPersonCam.NearClip = r_first_person_render_z_start->GetFloat();
			firstPersonCam.FarClip = r_first_person_render_z_end->GetFloat();
			firstPersonCam.FOV = r_first_person_fov->GetFloat();

			restoreCamOnExit.needRestore = true ;

			r3dRenderer->SetCamera( firstPersonCam, true ) ;
		}

		if( This->PlayerAuraType != obj_Player::AT_NONE )
		{
			draw_weapon = false ;
			dt = CUberEquip::DT_AURA ;
			r3dMaterial::SetupTransparentStates( 1 );

			void SetParticleDepth();
			SetParticleDepth();

			float r(1.0f), g(1.0), b(1.0f);

			if( This->PlayerAuraType == obj_Player::AT_HEALING )
			{
				r = 0.f ;
				g = 1.f ;
				b = 0.25f ;
			}
			else if(This->PlayerAuraType == obj_Player::AT_MORHINE)
			{
				r = 0.f ;
				g = 0.f ;
				b = 1.0f ;
			}
			else if(This->PlayerAuraType == obj_Player::AT_BERSERK)
			{
				r = 1.f ;
				g = 0.f ;
				b = 0.f ;
			}
			else if(This->PlayerAuraType == obj_Player::AT_SPAWNPROTECTION) // golden color
			{
				r = 1.0f ;
				g = 0.62f ;
				b = 0.15f ;
			}

			r3dRenderer->SetTex( gPlayer_AuraTexture ) ;

			{
				float vConst[ 4 ] = { r, g, b, r3dGetTime()  } ;
				D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( MC_AURA, vConst, 1 ) );
			}

			void SetupFilmToneConstants( int reg ) ;
			SetupFilmToneConstants( 24 ) ;

			r3dRenderer->SetPixelShader( PS_TRANSPARENT_AURA_ID ) ;
			r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_ZC);
			r3dRenderer->pd3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE);
			r3dRenderer->pd3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		}
		else
		{
			const Weapon* wpn = This->Parent->m_Weapons[This->Parent->m_SelectedWeapon];
			if(wpn && wpn->getCategory() == storecat_GRENADE)
			{
				if(This->Parent->uberAnim_->IsGrenadeLaunched())
					draw_weapon = false; // throwing grenade, so do not render grenade in hand
			}

			if( This->Transparent )
			{
				const CamoTimeLine &ctl = This->Parent->camoTimeLine;
				float s = ctl.transparencyOnShock;

				float vConst[ 4 ][ 4 ] =
				{
					// float4 Aura				: register(c14); // (c65);
					{ 1.0f,	s * (1 - s) * 4, ctl.transparentCamoAlpha, 0},
					// float4 TCamoExtra		: register(c15);
					{ 0, 0, 0, 0.f },
					// float4 TCamoColor0			: register(c16);
					{ 0, 0, 0, 0.f },
					// float4 TCamoColor1			: register(c17);
					{ 0, 0, 0, 0 }
				};
				D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( MC_AURA, (float*)vConst, 4 ) );

				vConst[0][3] = 0.01f;
				D3D_V( r3dRenderer->pd3ddev->SetVertexShaderConstantF( 23, (float*)vConst, 1 ) ) ;

				for(int i=0; i<=SLOT_Helmet; i++)
				{
					if(This->Parent->uberEquip_->slots_[i].gear)
					{
						bool isFirstPerson = g_camera_mode->GetInt()==2 && This->Parent->NetworkLocal;
						if( r3dMesh* mesh = This->Parent->uberEquip_->slots_[i].gear->getModel(isFirstPerson) )
						{
							mesh->ExtrudeAmmount = 0.01f ;
						}
					}
				}

				r3dRenderer->SetTex( gPlayer_CamoTexture, 9 ) ;

				r3dRenderer->pd3ddev->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
				r3dRenderer->pd3ddev->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

			}

			if(This->Parent->bDead)
				draw_weapon = false;

			This->ApplyCamouflageSettings();
			dt = CUberEquip::DT_DEFERRED;
		}

		float vCData[ 4 ] = { 1, 1, 1, 0 } ;
#if 0
		if(gClientLogic().localPlayer_ && This->Parent->TeamID != gClientLogic().localPlayer_->TeamID)
		{
			vCData[ 0 ] = 1 ; vCData[ 1 ] = 0 ; vCData[ 2 ] = 0 ; 
		}
		else
		{
			vCData[ 0 ] = 0 ; vCData[ 1 ] = 0 ; vCData[ 2 ] = 1 ; 
		}
#endif
		D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( MC_MASKCOLOR, vCData, 1 ) );

		if (This->matFlagsModifier)
		{
			for (int i = 0; i < SLOT_Max; ++i)
			{
				r3dMesh *m = This->Parent->uberEquip_->getSlotMesh((ESlot)i);
				if(!m) continue;
				if(!m->IsDrawable()) continue;

				for (int j = 0, e = m->GetNumMaterials(); j < e; ++j)
				{
					r3dTriBatch &tb = m->MatChunks[j];
					r3dMaterial *mat = tb.Mat;
					if (!mat) continue;

					This->matFlagsModifier(mat);
				}
			}
		}

		if( This->Transparent )
		{
			void SetVolumeFogParams() ;
			SetVolumeFogParams() ;

			// if it's local, draw depth prepass to disable backfaces for camo so that everything is not uggly.
			// TODO : if we spectate we should do this on spectate target!
			if( This->Parent->NetworkLocal && r3dRenderer->SupportsR32FBlending && !This->FirstPerson )
			{
				void ActivateTransparentBuffer();
				void ActivateParticleDepth();
				void DeactivateTransparentBuffer();
				void DeactivateParticleDepth();
				void SetParticleDepth();
				void RemoveParticleDepth();

				RemoveParticleDepth();
				r3dTransparentDistortFlush() ;
				DeactivateTransparentBuffer();
				ActivateParticleDepth();

				r3dRenderer->SetPixelShader( PS_DEPTH_ID ) ;

				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_ALPHABLENDENABLE,  TRUE ) );

				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ONE ) );
				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE ) );

				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_MIN ) );

				This->Parent->uberEquip_->Draw( This->Parent->GetSkeleton(), This->Parent->DrawFullMatrix_Localized, draw_weapon, CUberEquip::DT_DEPTH, This->FirstPerson ? true : false );

				DeactivateParticleDepth();
				ActivateTransparentBuffer();
				r3dTransparentSetDistort( 1 ) ;
				SetParticleDepth();

				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD ) );
			}

			{
				D3DXMATRIX sm0, sm1 ;

				D3DXMatrixTranspose( &sm0, &This->Parent->DrawFullMatrix_Localized ) ;

				sm1 = This->Parent->DrawFullMatrix_Localized * r3dRenderer->ViewMatrix_Localized ;

				D3DXMatrixTranspose( &sm1, &sm1 ) ;

				float consts[ 6 ][ 4 ] =
				{
					// float4x3 mWorld : register( c32 ) ;
					{ sm0._11, sm0._12, sm0._13, sm0._14 },
					{ sm0._21, sm0._22, sm0._23, sm0._24 },
					{ sm0._31, sm0._32, sm0._33, sm0._34 },
					// float4x3 mWorldView : register( c35 ) ;
					{ sm1._11, sm1._12, sm1._13, sm1._14 },
					{ sm1._21, sm1._22, sm1._23, sm1._24 },
					{ sm1._31, sm1._32, sm1._33, sm1._34 },
				} ;

				D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 32, (float*)consts, 6 ) ) ;
			}

			r3dRenderer->SetRenderingMode( R3D_BLEND_ALPHA | R3D_BLEND_ZC );
			D3D_V( r3dRenderer->pd3ddev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_ONE ) );
			D3D_V( r3dRenderer->pd3ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA ) );

			r3dTransparentSetDistort( 1 ) ;

			if( This->FirstPerson && r_particles_quality->GetInt() > 3 )
			{
				D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_ZENABLE, FALSE ) ) ;
			}

		}

		if(This->Parent->forcedEmptyHands)
			draw_weapon = false;

		This->Parent->uberEquip_->Draw( This->Parent->GetSkeleton(), This->Parent->DrawFullMatrix_Localized, draw_weapon, dt, This->FirstPerson ? true : false );

		if( This->PlayerAuraType != obj_Player::AT_NONE )
		{
			r3dRenderer->SetPixelShader( PS_TRANSPARENT_ID ) ;
		}
	}

	obj_Player* Parent ;
	typedef void (*MaterialModifyFlags)(r3dMaterial *mat);
	MaterialModifyFlags matFlagsModifier;

	char PlayerAuraType ;
	char Transparent ;
	char FirstPerson ;
};

//------------------------------------------------------------------------
struct PlayerShadowRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlayerShadowRenderable* This = static_cast<PlayerShadowRenderable*>(RThis);

		bool draw_weapon = true;
		const Weapon* wpn = This->Parent->m_Weapons[This->Parent->m_SelectedWeapon];
		if(wpn && wpn->getCategory() == storecat_GRENADE)
		{
			if(This->Parent->uberAnim_->IsGrenadeLaunched())
				draw_weapon = false; // throwing grenade, so do not render grenade in hand
		}
		if(This->Parent->forcedEmptyHands)
			draw_weapon = false;
		This->Parent->uberEquip_->Draw(This->Parent->GetSkeleton(), This->Parent->DrawFullMatrix_Localized, draw_weapon, CUberEquip::DT_SHADOWS, false );
	}

	obj_Player* Parent;
};

struct PlayerFlashRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlayerFlashRenderable* This = static_cast<PlayerFlashRenderable*>( RThis );

		This->Parent->DrawLabel();
		This->Parent->DrawReticle();
	}

	obj_Player* Parent;
};

struct PlayerDebugRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlayerDebugRenderable* This = static_cast<PlayerDebugRenderable*>( RThis );
		r3dRenderer->SetTex(NULL);
		r3dRenderer->SetMaterial(NULL);
		r3dRenderer->SetRenderingMode(R3D_BLEND_NOALPHA | R3D_BLEND_NZ);

		This->Parent->GetSkeleton()->DrawSkeleton(Cam, This->Parent->GetPosition());

		/*if(This->Parent->explosionTime > 0)
		{
		This->Parent->explosionTime-=r3dGetFrameTime();
		r3dDrawSphereSolid(This->Parent->explosionCenter, This->Parent->explosionRadius, Cam, r3dColor::red);
		}*/
		//r3dDrawBoundBox(This->Parent->GetBBoxWorld(), Cam, r3dColor24::green/*debugColorLevels[m_Level]*/, 0.05f);
	}

	obj_Player* Parent;
};

#define	RENDERABLE_AI_PLAYER_SORT_VALUE (2*RENDERABLE_USER_SORT_VALUE)

extern RenderArray	g_render_arrays[ rsCount ];

void
obj_Player::AppendShadowRenderables( RenderArray & rarr, const r3dCamera& Cam )
{
	r3d_assert( &rarr - g_render_arrays - rsCreateSM < NumShadowSlices ) ;

	// HACK: For the moment. 
#if VEHICLES_ENABLED
	if ( isInVehicle() ) 
	{
		//return; 
	}
#endif

	int firstPerson = g_camera_mode->GetInt() == 2 && NetworkLocal && !bDead;

	if(firstPerson) // do not render our own shadow in FPS mode (same as in BF3)
		return;

	if( !gDisableDynamicObjectShadows && &rarr - g_render_arrays - rsCreateSM != r_active_shadow_slices->GetInt() - 1 )
	{
		PlayerShadowRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_AI_PLAYER_SORT_VALUE;

		rarr.PushBack( rend );
	}
}

//////////////////////////////////////////////////////////////////////////

void SetTransparentCamoMatFlags(r3dMaterial *mat )
{
	mat->Flags |= R3D_MAT_TRANSPARENT | R3D_MAT_TRANSPARENT_CAMOUFLAGE ;
	if (strstr(mat->Name, "nocamo"))
		mat->Flags |= R3D_MAT_SKIP_DRAW;
}

void SetTransparentCamoMatFlags_FP(r3dMaterial *mat )
{
	mat->Flags |= R3D_MAT_TRANSPARENT | R3D_MAT_TRANSPARENT_CAMO_FP ;
	if (strstr(mat->Name, "nocamo"))
		mat->Flags |= R3D_MAT_SKIP_DRAW;
}

//////////////////////////////////////////////////////////////////////////

void UnsetTransparentCamoMatFlags(r3dMaterial *mat)
{
	mat->Flags &= ~(R3D_MAT_TRANSPARENT | R3D_MAT_TRANSPARENT_CAMOUFLAGE | R3D_MAT_TRANSPARENT_CAMO_FP |  R3D_MAT_SKIP_DRAW);
}

struct PlaceableConsumableItemRenderable : MeshDeferredRenderable
{
	void Init( int a_transparent )
	{
		DrawFunc = Draw;
		Transparent = a_transparent ;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		PlaceableConsumableItemRenderable* This = static_cast< PlaceableConsumableItemRenderable* >( RThis );
		r3dApplyPreparedMeshVSConsts( This->Parent->m_ConsumableItemPlacementMeshVSConsts );

		int wasTransparent = 0 ;

		float oldTranspMultiplier = 1.0f ;

		bool hasMatForTrasnparency = This && This->Transparent && This->Mesh && This->Mesh->MatChunks[This->BatchIdx].Mat ;

		if ( hasMatForTrasnparency )
		{
			// because previous material may be the same
			// and thus it will filter our transparency hacks that follow
			r3dMaterial::ResetMaterialFilter() ;

			r3dMaterial* mat = This->Mesh->MatChunks[This->BatchIdx].Mat ;

			wasTransparent = mat->Flags & R3D_MAT_TRANSPARENT ;
			oldTranspMultiplier = mat->TransparencyMultiplier ;

			if( !wasTransparent )
				mat->TransparencyMultiplier = 0.75f ;

			mat->Flags |= R3D_MAT_TRANSPARENT ;

			SetLightsIfTransparent( mat, This->Parent->GetBBoxWorld() ) ;
		}

		if(This->Mesh->IsSkeletal())
			r3dSkeleton::SetDefaultShaderConstants();

		MeshDeferredRenderable::Draw( RThis, Cam );

		if ( hasMatForTrasnparency )
		{
			r3dMaterial* mat = This->Mesh->MatChunks[This->BatchIdx].Mat ;

			int& flags = mat->Flags ;
			flags &= ~R3D_MAT_TRANSPARENT ;
			flags |= wasTransparent ;

			mat->TransparencyMultiplier = oldTranspMultiplier ;
		}
	}

	obj_Player* Parent ;
	int Transparent ;
};

//------------------------------------------------------------------------
// special callback to ignore terrain for beacon placement, as otherwise it is almost impossible to place beacon on uneven terrain
class beaconFilterCallback : public PxSceneQueryFilterCallback
{
public:
	beaconFilterCallback(){};

	virtual PxSceneQueryHitType::Enum preFilter(const PxFilterData& filterData, PxShape* shape, PxSceneQueryFilterFlags& filterFlags)
	{
		if(shape)
		{
			PxRigidActor& actor = shape->getActor();
			PhysicsCallbackObject* target = static_cast<PhysicsCallbackObject*>(actor.userData);
			if(target)
			{
				GameObject *gameObj = target->isGameObject();
				if(gameObj)
				{
					if(gameObj->isObjType(OBJTYPE_Terrain))
						return PxSceneQueryHitType::eNONE;
				}
			}
		}
		return PxSceneQueryHitType::eBLOCK;
	}

	virtual PxSceneQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxSceneQueryHit& hit)
	{
		// shouldn't be called!
		return PxSceneQueryHitType::eBLOCK;
	}
};


void
obj_Player::AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam )
{
	if(!m_enableRendering)
		return;

	// HACK: For the moment. 
#if VEHICLES_ENABLED
	if ( isInVehicle() ) 
	{
		//return; 
	}
#endif


	int firstPerson = g_camera_mode->GetInt() == 2 && NetworkLocal && !bDead;

	if(firstPerson)
	{
		if(hasScopeMode() && CurrentRig.allowScope)
		{
			r_first_person_render_z_start->SetFloat(0.01f);
			r_first_person_render_z_end->SetFloat(0.7f);
		}
		else
		{
			r_first_person_render_z_start->SetFloat(0.1f);
			r_first_person_render_z_end->SetFloat(1.5f);
		}
	}

	// deferred
	if( !NetworkLocal || (NetworkLocal && (!g_CameraInsidePlayer || g_camera_mode->GetInt()==2)) ) // do not render player if camera is inside of player
	{
		if(!(NetworkLocal && hasScopeMode() && hasScopeMode_hidePlayerModel() && CurrentRig.allowScope)) // do not render yourself in scope mode
		{
			bool transparentCamo = camoTimeLine.camoMasterTransparency < 1.0f;
			if (camoTimeLine.camoMasterTransparency > 0)
			{
				PlayerRenderable rend;

				rend.Init( AT_NONE, 0 );
				rend.Parent		= this;
				rend.SortValue	= RENDERABLE_AI_PLAYER_SORT_VALUE;
				rend.matFlagsModifier = &UnsetTransparentCamoMatFlags;
				rend.FirstPerson = firstPerson ;

				render_arrays[ firstPerson ? rsFillGBufferFirstPerson : rsFillGBuffer ].PushBack( rend );
			}

			if ( transparentCamo )
			{
				PlayerRenderable rend ;

				rend.Init( AT_NONE, 1 ) ;
				rend.Parent		= this ;
				rend.FirstPerson = firstPerson ;

				rend.matFlagsModifier = firstPerson ? 
					&SetTransparentCamoMatFlags_FP
					:
				&SetTransparentCamoMatFlags ;

				float len = ( Cam - GetPosition() ).Length() ;
				rend.SortValue	= RENDERABLE_EMITTER_USER_SORT_VALUE | GetRevIDist( len );

				render_arrays[ rsDrawTransparents ].PushBack( rend ) ;			
			}

			if ( m_AuraType != AT_NONE )
			{
				PlayerRenderable rend ;

				rend.Init( m_AuraType, 0 ) ;
				//rend.Init( AT_HEALING , 0 ) ;
				rend.Parent		= this ;
				rend.FirstPerson = firstPerson ;

				rend.matFlagsModifier = &UnsetTransparentCamoMatFlags;

				float len = ( Cam - GetPosition() ).Length() ;

				// render after tcamo hecne +1
				rend.SortValue	= RENDERABLE_EMITTER_USER_SORT_VALUE | ( GetRevIDist( len ) + 1 );

				render_arrays[ rsDrawTransparents ].PushBack( rend ) ;			
			}

			if(m_needToDrawBarricadePlacement)
			{
				r3dMesh* riotMesh = NULL;
				if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_BarbWireBarricade)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Barricade_BarbWire_Built.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				else if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_WoodShieldBarricade)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Barricade_WoodShield_Built.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				else if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_RiotShieldBarricade)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Riot_Shield_01.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				else if(m_needToDrawBarricadePlacement == WeaponConfig::ITEMID_SandbagBarricade)
				{
					if(m_BarricadeMeshPlacement == NULL)
						m_BarricadeMeshPlacement = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\item_barricade_Sandbag_built.sco", true, false, true, true );
					riotMesh = m_BarricadeMeshPlacement;
				}
				if(riotMesh->IsDrawable())
				{
					r3dPoint3D *scale ;
					r3dPoint2D *texcScale ;
					scale = riotMesh->IsSkeletal() ? 0 : &riotMesh->unpackScale;
					texcScale = &riotMesh->texcUnpackScale ;

					D3DXMATRIX m1, m2, m3, world;
					r3dPoint3D pos = GetPosition() + GetvForw() * 1.0f;
					D3DXMatrixTranslation(&m1, pos.x, pos.y, pos.z);
					D3DXMatrixRotationY(&m2, R3D_PI);
					D3DXMatrixMultiply(&m3, &DrawRotMatrix, &m2);
					D3DXMatrixMultiply(&world, &m3, &m1);
					r3dPrepareMeshVSConsts(m_ConsumableItemPlacementMeshVSConsts, world, scale, texcScale, r3dRenderer->ViewMatrix, r3dRenderer->ViewProjMatrix );

					// check if we can place our riot shield. I don't want to do this in update, as anyway this function is called every frame, so should be fine
					{
						PxBoxGeometry bbox(riotMesh->localBBox.Size.x*0.5f, riotMesh->localBBox.Size.y*0.5f, riotMesh->localBBox.Size.z*0.5f);
						D3DXMATRIX rot = GetRotationMatrix();
						PxMat33 meshRot(PxVec3(rot._11, rot._21, rot._31), PxVec3(rot._12, rot._22, rot._32), PxVec3(rot._13, rot._23, rot._33));
						PxTransform pose(PxVec3(pos.x, pos.y+(riotMesh->localBBox.Size.y*0.5f)+0.5f, pos.z), PxQuat(meshRot));

						PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
						PxShape* shape;
						m_canPlaceConsumableItem = !g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, shape, filter) && bOnGround;
						if(m_canPlaceConsumableItem) // check if we are trying to place it on top of usable item
						{
							bbox.halfExtents.y *= 4.0f;
							PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
							PxShape* results[64] = {0};
							int numRes = g_pPhysicsWorld->PhysXScene->overlapMultiple(bbox, pose, results, 64, filter);
							for(int i=0; i<numRes; ++i)
							{
								PhysicsCallbackObject* clbObj = NULL;
								if( results[i] && (clbObj = static_cast<PhysicsCallbackObject*>(results[i]->getActor().userData)))
								{
									GameObject* gameObj = clbObj->isGameObject();
									if(gameObj && gameObj->isObjType(OBJTYPE_GameplayItem))
									{
										m_canPlaceConsumableItem = false;
										break;
									}
								}
							}
						}
					}

					{
						float dist = (Cam - GetPosition()).Length();

						uint32_t prevTranspCount = render_arrays[rsDrawTransparents].Count();

						riotMesh->AppendTransparentRenderables( render_arrays[rsDrawTransparents], m_canPlaceConsumableItem?r3dColor::white:r3dColor::red, dist, 1 );

						for( uint32_t i = prevTranspCount, e = render_arrays[rsDrawTransparents].Count(); i < e; i ++ )
						{
							PlaceableConsumableItemRenderable& rend = static_cast<PlaceableConsumableItemRenderable&>( render_arrays[rsDrawTransparents][ i ] ) ;
							rend.Init( 1 ) ;
							rend.Parent = this;
						}
					}
				}
			}
		}
	}

	// debug
	{
		PlayerDebugRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_AI_PLAYER_SORT_VALUE;

		render_arrays[ rsDrawBoundBox ].PushBack( rend );
	}

	// Flash UI
#ifndef FINAL_BUILD
	if((!d_video_spectator_mode->GetBool() && !d_disable_game_hud->GetBool() && !d_observer_mode->GetBool()) || d_force_scope_rendering->GetBool())
#endif
	{
		PlayerFlashRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_AI_PLAYER_SORT_VALUE;

		render_arrays[ rsDrawFlashUI ].PushBack( rend );
	}
}

void obj_Player::UpdateTransform()
{
	GameObject::UpdateTransform();
}

//////////////////////////////////////////////////////////////////////////
void obj_Player::stopFootstepsSound()
{
	if (SoundSys.IsHandleValid(footStepsSnd))
	{
		SoundSys.Stop(footStepsSnd);
	}
}

void obj_Player::UpdateSounds()
{
	R3DPROFILE_FUNCTION("UpdateSounds");

	if(NetworkLocal) // local sounds
	{
		if(!SoundSys.isPlaying(m_sndClothMovementSprint) && PlayerState == PLAYER_MOVE_SPRINT) // started sprinting
		{
			SoundSys.Start(m_sndClothMovementSprint);
		}
		else if(SoundSys.isPlaying(m_sndClothMovementSprint) && PlayerState != PLAYER_MOVE_SPRINT) // stopped sprinting
		{
			SoundSys.Stop(m_sndClothMovementSprint);
		}

		float breathVolume = 0.0f;
		float curStaminaR = m_Stamina/getMaxStamina();
		if(curStaminaR < 0.15f && curStaminaR > 0.1f)
			breathVolume = 1.0f-((curStaminaR-0.1f)*20.0f);
		else if(curStaminaR <= 0.1f)
			breathVolume = 1.0f;
		else if(curStaminaR > 0.15f)
			breathVolume = 0.0f;

		if(SoundSys.isPlaying(m_sndBreathSprint) && breathVolume <= 0.0f)
			SoundSys.Stop(m_sndBreathSprint);
		else if(!SoundSys.isPlaying(m_sndBreathSprint) && breathVolume > 0.0f)
			SoundSys.Start(m_sndBreathSprint);

		if(SoundSys.isPlaying(m_sndBreathSprint))
		{
			breathVolume *= m_sndBreathBaseVolume;
			SoundSys.SetProperty(m_sndBreathSprint, FMOD_EVENTPROPERTY_VOLUME, &breathVolume);
		}

		if(CurLoadout.Health < 15.0f && !SoundSys.isPlaying(m_sndInPain) && !bDead)
			SoundSys.Start(m_sndInPain);
		else if((CurLoadout.Health >= 15.0f || bDead) && SoundSys.isPlaying(m_sndInPain))
			SoundSys.Stop(m_sndInPain);


		bool scopeRendering = hasScopeMode() && CurrentRig.allowScope && m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getCategory() == storecat_SNP;
		if(scopeRendering)
		{
			if(m_isHoldingBreath && m_Breath > 0)
			{
				if(!SoundSys.isPlaying(m_sndSniperHeartSlow))
				{
					SoundSys.Stop(m_sndSniperBreath);
					SoundSys.Start(m_sndSniperHeartSlow);
					SoundSys.PlayAndForget(SoundSys.GetEventIDByPath(m_isFemaleHero?"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_INHALE_F":"Sounds/WarZ/PlayerSounds/PLAYER_SNIPER_INHALE_M"), GetPosition());
				}
			}
			else
			{
				if(SoundSys.isPlaying(m_sndSniperHeartSlow))
				{
					SoundSys.Stop(m_sndSniperHeartSlow);
					SoundSys.Start(m_sndSniperExhale);
				}
				if(!SoundSys.isPlaying(m_sndSniperExhale))
					if(!SoundSys.isPlaying(m_sndSniperBreath))
						SoundSys.Start(m_sndSniperBreath);
			}
		}
		else
		{
			if(SoundSys.isPlaying(m_sndSniperBreath))
				SoundSys.Stop(m_sndSniperBreath);
			if(SoundSys.isPlaying(m_sndSniperHeartSlow))
				SoundSys.Stop(m_sndSniperHeartSlow);
		}
	}
	else // for all players, 3D sounds
	{

	}
}

void obj_Player::UpdateFootsteps()
{
	R3DPROFILE_FUNCTION("UpdateFootsteps");
	R3DPROFILE_START("RayCast");
	// cast ray to see on what material player is standing
	PhysicsCallbackObject* target = NULL;
	const MaterialType *mt = 0;

	PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC));

	// did we hit anything?
	bool hitResult = g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y + 0.5f, GetPosition().z), PxVec3(0, -1, 0), 1.0f, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter);
	if( hitResult )
	{
		if( hit.shape && (target = static_cast<PhysicsCallbackObject*>(hit.shape->getActor().userData)))
		{
			r3dMaterial* material = 0;
			GameObject *gameObj = target->isGameObject();
			if(gameObj)
			{
				if( gameObj->isObjType( OBJTYPE_Mesh ) )
					material = static_cast< MeshGameObject* > ( target )->MeshLOD[ 0 ]->GetFaceMaterial( hit.faceIndex );
				if(gameObj->isObjType(OBJTYPE_Terrain))
				{
					mt = Terrain->GetMaterialType(GetPosition());
				}
			}
			else if(target->hasMesh())
			{
				material = target->hasMesh()->GetFaceMaterial( hit.faceIndex );
			}
			if (!material)
				material = target->GetMaterial(hit.faceIndex);

			if(material && !mt) // get material type from mesh material
			{
				mt = g_pMaterialTypes->Get(r3dHash::MakeHash(material->TypeName));
			}

			float waterDepth = getWaterDepthAtPos(GetPosition());
			if(waterDepth > 0.1f) 
			{
				mt = g_pMaterialTypes->Get(r3dHash::MakeHash("Water"));
			}

		}
	}
	R3DPROFILE_END("RayCast");

	{
		R3DPROFILE_FUNCTION("play sound");
		// hack: right now we have a problem with faceID in raycast not matching faceID of the actual mesh (collision mesh vs regular mesh). 
		// So, in case if we failed to find a proper material because of that, let's just use old foot step sound and that's it
		int fts = currentFootStepsSoundID; 
		if(mt && !bSwim)
		{
			if(bCrouch && mt->crouchFootstepsSound>=0)
				fts = mt->crouchFootstepsSound;
			else if(bProne && mt->proneFootstepsSound>=0)
				fts = mt->proneFootstepsSound;
			/*else if (bSwim && !bSwimShift)
			fts = mt->SwimSound;
			else if (bSwimShift)
			fts = mt->SwimFSound;*/
			else
				fts = mt->footStepsSound;
		}

		if (bSwim && PlayerState == PLAYER_SWIM)
			fts = SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_SLOW");
		else if (bSwim && !bSwimShift)
			fts = SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_NORMAL");
		else if (bSwimShift)
			fts = SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_FAST");

		if (bSwim && PlayerMoveDir == CUberData::ANIMDIR_Stand) // swim idie
		{
			fts = SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_SWIM_IDIE");
		}

		//footStepsSnd = NULL;

		if (!SoundSys.IsHandleValid(footStepsSnd) || fts != currentFootStepsSoundID)
		{
			SoundSys.Stop(footStepsSnd);
			footStepsSnd = NULL;

			for(uint32_t i=0; i<footStepsArr.Count(); ++i) 
			{
				if(footStepsArr[i].id == fts)
				{
					footStepsSnd = footStepsArr[i].snd;
					// check if handle is valid, sometimes it becomes invalid for some reason
					if(!SoundSys.IsHandleValid(footStepsSnd))
					{
						// handle is broken, remove it from the list, and remove it from FMOD
						SoundSys.Stop(footStepsArr[i].snd);
						SoundSys.Release(footStepsArr[i].snd);
						footStepsArr.Erase(i);	
						footStepsSnd = NULL;
					}
					break;
				}
			}

			if(footStepsSnd == NULL)
			{
				footStepsSnd = SoundSys.Play(fts, GetPosition());
				if (!SoundSys.IsHandleValid(footStepsSnd))
					return;
				footStepsArr.PushBack(footStepEntry_s(fts, footStepsSnd));
			}

			currentFootStepsSoundID = fts;
		}

		SoundSys.Set3DAttributes(footStepsSnd, &GetPosition(), 0, 0);

		static const float SpawnRatesByState[] = {
			0,	//PLAYER_IDLE,
			0,	//PLAYER_IDLEAIM,
			0.6f,	//PLAYER_MOVE_CROUCH,
			0.54f,	//PLAYER_MOVE_CROUCH_AIM,
			0.49f,	//PLAYER_MOVE_WALK_AIM,
			0.95f,	//PLAYER_MOVE_RUN,
			1.0f,	//PLAYER_MOVE_SPRINT,
			0.95f,	//PLAYER_MOVE_SWIM,
			0.95f,	//PLAYER_MOVE_SWIM,
			0.95f,	//PLAYER_MOVE_SWIM,
			0.95f,	//PLAYER_MOVE_SWIM,
			0.95f,	//PLAYER_MOVE_SWIM,
			0.95f,	//PLAYER_MOVE_SWIM,
			1.0f,	//PRONE
			1.0f,	//PRONE_AIM
			1.0f,  //PRONE_UP
			0,	//PRONE_DOWN
			0,	//PRONE_IDLE
			0,	//PLAYER_DIE,
		};
		//COMPILE_ASSERT( R3D_ARRAYSIZE(SpawnRatesByState) == PLAYER_NUM_STATES ) ;
		float spawnRate = SpawnRatesByState[PlayerState];
		if(hitResult == false && !bSwim)
		{
			spawnRate = 0.0f;
		}
		if (PlayerMoveDir == CUberData::ANIMDIR_Stand && !bSwim)
		{
			spawnRate = 0.0f;
		}

		if(spawnRate > 0)
		{
			footsteps_timeSinceLastMissingHitResult = r3dGetTime();
			if (SoundSys.GetState(footStepsSnd) != FMOD_EVENT_STATE_PLAYING)
			{
				SoundSys.Start(footStepsSnd);
			}

			SoundSys.SetProperty(footStepsSnd, FMOD_EVENTPROPERTY_SPAWNINTENSITY, &spawnRate);
		}
		else
		{
			if((r3dGetTime() - footsteps_timeSinceLastMissingHitResult)>0.25f && !bSwim) // workaround when you are running across wooden planks and sounds restart each time you step on new wooden plank
				SoundSys.Stop(footStepsSnd);
			else if (bSwim)
				SoundSys.Stop(footStepsSnd);
		}
	}
}

void obj_Player::updateWeaponUI()
{
	if(!hudMain)
		return;
	if(!NetworkLocal)
		return;

	for(int i=0; i<6; ++i)
	{
		const BaseItemConfig* baseItem = g_pWeaponArmory->getConfig(CurLoadout.Items[i].itemID);
		int numCharges = CurLoadout.Items[i].quantity;
		if(baseItem)
		{
			hudMain->setSlotInfo(i, baseItem->m_StoreName, numCharges, baseItem->m_StoreIcon);
		}
		else
			hudMain->setSlotInfo(i, "", 0, "");
	}
	if(!forcedEmptyHands && m_Weapons[m_SelectedWeapon])
		hudMain->setActiveSlot(m_SelectedWeapon);
	else
		hudMain->setActiveSlot(100);

	{
		bool isMelee = false;
		if(m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getCategory() == storecat_MELEE)
			isMelee = true;
		if(!forcedEmptyHands && ((m_SelectedWeapon < 2 && !isMelee && m_Weapons[m_SelectedWeapon]) || (m_SelectedWeapon>=2 && m_Weapons[m_SelectedWeapon] && m_Weapons[m_SelectedWeapon]->getCategory() == storecat_GRENADE)))
		{
			hudMain->showWeaponInfo(true);
		}
		else
		{
			hudMain->showWeaponInfo(false);
		}
	}
}

void obj_Player::switchFPS_TPS()
{
	r3d_assert(NetworkLocal);

	g_camera_mode->SetInt(g_camera_mode->GetInt()==2?0:2);

	// we only need to reload body meshes
	const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(CurLoadout.HeroItemID);
	loadHeroMeshes(heroConfig);
	// reload weapon meshes (to load skeleton for FPS mode)
	// create weapons & items
	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; i++)
	{
		if(m_Weapons[i])
		{
			m_Weapons[i]->setFirstPersonMeshFlag(g_camera_mode->GetInt()==2);
			m_Weapons[i]->ResetMeshLoadedFlag();
			uint32_t attmIDs[WPN_ATTM_MAX];
			m_Weapons[i]->getCurrentAttachmentIDs(attmIDs);
			uint32_t zeroID[WPN_ATTM_MAX] = {0};
			m_Weapons[i]->setWeaponAttachmentsByIDs(zeroID); // reset attachments if any
			m_Weapons[i]->setWeaponAttachmentsByIDs(attmIDs); // reload attachments
		}
	}

	uberAnim_->anim.StopAll();	// prevent animation blending on loadout switch
	uberAnim_->AnimPlayerState = -1;
	SyncAnimation(true);
}

void obj_Player::switchToEmptyHands()
{
	if(forcedEmptyHands)
		return;

	forcedEmptyHands = true;
	if(m_Weapons[m_SelectedWeapon])
		m_Weapons[m_SelectedWeapon]->OnUnequip();
	SyncAnimation(true);

	if(NetworkLocal)
		updateWeaponUI();
}

float obj_Player::getPlayerHeightForCamera() const
{
	return Height;
}

//////////////////////////////////////////////////////////////////////////

void obj_Player::DoRagdoll(bool toRagdoll, int hitBone, const r3dPoint3D* impulse)
{
#if ENABLE_RAGDOLL
	if (m_PhysSkeleton)
	{
		if (impulse)
		{
			m_PhysSkeleton->SwitchToRagdollWithForce(toRagdoll, hitBone, impulse);
		}
		else
		{
			m_PhysSkeleton->SwitchToRagdoll(toRagdoll);
		}

		if (!m_PhysSkeleton->IsRagdollMode())
		{
			SetBBoxLocal(m_OrigBBox);
		}
	}
#endif
}

void obj_Player::OnGameEnded()
{
	if(m_SelectedWeapon >= 0 && m_Weapons[m_SelectedWeapon] )
	{
		// currently we only need to do this to the equipped weapon, it can be expanded as needed.
		m_Weapons[m_SelectedWeapon]->OnGameEnded();
	}
}

void obj_Player::OnBackpackChanged(int idx)
{
	r3d_assert(NetworkLocal);

	if(idx == wiCharDataFull::CHAR_LOADOUT_WEAPON1 || idx == wiCharDataFull::CHAR_LOADOUT_WEAPON2)
	{
		// attachments are reset on item change (SERVER CODE SYNC POINT)
		CurLoadout.Attachment[idx].Reset();
		if(CurLoadout.Items[idx].Var2 > 0)
			CurLoadout.Attachment[idx].attachments[WPN_ATTM_CLIP] = CurLoadout.Items[idx].Var2;
	}

	// check if we need to modify loadout
	if(idx >= wiCharDataFull::CHAR_LOADOUT_WEAPON1 && idx <= wiCharDataFull::CHAR_LOADOUT_HEADGEAR)
	{
		UpdateLoadoutSlot(CurLoadout);

		updateWeaponUI();
	}

	// if trying to put weapon into quick slot that is active, then force empty hands
	if(idx >= wiCharDataFull::CHAR_LOADOUT_ITEM1 && idx <= wiCharDataFull::CHAR_LOADOUT_ITEM4 && 
		m_SelectedWeapon == idx && m_Weapons[m_SelectedWeapon] && !m_Weapons[m_SelectedWeapon]->isUsableItem())
	{	
		switchToEmptyHands();
		{
			PKT_C2C_PlayerSwitchWeapon_s n;
			n.wid       = forcedEmptyHands?255:m_SelectedWeapon;
			p2pSendToHost(this, &n, sizeof(n), true);
		}
	}


	if(hudPause && hudPause->isActive()) hudPause->reloadBackpackInfo();

	if(NetworkLocal && (idx == wiCharDataFull::CHAR_LOADOUT_ARMOR || idx == wiCharDataFull::CHAR_LOADOUT_HEADGEAR))
		SoundSys.PlayAndForget(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_CHANGECLOTHES"), GetPosition());
}

#if VEHICLES_ENABLED
obj_Vehicle* obj_Player::canEnterVehicle()
{
	// Fast version from Frank
	for( GameObject* obj = GameWorld().GetFirstObject(); obj; obj = GameWorld().GetNextObject(obj) )
	{
		if( obj->ObjTypeFlags & OBJTYPE_Vehicle )
		{
			obj_Vehicle* vehicle= static_cast< obj_Vehicle* > ( obj );

			float distSq = (GetPosition() - vehicle->GetPosition()).LengthSq();
			if( distSq < gDist_To_Allow_Vehicle_Entry * gDist_To_Allow_Vehicle_Entry )
			{
				return vehicle;
			}
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void obj_Player::TogglePhysicsSimulation(bool on)
{
	if (PhysicsObject)
	{
		PxActor *a = PhysicsObject->getPhysicsActor();
		if (a)
		{
			on ? g_pPhysicsWorld->AddActor(*a) : g_pPhysicsWorld->RemoveActor(*a);
		}
		if (m_PhysSkeleton)
		{
			m_PhysSkeleton->TogglePhysicsSimulation(on);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void obj_Player::enterVehicle( obj_Vehicle* target_Vehicle )
{
	// Fast version from Frank
	target_Vehicle->SwitchToDrivable( true );
	vehicleViewActive_ = VehicleView_Driver; 
	d_drive_vehicles->SetBool( true ); // to gain control currently. 
	// HACK: bash this currently. 
	target_Vehicle->NetworkLocal = true;

	//	Exclude player physics shapes from simulation
	TogglePhysicsSimulation(false);
}

void obj_Player::exitVehicle()
{

	// first let's look for a good location
	obj_Vehicle* currentCar = g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle();
	r3d_assert(currentCar);


	r3dVector rotationVector = currentCar->GetRotationVector();
	r3dMatrix Matrix;
	// NOTE: We might want to take out rotation in the x or z direction and only focus on the Y direction... 
	r3dBuildRotationMatrix(Matrix, rotationVector );

	bool success = false;
	for ( int exitIndex = 0; exitIndex < 4; exitIndex++  )
	{
		r3dVector exitPosition;
		bool returnValue = currentCar->getExitSpace( exitPosition, exitIndex );
		r3d_assert( returnValue );

		exitPosition *= Matrix;
		r3dVector targetPosition = currentCar->GetPosition() + exitPosition; 

		if ( TestExitPosition( targetPosition) )
		{
			TeleportPlayer( targetPosition, "Exiting vehicle");
			success = true;
			break;
		}
	}

	if( success == false )
	{
		// don't know the height right now. 
		r3dVector targetPosition = currentCar->GetPosition() + r3dVector( 0, 3, 0 );
		if ( TestExitPosition( targetPosition, true ) )
		{
			TeleportPlayer( targetPosition, "Exiting vehicle");
			success = true;
		}
	}

	if( success == true ) 
	{
		// Fast version from Frank
		vehicleViewActive_ = VehicleView_None; 
		d_drive_vehicles->SetBool( false ); // to release control currently. 
		TogglePhysicsSimulation(true);
	}

}

bool obj_Player::isDriving()
{
	return vehicleViewActive_ == VehicleView_Driver;
}

bool obj_Player::isInVehicle() 
{
	return vehicleViewActive_ != VehicleView_None;
}

bool obj_Player::TestExitPosition( r3dVector& exitPosition, bool ignoreGround)
{

	// snap to ground
	if ( ignoreGround == false ) {
		// first find out if there's ground. 
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
		if(g_pPhysicsWorld->raycastSingle(PxVec3(exitPosition.x, exitPosition.y+1, exitPosition.z), PxVec3(0, -1, 0), 2, PxSceneQueryFlag::eIMPACT, hit, filter))
		{
			exitPosition.x = hit.impact.x;
			exitPosition.y = hit.impact.y;
			exitPosition.z = hit.impact.z;
		} 
		else 
		{
			return false;
		}
	}

	// check spawn area. 
	PxShape* overlapHit = NULL;
	PxU32 collisionFlag = COLLIDABLE_STATIC_MASK ;
	PxBoxGeometry playerBox( 0.2f, 0.9f, 0.2f) ;
	PxTransform pose(PxVec3(exitPosition.x, exitPosition.y +1.1f, exitPosition.z), PxQuat(0,0,0,1));
	PxSceneQueryFilterData capsulefilter(PxFilterData(collisionFlag, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
	if ( g_pPhysicsWorld->PhysXScene->overlapAny( playerBox, pose, overlapHit, capsulefilter))
	{
		return false;
	}

	return true;
}

#endif


//////////////////////////////////////////////////////////////////////////

void ProcessCharacterEditor(obj_Player* pl, float left, float top, float height);
void ProcessCharacterEditor()
{
	obj_Player* pl = (obj_Player *)GameWorld().GetObject(EditorGameHUD::editorPlayerId);
	if (!pl)
	{
		imgui_Static(r3dRenderer->ScreenW-375, 85, "Spawn player firstly by pressing F8");
		return;
	}

	ProcessCharacterEditor(pl, 100, 100, 150);
}

void ProcessCharacterEditor(obj_Player* pl, float left, float top, float height)
{
	stringlist_t weaponDB_nameList;
	uint32_t* weaponDB_idList = 0;

	stringlist_t attachmentDB_muzzle_nameList;
	uint32_t* attachmentDB_muzzle_idList = 0;
	stringlist_t attachmentDB_upperRail_nameList;
	uint32_t* attachmentDB_upperRail_idList = 0;
	stringlist_t attachmentDB_leftRail_nameList;
	uint32_t* attachmentDB_leftRail_idList = 0;
	stringlist_t attachmentDB_bottomRail_nameList;
	uint32_t* attachmentDB_bottomRail_idList = 0;
	stringlist_t attachmentDB_clip_nameList;
	uint32_t* attachmentDB_clip_idList = 0;

	static int selectedPrimaryWeaponIndex = -1;
	static int selectedSecondaryWeaponIndex = -1;
	static int selectedBodyIndex = 0;
	static int selectedHeroIndex = 0;
	static int selectedLegsIndex = 0;
	static int selectedArmorIndex = 0;
	static int selectedHeadIndex = 0;
	static int selectedHeadArmorIndex = 0;

	stringlist_t armorDBnames, bodyDBnames, heroDBnames, legsDBnames, headDBnames, headarmorBDnames;
	uint32_t *armorDBids = 0, *bodyDBids = 0, *heroDBids = 0, *legsDBids = 0, *headDBids = 0, *headarmorDBids = 0;
	if(weaponDB_nameList.empty()) // fill in list
	{
		weaponDB_nameList.push_back("empty");
		attachmentDB_muzzle_nameList.push_back("empty");
		attachmentDB_upperRail_nameList.push_back("empty");
		attachmentDB_leftRail_nameList.push_back("empty");
		attachmentDB_bottomRail_nameList.push_back("empty");
		attachmentDB_clip_nameList.push_back("empty");

		// sort item names there.
		struct CWpnNameSort {
			static int sort(const void* P1, const void* P2)	{
				const WeaponConfig* g1 = *(const WeaponConfig**)P1;
				const WeaponConfig* g2 = *(const WeaponConfig**)P2;
				return strcmp(g1->m_StoreIcon, g2->m_StoreIcon);
			}
		};
		struct CWpnAttachmentNameSort {
			static int sort(const void* P1, const void* P2)	{
				const WeaponAttachmentConfig* g1 = *(const WeaponAttachmentConfig**)P1;
				const WeaponAttachmentConfig* g2 = *(const WeaponAttachmentConfig**)P2;
				return strcmp(g1->m_StoreIcon, g2->m_StoreIcon);
			}
		};
		std::vector<const WeaponConfig*> allWpns;
		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const WeaponConfig* config = g_pWeaponArmory->getWeaponConfig(itemID);
			if(config)
			{
				allWpns.push_back(config);
			}
		}
		qsort(&allWpns[0], allWpns.size(), sizeof(WeaponConfig*), CWpnNameSort::sort);

		int numItemsInWeaponList = g_pWeaponArmory->getNumWeapons() + 1;
		weaponDB_idList = new uint32_t[numItemsInWeaponList];
		weaponDB_idList[0] = -1;
		for(int i=1; i<numItemsInWeaponList; ++i)
		{
			const WeaponConfig* config = allWpns[i-1];

			// get item FNAME without .dds
			char wname[256];
			sprintf(wname, strrchr(config->m_StoreIcon, '/') + 1);
			wname[strlen(wname)-4] = 0;

			weaponDB_nameList.push_back(wname);
			weaponDB_idList[i] = config->m_itemID;

			if(selectedPrimaryWeaponIndex==-1 && config->m_itemID == pl->CurLoadout.Items[0].itemID)
				selectedPrimaryWeaponIndex = i;
			if(selectedSecondaryWeaponIndex==-1 && config->m_itemID == pl->CurLoadout.Items[1].itemID)
				selectedSecondaryWeaponIndex = i;
		}

		if(selectedPrimaryWeaponIndex==-1)
			selectedPrimaryWeaponIndex = 0;
		if(selectedSecondaryWeaponIndex==-1)
			selectedSecondaryWeaponIndex = 0;

		int numItemsInAttachmentList = g_pWeaponArmory->getNumAttachments() + 1;
		attachmentDB_muzzle_idList = new uint32_t[numItemsInAttachmentList]; int numMuzzles = 0;
		attachmentDB_muzzle_idList[0] = 0;
		attachmentDB_upperRail_idList = new uint32_t[numItemsInAttachmentList]; int numUpperRails = 0;
		attachmentDB_upperRail_idList[0] = 0;
		attachmentDB_leftRail_idList = new uint32_t[numItemsInAttachmentList]; int numLeftRails = 0;
		attachmentDB_leftRail_idList[0] = 0;
		attachmentDB_bottomRail_idList = new uint32_t[numItemsInAttachmentList]; int numBottomRails = 0;
		attachmentDB_bottomRail_idList[0] = 0;
		attachmentDB_clip_idList = new uint32_t[numItemsInAttachmentList]; int numClips = 0;
		attachmentDB_clip_idList[0] = 0;

		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const WeaponAttachmentConfig* config = g_pWeaponArmory->getAttachmentConfig(itemID);
			if(config)
			{
				switch(config->m_type)
				{
				case WPN_ATTM_MUZZLE:
					attachmentDB_muzzle_nameList.push_back(config->m_StoreName);
					attachmentDB_muzzle_idList[++numMuzzles] = config->m_itemID;
					break;
				case WPN_ATTM_UPPER_RAIL:
					attachmentDB_upperRail_nameList.push_back(config->m_StoreName);
					attachmentDB_upperRail_idList[++numUpperRails] = config->m_itemID;
					break;
				case WPN_ATTM_LEFT_RAIL:
					attachmentDB_leftRail_nameList.push_back(config->m_StoreName);
					attachmentDB_leftRail_idList[++numLeftRails] = config->m_itemID;
					break;
				case WPN_ATTM_BOTTOM_RAIL:
					attachmentDB_bottomRail_nameList.push_back(config->m_StoreName);
					attachmentDB_bottomRail_idList[++numBottomRails] = config->m_itemID;
					break;
				case WPN_ATTM_CLIP:
					attachmentDB_clip_nameList.push_back(config->m_StoreName);
					attachmentDB_clip_idList[++numClips] = config->m_itemID;
					break;
				default:
					break;
				}
			}
		}
		int numItemsInGearList = g_pWeaponArmory->getNumGears() + 1;
		armorDBids = new uint32_t[numItemsInGearList]; int numArmor = 0;
		headarmorDBids = new uint32_t[numItemsInGearList]; int numHeadarmor = 0;

		headarmorBDnames.push_back("empty");
		armorDBnames.push_back("empty");
		armorDBids[0] = 0;
		headarmorDBids[0] = 0;

		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const GearConfig* config = g_pWeaponArmory->getGearConfig(itemID);
			if(config)
			{
				switch(config->category)
				{
				case storecat_Armor:
					armorDBnames.push_back(config->m_StoreName);
					armorDBids[++numArmor] = config->m_itemID;
					if(selectedArmorIndex==-1 && config->m_itemID == pl->CurLoadout.Items[6].itemID)
						selectedArmorIndex = numArmor;
					break;
				case storecat_Helmet:
					headarmorBDnames.push_back(config->m_StoreName);
					headarmorDBids[++numHeadarmor] = config->m_itemID;
					if(selectedHeadArmorIndex==-1 && config->m_itemID == pl->CurLoadout.Items[7].itemID)
						selectedHeadArmorIndex = numHeadarmor;
					break;
				default:
					break;
				}
			}
		}
		int numItemsInHeroList = g_pWeaponArmory->getNumHeroes();
		heroDBids = new uint32_t[numItemsInHeroList]; int numHero = 0;

		g_pWeaponArmory->startItemSearch();
		while(g_pWeaponArmory->searchNextItem())
		{
			uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
			const HeroConfig* config = g_pWeaponArmory->getHeroConfig(itemID);
			if(config)
			{
				heroDBnames.push_back(config->m_StoreName);
				heroDBids[numHero] = config->m_itemID;
				if(selectedHeroIndex==-1 && config->m_itemID == pl->CurLoadout.HeroItemID)
					selectedHeroIndex = numHero;
				numHero++;
			}
		}
	}

	static const int width = 150;
	//	static const int height = 150;
	static const int shift = 25;

	static int selectedPrimaryWeapon_Muzzle = 0;
	static int selectedPrimaryWeapon_UpperRail = 0;
	static int selectedPrimaryWeapon_LeftRail = 0;
	static int selectedPrimaryWeapon_BottomRail= 0;
	static int selectedPrimaryWeapon_Clip = 0;

	static int selectedSecondaryWeapon_Muzzle = 0;
	static int selectedSecondaryWeapon_UpperRail = 0;
	static int selectedSecondaryWeapon_LeftRail = 0;
	static int selectedSecondaryWeapon_BottomRail= 0;
	static int selectedSecondaryWeapon_Clip = 0;

	float offset = top + shift + height - 20;
	{
		static float primWeaponOffset = 0;
		imgui_Static(left + width * 7, top, "Primary Weapon", width);
		imgui_DrawList(left + width * 7, top + shift, (float)width, (float)height, weaponDB_nameList, &primWeaponOffset, &selectedPrimaryWeaponIndex);

		if(g_camera_mode->GetInt() == 2)
		{
			static float primWeaponAttachmentMuzzleOffset = 0;
			imgui_Static(left + width * 7, offset, "Muzzle", width);
			imgui_DrawList(left + width * 7, offset + 20, (float)width, (float)100.0f, attachmentDB_muzzle_nameList, &primWeaponAttachmentMuzzleOffset, &selectedPrimaryWeapon_Muzzle);

			static float primWeaponAttachmentUpperRailOffset = 0;
			imgui_Static(left + width * 7, offset + 100, "Upper Rail", width);
			imgui_DrawList(left + width * 7, offset + 20 + 100, (float)width, (float)100.0f, attachmentDB_upperRail_nameList, &primWeaponAttachmentUpperRailOffset, &selectedPrimaryWeapon_UpperRail);

			static float primWeaponAttachmentLeftRailOffset = 0;
			imgui_Static(left + width * 7, offset + 200, "Left Rail", width);
			imgui_DrawList(left + width * 7, offset + 20 + 200, (float)width, (float)100.0f, attachmentDB_leftRail_nameList, &primWeaponAttachmentLeftRailOffset, &selectedPrimaryWeapon_LeftRail);

			static float primWeaponAttachmentBottomRailOffset = 0;
			imgui_Static(left + width * 7, offset + 300, "Bottom Rail", width);
			imgui_DrawList(left + width * 7, offset + 20 + 300, (float)width, (float)100.0f, attachmentDB_bottomRail_nameList, &primWeaponAttachmentBottomRailOffset, &selectedPrimaryWeapon_BottomRail);

			static float primWeaponAttachmentClipOffset = 0;
			imgui_Static(left + width * 7, offset + 400, "Clip", width);
			imgui_DrawList(left + width * 7, offset + 20 + 400, (float)width, (float)100.0f, attachmentDB_clip_nameList, &primWeaponAttachmentClipOffset, &selectedPrimaryWeapon_Clip);
		}
	}

	{
		static float secWeaponOffset = 0;
		imgui_Static(left + width * 6, top, "Secondary Weapon", width);
		imgui_DrawList(left + width * 6, top + shift, (float)width, (float)height, weaponDB_nameList, &secWeaponOffset, &selectedSecondaryWeaponIndex);

		if(g_camera_mode->GetInt() == 2)
		{
			static float primWeaponAttachmentMuzzleOffset = 0;
			imgui_Static(left + width * 6, offset, "Muzzle", width);
			imgui_DrawList(left + width * 6, offset + 20, (float)width, (float)100.0f, attachmentDB_muzzle_nameList, &primWeaponAttachmentMuzzleOffset, &selectedSecondaryWeapon_Muzzle);

			static float primWeaponAttachmentUpperRailOffset = 0;
			imgui_Static(left + width * 6, offset + 100, "Upper Rail", width);
			imgui_DrawList(left + width * 6, offset + 20 + 100, (float)width, (float)100.0f, attachmentDB_upperRail_nameList, &primWeaponAttachmentUpperRailOffset, &selectedSecondaryWeapon_UpperRail);

			static float primWeaponAttachmentLeftRailOffset = 0;
			imgui_Static(left + width * 6, offset + 200, "Left Rail", width);
			imgui_DrawList(left + width * 6, offset + 20 + 200, (float)width, (float)100.0f, attachmentDB_leftRail_nameList, &primWeaponAttachmentLeftRailOffset, &selectedSecondaryWeapon_LeftRail);

			static float primWeaponAttachmentBottomRailOffset = 0;
			imgui_Static(left + width * 6, offset + 300, "Bottom Rail", width);
			imgui_DrawList(left + width * 6, offset + 20 + 300, (float)width, (float)100.0f, attachmentDB_bottomRail_nameList, &primWeaponAttachmentBottomRailOffset, &selectedSecondaryWeapon_BottomRail);

			static float primWeaponAttachmentClipOffset = 0;
			imgui_Static(left + width * 6, offset + 400, "Clip", width);
			imgui_DrawList(left + width * 6, offset + 20 + 400, (float)width, (float)100.0f, attachmentDB_clip_nameList, &primWeaponAttachmentClipOffset, &selectedSecondaryWeapon_Clip);
		}
	}

	static float heroOffset = 0;
	imgui_Static(left + width * 5, top, "Heroes", width);
	int oldIndex = selectedHeroIndex;
	imgui_DrawList(left + width * 5, top + shift, (float)width, (float)height, heroDBnames, &heroOffset, &selectedHeroIndex);
	if(oldIndex != selectedHeroIndex)
	{
		selectedBodyIndex = 0;
		selectedLegsIndex = 0;
		selectedHeadIndex = 0;
	}

	{
		int heroItemID = heroDBids[selectedHeroIndex];
		const HeroConfig* heroConf = g_pWeaponArmory->getHeroConfig(heroItemID);
		if(heroConf)
		{
			bodyDBids = new uint32_t[heroConf->getNumBodys()]; int numBody = 0;
			legsDBids = new uint32_t[heroConf->getNumLegs()]; int numLegs = 0;
			headDBids = new uint32_t[heroConf->getNumHeads()]; int numHead = 0;

			char tmpStr[16];
			for(size_t i=0; i<heroConf->getNumBodys(); ++i)
			{
				sprintf(tmpStr, "Body %d", i+1);
				bodyDBnames.push_back(tmpStr);
				bodyDBids[numBody] = i;
				if(pl->CurLoadout.BodyIdx == i)
					selectedBodyIndex = numBody;
				numBody++;
			}
			for(size_t i=0; i<heroConf->getNumLegs(); ++i)
			{
				sprintf(tmpStr, "Legs %d", i+1);
				legsDBnames.push_back(tmpStr);
				legsDBids[numLegs] = i;
				if(pl->CurLoadout.LegsIdx == i)
					selectedLegsIndex = numLegs;
				numLegs++;
			}
			for(size_t i=0; i<heroConf->getNumHeads(); ++i)
			{
				sprintf(tmpStr, "Head %d", i+1);
				headDBnames.push_back(tmpStr);
				headDBids[numHead] = i;
				if(pl->CurLoadout.HeadIdx == i)
					selectedHeadIndex = numHead;
				numHead++;
			}
		}
	}

	static float legsOffset = 0;
	imgui_Static(left + width * 4, top, "Legs", width);
	imgui_DrawList(left + width * 4, top + shift, (float)width, (float)height, legsDBnames, &legsOffset, &selectedLegsIndex);

	static float bodyOffset = 0;
	imgui_Static(left + width * 3, top, "Body", width);
	imgui_DrawList(left + width * 3, top + shift, (float)width, (float)height, bodyDBnames, &bodyOffset, &selectedBodyIndex);

	static float headOffset = 0;
	imgui_Static(left + width * 2, top, "Head", width);
	imgui_DrawList(left + width * 2, top + shift, (float)width, (float)height, headDBnames, &headOffset, &selectedHeadIndex);

	static float armorOffset = 0;
	imgui_Static(left + width * 1, top, "Armor", width);
	imgui_DrawList(left + width * 1, top + shift, (float)width, (float)height, armorDBnames, &armorOffset, &selectedArmorIndex);

	static float headArmorOffset = 0;
	imgui_Static(left + width * 0, top, "Head Armor", 150);
	imgui_DrawList(left + width * 0, top + shift, (float)width, (float)height, headarmorBDnames, &headArmorOffset, &selectedHeadArmorIndex);


	//if(imgui_Button(500, 550, 300, 30, "Apply Changes"))
	{
		wiCharDataFull loadout;
		loadout.Health = 100;
		loadout.Items[0].itemID = weaponDB_idList[selectedPrimaryWeaponIndex];
		loadout.Items[1].itemID = weaponDB_idList[selectedSecondaryWeaponIndex];
		loadout.Items[6].itemID = armorDBids[selectedArmorIndex];
		loadout.Items[7].itemID = headarmorDBids[selectedHeadArmorIndex];

		loadout.HeroItemID = heroDBids[selectedHeroIndex];
		loadout.HeadIdx = headDBids[selectedHeadIndex];
		loadout.BodyIdx = bodyDBids[selectedBodyIndex];
		loadout.LegsIdx = legsDBids[selectedLegsIndex];

		loadout.Attachment[0].attachments[WPN_ATTM_MUZZLE] = attachmentDB_muzzle_idList[selectedPrimaryWeapon_Muzzle];
		loadout.Attachment[0].attachments[WPN_ATTM_UPPER_RAIL] = attachmentDB_upperRail_idList[selectedPrimaryWeapon_UpperRail];
		loadout.Attachment[0].attachments[WPN_ATTM_LEFT_RAIL] = attachmentDB_leftRail_idList[selectedPrimaryWeapon_LeftRail];
		loadout.Attachment[0].attachments[WPN_ATTM_BOTTOM_RAIL] = attachmentDB_bottomRail_idList[selectedPrimaryWeapon_BottomRail];
		loadout.Attachment[0].attachments[WPN_ATTM_CLIP] = attachmentDB_clip_idList[selectedPrimaryWeapon_Clip];

		loadout.Attachment[1].attachments[WPN_ATTM_MUZZLE] = attachmentDB_muzzle_idList[selectedSecondaryWeapon_Muzzle];
		loadout.Attachment[1].attachments[WPN_ATTM_UPPER_RAIL] = attachmentDB_upperRail_idList[selectedSecondaryWeapon_UpperRail];
		loadout.Attachment[1].attachments[WPN_ATTM_LEFT_RAIL] = attachmentDB_leftRail_idList[selectedSecondaryWeapon_LeftRail];
		loadout.Attachment[1].attachments[WPN_ATTM_BOTTOM_RAIL] = attachmentDB_bottomRail_idList[selectedSecondaryWeapon_BottomRail];
		loadout.Attachment[1].attachments[WPN_ATTM_CLIP] = attachmentDB_clip_idList[selectedSecondaryWeapon_Clip];

		if(pl->CurLoadout.Items[0].itemID != loadout.Items[0].itemID ||
			pl->CurLoadout.Items[1].itemID != loadout.Items[1].itemID ||
			pl->CurLoadout.Items[6].itemID != loadout.Items[6].itemID ||
			pl->CurLoadout.Items[7].itemID != loadout.Items[7].itemID ||
			pl->CurLoadout.HeroItemID != loadout.HeroItemID ||
			pl->CurLoadout.HeadIdx != loadout.HeadIdx ||
			pl->CurLoadout.BodyIdx != loadout.BodyIdx ||
			pl->CurLoadout.LegsIdx != loadout.LegsIdx ||
			pl->CurLoadout.Attachment[0] != loadout.Attachment[0] || 
			pl->CurLoadout.Attachment[1] != loadout.Attachment[1]
		)
		{
			pl->UpdateLoadoutSlot(loadout);
		}
	}

	delete [] weaponDB_idList;
	delete [] attachmentDB_muzzle_idList;
	delete [] attachmentDB_upperRail_idList;
	delete [] attachmentDB_leftRail_idList;
	delete [] attachmentDB_bottomRail_idList;
	delete [] attachmentDB_clip_idList;
	delete [] armorDBids;
	delete [] bodyDBids;
	delete [] legsDBids;
	delete [] headDBids;
	delete [] headarmorDBids;
}

//------------------------------------------------------------------------

static void AdjustHitPosition( DecalParams& params, GameObject* target )
{
	R3DPROFILE_FUNCTION( "AdjustHitPosition" );
	if( target && target->isObjType( OBJTYPE_Mesh ) )
	{
		MeshGameObject* mobj = static_cast< MeshGameObject* >(target);

		float interSearchLength = mobj->GetObjectsRadius() * 0.33f;

		float dist;

		r3dMaterial* mtl;
		int minFace;

		r3dPoint3D start = params.Pos + params.Dir * interSearchLength * 0.5f;

		if( mobj->MeshLOD[ 0 ]->ContainsRay( start, -params.Dir, interSearchLength, &dist, &mtl, mobj->GetPosition(), mobj->GetRotationMatrix(), &minFace ) )
		{
			params.Pos = start - params.Dir * dist;
		}
	}
}

//------------------------------------------------------------------------

bool ProcessBulletHit( int &damageFromPiercable, GameObject* owner, const r3dPoint3D &hitPoint, const r3dPoint3D &hitNormal, GameObject* shootTarget, const r3dMaterial* shootMaterial, const char* hitActorName, const WeaponConfig *weaponInfo, const r3dPoint3D& muzzlerPosAtFireStart) 
{	
	r3d_assert(owner);
	r3d_assert(owner->NetworkLocal==true);

	obj_Player* ownerPlayer = NULL;
	if(owner->isObjType(OBJTYPE_Human))
		ownerPlayer = (obj_Player*)owner;

	if( shootTarget == NULL && shootMaterial == NULL) // hit nothing
	{
		PKT_C2C_PlayerHitNothing_s n;
		p2pSendToHost(owner, &n, sizeof(n), true);
	} 
	else if(shootTarget && shootTarget->isObjType(OBJTYPE_Human)) // hit other player
	{
		r3d_assert(shootTarget->GetNetworkID());
		obj_Player* trgt = (obj_Player*)shootTarget;

		if(!trgt->bDead)
		{
			PKT_C2C_PlayerHitDynamic_s n;
			n.muzzler_pos = muzzlerPosAtFireStart;
			n.hit_pos = hitPoint;
			n.targetId = toP2pNetId(shootTarget->GetNetworkID());
			n.hit_body_part = trgt->getBodyNameFromBoneName(hitActorName);
			n.hit_body_bone = trgt->uberAnim_->GetBoneID(hitActorName);
			n.damageFromPiercing = damageFromPiercable;
			// NOTE: This can be broken if the player jumps after firing a sniper rifle, but shouldn't happen as it's too fast.
			n.state = 0;
			if(ownerPlayer)
				n.state |= (!ownerPlayer->bOnGround)?(0x1): 0 ; // [0]-in air 
			n.state |= (!trgt->bOnGround)?0x2:0 ; // [1]-in air
			p2pSendToHost( owner, &n, sizeof(n), true);

			if(!trgt->bDead && ownerPlayer) 
			{
				ownerPlayer->m_HitMarkerFadeout = 1.0f;
			}
		}
		else
		{
			PKT_C2C_PlayerHitNothing_s n;
			p2pSendToHost(owner, &n, sizeof(n), true);
		}

		if(ownerPlayer)
		{
			{
				ownerPlayer->AddBloodOnGround(hitPoint);
				SpawnImpactParticle(r3dHash::MakeHash("player"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
			}
		}
	}
	else if(shootTarget && shootTarget->isObjType(OBJTYPE_Zombie)) // hit zombie
	{
		r3d_assert(shootTarget->GetNetworkID());

		PKT_C2C_PlayerHitDynamic_s n;
		n.muzzler_pos = muzzlerPosAtFireStart;
		n.hit_pos  = hitPoint;
		n.targetId = toP2pNetId(shootTarget->GetNetworkID());
		n.hit_body_part = strcmp("Bip01_Head", hitActorName) == 0 ? 1 : 0;
		if(ownerPlayer)
			n.hit_body_bone = ownerPlayer->uberAnim_->GetBoneID(hitActorName); // zombies use same skeleton as player
		else
			n.hit_body_bone = 0xFF;

		n.damageFromPiercing = damageFromPiercable;
		n.state = 0;

		p2pSendToHost(owner, &n, sizeof(n), true);

		if(owner)
		{
			obj_Zombie* z = (obj_Zombie*)shootTarget;

			// apply ragdoll force from object position, or up if too close
			r3dPoint3D zragHitPos = z->GetPosition();
			r3dPoint3D zlastTimeHitForce = r3dPoint3D(0, 1, 0);
			if(ownerPlayer)
			{
				if(n.hit_body_bone < ownerPlayer->GetSkeleton()->NumBones) // zombies use same skeleton as player
					zragHitPos = ownerPlayer->uberEquip_->getBonePos(n.hit_body_bone, ownerPlayer->GetSkeleton(), z->GetTransformMatrix());
				if((zragHitPos - ownerPlayer->GetPosition()).LengthSq() > 0.001f)
					zlastTimeHitForce = (zragHitPos - ownerPlayer->GetPosition()).NormalizeTo();
			}

			z->lastTimeHitPos  = zlastTimeHitForce;//ownerPlayer->GetPosition();
			z->lastTimeDmgType = weaponInfo->category;
			z->lastTimeHitBoneID = n.hit_body_bone;
			z->PlayHurtSound();
		}
		if(ownerPlayer)
		{
			{
				ownerPlayer->AddBloodOnGround(hitPoint);
				SpawnImpactParticle(r3dHash::MakeHash("zombie"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
			}
		}
	}
#ifndef FINAL_BUILD
	else if(shootTarget && shootTarget->Class->Name == "obj_ZombieDummy")
	{
		obj_ZombieDummy* trgt = (obj_ZombieDummy*)shootTarget;

		r3dPoint3D hitForce = (hitPoint - ownerPlayer->GetPosition()).NormalizeTo();
		int hit_body_bone = trgt->anim_.GetCurrentSkeleton()->GetBoneID(hitActorName);

		if(strcmp("Bip01_Head", hitActorName) == 0)
		{
			r3dPoint3D hitForce_amp = hitForce * 30;
			trgt->physSkeleton->SwitchToRagdollWithForce(true, hit_body_bone, &hitForce_amp);
		}
		else
		{
			trgt->DoHit();
		}

		if(ownerPlayer)
		{
			ownerPlayer->AddBloodOnGround(hitPoint);
			SpawnImpactParticle(r3dHash::MakeHash("player"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
		}
	}
#endif	
	else if(shootTarget && shootTarget->GetNetworkID() && canDamageTarget(shootTarget))
	{
		// hit something that have network Id
		r3d_assert(shootTarget->GetNetworkID());
		PKT_C2C_PlayerHitDynamic_s n;
		n.muzzler_pos = muzzlerPosAtFireStart;		
		n.hit_pos = hitPoint;
		n.targetId = toP2pNetId(shootTarget->GetNetworkID());
		n.hit_body_bone = 0;
		n.hit_body_part = 0;
		n.state = 0;
		n.damageFromPiercing = damageFromPiercable; 
		p2pSendToHost( owner, &n, sizeof(n), true);

		//TODO: add decals
		SpawnImpactParticle(r3dHash::MakeHash("Metal"),r3dHash::MakeHash(weaponInfo->m_PrimaryAmmo->getDecalSource()), hitPoint, hitNormal);
	} 
	else 
	{
		DecalParams params;
		params.Dir	= hitNormal;
		params.Pos	= hitPoint;
		params.TypeID	= INVALID_DECAL_ID;

		AdjustHitPosition( params, shootTarget );

		uint32_t sourceHash = r3dHash::MakeHash( weaponInfo->m_PrimaryAmmo->getDecalSource());
		uint32_t matHash    = 0;
		int      particleIdx= -1;

		r3dPoint3D waterSplashPos;
		extern bool TraceWater(const r3dPoint3D& start, const r3dPoint3D& finish, r3dPoint3D& waterSplashPos);
		if( TraceWater( r3dPoint3D(gCam.x, gCam.y, gCam.z), hitPoint, waterSplashPos))
		{	
			matHash = r3dHash::MakeHash("Thirst");
			particleIdx = SpawnImpactParticle(matHash, sourceHash, waterSplashPos, r3dPoint3D(0,1,0));
			extern void WaterSplash(const r3dPoint3D& waterSplashPos, float height, float size, float amount, int texIdx);
			waterSplashPos.y -= 0.1f;
			WaterSplash(waterSplashPos, 0.1f, 30.0f, 0.04f, 0);
		}
		else
		{
			{
				if( shootTarget && shootTarget->isObjType(OBJTYPE_Terrain) )
				{
					params.TypeID	= Terrain->GetDecalID(params.Pos, sourceHash);
					particleIdx = SpawnImpactParticle(matHash, sourceHash, hitPoint, hitNormal);
				}
				else
				{
					if( shootMaterial )
					{
						matHash = r3dHash::MakeHash(shootMaterial->TypeName);
						params.TypeID	= GetDecalID(matHash, sourceHash);
						particleIdx = SpawnImpactParticle(matHash, sourceHash, hitPoint, hitNormal);
					}
				}

				if( params.TypeID != INVALID_DECAL_ID )
				{
					g_pDecalChief->Add( params );
				}

				// add impulse
				if(shootTarget && shootTarget->PhysicsObject)
				{
					shootTarget->PhysicsObject->AddImpulseAtPos(-hitNormal*weaponInfo->m_AmmoSpeed*weaponInfo->m_AmmoMass/150.0f, hitPoint);
				}
			}
		}
		int hitPierceableObject = 0;
		if(shootTarget && shootTarget->m_BulletPierceable > 0  && damageFromPiercable == 0 && weaponInfo->category!=storecat_MELEE ) // melee cannot pierce objects
		{
			hitPierceableObject = shootTarget->m_BulletPierceable;
		}

		if(hitPierceableObject)
		{
			PKT_C2C_PlayerHitStaticPierced_s n; 
			n.hit_pos  = hitPoint;
			n.hit_norm = hitNormal;
			n.decalIdx = (BYTE)(params.TypeID + 1);  // -1 is invalid decal, bring to 0-xx range
			n.particleIdx = (BYTE)(particleIdx + 1); // same here
			n.hash_obj = shootTarget?shootTarget->GetHashID():0;
			p2pSendToHost( owner, &n, sizeof(n));

			damageFromPiercable = shootTarget->m_BulletPierceable;

			return false;

		}
		else
		{
			PKT_C2C_PlayerHitStatic_s n;
			n.hit_pos  = hitPoint;
			n.hit_norm = hitNormal;
			n.decalIdx = (BYTE)(params.TypeID + 1);  // -1 is invalid decal, bring to 0-xx range
			n.particleIdx = (BYTE)(particleIdx + 1); // same here
			n.hash_obj = shootTarget?shootTarget->GetHashID():0;
			p2pSendToHost( owner, &n, sizeof(n));
		}
	}

	return true;
}
#endif