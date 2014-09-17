#pragma region VAULT_IS_WORKING
#include "r3dPCH.h"
#include "r3d.h"


#include "r3dDebug.h"


#include "HUDVault.h"

#include "backend/WOBackendAPI.h"
#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/ai/AI_Player.h"


#include "../multiplayer/MasterServerLogic.h"
#include "../multiplayer/ClientGameLogic.h"


extern const char* getReputationString(int Reputation);


HUDVault::HUDVault() :
isActive_(false),
isInit_(false),
prevKeyboardCaptureMovie(NULL)
{
}


HUDVault::~HUDVault()
{
}

bool HUDVault::BackPackFULL(int ItemID, int Var1)
{
	obj_Player* plr = gClientLogic().localPlayer_;
	wiCharDataFull& slot = plr->CurLoadout;

	bool Backfull = false;

	extern bool storecat_IsItemStackable(uint32_t ItemID);
	bool isStackable = storecat_IsItemStackable(ItemID);
	for(int i=0; i<slot.BackpackSize; i++)
	{
		const wiInventoryItem& wi2 = slot.Items[i];

		// can stack only non-modified items
		if(isStackable && wi2.itemID == ItemID && Var1 < 0 && wi2.Var1 < 0) {
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
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(ItemID);
		if (bic)
		if(bic->category == storecat_ASR || bic->category == storecat_SNP || bic->category == storecat_SHTG || bic->category == storecat_MG || bic->category == storecat_SMG)
			Backfull = true;
	}
	if (!isStackable && slot.Items[wiCharDataFull::CHAR_LOADOUT_WEAPON2].itemID == 0)
	{
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(ItemID);
		if (bic)
		if(bic->category == storecat_MELEE || bic->category == storecat_HG)
			Backfull = true;
	}
	if (!isStackable && slot.Items[wiCharDataFull::CHAR_LOADOUT_ARMOR].itemID == 0)
	{
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(ItemID);
		if (bic)
		if(bic->category == storecat_Armor)
			Backfull = true;
	}
	if (slot.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 0)
	{
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(ItemID);
		if (bic)
		if(bic->category == storecat_Helmet)
			Backfull = true;
	}

	return Backfull;
}

bool HUDVault::isverybig(int Quantity, int Item, int grid)
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

void HUDVault::addClientSurvivor(const wiCharDataFull& slot)
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
			/*case GBGameInfo::MAPID_WZ_PVE_Colorado:
			var[20].SetString("COLORADO PVE");	// last Map
			break;
			case GBGameInfo::MAPID_WZ_Cliffside:
			var[20].SetString("CLIFFSIDE PVP");	// last Map
			break;
			case GBGameInfo::MAPID_CaliWood:
			var[20].SetString("CALIWOOD");	// last Map
			break;*/
/*			case GBGameInfo::MAPID_Devmap:
			var[20].SetString("DEVMAP");	// last Map
			break;
			default:*/
			var[20].SetString("UNKNOWN");	// last Map
			break;
		}
    var[21].SetBoolean(true);

    gfxMovie.Invoke("_root.api.addClientSurvivor", var, 22);

    //gfxMovie.Invoke("_root.api.clearBackpack", NULL, 0);
    gfxMovie.Invoke("_root.api.clearBackpack", "");

    addBackpackItems(slot);
    
}
void HUDVault::addBackpackItems(const wiCharDataFull& slot)
{
    // reset backpack
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
void HUDVault::updateInventoryAndSkillItems()
{
	obj_Player* plr = gClientLogic().localPlayer_;
	gfxMovie.Invoke("_root.api.clearInventory", "");

	r3dOutToLog("Show Inventory Items\n");
    Scaleform::GFx::Value var[7];

	char* g_ServerApiKey = "a5gfd4u8df1jhjs47ws86F";
	CWOBackendReq req("api_GetInventoryData.aspx");
	req.AddSessionInfo(plr->CustomerID, Session);
	req.AddParam("skey1",  g_ServerApiKey);
	req.AddParam("Data", 0);
	req.AddParam("Inventory", 0);
	req.AddParam("CustomerID", plr->CustomerID);
	req.AddParam("CharID", 0);
	req.AddParam("Slot", 0);
	req.AddParam("ItemID", 0);
	req.AddParam("Quantity", 0);
	req.AddParam("Var1", 0);
	req.AddParam("Var2", 0);
	req.AddParam("Category",  0);
	//req.AddParam("Durability",  0);

	if(!req.Issue())
	{
		r3dOutToLog("GetInventoryData FAILED, code: %d\n", req.resultCode_);
		return;
	}

	for (int i=1;i<2048;i++)
	{
			IDInventory[i]=0;
			ItemsOnInventory[i]=0;
			Var_1[i]=0;
			Var_2[i]=0;
			Quantity[i]=0;

			ItemsData[i]=0;
			ItemsItem[i]=0;
			ItemsVar1[i]=0;
			ItemsVar2[i]=0;
			ItemsQuantity[i]=0;
	}
	int count = 0;
	int countBackpack = 0;
	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	pugi::xml_node xmlSafelock = xmlFile.child("UsersInventory");
	LastInventory = 0;
	while(!xmlSafelock.empty())
	{
		
        __int64 InventoryID = xmlSafelock.attribute("InventoryID").as_int64();
        int CustomerID = xmlSafelock.attribute("CustomerID").as_int();
        int CharID = xmlSafelock.attribute("CharID").as_int();
        int BackpackSlot = xmlSafelock.attribute("BackpackSlot").as_int();
        int ItemID = xmlSafelock.attribute("ItemID").as_int();
        int Quantity_1 = xmlSafelock.attribute("Quantity").as_int();
        int Var1_1 = xmlSafelock.attribute("Var1").as_int();
        int Var2_1 = xmlSafelock.attribute("Var2").as_int();
		//int Durability_1 = xmlSafelock.attribute("Durability").as_int();

		if (LastInventory<(int)InventoryID)
		{
			LastInventory=(int)InventoryID;
		}

			IDInventory[count]=(int)InventoryID;
			ItemsOnInventory[count]=ItemID;
			Var_1[count]=Var1_1;
			Var_2[count]=Var2_1;
			//Durability[count]=Durability_1;
			Quantity[count]=Quantity_1;
			count++;

			if (BackpackSlot == -1 || CharID == 0 && BackpackSlot == 0)
			{

			var[0].SetUInt(uint32_t(InventoryID));
			var[1].SetUInt(ItemID);
			var[2].SetNumber(Quantity_1);
			var[3].SetNumber(Var1_1);
			var[4].SetNumber(Var2_1);
			 bool isConsumable = false;
            {
                const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(ItemID);
                if(wc && wc->category == storecat_UsableItem && wc->m_isConsumable)
                   isConsumable = true;
            }
            var[5].SetBoolean(isConsumable);
            char tmpStr[128] = {0};
            getAdditionalDescForItem(ItemID, Var1_1, Var2_1, tmpStr);
            var[6].SetString(tmpStr);
			gfxMovie.Invoke("_root.api.addInventoryItem", var, 7);
			}
			else {
					ItemsData[BackpackSlot]=(int)InventoryID;
					ItemsItem[BackpackSlot]=ItemID;
					ItemsVar1[BackpackSlot]=Var1_1;
					ItemsVar2[BackpackSlot]=Var2_1;
					ItemsQuantity[BackpackSlot]=Quantity_1;
			}


		xmlSafelock = xmlSafelock.next_sibling();
	}
}
void HUDVault::updateSurvivorTotalWeight()
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

void HUDVault::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
    Deactivate();
}

