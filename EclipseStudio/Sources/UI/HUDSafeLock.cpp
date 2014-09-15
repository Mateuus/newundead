#pragma region VAULT_IS_WORKING
#include "r3dPCH.h"
#include "r3d.h"

#include "r3dDebug.h"

#include "HUDSafeLock.h"
#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/ai/AI_Player.h"

#include "../multiplayer/MasterServerLogic.h"
#include "../multiplayer/ClientGameLogic.h"
#include "backend/WOBackendAPI.h"
#include "ObjectsCode/Gameplay/obj_SafeLock.h"


extern const char* getReputationString(int Reputation);


HUDSafeLock::HUDSafeLock() :
isActive_(false),
isInit_(false),
prevKeyboardCaptureMovie(NULL)
{

}

HUDSafeLock::~HUDSafeLock()
{

}

void HUDSafeLock::AddSafeLock(int Safe, const char* Password)
{
	SafeLockID2 = Safe;
	strcpy(Pass,Password);
}

void HUDSafeLock::AddDate(const char* Date)
{
	strcpy(dateforexpire,Date);
}

void HUDSafeLock::EndSesion()
{
		PKT_C2S_SafelockData_s n;
		n.Status = 5;
		n.State = 0;
		n.StateSesion = 0;
		n.MapID = gClientLogic().m_gameInfo.mapId;
		n.GameServerID = gClientLogic().m_gameInfo.gameServerId;
		n.CustomerID = gClientLogic().localPlayer_->CustomerID;
		n.iDSafeLock = SafeLockID2;
		strcpy(n.Password,Pass); 
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
}

bool HUDSafeLock::BackPackFULL(int InventoryID)
{
	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	bool Backfull = false;

	extern bool storecat_IsItemStackable(uint32_t ItemID);
	bool isStackable = storecat_IsItemStackable(plr->SafeItems[InventoryID]);
	for(int i=0; i<slot.BackpackSize; i++)
	{
		const wiInventoryItem& wi2 = slot.Items[i];

		// can stack only non-modified items
		if(isStackable && wi2.itemID == plr->SafeItems[InventoryID] && plr->Var1[InventoryID] < 0 && wi2.Var1 < 0) {
			Backfull = true;
			break;
		}

		if (wi2.itemID == 0 && Backfull == false && i != 0 && i != 1 && i != 6 && i != 7)
		{
			Backfull = true;
			break;
		}

	}
	if (!isStackable && slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON1].itemID == 0)
	{
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(plr->SafeItems[InventoryID]);
		if (bic)
		if(bic->category == storecat_ASR || bic->category == storecat_SNP || bic->category == storecat_SHTG || bic->category == storecat_MG || bic->category == storecat_SMG)
			Backfull = true;
	}
	if (!isStackable && slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 0)
	{
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(plr->SafeItems[InventoryID]);
		if (bic)
		if(bic->category == storecat_MELEE || bic->category == storecat_HG)
			Backfull = true;
	}
	if (!isStackable && slot.Items[wiCharDataFull::CHAR_LOADOUT_ARMOR].itemID == 0)
	{
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(plr->SafeItems[InventoryID]);
		if (bic)
		if(bic->category == storecat_Armor)
			Backfull = true;
	}
	if (slot.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 0)
	{
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(plr->SafeItems[InventoryID]);
		if (bic)
		if(bic->category == storecat_Helmet)
			Backfull = true;
	}

	return Backfull;
}
bool HUDSafeLock::isverybig(int Quantity, int Item, int grid)
{
    obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	float totalWeight = slot.getTotalWeight();

	const BaseItemConfig* bic = g_pWeaponArmory->getConfig(slot.Items[grid].itemID);
        if(bic)
            totalWeight -= bic->m_Weight*slot.Items[grid].quantity;

        bic = g_pWeaponArmory->getConfig(Item);
        if(bic)
            totalWeight += bic->m_Weight*Quantity;
            
		const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(slot.BackpackID);
        r3d_assert(bc);
        if(totalWeight > bc->m_maxWeight)
        {
            return true;
        }
	return false;
}

