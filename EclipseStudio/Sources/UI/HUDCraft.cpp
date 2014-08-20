#include "r3dPCH.h"
#include "r3d.h"

#include "r3dDebug.h"

#include "HUDCraft.h"

#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/ai/AI_Player.h"

#include "../multiplayer/MasterServerLogic.h"
#include "../multiplayer/ClientGameLogic.h"

//extern const wchar_t* getReputationString(int Reputation);
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
void HUDCraft::SendData(int slotid,int slotid2,int itemid,int slotidq,int slotid2q,int slotid3,int slotid3q,int slotid4,int slotid4q,int slotid5,int slotid5q)
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
	n1.slotid5 = slotid5;
	n1.slotid5q = slotid5q;
	p2pSendToHost(gClientLogic().localPlayer_, &n1, sizeof(n1));
	Deactivate();
}

void HUDCraft::eventCraftItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
     r3d_assert(argCount == 1);

    int recipesId = args[0].GetUInt();

	obj_Player* plr = gClientLogic().localPlayer_;
    wiCharDataFull& slot = plr->CurLoadout;

	//Recetas
	//101349 - Create Craft of fraternity
	//20211	 - Homemade Gas Mask
	//101380 - Block_Wall_Wood
	//101402 - Scissors Fight
	//101263 - Cataplasma
	//101393 - Police baton abusive
	//101313 - Spiked club
	//101317 - Item_Barricade_WoodShield

	int Item1=0;
	int Item2=0;
	int Item3=0;

	switch (recipesId)
	{
	case 101349: // Create Craft of fraternity
				for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
				{
					if (slot.Items[i].itemID == 301319 && Item1==0)
						Item1=i;
					if (slot.Items[i].itemID == 101267 && Item2==0)
						Item2=i;
					if (slot.Items[i].itemID == 101344 && Item3==0)
						Item3=i;
				}
				if (Item1 != 0 && Item2 != 0 && Item3 != 0)
				{
					SendData(Item1,Item2,recipesId,1,1,Item3,1,99999,99999,99999,99999);
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 20211: // Crafted Gas Mask
				for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
				{
					if (slot.Items[i].itemID == 301331 && Item1==0)
							Item1=i;
					if (slot.Items[i].itemID == 301339 && Item2==0)
							Item2=i;
					if (slot.Items[i].itemID == 301320 && Item3==0)
							Item3=i;
				}
				if (Item1 != 0 && Item2 != 0 && Item3 != 0)
				{
					SendData(Item1,Item2,recipesId,1,1,Item3,1,99999,99999,99999,99999);
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;	
	case 101380: // Block_Wall_Wood
				if (slot.Stats.Wood>=25)
				{
					SendData(99999,99999,recipesId,1,1,99999,1,99999,99999,99999,99999);
					slot.Stats.Wood-=25;
					Wood=25;
					PKT_S2C_UpdateSlotsCraft_s n;
					n.Wood=slot.Stats.Wood;
					n.Stone=slot.Stats.Stone;
					n.Metal=slot.Stats.Metal;
					p2pSendToHost(plr, &n, sizeof(n));
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101402: // Scissors Fight
				for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
				{
					if (slot.Items[i].itemID == 301335 && Item1==0)
						Item1=i;
					if (slot.Items[i].itemID == 301319 && Item2==0)
						Item2=i;
				}
				if (Item1 != 0 && Item2 != 0)
				{
					SendData(Item1,Item2,recipesId,1,1,99999,99999,99999,99999,99999,99999);
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101263: // Cataplasma
				for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
				{
					if (slot.Items[i].itemID == 301319 && Item1==0)
						Item1=i;
					if (slot.Items[i].itemID == 301366 && Item2==0)
						Item2=i;
					if (slot.Items[i].itemID == 301331 && Item3==0)
						Item3=i;
				}
				if (Item1 != 0 && Item2 != 0 && Item3 != 0)
				{
					SendData(Item1,Item2,recipesId,1,1,Item3,1,99999,99999,99999,99999);
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101393: // Police baton abusive
				for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
				{
					if (slot.Items[i].itemID == 301332 && Item1==0)
						Item1=i;
					if (slot.Items[i].itemID == 101389 && Item2==0)
						Item2=i;

				}
				if (Item1 != 0 && Item2 != 0)
				{
					SendData(Item1,Item2,recipesId,1,1,99999,99999,99999,99999,99999,99999);
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101313: // Spiked club
				for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
				{
					if (slot.Items[i].itemID == 101278 && Item1==0)
						Item1=i;
					if (slot.Items[i].itemID == 301328 && Item2==0)
						Item2=i;
				}
				if (Item1 != 0 && Item2 != 0)
				{
					SendData(Item1,Item2,recipesId,1,1,99999,99999,99999,99999,99999,99999);
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101317: // Item_Barricade_WoodShield
				for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
				{
					if (slot.Items[i].itemID == 301328 && Item1==0)
						Item1=i;
					if (slot.Items[i].itemID == 101392 && Item2==0)
						Item2=i;
					if (slot.Items[i].itemID == 101345 && Item3==0)
						Item3=i;
				}
				if (Item1 != 0 && Item2 != 0 && Item3 != 0)
				{
					SendData(Item1,Item2,recipesId,1,1,Item3,1,99999,99999,99999,99999);
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101379: // Block_Wall_Metal
				if (slot.Stats.Metal>=25)
				{
					SendData(99999,99999,recipesId,1,1,99999,1,99999,99999,99999,99999);
					slot.Stats.Metal-=25;
					Metal=25;
					PKT_S2C_UpdateSlotsCraft_s n;
					n.Wood=slot.Stats.Wood;
					n.Stone=slot.Stats.Stone;
					n.Metal=slot.Stats.Metal;
					p2pSendToHost(plr, &n, sizeof(n));
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101377: // Block_Wall_Brick 
				if (slot.Stats.Stone>=25)
				{
					SendData(99999,99999,recipesId,1,1,99999,1,99999,99999,99999,99999);
					slot.Stats.Stone-=25;
					Stone=25;
					PKT_S2C_UpdateSlotsCraft_s n;
					n.Wood=slot.Stats.Wood;
					n.Stone=slot.Stats.Stone;
					n.Metal=slot.Stats.Metal;
					p2pSendToHost(plr, &n, sizeof(n));
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101378: // Block_Bric big 
				if (slot.Stats.Stone>=25)
				{
					SendData(99999,99999,recipesId,1,1,99999,1,99999,99999,99999,99999);
					slot.Stats.Stone-=25;
					Stone=25;
					PKT_S2C_UpdateSlotsCraft_s n;
					n.Wood=slot.Stats.Wood;
					n.Stone=slot.Stats.Stone;
					n.Metal=slot.Stats.Metal;
					p2pSendToHost(plr, &n, sizeof(n));
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	case 101371: // Block_Door 
				if (slot.Stats.Wood>=25)
				{
					SendData(99999,99999,recipesId,1,1,99999,1,99999,99999,99999,99999);
					slot.Stats.Wood-=25;
					Wood=25;
					PKT_S2C_UpdateSlotsCraft_s n;
					n.Wood=slot.Stats.Wood;
					n.Stone=slot.Stats.Stone;
					n.Metal=slot.Stats.Metal;
					p2pSendToHost(plr, &n, sizeof(n));
					r3dOutToLog("Recipient: %d Created\n",recipesId);
				}
				break;
	default:
		r3dOutToLog("Recipient: %d Not Found\n",recipesId);
	}
}
void HUDCraft::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	Deactivate();
}

bool HUDCraft::Init()
{
	Wood=25;
	Stone=25;
	Metal=25;
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
	obj_Player* plr = gClientLogic().localPlayer_;
    wiCharDataFull& slot = plr->CurLoadout;

	if (Wood>0 && Wood<=25)
	{
		Wood=25-slot.Stats.Wood;
	}
	else {
		Wood=0;
	}
	if (Stone>0 && Stone<=25)
	{
		Stone=25-slot.Stats.Stone;
	}
	else {
		Stone=0;
	}
	
	if (Metal>0 && Metal<=25)
	{
		Metal=25-slot.Stats.Metal;
	}
	else {
		Metal=0;
	}

	gfxMovie_.Invoke("_root.api.clearRecipes", "");
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101402);
    var[1].SetString("$CR_Chank");
    var[2].SetString("$CR_ChankInfo");
    var[3].SetString("$Data/Weapons/StoreIcons/Mel_Shiv.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101263);
    var[1].SetString("$CR_Bandage");
    var[2].SetString("$CR_BandageInfo");
    var[3].SetString("$Data/Weapons/StoreIcons/Item_Bandage_Improvised.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101349);
    var[1].SetString("$CR_CanoePaddleKnife");
    var[2].SetString("$CR_CanoePaddleKnifeInfo");
    var[3].SetString("$Data/Weapons/StoreIcons/MEL_CanoePaddle_Knife.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101380);
    var[1].SetString("$101380_name");
    var[2].SetString("$101380_desc");
    var[3].SetString("$Data/Weapons/StoreIcons/Block_Wall_Wood_2M_01_Crate.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101393);
    var[1].SetString("$CR_PoliceBatonWire");
    var[2].SetString("$CR_PoliceBatonWireInfo");
    var[3].SetString("$Data/Weapons/StoreIcons/Mel_PoliceBaton_Wire.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101313);
    var[1].SetString("$CR_SpikedBat");
    var[2].SetString("$CR_SpikedBatInfo");
    var[3].SetString("$Data/Weapons/StoreIcons/MEL_BaseballBat_Spikes_01.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(20211);
    var[1].SetString("$CR_GasMask");
    var[2].SetString("$CR_GasMaskInfo");
    var[3].SetString("$Data/Weapons/StoreIcons/HGear_GasMask_Improvised_01.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101317);
    var[1].SetString("$101317_name");
    var[2].SetString("$101317_desc");
    var[3].SetString("$Data/Weapons/StoreIcons/Item_Barricade_WoodShield.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101379);
    var[1].SetString("$101379_name");
    var[2].SetString("$101379_desc");
    var[3].SetString("$Data/Weapons/StoreIcons/Block_Wall_Metal_2M_01_Crate.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}

	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101377);
    var[1].SetString("$101377_name");
    var[2].SetString("$101377_desc");
    var[3].SetString("$Data/Weapons/StoreIcons/Block_Wall_Brick_Short_01_Crate.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}

	{
	if (Stone<0)
		Stone=0;
	else if (Stone>25)
		Stone=25;
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101377);
    var1[1].SetUInt(301387);
    var1[2].SetUInt(Stone);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}	
/////////////////////
	{
	if (Metal<0)
		Metal=0;
	else if (Metal>25)
		Metal=25;
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101379);
    var1[1].SetUInt(301386);
    var1[2].SetUInt(Metal);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}

/////////////////////
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101371);
    var[1].SetString("$101371_name");
    var[2].SetString("$101371_desc");
    var[3].SetString("$Data/Weapons/StoreIcons/Block_Door_Wood_2M_01_Crate.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	if (Wood<0)
		Wood=0;
	else if (Wood>25)
		Wood=25;
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101371);
    var1[1].SetUInt(301388);
    var1[2].SetUInt(Wood);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	
	{
	Scaleform::GFx::Value var[4];
	var[0].SetUInt(101378);
    var[1].SetString("$101378_name");
    var[2].SetString("$101378_desc");
    var[3].SetString("$Data/Weapons/StoreIcons/Block_Wall_Brick_Tall_01_Crate.dds");
	gfxMovie_.Invoke("_root.api.addRecipe", var, 4);
	}
	{
	if (Stone<0)
		Stone=0;
	else if (Stone>25)
		Stone=25;
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101378);
    var1[1].SetUInt(301387);
    var1[2].SetUInt(Stone);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}	
									// Brother Hood (Canoe Knifes)
	{
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101349);
    var1[1].SetUInt(101344);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}

	{
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101349);
    var1[1].SetUInt(101267);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
									// Brother Hood Part 2
	{
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101349);
    var1[1].SetUInt(301319);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	                               // Crafted Gas Mask
	{
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(20211);
    var1[1].SetUInt(301320);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}

	{
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(20211);
    var1[1].SetUInt(301339);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(20211);
    var1[1].SetUInt(301331);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	                              // Bada Bum
	{
	if (Wood<0)
		Wood=0;
	else if (Wood>25)
		Wood=25;
	Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101380);
    var1[1].SetUInt(301388);
    var1[2].SetUInt(Wood);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}

	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301388);
		var[1].SetNumber(50);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Resource_Wood.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301386);
		var[1].SetNumber(50);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Resource_Metal.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(301387);
		var[1].SetNumber(50);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Resource_Stone.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
				// Shank
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101402);
    var1[1].SetUInt(301335);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
				Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101402);
    var1[1].SetUInt(301319);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
		// Bandage Crafted
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101263);
    var1[1].SetUInt(301331);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101263);
    var1[1].SetUInt(301366);
    var1[2].SetUInt(1); //default 1
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101263);
    var1[1].SetUInt(301319);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	// Silencer Crafted
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101317);
    var1[1].SetUInt(101345);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101317);
    var1[1].SetUInt(101392);
    var1[2].SetUInt(1); //default 1
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101317);
    var1[1].SetUInt(301328);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
		// Police Baton Wire
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101393);
    var1[1].SetUInt(101389);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
				Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101393);
    var1[1].SetUInt(301332);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
		// Spiked Bat
	{
		Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101313);
    var1[1].SetUInt(301328);
    var1[2].SetUInt(1);
	gfxMovie_.Invoke("_root.api.addRecipeComponent", var1, 3);
	}
	{
				Scaleform::GFx::Value var1[3];
	var1[0].SetUInt(101313);
    var1[1].SetUInt(101278);
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
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(301332);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_RazerWire_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(101389);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Mel_PoliceBaton.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(101344);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/MEL_Canoe_paddle.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(101267);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/MEL_Knife_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(101278);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/MEL_BaseballBat_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(301327);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_MetalScrap_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(301324);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_GunPowder_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(301318);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_EmptyCan_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(101380);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Block_Wall_Wood_2M_01_Crate.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(20211);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/HGear_GasMask_Improvised_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(301320);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_EmptyBottle_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(301357);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Charcoal_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(301331);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Rag_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	var[0].SetUInt(301366);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Ointment_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
		var[0].SetUInt(101317);
		var[1].SetNumber(28);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Item_Barricade_WoodShield.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(101392);
		var[1].SetNumber(25);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/Mel_NailGun.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		gfxMovie_.Invoke("_root.api.addItem", var, 9);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(101345);
		var[1].SetNumber(29);
		var[2].SetString("");
		var[3].SetString("");
		var[4].SetString("$Data/Weapons/StoreIcons/MEL_Cricket_bat.dds");
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
	addRecipes();
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