void HUDVault::eventBackpackFromInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
 	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);

	BackPackFromInv = r3dGetTime();
	wiCharDataFull& slot = plr->CurLoadout;

    m_inventoryID = args[0].GetUInt();
    m_gridTo = args[1].GetInt();
    m_gridFrom = args[2].GetInt(); 

    uint32_t itemID = 0;
	for(uint32_t i=0; i<2048; ++i)
	{
		if (IDInventory[i] != 0)
		{
			if (IDInventory[i] == (int)m_inventoryID)
			{
				if (isverybig(1,ItemsOnInventory[i],m_gridTo) == true)
				{
					Scaleform::GFx::Value var[3];
					var[0].SetStringW(gLangMngr.getString("FR_PAUSE_TOO_MUCH_WEIGHT"));
					var[1].SetBoolean(true);
					var[2].SetString("");
					gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
					return;
				}
				if(!BackPackFULL(ItemsOnInventory[i],Var_1[i]))
				{
					Scaleform::GFx::Value var[3];
					var[0].SetStringW(gLangMngr.getString("InfoMsg_NoFreeBackpackSlots"));
					var[1].SetBoolean(true);
					var[2].SetString("");
					gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
					return;
				}
				PKT_C2S_VaultBackpackFromInv_s n;
				n.itemID = ItemsOnInventory[i];
				n.m_inventoryID = IDInventory[i];
				n.var1 = Var_1[i];
				n.var2 = Var_2[i];
				n.Quantity = 1;
				//n.Durability = Durability[i];
				p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);
				m_Amount = Quantity[i];

				Scaleform::GFx::Value var[2];
				var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
				var[1].SetBoolean(false);
				gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);
				break;
			}
		}
	}

	OnBackpackFromInventorySuccess = true;
}

void HUDVault::eventBackpackToInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);
	BackPackToInv = r3dGetTime();

    m_gridFrom = args[0].GetInt();
    m_Amount = args[1].GetInt();

	wiInventoryItem& wi = plr->CurLoadout.Items[m_gridFrom];

	bool CheckStatus = false;
	for(uint32_t i=0; i<2048; ++i)
	{
		if (IDInventory[i] != 0)
		{
			if (IDInventory[i] == ItemsData[m_gridFrom])
			{
				ItemID = ItemsOnInventory[i];
				m_inventoryID = IDInventory[i];
				Var1 = wi.Var1;
				Var2 = wi.Var2;
				CheckStatus=true;
				break;
			}
		}
	}

	if (!CheckStatus)
		return;
	PKT_C2S_VaultBackpackToInv_s n;
	n.Inventory = (int)m_inventoryID;
	n.m_gridFrom = m_gridFrom;
	n.var1 = Var1;
	n.var2 = Var2; 
	n.Quantity = m_Amount;

	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);

	Scaleform::GFx::Value var[3];
    var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
    var[1].SetBoolean(false);
    var[2].SetString("");
    gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);

	OnBackpackToInventorySuccess = true;
}

