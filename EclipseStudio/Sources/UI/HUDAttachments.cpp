#include "r3dPCH.h"
#include "r3d.h"

#include "../../../Eternity/sf/Console/config.h"
#include "HUDPause.h"
#include "HUDAttachments.h"
#include "LangMngr.h"

#include "FrontendShared.h"

#include "../multiplayer/clientgamelogic.h"
#include "../ObjectsCode/AI/AI_Player.H"
#include "../ObjectsCode/weapons/Weapon.h"
#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../GameLevel.h"

HUDAttachments::HUDAttachments()
{
	isActive_ = false;
	isInit = false;
}

HUDAttachments::~HUDAttachments()
{
}

void HUDAttachments::eventSelectAttachment(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 3);
	int id = args[0].GetInt();
	int slotID = args[1].GetInt();
	int attmID = args[2].GetInt();

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);
	wiCharDataFull& slot = plr->CurLoadout;

	const WeaponConfig* wc = plr->m_Weapons[plr->m_SelectedWeapon]->getConfig();

	// check if attm is default attm
	{
		if((wc->FPSDefaultID[id] == attmID || attmID==0) && id!=WPN_ATTM_CLIP)
		{
			plr->RemoveWpnAttm(plr->m_SelectedWeapon, id);

			Scaleform::GFx::Value var[2];
			var[0].SetInt(id);
			var[1].SetInt(slotID);
			gfxMovie.Invoke("_root.api.setSlotActive", var, 2);
			return;
		}
	}

	// find this attm in backpack
	for(int i=0; i<slot.BackpackSize; ++i)
	{
		if(slot.Items[i].itemID == attmID)
		{
			plr->EquipWpnAttm(plr->m_SelectedWeapon, i);

			const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(attmID);
			Scaleform::GFx::Value var[2];
			var[0].SetInt(wac->m_type);
			var[1].SetInt(slotID);
			gfxMovie.Invoke("_root.api.setSlotActive", var, 2);

			// need to close attachment screen, otherwise reload animation don't work by some reasons
			if(wac->m_type == WPN_ATTM_CLIP)
				Deactivate();

			return;
		}
	}

	r3dOutToLog("!!! eventSelectAttachment: unknown attm??\n");
}

bool HUDAttachments::Init()
{
	if(!gfxMovie.Load("Data\\Menu\\WarZ_HUD_AttachUI.swf", false)) 
		return false;

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDAttachments>(this, &HUDAttachments::FUNC)
	gfxMovie.RegisterEventHandler("eventSelectAttachment", MAKE_CALLBACK(eventSelectAttachment));

	gfxMovie.SetCurentRTViewport( Scaleform::GFx::Movie::SM_ExactFit );

	isActive_ = false;
	isInit = true;
	return true;
}

bool HUDAttachments::Unload()
{
	gfxMovie.Unload();
	isActive_ = false;
	isInit = false;
	return true;
}

void HUDAttachments::Update()
{
}

void HUDAttachments::Draw()
{
	gfxMovie.UpdateAndDraw();
}

void HUDAttachments::Deactivate()
{
	if( !g_cursor_mode->GetInt() )
	{
		r3dMouse::Hide();
	}

	{
		Scaleform::GFx::Value var[1];
		var[0].SetString("menu_close");
		gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);
	}


	isActive_ = false;

	ResetItemId();
}

void HUDAttachments::ResetItemId()
{
	for (int i =0;i<72;i++)
	{
		ItemIdAdded[i] = -1;
	}
}

void HUDAttachments::AddItemIdAdded(int ItemId)
{
	for (int i =0;i<72;i++)
	{
		if (ItemIdAdded[i] == ItemId)
			return;
	}

	for (int i =0;i<72;i++)
	{
		if (ItemIdAdded[i] == -1)
		{
			ItemIdAdded[i] = ItemId;
			return;
		}
	}
}

bool HUDAttachments::isItemAdded(int ItemId)
{
	for (int i =0;i<72;i++)
	{
		if	(ItemIdAdded[i] == ItemId && ItemId > 0)
			return true;
	}
	return false;
}

