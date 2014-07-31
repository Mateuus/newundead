#include "r3dpch.h"
#include "r3d.h"

#include "obj_ServerPlayer.h"

#include "ServerWeapons/ServerWeapon.h"
#include "ServerWeapons/ServerGear.h"
#include "../EclipseStudio/Sources/ObjectsCode/weapons/WeaponArmory.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"
#include "MasterServerLogic.h"

#include "ObjectsCode/obj_ServerPostBox.h"
#include "ObjectsCode/sobj_DroppedItem.h"
#include "ObjectsCode/sobj_Note.h"
#include "ObjectsCode/sobj_Grave.h"
#include "ObjectsCode/sobj_SafeLock.h"
#include "ObjectsCode/sobj_Animals.h"
#include "ObjectsCode/obj_ServerBarricade.h"
#include "sobj_Vehicle.h"
#include "AsyncFuncs.h"
#include "Async_Notes.h"

#include "../EclipseStudio/Sources/Gameplay_Params.h"
extern CGamePlayParams		GPP_Data;

IMPLEMENT_CLASS(obj_ServerPlayer, "obj_ServerPlayer", "Object");
AUTOREGISTER_CLASS(obj_ServerPlayer);

CVAR_COMMENT("_ai_", "AI variables");

obj_ServerPlayer::obj_ServerPlayer() : 
netMover(this, 0.2f, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
{
	ObjTypeFlags = OBJTYPE_Human;

	peerId_ = -1;
	startPlayTime_ = r3dGetTime();
	lastObjectLoad = r3dGetTime();

	m_PlayerFlyingAntiCheatTimer = 0.0f;
	m_PlayerUndergroundAntiCheatTimer = 0.0f;

	wasDisconnected_   = false;

	isHighPing = false;
	moveInited = false;

	r3dscpy(userName, "unknown");
	m_SelectedWeapon   = 0;
	m_ForcedEmptyHands = false;

	for(int i=0; i<NUM_WEAPONS_ON_PLAYER; i++)
		m_WeaponArray[i] = NULL;

	for(int i=0; i<SLOT_Max; i++)
		gears_[i] = NULL;

	lastTimeHit     = 0;
	lastHitBodyPart = 0;

	isTargetDummy_ = false;

	lastChatTime_    = -1;
	numChatMessages_ = 0;

	m_PlayerRotation = 0;

	lastCharUpdateTime_  = r3dGetTime();
	lastWorldUpdateTime_ = -1;
	lastWorldFlags_      = -1;
	lastVisUpdateTime_   = -1;

	haveBadBackpack_ = 0;

	return;
}

/*static unsigned int WINAPI UpdateVisData(void* param)
{
obj_ServerPlayer* plr = (obj_ServerPlayer*)param;

gServerLogic.UpdateNetObjVisData(plr);


return 0;
}*/

static unsigned int WINAPI UpdateVisData(void* param)
{
	obj_ServerPlayer* plr = (obj_ServerPlayer*)param;
	while (!plr->isDestroy)
	{
		if (plr)
		{
			gServerLogic.UpdateNetObjVisData(plr);
		}
		else
		{
			plr = NULL;
			return 0;
		}

		Sleep(1000);
	}
	//gServerLogic.ResetNetObjVisData(plr);
	plr = NULL;
	return 0;
}

BOOL obj_ServerPlayer::OnCreate()
{
	firstTime = true;
	lastCurTime = 0.0f;
	parent::OnCreate();
	DrawOrder	= OBJ_DRAWORDER_FIRST;

	SetVelocity(r3dPoint3D(0, 0, 0));

	r3d_assert(!NetworkLocal);

	myPacketSequence = 0;
	clientPacketSequence = 0;
	packetBarrierReason = "";

	FireHitCount = 0;

	lastWeapDataRep   = r3dGetTime();
	weapCheatReported = false;

	lastPlayerAction_ = r3dGetTime();
	m_PlayerState = 0;

	lastUpdate = 0;
	m_Stamina = GPP_Data.c_fSprintMaxEnergy;

	if(loadout_->Stats.skillid1 == 1){
		m_Stamina +=  25.5f;
		if(loadout_->Stats.skillid4 == 1)
			m_Stamina += 30.5f;
	}

	// set that character is alive
	loadout_->Alive   = 1;
	loadout_->GamePos = GetPosition();
	loadout_->GameDir = m_PlayerRotation;

	// invalidate last sended vitals
	lastVitals_.Health = 0xFF;
	lastVitals_.Hunger = 0xFF;

	gServerLogic.NetRegisterObjectToPeers(this);
	// detect what objects is visible right now
	gServerLogic.UpdateNetObjVisData(this);
	lastVisUpdateTime_ = r3dGetTime();
	LastHackShieldLog = r3dGetTime();

	// for initing cellMover there
	TeleportPlayer(GetPosition());

	isDestroy = false;

	//tThread = (HANDLE)_beginthreadex(NULL, 0, UpdateVisData, (void*)this, 0, NULL);
	/*UpdateVisThread = (HANDLE)_beginthreadex(NULL, 0, UpdateVisData, (void*)this, 0, NULL);*/

	/*PhysicsConfig.group = PHYSCOLL_CHARACTERCONTROLLER;
	PhysicsConfig.type = PHYSICS_TYPE_CONTROLLER;
	PhysicsConfig.mass = 100.0f;
	PhysicsConfig.ready = true;
	PhysicsObject = BasePhysicsObject::CreateCharacterController(PhysicsConfig, this);*/

	distToCreateSq = 1024 * 1024;
	distToDeleteSq = 1500 * 1500;


	return TRUE;
}


BOOL obj_ServerPlayer::OnDestroy()
{
	for (int i=0;i<72;i++)
	{
		TradeSlot[i] = 0;
		TradeItems[i].Reset();	
	}
	Tradetargetid = 0;
	//CloseHandle(tThread);
	isDestroy = true;
	isTradeAccept = false;
	//TerminateThread(tThread,-1);
	//gServerLogic.ResetNetObjVisData(this);
	return parent::OnDestroy();
}

BOOL obj_ServerPlayer::Load(const char *fname)
{
	if(!parent::Load(fname))
		return FALSE;

	// Object won't be saved when level saved
	bPersistent = 0;

	Height      = SRV_WORLD_SCALE (1.8f);

	RecalcBoundBox();

	return TRUE;
}
bool storecat_CanPlaceItemToSlot(const BaseItemConfig* itemCfg, int idx)
{
	bool canPlace = true;
	int cat = itemCfg->category;
	switch(idx)
	{
	case wiCharDataFull::CHAR_LOADOUT_WEAPON1:
		if(cat != storecat_ASR && cat != storecat_SNP && cat != storecat_SHTG && cat != storecat_MG && cat != storecat_SMG)// pt: temp, ui doesn't support that && cat != storecat_HG)
			canPlace = false;
		break;
	case wiCharDataFull::CHAR_LOADOUT_WEAPON2:
		if(cat != storecat_MELEE && cat != storecat_HG)
			canPlace = false;
		break;
	case wiCharDataFull::CHAR_LOADOUT_ITEM1:
	case wiCharDataFull::CHAR_LOADOUT_ITEM2:
	case wiCharDataFull::CHAR_LOADOUT_ITEM3:
	case wiCharDataFull::CHAR_LOADOUT_ITEM4:
		//if(cat != storecat_UsableItem)
		//	canPlace = false;
		canPlace = true;
		break;
	case wiCharDataFull::CHAR_LOADOUT_ARMOR:
		if(cat != storecat_Armor)
			canPlace = false;
		break;
	case wiCharDataFull::CHAR_LOADOUT_HEADGEAR:
		if(cat != storecat_Helmet)
			canPlace = false;
		break;
	}

	return canPlace;
}
void obj_ServerPlayer::SetProfile(const CServerUserProfile& in_profile)
{
	profile_ = in_profile;
	loadout_ = &profile_.ProfileData.ArmorySlots[0];
	savedLoadout_ = *loadout_;

	// those was already checked in GetProfileData, but make sure about that  
	r3d_assert(profile_.ProfileData.ArmorySlots[0].LoadoutID);
	r3d_assert(profile_.ProfileData.NumSlots == 1);
	r3d_assert(loadout_->LoadoutID > 0);
	r3d_assert(loadout_->Alive > 0);

	r3dscpy(userName, loadout_->Gamertag);

	boostXPBonus_          = 0.0f; // % to add
	boostWPBonus_          = 0.0f; // % to add

	//r3dOutToLog("SetProfile %s\n", userName); CLOG_INDENT;

	ValidateBackpack();
	ValidateAttachments();
	SetLoadoutData();

	return;
}

void obj_ServerPlayer::ValidateBackpack()
{
	for(int i=0; i<loadout_->CHAR_MAX_BACKPACK_SIZE; i++)
	{
		wiInventoryItem& wi = loadout_->Items[i];
		if(wi.itemID == 0)
			continue;

		if(g_pWeaponArmory->getConfig(wi.itemID) == NULL)
		{
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, false, "ValidateBackpack",
				"%d", wi.itemID);
			wi.Reset();

			haveBadBackpack_ = 1;
		}
	}
}

void obj_ServerPlayer::ValidateAttachments()
{
	for(int i=0; i<2; i++)
	{
		for(int j=0; j<WPN_ATTM_MAX; j++)
		{
			uint32_t itm = loadout_->Attachment[i].attachments[j];
			if(itm == 0)
				continue;

			if(g_pWeaponArmory->getAttachmentConfig(itm) == NULL)
			{
				gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, false, "ValidateAttachments",
					"%d", itm);

				loadout_->Attachment[i].attachments[j] = 0;

				haveBadBackpack_ = 1;
			}
		}
	}
}

void obj_ServerPlayer::DoDeath()
{
	gServerLogic.LogInfo(peerId_, "Death", ""); CLOG_INDENT;

	deathTime     = r3dGetTime();

	// set that character is dead
	loadout_->Alive   = 0;
	loadout_->GamePos = GetPosition();
	loadout_->Health  = 0;
	// clear attachments
	loadout_->Attachment[0].Reset();
	loadout_->Attachment[1].Reset();
	//NOTE: server WZ_Char_SRV_SetStatus will clear player backpack, so make that CJobUpdateChar::Exec() won't update it
	savedLoadout_ = *loadout_;

	bool isDropSNP  = u_GetRandom() >= 0.90f ? false : true;
	if (!profile_.ProfileData.isDevAccount)
	{

		// drop all items
		for(int i=0; i<loadout_->BackpackSize; i++)
		{
			const wiInventoryItem& wi = loadout_->Items[i];
			if(wi.itemID > 0) {
				/*if (g_pWeaponArmory->getConfig(wi.itemID) && g_pWeaponArmory->getConfig(wi.itemID)->category == storecat_SNP)
				{
				if (isDropSNP)
				BackpackDropItem(i);
				else
				isDropSNP = true;
				}
				else*/
				BackpackDropItem(i);


			}
		}
		DoRemoveAllItems(this);
		gServerLogic.ApiPlayerUpdateChar(this);

		// drop not-default backpack as well
		if(loadout_->BackpackID != 20176)
		{
			// create network object
			obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", GetRandomPosForItemDrop());
			obj->SetNetworkID(gServerLogic.GetFreeNetId());
			obj->NetworkLocal = true;
			// vars
			obj->m_Item.itemID   = loadout_->BackpackID;
			obj->m_Item.quantity = 1;
		}
	}

	

	gServerLogic.ApiPlayerUpdateChar(this);

	SetLatePacketsBarrier("death");

	return;
}

wiStatsTracking obj_ServerPlayer::AddReward(const wiStatsTracking& in_rwd)
{
	float XPBonus = boostXPBonus_;
	float WPBonus = boostWPBonus_;

	wiStatsTracking rwd = in_rwd;
	// round up. basically if we award only 2 points, with +25% it would give us 0, so, let's make users more happy by round up
	// in case if that will be a balancing problem - we can always round it down with floorf
	rwd.XP += int(ceilf(R3D_ABS(rwd.XP)*XPBonus));
	rwd.GP += int(ceilf(rwd.GP*WPBonus));
	rwd.GD += int(ceilf(rwd.GD*WPBonus));

	// adjust player stats
	if (profile_.ProfileData.isPremium && gServerLogic.ginfo_.ispre)
	{
		loadout_->Stats.XP  += rwd.XP*2;
	}
	else
	{
		loadout_->Stats.XP += rwd.XP;
	}
	profile_.ProfileData.GamePoints  += rwd.GP;
	profile_.ProfileData.GameDollars += rwd.GD;

	//loadout_->Stats.XP += rwd.XP;

	return rwd;
}

wiNetWeaponAttm	obj_ServerPlayer::GetWeaponNetAttachment(int wid)
{
	wiNetWeaponAttm atm;

	const ServerWeapon* wpn = m_WeaponArray[wid];
	if(!wpn)
		return atm;

	if(wpn->m_Attachments[WPN_ATTM_LEFT_RAIL])
		atm.LeftRailID = wpn->m_Attachments[WPN_ATTM_LEFT_RAIL]->m_itemID;
	if(wpn->m_Attachments[WPN_ATTM_MUZZLE])
		atm.MuzzleID = wpn->m_Attachments[WPN_ATTM_MUZZLE]->m_itemID;

	return atm;
}

float obj_ServerPlayer::CalcWeaponDamage(const r3dPoint3D& shootPos)
{
	// calc damaged based on weapon
	// decay damage based from distance from player to target
	float dist   = (GetPosition() - shootPos).Length();
	float damage = 0;
	if(m_WeaponArray[m_SelectedWeapon])
		damage = m_WeaponArray[m_SelectedWeapon]->calcDamage(dist);

	return damage;
}

bool obj_ServerPlayer::FireWeapon(int wid, bool wasAiming, int executeFire, DWORD targetId, const char* pktName)
{
	r3d_assert(loadout_->Alive);

	lastPlayerAction_ = r3dGetTime();

	if(targetId)
	{
		GameObject* targetObj = GameWorld().GetNetworkObject(targetId);
		if(targetObj == NULL) 
		{
			// target already disconnected (100% cases right now) or invalid.
			return false;
		}
	}

	if(wid < 0 || wid >= NUM_WEAPONS_ON_PLAYER)
	{
		gServerLogic.LogInfo(peerId_, "wid invalid", "%s %d", pktName, wid);
		return false;
	}

	if(wid != m_SelectedWeapon) 
	{
		// just log for now... we'll see how much mismatches we'll get
		gServerLogic.LogInfo(peerId_, "wid mismatch", "%s %d vs %d", pktName, wid, m_SelectedWeapon);
	}

	if(m_ForcedEmptyHands)
	{
		gServerLogic.LogInfo(peerId_, "empty hands", "%s %d vs %d", pktName, wid, m_SelectedWeapon);
		return false;
	}

	ServerWeapon* wpn = m_WeaponArray[wid];
	if(wpn == NULL) 
	{
		gServerLogic.LogInfo(peerId_, "no wid", "%s %d", pktName, wid);
		return false;
	}

	// can't fire in safe zones 
	if(loadout_->GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox)
	{
		return false;
	}
	// melee - infinite ammo
	if(wpn->getCategory() == storecat_MELEE)
	{
		gServerLogic.TrackWeaponUsage(wpn->getConfig()->m_itemID, 1, 0, 0);

		FireHitCount++;
		return true;
	}

	if(wpn->getCategory() == storecat_GRENADE) // grenades are treated as items
	{
		gServerLogic.TrackWeaponUsage(wpn->getConfig()->m_itemID, 1, 0, 0);

		wiInventoryItem& wi = wpn->getPlayerItem();

		// remove used item
		wi.quantity--;
		if(wi.quantity <= 0) {
			wi.Reset();
		}

		return true;
	}

	if(wpn->getClipConfig() == NULL)
	{
		gServerLogic.LogInfo(peerId_, "no clip", "%s %d", pktName, wid);
		return false;
	}

	// incr fire count, decremented on hit event
	//r3dOutToLog("FireHitCount++\n");
	FireHitCount++;

	// track ShotsFired
	loadout_->Stats.ShotsFired++;

	gServerLogic.TrackWeaponUsage(wpn->getConfig()->m_itemID, 1, 0, 0);

	if(executeFire && gServerLogic.weaponDataUpdates_ < 2)
	{
		// check if we fired more that we was able
		wiInventoryItem& wi = wpn->getPlayerItem();
		if(wi.Var1 <= 0)
		{
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NumShots, true, "bullethack",
				"%d/%d clip:%d(%s)", 
				wi.Var1,
				wpn->getClipConfig()->m_Clipsize,
				wpn->getClipConfig()->m_itemID,
				wpn->getClipConfig()->m_StoreName
				);
			return false;
		}

		wpn->getPlayerItem().Var1--;
	}

	return true;
}

float obj_ServerPlayer::ReduceDamageByGear(int gslot, float damage)
{
	if(gears_[gslot] == NULL)
		return damage;

	float armorProtectionMul = 1.0f;
	float armorProtectionAdd = 0.0f;

	float new_damage = gears_[gslot]->GetDamage(damage, armorProtectionMul, armorProtectionAdd, isTargetDummy_);
	return new_damage;
}

// params should be in [0..360] range
float getMinimumAngleDistance(float from, float to)
{
	float d = to - from;
	if(d <-180.0f)	d += 360.0f;
	if(d > 180.0f)	d -= 360.0f;
	return d;
}


float obj_ServerPlayer::ApplyDamage(float damage, GameObject* fromObj, int bodyPart, STORE_CATEGORIES damageSource)
{
	lastTimeHit     = r3dGetTime();
	lastHitBodyPart = bodyPart;

	//r3dOutToLog("Player(%s) received gamage\n", userName); CLOG_INDENT;
	//r3dOutToLog("raw damage(%.2f) at part (%d)\n", damage, bodyPart);

	// adjust damage based on hit part
	if(damageSource != storecat_MELEE)
	{
		switch(bodyPart) 
		{
		case 1: // head
			damage *= 2;
			break;

			// case 2: // hands
		case 3: // legs
			damage *= 0.75f;
			if (damage > 20)
			{
				// loadout_->legfall = true;
			}
			break;
		}

		if (damage > 15 && !loadout_->bleeding)
		{
			loadout_->bleeding = u_GetRandom() >= 0.20f ? false : true;
		}
	}

	// reduce damage by armor		
	switch(bodyPart)
	{
	case 1: // head
		damage = ReduceDamageByGear(SLOT_Headgear, damage);
		break;

	default:
		//damage = ReduceDamageByGear(SLOT_Char,  damage);
		damage = ReduceDamageByGear(SLOT_Armor, damage);
		break;
	}

	if(loadout_->Stats.skillid0 == 1)
	{
		//r3dOutToLog("Player %s has learned Skill: Alive and Well 1 - removing 0.1% from Damage!\n", loadout_->Gamertag);
		damage *= 0.9f;
		if(loadout_->Stats.skillid7 == 1)
			damage *= 0.8f;

		if (profile_.ProfileData.isPunisher)
		{
			damage *= 0.6f;
		}
	}
	else
	{
		if (profile_.ProfileData.isPunisher)
		{
			damage *= 0.7f;
		}
	}
	// r3dOutToLog("gear adjusted damage(%.2f)\n", damage);

	if(damage < 0)
		damage = 0;

	// reduce health, now works for target dummy as well!
	if (!gServerLogic.ginfo_.isfarm)
	{
		if(!isTargetDummy_ || 1)
			loadout_->Health -= damage;
	}

	//r3dOutToLog("%s damaged by %s by %.1f points, %.1f left\n", userName, fromObj->Name.c_str(), damage, m_Health);

	return damage;    
}

void obj_ServerPlayer::SetWeaponSlot(int wslot, uint32_t weapId, const wiWeaponAttachment& attm)
{
	r3d_assert(wslot < NUM_WEAPONS_ON_PLAYER);
	SAFE_DELETE(m_WeaponArray[wslot]);

	if(weapId == 0)
		return;

	const WeaponConfig* weapCfg = g_pWeaponArmory->getWeaponConfig(weapId);
	if(weapCfg == NULL) {
		r3dOutToLog("!!! %s does not have weapon id %d\n", userName, weapId);
		return;
	}

	//r3dOutToLog("Creating wpn %s\n", weapCfg->m_StoreName); CLOG_INDENT;
	m_WeaponArray[wslot] = new ServerWeapon(weapCfg, this, wslot, attm);

	if(weapCfg->category != storecat_MELEE)
	{
		if(m_WeaponArray[wslot]->getClipConfig() == NULL) {
			r3dOutToLog("!!! weapon id %d does not have default clip attachment\n", weapId);
		}
	}

	return;
}

