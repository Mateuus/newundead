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
#include "HUDGeneralStore.h"
#include "../multiplayer/ClientGameLogic.h"

HUDGeneralStore::HUDGeneralStore() :
isActive_(false),
isInit_(false),
prevKeyboardCaptureMovie_(NULL)
{
}

HUDGeneralStore::~HUDGeneralStore()
{
}

void HUDGeneralStore::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	Deactivate();
}

unsigned int WINAPI HUDGeneralStore::as_BuyItemThread(void* in_data)
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	HUDGeneralStore* This = (HUDGeneralStore*)in_data;

	This->async_.DelayServerRequest();
    
	gClientLogic().SendBuyItemReq(This->ItemID);
	float endWait = r3dGetTime() + 30;
	
	while(r3dGetTime() < endWait)
	{
       if (gClientLogic().isBuyAns)
	   {
		   if (gClientLogic().BuyAnsCode != 0)
		   {
              This->async_.SetAsyncError(0,gLangMngr.getString("BuyItemFail"));
			  return 1;
		   }
		   return 0;
	   }
	}

	This->async_.SetAsyncError(0,gLangMngr.getString("BuyItemFail"));
	return 1;
}

void HUDGeneralStore::OnBuyItemSuccess()
{
	gfxMovie_.Invoke("_root.api.hideInfoMsg", 0);

   	if(storeBuyPrice_ > 0)
		gUserProfile.ProfileData.GamePoints -= storeBuyPrice_;

	if(storeBuyPriceGD_ > 0)
	    gUserProfile.ProfileData.GameDollars -= storeBuyPriceGD_;

	market_.setCurrency();
}

void HUDGeneralStore::eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	//market_.eventBuyItem(pMovie, args, argCount);
	//PKT_C2S_BuyItemReq_s n;
	//p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
    ItemID = args[0].GetInt();

	storeBuyPrice_ = args[1].GetInt();
	storeBuyPriceGD_ = args[2].GetInt();

	Scaleform::GFx::Value var[2];
	var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
	var[1].SetBoolean(false);
	gfxMovie_.Invoke("_root.api.showInfoMsg", var, 2);

	async_.StartAsyncOperation(this, &HUDGeneralStore::as_BuyItemThread, &HUDGeneralStore::OnBuyItemSuccess);
}

bool HUDGeneralStore::Init()
{
 	if(!gfxMovie_.Load("Data\\Menu\\WarZ_HUD_GeneralStore.swf", false)) 
	{
 		return false;
	}

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDGeneralStore>(this, &HUDGeneralStore::FUNC)
 	gfxMovie_.RegisterEventHandler("eventReturnToGame", MAKE_CALLBACK(eventReturnToGame));
	gfxMovie_.RegisterEventHandler("eventBuyItem", MAKE_CALLBACK(eventBuyItem));

	gfxMovie_.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);

	market_.initialize(&gfxMovie_);

	isActive_ = false;
	isInit_ = true;
	return true;
}

bool HUDGeneralStore::Unload()
{
 	gfxMovie_.Unload();
	isActive_ = false;
	isInit_ = false;
	return true;
}

void HUDGeneralStore::Update()
{
	async_.ProcessAsyncOperation(this, gfxMovie_);
	market_.update();
}

void HUDGeneralStore::Draw()
{
 	gfxMovie_.UpdateAndDraw();
}

void HUDGeneralStore::Deactivate()
{
	if (market_.processing() || async_.Processing())
	{
		return;
	}

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_close");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);

	if(prevKeyboardCaptureMovie_)
	{
		prevKeyboardCaptureMovie_->SetKeyboardCapture();
		prevKeyboardCaptureMovie_ = NULL;
	}

	if(!g_cursor_mode->GetInt())
	{
		r3dMouse::Hide();
	}

	isActive_ = false;
}

void HUDGeneralStore::Activate()
{
	//prevKeyboardCaptureMovie_ = gfxMovie_.SetKeyboardCapture(); // for mouse scroll events

	r3d_assert(!isActive_);
	r3dMouse::Show();
	isActive_ = true;

	gfxMovie_.Invoke("_root.api.showMarketplace", 0);

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_open");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);
}