#pragma once

#include "APIScaleformGfx.h"
#include "UIItemInventory.h"

#include "multiplayer/P2PMessages.h"
#include "FrontEndShared.h"
#include "../GameCode/UserProfile.h"


#include "UIAsync.h"

class obj_Player;
class HUDRepair
{
public:
	HUDRepair();
	~HUDRepair();

	bool isActive() const { return isActive_; }
	void Activate();
	void Deactivate();
	bool Init();
	bool Unload();
	void setGD();
	void Update();
	void Draw();
	void ClearBackpack();
	void addBackpackItems(const wiCharDataFull& slot);
	void reloadBackpackInfo();
	void eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void updateSurvivorTotalWeight(const wiCharDataFull& slot);
	void addClientSurvivor(const wiCharDataFull& slot);
	bool IsInited() const { return isInit_; }
	void addBackpackItemRepairInfo(const wiCharDataFull& slot);
	bool storecat_isWeapons(const BaseItemConfig* itemCfg);

private:
	UIAsync<HUDRepair> async_;
	r3dScaleformMovie gfxMovie_;
	r3dScaleformMovie* prevKeyboardCaptureMovie_;

	bool isActive_;
	bool isInit_;
	UIItemInventory itemInventory_;
};