void obj_ServerPlayer::SetGearSlot(int gslot, uint32_t gearId)
{
	r3d_assert(gslot >= 0 && gslot < SLOT_Max);
	SAFE_DELETE(gears_[gslot]);

	if(gearId == 0)
		return;

	if(g_pWeaponArmory->getGearConfig(gearId) == NULL) {
		r3dOutToLog("!!! %s does not have gear id %d\n", userName, gearId);
		return;
	}

	gears_[gslot] = g_pWeaponArmory->createGear(gearId);
	return;
}

void obj_ServerPlayer::SetLoadoutData()
{
	wiCharDataFull& slot = profile_.ProfileData.ArmorySlots[0];
	slot.GroupID = 0;
	slot.isVisible = true;
	//slot.IsCallForHelp = false;
	//@ FOR NOW, attachment are RESET on entry. need to detect if some of them was dropped
	// (SERVER CODE SYNC POINT)
	slot.Attachment[0] = wiWeaponAttachment();
	if(slot.Items[0].Var2 > 0)
		slot.Attachment[0].attachments[WPN_ATTM_CLIP] = slot.Items[0].Var2;

	slot.Attachment[1] = wiWeaponAttachment();
	if(slot.Items[1].Var2 > 0)
		slot.Attachment[1].attachments[WPN_ATTM_CLIP] = slot.Items[1].Var2;

	SetWeaponSlot(0, slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID, slot.Attachment[0]);
	SetWeaponSlot(1, slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID, slot.Attachment[1]);

	//SetGearSlot(SLOT_Char,     slot.HeroItemID);
	SetGearSlot(SLOT_Armor,    slot.Items[wiCharDataFull::CHAR_LOADOUT_ARMOR].itemID);
	SetGearSlot(SLOT_Headgear, slot.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID);


	return;
}
void obj_ServerPlayer::DoRemoveItems(int slotid)
{
	PKT_S2C_BackpackModify_s n;
	n.SlotTo     = slotid;
	n.Quantity   = 0;
	n.dbg_ItemID = loadout_->Items[slotid].itemID;
	gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));
	loadout_->Items[slotid].Reset();
	OnBackpackChanged(slotid);
}
void obj_ServerPlayer::DoRemoveAllItems(obj_ServerPlayer* plr)
{
	for (int i =0;i<72;i++)
	{
		PKT_S2C_BackpackModify_s n;
		n.SlotTo     = i;
		n.Quantity   = 0;
		n.dbg_ItemID = plr->loadout_->Items[i].itemID;
		gServerLogic.p2pSendToPeer(plr->peerId_, plr, &n, sizeof(n));
		plr->loadout_->Items[i].Reset();
		plr->OnBackpackChanged(i);
	}
}


void obj_ServerPlayer::DoTrade(obj_ServerPlayer* plr , obj_ServerPlayer* plr2)
{

	//if (!plr->isTradeAccept) return;

	if (!plr || !plr2) return;


	r3dOutToLog("DoTrade(%s,%s)\n",plr->userName,plr2->userName);

	// check a trade data and backpack is match.

	/*for (int i =0;i<72;i++)
	{
	if (plr->loadout_->Items[plr->TradeSlot[i]].itemID != plr->TradeItems[i].itemID && plr->TradeItems[i].itemID != 0
	|| plr->loadout_->Items[plr->TradeSlot[i]].quantity != plr->TradeItems[i].quantity && plr->TradeItems[i].quantity != 0) // mismatch disconnect and cancel it.
	{
	gServerLogic.DisconnectPeer(plr->peerId_,true,"Trade itemid mismatch %d %d ",plr->loadout_->Items[plr->TradeSlot[i]].itemID,plr->TradeItems[i].itemID);
	return;
	}
	if (plr2->loadout_->Items[plr2->TradeSlot[i]].itemID != plr2->TradeItems[i].itemID && plr2->TradeItems[i].itemID != 0 
	|| plr2->loadout_->Items[plr2->TradeSlot[i]].quantity != plr2->TradeItems[i].quantity && plr2->TradeItems[i].quantity != 0) // mismatch disconnect and cancel it.
	{
	gServerLogic.DisconnectPeer(plr2->peerId_,true,"Trade itemid mismatch %d %d",plr2->loadout_->Items[plr2->TradeSlot[i]].itemID,plr2->TradeItems[i].itemID);
	return;
	}
	}*/

	// check slot and weight
	float totalTradeweight = 0;
	bool freeWeight = false;
	float totalTradeweight2 = 0;
	bool freeWeight2 = false;
	for (int i =0;i<72;i++)
	{
		if (plr->TradeItems[i].itemID == 0)
			continue;
		wiInventoryItem item = plr->TradeItems[i];
		const BaseItemConfig* itemCfg = g_pWeaponArmory->getConfig(item.itemID);
		if (itemCfg)
		{
			totalTradeweight += itemCfg->m_Weight;
		}
	}
	for (int i =0;i<72;i++)
	{
		if (plr2->TradeItems[i].itemID == 0)
			continue;
		wiInventoryItem item = plr2->TradeItems[i];
		const BaseItemConfig* itemCfg = g_pWeaponArmory->getConfig(item.itemID);
		if (itemCfg)
		{
			totalTradeweight2 += itemCfg->m_Weight;
		}
	}
	// totalweight 2 is plr2 trade weight.
	/*const BackpackConfig* tbc = g_pWeaponArmory->getBackpackConfig(plr2->loadout_->BackpackID);
	if (tbc && plr2->loadout_->getTotalWeight()-totalTradeweight2+totalTradeweight <= tbc->m_maxWeight)
	{
	freeWeight2 = true;
	}
	const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(plr->loadout_->BackpackID);
	if (bc && plr->loadout_->getTotalWeight()-totalTradeweight+totalTradeweight2 <= bc->m_maxWeight)
	{
	freeWeight = true;
	}

	if (!freeWeight || !freeWeight2) // player have a problem.
	{
	char chatmessage[128] = {0};
	PKT_C2C_ChatMessage_s n;
	sprintf(chatmessage, "Other player cannot carry so much");
	r3dscpy(n.gamertag, "<Trade>");
	r3dscpy(n.msg, chatmessage);
	n.msgChannel = 0;
	n.userFlag = 0;
	gServerLogic.p2pSendRawToPeer(plr->peerId_,&n,sizeof(n));
	gServerLogic.p2pSendRawToPeer(plr2->peerId_,&n,sizeof(n));
	return;
	}*/

	// trading...
	// NOTE : NOT FORGOT TRADELOG!

	// before trade. we need to save old Items of players.
	wiInventoryItem Items1[72];
	wiInventoryItem Items2[72];

	for (int i=0; i<72;i++)
	{
		Items1[i].Reset();
		Items2[i].Reset();
		Items1[i] = plr->loadout_->Items[i];
		Items2[i] = plr2->loadout_->Items[i];
	}

	bool Failed1 = false;
	bool Failed2 = false;
	for (int i=0; i<72;i++)
	{
		if (plr->TradeItems[i].itemID == 0 || plr->TradeSlot[i] == -1)
			continue;

		if (plr->loadout_->Items[plr->TradeSlot[i]].itemID != plr->TradeItems[i].itemID)
			continue;

		if (plr2->BackpackAddItem(plr->TradeItems[i]))
		{
			g_AsyncApiMgr->AddJob(new CJobTradeLog(plr->profile_.CustomerID,plr2->profile_.CustomerID,plr->loadout_->LoadoutID,plr2->loadout_->LoadoutID,plr->userName,plr2->userName,gServerLogic.ginfo_.gameServerId,plr->TradeItems[i]));
			plr->DoRemoveItems(plr->TradeSlot[i]);
		}
		else
			Failed2 = true;

		plr->TradeItems[i].Reset();
	}
	for (int i=0; i<72;i++)
	{
		if (plr2->TradeItems[i].itemID == 0 || plr2->TradeSlot[i] == -1)
			continue;

		if (plr2->loadout_->Items[plr2->TradeSlot[i]].itemID != plr2->TradeItems[i].itemID)
			continue;

		if (plr->BackpackAddItem(plr2->TradeItems[i]))
		{
			g_AsyncApiMgr->AddJob(new CJobTradeLog(plr2->profile_.CustomerID,plr->profile_.CustomerID,plr2->loadout_->LoadoutID,plr->loadout_->LoadoutID,plr2->userName,plr->userName,gServerLogic.ginfo_.gameServerId,plr2->TradeItems[i]));
			plr2->DoRemoveItems(plr2->TradeSlot[i]);
		}
		else
			Failed1 = true;

		plr2->TradeItems[i].Reset();
	}
	if (Failed1 || Failed2)
	{
		DoRemoveAllItems(plr);
		DoRemoveAllItems(plr2);

		for (int i=0;i<72;i++)
		{
			if (Items1[i].itemID != 0)
				plr->BackpackAddItem(Items1[i]);

			if (Items2[i].itemID != 0)
				plr2->BackpackAddItem(Items2[i]);
		}

		char chatmessage[128] = {0};
		PKT_C2C_ChatMessage_s n;
		sprintf(chatmessage, "Other player does not have enough free backpack slots");
		r3dscpy(n.gamertag, "<Trade>");
		r3dscpy(n.msg, chatmessage);
		n.msgChannel = 0;
		n.userFlag = 0;
		gServerLogic.p2pSendRawToPeer(plr->peerId_,&n,sizeof(n));
		gServerLogic.p2pSendRawToPeer(plr2->peerId_,&n,sizeof(n));
	}
	gServerLogic.ApiPlayerUpdateChar(plr2);
	gServerLogic.ApiPlayerUpdateChar(plr);
}



