#include "r3dPCH.h"
#include "r3d.h"

#include "r3dDebug.h"

#include "HUDCraft.h"

#include "FrontendShared.h"

#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/ai/AI_Player.h"


#include "../multiplayer/MasterServerLogic.h"
#include "../multiplayer/ClientGameLogic.h"

extern const char* getReputationString(int Reputation);

HUDCraft::HUDCraft() :
isActive_(false),
isInit_(false),
prevKeyboardCaptureMovie_(NULL)
{
}

HUDCraft::~HUDCraft()
{
}
void HUDCraft::SendData(int slotid,int slotid2,int itemid,int slotidq,int slotid2q,int slotid3,int slotid3q,int slotid4,int slotid4q)
{
	PKT_C2C_PlayerCraftItem_s n1;
	n1.itemid = itemid;
	n1.slotid1 = slotid;
	n1.slotid2 = slotid2;
	n1.slotid1q = slotidq;
	n1.slotid2q = slotid2q;
	n1.slotid3 = slotid3;
	n1.slotid3q = slotid3q;
	n1.slotid4 = slotid4;
	n1.slotid4q = slotid4q;
	p2pSendToHost(gClientLogic().localPlayer_, &n1, sizeof(n1));
	Deactivate();
}
void HUDCraft::FindComp4(int slotid,int slotid2,int slotid3,int itemid)
{
}
void HUDCraft::FindComp3(int slotid,int slotid2,int itemid)
{
	r3dOutToLog("K3 Run\n");
	r3dOutToLog("K3 %d \n",itemid);
	for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
	{
		obj_Player* plr = gClientLogic().localPlayer_;
		wiCharDataFull& slot = plr->CurLoadout;
		if (itemid == 101022) // AK-74M
		{
			if (slot.Items[i].itemID == 301335) // Sci
			{
				SendData(slotid,slotid2,itemid,1,1,i,1,99999,99999);
				r3dOutToLog("OK3\n");
			}
		}
	}
}
void HUDCraft::FindComp2(int slotid,int itemid)
{
	for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
	{
		obj_Player* plr = gClientLogic().localPlayer_;
		wiCharDataFull& slot = plr->CurLoadout;
		if (itemid == 101267) // Tafucking Knife
		{
			if (slot.Items[i].itemID == 301370) // Rope
			{
				SendData(slotid,i,itemid,1,1,99999,99999,99999,99999);
				return;
			}
		}
		if (itemid == 101262) // band dx
		{
			if (slot.Items[i].itemID == 301319) // fuck tape
			{
				SendData(slotid,i,itemid,1,1,99999,99999,99999,99999);
				return;
			}
		}
		if (itemid == 101022) // AK-74M
		{
			if (slot.Items[i].itemID == 301339) // shit thread
			{
				FindComp3(slotid,i,101022); // Find Comp 3
				r3dOutToLog("OK2\n");
				return;
			}
		}
	}
}
void HUDCraft::eventCraftItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 1);

	int recipesId = args[0].GetUInt();
	r3dOutToLog("%d\n",recipesId);
	for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
	{
		obj_Player* plr = gClientLogic().localPlayer_;
		wiCharDataFull& slot = plr->CurLoadout;
		if (recipesId == 101022) // AK-74M
		{
			if (slot.Items[i].itemID == 301328) // Nails
			{
				FindComp2(i,101022);
				r3dOutToLog("OK\n");
				return;
			}
		}
		if (recipesId == 101267) // Tafucknical Knife
		{



			if (slot.Items[i].itemID == 301335) // Sci
			{
				FindComp2(i,101267);
				return;
			}
		}
		if (recipesId == 101262) // Bandage DX
		{

			if (slot.Items[i].itemID == 101261) // Bandage
			{
				FindComp2(i,101262);
				return;
			}
		}
	}

}
void HUDCraft::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	Deactivate();
}

