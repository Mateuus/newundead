#pragma once

#include "APIScaleformGfx.h"
#include "UIItemInventory.h"


#include "FrontEndShared.h"
#include "../GameCode/UserProfile.h"


#include "UIAsync.h"


class HUDCraft
{
public:
	HUDCraft();
	~HUDCraft();

	bool Init();
	bool Unload();

	bool IsInited() const { return isInit_; }

	void Update();
	void Draw();

	bool isActive() const { return isActive_; }
	void Activate();
	void Deactivate();
	void addRecipes();
	void refreshRecipes();
	    void addCategories();
    void addTabTypes();
    void addItems();
	//void addRecipesComp();
void addClientSurvivor(const wiCharDataFull& slot);
void addBackpackItems(const wiCharDataFull& slot);
void reloadBackpackInfo();
void updateInventoryAndSkillItems();
void updateSurvivorTotalWeight(const wiCharDataFull& slot);
void FindComp2(int slotid,int itemid);
void SendData(int slotid,int slotid2,int itemid,int slotidq,int slotid2q,int slotid3,int slotid3q,int slotid4,int slotid4q);
void FindComp4(int slotid,int slotid2,int slotid3,int itemid);
void FindComp3(int slotid,int slotid2,int itemid);
	void eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventCraftItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	//void eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

private:
	 UIAsync<HUDCraft> async_;
	r3dScaleformMovie gfxMovie_;
	r3dScaleformMovie* prevKeyboardCaptureMovie_;

	bool isActive_;
	bool isInit_;
};