BOOL obj_ServerPlayer::Update()
{
	//if (lastUpdate + 0.5f > r3dGetTime()) return true;

	//  lastUpdate = r3dGetTime();
	parent::Update();

	const float timePassed = r3dGetFrameTime();
	const float curTime = r3dGetTime();

	// pereodically update network objects visibility

	if (loadout_->Alive == 0 && deathTime + 60.0f < r3dGetTime())
	{
		gServerLogic.DisconnectPeer(peerId_,false,"Death Time Left");
		return true;
	}

	if (isLeaveGroup && LeaveGroupTime < r3dGetTime())
	{
		isLeaveGroup = false;
		loadout_->GroupID = 0;
	}
	if (isTradeAccept)
	{
		GameObject* obj = GameWorld().GetNetworkObject(Tradetargetid);
		if (IsServerPlayer(obj))
		{
			obj_ServerPlayer* plr = (obj_ServerPlayer*)obj;
			if (plr)
			{
				if (plr->isTradeAccept && isTradeAccept && plr->Tradetargetid == GetNetworkID() && Tradetargetid == plr->GetNetworkID() && plr != this && plr->Tradetargetid != 0 && Tradetargetid != 0)
				{
					// start tradeing..
					this->DoTrade(this,plr);

					// trade finished 
					for (int i=0;i<72;i++)
					{
						this->TradeSlot[i] = 0;
						this->TradeItems[i].Reset();
						plr->TradeSlot[i] = 0;
						plr->TradeItems[i].Reset();	
					}

					this->Tradetargetid = 0;
					plr->Tradetargetid = 0;
					this->isTradeAccept = false;
					plr->isTradeAccept = false;
					PKT_C2S_TradeSuccess_s n;
					gServerLogic.p2pSendToPeer(plr->peerId_,plr,&n,sizeof(n));
					gServerLogic.p2pSendToPeer(peerId_,this,&n,sizeof(n));
				}
			}
		}
	}
	if (loadout_->GroupID != 0)
	{
		int count = 0;
		bool haveLeader = false;
		for(int i=0; i<gServerLogic.MAX_PEERS_COUNT; i++)
		{
			obj_ServerPlayer* plr = gServerLogic.peers_[i].player;
			if(!plr)
				continue;
			if (plr->loadout_->GroupID == loadout_->GroupID)
				count++;

			if (plr->loadout_->LoadoutID == loadout_->GroupID)
				haveLeader = true;
		}
		if (count <= 1)
		{
			loadout_->GroupID = 0;
		}
		else
		{
			if (!haveLeader)
			{
				int oldGroupID = loadout_->GroupID;
				loadout_->GroupID = loadout_->LoadoutID;

				// send this before , client need to know who is leader at now.
				PKT_C2S_GroupID_s n;
				n.GroupID = loadout_->GroupID;
				n.peerId = peerId_;
				gServerLogic.p2pSendToPeer(peerId_,this,&n,sizeof(n));

				// send swap notify message to leader
				{
					PKT_C2S_GroupSwapLeader_s n;
					n.peerId = peerId_;
					gServerLogic.p2pSendToPeer(peerId_,this,&n,sizeof(n));
				}

				for(int i=0; i<gServerLogic.MAX_PEERS_COUNT; i++)
				{
					obj_ServerPlayer* plr = gServerLogic.peers_[i].player;
					if(!plr)
						continue;
					if (plr->loadout_->GroupID == oldGroupID && oldGroupID != 0 && plr->loadout_->GroupID != 0)
					{
						if (plr != this) // not for leader
							plr->loadout_->GroupID = loadout_->GroupID;

						// send this before , client need to know who is leader at now.
						PKT_C2S_GroupID_s n;
						n.GroupID = plr->loadout_->GroupID;
						n.peerId = plr->peerId_;
						gServerLogic.p2pSendToPeer(peerId_,plr,&n,sizeof(n));

						// send swap notify message to all player in group.
						{
							PKT_C2S_GroupSwapLeader_s n;
							n.peerId = peerId_;
							gServerLogic.p2pSendToPeer(plr->peerId_,plr,&n,sizeof(n));
						}
					}
				}
			}
		}
	}
	//if (loadout_->Alive != 0)
	//{

	if (r3dGetTime() > SendGroupIDTime + 0.1f)
	{
		for(int i=0; i<gServerLogic.MAX_PEERS_COUNT; i++)
		{
			obj_ServerPlayer* plr = gServerLogic.peers_[i].player;
			if(!plr)
				continue;
			if (plr != this)
			{
				if (plr->loadout_->GroupID == this->loadout_->GroupID && this->loadout_->GroupID != 0)
				{
					INetworkHelper* nh = plr->GetNetworkHelper();
					if (nh && nh->PeerVisStatus[peerId_] == 0)
					{
						nh->PeerVisStatus[peerId_] = 1;
						int packetSize = 0;
						DefaultPacket* packetData = nh->NetGetCreatePacket(&packetSize);
						if(packetData)
						{
							gServerLogic.net_->SendToPeer(packetData, packetSize, peerId_, true);
						}
					}

					PKT_C2S_GroupID_s n;
					n.GroupID = plr->loadout_->GroupID;
					n.peerId = plr->peerId_;
					gServerLogic.p2pSendToPeer(peerId_,this,&n,sizeof(n));
				}
			}
		}
		SendGroupIDTime = r3dGetTime();
	}
	//}
	// check if warguard not running (bypass tprogame)
	/*if (profile_.ProfileData.WarGuardSession != profile_.SessionID)
	{
	PKT_S2C_CheatMsg_s n2;
	char msg [512];
	sprintf(msg,"WarGuard Kicked '%s' [FOR 0 minute] : WarGuard Service NOT Running.",userName);
	r3dscpy(n2.cheatreason,msg); // WTF? 0 minute?
	gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));

	gServerLogic.DisconnectPeer(peerId_, true, "WarGuardSession mismatch %d , %d",profile_.ProfileData.WarGuardSession,profile_.SessionID);
	return 0;
	}*/
	if (r3dGetTime() >  LastHackShieldLog + 20.0f && (((r3dGetTime() - startPlayTime_) >= 90.0f)) && loadout_->Alive != 0)
	{
		PKT_S2C_CheatMsg_s n2;
		r3dscpy(n2.cheatreason,"HackShield Kicked from server. : HackShield Service not responding.");
		gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));

		gServerLogic.DisconnectPeer(peerId_, false, "HackShield Late.");
	}

	/*for(int i=0; i<loadout_->BackpackSize; i++) // scan all items.
	{
	const wiInventoryItem& wi = loadout_->Items[i];
	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(wi.itemID);
	if (wc) // it's weapons.
	{
	if (wi.Var2 != -1)
	{
	const WeaponAttachmentConfig* cfg = g_pWeaponArmory->getAttachmentConfig(wi.Var2);
	if (cfg)
	{
	if (wi.Var1 > cfg->m_Clipsize) // he cheat!
	{
	// he will get force banned and disconnect , he never see a message. not send message to him.
	g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,"Bullet Cheat."));
	// sent banned notice to all servers.
	// NOTE : NOT NEED SEND A CHAT MESSAGE AT NOW. MASTER WILL SEND A MESSAGE PACKET AGAIN AND WE WILL BROSTCAST IT TO OTHER PLAYER.
	gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s' : bullet cheat.",userName);

	gServerLogic.DisconnectPeer(peerId_,true,"Weapons bullet cheat. itemid %d , bullet %d , attmid %d , Clipsize %d\n",wi.itemID,wi.Var1,wi.Var2,cfg->m_Clipsize);
	}
	}
	}
	}
	}*/


	if(curTime > lastVisUpdateTime_ + 10)
	{
		lastVisUpdateTime_ = r3dGetTime();
		gServerLogic.UpdateNetObjVisData(this);
		//_beginthreadex(NULL, 0, UpdateVisData, (void*)this, 0, NULL);
	}

	if(loadout_->Alive == 0) 
	{
		return TRUE; 
	}

	if(wasDisconnected_)
		return TRUE;

	// disconnect player after few ticks if he had bad items in inventory		
	if(haveBadBackpack_)
	{
		if(++haveBadBackpack_ > 5)
		{
			haveBadBackpack_ = 0;
			gServerLogic.DisconnectPeer(peerId_, false, "haveBadBackpack");
			return TRUE;
		}
	}

	// give x4 time for weapon packet to arrive (make sure it's bigger that r3dNetwork peers disconnect)
	if(!isTargetDummy_ && curTime > (lastWeapDataRep + PKT_C2S_PlayerWeapDataRep_s::REPORT_PERIOD * 4))
	{

		char chatmessage[128] = {0};
		PKT_C2C_ChatMessage_s n;
		sprintf(chatmessage, "Kicked '%s' from server : no weapdatarep (respond timeout)",loadout_->Gamertag);
		r3dscpy(n.gamertag, "System");
		r3dscpy(n.msg, chatmessage);
		n.msgChannel = 1;
		n.userFlag = 2;
		//gServerLogic.p2pBroadcastToAll(NULL, &n, sizeof(n), true);

		gServerLogic.DisconnectPeer(peerId_, true, "no weapdatarep");
		return TRUE;
	}

	// STAT LOGIC
	{
		if(loadout_->Toxic < 100)
		{
			if(loadout_->Toxic > GPP_Data.c_fBloodToxicIncLevel2)
				loadout_->Toxic+= timePassed*GPP_Data.c_fBloodToxicIncLevel2Value;
			else if(loadout_->Toxic > GPP_Data.c_fBloodToxicIncLevel1)
				loadout_->Toxic+= timePassed*GPP_Data.c_fBloodToxicIncLevel1Value;
		}

		if(loadout_->Thirst < 100)
		{
			if(m_PlayerState == PLAYER_MOVE_SPRINT)
				loadout_->Thirst += timePassed*GPP_Data.c_fThirstSprintInc;
			else
				loadout_->Thirst += timePassed*GPP_Data.c_fThirstInc;
			if(loadout_->Toxic > GPP_Data.c_fThirstHighToxicLevel)
				loadout_->Thirst += timePassed*GPP_Data.c_fThirstHighToxicLevelInc;
		}
		if(loadout_->Hunger < 100)
		{
			float HungerSprint = timePassed*GPP_Data.c_fHungerSprintInc; //2,5

			float HungerRun		= timePassed*GPP_Data.c_fHungerRunInc;  // 1.5
			float HungerNorm	= timePassed*GPP_Data.c_fHungerInc;    // 1.5
			float HungerToxici  = timePassed*GPP_Data.c_fHungerHighToxicLevelInc; // 3.0

			if(loadout_->Stats.skillid19 == 1){
				HungerSprint = HungerSprint - 0.000161f;
				HungerRun = HungerRun - 0.000161f;
				HungerNorm = HungerNorm - 0.000161f;
				HungerToxici = HungerToxici - 0.000161f;
				if(loadout_->Stats.skillid26 == 1){
					HungerSprint = HungerSprint - 0.000161f;
					HungerRun = HungerRun + 0.000161f;
					HungerNorm = HungerNorm + 0.000161f;
					HungerToxici = HungerToxici + 0.000161f;
				}
				// Skillsystem 19
			}	
			if(m_PlayerState == PLAYER_MOVE_SPRINT)
				loadout_->Hunger += HungerSprint;
			else if(m_PlayerState == PLAYER_MOVE_RUN)
				loadout_->Hunger += HungerRun;
			else
				loadout_->Hunger += HungerNorm;
			if(loadout_->Toxic > GPP_Data.c_fHungerHighToxicLevel)
				loadout_->Hunger += HungerToxici;
		}

		if(loadout_->Toxic > GPP_Data.c_fBloodToxicLevel3)
			loadout_->Health -= timePassed*GPP_Data.c_fBloodToxicLevel3_HPDamage;
		else if(loadout_->Toxic > GPP_Data.c_fBloodToxicLevel2)
			loadout_->Health -= timePassed*GPP_Data.c_fBloodToxicLevel2_HPDamage;
		else if(loadout_->Toxic > GPP_Data.c_fBloodToxicLevel1)
			loadout_->Health -= timePassed*GPP_Data.c_fBloodToxicLevel1_HPDamage;

		float HealthHungerDmg = timePassed*GPP_Data.c_fHungerLevel_HPDamage;
		float HealthThirstDmg = timePassed*GPP_Data.c_fThirstLevel_HPDamage;

		// Skillsystem 22 & 26
		/*if(loadout_->Stats.skillid22 == 1){
		HealthHungerDmg -= 0.005f;
		HealthThirstDmg -= 0.005f;
		if(loadout_->Stats.skillid26 == 1){
		HealthHungerDmg -= 0.015f;
		HealthThirstDmg -= 0.015f;
		}
		}*/

		if(loadout_->Hunger > GPP_Data.c_fHungerLevel1)
			loadout_->Health -= HealthHungerDmg;
		if(loadout_->Thirst > GPP_Data.c_fThirstLevel1)
			loadout_->Health -= HealthThirstDmg;

		if(loadout_->Health <= 0.0f || loadout_->Health != loadout_->Health)
		{
			gServerLogic.DoKillPlayer(this, this, storecat_INVALID, true);
			return TRUE;
		}
	}

	// physics checking on server-side.
	//controllerPhysObj* controller = (ControllerPhysObj*)PhysicsObject;
	/*if(GetVelocity().LengthSq() > 0.0001f)
	{
	r3dPoint3D pos = GetPosition() + GetVelocity() * r3dGetFrameTime();
	r3dPoint3D v = pos - GetPosition();
	float d = GetVelocity().Dot(v);
	if(d < 0) {
	SetVelocity(r3dPoint3D(0, 0, 0));
	pos = GetPosition();
	}
	else
	{*/
	/*	float heighOffset = 0.8f;
	//if (m_PlayerState ==

	PxBoxGeometry bbox(0.05f, heighOffset, 0.05f);
	PxTransform pose(PxVec3(GetPosition().x, GetPosition().y, GetPosition().z), PxQuat(0,0,0,1));
	PxShape* hit = NULL;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK,0,0,0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);

	if (g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, hit, filter))
	{
	r3dOutToLog("player is bug\n");
	TeleportPlayer(GetPosition());
	}*/
	//}

	/*if(GetVelocity().LengthSq() > 0.0001f)
	{
	if (prevpos.LengthSq() == 0)
	prevpos = GetPosition();

	r3dPoint3D pos = prevpos + GetVelocity() * r3dGetFrameTime();
	r3dPoint3D v = pos - tpos;
	float d = GetVelocity().Dot(v);
	if(d < 0)
	{
	SetVelocity(r3dPoint3D(0,0,0));
	PKT_C2S_SetVelocity_s n;
	n.pos = pos;
	gServerLogic.p2pSendToPeer(peerId_,this,&n,sizeof(n));
	}
	else
	{
	PKT_C2S_SetVelocity_s n;
	n.pos = pos;
	gServerLogic.p2pSendToPeer(peerId_,this,&n,sizeof(n));
	}
	prevpos = pos;
	}*/
	//}*/


	// STAMINA LOGIC SHOULD BE SYNCED WITH CLIENT CODE!
	// (stamina penalty and bOnGround is not synced with server, as it will not cause desync for non cheating client)
	/*{
	const float TimePassed = R3D_MIN(r3dGetFrameTime(), 0.1f);
	if(m_PlayerState == PLAYER_MOVE_SPRINT)
	{
	m_Stamina -= TimePassed;
	if(m_Stamina < -60.0f) // allow one minute of stamina cheating
	{
	gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Stamina, true, "stamina");
	}
	}
	else 
	{
	float regen_rate = loadout_->Health<50?GPP_Data.c_fSprintRegenRateLowHealth:GPP_Data.c_fSprintRegenRateNormal;
	if(loadout_->Stats.skillid3 == 1){
	regen_rate *= 2.0f;
	if(loadout_->Stats.skillid5 == 1){
	regen_rate *= 3.0f;
	}
	}
	m_Stamina += TimePassed*regen_rate; // regeneration rate
	}
	m_Stamina = R3D_CLAMP((float)m_Stamina, 0.0f, GPP_Data.c_fSprintMaxEnergy);
	}*/

	/*if (loadout_->bleeding) // - health
	{
	loadout_->Health -= 0.00050f;
	}*/

	// send vitals if they're changed
	PKT_S2C_SetPlayerVitals_s vitals;
	vitals.FromChar(loadout_);
	vitals.GroupID = loadout_->GroupID;
	vitals.isVisible = loadout_->isVisible;
	vitals.bleeding = loadout_->bleeding;
	vitals.legfall = loadout_->legfall;
	vitals.IsCallForHelp = loadout_->IsCallForHelp;
	vitals.GroupID2 = loadout_->LoadoutID;
	if(((vitals != lastVitals_ || vitals.GroupID != lastVitals_.GroupID || vitals.bleeding != lastVitals_.bleeding || vitals.legfall != lastVitals_.legfall || vitals.IsCallForHelp != lastVitals_.IsCallForHelp || vitals.isVisible != lastVitals_.isVisible) && r3dGetTime() > SendVitals + 4) || lastVitals_.Health != vitals.Health)
	{
		gServerLogic.p2pBroadcastToActive(this, &vitals, sizeof(vitals));
		lastVitals_.FromChar(loadout_);
		SendVitals = r3dGetTime();
	}

	float CHAR_UPDATE_INTERVAL = 60 + u_GetRandom(5,30);
	if(curTime > lastCharUpdateTime_ + CHAR_UPDATE_INTERVAL)
	{
		lastCharUpdateTime_ = curTime;
		gServerLogic.ApiPlayerUpdateChar(this);
	}

	const float WORLD_UPDATE_INTERVAL = 1;
	if(curTime > lastWorldUpdateTime_ + WORLD_UPDATE_INTERVAL)
	{
		lastWorldUpdateTime_ = curTime;
		UpdateGameWorldFlags();
	}

	//if(CheckForFastMove())
	//	return TRUE;

	// anti cheat: player is under the ground, or player is flying above the ground
	{
		//PxRaycastHit hit;
		PxSweepHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
		r3dVector pos = GetPosition();
		PxBoxGeometry boxg(0.5f, 0.1f, 0.5f);
		PxTransform pose(PxVec3(pos.x, pos.y+0.5f, pos.z));
		if(!g_pPhysicsWorld->PhysXScene->sweepSingle(boxg, pose, PxVec3(0,-1,0), 2000.0f, PxSceneQueryFlag::eDISTANCE|PxSceneQueryFlag::eINITIAL_OVERLAP|PxSceneQueryFlag::eINITIAL_OVERLAP_KEEP, hit, filter))
		{
			m_PlayerUndergroundAntiCheatTimer += r3dGetFrameTime();
			/*if(m_PlayerUndergroundAntiCheatTimer > 2.0f)
			{
			char message[128] = {0};
			r3dOutToLog("Detect Player Underground");
			//gServerLogic.DoKillPlayer(this, this, storecat_INVALID, true);
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Flying, false, "player underground - killing", "%.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
			PKT_S2C_CheatMsg_s n2;
			sprintf(message, "Kicked from server : player underground");
			r3dscpy(n2.cheatreason,message);
			//gServerLogic.p2pSendToPeer(peerId_, NULL, &n2, sizeof(n2));
			gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));
			gServerLogic.DisconnectPeer(peerId_, true, "UnderGround");
			}*/
		}
		else
		{

			if(m_PlayerUndergroundAntiCheatTimer > 0)
				m_PlayerUndergroundAntiCheatTimer -= r3dGetFrameTime();

			float dist = hit.distance;
			//r3dOutToLog("@@@@ dist=%.2f\n", dist);
			/*	if(dist > 2.1f) // higher than 1.6 meter above ground
			{
			// check if he is not falling, with some safe margin in case if he is walking down the hill
			if(!profile_.ProfileData.isDevAccount)
			{
			if(!profile_.ProfileData.isPunisher)
			{
			if( (oldstate.Position.y - GetPosition().y) < 0.1f )
			{
			m_PlayerFlyingAntiCheatTimer += r3dGetFrameTime();
			if(m_PlayerFlyingAntiCheatTimer > 5.0f)
			{
			char chatmessage[128] = {0};
			PKT_C2C_ChatMessage_s n;
			sprintf(chatmessage, "Kicked '%s' from server : Player - player flying",loadout_->Gamertag);
			r3dscpy(n.gamertag, "System");
			r3dscpy(n.msg, chatmessage);
			n.msgChannel = 1;
			n.userFlag = 2;
			gServerLogic.p2pBroadcastToAll(NULL, &n, sizeof(n), true);
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Flying, true, "player flying", "dist=%.2f, pos=%.2f, %.2f, %.2f", dist, pos.x, pos.y, pos.z);
			}
			else
			{
			if( (oldstate.Position.y - GetPosition().y) < 0.02f )
			{
			m_PlayerFlyingAntiCheatTimer += r3dGetFrameTime();
			if(m_PlayerFlyingAntiCheatTimer > 5.0f)
			{
			char chatmessage[128] = {0};
			PKT_C2C_ChatMessage_s n;
			sprintf(chatmessage, "Kicked '%s' from server : Player - player flying",loadout_->Gamertag);
			r3dscpy(n.gamertag, "System");
			r3dscpy(n.msg, chatmessage);
			n.msgChannel = 1;
			n.userFlag = 2;
			gServerLogic.p2pBroadcastToAll(NULL, &n, sizeof(n), true);
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Flying, true, "player flying", "dist=%.2f, pos=%.2f, %.2f, %.2f", dist, pos.x, pos.y, pos.z);
			}
			}
			}
			}
			}
			}
			else if(m_PlayerFlyingAntiCheatTimer > 0.0f)
			m_PlayerFlyingAntiCheatTimer-=r3dGetFrameTime(); // slowly decrease timer
			}
			}*/
		}
	}

	// AllrightTH : Raccoon City if pos y > 127 will kill and kick by server

	// 	// afk kick
	// 	const float AFK_KICK_TIME_IN_SEC = 90.0f;
	// 	if(!isTargetDummy_ && curTime > lastPlayerAction_ + AFK_KICK_TIME_IN_SEC)
	// 	{
	// 		if(profile_.ProfileData.isDevAccount)
	// 		{
	// 			// do nothing for admin accs
	// 		}
	// 		else
	// 		{
	// 			PKT_S2C_CheatWarning_s n;
	// 			n.cheatId = PKT_S2C_CheatWarning_s::CHEAT_AFK;
	// 			gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n), true);
	// 
	// 			gServerLogic.DisconnectPeer(peerId_, false, "afk_kick");
	// 			return TRUE;
	// 		}
	// 	}


	return TRUE;
}

void obj_ServerPlayer::RecalcBoundBox()
{
	float	x_size = 0.8f;
	float	z_size = x_size;
	float	y_size = Height;

	r3dPoint3D pos = GetPosition();
	r3dBoundBox bboxlocal;
	bboxlocal.Org.Assign(pos.X - x_size / 2, pos.Y, pos.Z - z_size / 2);
	bboxlocal.Size.Assign(x_size, y_size, z_size);
	SetBBoxLocal(bboxlocal);

	return;
}

BOOL obj_ServerPlayer::OnCollide(GameObject *tobj, CollisionInfo &trace)
{
	return TRUE;
}

void obj_ServerPlayer::UpdateGameWorldFlags()
{
	loadout_->GameFlags = 0;

	//Spawn Protection Code here
	if(loadout_->GameMapId == GBGameInfo::MAPID_WZ_DoomTown || loadout_->GameMapId == GBGameInfo::MAPID_WZ_BeastMap || loadout_->GameMapId == GBGameInfo::MAPID_WZ_Tennessee)
	{
		if((((r3dGetTime() - startPlayTime_) <= 20.0f)))
		{
			loadout_->GameFlags |= wiCharDataFull::GAMEFLAG_isSpawnProtected;
			//loadout_->GameFlags |= wiCharDataFull::GAMEFLAG_NearPostBox;
		}
	}
	else
	{
		if((((r3dGetTime() - startPlayTime_) <= 30.0f)))
		{
			loadout_->GameFlags |= wiCharDataFull::GAMEFLAG_isSpawnProtected;
			//loadout_->GameFlags |= wiCharDataFull::GAMEFLAG_NearPostBox;
		}
	}


	if (loadout_->Health > 100)
	{
		loadout_->Health = 100;
	}
	//ServerWeapon* wpn = m_WeaponArray[wid];

	//wpn->getCategory



	// scan for near postboxes
	for(int i=0; i<gPostBoxesMngr.numPostBoxes_; i++)
	{
		obj_ServerPostBox* pbox = gPostBoxesMngr.postBoxes_[i];
		float dist = (GetPosition() - pbox->GetPosition()).Length();
		if(dist < pbox->useRadius)
		{
			loadout_->GameFlags |= wiCharDataFull::GAMEFLAG_NearPostBox;
			break;
		}
	}

	if(lastWorldFlags_ != loadout_->GameFlags)
	{
		lastWorldFlags_ = loadout_->GameFlags;
	}
	PKT_S2C_SetPlayerWorldFlags_s n;
	n.flags = loadout_->GameFlags;
	gServerLogic.p2pSendToPeer(peerId_,this,&n,sizeof(n));
	RelayPacket(&n, sizeof(n));
	/*if (loadout_->Health  = 100)
	{

	if ((loadout_->GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected) || (loadout_->GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox))
	{
	if(loadout_->Stats.skillid7 == 1)
	loadout_->Health = 200; 
	r3dOutToLog("Set Health to 200 because have skill.\n");
	r3dOutToLog("Return...\n");
	return;

	if(loadout_->Stats.skillid0 == 1)

	loadout_->Health = 150; 
	r3dOutToLog("Set Health to 150 because have skill.\n");

	}

	}
	*/
	return;
}

bool obj_ServerPlayer::IsItemCanAddToInventory(const wiInventoryItem& wi1)
{
	const BaseItemConfig* itemCfg = g_pWeaponArmory->getConfig(wi1.itemID);
	if(!itemCfg) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, false, "BackpackAddItem",
			"%d", wi1.itemID);
		return false;
	}

	int slot_exist = -1;
	int slot_free  = -1;

	extern bool storecat_IsItemStackable(uint32_t ItemID);
	bool isStackable = storecat_IsItemStackable(wi1.itemID);
	for(int i=0; i<loadout_->BackpackSize; i++)
	{
		const wiInventoryItem& wi2 = loadout_->Items[i];

		// can stack only non-modified items
		if(isStackable && wi2.itemID == wi1.itemID && wi1.Var1 < 0 && wi2.Var1 < 0) {
			slot_exist = i;
			break;
		}

		// check if we can place that item to loadout slot
		bool canPlace = storecat_CanPlaceItemToSlot(itemCfg, i);
		if(canPlace && wi2.itemID == 0 && slot_free == -1) {
			slot_free = i;
			break;
		}
	}

	// check weight
	if (slot_exist >= 0 || slot_free >= 0)
	{
		float totalWeight = loadout_->getTotalWeight();
		if(loadout_->Stats.skillid2 == 1){
			totalWeight *= 0.9f;
			if(loadout_->Stats.skillid6 == 1)
				totalWeight *= 0.7f;
		}
		totalWeight += itemCfg->m_Weight*wi1.quantity;

		const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(loadout_->BackpackID);
		if(totalWeight > bc->m_maxWeight)
		{
			PKT_S2C_BackpackModify_s n;
			n.SlotTo = 0xFE;

			gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));

			return false;
		}
	}

	return (slot_exist >= 0 || slot_free >= 0);
}

bool obj_ServerPlayer::BackpackAddItem(const wiInventoryItem& wi1)
{
	// SPECIAL case - GOLD item
	if(wi1.itemID == 'GOLD')
	{
		r3dOutToLog("%s BackpackAddItem %d GameDollars\n", userName, wi1.quantity); CLOG_INDENT;

		// FOR PREMIUM USERS
		int gd = wi1.quantity;
		if (profile_.ProfileData.isPremium && gServerLogic.ginfo_.ispre)
			gd *= 2; // DOUBLE DOLLARS

		profile_.ProfileData.GameDollars += gd;

		// report to client
		PKT_S2C_BackpackAddNew_s n;
		n.SlotTo = 0;
		n.Item   = wi1;
		gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));
		return true;
	}

	//r3dOutToLog("%s BackpackAddItem %dx%d\n", userName, wi1.itemID, wi1.quantity); CLOG_INDENT;
	r3d_assert(wi1.itemID > 0);
	r3d_assert(wi1.quantity > 0);

	const BaseItemConfig* itemCfg = g_pWeaponArmory->getConfig(wi1.itemID);
	if(!itemCfg) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, false, "BackpackAddItem",
			"%d", wi1.itemID);
		return false;
	}

	int slot_exist = -1;
	int slot_free  = -1;

	extern bool storecat_IsItemStackable(uint32_t ItemID);
	bool isStackable = storecat_IsItemStackable(wi1.itemID);
	for(int i=0; i<loadout_->BackpackSize; i++)
	{
		const wiInventoryItem& wi2 = loadout_->Items[i];

		// can stack only non-modified items
		if(isStackable && wi2.itemID == wi1.itemID && wi1.Var1 < 0 && wi2.Var1 < 0) {
			slot_exist = i;
			break;
		}

		// check if we can place that item to loadout slot
		bool canPlace = storecat_CanPlaceItemToSlot(itemCfg, i);
		if(canPlace && wi2.itemID == 0 && slot_free == -1) {
			slot_free = i;
			break;
		}
	}

	if(slot_exist == -1 && slot_free == -1)
	{
		PKT_S2C_BackpackModify_s n;
		n.SlotTo = 0xFF;

		gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));
		return false;
	}

	// check weight
	float totalWeight = loadout_->getTotalWeight();
	if(loadout_->Stats.skillid2 == 1){
		totalWeight *= 0.9f;
		if(loadout_->Stats.skillid6 == 1)
			totalWeight *= 0.7f;
	}
	totalWeight += itemCfg->m_Weight*wi1.quantity;

	const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(loadout_->BackpackID);
	r3d_assert(bc);
	if(totalWeight > bc->m_maxWeight)
	{
		PKT_S2C_BackpackModify_s n;
		n.SlotTo = 0xFE;

		gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));

		return false;
	}


	if(slot_exist >= 0)
	{
		// modify backpack
		r3d_assert(loadout_->Items[slot_exist].itemID == wi1.itemID);
		loadout_->Items[slot_exist].quantity += wi1.quantity;

		// report to client
		PKT_S2C_BackpackModify_s n;
		n.SlotTo     = (BYTE)slot_exist;
		n.Quantity   = (WORD)loadout_->Items[slot_exist].quantity;
		n.dbg_ItemID = loadout_->Items[slot_exist].itemID;
		gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));

		OnBackpackChanged(slot_exist);

		return true;
	}

	if(slot_free >= 0)
	{
		// modify backpack
		r3d_assert(loadout_->Items[slot_free].itemID == 0);
		loadout_->Items[slot_free] = wi1;

		// report to client
		PKT_S2C_BackpackAddNew_s n;
		n.SlotTo = (BYTE)slot_free;
		n.Item   = wi1;
		gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));

		OnBackpackChanged(slot_free);

		return true;
	}

	r3d_assert(false);
	return false;
}