bool HUDCraft::Init()
{
	if(!gfxMovie_.Load("Data\\Menu\\WarZ_HUD_CraftingMenu.swf", false)) 
	{
		return false;
	}

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDCraft>(this, &HUDCraft::FUNC)
	gfxMovie_.RegisterEventHandler("eventReturnToGame", MAKE_CALLBACK(eventReturnToGame));
	gfxMovie_.RegisterEventHandler("eventCraftItem", MAKE_CALLBACK(eventCraftItem));

	gfxMovie_.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);

	isActive_ = false;
	isInit_ = true;
	return true;
}

bool HUDCraft::Unload()
{
	gfxMovie_.Unload();
	isActive_ = false;
	isInit_ = false;
	return true;
}

void HUDCraft::Update()
{

}

void HUDCraft::Draw()
{
	gfxMovie_.UpdateAndDraw();
}

void HUDCraft::Deactivate()
{
	gfxMovie_.Invoke("_root.api.hideCraftScreen", 0);

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_close");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);

	if(prevKeyboardCaptureMovie_)
	{
		//	prevKeyboardCaptureMovie_->SetKeyboardCapture();
		//	prevKeyboardCaptureMovie_ = NULL;
	}

	if(!g_cursor_mode->GetInt())
	{
		r3dMouse::Hide();
	}

	isActive_ = false;
}
void HUDCraft::refreshRecipes()
{
	gfxMovie_.Invoke("_root.api.Main.refreshRecipeList","");
}
void HUDCraft::reloadBackpackInfo()
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
void HUDCraft::addBackpackItems(const wiCharDataFull& slot)
{
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
			gfxMovie_.Invoke("_root.api.addBackpackItem", var, 7);
		}
	}
}
void HUDCraft::addClientSurvivor(const wiCharDataFull& slot)
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


	gfxMovie_.Invoke("_root.api.clearBackpack", NULL, 0);


	addBackpackItems(slot);
}
void HUDCraft::addRecipes()
{
	gfxMovie_.Invoke("_root.api.clearRecipes", "");
	{
		Scaleform::GFx::Value var[4];
		var[0].SetUInt(101267);
		var[1].SetString("Tactical Knife");
		var[2].SetString("A standard issue knife with an fixed blade, this knife was first designed in 1985 for the United States Army and was later adapted by several other countries as well as mercenary groups throughout the world due to its sturdy design and stopping power.");
		var[3].SetString("$Data/Weapons/StoreIcons/MEL_Knife_01.dds");
		gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
		Scaleform::GFx::Value var[4];
		var[0].SetUInt(101262);
		var[1].SetString("Bandage DX");
		var[2].SetString("Duck Tape & Bandage");
		var[3].SetString("$Data/Weapons/StoreIcons/Item_bandage_02.dds");
		gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	/*		{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101027);
	var[1].SetString("AK-74M ELITE");
	var[2].SetString("Extreme Weapons");
	var[3].SetString("$Data/Weapons/StoreIcons/ASR_AK74M_Elite.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}*/
	{
		Scaleform::GFx::Value var[4];
		var[0].SetUInt(101022);
		var[1].SetString("AK-74M");
		var[2].SetString("");
		var[3].SetString("$Data/Weapons/StoreIcons/ASR_AK74M.dds");
		gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	// AK-74M
	{
		Scaleform::GFx::Value var1[3];
		var1[0].SetUInt(101022);
		var1[1].SetUInt(301339);
		var1[2].SetUInt(1);
		gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}

	{
		Scaleform::GFx::Value var1[3];
		var1[0].SetUInt(101022);
		var1[1].SetUInt(301335);
		var1[2].SetUInt(1);
		gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	// AK-74M
	{
		Scaleform::GFx::Value var1[3];
		var1[0].SetUInt(101022);
		var1[1].SetUInt(301328);
		var1[2].SetUInt(1);
		gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	// AK-74M
	{
		Scaleform::GFx::Value var1[3];
		var1[0].SetUInt(101027);
		var1[1].SetUInt(101022);
		var1[2].SetUInt(1);
		gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	// Knife
	{
		Scaleform::GFx::Value var1[3];
		var1[0].SetUInt(101267);
		var1[1].SetUInt(301335);
		var1[2].SetUInt(1);
		gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
		Scaleform::GFx::Value var1[3];
		var1[0].SetUInt(101267);
		var1[1].SetUInt(301370);
		var1[2].SetUInt(1);
		gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	// Bandage DX
	{
		Scaleform::GFx::Value var1[3];
		var1[0].SetUInt(101262);
		var1[1].SetUInt(101261);
		var1[2].SetUInt(1);
		gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
		Scaleform::GFx::Value var1[3];
		var1[0].SetUInt(101262);
		var1[1].SetUInt(301319);
		var1[2].SetUInt(1);
		gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}

	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301365);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_MetalPipe_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301319);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_DuckTape_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(101261);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Item_bandage_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301335);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Scissors_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	/*	{
	Scaleform::GFx::Value var[9];
	var[0].SetUInt(101022);
	var[1].SetNumber(28);
	var[2].SetString("");
	var[3].SetString("");
	var[4].SetString("$Data/Weapons/StoreIcons/ASR_AK74M.dds");
	var[5].SetBoolean(false);					// is Stackable
	var[6].SetNumber(0);						// weight
	var[7].SetInt(0);
	var[8].SetNumber(0);
	gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}*/
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301370);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Rope_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301339);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_thread_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301328);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Nails_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}

}

