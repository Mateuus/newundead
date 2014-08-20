#include "r3dPCH.h"
#include "r3d.h"

#include "UIItemInventory.h"

#include "APIScaleformGfx.h"

#include "ObjectsCode/weapons/WeaponArmory.h"

UIItemInventory::UIItemInventory() :
gfxMovie_(NULL)
{
}

void UIItemInventory::initialize(r3dScaleformMovie* gfxMovie)
{
	r3d_assert(gfxMovie);

	gfxMovie_ = gfxMovie;

	addTabTypes();
	addItems();
	addCategories();
}

void UIItemInventory::addTabTypes()
{
	//r3dOutToLog("Adding Weapons Tab\n");
	//r3dOutToLog("Added\n");
	Scaleform::GFx::Value var[8];
	var[2].SetNumber(0); 
	var[3].SetNumber(0);
	var[4].SetNumber(0);
	var[5].SetNumber(0);
	var[6].SetNumber(0);
	var[7].SetBoolean(true); // visible in store

	// Market Place

	// store & inventory tabs

	// Adding Weapons Tab

var[0].SetNumber(0);
	var[1].SetString("weapon");
	var[2].SetBoolean(false);
	var[3].SetBoolean(true);
	gfxMovie_->Invoke("_root.api.addTabType", var, 4);

	var[0].SetNumber(1);
	var[1].SetString("ammo");
	var[2].SetBoolean(true);
	var[3].SetBoolean(true);
	gfxMovie_->Invoke("_root.api.addTabType", var, 4);

	var[0].SetNumber(2);
	var[1].SetString("explosives");
	var[2].SetBoolean(true);
	var[3].SetBoolean(true);
	gfxMovie_->Invoke("_root.api.addTabType", var, 4);

	var[0].SetNumber(3);
	var[1].SetString("gear");
	var[2].SetBoolean(true);
	var[3].SetBoolean(true);
	gfxMovie_->Invoke("_root.api.addTabType", var, 4);

	var[0].SetNumber(4);
	var[1].SetString("food");
	var[2].SetBoolean(true);
	var[3].SetBoolean(true);
	gfxMovie_->Invoke("_root.api.addTabType", var, 4);

	var[0].SetNumber(5);
	var[1].SetString("survival");
	var[2].SetBoolean(true);
	var[3].SetBoolean(true);
	gfxMovie_->Invoke("_root.api.addTabType", var, 4);

	var[0].SetNumber(6);
	var[1].SetString("equipment");
	var[2].SetBoolean(true);
	var[3].SetBoolean(true);
	gfxMovie_->Invoke("_root.api.addTabType", var, 4);

	var[0].SetNumber(7);
	var[1].SetString("account");
	var[2].SetBoolean(true);
	var[3].SetBoolean(false);
	gfxMovie_->Invoke("_root.api.addTabType", var, 4);
}

