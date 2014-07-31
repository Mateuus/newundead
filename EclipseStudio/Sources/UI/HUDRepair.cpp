#include "r3dPCH.h"
#include "r3d.h"

#include "r3dDebug.h"

#include "HUDRepair.h"

#include "FrontendShared.h"

#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/ai/AI_Player.h"


#include "../multiplayer/MasterServerLogic.h"
#include "../multiplayer/ClientGameLogic.h"

extern const char* getReputationString(int Reputation);

HUDRepair::HUDRepair() :
isActive_(false),
isInit_(false),
prevKeyboardCaptureMovie_(NULL)
{
}

HUDRepair::~HUDRepair()
{
}

bool HUDRepair::Init()
{
	if(!gfxMovie_.Load("Data\\Menu\\WarZ_HUD_Repair.swf", false)) 
	{
		return false;
	}

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDRepair>(this, &HUDRepair::FUNC)
    gfxMovie_.RegisterEventHandler("eventReturnToGame", MAKE_CALLBACK(eventReturnToGame));
	// repairall , repairiteminslot

	gfxMovie_.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);
	itemInventory_.initialize(&gfxMovie_);

	isActive_ = false;
	isInit_ = true;
	return true;
}

bool HUDRepair::Unload()
{
	gfxMovie_.Unload();
	isActive_ = false;
	isInit_ = false;
	return true;
}

void HUDRepair::Update()
{
   
}

void HUDRepair::Draw()
{
	gfxMovie_.UpdateAndDraw();
}

void HUDRepair::Deactivate()
{
	gfxMovie_.Invoke("_root.api.hideRepairScreen", 0);

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_close");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);

	if(!g_cursor_mode->GetInt())
	{
		r3dMouse::Hide();
	}

	isActive_ = false;
}

void HUDRepair::Activate()
{
	r3d_assert(!isActive_);
	r3dMouse::Show();
	isActive_ = true;

	ClearBackpack();
	addClientSurvivor(gClientLogic().localPlayer_->CurLoadout);
	reloadBackpackInfo();
	updateSurvivorTotalWeight(gClientLogic().localPlayer_->CurLoadout);
	addBackpackItemRepairInfo(gClientLogic().localPlayer_->CurLoadout);
	setGD();

	gfxMovie_.Invoke("_root.api.showRepairScreen", 0);

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_open");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);
}

void HUDRepair::updateSurvivorTotalWeight(const wiCharDataFull& slot)
{
	float totalWeight = slot.getTotalWeight();


	Scaleform::GFx::Value var[2];
	var[0].SetString(slot.Gamertag);
	var[1].SetNumber(totalWeight);
	gfxMovie_.Invoke("_root.api.updateClientSurvivorWeight", var, 2);
}

void HUDRepair::ClearBackpack()
{
	gfxMovie_.Invoke("_root.api.clearBackpack", 0);
    gfxMovie_.Invoke("_root.api.clearBackpacks", 0);
}

void HUDRepair::addClientSurvivor(const wiCharDataFull& slot)
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
	var[19].SetString(getReputationString(slot.Stats.Reputation));    // alignment
	var[20].SetString("COLORADO");    // last Map
	var[21].SetBoolean(slot.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox);


	gfxMovie_.Invoke("_root.api.addClientSurvivor", var, 22);


	gfxMovie_.Invoke("_root.api.clearBackpack", 0);


	addBackpackItems(slot);
}

void HUDRepair::addBackpackItems(const wiCharDataFull& slot)
{
	Scaleform::GFx::Value var[7];
	//r3dOutToLog("AddBackPackItems\n");
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
			gfxMovie_.Invoke("_root.api.addBackpackItem", var, 7);
		}
	}
}

bool HUDRepair::storecat_isWeapons(const BaseItemConfig* itemCfg)
{
	int cat = itemCfg->category;
	
	return (cat >= storecat_ASR && cat <= storecat_SMG);
}

void HUDRepair::addBackpackItemRepairInfo(const wiCharDataFull& slot)
{
	Scaleform::GFx::Value var[3];
	for (int a = 0; a < slot.BackpackSize; a++)
	{
		if (slot.Items[a].InventoryID != 0 && slot.Items[a].itemID > 0)
		{
             const BaseItemConfig* itemCfg = g_pWeaponArmory->getConfig(slot.Items[a].itemID);
			 if (itemCfg)
			 {
				 if (storecat_isWeapons(itemCfg))
				 {
                     var[0].SetUInt(a);
					 // for debug gfx
					 // tested. works!
					 var[1].SetUInt(55);
					 var[2].SetUInt(55*50);
                     gfxMovie_.Invoke("_root.api.addBackpackItemRepairInfo", var, 3);
				 }
			 }
		}
	}
}

void HUDRepair::reloadBackpackInfo()
{
	// reset backpack
	{
		gfxMovie_.Invoke("_root.api.clearBackpack", "");
		gfxMovie_.Invoke("_root.api.clearBackpacks", "");
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
				gfxMovie_.Invoke("_root.api.addBackpackItem", var, 7);


				const BackpackConfig* bpc = g_pWeaponArmory->getBackpackConfig(slot.Items[a].itemID);
				if(bpc)
				{
					if(std::find<std::vector<uint32_t>::iterator, uint32_t>(uniqueBackpacks.begin(), uniqueBackpacks.end(), slot.Items[a].itemID) != uniqueBackpacks.end())
						continue;


					// add backpack info
					var[0].SetInt(backpackSlotIDInc++);
					var[1].SetUInt(slot.Items[a].itemID);
					gfxMovie_.Invoke("_root.api.addBackpack", var, 2);


					uniqueBackpacks.push_back(slot.Items[a].itemID);
				}
			}
		}
	}


	gfxMovie_.Invoke("_root.api.Main.Inventory.showBackpack", "");
}

void HUDRepair::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	Deactivate();
}

void HUDRepair::setGD()
{
	gfxMovie_.Invoke("_root.api.Main.setGD", gUserProfile.ProfileData.GameDollars);
}