r3dPoint3D obj_ServerPlayer::GetRandomPosForItemDrop()
{
	// create random position around player
	r3dPoint3D pos = GetPosition();
	pos.y += 0.4f;
	pos.x += u_GetRandom(-1, 1);
	pos.z += u_GetRandom(-1, 1);

	return pos;
}

void obj_ServerPlayer::OnPlayerUpdateCharSuccess(wiInventoryItem wi,int slot)
{
	// create network object
	obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", GetRandomPosForItemDrop());
	obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	// vars
	obj->m_Item       = wi;

	wiInventoryItem& wi1 = loadout_->Items[slot];

	if (loadout_->Alive == 0)
	wi1.Reset();
	else
	{
		wi1.quantity--;
		if (wi1.quantity <= 0)
			wi1.Reset();
	}

	// remove from remote inventory
	PKT_S2C_BackpackModify_s n;
	n.SlotTo     = slot;
	n.Quantity   = wi1.quantity;
	n.dbg_ItemID = wi.itemID;
	gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));

	OnBackpackChanged(slot);
	gServerLogic.ApiPlayerUpdateChar(this);
}

void obj_ServerPlayer::BackpackDropItem(int idx)
{
	wiInventoryItem& wi = loadout_->Items[idx];
	r3d_assert(wi.itemID);

	// create network object
	obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", GetRandomPosForItemDrop());
	obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	// vars
	obj->m_Item       = wi;

	// remove from remote inventory
	PKT_S2C_BackpackModify_s n;
	n.SlotTo     = idx;
	n.Quantity   = 0;
	n.dbg_ItemID = wi.itemID;
	gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));

	// remove from local inventory
	wi.Reset();

	CJobUpdateChar* job = new CJobUpdateChar(this);
	job->CharData   = *loadout_;
	job->OldData    = savedLoadout_;
	job->Disconnect = false;
	job->GameDollars = profile_.ProfileData.GameDollars;
	job->peerId = peerId_;
	job->drop = wi;
	job->slot = idx;
	// add character play time to update data
	job->CharData.Stats.TimePlayed += (int)(r3dGetTime() - startPlayTime_);
	g_AsyncApiMgr->AddJob(job);
	// replace character saved loadout. if update will fail, we'll disconnect player and keep everything at sync
	savedLoadout_ = job->CharData;

	//gServerLogic.ApiPlayerUpdateChar(this);
}

void obj_ServerPlayer::OnBackpackChanged(int idx)
{
	// if slot changed is related to loadout - relay to other players
	switch(idx)
	{
	case wiCharDataFull::CHAR_LOADOUT_WEAPON1:
	case wiCharDataFull::CHAR_LOADOUT_WEAPON2:
		// attachments are reset on item change (SERVER CODE SYNC POINT)
		loadout_->Attachment[idx].Reset();
		if(loadout_->Items[idx].Var2 > 0)
			loadout_->Attachment[idx].attachments[WPN_ATTM_CLIP] = loadout_->Items[idx].Var2;

		SetWeaponSlot(idx, loadout_->Items[idx].itemID, loadout_->Attachment[idx]);
		OnLoadoutChanged();
		break;

	case wiCharDataFull::CHAR_LOADOUT_ARMOR:
		SetGearSlot(SLOT_Armor, loadout_->Items[idx].itemID);
		OnLoadoutChanged();
		break;
	case wiCharDataFull::CHAR_LOADOUT_HEADGEAR:
		SetGearSlot(SLOT_Headgear, loadout_->Items[idx].itemID);
		OnLoadoutChanged();
		break;

	case wiCharDataFull::CHAR_LOADOUT_ITEM1:
	case wiCharDataFull::CHAR_LOADOUT_ITEM2:
	case wiCharDataFull::CHAR_LOADOUT_ITEM3:
	case wiCharDataFull::CHAR_LOADOUT_ITEM4:
		OnLoadoutChanged();
		break;
	}
	gServerLogic.ApiPlayerUpdateChar(this);
}

void obj_ServerPlayer::OnLoadoutChanged()
{
	//obj_ServerPlayer* plr;
	char message[128] = {0};
	char chatmessage[128] = {0};
	if (loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 101087 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 101085 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 101084 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 101068 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 101088 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 101217 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 101232 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 101247)
	{
		if (loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].Var1 > 20)
		{
			PKT_C2C_ChatMessage_s n;
			sprintf(chatmessage, "Kicked '%s' from server : Weapon - SniperOverloadMag Weapon1 ,Itemid '%d' ,bullet '%d'",loadout_->Gamertag,loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID,loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].Var1);
			r3dscpy(n.gamertag, "System");
			r3dscpy(n.msg, chatmessage);
			n.msgChannel = 1;
			n.userFlag = 2;
			//gServerLogic.p2pBroadcastToAll(NULL, &n, sizeof(n), true);


			PKT_S2C_CheatMsg_s n2;
			sprintf(message, "Kicked from server : Weapon - SniperOverloadMag Weapon1 ,Itemid '%d' ,bullet '%d'",loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID,loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].Var1);
			r3dscpy(n2.cheatreason,message);
			//gServerLogic.p2pSendToPeer(peerId_, NULL, &n2, sizeof(n2));
			//gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NumShots, false, "Weapon - SniperOverloadMag Weapon1","ItemID '%d',Ammo '%d'",loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID,loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].Var1);
			gServerLogic.DisconnectPeer(peerId_, true, "OverAmmoMag");


		}
	}

	if (loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 101087 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 101085 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 101084 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 101068 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 101088 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 101217 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 101232 ||
		loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 101247)
	{
		if (loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].Var1 > 20)
		{
			PKT_S2C_CheatMsg_s n2;
			sprintf(message, "Kicked from server : Weapon - SniperOverloadMag Weapon2 ,Itemid '%d' ,bullet '%d'",loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID,loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].Var1);
			r3dscpy(n2.cheatreason,message);
			//gServerLogic.p2pSendToPeer(peerId_, NULL, &n2, sizeof(n2));
			//gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));

			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NumShots, false, "Weapon - SniperOverloadMag Weapon2","ItemID '%d',Ammo '%d'",loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID,loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].Var1);
			gServerLogic.DisconnectPeer(peerId_, true, "OverAmmoMag");

			PKT_C2C_ChatMessage_s n1;
			sprintf(chatmessage, "Kicked '%s' from server : Weapon - SniperOverloadMag Weapon2 ,Itemid '%d' ,bullet '%d'",loadout_->Gamertag,loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID,loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].Var1);
			r3dscpy(n1.gamertag, "SysTem");
			r3dscpy(n1.msg, chatmessage);
			n1.msgChannel = 1;
			n1.userFlag = 2;
			//gServerLogic.p2pBroadcastToAll(NULL, &n1, sizeof(n1), true);
		}
	}

	PKT_S2C_SetPlayerLoadout_s n;
	n.WeaponID0  = loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID;
	n.WeaponID1  = loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID;
	n.QuickSlot1 = loadout_->Items[wiCharDataFull::CHAR_LOADOUT_ITEM1].itemID;
	n.QuickSlot2 = loadout_->Items[wiCharDataFull::CHAR_LOADOUT_ITEM2].itemID;
	n.QuickSlot3 = loadout_->Items[wiCharDataFull::CHAR_LOADOUT_ITEM3].itemID;
	n.QuickSlot4 = loadout_->Items[wiCharDataFull::CHAR_LOADOUT_ITEM4].itemID;
	n.ArmorID    = loadout_->Items[wiCharDataFull::CHAR_LOADOUT_ARMOR].itemID;
	n.HeadGearID = loadout_->Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID;
	n.BackpackID = loadout_->BackpackID;

	//TODO: for network traffic optimization (do not send to us) - change to RelayPacket (and add preparePacket there)
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n), true);
}

void obj_ServerPlayer::OnAttachmentChanged(int wid, int atype)
{
	// send packet only if attachments specified in wiNetWeaponAttm was changed
	if(atype != WPN_ATTM_LEFT_RAIL && atype != WPN_ATTM_MUZZLE)
		return;

	PKT_S2C_SetPlayerAttachments_s n;
	n.wid  = (BYTE)wid;
	n.Attm = GetWeaponNetAttachment(wid);

	//TODO: for network traffic optimization (do not send to us) - change to RelayPacket (and add preparePacket there)
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n), true);
}

void obj_ServerPlayer::OnChangeBackpackSuccess(const std::vector<wiInventoryItem>& droppedItems)
{
	// backpack change was successful, drop items to the ground
	for(size_t i=0; i<droppedItems.size(); i++)
	{
		// create network object
		obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", GetRandomPosForItemDrop());
		obj->SetNetworkID(gServerLogic.GetFreeNetId());
		obj->NetworkLocal = true;
		// vars
		obj->m_Item       = droppedItems[i];
	}
}

void obj_ServerPlayer::UseItem_CreateNote(const PKT_C2S_CreateNote_s& n)
{
	if(n.SlotFrom >= loadout_->BackpackSize) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "PKT_C2S_CreateNote_s",
			"slot: %d", n.SlotFrom);
		return;
	}
	wiInventoryItem& wi = loadout_->Items[n.SlotFrom];
	uint32_t usedItemId = wi.itemID;

	if(wi.itemID != WeaponConfig::ITEMID_PieceOfPaper) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "PKT_C2S_CreateNote_s",
			"itemid: %d vs %d", wi.itemID, WeaponConfig::ITEMID_PieceOfPaper);
		return;
	}
	if(wi.quantity <= 0) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_UseItem, true, "PKT_C2S_CreateNote_s",
			"%d", wi.quantity);
		return;
	}

	// remove used item
	wi.quantity--;
	if(wi.quantity <= 0) {
		wi.Reset();
	}

	// create network object
	obj_Note* obj = (obj_Note*)srv_CreateGameObject("obj_Note", "obj_Note", n.pos);
	obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	// vars
	obj->m_Note.NoteID     = 0;
	_time64(&obj->m_Note.CreateDate);
	obj->m_Note.TextFrom   = n.TextFrom;
	obj->m_Note.TextSubj   = n.TextSubj;
	obj->m_Note.ExpireMins = n.ExpMins;
	obj->OnCreate();

	CJobAddNote* job = new CJobAddNote(this);
	job->GameServerId= gServerLogic.ginfo_.gameServerId;
	job->GamePos     = obj->GetPosition();
	job->TextFrom    = obj->m_Note.TextFrom;
	job->TextSubj    = obj->m_Note.TextSubj;
	job->ExpMins     = n.ExpMins;
	job->NoteGameObj = obj->GetSafeID();
	g_AsyncApiMgr->AddJob(job);

	return;
}

void obj_ServerPlayer::TeleportPlayer(const r3dPoint3D& pos)
{
	SetPosition(pos);
	netMover.SrvSetCell(GetPosition());
	loadout_->GamePos = GetPosition();

	moveInited = false;

	gServerLogic.UpdateNetObjVisData(this);
}

bool obj_ServerPlayer::CheckForFastMove()
{
	if(!moveInited)
		return false;

	// check every 5 sec and check against sprint speed with bigger delta
	moveAccumTime += r3dGetFrameTime();
	if(moveAccumTime < 5.0f)
		return false;

	float avgSpeed = moveAccumDist / moveAccumTime;
	bool isCheat   = false;
	bool isInVehicle = false;
	r3dOutToLog("avgSpeed: %f vs %f\n", avgSpeed, GPP_Data.AI_SPRINT_SPEED);
	ObjectManager& GW = GameWorld();
	for (GameObject *targetObj = GW.GetFirstObject(); targetObj; targetObj = GW.GetNextObject(targetObj))
	{
		if (targetObj->isObjType(OBJTYPE_Vehicle))
		{
			obj_Vehicle* car = (obj_Vehicle*)targetObj;
			if (car)
			{
				if (car->owner == this)
				{
					isInVehicle = true;
				}
			}
		}
	}

	if(loadout_->Alive && avgSpeed > GPP_Data.AI_SPRINT_SPEED * 1.5f && !profile_.ProfileData.isDevAccount && !isInVehicle)
	{
		PKT_S2C_CheatMsg_s n2;
		r3dscpy(n2.cheatreason,"Kicked From Server : SpeedHack Found.");
		gServerLogic.p2pSendRawToPeer(peerId_, &n2, sizeof(n2));

		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_FastMove, true, "CheatFastMove", 
			"dist: %f for %f, speed:%f\n", 
			moveAccumDist, 
			moveAccumTime, 
			avgSpeed
			);
		isCheat = true;

		/*char chatmessage[128] = {0};
		PKT_C2C_ChatMessage_s n;
		sprintf(chatmessage, "Antihack detect FastMove on player '%s' : Log Cheat Success",loadout_->Gamertag);
		r3dscpy(n.gamertag, "System");
		r3dscpy(n.msg, chatmessage);
		n.msgChannel = 1;
		n.userFlag = 0;
		gServerLogic.p2pBroadcastToAll(NULL, &n, sizeof(n), true);*/
	}

	// reset accomulated vars
	moveAccumTime = 0;
	moveAccumDist = 0;

	return isCheat;
}

/*void obj_ServerPlayer::OnNetPacket(const PKT_C2S_SetVelocity_s& n)
{
tpos = n.pos;
SetVelocity(n.vel);
}*/
void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PacketBarrier_s& n)
{
	// client switched to next sequence
	clientPacketSequence++;
	r3dOutToLog("peer%02d PKT_C2C_PacketBarrier_s %s %d vs %d\n", peerId_, packetBarrierReason, myPacketSequence, clientPacketSequence);
	packetBarrierReason = "";

	// reset move cheat detection
	moveInited = false;
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_CarKill_s& n)
{
	GameObject* target = GameWorld().GetNetworkObject(n.targetId);
	if (!target) return;

	if (target->isObjType(OBJTYPE_Zombie))
		gServerLogic.ApplyDamageToZombie(this,target,GetPosition()+r3dPoint3D(0,1,0),100, 1, 1, false, storecat_INVALID,true);
	else if (target->isObjType(OBJTYPE_Human))
		gServerLogic.DoKillPlayer(this,this,storecat_INVALID);

}
void obj_ServerPlayer::OnNetPacket(const PKT_C2C_MoveSetCell_s& n)
{
	// if by some fucking unknown method you appeared at 0,0,0 - don't do that!
	if(gServerLogic.ginfo_.mapId != GBGameInfo::MAPID_ServerTest && n.pos.Length() < 10)
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, true, "ZeroTeleport",
			"%f %f %f", 
			n.pos.x, n.pos.y, n.pos.z);
		return;
	}

	if(moveInited)
	{
		// for now we will check ONLY ZX, because somehow players is able to fall down
		r3dPoint3D p1 = netMover.SrvGetCell();
		r3dPoint3D p2 = n.pos;
		p1.y = 0;
		p2.y = 0;
		float dist = (p1 - p2).Length();

		// check for teleport - more that 3 sec of sprint speed. MAKE sure that max dist is more that current netMover.cellSize
		/*if(loadout_->Alive && dist > GPP_Data.AI_SPRINT_SPEED * 3.0f)
		{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_FastMove, true, (dist > 500.0f ? "huge_teleport" : "teleport"),
		"%f, srvGetCell: %.2f, %.2f, %.2f; n.pos: %.2f, %.2f, %.2f", 
		dist, 
		netMover.SrvGetCell().x, netMover.SrvGetCell().y, netMover.SrvGetCell().z, 
		n.pos.x, n.pos.y, n.pos.z
		);
		return;
		}*/
	}

	netMover.SetCell(n);
	SetPosition(n.pos);

	// keep them guaranteed
	RelayPacket(&n, sizeof(n), true);
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2C_MoveRel_s& n)
{
	// decode move
	const CNetCellMover::moveData_s& md = netMover.DecodeMove(n);

	if(moveInited)
	{
		// for now we will check ONLY ZX, because somehow players is able to fall down
		r3dPoint3D p1 = GetPosition();
		r3dPoint3D p2 = md.pos;
		p1.y = 0;
		p2.y = 0;
		float dist = (p1 - p2).Length();
		moveAccumDist += dist;
	}

	// check if we need to reset accomulated speed
	if(!moveInited) 
	{
		moveInited    = true;
		moveAccumTime = 0.0f;
		moveAccumDist = 0.0f;
	}

	// update last action if we moved or rotated
	if((GetPosition() - md.pos).Length() > 0.01f || m_PlayerRotation != md.turnAngle)
	{
		lastPlayerAction_ = r3dGetTime();
	}

	SetPosition(md.pos);
	m_PlayerRotation = md.turnAngle;
	m_PlayerState = md.state&0xF; // PlayerState&0xF
	//r3dOutToLog("m_PlayerState %d\n",m_PlayerState);

	loadout_->GamePos = GetPosition();
	loadout_->GameDir = m_PlayerRotation;

	/*r3dPoint3D vel = netMover.GetVelocityToNetTarget(
	GetPosition(),
	GPP_Data.AI_SPRINT_SPEED * 1.5f,
	1.0f);

	SetVelocity(vel);

	r3dOutToLog("%.2f\n",GetVelocity().LengthSq());*/

	RelayPacket(&n, sizeof(n), true);
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerJump_s& n)
{
	RelayPacket(&n, sizeof(n), true);
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_PlayerEquipAttachment_s& n)
{
	if(n.wid >= 2) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"wid: %d", n.wid);
		return;
	}
	if(m_WeaponArray[n.wid] == NULL) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"nowpn: %d", n.wid);
		return;
	}
	if(n.AttmSlot >= loadout_->BackpackSize) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"slot: %d", n.AttmSlot);
		return;
	}
	if(n.dbg_WeaponID != m_WeaponArray[n.wid]->getConfig()->m_itemID) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"wid: %d %d", n.dbg_WeaponID, m_WeaponArray[n.wid]->getConfig()->m_itemID);
		return;
	}
	if(n.dbg_AttmID != loadout_->Items[n.AttmSlot].itemID) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"aid: %d %d", n.dbg_AttmID, loadout_->Items[n.AttmSlot].itemID);
		return;
	}

	// get attachment config
	wiInventoryItem& wi = loadout_->Items[n.AttmSlot];
	const WeaponAttachmentConfig* attachCfg = g_pWeaponArmory->getAttachmentConfig(wi.itemID);
	if(!attachCfg) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"bad itemid: %d", wi.itemID);
		return;
	}

	ServerWeapon* wpn = m_WeaponArray[n.wid];
	// verify that attachment is legit and can go into this weapon
	if(!wpn->m_pConfig->isAttachmentValid(attachCfg))
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, false, "attachment",
			"not valid attm itemid: %d", wi.itemID);
		return;
	}

	r3dOutToLog("%s: equip attachment %s for %s\n", userName, attachCfg->m_StoreName, wpn->getConfig()->m_StoreName); CLOG_INDENT;

	// set wpn attachment
	wpn->m_Attachments[attachCfg->m_type] = attachCfg;
	wpn->recalcAttachmentsStats();

	loadout_->Attachment[n.wid].attachments[attachCfg->m_type] = attachCfg->m_itemID;

	// report new loadout in case if flashlight/laser was changed
	OnLoadoutChanged();

	// report to other players
	OnAttachmentChanged(n.wid, attachCfg->m_type);
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2C_CarPass_s& n)
{
	PKT_C2C_CarPass_s n1;
	n1.NetID = n.NetID;
	gServerLogic.p2pBroadcastToActive(this, &n1, sizeof(n1));
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_PlayerRemoveAttachment_s& n)
{
	if(n.wid >= 2) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"wid: %d", n.wid);
		return;
	}
	if(m_WeaponArray[n.wid] == NULL) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"nowpn: %d", n.wid);
		return;
	}
	if(n.WpnAttmType >= WPN_ATTM_MAX) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "attachment",
			"WpnAttmType: %d", n.WpnAttmType);
		return;
	}

	ServerWeapon* wpn = m_WeaponArray[n.wid];
	// remove wpn attachment, equip default if have
	wpn->m_Attachments[n.WpnAttmType] = g_pWeaponArmory->getAttachmentConfig(wpn->m_pConfig->FPSDefaultID[n.WpnAttmType]);
	wpn->recalcAttachmentsStats();

	loadout_->Attachment[n.wid].attachments[n.WpnAttmType] = 0;

	// report new loadout in case if flashlight/laser was changed
	OnLoadoutChanged();

	// report to other players
	OnAttachmentChanged(n.wid, n.WpnAttmType);
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerSwitchWeapon_s& n)
{
	if(n.wid == 255) // signal that player has empty hands, just relay it
	{
		m_ForcedEmptyHands = true;
		RelayPacket(&n, sizeof(n));
		return;
	}
	m_ForcedEmptyHands = false;

	if(n.wid >= NUM_WEAPONS_ON_PLAYER) {
		gServerLogic.LogInfo(peerId_, "SwitchWeapon", "wrong weaponslot %d", n.wid);
		return;
	}

	// ptumik: because server creating weapons only for 1 and 2 slots, user can switch to usable items. 
	// so, having NULL here is totally legitimate. 
	// also, because of this, before using m_WeaponArray[m_SelectedWeapon] we need to check that it is not NULL
	// 	if(m_WeaponArray[n.wid] == NULL) {
	// 		gServerLogic.LogInfo(peerId_, "SwitchWeapon", "empty weaponslot %d", n.wid);
	// 		return;
	// 	}

	m_SelectedWeapon = n.wid;

	RelayPacket(&n, sizeof(n),true);
}