bool HUDSafeLock::SafeLockisFull(int InventoryID)
{
	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	bool Safelockfull = true;

	extern bool storecat_IsItemStackable(uint32_t ItemID);
	bool isStackable = storecat_IsItemStackable(slot.Items[InventoryID].itemID);
	for(int i=1; i<33; i++)
	{
		if(isStackable && plr->SafeItems[i] == slot.Items[InventoryID].itemID && slot.Items[InventoryID].Var1 < 0 && plr->Var1[i] < 0) {
			Safelockfull = false;
			break;
		}
		if (plr->SafeItems[i] == 0 && Safelockfull == true)
		{
			Safelockfull = false;
			break;
		}
	}
	return Safelockfull;
}

void HUDSafeLock::addClientSurvivor(const wiCharDataFull& slot)
{
    Scaleform::GFx::Value var[22];
    char tmpGamertag[128];
    if(slot.ClanID != 0)
        sprintf(tmpGamertag, "[%s] %s", slot.ClanTag, slot.Gamertag);
    else
        r3dscpy(tmpGamertag, slot.Gamertag);
    var[0].SetString(tmpGamertag);
    var[1].SetNumber(slot.Health);
    var[2].SetNumber(slot.Stats.XP);
    var[3].SetNumber(slot.Stats.TimePlayed);
    var[4].SetNumber(slot.Hardcore);
    var[5].SetNumber(slot.HeroItemID);
    var[6].SetNumber(slot.HeadIdx);
    var[7].SetNumber(slot.BodyIdx);
    var[8].SetNumber(slot.LegsIdx);
    var[9].SetNumber(slot.Alive);
    var[10].SetNumber(slot.Hunger);
    var[11].SetNumber(slot.Thirst);
    var[12].SetNumber(slot.Toxic);
    var[13].SetNumber(slot.BackpackID);
    var[14].SetNumber(slot.BackpackSize);
    var[15].SetNumber(0);        // weight
    var[16].SetNumber(slot.Stats.KilledZombies);        // zombies Killed
    var[17].SetNumber(slot.Stats.KilledBandits);        // bandits killed
    var[18].SetNumber(slot.Stats.KilledSurvivors);        // civilians killed
	char repu[128];
	sprintf(repu,"[%s] [%d]",getReputationString(slot.Stats.Reputation),slot.Stats.Reputation);
	var[19].SetString(repu);	// alignment
	switch(slot.GameMapId)
		{
			case GBGameInfo::MAPID_WZ_Colorado:
			var[20].SetString("COLORADO PVP");	// last Map
			break;
		/*	case GBGameInfo::MAPID_WZ_PVE_Colorado:
			var[20].SetString("COLORADO PVE");	// last Map
			break;
			case GBGameInfo::MAPID_WZ_Cliffside:
			var[20].SetString("CLIFFSIDE PVP");	// last Map
			break;
			case GBGameInfo::MAPID_CaliWood:
			var[20].SetString("CALIWOOD");	// last Map
			break;*/
			case GBGameInfo::MAPID_ServerTest:
			var[20].SetString("DEVMAP");	// last Map
			break;
			default:
			var[20].SetString("UNKNOWN");	// last Map
			break;
		}
    var[21].SetBoolean(true);
    gfxMovie.Invoke("_root.api.addClientSurvivor", var, 22);
    gfxMovie.Invoke("_root.api.clearBackpack", "");
    addBackpackItems(slot);

}