void HUDAttachments::Activate()
{
	r3d_assert(!isActive_);

	ResetItemId();

	// check if user has weapon in his current slot
	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);

	wiCharDataFull& slot = plr->CurLoadout;

	if(plr->m_SelectedWeapon !=0 && plr->m_SelectedWeapon!=1) // attm only for primary and secondary slots
		return;

	extern HUDPause*	hudPause;
	if(hudPause->isActive())
		return;

	wiInventoryItem& inv = slot.Items[plr->m_SelectedWeapon];
	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(inv.itemID);
	if(!wc)
	{
		return; // no weapon - do not activate this screen
	}
	if(wc->category == storecat_MELEE)
		return;

	if(g_camera_mode->GetInt() == 0) // switch player to FPS mode
	{
		plr->switchFPS_TPS();
	}

	r3dMouse::Show();
	isActive_ = true;

	{
		Scaleform::GFx::Value var[1];
		var[0].SetString("menu_open");
		gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);
	}

	gfxMovie.Invoke("_root.api.clearAttachments", "");

	const r3dPoint2D attmSlotPos[WPN_ATTM_MAX] = {
		r3dPoint2D(r3dRenderer->ScreenW*0.05f, r3dRenderer->ScreenH*0.05f), // WPN_ATTM_MUZZLE
		r3dPoint2D(r3dRenderer->ScreenW*0.5f, r3dRenderer->ScreenH*0.1f), // WPN_ATTM_UPPER_RAIL
		r3dPoint2D(r3dRenderer->ScreenW*0.15f, r3dRenderer->ScreenH*0.35f), // WPN_ATTM_LEFT_RAIL
		r3dPoint2D(r3dRenderer->ScreenW*0.3f, r3dRenderer->ScreenH*0.8f), // WPN_ATTM_BOTTOM_RAIL
		r3dPoint2D(r3dRenderer->ScreenW*0.7f, r3dRenderer->ScreenH*0.7f), // WPN_ATTM_CLIP
		r3dPoint2D(r3dRenderer->ScreenW*0.0f, r3dRenderer->ScreenH*0.0f), // WPN_ATTM_RECEIVER
		r3dPoint2D(r3dRenderer->ScreenW*0.0f, r3dRenderer->ScreenH*0.0f), // WPN_ATTM_STOCK
		r3dPoint2D(r3dRenderer->ScreenW*0.0f, r3dRenderer->ScreenH*0.0f)  // WPN_ATTM_BARREL
	};

	// add attachment slots
	Scaleform::GFx::Value var[4];
	for(int attmType=0; attmType<WPN_ATTM_RECEIVER; ++attmType)
	{
		int slotInc = 0;
		var[0].SetInt(attmType);
		var[1].SetInt(int(attmSlotPos[attmType].x));
		var[2].SetInt(int(attmSlotPos[attmType].y));
		var[3].SetInt(attmType);
		gfxMovie.Invoke("_root.api.addAttachment", var, 4);

		const WeaponAttachmentConfig* wac = NULL;
		// check for default attachment, if any
		const char* attmName = "NONE";
		int attmID = 0;

		bool defaultSlotActive = true;

		// add using item at top.
		if (attmType == WPN_ATTM_CLIP)
		{
			wac = g_pWeaponArmory->getAttachmentConfig(inv.Var2);
			if(wac)
			{
				attmName = wac->m_StoreName;

				var[1].SetInt(slotInc++);
				var[2].SetString(attmName);
				var[3].SetInt(inv.Var2);
				gfxMovie.Invoke("_root.api.addSlot", var, 4);

				AddItemIdAdded(inv.Var2);
			}
		}
		else
		{
			if(wc->FPSDefaultID[attmType]!=0)
			{
				wac = g_pWeaponArmory->getAttachmentConfig(wc->FPSDefaultID[attmType]);
				if(wac)
				{
					attmName = wac->m_StoreName;
					attmID = wc->FPSDefaultID[attmType];
				}
			}
			var[1].SetInt(slotInc++);
			var[2].SetString(attmName);
			var[3].SetInt(attmID);
			gfxMovie.Invoke("_root.api.addSlot", var, 4);

			if (attmID > 0) AddItemIdAdded(attmID);
		}

		// go through backpack and look for attachments that fit this weapon
		for (int a = 0; a < slot.BackpackSize; a++)
		{
			if (slot.Items[a].itemID != 0 && (wac=g_pWeaponArmory->getAttachmentConfig(slot.Items[a].itemID))!=NULL)
			{
				if(/*wac->m_itemID != wc->FPSDefaultID[attmType] &&*/ wac->m_type == attmType)
				{
					if(wac->m_specID == wc->FPSSpecID[attmType] && wc->FPSSpecID[attmType] > 0 && !isItemAdded(wac->m_itemID)) // specID equal zero means nothing goes in it except for default attm
					{
						AddItemIdAdded(wac->m_itemID);

						var[1].SetInt(slotInc++);
						var[2].SetString(wac->m_StoreName);
						var[3].SetInt(wac->m_itemID);
						gfxMovie.Invoke("_root.api.addSlot", var, 4);
					}

					if(slot.Attachment[plr->m_SelectedWeapon].attachments[attmType] == wac->m_itemID && attmType != WPN_ATTM_CLIP)
					{
						var[1].SetInt(slotInc-1);
						gfxMovie.Invoke("_root.api.setSlotActive", var, 2);
						defaultSlotActive = false;
					}
				}
			}
		}

		if(defaultSlotActive)
		{
			var[1].SetInt(0);
			gfxMovie.Invoke("_root.api.setSlotActive", var, 2);
		}

	}

}