void HUDCraft::updateInventoryAndSkillItems()
{
	Scaleform::GFx::Value var[7];
	// clear inventory DB
	gfxMovie_.Invoke("_root.api.clearInventory", NULL, 0);

	// add all items
	for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
	{
		var[0].SetUInt(uint32_t(gUserProfile.ProfileData.Inventory[i].InventoryID));
		var[1].SetUInt(gUserProfile.ProfileData.Inventory[i].itemID);
		var[2].SetNumber(gUserProfile.ProfileData.Inventory[i].quantity);
		var[3].SetNumber(gUserProfile.ProfileData.Inventory[i].Var1);
		var[4].SetNumber(gUserProfile.ProfileData.Inventory[i].Var2);
		bool isConsumable = false;
		{
			const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(gUserProfile.ProfileData.Inventory[i].itemID);
			if(wc && wc->category == storecat_UsableItem && wc->m_isConsumable)
				isConsumable = true;
		}
		var[5].SetBoolean(isConsumable);
		char tmpStr[128] = {0};
		getAdditionalDescForItem(gUserProfile.ProfileData.Inventory[i].itemID, gUserProfile.ProfileData.Inventory[i].Var1, gUserProfile.ProfileData.Inventory[i].Var2, tmpStr);
		var[6].SetString(tmpStr);
		gfxMovie_.Invoke("_root.api.addInventoryItem", var, 7);
	}
}
void HUDCraft::updateSurvivorTotalWeight(const wiCharDataFull& slot)
{
	float totalWeight = slot.getTotalWeight();


	Scaleform::GFx::Value var[2];
	var[0].SetString(slot.Gamertag);
	var[1].SetNumber(totalWeight);
	gfxMovie_.Invoke("_root.api.updateClientSurvivorWeight", var, 2);
}
void HUDCraft::Activate()
{
	//addRecipes();
	//prevKeyboardCaptureMovie_ = gfxMovie_.SetKeyboardCapture(); // for mouse scroll events
	addClientSurvivor(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);
	updateInventoryAndSkillItems();
	updateSurvivorTotalWeight(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);


	reloadBackpackInfo();

	r3d_assert(!isActive_);
	r3dMouse::Show();
	isActive_ = true;

	gfxMovie_.Invoke("_root.api.showCraftScreen", 0);

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_open");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);
}