void obj_ServerPlayer::UseItem_Barricade(const r3dPoint3D& pos, float rotX, uint32_t itemID)
{
	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(itemID);
	if(!wc)
		return;

	if((GetPosition() - pos).Length() > 5.0f)
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_UseItem, true, "distance",
			"%d", 
			itemID
			);
		return;
	}

	// spawn
	obj_ServerBarricade* shield= (obj_ServerBarricade*)srv_CreateGameObject("obj_ServerBarricade", "barricade", pos);
	shield->m_ItemID = itemID;
	shield->Health = wc->m_AmmoDamage;
	shield->SetRotationVector(r3dPoint3D(rotX,0,0));
	shield->SetNetworkID(gServerLogic.GetFreeNetId());
	shield->NetworkLocal = true;
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2C_CarStatus_s& n)
{
	GameObject* obj = GameWorld().GetNetworkObject(n.CarID);
	if (!obj) return;
	obj_Vehicle* car = (obj_Vehicle*)obj;
	if (!car->owner) // Not have player in vehicle lets driver now
		car->owner = this;
	else
	{
		car->owner = NULL; // already have player in vehicle i will remove player now (exit car)
		car->lastDriveTime = r3dGetTime();
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerCraftItem_s& n)
{
	r3dOutToLog("Plr %s craft slot %d\n",loadout_->Gamertag,n.slotid1);
	r3dOutToLog("Plr %s craft slot %d\n",loadout_->Gamertag,n.slotid2);
	r3dOutToLog("Plr %s craft slot %d\n",loadout_->Gamertag,n.slotid3);
	r3dOutToLog("Plr %s craft slot %d\n",loadout_->Gamertag,n.slotid4);
	r3dOutToLog("Plr %s craft %d\n",loadout_->Gamertag,n.itemid);
	wiInventoryItem& wi1 = loadout_->Items[n.slotid1];
	wiInventoryItem& wi2 = loadout_->Items[n.slotid2];
	if (n.slotid3 != 99999)
	{
		wiInventoryItem& wi3 = loadout_->Items[n.slotid3];
		int q3 = wi3.quantity;
		q3 -= n.slotid3q;
		PKT_S2C_BackpackModify_s n1;
		n1.SlotTo     = n.slotid3;
		n1.Quantity   = q3;
		n1.dbg_ItemID = wi3.itemID;
		gServerLogic.p2pSendToPeer(peerId_, this, &n1, sizeof(n1));
		if (wi3.quantity < 1)
		{
			wi3.Reset();
		}
	}
	if (n.slotid4 != 99999)
	{
		wiInventoryItem& wi4 = loadout_->Items[n.slotid4];
		int q4 = wi4.quantity;
		q4 -= n.slotid4q;
		PKT_S2C_BackpackModify_s n1;
		n1.SlotTo     = n.slotid4;
		n1.Quantity   = q4;
		n1.dbg_ItemID = wi4.itemID;
		gServerLogic.p2pSendToPeer(peerId_, this, &n1, sizeof(n1));
		if (wi4.quantity < 1)
		{
			wi4.Reset();
		}

	}
	// Remove CraftComp item
	/*wi1.quantity--;
	if(wi1.quantity <= 0) {
	wi1.Reset();
	}

	wi2.quantity--;
	if(wi2.quantity <= 0) {
	wi2.Reset();
	}*/

	// Send Remove Packet

	int q1 = wi1.quantity;
	int q2 = wi2.quantity;


	q1 -= n.slotid1q;
	q2 -= n.slotid2q;


	PKT_S2C_BackpackModify_s n1;
	n1.SlotTo     = n.slotid1;
	n1.Quantity   = q1;
	n1.dbg_ItemID = wi1.itemID;
	gServerLogic.p2pSendToPeer(peerId_, this, &n1, sizeof(n1));

	PKT_S2C_BackpackModify_s n2;
	n2.SlotTo     = n.slotid2;
	n2.Quantity   = q2;
	n2.dbg_ItemID = wi2.itemID;
	gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));

	if (wi1.quantity < 1)
	{
		wi1.Reset();
	}
	if (wi2.quantity < 1)
	{
		wi2.Reset();
	}

	//r3dOutToLog("Send Packet");
	// Add Crafted item to player
	int itemids = n.itemid;
	wiInventoryItem wi;
	wi.itemID   = itemids;
	wi.quantity = 1;	
	BackpackAddItem(wi);
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerUseItem_s& n)
{
	//gServerLogic.LogInfo(peerId_, "UseItem", "%d", n.dbg_ItemID); CLOG_INDENT;

	if(n.SlotFrom >= loadout_->BackpackSize) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "useitem",
			"slot: %d", n.SlotFrom);
		return;
	}
	wiInventoryItem& wi = loadout_->Items[n.SlotFrom];
	uint32_t usedItemId = wi.itemID;

	if(wi.itemID != n.dbg_ItemID) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "useitem",
			"itemid: %d vs %d", wi.itemID, n.dbg_ItemID);
		return;
	}

	if(wi.quantity <= 0) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_UseItem, true, "quantity",
			"%d", wi.quantity);
		return;
	}

	// remove used item
	wi.quantity--;
	if(wi.quantity <= 0) {
		wi.Reset();
	}

	RelayPacket(&n, sizeof(n));

	/*if (wi.itemID == WeaponConfig::ITEMID_PersonalLocker)
	{
	obj_SafeLock* obj = (obj_SafeLock*)srv_CreateGameObject("obj_SafeLock", "obj_SafeLock", GetPosition());
	obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	obj->m_Note.pass = 0;
	obj->m_Note.SafeLockID = 0;
	obj->m_Note.in_GamePos = GetPosition();
	// vars
	obj->OnCreate();
	}*/

	const FoodConfig* foodConfig = g_pWeaponArmory->getFoodConfig(usedItemId);
	if(foodConfig)
	{
		loadout_->Health += foodConfig->Health;   loadout_->Health = R3D_CLAMP(loadout_->Health, 0.0f, 100.0f);
		loadout_->Toxic  += foodConfig->Toxicity; loadout_->Toxic  = R3D_CLAMP(loadout_->Toxic,  0.0f, 100.0f);
		loadout_->Hunger -= foodConfig->Food;     loadout_->Hunger = R3D_CLAMP(loadout_->Hunger, 0.0f, 100.0f);
		loadout_->Thirst -= foodConfig->Water;    loadout_->Thirst = R3D_CLAMP(loadout_->Thirst, 0.0f, 100.0f);

		m_Stamina += GPP_Data.c_fSprintMaxEnergy*foodConfig->Stamina;
		return;
	}

	bool useOnOtherPlayer = usedItemId==WeaponConfig::ITEMID_Antibiotics||usedItemId==WeaponConfig::ITEMID_Bandages||usedItemId==WeaponConfig::ITEMID_Bandages2||
		usedItemId==WeaponConfig::ITEMID_Painkillers||usedItemId==WeaponConfig::ITEMID_ZombieRepellent||usedItemId==WeaponConfig::ITEMID_C01Vaccine||
		usedItemId==WeaponConfig::ITEMID_C04Vaccine||usedItemId==WeaponConfig::ITEMID_Medkit;
	if(useOnOtherPlayer && n.var4!=0)
	{
		GameObject* obj = GameWorld().GetNetworkObject(n.var4);
		if(obj && obj->isObjType(OBJTYPE_Human))
		{
			obj_ServerPlayer* otherPlayer = (obj_ServerPlayer*)obj;
			otherPlayer->UseItem_ApplyEffect(n, usedItemId);
		}
		else
		{
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_UseItem, false, "otherplayer", "%d", n.var4);
		}
	}
	else
		UseItem_ApplyEffect(n, usedItemId);

	return;
}

void obj_ServerPlayer::UseItem_ApplyEffect(const PKT_C2C_PlayerUseItem_s& n, uint32_t itemID)
{
	if (itemID == WeaponConfig::ITEMID_Bandages || itemID == WeaponConfig::ITEMID_Bandages2)
	{
		if (loadout_->bleeding)
		{
			loadout_->bleeding = false;
		}
	}
	else if (itemID == WeaponConfig::ITEMID_Painkillers)
	{
		if (loadout_->legfall)
		{
			loadout_->legfall = false;
		}
	}
	switch(itemID)
	{
	case WeaponConfig::ITEMID_Medkit:
	case WeaponConfig::ITEMID_Bandages:
	case WeaponConfig::ITEMID_Bandages2:
	case WeaponConfig::ITEMID_Antibiotics:
	case WeaponConfig::ITEMID_Painkillers:
		{
			const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(itemID);
			if(!wc) {
				r3d_assert(false && "bandages must be a weapon");
				return;
			}

			float bandageEffect = wc->m_AmmoDamage;
			if(loadout_->Stats.skillid20 == 1){
				bandageEffect *= 1.25f;
				if(loadout_->Stats.skillid25 == 1){
					bandageEffect *= 1.50f;
					if(loadout_->Stats.skillid28 == 1)
						bandageEffect *= 1.75f;
				}
			}


			// Alive And Well
			if(loadout_->Stats.skillid7 == 1)
			{
				loadout_->Health += bandageEffect; 
				loadout_->Health = R3D_MIN(loadout_->Health, 200.0f);
				r3dOutToLog("bandage used with skill 7, %f\n", bandageEffect);
				return;
			}
			else if (loadout_->Stats.skillid0 == 1)
			{
				loadout_->Health += bandageEffect; 
				loadout_->Health = R3D_MIN(loadout_->Health, 150.0f);
				r3dOutToLog("bandage used with skill 0, %f\n", bandageEffect);
			}
			else
			{
				loadout_->Health += bandageEffect; 
				loadout_->Health = R3D_MIN(loadout_->Health, 100.0f);
				r3dOutToLog("bandage used, %f\n", bandageEffect);
			}
		}
		break;
	case WeaponConfig::ITEMID_C01Vaccine:
	case WeaponConfig::ITEMID_C04Vaccine:
		{
			const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(itemID);
			if(!wc) {
				r3d_assert(false && "vaccine must be a weapon");
				return;
			}

			float vaccineEffect = wc->m_AmmoDamage;
			loadout_->Toxic -= vaccineEffect; 
			loadout_->Toxic = R3D_CLAMP(loadout_->Toxic, 0.0f, 100.0f);
			r3dOutToLog("vaccine used, %f\n", vaccineEffect);
		}
		break;
	case WeaponConfig::ITEMID_BarbWireBarricade:
	case WeaponConfig::ITEMID_WoodShieldBarricade:
	case WeaponConfig::ITEMID_RiotShieldBarricade:
	case WeaponConfig::ITEMID_SandbagBarricade:
		UseItem_Barricade(n.pos, n.var1, itemID);
		break;
	case WeaponConfig::ITEMID_PersonalLocker:
		{
			obj_SafeLock* obj = (obj_SafeLock*)srv_CreateGameObject("obj_SafeLock", "obj_SafeLock", GetPosition());
			obj->SetNetworkID(gServerLogic.GetFreeNetId());
			obj->NetworkLocal = true;
			obj->m_Note.pass = 0;
			obj->m_Note.SafeLockID = 0;
			obj->m_Note.in_GamePos = GetPosition();
			// vars
			obj->OnCreate();
		}
		break;
	case WeaponConfig::ITEMID_ZombieRepellent:
	case 301321: // gas
		//todo
		break;

	default:
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_UseItem, true, "baditemid",
			"%d", 
			itemID
			);
		break;
	}

	return;
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_ValidateBackpack_s& n)
{
	for (int i=0;i<72;i++)
	{
		const wiInventoryItem wi = n.Items[i];
		// first. check for itemid sync
		if (wi.itemID != loadout_->Items[i].itemID && wi.itemID != 0 && loadout_->Items[i].itemID != 0)
		{
			char message[128] = {0};
			PKT_S2C_CheatMsg_s n2;
			sprintf(message, "KICKED BY SERVER : ValidateBackpack Itemid mismatch! %d %d",wi.itemID,loadout_->Items[i].itemID);
			r3dscpy(n2.cheatreason,message);
			gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));
			//gMasterServerLogic.SendNoticeMsg("ValidateBackpack");
			gServerLogic.LogCheat(peerId_,PKT_S2C_CheatWarning_s::CHEAT_Data,true,"ValidateBackpack","Itemid mismatch slot %d itemid cl:%d sv:%d",i,wi.itemID,loadout_->Items[i].itemID);
		}
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_GroupNoAccept_s& n)
{
	obj_ServerPlayer* tplr = gServerLogic.peers_[n.peerId].player;
	if (tplr)
	{
		PKT_C2S_GroupNoAccept_s n2;
		n2.peerId = peerId_;
		gServerLogic.p2pSendToPeer(tplr->peerId_, tplr, &n2, sizeof(n2));
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_GroupKick_s& n)
{
	obj_ServerPlayer* tplr = gServerLogic.peers_[n.peerId].player;

	// check some fucking player in order group kick this.
	if (!tplr) return;
	if (loadout_->GroupID == 0) return;
	if (loadout_->GroupID != tplr->loadout_->GroupID) return;

	if (tplr->loadout_->GroupID == 0) return;

	if (tplr->isLeaveGroup) return;

	r3dOutToLog("%s kick %s from group\n",userName,tplr->userName);
	// not kicked player at now. this player will kicked in 45 secs by obj_ServerPlayer::Update()
	//tplr->loadout_->GroupID = 0;

	tplr->LeaveGroupTime = r3dGetTime() + 45;
	tplr->isLeaveGroup = true;
	for(int i=0; i<gServerLogic.MAX_PEERS_COUNT; i++)
	{
		obj_ServerPlayer* plr = gServerLogic.peers_[i].player;
		if(!plr)
			continue;
		if(plr->loadout_->GroupID == tplr->loadout_->GroupID && plr->loadout_->GroupID != 0 && tplr->loadout_->GroupID != 0)
		{
			PKT_C2S_GroupKick_s n1;
			n1.peerId = tplr->peerId_;
			gServerLogic.p2pSendToPeer(plr->peerId_, plr, &n1, sizeof(n1));
		}
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_GroupInvite_s& n)
{
	obj_ServerPlayer* tplr = gServerLogic.peers_[n.peerId].player;
	if (tplr)
	{
		PKT_C2S_GroupReInvite_s n1;
		n1.peerId = peerId_;
		gServerLogic.p2pSendToPeer(n.peerId,tplr,&n1,sizeof(n));
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_GroupAccept_s& n)
{
	// not set player to group if player is joined groups
	if (loadout_->GroupID != 0) return;

	obj_ServerPlayer* tplr = gServerLogic.peers_[n.peerId].player;
	if (tplr)
	{
		if (tplr->loadout_->GroupID == 0) // create group
		{
			tplr->loadout_->GroupID = tplr->loadout_->LoadoutID;
		}
		loadout_->GroupID = tplr->loadout_->GroupID;
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerReload_s& n)
{
	if(n.WeaponSlot >= loadout_->BackpackSize || n.AmmoSlot >= loadout_->BackpackSize) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "reload",
			"slot: %d %d", n.WeaponSlot, n.AmmoSlot);
		return;
	}
	if(n.WeaponSlot != wiCharDataFull::CHAR_LOADOUT_WEAPON1 && n.WeaponSlot != wiCharDataFull::CHAR_LOADOUT_WEAPON2) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "reload",
			"wslot: %d", n.WeaponSlot);
		return;
	}

	// validate weapon
	ServerWeapon* wpn = m_WeaponArray[n.WeaponSlot];

	/*char message[128] = {0};
	char chatmessage[128] = {0};
	if (wpn->getPlayerItem().itemID == 101087 ||
	wpn->getPlayerItem().itemID == 101085 ||
	wpn->getPlayerItem().itemID == 101084 ||
	wpn->getPlayerItem().itemID == 101068 ||
	wpn->getPlayerItem().itemID == 101088 ||
	wpn->getPlayerItem().itemID == 101217 ||
	wpn->getPlayerItem().itemID == 101232 ||
	wpn->getPlayerItem().itemID == 101247)
	{
	if (wpn->getPlayerItem().Var1 > 20)
	{
	PKT_S2C_CheatMsg_s n2;
	sprintf(message, "Kicked from server : Weapon - SniperOverloadMag Weapon1 ,Itemid '%d' ,bullet '%d'",wpn->getPlayerItem().itemID,wpn->getPlayerItem().Var1);
	r3dscpy(n2.cheatreason,message);
	//gServerLogic.p2pSendToPeer(peerId_, NULL, &n2, sizeof(n2));
	//	gServerLogic.p2pSendRawToPeer(peerId_, &n2, sizeof(n2));
	gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NumShots, false, "Weapon - SniperOverloadMag Weapon1","ItemID '%d',Ammo '%d'",wpn->getPlayerItem().itemID,wpn->getPlayerItem().Var1);
	gServerLogic.DisconnectPeer(peerId_, true, "OverAmmoMag");

	PKT_C2C_ChatMessage_s n;
	sprintf(chatmessage, "Kicked '%s' from server : Weapon - SniperOverloadMag Weapon1 ,Itemid '%d' ,bullet '%d'",loadout_->Gamertag,wpn->getPlayerItem().itemID,wpn->getPlayerItem().Var1);
	r3dscpy(n.gamertag, "System");
	r3dscpy(n.msg, chatmessage);
	n.msgChannel = 1;
	n.userFlag = 2;
	//gServerLogic.p2pBroadcastToAll(NULL, &n, sizeof(n), true);
	return;

	}
	}

	if (wpn->getPlayerItem().itemID == 101087 ||
	wpn->getPlayerItem().itemID == 101085 ||
	wpn->getPlayerItem().itemID == 101084 ||
	wpn->getPlayerItem().itemID == 101068 ||
	wpn->getPlayerItem().itemID == 101088 ||
	wpn->getPlayerItem().itemID == 101217 ||
	wpn->getPlayerItem().itemID == 101232 ||
	wpn->getPlayerItem().itemID == 101247)
	{
	if (wpn->getPlayerItem().Var1 > 20)
	{
	PKT_S2C_CheatMsg_s n2;
	sprintf(message, "Kicked from server : Weapon - SniperOverloadMag Weapon2 ,Itemid '%d' ,bullet '%d'",wpn->getPlayerItem().itemID,wpn->getPlayerItem().Var1);
	r3dscpy(n2.cheatreason,message);
	//gServerLogic.p2pSendToPeer(peerId_, NULL, &n2, sizeof(n2));
	//	gServerLogic.p2pSendRawToPeer(peerId_, &n2, sizeof(n2));

	gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NumShots, false, "Weapon - SniperOverloadMag Weapon2","ItemID '%d',Ammo '%d'",loadout_->Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID,wpn->getPlayerItem().Var1);
	gServerLogic.DisconnectPeer(peerId_, true, "OverAmmoMag");

	PKT_C2C_ChatMessage_s n1;
	sprintf(chatmessage, "Kicked '%s' from server : Weapon - SniperOverloadMag Weapon2 ,Itemid '%d' ,bullet '%d'",loadout_->Gamertag,wpn->getPlayerItem().itemID,wpn->getPlayerItem().Var1);
	r3dscpy(n1.gamertag, "SysTem");
	r3dscpy(n1.msg, chatmessage);
	n1.msgChannel = 1;
	n1.userFlag = 2;
	//	gServerLogic.p2pBroadcastToAll(NULL, &n1, sizeof(n1), true);
	return;
	}
	}*/

	if(wpn == NULL) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, true, "reload",
			"wempty: %d", n.WeaponSlot);
		return;
	}
	if(wpn->getClipConfig() == NULL) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, true, "reload",
			"noclip: %d", n.WeaponSlot);
		return;
	}

	// validate ammo slot
	wiInventoryItem& wi = loadout_->Items[n.AmmoSlot];
	if(wi.itemID == 0 || wi.quantity == 0) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "reload",
			"aempty: %d %d", wi.itemID, wi.quantity);
		return;
	}

	// validate if we reloaded correct amount
	int ammoReloaded = wi.Var1 < 0 ? wpn->getClipConfig()->m_Clipsize : wi.Var1;
	if(n.dbg_Amount != ammoReloaded) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "reload",
			"amount:%d var1:%d clip:%d", n.dbg_Amount, wi.Var1, wpn->getClipConfig()->m_Clipsize);
		return;
	}

	r3dOutToLog("reloaded using slot %d - %d left\n", n.AmmoSlot, wi.quantity);

	//@TODO: check if we can use that ammo

	// remove single clip
	wi.quantity--;
	if(wi.quantity <= 0)
		wi.Reset();
	// drop current ammo clip (if have clip speficied and have ammo)
	if(wpn->getPlayerItem().Var1 > 0 && wpn->getPlayerItem().Var2 > 0)
	{
		obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", GetRandomPosForItemDrop());
		obj->SetNetworkID(gServerLogic.GetFreeNetId());
		obj->NetworkLocal = true;
		// vars
		obj->m_Item.itemID   = wpn->getPlayerItem().Var2;
		obj->m_Item.quantity = 1;
		obj->m_Item.Var1     = wpn->getPlayerItem().Var1;
	}
	// reload weapon

	wpn->getPlayerItem().Var1 = ammoReloaded;
	wpn->getPlayerItem().Var2 = wpn->getClipConfig()->m_itemID;

	RelayPacket(&n, sizeof(n));
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerFired_s& n)
{
	if(!FireWeapon(n.debug_wid, n.wasAiming != 0, n.execWeaponFire, 0, "PKT_C2C_PlayerFired_s"))
	{
		return;
	}

	if(n.execWeaponFire)
	{
		gServerLogic.InformZombiesAboutSound(this, m_WeaponArray[n.debug_wid]);
	}

	RelayPacket(&n, sizeof(n),true);
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerHitStatic_s& n)
{
	//r3dOutToLog("FireHitCount--: PKT_C2C_PlayerHitStatic_s\n");
	FireHitCount--;
	if(FireHitCount < -10) // -10 - to allow some buffer
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NumShots, true, "bullethack2");
		return;
	}

	RelayPacket(&n, sizeof(n), true);
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerHitStaticPierced_s& n)
{
	// just relay packet. not a real hit, just idintication that we pierced some static geometry, will be followed up by real HIT packet
	RelayPacket(&n, sizeof(n), true);
}


