#include "r3dPCH.h"
#include "r3d.h"
#include "r3dDebug.h"

#include "UIMarket.h"

#include "APIScaleformGfx.h"

#include "LangMngr.h"
#include "GameCode/UserProfile.h"
#include "ObjectsCode/weapons/WeaponArmory.h"
#include "FrontEndShared.h"
#include "multiplayer/MasterServerLogic.h"

UIMarket::UIMarket() :
gfxMovie_(NULL)
{
}

void UIMarket::initialize(r3dScaleformMovie* gfxMovie)
{
	r3d_assert(gfxMovie);

	gfxMovie_ = gfxMovie;

	setCurrency();
	itemInventory_.initialize(gfxMovie_);
	addStore();
}

void UIMarket::setCurrency()
{
	Scaleform::GFx::Value var[1];

	var[0].SetInt(gUserProfile.ProfileData.GamePoints);
	gfxMovie_->Invoke("_root.api.setGC", var, 1);

	var[0].SetInt(gUserProfile.ProfileData.GameDollars);
	gfxMovie_->Invoke("_root.api.setDollars", var, 1);

	var[0].SetInt(0);
	gfxMovie_->Invoke("_root.api.setCells", var, 1);
}

static void addAllItemsToStore()
{
	// reset store and add all items from DB
	g_NumStoreItems = 0;

	#define SET_STOREITEM \
		memset(&st, 0, sizeof(st)); \
		st.itemID = item->m_itemID;\
		st.pricePerm = 60000;\
		st.gd_pricePerm = 0;

	std::vector<const ModelItemConfig*> allItems;
	std::vector<const WeaponConfig*> allWpns;
	std::vector<const GearConfig*> allGear;
	
	g_pWeaponArmory->startItemSearch();
	while(g_pWeaponArmory->searchNextItem())
	{
		uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
		const WeaponConfig* weaponConfig = g_pWeaponArmory->getWeaponConfig(itemID);
		if(weaponConfig)
		{
			allWpns.push_back(weaponConfig);
		}

		const ModelItemConfig* itemConfig = g_pWeaponArmory->getItemConfig(itemID);
		if(itemConfig)
		{
			allItems.push_back(itemConfig);
		}

		const GearConfig* gearConfig = g_pWeaponArmory->getGearConfig(itemID);
		if(gearConfig)
		{
			allGear.push_back(gearConfig);
		}
	}

	const int	itemSize = allItems.size();
	const int	weaponSize = allWpns.size();
	const int	gearSize = allGear.size();

	for(int i=0; i< itemSize; ++i)
	{
		const ModelItemConfig* item = allItems[i];
		wiStoreItem& st = g_StoreItems[g_NumStoreItems++];
		SET_STOREITEM;
	}

	for(int i=0; i< weaponSize; ++i)
	{
		const WeaponConfig* item = allWpns[i];
		wiStoreItem& st = g_StoreItems[g_NumStoreItems++];
		SET_STOREITEM;
	}

	for(int i=0; i< gearSize; ++i)
	{
		const GearConfig* item = allGear[i];
		wiStoreItem& st = g_StoreItems[g_NumStoreItems++];
		SET_STOREITEM;
	}
	
	#undef SET_STOREITEM
}

bool UIMarket::itemIsFiltered(uint32_t itemId)
{
	// clan items
	if(itemId >= 301151 && itemId <= 301157) // clan items
		return true;
	return false;
}

void UIMarket::addStore()
{
#if 0
	// add all items to store for test purpose
	addAllItemsToStore();
#endif	

	Scaleform::GFx::Value var[10];
	for(uint32_t i=0; i<g_NumStoreItems; ++i)
	{
		if(itemIsFiltered(g_StoreItems[i].itemID))
			continue;

		int quantity = 1;
		{
			const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(g_StoreItems[i].itemID);
			if(wc)
				quantity = wc->m_ShopStackSize;
			const FoodConfig* fc = g_pWeaponArmory->getFoodConfig(g_StoreItems[i].itemID);
			if(fc)
				quantity = fc->m_ShopStackSize;
		}
	
		var[0].SetUInt(g_StoreItems[i].itemID);
		var[1].SetNumber(g_StoreItems[i].pricePerm);
		var[2].SetNumber(g_StoreItems[i].gd_pricePerm);
		var[3].SetNumber(quantity);		// quantity
		var[4].SetBoolean(g_StoreItems[i].isNew);

		gfxMovie_->Invoke("_root.api.addStoreItem", var, 5);
	}
}

void UIMarket::eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	storeBuyItemID_ = args[0].GetUInt(); // legsID
	storeBuyPrice_ = args[1].GetInt();
	storeBuyPriceGD_ = args[2].GetInt();

	if(gUserProfile.ProfileData.GameDollars < storeBuyPriceGD_ || gUserProfile.ProfileData.GamePoints < storeBuyPrice_)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetStringW(gLangMngr.getString("NotEnougMoneyToBuyItem"));
		var[1].SetBoolean(true);
		gfxMovie_->Invoke("_root.api.showInfoMsg", var, 2);
		return;
	}

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie_->Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &UIMarket::as_BuyItemThread, &UIMarket::OnBuyItemSuccess);
}

unsigned int WINAPI UIMarket::as_BuyItemThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	UIMarket* This = (UIMarket*)in_data;

	This->async_.DelayServerRequest();
	
	int buyIdx = This->StoreDetectBuyIdx();
	if(buyIdx == 0)
	{
		This->async_.SetAsyncError(-1, gLangMngr.getString("BuyItemFailNoIndex"));
		return 0;
	}

	int apiCode = gUserProfile.ApiBuyItem(This->storeBuyItemID_, buyIdx, &This->inventoryID_);
	if(apiCode != 0)
	{
		This->async_.SetAsyncError(apiCode, gLangMngr.getString("BuyItemFail"));
		return 0;
	}

	return 1;
}

