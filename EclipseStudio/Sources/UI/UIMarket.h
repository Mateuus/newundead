#pragma once

#include "APIScaleformGfx.h"

#include "UIItemInventory.h"
#include "UIAsync.h"

class UIMarket
{
public:
	UIMarket();

	void initialize(r3dScaleformMovie* gfxMovie);

	void eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

	void update();
	void setCurrency();
	bool processing();

private:
	void addTabTypes();

	void addItems();
	void addCategories();
	void addStore();
	void addAllItemsToStore();
	bool itemIsFiltered(uint32_t itemId);

	static unsigned int WINAPI as_BuyItemThread(void* in_data);
	void OnBuyItemSuccess();

	bool UpdateInventoryWithBoughtItem();
	void updateDefaultAttachments(bool isNewItem, uint32_t itemID);
	int	StoreDetectBuyIdx();

private:
	r3dScaleformMovie* gfxMovie_;
	UIItemInventory itemInventory_;
	UIAsync<UIMarket> async_;

	uint32_t storeBuyItemID_;
	int	storeBuyPrice_;
	int	storeBuyPriceGD_;
	__int64	inventoryID_;
};