void HUDSafeLock::addBackpackItems(const wiCharDataFull& slot)
{
	{
		gfxMovie.Invoke("_root.api.clearBackpack", "");
		gfxMovie.Invoke("_root.api.clearBackpacks", "");
	}
    Scaleform::GFx::Value var[7];
    r3dOutToLog("AddBackPackItems\n");

    for (int a = 0; a < slot.BackpackSize; a++)
    {
        if (slot.Items[a].InventoryID != 0)
        {
            var[0].SetInt(a);
            var[1].SetUInt(uint32_t(slot.Items[a].InventoryID));
            var[2].SetUInt(slot.Items[a].itemID);
            var[3].SetInt(slot.Items[a].quantity);
            var[4].SetInt(slot.Items[a].Var1);
            var[5].SetInt(slot.Items[a].Var2);
            char tmpStr[128] = {0};
            getAdditionalDescForItem(slot.Items[a].itemID, slot.Items[a].Var1, slot.Items[a].Var2, tmpStr);
            var[6].SetString(tmpStr);
            gfxMovie.Invoke("_root.api.addBackpackItem", var, 7);
        }
    }
}
void HUDSafeLock::updateInventoryAndSkillItems()
{
	gfxMovie.Invoke("_root.api.clearInventory", NULL, 0);

	r3dOutToLog("Show Inventory Items\n");
    Scaleform::GFx::Value var[7];

	CWOBackendReq req("api_GetSafelockDATA.aspx");
	req.AddParam("411", "1");

	if(!req.Issue())
	{
		r3dOutToLog("ReadDBSafelock FAILED, code: %d\n", req.resultCode_);
		return;
	}

	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	pugi::xml_node xmlSafelock = xmlFile.child("SafeLock_items");
	int count = 0;
	SafeWeight = 0.0f;

	for (int i=1;i<33;i++)
	{
			gClientLogic().localPlayer_->SafeItems[i]=0;
			gClientLogic().localPlayer_->Var1[i]=0;
			gClientLogic().localPlayer_->Var2[i]=0;
			gClientLogic().localPlayer_->Quantity[i]=0;
	}

	while(!xmlSafelock.empty())
	{
		r3dPoint3D Position;
		SafelockID = xmlSafelock.attribute("SafeLockID").as_uint();
		ItemID = xmlSafelock.attribute("ItemID").as_uint();
		uint32_t GameServerID = xmlSafelock.attribute("GameServerID").as_uint();

		r3dscpy(Password, xmlSafelock.attribute("Password").value());
		ExpireTime = xmlSafelock.attribute("ExpireTime").as_uint();
		sscanf(xmlSafelock.attribute("GamePos").value(), "%f %f %f", &Position.x, &Position.y, &Position.z);
		float rotation = xmlSafelock.attribute("GameRot").as_float();
		MapID = xmlSafelock.attribute("MapID").as_uint();
		Quantity = xmlSafelock.attribute("Quantity").as_uint();
		Var1 = xmlSafelock.attribute("Var1").as_uint();
		Var2 = xmlSafelock.attribute("Var2").as_uint();
		//int Durability = xmlSafelock.attribute("Durability").as_uint();

		if (ItemID==0 && SafelockID == SafeLockID2 && GameServerID == gClientLogic().m_gameInfo.gameServerId)
		CustomerID = xmlSafelock.attribute("CustomerID").as_int();

		if (ItemID > 0 && SafelockID == SafeLockID2 && GameServerID == gClientLogic().m_gameInfo.gameServerId)
		{

			count++;

		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(ItemID);
			
        if(bic)
            SafeWeight += bic->m_Weight*Quantity;

			gClientLogic().localPlayer_->SafeItems[count]=ItemID;
			gClientLogic().localPlayer_->Var1[count]=Var1;
			gClientLogic().localPlayer_->Var2[count]=Var2;
			gClientLogic().localPlayer_->Quantity[count]=Quantity;
			//gClientLogic().localPlayer_->Durability[count]=Durability;
			var[0].SetUInt(count);
			var[1].SetUInt(ItemID);
			var[2].SetNumber(Quantity);
			var[3].SetNumber(Var1);
			var[4].SetNumber(Var2);
			 bool isConsumable = false;
            {
                const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(ItemID);
                if(wc && wc->category == storecat_UsableItem && wc->m_isConsumable)
                   isConsumable = true;
            }
            var[5].SetBoolean(isConsumable);
            char tmpStr[128] = {0};
            getAdditionalDescForItem(ItemID, Var1, Var2, tmpStr);
            var[6].SetString(tmpStr);
			gfxMovie.Invoke("_root.api.addInventoryItem", var, 7);
		}

		xmlSafelock = xmlSafelock.next_sibling();
	}
}


