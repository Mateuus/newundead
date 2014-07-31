#pragma once

#include "APIScaleformGfx.h"
#include "UIItemInventory.h"

#include "multiplayer/P2PMessages.h"
#include "FrontEndShared.h"
#include "../GameCode/UserProfile.h"


#include "UIAsync.h"

class obj_Player;
class HUDTrade
{
public:
	HUDTrade();
	~HUDTrade();

	bool Init();
	bool Unload();

	void setOppositePlayerName(const char* var1);
	void setUserTradeIndicator(bool var1);
	void enableUserTradeIndicator(bool var1);
	void showOppositeTrade();
	void showUserTrade();
	void showBackpack();
	void addBackpackItems(const wiCharDataFull& slot);
	void addClientSurvivor(const wiCharDataFull& slot);
	bool IsInited() const { return isInit_; }
	void updateSurvivorTotalWeight(const wiCharDataFull& slot);
	void updateInventoryAndSkillItems();

	void reloadBackpackInfo();
	void Update();
	void Draw();

	int SlotItems[72];
	bool OpTradeIndicator;
	bool isAccept;
	float EnableButton;
	bool isAcceptButton;
	obj_Player* plr;
	wiInventoryItem Items[72];
	void setOppositeTradeIndicator(bool var1);
	void eventBackpackToTrade(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventTradeToBackpack(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	bool isActive() const { return isActive_; }
	void Activate();
	void Deactivate();
	void eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
    void ClearInventory();
	void eventTradeDecline(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventTradeAccept(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void OnNetPacket(const PKT_C2S_TradeBacktoOp_s& n);
	void OnNetPacket(const PKT_C2S_TradeOptoBack_s& n);
private:
	UIAsync<HUDTrade> async_;
	r3dScaleformMovie gfxMovie_;
	r3dScaleformMovie* prevKeyboardCaptureMovie_;

	bool isActive_;
	bool isInit_;
	UIItemInventory itemInventory_;
};