void UIItemInventory::addItems()
{
	Scaleform::GFx::Value var[20];
	std::vector<const WeaponConfig*> allWpns;
	std::vector<const GearConfig*> allGear;
	std::vector<const ModelItemConfig*> allItem;
	std::vector<const HeroConfig*> allHeroes;
	std::vector<const FoodConfig*> allFood;
	std::vector<const CraftArmoryConfig*> allCraftArmory;//Mateuus Craft
	std::vector<const BackpackConfig*> allBackpack;
	std::vector<const WeaponAttachmentConfig*> allAmmo;
	std::vector<const WeaponAttachmentConfig*> allAttachments;

	g_pWeaponArmory->startItemSearch();
	while(g_pWeaponArmory->searchNextItem())
	{
		uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
		const WeaponConfig* config = g_pWeaponArmory->getWeaponConfig(itemID);
		if(config)
		{
			allWpns.push_back(config);
		}
		const GearConfig* gearConfig = g_pWeaponArmory->getGearConfig(itemID);
		if(gearConfig)
		{
			allGear.push_back(gearConfig);
		}
		const ModelItemConfig* itemConfig = g_pWeaponArmory->getItemConfig(itemID);
		if(itemConfig)
		{
			allItem.push_back(itemConfig);
		}
		const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(itemID);
		if(heroConfig)
		{
			allHeroes.push_back(heroConfig);
		}			
		const FoodConfig* foodConfig = g_pWeaponArmory->getFoodConfig(itemID);
		if(foodConfig)
		{
			allFood.push_back(foodConfig);
		}	
		/////////////////////////////////////////////////////////////////////////////////////////
		//Mateuus Craft
		const CraftArmoryConfig* CraftArmoryConfig = g_pWeaponArmory->getCraftArmoryConfig(itemID);
		if(CraftArmoryConfig)
		{
			allCraftArmory.push_back(CraftArmoryConfig);
		}
		/////////////////////////////////////////////////////////////////////////////////////////
		const BackpackConfig* backpackConfig = g_pWeaponArmory->getBackpackConfig(itemID);
		if(backpackConfig)
		{
			allBackpack.push_back(backpackConfig);
		}			
		const WeaponAttachmentConfig* wpnAttmConfig = g_pWeaponArmory->getAttachmentConfig(itemID);
		if(wpnAttmConfig)
		{
			if(wpnAttmConfig->m_type == WPN_ATTM_CLIP)
				allAmmo.push_back(wpnAttmConfig);
			else
				allAttachments.push_back(wpnAttmConfig);
		}			
	}

	const int backpackSize = allBackpack.size ();
	for(int i = 0; i < backpackSize; ++i)
	{
		const BackpackConfig* backpack = allBackpack[i];

		var[0].SetUInt(backpack->m_itemID);
		var[1].SetNumber(backpack->category);
		var[2].SetStringW(backpack->m_StoreNameW);
		var[3].SetStringW(backpack->m_DescriptionW);
		var[4].SetString(backpack->m_StoreIcon);
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(backpack->m_Weight);						// weight
		var[7].SetInt(backpack->m_maxSlots);
		var[8].SetNumber(backpack->m_maxWeight);
		gfxMovie_->Invoke("_root.api.addItem", var, 9);
	}


	const int gearSize = allGear.size ();
	for(int i = 0; i < gearSize; ++i)
	{
		const GearConfig* gear = allGear[i];

		var[0].SetUInt(gear->m_itemID);
		var[1].SetNumber(gear->category);
		var[2].SetStringW(gear->m_StoreNameW);
		var[3].SetStringW(gear->m_DescriptionW);
		var[4].SetString(gear->m_StoreIcon);
		var[5].SetBoolean(false); // is stackable
		var[6].SetNumber(gear->m_Weight);
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
//////////////////////////////////////////////////////////////////////////////
	//Codex Craft
	{ //Craft Item show Inventory Name
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301365);
		var[1].SetNumber(28);
		var[2].SetString("Metal Pipe");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_MetalPipe_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301370);
		var[1].SetNumber(28);
		var[2].SetString("Rope");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Rope_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301319);
		var[1].SetNumber(28);
		var[2].SetString("Duck Tape");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_DuckTape_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301335);
		var[1].SetNumber(28);
		var[2].SetString("Scissors");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Scissors_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301332);
		var[1].SetNumber(28);
		var[2].SetString("Razer Wire");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_RazerWire_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(101389);
		var[1].SetNumber(28);
		var[2].SetString("Police Baton");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Mel_PoliceBaton.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301328);
		var[1].SetNumber(28);
		var[2].SetString("Nails");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Nails_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301339);
		var[1].SetNumber(28);
		var[2].SetString("Thread");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_thread_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301327);
		var[1].SetNumber(28);
		var[2].SetString("Metal Scrap");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_MetalScrap_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301324);
		var[1].SetNumber(28);
		var[2].SetString("Gun Powder");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_GunPowder_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
    {
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301318);
		var[1].SetNumber(28);
		var[2].SetString("Empty Can");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_EmptyCan_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301366);
		var[1].SetNumber(28);
		var[2].SetString("Ointment");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Ointment_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
    {
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301331);
		var[1].SetNumber(28);
		var[2].SetString("Rag");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Rag_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	{
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301357);
		var[1].SetNumber(28);
		var[2].SetString("Charcoal");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_Charcoal_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
    {
		Scaleform::GFx::Value var[9];
	    var[0].SetUInt(301320);
		var[1].SetNumber(28);
		var[2].SetString("Empty Bottle");
		var[3].SetString("Craft Component Item");
		var[4].SetString("$Data/Weapons/StoreIcons/Craft_EmptyBottle_01.dds");
		var[5].SetBoolean(false);					// is Stackable
		var[6].SetNumber(0);						// weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	const int itemSize = allItem.size ();
	for(int i = 0; i < itemSize; ++i)
	{
		const ModelItemConfig* gear = allItem[i];

		var[0].SetUInt(gear->m_itemID);
		var[1].SetNumber(gear->category);
		var[2].SetStringW(gear->m_StoreNameW);
		var[3].SetStringW(gear->m_DescriptionW);
		var[4].SetString(gear->m_StoreIcon);
		var[5].SetBoolean(false); // is stackable
		var[6].SetNumber(gear->m_Weight);
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}

	const int foodSize = allFood.size ();
	for(int i = 0; i < foodSize; ++i)
	{
		const FoodConfig* food = allFood[i];

		var[0].SetUInt(food->m_itemID);
		var[1].SetNumber(food->category);
		var[2].SetStringW(food->m_StoreNameW);
		var[3].SetStringW(food->m_DescriptionW);
		var[4].SetString(food->m_StoreIcon);
		var[5].SetBoolean(false); // is stackable
		var[6].SetNumber(food->m_Weight);
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}

	////////////////////////////////////////////////////////
	//Mateuus Craft
	const int CraftArmorySize = allCraftArmory.size ();
	for(int i = 0; i < CraftArmorySize; ++i)
	{
		const CraftArmoryConfig* CraftArmory = allCraftArmory[i];

		var[0].SetUInt(CraftArmory->m_itemID);
		var[1].SetNumber(CraftArmory->category);
		var[2].SetStringW(CraftArmory->m_StoreNameW);
		var[3].SetStringW(CraftArmory->m_DescriptionW);
		var[4].SetString(CraftArmory->m_StoreIcon);
		var[5].SetBoolean(false); // is stackable
		var[6].SetNumber(CraftArmory->m_Weight);
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
	////////////////////////////////////////////////////////

	const int heroSize = allHeroes.size ();
	for(int i = 0; i < heroSize; ++i)
	{
		const HeroConfig* hero = allHeroes[i];
		if(hero->m_Weight < 0) // not available to players
			continue;

		var[0].SetUInt(hero->m_itemID);
		var[1].SetNumber(hero->category);
		var[2].SetStringW(hero->m_StoreNameW);
		var[3].SetStringW(hero->m_DescriptionW);
		var[4].SetString(hero->m_StoreIcon);
		char tmpStr[256];
		r3dscpy(tmpStr, hero->m_StoreIcon);
		r3dscpy(&tmpStr[strlen(tmpStr)-4], "2.dds");
		var[5].SetString(tmpStr);

		char tmpStr2[256];
		r3dscpy(tmpStr2, hero->m_StoreIcon);
		r3dscpy(&tmpStr2[strlen(tmpStr2)-4], "3.dds");
		var[6].SetString(tmpStr2);

		var[7].SetInt (hero->getNumHeads ());
		var[8].SetInt (hero->getNumBodys ());
		var[9].SetInt (hero->getNumLegs ());
		gfxMovie_->Invoke("_root.api.addHero", var, 10);

		var[0].SetUInt(hero->m_itemID);
		var[1].SetNumber(hero->category);
		var[2].SetStringW(hero->m_StoreNameW);
		var[3].SetStringW(hero->m_DescriptionW);
		var[4].SetString(hero->m_StoreIcon);
		gfxMovie_->Invoke("_root.api.addItem", var, 5);
	}

	const int weaponSize = allWpns.size ();
	for(int i = 0; i < weaponSize; ++i)
	{
		const WeaponConfig* weapon = allWpns[i];

		var[0].SetUInt(weapon->m_itemID);
		var[1].SetNumber(weapon->category);
		var[2].SetStringW(weapon->m_StoreNameW);
		var[3].SetStringW(weapon->m_DescriptionW);
		var[4].SetString(weapon->m_StoreIcon);
		var[5].SetBoolean(weapon->category == storecat_GRENADE); // multi Purchase Item
		var[6].SetNumber(weapon->m_Weight); // weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
		{
				var[0].SetUInt(weapon->m_itemID);
		var[1].SetNumber(weapon->category);
		var[2].SetStringW(weapon->m_StoreNameW);
		var[3].SetStringW(weapon->m_DescriptionW);
		var[4].SetString(weapon->m_StoreIcon);
		var[5].SetBoolean(weapon->category == storecat_GRENADE); // multi Purchase Item
		var[6].SetNumber(weapon->m_Weight); // weight
		var[7].SetInt(0);
		var[8].SetNumber(0);
		}
		gfxMovie_->Invoke("_root.api.addItem", var, 9);
	}
	const int ammoSize = allAmmo.size();
	for(int i = 0; i < ammoSize; ++i)
	{
		const WeaponAttachmentConfig* attm = allAmmo[i];

		var[0].SetUInt(attm->m_itemID);
		var[1].SetNumber(419);
		var[2].SetStringW(attm->m_StoreNameW);
		var[3].SetStringW(attm->m_DescriptionW);
		var[4].SetString(attm->m_StoreIcon);
		var[5].SetBoolean(false);					// multi Purchase Item
		var[6].SetNumber(attm->m_Weight); // weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}

	const int attmSize = allAttachments.size();
	for(int i = 0; i < attmSize; ++i)
	{
		const WeaponAttachmentConfig* attm = allAttachments[i];

		var[0].SetUInt(attm->m_itemID);
		var[1].SetNumber(attm->category);
		var[2].SetStringW(attm->m_StoreNameW);
		var[3].SetStringW(attm->m_DescriptionW);
		var[4].SetString(attm->m_StoreIcon);
		var[5].SetBoolean(false);					// multi Purchase Item
		var[6].SetNumber(attm->m_Weight); // weight
		gfxMovie_->Invoke("_root.api.addItem", var, 7);
	}
}

void UIItemInventory::addCategories()
{
	Scaleform::GFx::Value var[4];

	var[0].SetNumber(storecat_HeroPackage);
	var[1].SetString("storecat_HeroPackage");
	var[2].SetNumber(-1);
	var[3].SetNumber(-1);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_Backpack);
	var[1].SetString("storecat_Backpack");
	var[2].SetNumber(3);
	var[3].SetNumber(-1);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);
	
	var[0].SetNumber(storecat_Armor);
	var[1].SetString("storecat_Armor");
	var[2].SetNumber(3);
	var[3].SetNumber(2);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_Helmet);
	var[1].SetString("storecat_Helmet");
	var[2].SetNumber(3);
	var[3].SetNumber(3);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_ASR);
	var[1].SetString("storecat_ASR");
	var[2].SetNumber(0);
	var[3].SetNumber(0);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_SNP);
	var[1].SetString("storecat_SNP");
	var[2].SetNumber(0);
	var[3].SetNumber(0);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_SHTG);
	var[1].SetString("storecat_SHTG");
	var[2].SetNumber(0);
	var[3].SetNumber(0);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_MG);
	var[1].SetString("storecat_MG");
	var[2].SetNumber(0);
	var[3].SetNumber(0);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_HG);
	var[1].SetString("storecat_HG");
	var[2].SetNumber(0);
	var[3].SetNumber(1);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_SMG);
	var[1].SetString("storecat_SMG");
	var[2].SetNumber(0);
	var[3].SetNumber(0);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_UsableItem);
	var[1].SetString("storecat_UsableItem");
	var[2].SetNumber(5);
	var[3].SetNumber(4);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_GRENADE);
	var[1].SetString("storecat_GRENADE");
	var[2].SetNumber(2);
	var[3].SetNumber(4);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_MELEE);
	var[1].SetString("storecat_MELEE");
	var[2].SetNumber(2);
	var[3].SetNumber(1);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_Food);
	var[1].SetString("storecat_Food");
	var[2].SetNumber(4);
	var[3].SetNumber(-1);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_Water);
	var[1].SetString("storecat_Water");
	var[2].SetNumber(4);
	var[3].SetNumber(-1);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	// special category for ammo
	var[0].SetNumber(419);
	var[1].SetString("ammo");
	var[2].SetNumber(1); 
	var[3].SetNumber(-1);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);

	var[0].SetNumber(storecat_FPSAttachment);
	var[1].SetString("storecat_FPSAttachment");
	var[2].SetNumber(6); 
	var[3].SetNumber(-1);
	gfxMovie_->Invoke("_root.api.addCategory", var, 4);
}