void HUDSafeLock::updateSurvivorTotalWeight()
{
    obj_Player* plr = gClientLogic().localPlayer_;
    r3d_assert(plr);

    char tmpGamertag[128];
    if(plr->CurLoadout.ClanID != 0)
        sprintf(tmpGamertag, "[%s] %s", plr->CurLoadout.ClanTag, plr->CurLoadout.Gamertag);
    else
        r3dscpy(tmpGamertag, plr->CurLoadout.Gamertag);

    float totalWeight = plr->CurLoadout.getTotalWeight();

    Scaleform::GFx::Value var[2];
    var[0].SetString(tmpGamertag);
    var[1].SetNumber(totalWeight);
    gfxMovie.Invoke("_root.api.updateClientSurvivorWeight", var, 2);
}

void HUDSafeLock::eventChangeKeyCode(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	obj_Player* plr = gClientLogic().localPlayer_;
	
	char NewPass[512];
	strcpy(NewPass,args[0].GetString());

	if (strcmp(Pass,NewPass) == 0)
	{
		if (CustomerID == plr->CustomerID)
		{
			Scaleform::GFx::Value var[3];
			var[0].SetStringW(gLangMngr.getString("$DataUpdateGood"));
			var[1].SetBoolean(true);
			var[2].SetString("");
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
			return;
		}
	}

	if (CustomerID == plr->CustomerID)
	{
		PKT_C2S_SafelockData_s n;
		n.Status = 8;
		strcpy(n.NewPassword,NewPass);
		n.IDofSafeLock=SafeLockID2;
		n.IDPlayer=gClientLogic().localPlayer_->GetNetworkID();
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);

		Scaleform::GFx::Value var[3];
		var[0].SetStringW(gLangMngr.getString("$DataUpdateGood"));
		var[1].SetBoolean(true);
		var[2].SetString("");
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
	}
	else {
		Scaleform::GFx::Value var[3];
		var[0].SetStringW(gLangMngr.getString("$FR_YouNotOwnerSF"));
		var[1].SetBoolean(true);
		var[2].SetString("");
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
	}
}

void HUDSafeLock::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
    Deactivate();
}

void HUDSafeLock::eventBackpackToInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);
	BackPackToInv = r3dGetTime();

    m_gridFrom = args[0].GetInt();
    m_Amount = args[1].GetInt();

	wiInventoryItem& wi = plr->CurLoadout.Items[m_gridFrom];

	const BaseItemConfig* bic = g_pWeaponArmory->getConfig(plr->CurLoadout.Items[m_gridFrom].itemID);

	if (SafeWeight+bic->m_Weight*Quantity >= 300)
	{
       Scaleform::GFx::Value var[3];
       var[0].SetStringW(gLangMngr.getString("InfoMsg_TooMuchWeight"));
       var[1].SetBoolean(true);
       var[2].SetString("");
       gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
		return;
	}

	if (SafeLockisFull(m_gridFrom))
	{
       Scaleform::GFx::Value var[3];
       var[0].SetStringW(gLangMngr.getString("InGameUI_ErrorMsg_SafelockNoSpace"));
       var[1].SetBoolean(true);
       var[2].SetString("");
       gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
		return;
	}

	PKT_C2S_SafelockData_s n;
	n.Status = 0;
	n.itemIDorGrid = m_gridFrom;
	n.Var1 = wi.Var1;
	n.Var2 = wi.Var2;
	n.Quantity = m_Amount;
	n.SafelockID = SafeLockID2;
	n.MapID = MapID;
	n.ExpireTime = ExpireTime;
	strcpy(n.Password,Pass);
	//n.Durability = wi.Durability;
	n.GameServerID = gClientLogic().m_gameInfo.gameServerId;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);

	Scaleform::GFx::Value var[3];
    var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
    var[1].SetBoolean(false);
    var[2].SetString("");
    gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);

	OnBackpackToInventorySuccess = true;
}