void HUDVault::eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
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


bool HUDVault::Init()
{
     if(!gfxMovie.Load("Data\\Menu\\WarZ_HUD_PostOffice.swf", false)) 
    {
         return false;
    }


#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDVault>(this, &HUDVault::FUNC)
     gfxMovie.RegisterEventHandler("eventBackpackFromInventory", MAKE_CALLBACK(eventBackpackFromInventory));
    gfxMovie.RegisterEventHandler("eventBackpackToInventory", MAKE_CALLBACK(eventBackpackToInventory));
    gfxMovie.RegisterEventHandler("eventBackpackGridSwap", MAKE_CALLBACK(eventBackpackGridSwap));
    gfxMovie.RegisterEventHandler("eventReturnToGame", MAKE_CALLBACK(eventReturnToGame));


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


bool HUDVault::Unload()
{
     gfxMovie.Unload();
    isActive_ = false;
    isInit_ = false;
	BackPackToInv = 0.0f;
	BackPackFromInv = 0.0f;
    return true;
}


void HUDVault::Update()
{
  if(OnBackpackToInventorySuccess)
  {
    if ((r3dGetTime() - BackPackToInv) > 1.5)
	{
        obj_Player* plr = gClientLogic().localPlayer_;
	    r3d_assert(plr);
		/*char* g_ServerApiKey = "a5gfd4u8df1jhjs47ws86F";
		CWOBackendReq req("api_GetInventoryData.aspx");
		req.AddSessionInfo(plr->CustomerID, Session);
		req.AddParam("skey1",  g_ServerApiKey);
		req.AddParam("Data", 2);
		req.AddParam("Inventory", LastInventory+1);//(int)m_inventoryID);
		req.AddParam("CustomerID", plr->CustomerID);
		req.AddParam("CharID", 0);
		req.AddParam("Slot", -1);
		req.AddParam("ItemID", ItemID);
		req.AddParam("Quantity", m_Amount);
		req.AddParam("Var1", Var1);
		req.AddParam("Var2", Var2);
		const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(ItemID);
		if (wc)
		req.AddParam("Category",  wc->category);
		else
		req.AddParam("Category",  0);

		if(!req.Issue())
		{
			r3dOutToLog("GetInventoryData FAILED, code: %d\n", req.resultCode_);
			return;
		}*/
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
		char* g_ServerApiKey = "a5gfd4u8df1jhjs47ws86F";
		CWOBackendReq req("api_GetInventoryData.aspx");
		req.AddSessionInfo(plr->CustomerID, Session);
		req.AddParam("skey1",  g_ServerApiKey);
		req.AddParam("Data", 1);
		req.AddParam("Inventory", (int)m_inventoryID);
		req.AddParam("CustomerID", plr->CustomerID);
		req.AddParam("CharID", 0);
		req.AddParam("Slot", m_gridTo);
		req.AddParam("ItemID", 0);
		req.AddParam("Quantity", m_Amount);
		req.AddParam("Var1", 0);
		req.AddParam("Var2", 0);
		req.AddParam("Category",  0);
		//req.AddParam("Durability",  0);

		if(!req.Issue())
		{
			r3dOutToLog("GetInventoryData FAILED, code: %d\n", req.resultCode_);
			return;
		}	
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


void HUDVault::Draw()
{
     gfxMovie.UpdateAndDraw();
}


void HUDVault::Deactivate()
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
}


void HUDVault::Activate()
{
    prevKeyboardCaptureMovie = gfxMovie.SetKeyboardCapture(); // for mouse scroll events
    Session = gUserProfile.SessionID;
    r3d_assert(!isActive_);
    r3dMouse::Show();
    isActive_ = true;
	BackPackToInv = 0.0f;
	OnBackpackToInventorySuccess = false;
    addClientSurvivor(gClientLogic().localPlayer_->CurLoadout);
    updateInventoryAndSkillItems();
    updateSurvivorTotalWeight();
    reloadBackpackInfo();
    gfxMovie.Invoke("_root.api.showInventoryScreen", "");

    Scaleform::GFx::Value var[1];
    var[0].SetString("menu_open");
    gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);
}
void HUDVault::reloadBackpackInfo()
{
    // reset backpack
    {
        gfxMovie.Invoke("_root.api.clearBackpack", "");
        gfxMovie.Invoke("_root.api.clearBackpacks", "");
    }


    std::vector<uint32_t> uniqueBackpacks; // to filter identical backpack
    int backpackSlotIDInc = 0;
    // add backpack content info
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