void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerHitNothing_s& n)
{
	//r3dOutToLog("FireHitCount--: PKT_C2C_PlayerHitNothing_s\n");
	FireHitCount--;
	if(FireHitCount < -10) // -10 - to allow some buffer
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NumShots, true, "bullethack2");
		return;
	}

	//RelayPacket(&n, sizeof(n), false);
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerHitDynamic_s& n)
{
	//r3dOutToLog("FireHitCount--: PKT_C2C_PlayerHitDynamic_s\n");
	FireHitCount--;
	if(FireHitCount < -10) // -10 - to allow some buffer
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NumShots, true, "bullethack2");
		return;
	}

	// make sure we're shooting to another player
	GameObject* targetObj = GameWorld().GetNetworkObject(n.targetId);
	if(!targetObj)
	{
		gServerLogic.LogInfo(peerId_, "HitBody0", "not valid targetId");
		return;
	}

	//r3dOutToLog("hit from %s to %s\n", fromObj->Name.c_str(), targetObj->Name.c_str()); CLOG_INDENT;

	if(!gServerLogic.CanDamageThisObject(targetObj))
	{
		gServerLogic.LogInfo(peerId_, "HitBody1", "hit object that is not damageable!");
		return;
	}

	// validate hit_pos is close to the targetObj, if not, that it is a hack
	if(n.damageFromPiercing == 0) // 0 - bullet didn't pierce anything
	{
		const float dist  = (n.hit_pos - targetObj->GetPosition()).Length();
		const float allow = GPP_Data.AI_SPRINT_SPEED*2.0f;
		if(dist > allow) // if more than Xsec of sprint
		{
			// ptumik: disabled cheat report, as we might receive packet for a player that is dead for client, but respawned on server -> distance difference
			//gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_ShootDistance, false, "HitBodyBigDistance",
			//	"hit %s, dist %f vs %f", 
			//	targetObj->Name.c_str(), dist, allow
			//	);
			return;
		}
	}

	// validate that we currently have valid weapon
	if(m_WeaponArray[m_SelectedWeapon] == NULL)
	{
		gServerLogic.LogInfo(peerId_, "HitBody2", "empty weapon");
		return;
	}

	// validate melee range
	if(m_WeaponArray[m_SelectedWeapon]->getCategory()==storecat_MELEE)
	{
		float dist = (GetPosition() - targetObj->GetPosition()).Length();
		if(dist > 5.0f)
		{
			gServerLogic.LogInfo(peerId_, "HitBody0", "knife cheat %f", dist);
			return;
		}
	}

	// validate muzzle position
	{
		if((GetPosition() - n.muzzler_pos).Length() > 5.0f)
		{
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NoGeomtryFiring, true, "muzzle pos cheat");
			return;
		}
	}

	// validate ray cast (should work for ballistic bullets too)
	r3dPoint3D muzzler_pos = /*r3dPoint3D(n.FirePos.x,n.muzzler_pos.y,n.FirePos.z)*/n.muzzler_pos;
	{
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
		r3dVector pos = muzzler_pos;
		r3dVector dir = n.hit_pos - muzzler_pos;
		float dirl = dir.Length(); 
		dir.Normalize();
		if(g_pPhysicsWorld->raycastSingle(PxVec3(pos.x, pos.y, pos.z), PxVec3(dir.x,dir.y,dir.z), dirl, PxSceneQueryFlag::eIMPACT, hit, filter))
		{

			// we shouldn't hit any static geometry, if we did, than probably user is cheating.
			// so let's discard this packet for now
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NoGeomtryFiring, false, 
				"raycast failed", "player pos: %.2f, %.2f, %.2f, muzzler: %.2f, %.2f, %.2f, distance: %.2f, hitpos: %.2f, %.2f, %.2f", 
				GetPosition().x, GetPosition().y ,GetPosition().z, pos.x, pos.y, pos.z, dirl, n.hit_pos.x, n.hit_pos.y, n.hit_pos.z);

			return;
		}

		// and now validate raycast in opposite direction to check for ppl shooting from inside buildings and what not
		pos = n.hit_pos;
		dir = muzzler_pos - n.hit_pos;
		dirl = dir.Length(); 
		dir.Normalize();
		if(g_pPhysicsWorld->raycastSingle(PxVec3(pos.x, pos.y, pos.z), PxVec3(dir.x,dir.y,dir.z), dirl, PxSceneQueryFlag::eIMPACT, hit, filter))
		{

			// we shouldn't hit any static geometry, if we did, than probably user is cheating.
			// so let's discard this packet for now
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_NoGeomtryFiring, false, 
				"reverse raycast failed", "player pos: %.2f, %.2f, %.2f, muzzler: %.2f, %.2f, %.2f, distance: %.2f, hitpos: %.2f, %.2f, %.2f", 
				GetPosition().x, GetPosition().y ,GetPosition().z, pos.x, pos.y, pos.z, dirl, n.hit_pos.x, n.hit_pos.y, n.hit_pos.z);

			return;
		}

	}

	RelayPacket(&n, sizeof(n),true);

	// calc damage
	float damage = CalcWeaponDamage(targetObj->GetPosition());
	if(n.damageFromPiercing > 0)
	{
		float dmod = float(n.damageFromPiercing)/100.0f;
		damage *= dmod;
	}
	if(loadout_->Stats.skillid8 == 1){
		if(m_WeaponArray[m_SelectedWeapon]->getCategory()==storecat_MELEE){
			damage *= 1.25f;
		}
		if(loadout_->Stats.skillid10 == 1){
			if(m_WeaponArray[m_SelectedWeapon]->getCategory()==storecat_MELEE){
				damage *= 1.25f;
			}
		}
	}
	// track ShotsHits

	bool isSpecial = false;

	loadout_->Stats.ShotsHits++;

	if (profile_.ProfileData.isDevAccount || profile_.ProfileData.isPunisher)
	{
		isSpecial = true;
	}

	if(obj_ServerPlayer* targetPlr = IsServerPlayer(targetObj))
	{
		if(gServerLogic.ApplyDamageToPlayer(this, targetPlr, GetPosition()+r3dPoint3D(0,1,0), damage, n.hit_body_bone, n.hit_body_part, false, m_WeaponArray[m_SelectedWeapon]->getCategory()))
		{
			//HACK: track Kill here, because we can't pass weapon ItemID to ApplyDamageToPlayer yet
			int isKill = ((obj_ServerPlayer*)targetObj)->loadout_->Alive == 0 ? 1 : 0;
			gServerLogic.TrackWeaponUsage(m_WeaponArray[m_SelectedWeapon]->getConfig()->m_itemID, 0, 1, isKill);
		}
	}
	else if(targetObj->isObjType(OBJTYPE_Zombie))
	{
		gServerLogic.ApplyDamageToZombie(this, targetObj, GetPosition()+r3dPoint3D(0,1,0), damage, n.hit_body_bone, n.hit_body_part, false, m_WeaponArray[m_SelectedWeapon]->getCategory(),isSpecial);
	}
	else if(targetObj->Class->Name == "obj_Animals")
	{
		obj_Animals* animal = (obj_Animals*)targetObj;
		animal->ApplyDamage(damage,this);
	}
	else
	{
		gServerLogic.TrackWeaponUsage(m_WeaponArray[m_SelectedWeapon]->getConfig()->m_itemID, 0, 1, 0);
		gServerLogic.ApplyDamage(this, targetObj, this->GetPosition()+r3dPoint3D(0,1,0), damage, false, m_WeaponArray[m_SelectedWeapon]->getCategory(),isSpecial);
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerReadyGrenade_s& n)
{
    RelayPacket(&n, sizeof(n));
}