void HUDSafeLock::eventBackpackFromInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);

	BackPackFromInv = r3dGetTime();
	wiCharDataFull& slot = plr->CurLoadout;

    m_inventoryID = args[0].GetUInt();
    m_gridTo = args[1].GetInt();
    m_Amount = args[2].GetInt();

	if (isverybig(1,plr->SafeItems[m_inventoryID],m_gridTo) == true)
	{
       Scaleform::GFx::Value var[3];
       var[0].SetStringW(gLangMngr.getString("FR_PAUSE_TOO_MUCH_WEIGHT"));
       var[1].SetBoolean(true);
       var[2].SetString("");
       gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
	   return;
	}
	if(!BackPackFULL((int)m_inventoryID))
	{
       Scaleform::GFx::Value var[3];
       var[0].SetStringW(gLangMngr.getString("InfoMsg_NoFreeBackpackSlots"));
       var[1].SetBoolean(true);
       var[2].SetString("");
       gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
		return;
	}
	if(plr->SafeItems[m_inventoryID] != 0)
	{
	   PKT_C2S_SafelockData_s n;
	   n.Status = 1;
	   n.itemIDorGrid = plr->SafeItems[m_inventoryID];
	   n.Var1 = gClientLogic().localPlayer_->Var1[m_inventoryID];
	   n.Var2 = gClientLogic().localPlayer_->Var2[m_inventoryID];
	   n.Quantity = 1;
	   n.SafelockID = SafeLockID2;
	   n.MapID = MapID;
	   n.ExpireTime = ExpireTime;
	   strcpy(n.Password,Pass);
	   //n.Durability = gClientLogic().localPlayer_->Durability[m_inventoryID];
	   n.GameServerID = gClientLogic().m_gameInfo.gameServerId;
	   p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);

	   Scaleform::GFx::Value var[2];
       var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
       var[1].SetBoolean(false);
       gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
	}
	else
	{
		r3dOutToLog("#### ERROR!!! ItemID = %i\n", plr->SafeItems[m_inventoryID]);
	}
	OnBackpackFromInventorySuccess = true;
}

void HUDSafeLock::eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{

	r3d_assert(argCount == 2);
	SwapGridFrom = args[0].GetInt();
	SwapGridTo = args[1].GetInt();

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);
	wiCharDataFull& slot = plr->CurLoadout;

	//local logic
	wiInventoryItem& wi1 = slot.Items[SwapGridFrom];
	wiInventoryItem& wi2 = slot.Items[SwapGridTo];

	// if we can stack slots - do it
	extern bool storecat_IsItemStackable(uint32_t ItemID);
	if(wi1.itemID && wi1.itemID == wi2.itemID && storecat_IsItemStackable(wi1.itemID) && wi1.Var1 < 0 && wi2.Var1 < 0)
	{
		wi2.quantity += wi1.quantity;
		wi1.Reset();

		PKT_C2S_BackpackJoin_s n;
		n.SlotFrom = SwapGridFrom;
		n.SlotTo   = SwapGridTo;
		p2pSendToHost(plr, &n, sizeof(n));
	}
	else
	{
		R3D_SWAP(wi1, wi2);

		PKT_C2S_BackpackSwap_s n;
		n.SlotFrom = SwapGridFrom;
		n.SlotTo   = SwapGridTo;
		p2pSendToHost(plr, &n, sizeof(n));
	}

	Scaleform::GFx::Value var[2];
    var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
    var[1].SetBoolean(false);
    gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);

	OnBackpackGridSwapSuccess = true;
	
}
void HUDSafeLock::eventPickupLockbox(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;
	bool Backfull = false;

	int SlotsUSed = 0;

	if (CustomerID != plr->CustomerID)
	{
		Scaleform::GFx::Value var[3];
		var[0].SetStringW(gLangMngr.getString("$FR_YouNotOwnerSF"));
		var[1].SetBoolean(true);
		var[2].SetString("");
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
		return;
	}

	for (int i=1;i<33;i++)
	{
		if (gClientLogic().localPlayer_->SafeItems[i] != 0)
		{
			SlotsUSed++;
		}
	}

	if (isverybig(1,101348,0) == true)
	{
		Scaleform::GFx::Value var[3];
		var[0].SetStringW(gLangMngr.getString("FR_PAUSE_TOO_MUCH_WEIGHT"));
		var[1].SetBoolean(true);
		var[2].SetString("");
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
		return;
	}


	for(int i=0; i<slot.BackpackSize; i++)
	{
		const wiInventoryItem& wi2 = slot.Items[i];
		if (wi2.itemID == 0 && Backfull == false && i != 0 && i != 1 && i != 6 && i != 7)
		{
			Backfull = true;
			break;
		}
	}
	if(!Backfull)
	{
		Scaleform::GFx::Value var[3];
		var[0].SetStringW(gLangMngr.getString("InfoMsg_NoFreeBackpackSlots"));
		var[1].SetBoolean(true);
		var[2].SetString("");
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
		return;
	}

	if (SlotsUSed>0)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("$NeedEmptyFrist"));
		var[1].SetBoolean(true);
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}
	
	PKT_C2S_SafelockData_s n;
	n.Status = 3;
	n.SafelockID = SafeLockID2;
	n.GameServerID = gClientLogic().m_gameInfo.gameServerId;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);
	Deactivate();
}