void UIMarket::OnBuyItemSuccess()
{
	//
	// TODO: get inventory ID stored in ApiBuyItem, search in inventory.
	// if found - increate quantity by 1, if not - add new item with that ID
	//
	bool isNewItem = !UpdateInventoryWithBoughtItem();
	setCurrency();
	gfxMovie_->Invoke("_root.api.buyItemSuccessful", "");
	gfxMovie_->Invoke("_root.api.hideInfoMsg", "");
	return;
}

bool UIMarket::UpdateInventoryWithBoughtItem()
{
	int numItem = gUserProfile.ProfileData.NumItems;

	// check if we bought consumable
	int quantityToAdd = 1;
	int totalQuantity = 1;

	// see if we already have this item in inventory
	bool found = false;
	uint32_t inventoryID = 0;
	int	var1 = -1;
	int	var2 = 0;

	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(storeBuyItemID_);
	const GearConfig* gc = g_pWeaponArmory->getGearConfig(storeBuyItemID_);
	const FoodConfig* fc = g_pWeaponArmory->getFoodConfig(storeBuyItemID_);

	// todo: store should report how much items we bought...
	if (wc)
		quantityToAdd = wc->m_ShopStackSize;
	if (fc)
		quantityToAdd = fc->m_ShopStackSize;

	for( int i=0; i<numItem; ++i)
	{
		if(gUserProfile.ProfileData.Inventory[i].InventoryID == inventoryID_)
		{
			inventoryID = uint32_t(gUserProfile.ProfileData.Inventory[i].InventoryID);
			var1 = gUserProfile.ProfileData.Inventory[i].Var1;
			var2 = gUserProfile.ProfileData.Inventory[i].Var2;

			gUserProfile.ProfileData.Inventory[i].quantity += quantityToAdd;
			totalQuantity = gUserProfile.ProfileData.Inventory[i].quantity;

			found = true;
			break; 
		}
	}

	if(!found)
	{
		wiInventoryItem& itm = gUserProfile.ProfileData.Inventory[gUserProfile.ProfileData.NumItems++];
		itm.InventoryID = inventoryID_;
		itm.itemID     = storeBuyItemID_;
		itm.quantity   = quantityToAdd;
		itm.Var1   = var1;
		itm.Var2   = var2;
		
		inventoryID = uint32_t(itm.InventoryID);

		totalQuantity = quantityToAdd;
	}

	Scaleform::GFx::Value var[7];
	var[0].SetUInt(inventoryID);
	var[1].SetUInt(storeBuyItemID_);
	var[2].SetNumber(totalQuantity);
	var[3].SetNumber(var1);
	var[4].SetNumber(var2);
	var[5].SetBoolean(false);
	char tmpStr[128] = {0};
	getAdditionalDescForItem(storeBuyItemID_, var1, var2, tmpStr);
	var[6].SetString(tmpStr);
	gfxMovie_->Invoke("_root.api.addInventoryItem", var, 7);

	updateDefaultAttachments(!found, storeBuyItemID_);
	return found;
}

void UIMarket::updateDefaultAttachments(bool isNewItem, uint32_t itemID)
{
	// add default attachments
/*	const WeaponConfig* wpn = g_pWeaponArmory->getWeaponConfig(itemID);
	if(wpn)
	{
		if(isNewItem)
		{
			for(int i=0; i<WPN_ATTM_MAX; ++i)
			{
				if(wpn->FPSDefaultID[i]>0)
				{
					gUserProfile.ProfileData.FPSAttachments[gUserProfile.ProfileData.NumFPSAttachments++] = wiUserProfile::temp_fps_attach(itemID, wpn->FPSDefaultID[i], mStore_buyItemExp*1440, 1);
					const WeaponAttachmentConfig* attm = gWeaponArmory.getAttachmentConfig(wpn->FPSDefaultID[i]);
					Scaleform::GFx::Value var[3];
					var[0].SetNumber(itemID);
					var[1].SetNumber(attm->m_type);
					var[2].SetNumber(attm->m_itemID);
					gfxMovie.Invoke("_root.api.setAttachmentSpec", var, 3);
				}
			}
		}
		else
		{
			for(int i=0; i<WPN_ATTM_MAX; ++i)
			{
				if(wpn->FPSDefaultID[i]>0)
				{
					for(uint32_t j=0; j<gUserProfile.ProfileData.NumFPSAttachments; ++j)
					{
						if(gUserProfile.ProfileData.FPSAttachments[j].WeaponID == itemID && gUserProfile.ProfileData.FPSAttachments[j].AttachmentID == wpn->FPSDefaultID[i])
						{
							gUserProfile.ProfileData.FPSAttachments[j].expiration += mStore_buyItemExp*1440;
						}
					}
				}
			}
		}
	}
*/
}

int UIMarket::StoreDetectBuyIdx()
{
	// 4 for GamePoints (CASH)
	// 8 for GameDollars (in-game)
	int buyIdx = 0;
	if(storeBuyPrice_ > 0)
	{
		buyIdx = 4;
	}
	else if(storeBuyPriceGD_ > 0)
	{
		buyIdx = 8;
	}
	return buyIdx;
}

void UIMarket::update()
{
	async_.ProcessAsyncOperation(this, *gfxMovie_);
}

bool UIMarket::processing()
{
	return async_.Processing();
}