void obj_ServerPlayer::OnNetPacket(const PKT_C2C_PlayerThrewGrenade_s& n)
{
    r3d_assert(loadout_->Alive);


    lastPlayerAction_ = r3dGetTime();


    if(n.debug_wid < 0 || n.debug_wid>= NUM_WEAPONS_ON_PLAYER)
    {
        gServerLogic.LogInfo(peerId_, "wid invalid", "%s %d", "PKT_C2C_PlayerThrewGrenade_s", n.debug_wid);
        return;
    }


    if(m_ForcedEmptyHands)
    {
        gServerLogic.LogInfo(peerId_, "empty hands", "%s %d vs %d", "PKT_C2C_PlayerThrewGrenade_s", n.debug_wid, m_SelectedWeapon);
        return;
    }


    if(n.slotFrom >= loadout_->BackpackSize) {
        gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "ThrewGrenade",
            "slot: %d", n.slotFrom);
        return;
    }
    wiInventoryItem& wi = loadout_->Items[n.slotFrom];
    uint32_t usedItemId = wi.itemID;


    if(wi.quantity <= 0) {
        gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_UseItem, true, "ThrewGrenade quantity",
            "%d", wi.quantity);
        return;
    }


    gServerLogic.TrackWeaponUsage(usedItemId, 1, 0, 0);


    // remove used item
    wi.quantity--;
    if(wi.quantity <= 0) {
        wi.Reset();
    }


    RelayPacket(&n, sizeof(n));
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_PlayerChangeBackpack_s& n)
{
	if(n.SlotFrom >= loadout_->BackpackSize) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"chbp slot: %d", n.SlotFrom);
		return;
	}

	const BackpackConfig* cfg = g_pWeaponArmory->getBackpackConfig(loadout_->Items[n.SlotFrom].itemID);
	if(cfg == NULL) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"chbp item: %d", loadout_->Items[n.SlotFrom].itemID);
		return;
	}
	if(cfg->m_maxSlots != n.BackpackSize) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"chbp slots: %d %d vs %d", loadout_->Items[n.SlotFrom].itemID, cfg->m_maxSlots, n.BackpackSize);
		return;
	}

	gServerLogic.LogInfo(peerId_, "PKT_C2S_PlayerChangeBackpack_s", "%d->%d", loadout_->BackpackSize, cfg->m_maxSlots); CLOG_INDENT;

	// check for same backpack
	if(loadout_->BackpackID == loadout_->Items[n.SlotFrom].itemID) {
		return;
	}

	// replace backpack in used slot with current one (SERVER CODE SYNC POINT)
	loadout_->Items[n.SlotFrom].itemID = loadout_->BackpackID;

	// remove items that won't fit into backpack and build list of dropped items
	std::vector<wiInventoryItem> droppedItems;
	if(cfg->m_maxSlots < loadout_->BackpackSize)
	{
		for(int i=cfg->m_maxSlots; i<loadout_->BackpackSize; i++)
		{
			wiInventoryItem& wi = loadout_->Items[i];
			if(wi.itemID > 0) 
			{
				droppedItems.push_back(wi);

				// remove from remote inventory
				PKT_S2C_BackpackModify_s n;
				n.SlotTo     = i;
				n.Quantity   = 0;
				n.dbg_ItemID = wi.itemID;
				gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));

				// remove from local inventory
				wi.Reset();
			}
		}
	}

	// update backpack, safe to do here as those params will be updated in api job and if it fails, player will be disconnected
	loadout_->BackpackSize = cfg->m_maxSlots;
	loadout_->BackpackID   = cfg->m_itemID;

	// force player inventory update, so items will be deleted
	gServerLogic.ApiPlayerUpdateChar(this);

	// create api job for backpack change
	CJobChangeBackpack* job = new CJobChangeBackpack(this);
	job->BackpackID   = cfg->m_itemID;
	job->BackpackSize = cfg->m_maxSlots;
	job->DroppedItems = droppedItems;
	g_AsyncApiMgr->AddJob(job);

	OnLoadoutChanged();

	return;
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_BackpackDrop_s& n)
{
	if(n.SlotFrom >= loadout_->BackpackSize) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"slot: %d", n.SlotFrom);
		return;
	}

	float dropLength = (GetPosition() - n.pos).Length();
	if(dropLength > 20.0f)
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"dlen: %f", dropLength);
		return;
	}

	wiInventoryItem& wi = loadout_->Items[n.SlotFrom];
	if(wi.itemID == 0 || wi.quantity < 1)
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"id: %d %d %d", n.SlotFrom, wi.itemID, wi.quantity);
		return;
	}

	// create network object
	obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", n.pos);
	obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	// vars
	obj->m_Item          = wi;
	obj->m_Item.quantity = 1;

	// modify backpack (need after item creation)
	wi.quantity--;
	if(wi.quantity <= 0)
		wi.Reset();

	OnBackpackChanged(n.SlotFrom);

	//[Krit] Save Char when drop item from Backpack
	gServerLogic.ApiPlayerUpdateChar(this);

	/*CJobUpdateChar* job = new CJobUpdateChar(this);
	job->CharData   = *loadout_;
	job->OldData    = savedLoadout_;
	job->Disconnect = false;
	job->GameDollars = profile_.ProfileData.GameDollars;
	job->peerId = peerId_;
	job->drop = wi;
	job->slot = n.SlotFrom;
	// add character play time to update data
	job->CharData.Stats.TimePlayed += (int)(r3dGetTime() - startPlayTime_);
	g_AsyncApiMgr->AddJob(job);
	// replace character saved loadout. if update will fail, we'll disconnect player and keep everything at sync
	savedLoadout_ = job->CharData;*/

	return;
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_BackpackSwap_s& n)
{
	if(n.SlotFrom >= loadout_->BackpackSize || n.SlotTo >= loadout_->BackpackSize)	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"move: %d->%d %d", n.SlotFrom, n.SlotTo, loadout_->BackpackSize);
		return;
	}

	// check if we can place item to slot by type
	/*const BaseItemConfig* itmFrom = g_pWeaponArmory->getConfig(loadout_->Items[n.SlotFrom].itemID);
	const BaseItemConfig* itmTo   = g_pWeaponArmory->getConfig(loadout_->Items[n.SlotTo].itemID);
	if(itmFrom && !storecat_CanPlaceItemToSlot(itmFrom, n.SlotTo))
	{
	gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
	"bad swap: %d->%d", itmFrom->m_itemID, n.SlotTo);
	return;
	}
	if(itmTo && !storecat_CanPlaceItemToSlot(itmTo, n.SlotFrom))
	{
	gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
	"bad swap: %d->%d", itmTo->m_itemID, n.SlotFrom);
	return;
	}*/


	R3D_SWAP(loadout_->Items[n.SlotFrom], loadout_->Items[n.SlotTo]);

	OnBackpackChanged(n.SlotFrom);
	OnBackpackChanged(n.SlotTo);
	return;
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_BackpackJoin_s& n)
{
	if(n.SlotFrom >= loadout_->BackpackSize || n.SlotTo >= loadout_->BackpackSize)	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"join: %d->%d %d", n.SlotFrom, n.SlotTo, loadout_->BackpackSize);
		return;
	}

	wiInventoryItem& wi1 = loadout_->Items[n.SlotFrom];
	wiInventoryItem& wi2 = loadout_->Items[n.SlotTo];
	if(wi1.itemID == 0 || wi1.itemID != wi2.itemID) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"join: itm %d %d", wi1.itemID, wi2.itemID);
		return;
	}
	if(wi1.Var1 >= 0 || wi2.Var1 >= 0) {
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, true, "backpack",
			"join: var %d %d", wi1.Var1, wi2.Var1);
		return;
	}

	wi2.quantity += wi1.quantity;
	wi1.Reset();

	OnBackpackChanged(n.SlotFrom);
	OnBackpackChanged(n.SlotTo);
	return;
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_InventoryOp_s& n)
{
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_HackShieldLog_s& n)
{
	//LastHackShieldLog = r3dGetTime();
	//return;
	//if ((((r3dGetTime() - startPlayTime_) <= 5.0f))) return;

	//__int64 time = _time64(&time);
	// show a log.
	//r3dOutToLog("HackShieldLog || plr %d %d name %s CallBack %x , Status %d , time %.2f , tick %.2f , isHighPing %s\n",peerId_,profile_.CustomerID,userName,n.lCode,n.Status,r3dGetTime()-LastHackShieldLog , (GetTickCount() / 1000.0f) - n.time,(isHighPing ? "Yes" : "No"));

	// speed hack check.
	// step 1
	// check for tick in client is incorrect with server. i think it's speedhack , no people can play while ping > 1000ms.
	// NOTE : IF PLAYER IS SPEEDHACK CHEAT. NOT DO ANYTHING AFTER DISCONNECTED.!
	// fuck. tick not sync with server. not used!
	/* if ((((r3dGetTime() - startPlayTime_) >= 30.0f) && ((GetTickCount() / 1000.0f) - n.time) < -2.0f))
	{
	r3dOutToLog("HackShieldLog || !! plr %d %d name %s too diffirent tick. disconnect him!! pName %s \n",peerId_,profile_.CustomerID,userName,n.pName);
	gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_SpeedHack, false, "player send HackShieldlog too diffirent tick.");
	// send this packet to player. player will see this message after disconnected.
	PKT_S2C_CheatMsg_s n2;
	r3dscpy(n2.cheatreason,"HackShield Kicked from server. : diffirent tick time.");
	gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));
	gServerLogic.DisconnectPeer(peerId_, true, "diffirent tick time.");
	return;
	}*/

	// step 2
	// check for client is speedhack. send packet faster than 0.8 sec.
	// !!!! NOTE: YOU NEED TO CHECK PING. IF NOT YOU WILL HAVE PROBLEM WITH LAG CLIENT!!!
	// we must to check ping before.
	// if player is slow to send this packet more than 0.8 sec. we will give one more time for this client. in two times. player will send packet very fast. 
	// this is very BIG problem. we must fixed.

	bool isLagN = false;
	if (!isHighPing && (r3dGetTime() - LastHackShieldLog) >= 5.8f && ((r3dGetTime() - startPlayTime_) >= 30.0f)) // this player is send slower than 0.8 sec. because player is high ping.
	{
		isHighPing = true;
		isLagN = true;
		// aomsin2526 - i should send warning message to player?
		r3dOutToLog("HackShieldLog || !! plr %d %d name %s pName %s , IS LAG!\n",peerId_,profile_.CustomerID,userName,n.pName);
		// send message to player.
		/*char chatmessage[128] = {0};
		PKT_C2C_ChatMessage_s n1;
		sprintf(chatmessage, "WARNING !! YOUR PING IS HIGH.");
		r3dscpy(n1.gamertag, "<SYSTEM>");
		r3dscpy(n1.msg, chatmessage);
		n1.msgChannel = 1;
		n1.userFlag = 2;
		gServerLogic.net_->SendToPeer(&n1,sizeof(n1),peerId_);*/
	}

	// NOTE: NOT CHECK THIS IF PLAYER IS LAG.
	if (!isHighPing)
	{
		if ((((r3dGetTime() - startPlayTime_) >= 30.0f) && (r3dGetTime() - LastHackShieldLog) < 4.2f) && !isHighPing) // this is speedhack. too fast disconnect player at now.
		{
			r3dOutToLog("HackShieldLog || !! plr %d %d name %s too fast log. disconnect him!! pName %s \n",peerId_,profile_.CustomerID,userName,n.pName);
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_SpeedHack, false, "player send HackShieldlog too fast.");
			// send this packet to player. player will see this message after disconnected.
			PKT_S2C_CheatMsg_s n2;
			r3dscpy(n2.cheatreason,"Kicked to prevent a crash");
			gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));
			gServerLogic.DisconnectPeer(peerId_, true, "too fast HS packet.");
			return;
		}
	}

	// clear high ping status.
	if (isHighPing && !isLagN)
	{
		r3dOutToLog("HackShieldLog || plr %d %d name %s pName %s clear isHighPing... \n",peerId_,profile_.CustomerID,userName,n.pName);
		isHighPing = false;
	}

	// for this callback not care it.
	// NOTE: FOR HOOKFUNCTION WE NOT USED. WILL HAVE PROBLEM WITH WARGUARD ANTIHACK.
	if (n.lCode == AHNHS_ENGINE_DETECT_WINDOWED_HACK || n.lCode == AHNHS_ACTAPC_DETECT_CODEMISMATCH || n.lCode == AHNHS_ACTAPC_DETECT_HOOKFUNCTION || n.lCode == AHNHS_ACTAPC_DETECT_ALREADYHOOKED)
	{
		LastHackShieldLog = r3dGetTime();
		return;
	}

	// step 3 check callback value.
	if (n.lCode != 0)
	{
		if (n.lCode != 0x010001) // hackshield running.
		{
			r3dOutToLog("HackShieldLog || !! plr %d %d name %s diffirent callback!. disconnect him!! pName %s \n",peerId_,profile_.CustomerID,userName,n.pName);

			char msg[128] = {0};
			switch(n.lCode)
			{
				//Engine Callback
			case AHNHS_ENGINE_DETECT_GAME_HACK:
				{
					sprintf(msg,"Game Hack Found. - \n %s",n.pName);
					break;
				}
			case AHNHS_ACTAPC_DETECT_CODEMISMATCH:
				{
					sprintf(msg,"HackShield Service not running correctly.");
					break;
				}
			case AHNHS_ENGINE_DETECT_WINDOWED_HACK:
				{
					//sprintf(msg,"Windowed Hack Found.");
					break;
				}
			case AHNHS_ACTAPC_DETECT_PROTECTSCREENFAILED:
				{
					sprintf(msg,"Protect Screen Failed.");
					break;
				}
			case AHNHS_ACTAPC_DETECT_SPEEDHACK:
			case AHNHS_ACTAPC_DETECT_SPEEDHACK_RATIO:
				{
					sprintf(msg,"Speed Hack Found.");
					break;
				}
				case AHNHS_ACTAPC_DETECT_AUTOMACRO:
				{
				sprintf(msg,"Automacro Found.");
				break;
				}

			case AHNHS_ACTAPC_DETECT_ABNORMAL_FUNCTION_CALL:
				{
					sprintf(msg,"Undefined Function Found.");
					break;
				}
			case AHNHS_ACTAPC_DETECT_ABNORMAL_MEMORY_ACCESS:
				{
					sprintf(msg,"ABNORMAL Memory Access.");
					break;
				}


			case AHNHS_ACTAPC_DETECT_AUTOMOUSE:
				{
					sprintf(msg,"AutoMouse Found.");
					break;
				}
			case AHNHS_CHKOPT_DETECT_VIRTUAL_MACHINE:
				{
					sprintf(msg,"Vitiual Machine Found.");
					break;
				}
			case AHNHS_ACTAPC_DETECT_ALREADYHOOKED:
			case AHNHS_ACTAPC_DETECT_HOOKFUNCTION:
				{
					sprintf(msg,"Direct3DX Hack Found.");
					break;
				}
			case AHNHS_ACTAPC_DETECT_KDTRACE:
			case AHNHS_ACTAPC_DETECT_KDTRACE_CHANGED:
				{
					sprintf(msg,"Hack Found.");
					break;
				}
			default:
				{
					sprintf(msg,"Undefined. %x",n.lCode);
					r3dOutToLog("HackShieldLog || plr %d %s : %s\n",peerId_,userName,msg);
					LastHackShieldLog = r3dGetTime();
					return;
					break;
				}
			}

			r3dOutToLog("HackShieldLog || plr %d %s : %s\n",peerId_,userName,msg);
			/*char message[128] = {0};
			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Network, false, msg);
			PKT_S2C_CheatMsg_s n2;
			sprintf(message, "HackShield Kicked from server. : %s",msg);
			r3dscpy(n2.cheatreason,message);
			gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));*/
			gServerLogic.DisconnectPeer(peerId_, true, msg);
		}
	}
	LastHackShieldLog = r3dGetTime();
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_SendHelpCall_s& n)
{
	//RelayPacket(&n, sizeof(n), true);
	PKT_S2C_SendHelpCallData_s n1;
	sprintf(n1.DistressText,n.DistressText);
	n1.peerid = peerId_;
	n1.pName = n.pName;
	n1.pos = n.pos;
	sprintf(n1.rewardText,n.rewardText);
	//gServerLogic.p2pBroadcastToActive(this, &n1, sizeof(n1));
	//gServerLogic.p2pSendToPeer(peerId_, this, &n1, sizeof(n1));
	gServerLogic.p2pBroadcastToAll(NULL, &n1, sizeof(n1), true);
	//r3dOutToLog("SendToHelp '%s' '%s '%s'\n",n.pName,n.DistressText,n.rewardText);
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_UnloadClipReq_s& n)
{
	if(loadout_->Alive == 0)
		return;

	wiInventoryItem item = loadout_->Items[n.slotID];

	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(item.itemID);
	//m_WeaponArray[n.slotID]
	if(wc)
	{
		if(wc->category >= storecat_ASR && wc->category <= storecat_SMG)
		{
			if(item.Var1 > 0 && item.Var2 > 0)
			{
				wiInventoryItem wi;
				wi.itemID   = item.Var2;
				wi.quantity = 1;
				wi.Var1 = item.Var1;

				item.Var1 = 0;
				item.Var2 = wi.itemID;

				if(BackpackAddItem(wi))
					loadout_->Items[n.slotID] = item;

				OnBackpackChanged(n.slotID);

			}else{
				gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, false, "Clip is serverside empty",
					"slot%d %d - %d", n.slotID, item.itemID, item.Var1);
			}
		}
	}
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_DisconnectReq_s& n)
{
	//loadout_->isDisconnect = true;
	r3dOutToLog("PKT_C2S_DisconnectReq for %s\n", userName);
	if(loadout_->Alive == 0)
	{
		//PKT_C2S_DisconnectReq_s n2;
		//gServerLogic.p2pSendToPeer(peerId_, this, &n2, sizeof(n2));

		gServerLogic.DisconnectPeer(peerId_, false, "disconnect request while dead, we already updated profile");
		return;
	}

	// start update thread, player will disconnect itself when thread is finished
	if(!wasDisconnected_)
	{
		gServerLogic.ApiPlayerUpdateChar(this, true);
		wasDisconnected_ = true;
	}
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_FallingDamage_s& n)
{
	r3dOutToLog("Falling damage to %s, damage=%.2f\n", Name.c_str(), n.damage); CLOG_INDENT;
	//r3dOutToLog("Falling damage Disable\n", Name.c_str(), n.damage); CLOG_INDENT; // No Damage Enabled
	if (!profile_.ProfileData.isGod)
	{
		gServerLogic.ApplyDamage(this, this, GetPosition(), n.damage, true, storecat_INVALID,false);
	}
	/*if ( n.damage > 20.0f)
	{
	loadout_->legfall = true;
	}*/
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_ValidateEnvironment_s& n)
{
	r3dOutToLog("%s lastCurTime %.2f , CurTime %.2f\n",userName,lastCurTime,n.CurTime);
	if (fabs(n.CurTime - lastCurTime) < 0.01f && !firstTime) // 
	{
		char msg[512];
		sprintf(msg,"lastCurTime %.2f , CurTime %.2f\n",lastCurTime,n.CurTime);
		gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
		g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
		gServerLogic.DisconnectPeer(peerId_,true,msg);
		return;
	}
	if (firstTime)
	{
		firstTime = false;
		lastCurTime = n.CurTime;
	}

	lastCurTime = n.CurTime;
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_BulletValidateConfig_s& n)
{
	// not need to check weapons. this is for bullet , not weapon.

	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig((uint32_t)n.m_itemId);
	bool heHaveWep = false; // the weapons need to sync with servers.
	for (int i = 0; i<2;i++)
	{
		ServerWeapon* wpn = m_WeaponArray[i];
		if (!wpn)
			continue;

		if (wpn->m_pConfig->m_itemID == (uint32_t)n.m_itemId && wc == wpn->m_pConfig) // yeah he have this weapon
		{
			heHaveWep = true;
			break;
		}
	}
	// not use this , this is force disconnect
	// use this but not disconnect , use LogCheat.
	if (!wc && n.m_itemId != 0 || !heHaveWep) // itemDB.xml or weapon not sync with servers. disconnect him!
	{
		// i think he have a problem. not all % he cheats.
		// gServerLogic.DisconnectPeer(peerId_,true,"BulletValidate: %s weapon not sync with servers. itemid %d",userName,n.m_itemId);
		gServerLogic.LogCheat(peerId_,PKT_S2C_CheatWarning_s::CHEAT_BadWeapData,false,"BulletValidate","%s weapon not sync with servers. itemid %d",userName,n.m_itemId);
		return;
	}

	// used for debug.
	//r3dOutToLog("PKT_C2S_BulletValidateConfig_s from %s , speed %.2f , mass %.2f , svspeed %.2f , svmass %.2f\n",userName,n.m_AmmoSpeed,n.m_AmmoMass,(float)wc->m_AmmoSpeed,(float)wc->m_AmmoMass);

	// fap fap fap function
	char msg[512];
	if (fabs((float)wc->m_AmmoMass - n.m_AmmoMass) > 0.01f) // no bullet drop hack.
	{
		sprintf(msg,"m_AmmoMass not match itemid:%d sv:%.2f , cl:%.2f\n",n.m_itemId,(float)wc->m_AmmoMass,n.m_AmmoMass);
		g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
		r3dOutToLog(msg);
		gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
		gServerLogic.DisconnectPeer(peerId_,true,msg);
		return;
	}
	else if (fabs((float)wc->m_AmmoSpeed - n.m_AmmoSpeed) > 0.01f) // instant hit hack.
	{
		sprintf(msg,"m_AmmoSpeed not match itemid:%d sv:%.2f , cl:%.2f\n",n.m_itemId,(float)wc->m_AmmoSpeed,n.m_AmmoSpeed);
		g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
		r3dOutToLog(msg);
		gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
		gServerLogic.DisconnectPeer(peerId_,true,msg);
		return;
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_PlayerState_s& n)
{
	// NOTE: STATE NEED TO SYNC WITH SERVER!
	char msg[512];
	if (!profile_.ProfileData.isDevAccount)
	{
		if (n.state == PLAYER_MOVE_SPRINT)
		{
			// for this check only forward speed. cannot move backward.
			if (n.accel.z > 5.46f) 
			{
				sprintf(msg,"SpeedHack PLAYER_MOVE_SPRINT cl:%.2f\n",n.accel.z);
				g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
				r3dOutToLog(msg);
				gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
				gServerLogic.DisconnectPeer(peerId_,true,msg);
				return;
			}
		}
		else if (n.state == PLAYER_MOVE_RUN)
		{
			if (n.accel.z > 3.64f || n.accel.z < -2.73f)
			{
				sprintf(msg,"SpeedHack PLAYER_MOVE_RUN cl:%.2f\n",n.accel.z);
				g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
				r3dOutToLog(msg);
				gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
				gServerLogic.DisconnectPeer(peerId_,true,msg);
				return;
			}
		}
		else if (n.state == PLAYER_MOVE_WALK_AIM)
		{
			if (n.accel.z > 2.08f || n.accel.z < -1.56f)
			{
				sprintf(msg,"SpeedHack PLAYER_MOVE_WALK_AIM cl:%.2f\n",n.accel.z);
				g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
				r3dOutToLog(msg);
				gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
				gServerLogic.DisconnectPeer(peerId_,true,msg);
				return;
			}
		}
		else if (n.state == PLAYER_MOVE_CROUCH_AIM)
		{
			if (n.accel.z > 0.83f || n.accel.z < -0.65f)
			{
				sprintf(msg,"SpeedHack PLAYER_MOVE_CROUCH_AIM cl:%.2f\n",n.accel.z);
				g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
				r3dOutToLog(msg);
				gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
				gServerLogic.DisconnectPeer(peerId_,true,msg);
				return;
			}
		}
		else if (n.state == PLAYER_MOVE_PRONE)
		{
			if (n.accel.z > 0.73f || n.accel.z < -0.55f)
			{
				sprintf(msg,"SpeedHack PLAYER_MOVE_PRONE cl:%.2f\n",n.accel.z);
				g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
				r3dOutToLog(msg);
				gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
				gServerLogic.DisconnectPeer(peerId_,true,msg);
				return;
			}
		}
		else if (n.state == PLAYER_PRONE_AIM)
		{
			if (n.accel.z != 0.0f) // player cant move in this state.
			{
				sprintf(msg,"SpeedHack PLAYER_MOVE_PRONE_AIM cl:%.2f\n",n.accel.z);
				g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
				r3dOutToLog(msg);
				gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
				gServerLogic.DisconnectPeer(peerId_,true,msg);
				return;
			}
		}
		/*else if (n.state == PLAYER_MOVE_CROUCH)
		{
		if (n.accel.z > 1.46f || n.accel.z < -1.10f)
		{
		sprintf(msg,"SpeedHack PLAYER_MOVE_CROUCH cl:%.2f\n",n.accel.z);
		g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
		r3dOutToLog(msg);
		gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
		gServerLogic.DisconnectPeer(peerId_,true,msg);
		return;
		}
		}*/
	}
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_BuyItemReq_s& n)
{
	for (uint32_t i=0;i<g_NumStoreItems;i++)
	{
		const wiStoreItem& itm = g_StoreItems[i];
		if (itm.itemID == n.ItemID && n.ItemID != 0)
		{
			int quantity = 1;
			{
				const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(g_StoreItems[i].itemID);
				if(wc)
					quantity = wc->m_ShopStackSize;
				const FoodConfig* fc = g_pWeaponArmory->getFoodConfig(g_StoreItems[i].itemID);
				if(fc)
					quantity = fc->m_ShopStackSize;
			}

			wiInventoryItem wi;
			wi.itemID = n.ItemID;
			wi.quantity = quantity;
			if (IsItemCanAddToInventory(wi))
			{
				g_AsyncApiMgr->AddJob(new CJobSrvBuyItem(this,wi,itm));
				/*PKT_C2S_BuyItemAns_s n1;
				n1.ansCode = 0;
				gServerLogic.p2pSendRawToPeer(peerId_,&n1,sizeof(n1));*/
				return;
			}
			else
				break;
		}
	}

	PKT_C2S_BuyItemAns_s n1;
	n1.ansCode = 1;
	gServerLogic.p2pSendRawToPeer(peerId_,&n1,sizeof(n1));
}

void obj_ServerPlayer::OnNetPacket(const PKT_C2S_TradeAccept_s& n)
{
	GameObject* targetObj = GameWorld().GetNetworkObject((DWORD)n.netId);

	for (int i=0;i<72;i++)
	{
		this->TradeItems[i].Reset();
		this->isTradeAccept = false;
		this->TradeSlot[i] = -1;
	}

	Tradetargetid = (DWORD)n.netId;

	TradeNums = 0;

	if (targetObj && IsServerPlayer(targetObj))
	{
		obj_ServerPlayer * plr = (obj_ServerPlayer*)targetObj;

		plr->TradeNums = 0;
		plr->Tradetargetid = GetNetworkID();

		for (int i=0;i<72;i++)
		{
			plr->TradeItems[i].Reset();
			plr->isTradeAccept = false;
			plr->TradeSlot[i] = -1;
		}
		PKT_C2S_TradeAccept_s n1;
		n1.netId = this->GetNetworkID();
		gServerLogic.p2pSendToPeer(plr->peerId_,plr,&n1,sizeof(n));
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_WpnLog_s& n)
{
	return;
	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig((uint32_t)n.itemid);
	LastWpnLog = r3dGetTime();

	ServerWeapon* wpn = m_WeaponArray[n.slot];
	if (!wc && n.itemid != 0) // not match.
	{
		gServerLogic.DisconnectPeer(peerId_,true,"WpnLog not match. itemid:%d",n.itemid);
		return;
	}

	// NOTE : THIS IS VERY IMPORTANT CODE. IF FAILED THIS PLAYER WILL GET BANNED BY ERROR SYSTEM.
	float spread = (float)wc->m_spread;	spread = spread * (1.0f+n.m_spreadattm);
	spread *= 0.5f;

	// SKILL SYNC CODE
	if (loadout_->Stats.skillid9)
		spread *= 0.8f;


	float recoil = (float)wc->m_recoil;
	recoil = recoil * (1.0f+n.m_recoilattm);

	// SKILL SYNC CODE
	if (loadout_->Stats.skillid11)
		recoil *= 0.8f;

	recoil *= 0.05f;

	// copy from client.
	float fireDelay = (float)wc->m_fireDelay;
	int firerate = (int)ceilf(60.0f / fireDelay); // convert back to rate of fire
	firerate = (int)ceilf(float(firerate) * (1.0f+n.m_rateattm)); // add bonus , but i will use value by client. it's not sync with servers.
	fireDelay = 60.0f / firerate; // convert back to fire delay
	float fireRatePerc = 0.0f;
	float adjInPerc = (fireDelay / 100.0f) * fireRatePerc;
	fireDelay -= adjInPerc;



	//r3dOutToLog("PKT_C2S_WpnLog_s recieved itemid %d spread %.2f svspread %.2f\n",n.itemid,n.m_spread,spread);

	// CJobBanID(CustomerID , Reason , ...)
	// MasterServerLogic::SendNoticeMsg(msg , ...)
	char msg[512];
	if (fabs((float)spread - (float)n.m_spread) > 0.01f) // spread hack
	{
		sprintf(msg,"m_spread not match itemid:%d sv:%.2f , cl:%.2f\n",n.itemid,spread,n.m_spread);
		g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
		r3dOutToLog(msg);
		gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
		gServerLogic.DisconnectPeer(peerId_,true,"m_spread not match itemid:%d sv:%.2f , cl:%.2f",n.itemid,wc->m_spread,n.m_spread);
		return;
	}
	else if (fabs((float)recoil - (float)n.m_recoil) > 0.01f) // recoil hack.
	{
		sprintf(msg,"m_recoil not match itemid:%d sv:%.2f , cl:%.2f\n",n.itemid,recoil,n.m_recoil);
		r3dOutToLog(msg);
		g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
		gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
		gServerLogic.DisconnectPeer(peerId_,true,"m_recoil not match itemid:%d sv:%.2f , cl:%.2f",n.itemid,wc->m_recoil,n.m_recoil);
		return;
	}
	else if (fabs((float)fireDelay - (float)n.m_rateoffire) > 0.01f) // rapid fire hack.
	{
		sprintf(msg,"m_rateoffire not match itemid:%d sv:%.2f , cl:%.2f\n",n.itemid,fireDelay,n.m_rateoffire);
		r3dOutToLog(msg);
		g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
		gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
		gServerLogic.DisconnectPeer(peerId_,true,"m_rateoffire not match itemid:%d sv:%.2f , cl:%.2f",n.itemid,fireDelay,n.m_rateoffire);
		return;
	}

	// STEP 2 - TPROGAME NO RECOIL LOGIC
	// USED m_recoil2
	recoil *= 1.1f; // recoil = getRecoil()*1.1f;
	if (fabs((float)recoil - (float)n.m_recoil2) > 0.01f) // RECOIL HACK
	{
		sprintf(msg,"m_recoil2 not match itemid:%d sv:%.2f , cl:%.2f\n",n.itemid,recoil,n.m_recoil2);
		r3dOutToLog(msg);
		g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,msg));
		gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
		gServerLogic.DisconnectPeer(peerId_,true,msg);
		return;
	}

}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_TradeCancel_s& n)
{
	GameObject* targetObj = GameWorld().GetNetworkObject((DWORD)n.netId);
	if (targetObj)
	{
		if (!IsServerPlayer(targetObj))
		{
			gServerLogic.LogCheat(peerId_,99,true,"Trade","not player!");
			return;
		}
		obj_ServerPlayer* tplr = (obj_ServerPlayer*)targetObj;
		for (int i=0;i<72;i++)
		{
			tplr->TradeItems[i].Reset();
			tplr->isTradeAccept = false;
			tplr->TradeSlot[i] = -1;
		}
		PKT_C2S_TradeCancel_s n1;
		gServerLogic.p2pSendToPeer(tplr->peerId_,tplr,&n1,sizeof(n1));
	}
	for (int i=0;i<72;i++)
	{
		this->TradeItems[i].Reset();
		this->isTradeAccept = false;
		this->TradeSlot[i] = -1;
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_TradeOptoBack_s& n)
{
	if (isTradeAccept) 
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, true, "isTradeAccept");
		return;
	}

	GameObject* targetObj = GameWorld().GetNetworkObject((DWORD)n.netId);
	if (targetObj)
	{
		if (!IsServerPlayer(targetObj))
		{
			gServerLogic.LogCheat(peerId_,99,true,"Trade","not player!");
			return;
		}

		r3dOutToLog("Trade: OptoBack %s ItemID %d\n",userName,n.Item.itemID);
		//TradeItems[TradeNums].Reset();
		//TradeSlot[TradeNums] = -1;

		//if (TradeNums > 0)
		// TradeNums--;

		GameObject* obj = GameWorld().GetNetworkObject(Tradetargetid);
		if (IsServerPlayer(obj))
		{
			obj_ServerPlayer* plr = (obj_ServerPlayer*)obj;
			if (plr && plr->isTradeAccept)
			{
				plr->isTradeAccept = false;
				for (int i=0;i<72;i++)
				{
					plr->TradeItems[i].Reset();
					plr->TradeSlot[i] = -1;
				}
			}
		}

		isTradeAccept = false;
		for (int i=0;i<72;i++)
		{
			TradeItems[i].Reset();
			TradeSlot[i] = -1;
		}

		obj_ServerPlayer* tplr = (obj_ServerPlayer*)targetObj;
		PKT_C2S_TradeOptoBack_s n1;
		n1.Item = n.Item;
		n1.slotfrom = n.slotfrom;
		gServerLogic.p2pSendToPeer(tplr->peerId_,tplr,&n1,sizeof(n1));
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_TradeBacktoOp_s& n)
{
	if (isTradeAccept) 
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, true, "isTradeAccept");
		return;
	}

	GameObject* targetObj = GameWorld().GetNetworkObject((DWORD)n.netId);
	if (targetObj)
	{
		if (!IsServerPlayer(targetObj))
		{
			gServerLogic.LogCheat(peerId_,99,true,"Trade","not player!");
			return;
		}

		r3dOutToLog("Trade: BackToOp %s ItemID %d\n",userName,n.Item.itemID);

		//TradeItems[TradeNums].Reset();
		//TradeItems[TradeNums] = n.Item;
		//TradeSlot[TradeNums] = n.slotto;
		//TradeNums++;

		GameObject* obj = GameWorld().GetNetworkObject(Tradetargetid);
		if (IsServerPlayer(obj))
		{
			obj_ServerPlayer* plr = (obj_ServerPlayer*)obj;
			if (plr && plr->isTradeAccept)
			{
				plr->isTradeAccept = false;
				for (int i=0;i<72;i++)
				{
					plr->TradeItems[i].Reset();
					plr->TradeSlot[i] = -1;
				}
			}
		}

		isTradeAccept = false;
		for (int i=0;i<72;i++)
		{
			TradeItems[i].Reset();
			TradeSlot[i] = -1;
		}

		obj_ServerPlayer* tplr = (obj_ServerPlayer*)targetObj;
		PKT_C2S_TradeBacktoOp_s n1;
		n1.Item = n.Item;
		n1.slotto = n.slotto;
		gServerLogic.p2pSendToPeer(tplr->peerId_,tplr,&n1,sizeof(n1));
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_TradeRequest_s& n)
{
	GameObject* targetObj = GameWorld().GetNetworkObject((DWORD)n.netId);
	if (targetObj)
	{
		if (!IsServerPlayer(targetObj))
		{
			gServerLogic.LogCheat(peerId_,99,true,"Trade","not player!");
			return;
		}
		obj_ServerPlayer* tplr = (obj_ServerPlayer*)targetObj;
		PKT_C2S_TradeRequest_s n1;
		n1.netId = (int)this->GetNetworkID();
		gServerLogic.p2pSendToPeer(tplr->peerId_,tplr,&n1,sizeof(n1));
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_TradeAccept2_s& n)
{
	// stored data

	if (isTradeAccept) 
	{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Data, true, "isTradeAccept");
		return;
	}

	for (int i=0;i<72; i++)
	{
		TradeItems[i] = n.Item[i];
		TradeSlot[i] = n.slot[i];
	}

	this->Tradetargetid = (DWORD)n.netId;
	this->isTradeAccept = true;
	// the trading will start in Update() now we will wait a other player

	GameObject* targetObj = GameWorld().GetNetworkObject((DWORD)n.netId);
	if (targetObj && IsServerPlayer(targetObj))
	{
		obj_ServerPlayer * plr = (obj_ServerPlayer*)targetObj;
		PKT_C2S_TradeAccept2_s n1;
		gServerLogic.p2pSendToPeer(plr->peerId_,plr,&n1,sizeof(n1));
	}
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_StackClips_s& n)
{
	wiInventoryItem wi = loadout_->Items[n.slotId];
	const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(wi.itemID);
	if (wi.itemID == 0 || !wac || wac->category != storecat_FPSAttachment) 
		return;

	// save old loadout. use if additem failed.
	wiInventoryItem OldItems[72];
	for (int i=0;i<72;i++)
	{
		OldItems[i].Reset();

		OldItems[i] = loadout_->Items[i];
	}

	// find the same clips itemid. and not full clip.
	int slotid[72];

	for (int i=0;i<72;i++)
	{
		slotid[i] = -1;
	}

	for (int i=0;i<loadout_->BackpackSize;i++)
	{
		wiInventoryItem wi2 = loadout_->Items[i];
		if (wi2.itemID != 0 && wi2.itemID == wi.itemID /*&& wi2.Var1 != wac->m_Clipsize*/)
			slotid[i] = i;
	}

	int NumBulletToStack = 0;
	for (int i=0;i<72;i++)
	{
		if (slotid[i] != -1)
		{
			wiInventoryItem mag = loadout_->Items[slotid[i]];
			NumBulletToStack += (mag.Var1==-1 ? wac->m_Clipsize : mag.Var1)*mag.quantity;
			//r3dOutToLog("Var1 %d\n",mag.Var1);
			DoRemoveItems(slotid[i]);
		}
	}

	int NumLeftBulletToStack = NumBulletToStack;
	//int NumAttmStack = NumBulletToStack/wac->m_Clipsize;

	//if (NumAttmStack < 1) NumAttmStack = 1;
	//r3dOutToLog("%d\n",NumLeftBulletToStack);

	while (NumLeftBulletToStack > 0)
	{
		wiInventoryItem clips;
		clips.itemID = wi.itemID;
		clips.quantity = 1;
		if (NumLeftBulletToStack >= wac->m_Clipsize)
		{
			clips.Var1 = -1; // -1 is full bullets. this is for stackable Clips.
			NumLeftBulletToStack -= wac->m_Clipsize;
		}
		else
		{
			clips.Var1 = NumLeftBulletToStack;
			NumLeftBulletToStack = -1;
		}
		if (!BackpackAddItem(clips)) // this is happened , your backpack is full!
		{
			// clear all items and fill the old items loadout.
			DoRemoveAllItems(this);
			for (int i=0;i<72;i++)
			{
				if (OldItems[i].itemID > 0)
				{
					BackpackAddItem(OldItems[i]);
				}
			}
			break; // stop this loop.
		}
		//r3dOutToLog("%d\n",NumLeftBulletToStack);
	}

	for (int i=0;i<72;i++)
	{
		OldItems[i].Reset();

		OldItems[i] = loadout_->Items[i];
	}
	r3dOutToLog("%s Stack Clips %s %d %d %d\n",userName,NumLeftBulletToStack > 0 ? "Failed" : "Success",wi.itemID,NumBulletToStack,wac->m_Clipsize);
	gServerLogic.ApiPlayerUpdateChar(this);
}
void obj_ServerPlayer::OnNetPacket(const PKT_C2S_PlayerWeapDataRep_s& n)
{
	lastWeapDataRep = r3dGetTime();

	// if weapon data was updated more that once it mean that updated happened in middle of the game
	// so skip validation
	if(gServerLogic.weaponDataUpdates_ >= 2)
		return;

	for(int i=0; i<2; i++)
	{
		if(m_WeaponArray[i] == NULL)
			continue;
		DWORD hash = m_WeaponArray[i]->getConfig()->GetClientParametersHash();

		if(hash == n.weaponsDataHash[i])
			continue;

		const WeaponConfig& wc1 = *m_WeaponArray[i]->getConfig();
		WeaponConfig wc2(n.debug_wid[i]); 
		wc2.copyParametersFrom(n.debug_winfo[i]);

		if(wc1.m_itemID != wc2.m_itemID)
		{
			/*char chatmessage[128] = {0};
			PKT_C2C_ChatMessage_s n;
			sprintf(chatmessage, "Kicked '%s' from server : weapDataRep different",loadout_->Gamertag);
			r3dscpy(n.gamertag, "System");
			r3dscpy(n.msg, chatmessage);
			n.msgChannel = 1;
			n.userFlag = 2;
			gServerLogic.p2pBroadcastToAll(NULL, &n, sizeof(n), true);*/

			gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_Protocol, false, "weapDataRep different",
				"slot%d %d - %d", i, wc1.m_itemID, wc2.m_itemID);
		}
		else
		{
			// create diffs string for logging
			char diffs[4096] = "";
			if(fabs((float)wc1.m_spread - (float)wc2.m_spread) > 0.01f)
				sprintf(diffs + strlen(diffs), "spread:%.2f/%.2f ", (float)wc1.m_spread, (float)wc2.m_spread);
			if(fabs((float)wc1.m_recoil - (float)wc2.m_recoil) > 0.01f)
				sprintf(diffs + strlen(diffs), "recoil:%.2f/%.2f ", (float)wc1.m_recoil, (float)wc2.m_recoil);
			if(fabs((float)wc1.m_reloadTime - (float)wc2.m_reloadTime) > 0.01f)
				sprintf(diffs + strlen(diffs), "reloadtime:%.2f/%.2f ", (float)wc1.m_reloadTime, (float)wc2.m_reloadTime);
			if(fabs((float)wc1.m_fireDelay - (float)wc2.m_fireDelay) > 0.01f)
				sprintf(diffs + strlen(diffs), "rateoffire:%.2f/%.2f ", (float)wc1.m_fireDelay, (float)wc2.m_fireDelay);
			if(fabs((float)wc1.m_AmmoSpeed - (float)wc2.m_AmmoSpeed) > 0.01f)
				sprintf(diffs + strlen(diffs), "ammospeed:%.2f/%.2f ", (float)wc1.m_AmmoSpeed, (float)wc2.m_AmmoSpeed);

			// report it only once per session (for now, because there is no disconnect yet)
			if(diffs[0] && !weapCheatReported)
			{
				weapCheatReported = true;
				gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_BadWeapData, false, "weapDataRep different",
					"id:%d, diff:%s", wc1.m_itemID, diffs);
			}
			// automatic ban.
			if(diffs[0])
			{
				g_AsyncApiMgr->AddJob(new CJobBanID(profile_.CustomerID,diffs));
				gMasterServerLogic.SendNoticeMsg("FairPlay Banned '%s'",userName);
				gServerLogic.DisconnectPeer(peerId_,true,diffs);
			}
		}
	}
}