bool HUDSafeLock::Init()
{
     if(!gfxMovie.Load("Data\\Menu\\WarZ_HUD_Safelock.swf", false))
    {
         return false;
    }

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDSafeLock>(this, &HUDSafeLock::FUNC)
     gfxMovie.RegisterEventHandler("eventBackpackFromInventory", MAKE_CALLBACK(eventBackpackFromInventory));
    gfxMovie.RegisterEventHandler("eventPickupLockbox", MAKE_CALLBACK(eventPickupLockbox));
    gfxMovie.RegisterEventHandler("eventBackpackGridSwap", MAKE_CALLBACK(eventBackpackGridSwap));
    gfxMovie.RegisterEventHandler("eventReturnToGame", MAKE_CALLBACK(eventReturnToGame));
	gfxMovie.RegisterEventHandler("eventBackpackToInventory", MAKE_CALLBACK(eventBackpackToInventory));
	gfxMovie.RegisterEventHandler("eventChangeKeyCode", MAKE_CALLBACK(eventChangeKeyCode));

    gfxMovie.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);
    itemInventory_.initialize(&gfxMovie);
    isActive_ = false;
    isInit_ = true;
	BackPackToInv = 0.0f;
	BackPackFromInv = 0.0f;
	OnBackpackToInventorySuccess = false;
	OnBackpackFromInventorySuccess = false;

	BackPackGridSwap = 0.0f;
	OnBackpackGridSwapSuccess = false;
    return true;
}

bool HUDSafeLock::Unload()
{
     gfxMovie.Unload();
    isActive_ = false;
    isInit_ = false;
	BackPackToInv = 0.0f;
	BackPackFromInv = 0.0f;
    return true;
}


void HUDSafeLock::Update()
{
  if(OnBackpackToInventorySuccess)
  {
    if ((r3dGetTime() - BackPackToInv) > 1.5)
	{
        obj_Player* plr = gClientLogic().localPlayer_;
	    r3d_assert(plr);
		addBackpackItems(plr->CurLoadout);
	    updateSurvivorTotalWeight();
	    plr->OnBackpackChanged(m_gridFrom);
	    reloadBackpackInfo();
	    updateInventoryAndSkillItems();
		gfxMovie.Invoke("_root.api.hideInfoMsg", "");
		gfxMovie.Invoke("_root.api.backpackToInventorySuccess", "");
		OnBackpackToInventorySuccess = false;
		BackPackToInv = 0;
	}
  }
  if(OnBackpackFromInventorySuccess)
  {
    if ((r3dGetTime() - BackPackFromInv) > 1.5)
	{
        obj_Player* plr = gClientLogic().localPlayer_;
	    r3d_assert(plr);
		addBackpackItems(plr->CurLoadout);
	    updateSurvivorTotalWeight();
	    plr->OnBackpackChanged(m_gridTo);
	    reloadBackpackInfo();
	    updateInventoryAndSkillItems();
		gfxMovie.Invoke("_root.api.hideInfoMsg", "");
        gfxMovie.Invoke("_root.api.backpackFromInventorySuccess", "");
		OnBackpackFromInventorySuccess = false;
		BackPackFromInv = 0;
	}
  }
  if(OnBackpackGridSwapSuccess)
  {
    if ((r3dGetTime() - BackPackGridSwap) > 1.1)
	{
        obj_Player* plr = gClientLogic().localPlayer_;
	    r3d_assert(plr);
		plr->OnBackpackChanged(SwapGridFrom);
	    plr->OnBackpackChanged(SwapGridTo);
		addBackpackItems(plr->CurLoadout);
	    updateSurvivorTotalWeight();
	   
	    reloadBackpackInfo();
	    updateInventoryAndSkillItems();
		gfxMovie.Invoke("_root.api.hideInfoMsg", "");
        gfxMovie.Invoke("_root.api.backpackGridSwapSuccess", "");
		OnBackpackGridSwapSuccess = false;
		BackPackGridSwap = 0;
	}
  }
}

