#pragma once

#include "APIScaleformGfx.h"
#include "UIItemInventory.h"

#include "multiplayer/P2PMessages.h"
#include "FrontEndShared.h"
#include "../GameCode/UserProfile.h"
#include "UIMarket.h"

class HUDGeneralStore
{
public:
	HUDGeneralStore();
	~HUDGeneralStore();

	bool Init();
	bool Unload();

	bool IsInited() const { return isInit_; }

	void Update();
	void Draw();

	bool isActive() const { return isActive_; }
	void Activate();
	void Deactivate();

	void OnBuyItemSuccess();

	static unsigned int WINAPI as_BuyItemThread(void* in_data);
	void eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	int	storeBuyPrice_;
	int	storeBuyPriceGD_;
	int ItemID;
	int Price;

private:
	r3dScaleformMovie gfxMovie_;
	r3dScaleformMovie* prevKeyboardCaptureMovie_;
	UIAsync<HUDGeneralStore> async_;

	bool isActive_;
	bool isInit_;

	UIMarket market_;
};