void obj_ServerPlayer::SetLatePacketsBarrier(const char* reason)
{
	if(isTargetDummy_)
		return;

	r3dOutToLog("peer%02d, SetLatePacketsBarrier: %s\n", peerId_, reason);

	packetBarrierReason = reason;
	myPacketSequence++;
	lastWeapDataRep = r3dGetTime();

	PKT_C2C_PacketBarrier_s n;
	gServerLogic.p2pSendToPeer(peerId_, this, &n, sizeof(n));

	// NOTE:
	// from now on, we'll ignore received packets until client ack us with same barrier packet.
	// so, any fire/move/etc requests that will invalidate logical state of player will be automatically ignored
}


#undef DEFINE_GAMEOBJ_PACKET_HANDLER
#define DEFINE_GAMEOBJ_PACKET_HANDLER(xxx) \
	case xxx: { \
	const xxx##_s&n = *(xxx##_s*)packetData; \
	if(packetSize != sizeof(n)) { \
	r3dOutToLog("!!!!errror!!!! %s packetSize %d != %d\n", #xxx, packetSize, sizeof(n)); \
	return TRUE; \
	} \
	OnNetPacket(n); \
	return TRUE; \
}

BOOL obj_ServerPlayer::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	//@TODO somehow check that originator of that packet have playerIdx that match peer

	// packets that ignore packet sequence
	switch(EventID)
	{
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PacketBarrier);
	}

	if(myPacketSequence != clientPacketSequence)
	{
		// we get late packet after late packet barrier, skip it
		r3dOutToLog("peer%02d, CustomerID:%d LatePacket %d %s\n", peerId_, profile_.CustomerID, EventID, packetBarrierReason);
		return TRUE;
	}

	// packets while dead
	switch(EventID)
	{
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveSetCell);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveRel);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_DisconnectReq);
	}

	if(wasDisconnected_)
		return TRUE;

	if(loadout_->Alive == 0)
	{
		//r3dOutToLog("peer%02d, CustomerID:%d packet while dead %d\n", peerId_, profile_.CustomerID, EventID);
		return TRUE;
	}

	switch(EventID)
	{
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_PlayerWeapDataRep);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerJump);
		//DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_SetVelocity);

		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_StackClips);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerReload);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerFired);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitNothing);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_CarKill);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeAccept);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_BuyItemReq);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeCancel);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeOptoBack);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_WpnLog);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeRequest);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_PlayerState);

		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitStatic);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitStaticPierced);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerHitDynamic);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_PlayerChangeBackpack);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerReadyGrenade);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerThrewGrenade);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_BackpackDrop);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_BackpackSwap);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_BackpackJoin);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_InventoryOp);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_FallingDamage);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_PlayerEquipAttachment);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_PlayerRemoveAttachment);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_CarPass);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerSwitchWeapon);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerUseItem);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_PlayerCraftItem);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeAccept2);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupInvite);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupNoAccept);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupKick);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_GroupAccept);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_CarStatus);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_ValidateBackpack);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_UnloadClipReq);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_TradeBacktoOp);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_ValidateEnvironment);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_SendHelpCall);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_HackShieldLog);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_BulletValidateConfig);
	}

	return FALSE;
}
#undef DEFINE_GAMEOBJ_PACKET_HANDLER

DefaultPacket* obj_ServerPlayer::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreatePlayer_s n;

	r3d_assert(GetNetworkID());

	n.CustomerID= profile_.CustomerID ^ 0x54281162; // encode CustomerID so it won't look linear on client side
	n.playerIdx = BYTE(GetNetworkID() - NETID_PLAYERS_START);
	n.spawnPos  = GetPosition();
	n.moveCell  = netMover.SrvGetCell();
	n.spawnDir  = m_PlayerRotation;
	n.weapIndex = m_SelectedWeapon;
	n.isFreeHands = m_ForcedEmptyHands?1:0;

	// loadout part
	const wiCharDataFull& slot = *loadout_;
	n.HeroItemID = slot.HeroItemID;
	n.HeadIdx    = (BYTE)slot.HeadIdx;
	n.BodyIdx    = (BYTE)slot.BodyIdx;
	n.LegsIdx    = (BYTE)slot.LegsIdx;
	n.WeaponID0  = slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID;
	n.WeaponID1  = slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID;
	n.ArmorID    = slot.Items[wiCharDataFull::CHAR_LOADOUT_ARMOR].itemID;
	n.HeadGearID = slot.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID;
	n.Item0      = slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM1].itemID;
	n.Item1      = slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM2].itemID;
	n.Item2      = slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM3].itemID;
	n.Item3      = slot.Items[wiCharDataFull::CHAR_LOADOUT_ITEM4].itemID;
	n.BackpackID = slot.BackpackID;

	DWORD IP = gServerLogic.net_->GetPeerIp(peerId_);

	sprintf(n.ip,"");

	n.Attm0      = GetWeaponNetAttachment(0);
	n.Attm1      = GetWeaponNetAttachment(1);

	r3dscpy(n.gamertag, slot.Gamertag);
	n.ClanID = slot.ClanID;
	n.GroupID = slot.GroupID;
	r3dscpy(n.ClanTag, slot.ClanTag);
	n.ClanTagColor = slot.ClanTagColor;

	n.Reputation = slot.Stats.Reputation;

	*out_size = sizeof(n);
	return &n;
}

void obj_ServerPlayer::RelayPacket(const DefaultPacket* packetData, int packetSize, bool guaranteedAndOrdered)
{
	gServerLogic.RelayPacket(peerId_, this, packetData, packetSize, guaranteedAndOrdered);
}