void HUDSafeLock::Draw()
{
     gfxMovie.UpdateAndDraw();
}


void HUDSafeLock::Deactivate()
{
    if (async_.Processing())
    {
        return;
    }
	
    Scaleform::GFx::Value var[1];
    var[0].SetString("menu_close");
    gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);

    if(prevKeyboardCaptureMovie)
    {
        prevKeyboardCaptureMovie->SetKeyboardCapture();
        prevKeyboardCaptureMovie = NULL;
    }
    if(!g_cursor_mode->GetInt())
    {
        r3dMouse::Hide();
    }
    isActive_ = false;
	EndSesion();
}


void HUDSafeLock::Activate()
{
    prevKeyboardCaptureMovie = gfxMovie.SetKeyboardCapture(); // for mouse scroll events

    r3d_assert(!isActive_);
    r3dMouse::Show();
    isActive_ = true;
	BackPackToInv = 0.0f;
	OnBackpackToInventorySuccess = false;
    addClientSurvivor(gClientLogic().localPlayer_->CurLoadout);
    updateInventoryAndSkillItems();
    updateSurvivorTotalWeight();
    reloadBackpackInfo();

	Scaleform::GFx::Value var[1];
	char Text[512];
	sprintf(Text,"$FR_SAFELOCK_TITLE - $Safelock_Expire %s",dateforexpire);
	var[0].SetStringW(gLangMngr.getString(Text));
    gfxMovie.Invoke("_root.api.showInventoryScreen", var, 1);

   
    var[0].SetString("menu_open");
    gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);
}
void HUDSafeLock::reloadBackpackInfo()
{
    {
        gfxMovie.Invoke("_root.api.clearBackpack", "");
        gfxMovie.Invoke("_root.api.clearBackpacks", "");
    }

    std::vector<uint32_t> uniqueBackpacks; // to filter identical backpack
    int backpackSlotIDInc = 0;

    {
        obj_Player* plr = gClientLogic().localPlayer_;
        r3d_assert(plr);
        wiCharDataFull& slot = plr->CurLoadout;


        Scaleform::GFx::Value var[7];
        for (int a = 0; a < slot.BackpackSize; a++)
        {
            if (slot.Items[a].itemID != 0)
            {
                var[0].SetInt(a);
                var[1].SetUInt(0); // not used for game
                var[2].SetUInt(slot.Items[a].itemID);
                var[3].SetInt(slot.Items[a].quantity);
                var[4].SetInt(slot.Items[a].Var1);
                var[5].SetInt(slot.Items[a].Var2);
                char tmpStr[128] = {0};
                getAdditionalDescForItem(slot.Items[a].itemID, slot.Items[a].Var1, slot.Items[a].Var2, tmpStr);
                var[6].SetString(tmpStr);
                gfxMovie.Invoke("_root.api.addBackpackItem", var, 7);


                const BackpackConfig* bpc = g_pWeaponArmory->getBackpackConfig(slot.Items[a].itemID);
                if(bpc)
                {
                    if(std::find<std::vector<uint32_t>::iterator, uint32_t>(uniqueBackpacks.begin(), uniqueBackpacks.end(), slot.Items[a].itemID) != uniqueBackpacks.end())
                        continue;

                    // add backpack info
                    var[0].SetInt(backpackSlotIDInc++);
                    var[1].SetUInt(slot.Items[a].itemID);
                    gfxMovie.Invoke("_root.api.addBackpack", var, 2);


                    uniqueBackpacks.push_back(slot.Items[a].itemID);
                }
            }
        }
    }
    gfxMovie.Invoke("_root.api.Main.Inventory.showBackpack", "");
}
