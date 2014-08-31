#include "r3dPCH.h"
#include "r3d.h"

#include "../../../Eternity/sf/Console/config.h"
#include "HUDPause.h"
#include "HUDDisplay.h"
#include "HUDCraft.h"//Codex Craft
#include "HUDAttachments.h"
#include "LangMngr.h"

#include "FrontendShared.h"

#include "../multiplayer/clientgamelogic.h"
#include "../ObjectsCode/AI/AI_Player.H"
#include "../ObjectsCode/weapons/Weapon.h"
#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../GameLevel.h"
#include "../../GameEngine/gameobjects/obj_Vehicle.h"//Codex Carros

void writeGameOptionsFile();

extern HUDAttachments*	hudAttm;
extern HUDDisplay* hudMain;
extern HUDCraft* hudCraft;//Codex Craft

HUDPause::HUDPause()
{
	isActive_ = false;
	isInit = false;
	prevKeyboardCaptureMovie = NULL;
}

HUDPause::~HUDPause()
{
}

void HUDPause::eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 2);
	int gridFrom = args[0].GetInt();
	int gridTo = args[1].GetInt();

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);
	wiCharDataFull& slot = plr->CurLoadout;

	//local logic
	wiInventoryItem& wi1 = slot.Items[gridFrom];
	wiInventoryItem& wi2 = slot.Items[gridTo];

	// if we can stack slots - do it
	extern bool storecat_IsItemStackable(uint32_t ItemID);
	if(wi1.itemID && wi1.itemID == wi2.itemID && storecat_IsItemStackable(wi1.itemID) && wi1.Var1 < 0 && wi2.Var1 < 0)
	{
		wi2.quantity += wi1.quantity;
		wi1.Reset();

		PKT_C2S_BackpackJoin_s n;
		n.SlotFrom = gridFrom;
		n.SlotTo   = gridTo;
		p2pSendToHost(plr, &n, sizeof(n));
	}
	else
	{
		R3D_SWAP(wi1, wi2);

		PKT_C2S_BackpackSwap_s n;
		n.SlotFrom = gridFrom;
		n.SlotTo   = gridTo;
		p2pSendToHost(plr, &n, sizeof(n));
	}

	plr->OnBackpackChanged(gridFrom);
	plr->OnBackpackChanged(gridTo);

	updateSurvivorTotalWeight();

	gfxMovie.Invoke("_root.api.backpackGridSwapSuccess", "");
}

void HUDPause::eventBackpackDrop(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount==1);
	int slotID = args[0].GetInt();

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);

	plr->DropItem(slotID);

	updateSurvivorTotalWeight();
	gfxMovie.Invoke("_root.api.backpackGridSwapSuccess", "");
}
void	HUDPause::eventMissionAccept(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	int id = args[0].GetUInt();
	if (id == 1) // Mission Lets Play
	{
		Deactivate();
		PKT_C2S_PlayerAcceptMission_s n1;
		n1.id = 1;
		p2pSendToHost(gClientLogic().localPlayer_, &n1, sizeof(n1), true);
		hudMain->addMissionInfo("Lets Play");
		hudMain->addMissionObjective(0,"PICK UP ITEM",false,"0/1",true);
		hudMain->addMissionObjective(0,"KILL 5 ZOMBIES",false,"0/5",true);
		hudMain->showMissionInfo(true);
		gClientLogic().localPlayer_->CurLoadout.Mission1 = 1;
	}
}
void	HUDPause::eventMissionRequestList(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	addMission();
	gfxMovie.Invoke("_root.api.Main.Missions.showMissionList", "");
}
void HUDPause::eventBackpackUseItem(int slotID)
{
	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);
	wiInventoryItem& wi = plr->CurLoadout.Items[slotID];
	//r3d_assert(wi.itemID && wi.quantity < 0);

	/*{
	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(wi.itemID);
	if(wc)
	{
	if(wc->category != storecat_UsableItem) // if consumable, it will send use packet
	{
	Scaleform::GFx::Value newargs[2];
	newargs[0].SetInt(slotID);
	int slotTo = 0;
	if(wc->category == storecat_MELEE || wc->category==storecat_HG)
	slotTo = 1;
	else if(wc->category == storecat_GRENADE)
	{
	// find empty quick slot
	if(plr->CurLoadout.Items[2].itemID==0)
	slotTo = 2;
	else if(plr->CurLoadout.Items[3].itemID==0)
	slotTo = 3;
	else if(plr->CurLoadout.Items[4].itemID==0)
	slotTo = 4;
	else if(plr->CurLoadout.Items[5].itemID==0)
	slotTo = 5;
	else
	slotTo = 5;
	}
	newargs[1].SetInt(slotTo);
	eventBackpackGridSwap(pMovie, newargs, 2);
	return; 
	}
	}
	const GearConfig* gc = g_pWeaponArmory->getGearConfig(wi.itemID);
	if(gc)
	{
	Scaleform::GFx::Value newargs[2];
	newargs[0].SetInt(slotID);
	int slotTo = 6;
	if(gc->category == storecat_Helmet)
	slotTo = 7;
	newargs[1].SetInt(slotTo);
	eventBackpackGridSwap(pMovie, newargs, 2);
	return;
	}
	const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(wi.itemID);
	if(bc)
	{
	gfxMovie.Invoke("_root.api.Main.showChangeBackpack", "");
	return;
	}
	const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(wi.itemID);
	if(wac)
	{
	Deactivate();
	hudAttm->Activate();
	return;
	}
	}*/

	// world note
	if(wi.itemID == WeaponConfig::ITEMID_PieceOfPaper)
	{
		if(hudMain->canShowWriteNote())
		{
			Deactivate();
			hudMain->showWriteNote(slotID);
		}
		return;
	}
	else if(wi.itemID == WeaponConfig::ITEMID_Binoculars || wi.itemID == WeaponConfig::ITEMID_RangeFinder || wi.itemID == WeaponConfig::ITEMID_AirHorn)
	{
		return;
	}
	else if(wi.itemID == WeaponConfig::ITEMID_BarbWireBarricade ||
		wi.itemID == WeaponConfig::ITEMID_WoodShieldBarricade ||
		wi.itemID == WeaponConfig::ITEMID_RiotShieldBarricade ||
		wi.itemID == WeaponConfig::ITEMID_SandbagBarricade    ||
		wi.itemID == WeaponConfig::ITEMID_WoodenDoor2M        ||
		wi.itemID == WeaponConfig::ITEMID_BrickWallBlock      ||
		wi.itemID == WeaponConfig::ITEMID_TownBuilding        ||
		wi.itemID == WeaponConfig::ITEMID_WallMetalBlock      ||
		wi.itemID == WeaponConfig::ITEMID_WoodWall2M)
	{
		Scaleform::GFx::Value var[3];
		var[0].SetString("Place barricade into quick slot and use it from there");
		var[1].SetBoolean(true);
		var[2].SetString("");
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
		return;
	}
	else
	{
		if((wi.itemID == WeaponConfig::ITEMID_Bandages || wi.itemID == WeaponConfig::ITEMID_Bandages2 || wi.itemID == WeaponConfig::ITEMID_Antibiotics ||
			wi.itemID == WeaponConfig::ITEMID_Painkillers || wi.itemID == WeaponConfig::ITEMID_Medkit) 
			&& plr->CurLoadout.Health > 99)
		{
			Scaleform::GFx::Value var[3];
			var[0].SetString("Maximum health already");
			var[1].SetBoolean(true);
			var[2].SetString("");
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
			return;
		}

		if((wi.itemID == WeaponConfig::ITEMID_Bandages || wi.itemID == WeaponConfig::ITEMID_Bandages2 || wi.itemID == WeaponConfig::ITEMID_Antibiotics ||
			wi.itemID == WeaponConfig::ITEMID_Painkillers || wi.itemID == WeaponConfig::ITEMID_Medkit || wi.itemID == 101283 || wi.itemID == 101284 || wi.itemID == 101285 ||
			wi.itemID == 101286 || wi.itemID == 101288 || wi.itemID == 101289 || wi.itemID == 101290 || wi.itemID == 101291 || wi.itemID == 101292 || wi.itemID == 101293 || wi.itemID == 101294 || wi.itemID == 101295 || wi.itemID == 101296 || wi.itemID == 101297 || wi.itemID == 101298 || wi.itemID == 101299 || wi.itemID == 101284) && hudMain->Cooldown > 0)
		{
			Scaleform::GFx::Value var[3];
			var[0].SetString("Currenty Cooldown");
			var[1].SetBoolean(true);
			var[2].SetString("");
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
			return;
		}

		if((wi.itemID == WeaponConfig::ITEMID_C01Vaccine || wi.itemID == WeaponConfig::ITEMID_C04Vaccine) 
			&& plr->CurLoadout.Toxic < 1.0f)
		{
			Scaleform::GFx::Value var[3];
			var[0].SetString("no toxic in blood");
			var[1].SetBoolean(true);
			var[2].SetString("");
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
			return;
		}

		if((wi.itemID == WeaponConfig::ITEMID_Bandages || wi.itemID == WeaponConfig::ITEMID_Bandages2 || wi.itemID == WeaponConfig::ITEMID_Antibiotics ||
			wi.itemID == WeaponConfig::ITEMID_Painkillers || wi.itemID == WeaponConfig::ITEMID_Medkit || wi.itemID == 101284))
		{
			hudMain->currentslot = slotID;
			hudMain->Cooldown = r3dGetTime() + 10.0f;
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//Codex Carros

		if(wi.itemID == 101398) // Repair Car System
	    {
		obj_Player* plr = gClientLogic().localPlayer_;
		if (plr->isInVehicle())
		{
			obj_Vehicle* target_Vehicle = plr->ActualVehicle;
								if (target_Vehicle->DamageCar<5.0f)
								{
									if ((target_Vehicle->DamageCar+2.0f)>5)
										target_Vehicle->DamageCar=5.0f;
									else
										target_Vehicle->DamageCar+=2.0f;

									Scaleform::GFx::Value var[3];
									var[0].SetString("$RepairwithExit");
									var[1].SetBoolean(true);
									var[2].SetString("");
									gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
								}
								else {

									Scaleform::GFx::Value var[3];
									var[0].SetString("$NoneedRepairCar2");
									var[1].SetBoolean(true);
									var[2].SetString("");
									gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
									return;
								}
		}
		else {
			Scaleform::GFx::Value var[3];
			var[0].SetString("$NeedYouAreOnVehicle");
			var[1].SetBoolean(true);
			var[2].SetString("");
			gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
			return;
		}
	}
			if(wi.itemID == 301321) // Server Vehicles Gasoline
	{
		obj_Player* plr = gClientLogic().localPlayer_;
		if (plr->isInVehicle())
		{
			obj_Vehicle* target_Vehicle = plr->ActualVehicle;
			if ( target_Vehicle )
			{
				if (target_Vehicle->DamageCar>0)
				{
							if (target_Vehicle->GasolineCar<100)
							{
									float maxGas=25.0f;
									if ((target_Vehicle->GasolineCar+maxGas)>100)
										target_Vehicle->GasolineCar=100.0f;
									else
										target_Vehicle->GasolineCar=target_Vehicle->GasolineCar+maxGas;
									target_Vehicle->ExitVehicle=true;
									Scaleform::GFx::Value var[3];
									var[0].SetString("$GassFullOK");
									var[1].SetBoolean(true);
									var[2].SetString("");
									gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
									//return;
							}
							else {
									Scaleform::GFx::Value var[3];
									var[0].SetString("$GassFULL");
									var[1].SetBoolean(true);
									var[2].SetString("");
									gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
									return;
							}
						}
						else {
									Scaleform::GFx::Value var[3];
									var[0].SetString("$NoGassVehDestroy");
									var[1].SetBoolean(true);
									var[2].SetString("");
									gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
									return;
						}
				}
		}
		else {
			if (plr->isPassenger())
			{
				Scaleform::GFx::Value var[3];
				var[0].SetString("$GassPassenger");
				var[1].SetBoolean(true);
				var[2].SetString("");
				gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
				return;
			}
			else {
				Scaleform::GFx::Value var[3];
				var[0].SetString("$Not_in_Vehicle");
				var[1].SetBoolean(true);
				var[2].SetString("");
				gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
				return;
			}
		}
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		plr->localPlayer_UseItem(slotID, wi.itemID, plr->GetPosition());
	}


	//local logic
	wi.quantity--;
	if(wi.quantity <= 0) {
		wi.Reset();
	}

	plr->OnBackpackChanged(slotID);

	updateSurvivorTotalWeight();

	gfxMovie.Invoke("_root.api.backpackGridSwapSuccess", "");
}

void HUDPause::eventChangeBackpack(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount==2);
	int bpslotID = args[0].GetInt();
	uint32_t itemID = args[1].GetUInt();

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);
	wiCharDataFull& slot = plr->CurLoadout;

	// find inventory slot
	int slotFrom = -1;
	for(int i=0; i<slot.BackpackSize; ++i)
	{
		if(slot.Items[i].itemID == itemID)
		{


			if(slot.Items[i].quantity > 1)
			{
				r3dMouse::Show();


				char Bcbksgs[64];
				sprintf(Bcbksgs, "Failed to change backpack");


				Scaleform::GFx::Value var[3];
				var[0].SetString(Bcbksgs);
				var[1].SetString("Okey");
				var[2].SetString("Warning");
				gfxMovie.Invoke("_root.api.showInfoMsg2", var, 3);
				return;
			}

			slotFrom = i;
			break;
		}
	}
	if(slotFrom == -1)
		return;

	if(plr->ChangeBackpack(slotFrom))
	{
		{
			Scaleform::GFx::Value var[11];
			var[0].SetString(slot.Gamertag);
			var[1].SetNumber(slot.Health);
			var[2].SetNumber(slot.Stats.XP);
			var[3].SetNumber(slot.Stats.TimePlayed);
			var[4].SetNumber(slot.Alive);
			var[5].SetNumber(slot.Hunger);
			var[6].SetNumber(slot.Thirst);
			var[7].SetNumber(slot.Toxic);
			var[8].SetNumber(slot.BackpackID);
			var[9].SetNumber(slot.BackpackSize);
			var[10].SetNumber(slot.Stats.SkillXPPool);
			gfxMovie.Invoke("_root.api.updateClientSurvivor", var, 11);

		}
		reloadBackpackInfo();
		updateSurvivorTotalWeight();
		gfxMovie.Invoke("_root.api.changeBackpackSuccess", "");
		showInventory();
		Deactivate();
		showInventory();
	}
}

void HUDPause::eventMsgBoxCallback(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	// FOR DENIS: send cancel disconnect request to server
	/*
	if(DisconnectAt > 0)
	{
	//PKT_C2S_DisconnectReq_s n;
	//p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);

	DisconnectAt = -1; 
	}*/
}

void HUDPause::eventBackToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	Deactivate();
	gClientLogic().localPlayer_->ShowCrosshair=true;
}

void HUDPause::eventQuitGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	// send disconnect request to server
	PKT_C2S_DisconnectReq_s n;
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);

	if(!gClientLogic().localPlayer_->bDead)
		DisconnectAt = r3dGetTime() + 10.0f;
	else
		DisconnectAt = r3dGetTime();

	//hudMain->disconnectTs3();
}

r3dPoint2D getMinimapPos(const r3dPoint3D& pos)
{
	r3dPoint3D worldOrigin = GameWorld().m_MinimapOrigin;
	r3dPoint3D worldSize = GameWorld().m_MinimapSize;
	float left_corner_x = worldOrigin.x;
	float bottom_corner_y = worldOrigin.z; 
	float x_size = worldSize.x;
	float y_size = worldSize.z;

	float x = R3D_CLAMP((pos.x-left_corner_x)/x_size, 0.0f, 1.0f);
	float y = 1.0f-R3D_CLAMP((pos.z-bottom_corner_y)/y_size, 0.0f, 1.0f);

	return r3dPoint2D(x, y);
}

void HUDPause::eventShowMap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);
	setMinimapPosition(plr->GetPosition(), plr->GetvForw());
}

void HUDPause::setMinimapPosition(const r3dPoint3D& pos, const r3dPoint3D& dir)
{
	r3dPoint2D mapPos = getMinimapPos(pos);

	// calculate rotation around Y axis
	r3dPoint3D d = dir;
	d.y = 0;
	d.Normalize();
	float dot1 = d.Dot(r3dPoint3D(0,0,1)); // north
	float dot2 = d.Dot(r3dPoint3D(1,0,0));
	float deg = acosf(dot1);
	deg = R3D_RAD2DEG(deg);
	if(dot2<0) 
		deg = 360 - deg;
	deg = R3D_CLAMP(deg, 0.0f, 360.0f);	

	Scaleform::GFx::Value var[3];
	var[0].SetNumber(mapPos.x);
	var[1].SetNumber(mapPos.y);
	var[2].SetNumber(deg);
	//gfxMovie.Invoke("_root.api.setPlayerPosition", var, 3);
	gfxMovie.Invoke("_root.api.setPlayerPosition", var, 3);
}

bool HUDPause::Init()
{
	if(!gfxMovie.Load("Data\\Menu\\WarZ_HUD_menu.swf", false)) 
		return false;

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDPause>(this, &HUDPause::FUNC)
	gfxMovie.RegisterEventHandler("eventBackpackGridSwap", MAKE_CALLBACK(eventBackpackGridSwap));
	gfxMovie.RegisterEventHandler("eventBackpackDrop", MAKE_CALLBACK(eventBackpackDrop));
	//	gfxMovie.RegisterEventHandler("eventBackpackUseItem", MAKE_CALLBACK(eventBackpackUseItem)); Not Used
	gfxMovie.RegisterEventHandler("eventChangeBackpack", MAKE_CALLBACK(eventChangeBackpack));
	gfxMovie.RegisterEventHandler("eventMissionRequestList", MAKE_CALLBACK(eventMissionRequestList));
	gfxMovie.RegisterEventHandler("eventMissionAccept", MAKE_CALLBACK(eventMissionAccept));
	gfxMovie.RegisterEventHandler("eventMsgBoxCallback", MAKE_CALLBACK(eventMsgBoxCallback));
	gfxMovie.RegisterEventHandler("eventBackToGame", MAKE_CALLBACK(eventBackToGame));
	gfxMovie.RegisterEventHandler("eventQuitGame", MAKE_CALLBACK(eventQuitGame));
	gfxMovie.RegisterEventHandler("eventShowMap", MAKE_CALLBACK(eventShowMap));
	gfxMovie.RegisterEventHandler("eventOptionsControlsRequestKeyRemap", MAKE_CALLBACK(eventOptionsControlsRequestKeyRemap));
	gfxMovie.RegisterEventHandler("eventOptionsControlsReset", MAKE_CALLBACK(eventOptionsControlsReset));
	gfxMovie.RegisterEventHandler("eventOptionsApply", MAKE_CALLBACK(eventOptionsApply));	
	gfxMovie.RegisterEventHandler("eventShowContextMenuCallback", MAKE_CALLBACK(eventShowContextMenuCallback));
	gfxMovie.RegisterEventHandler("eventSendCallForHelp", MAKE_CALLBACK(eventSendCallForHelp));	
	gfxMovie.RegisterEventHandler("eventContextMenu_Action", MAKE_CALLBACK(eventContextMenu_Action));

	gfxMovie.SetCurentRTViewport( Scaleform::GFx::Movie::SM_ExactFit );

	itemInventory_.initialize(&gfxMovie);

	{
		char sFullPath[512];
		char sFullPathImg[512];
		sprintf(sFullPath, "%s\\%s", r3dGameLevel::GetHomeDir(), "minimap.dds");
		sprintf(sFullPathImg, "$%s", sFullPath); // use '$' char to indicate absolute path

		if(r3dFileExists(sFullPath))
			gfxMovie.Invoke("_root.api.setMapIcon", sFullPathImg);
	}

	for(int i=0; i<r3dInputMappingMngr::KS_NUM; ++i)
	{
		Scaleform::GFx::Value args[2];
		args[0].SetStringW(gLangMngr.getString(InputMappingMngr->getMapName((r3dInputMappingMngr::KeybordShortcuts)i)));
		args[1].SetString(InputMappingMngr->getKeyName((r3dInputMappingMngr::KeybordShortcuts)i));
		gfxMovie.Invoke("_root.api.addKeyboardMapping", args, 2);
	}

	m_waitingForKeyRemap = -1;

	isActive_ = false;
	isInit = true;
	return true;
}

bool HUDPause::Unload()
{
	r_gameplay_blur_strength->SetFloat(0.0f);

	gfxMovie.Unload();
	isActive_ = false;
	isInit = false;
	return true;
}

void HUDPause::eventShowContextMenuCallback(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	uint32_t itemID = args[0].GetUInt();
	int slotID = args[1].GetInt();

	wiInventoryItem& wi = gClientLogic().localPlayer_->CurLoadout.Items[slotID];

	obj_Player* plr = gClientLogic().localPlayer_;

	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(itemID);
	const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(itemID);
	const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(itemID);
	const GearConfig* gc = g_pWeaponArmory->getGearConfig(itemID);
	const FoodConfig* fc = g_pWeaponArmory->getFoodConfig(itemID);

	//warz.events.PauseEvents.eventShowContextMenuCallback(loc2.itemID, this.DraggedItem.slotID);

	if(bc)
	{
		Scaleform::GFx::Value var[2];
		var[0].SetString("$FR_PAUSE_CHANGEBP");
		var[1].SetInt(6);
		gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
	}
	else if(wac){
		Scaleform::GFx::Value var[2];
		var[0].SetString("$FR_PAUSE_ATTACH");
		var[1].SetInt(4);
		gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
		
		var[0].SetString("$FR_PAUSE_COMBINE_CLIP");
		var[1].SetInt(8);
		gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
	}else if(wc)
	{
		if(wc->category >= storecat_ASR && wc->category <= storecat_SMG)
		{
			Scaleform::GFx::Value var[2];
			var[0].SetString("$FR_PAUSE_UNLOAD_WEAPON");
			var[1].SetInt(2);
			gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
		}
		if((wc->category == storecat_MELEE || (wc->category >= storecat_ASR && wc->category <= storecat_GRENADE)))
		{
			if((slotID != wiCharDataFull::CHAR_LOADOUT_WEAPON1 && slotID != wiCharDataFull::CHAR_LOADOUT_WEAPON2) || (slotID >= wiCharDataFull::CHAR_LOADOUT_ITEM1 && slotID <= wiCharDataFull::CHAR_LOADOUT_ITEM4))
			{
				Scaleform::GFx::Value var[2];
				var[0].SetString("$FR_PAUSE_EQUIP");
				var[1].SetInt(3);
				gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
			}
		}else if(wc->category != storecat_Backpack || storecat_CraftCom || storecat_CraftRe/*//Mateus Craft */){
			Scaleform::GFx::Value var[2];
			var[0].SetString("$FR_PAUSE_USE_ITEM");
			var[1].SetInt(5);
			gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
		}
	}else if(gc){
		Scaleform::GFx::Value var[2];
		var[0].SetString("$FR_PAUSE_EQUIP");
		var[1].SetInt(3);
		gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
	}else if(fc){
		Scaleform::GFx::Value var[2];
		var[0].SetString("$FR_PAUSE_USE_ITEM");
		var[1].SetInt(5);
		gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
	}
	Scaleform::GFx::Value var[2];
	var[0].SetString("$FR_PAUSE_INVENTORY_DROP_ITEM");
	var[1].SetInt(1);
	gfxMovie.Invoke("_root.api.Main.Inventory.addContextMenuOption", var, 2);
}

void HUDPause::eventContextMenu_Action(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	obj_Player* plr = gClientLogic().localPlayer_;

	int slotID = args[0].GetInt(); // SLOT
	int action = args[1].GetInt();

	wiInventoryItem& wi = plr->CurLoadout.Items[slotID];
	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(wi.itemID);
	const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(wi.itemID);
	const GearConfig* gc = g_pWeaponArmory->getGearConfig(wi.itemID);
	const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(wi.itemID);

	switch(action)
	{
	case 1: // DROP ITEM LOGIC
		if(!(plr->CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20188 ||
			plr->CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20187 ||
			plr->CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20067 ||
			plr->CurLoadout.Items[wiCharDataFull::CHAR_LOADOUT_HEADGEAR].itemID == 20209))
			r_hud_filter_mode->SetInt(HUDFilter_Default);

		plr->DropItem(slotID);
		plr->OnBackpackChanged(slotID);
		updateSurvivorTotalWeight();
		//r3dOutToLog("SlotID : %d\n",slotID);
		return;
		break;

	case 2: // UNLOAD CLIP FUNCTION
		if(wc)
		{
			if(wc->category >= storecat_ASR && wc->category <= storecat_SMG)
			{
				if(wi.Var1 > 0)
				{
					PKT_C2S_UnloadClipReq_s n;
					n.slotID = slotID;
					p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n), true);
					wi.Var1 = 0;
					plr->CurLoadout.Items[slotID] = wi;


					plr->OnBackpackChanged(slotID);
					updateSurvivorTotalWeight();
				}
			}
		}
		return;
		break;

	case 3:
		if(wc)
		{
			if(wc->category != storecat_UsableItem) // if consumable, it will send use packet
			{
				Scaleform::GFx::Value newargs[2];
				newargs[0].SetInt(slotID);
				int slotTo = 0;
				if(wc->category == storecat_MELEE || wc->category==storecat_HG)
					slotTo = 1;
				else if(wc->category == storecat_GRENADE)
				{
					// find empty quick slot
					if(plr->CurLoadout.Items[2].itemID==0)
						slotTo = 2;
					else if(plr->CurLoadout.Items[3].itemID==0)
						slotTo = 3;
					else if(plr->CurLoadout.Items[4].itemID==0)
						slotTo = 4;
					else if(plr->CurLoadout.Items[5].itemID==0)
						slotTo = 5;
					else
						slotTo = 5;
				}
				newargs[1].SetInt(slotTo);
				eventBackpackGridSwap(pMovie, newargs, 2);
				return; 
			}
		}

		if(gc)
		{
			Scaleform::GFx::Value newargs[2];
			newargs[0].SetInt(slotID);
			int slotTo = 6;
			if(gc->category == storecat_Helmet)
				slotTo = 7;
			newargs[1].SetInt(slotTo);
			eventBackpackGridSwap(pMovie, newargs, 2);
			return;
		}
		break;

	case 4:
		if(wac)
		{
			gClientLogic().localPlayer_->ShowCrosshair=false;
			Deactivate();
			hudAttm->Activate();
			return;
		}
		break;

	case 5: // USE ITEM
		eventBackpackUseItem(slotID);
		return;
		break;

	case 6:
		if(bc)
		{
			gfxMovie.Invoke("_root.api.Main.showChangeBackpack", "");
			return;
		}
		break;

	case 7:
		break;

	case 8: // STACK CLIPS
		if (wac)
		{
			PKT_C2S_StackClips_s n;
			n.slotId = slotID;
			p2pSendToHost(plr,&n,sizeof(n));
		}
		return;
		break;

	default:
		break;
	}
	r3dError("Something went wrong for Action: %d!", action);
}

void HUDPause::Update()
{
	if(m_waitingForKeyRemap != -1)
	{
		// query input manager for any input
		bool conflictRemapping = false;
		if(InputMappingMngr->attemptRemapKey((r3dInputMappingMngr::KeybordShortcuts)m_waitingForKeyRemap, conflictRemapping))
		{
			Scaleform::GFx::Value var[2];
			var[0].SetNumber(m_waitingForKeyRemap);
			var[1].SetString(InputMappingMngr->getKeyName((r3dInputMappingMngr::KeybordShortcuts)m_waitingForKeyRemap));
			gfxMovie.Invoke("_root.api.updateKeyboardMapping", var, 2);
			m_waitingForKeyRemap = -1;

			void writeInputMap();
			writeInputMap();

			if(conflictRemapping)
			{
				Scaleform::GFx::Value var[2];
				var[0].SetStringW(gLangMngr.getString("ConflictRemappingKeys"));
				var[1].SetBoolean(true);
				gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);		
			}
		}
	}

	if(DisconnectAt > 0)
	{
		int timeLeft = int(ceilf(DisconnectAt - r3dGetTime()));
		Scaleform::GFx::Value var[3];
		char tmpMsg[64];
		if(timeLeft > 0)
			sprintf(tmpMsg, "Disconnecting in %d seconds...", timeLeft);
		else
			sprintf(tmpMsg, "Disconnecting soon...");
		var[0].SetString(tmpMsg);
		var[1].SetBoolean(false);
		var[2].SetString("DISCONNECTING");
		gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
		// FOR DENIS: enable this to show CANCEL button
		// 		var[1].SetString("CANCEL");
		// 		var[2].SetString("DISCONNECTING");
		// 		gfxMovie.Invoke("_root.api.showInfoMsg2", var, 3);
	}
}

void HUDPause::Draw()
{
	gfxMovie.UpdateAndDraw();
}

void HUDPause::Deactivate()
{
	if(DisconnectAt > 0.0) // do not allow to hide this menu if player is disconneting
		return;

	{
		Scaleform::GFx::Value var[1];
		var[0].SetString("menu_close");
		gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);
	}

	if(prevKeyboardCaptureMovie)
	{
		prevKeyboardCaptureMovie->SetKeyboardCapture();
		prevKeyboardCaptureMovie = NULL;
	}

	if( !g_cursor_mode->GetInt() )
	{
		r3dMouse::Hide();
	}

	isActive_ = false;
	r_gameplay_blur_strength->SetFloat(0.0f);
}

void HUDPause::Activate()
{
	DisconnectAt = 0.0f;

	//prevKeyboardCaptureMovie = gfxMovie.SetKeyboardCapture(); // for mouse scroll events

	r3d_assert(!isActive_);
	r3dMouse::Show();
	isActive_ = true;
	r_gameplay_blur_strength->SetFloat(1.0f);

	Scaleform::GFx::Value var[30];
	var[0].SetNumber(0);
	var[1].SetNumber( r_overall_quality->GetInt());
	var[2].SetNumber( ((r_gamma_pow->GetFloat()-2.2f)+1.0f)/2.0f);
	var[3].SetNumber( 0.0f );//r_contrast->GetFloat());
	var[4].SetNumber( s_sound_volume->GetFloat());
	var[5].SetNumber( s_music_volume->GetFloat());
	var[6].SetNumber( s_comm_volume->GetFloat());
	var[7].SetNumber( g_tps_camera_mode->GetInt());
	var[8].SetNumber( g_enable_voice_commands->GetBool());
	var[9].SetNumber( r_antialiasing_quality->GetInt());
	var[10].SetNumber( r_ssao_quality->GetInt());
	var[11].SetNumber( r_terrain_quality->GetInt());
	var[12].SetNumber( r_decoration_quality->GetInt() ); 
	var[13].SetNumber( r_water_quality->GetInt());
	var[14].SetNumber( r_shadows_quality->GetInt());
	var[15].SetNumber( r_lighting_quality->GetInt());
	var[16].SetNumber( r_particles_quality->GetInt());
	var[17].SetNumber( r_mesh_quality->GetInt());
	var[18].SetNumber( r_anisotropy_quality->GetInt());
	var[19].SetNumber( r_postprocess_quality->GetInt());
	var[20].SetNumber( r_texture_quality->GetInt());
	var[21].SetNumber( g_vertical_look->GetBool());
	var[22].SetNumber( 0 ); // not used
	var[23].SetNumber( g_mouse_wheel->GetBool());
	var[24].SetNumber( g_mouse_sensitivity->GetFloat());
	var[25].SetNumber( g_mouse_acceleration->GetBool());
	var[26].SetNumber( g_toggle_aim->GetBool());
	var[27].SetNumber( g_toggle_crouch->GetBool());
	var[28].SetNumber(g_FastLoad->GetInt());
	var[29].SetNumber( r_vsync_enabled->GetInt()+1);

	gfxMovie.Invoke("_root.api.setOptions", var, 30);

	// add player info
	{
		Scaleform::GFx::Value var[23];

		obj_Player* plr = gClientLogic().localPlayer_;
		r3d_assert(plr);
		wiCharDataFull& slot = plr->CurLoadout;
		char tmpGamertag[128];
		if(plr->ClanID != 0)
			sprintf(tmpGamertag, "[%s] %s", plr->ClanTag, slot.Gamertag);
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

		var[15].SetNumber(0);		// weight
		var[16].SetNumber(0);		// zombies Killed
		var[17].SetNumber(0);		// bandits killed
		var[18].SetNumber(0);		// civilians killed
		var[19].SetString("");	// alignment
		var[20].SetString("");	// last Map
		var[21].SetBoolean(false); // globalInventory. For now false. Separate UI for it

		var[22].SetNumber(slot.Stats.SkillXPPool);

		gfxMovie.Invoke("_root.api.addClientSurvivor", var, 23);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////
	//Resources for Crafting////
	//Codex Craft
	{
		obj_Player* plr = gClientLogic().localPlayer_;
		r3d_assert(plr);
		wiCharDataFull& slot = plr->CurLoadout;

		Scaleform::GFx::Value var[3];
		var[0].SetUInt(slot.Stats.Wood); //Wood slot.Stats.Wood
		var[1].SetUInt(slot.Stats.Stone);//Stone slot.Stats.Stone
		var[2].SetUInt(slot.Stats.Metal);//Metal slot.Stats.Metal
        gfxMovie.Invoke("_root.api.setResourcesNum", var, 3);
	}
    ///////////////////////////////////////////////////////////////////////////////////////////////////

	{
		char tmpStr[256];
		sprintf(tmpStr, "SERVER NAME: %s", gClientLogic().m_gameInfo.name);
		gfxMovie.Invoke("_root.api.setServerName", tmpStr);
	}

	reloadBackpackInfo();
	addMission();
	updateSurvivorTotalWeight();

	// hide by default
	gfxMovie.Invoke("_root.api.showInventory", 0);
	gfxMovie.Invoke("_root.api.showMap", 0);
	gfxMovie.Invoke("_root.api.showMissions", 0);
	gfxMovie.Invoke("_root.api.showOptions", 0);

	{
		Scaleform::GFx::Value var[1];
		var[0].SetString("menu_open");
		gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);
	}
}
void HUDPause::SetMissionInfo(int var1,bool var2,bool var3,const char* var4,const char* var5,const char* var6,const char* var7,int var8)
{
	Scaleform::GFx::Value var[7];
	var[0].SetUInt(var1);
	var[1].SetBoolean(var2);
	var[2].SetBoolean(var3);
	var[3].SetString(var5);
	var[4].SetString(var6);
	var[5].SetString(var7);
	var[6].SetUInt(var8);
	gfxMovie.Invoke("_root.api.Main.Missions.SetMissionInfo", var, 7);
}
void HUDPause::addMission()
{
	// Idx , isDelied , isAccpet ,  NULL , Name , Desc , Icon , MissionID
	obj_Player* plr = gClientLogic().localPlayer_;
	if (plr->CurLoadout.Mission1 == 0)
		SetMissionInfo(1,false,false,"","Lets Play","PICK UP ITEM & KILL 5 ZOMBIE : 200XP","",1);
	else if (plr->CurLoadout.Mission1 != 0 && plr->CurLoadout.Mission1 !=3)
		SetMissionInfo(1,false,true,"","Lets Play","PICK UP ITEM & KILL 5 ZOMBIE : 200XP","",1);
}
void HUDPause::updateSurvivorTotalWeight()
{
	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);

	char tmpGamertag[128];
	if(plr->CurLoadout.ClanID != 0)
		sprintf(tmpGamertag, "[%s] %s", plr->CurLoadout.ClanTag, plr->CurLoadout.Gamertag);
	else
		r3dscpy(tmpGamertag, plr->CurLoadout.Gamertag);

	float totalWeight = plr->CurLoadout.getTotalWeight();

	Scaleform::GFx::Value var[2];
	var[0].SetString(tmpGamertag);
	var[1].SetNumber(totalWeight);
	gfxMovie.Invoke("_root.api.updateClientSurvivorWeight", var, 2);
}

void HUDPause::setTime(__int64 utcTime)
{
	const static char* months[12] = {
		"January",
		"February",
		"March",
		"April",
		"May",
		"June",
		"July",
		"August",
		"September",
		"October",
		"November",
		"December"
	};
	struct tm* tm = _gmtime64(&utcTime);

	char date[128];
	char time[128];
	sprintf(date, "%s %d, %d", months[tm->tm_mon], tm->tm_mday, 1900 + tm->tm_year);
	sprintf(time, "%02d:%02d", tm->tm_hour, tm->tm_min);

	Scaleform::GFx::Value var[2];
	var[0].SetString(date);
	var[1].SetString(time);
	gfxMovie.Invoke("_root.api.setTime", var, 2);
}

void HUDPause::showInventory()
{
	gfxMovie.Invoke("_root.api.showInventory", 1);
}

void HUDPause::showMap()
{
	obj_Player* LocalPlr = gClientLogic().localPlayer_;
	r3d_assert(LocalPlr);

	setMinimapPosition(LocalPlr->GetPosition(), LocalPlr->GetvForw());
	gfxMovie.Invoke("_root.api.showMap", 1);

	ClientGameLogic& CGL = gClientLogic();
	/*	for(int i=0; i<ClientGameLogic::MAX_NUM_PLAYERS; ++i)
	{
	obj_Player* plr = CGL.GetPlayer(i);
	//r3d_assert(plr);
	if(plr)
	{
	Scaleform::GFx::Value var[5];
	var[0].SetNumber(gClientLogic().HelpCalls[i].pos.x);
	var[1].SetNumber(gClientLogic().HelpCalls[i].pos.y);

	var[2].SetString(gClientLogic().HelpCalls[i].DistressText);
	var[3].SetString(gClientLogic().HelpCalls[i].rewardText);
	var[4].SetString(plr->CurLoadout.Gamertag);
	gfxMovie.Invoke("_root.api.Main.Map.addCallForHelpEvent", var, 5);
	}
	}*/
	for(int i=0; i<ClientGameLogic::MAX_NUM_PLAYERS; ++i)
	{
		obj_Player* plr = CGL.GetPlayer(i);
		//r3d_assert(plr);
		//if (plr)
		//{
		if(gClientLogic().HelpCalls[i].DistressText[0] && gClientLogic().HelpCalls[i].rewardText[0])
		{
			Scaleform::GFx::Value var[5];
			var[0].SetNumber(gClientLogic().HelpCalls[i].pos.x);
			var[1].SetNumber(gClientLogic().HelpCalls[i].pos.y);

			var[2].SetString(gClientLogic().HelpCalls[i].DistressText);
			var[3].SetString(gClientLogic().HelpCalls[i].rewardText);
			//	var[2].SetString("FAP");
			//var[3].SetString("FAPREWARD");
			var[4].SetString(gClientLogic().playerNames[i].Gamertag);
			gfxMovie.Invoke("_root.api.Main.Map.addCallForHelpEvent", var, 5);
		}
		//	}
		if(plr && plr != LocalPlr && plr->ClanID == LocalPlr->ClanID && plr->ClanID != 0)
		{
			r3dPoint2D pos = getMinimapPos(plr->GetPosition());
			Scaleform::GFx::Value var[2];
			var[0].SetNumber(pos.x);
			var[1].SetNumber(pos.y);
			gfxMovie.Invoke("_root.api.Main.Map.addClanPlayer", var, 2);
		}
		if(plr && plr != LocalPlr && plr->CurLoadout.GroupID == LocalPlr->CurLoadout.GroupID && plr->CurLoadout.GroupID != 0)
		{
			r3dPoint2D pos = getMinimapPos(plr->GetPosition());
			Scaleform::GFx::Value var[2];
			var[0].SetNumber(pos.x);
			var[1].SetNumber(pos.y);
			gfxMovie.Invoke("_root.api.Main.Map.addGroupPlayer", var, 2);
		}
	}

	gfxMovie.Invoke("_root.api.Main.Map.submitPlayerLocations", "");
}

void HUDPause::eventSendCallForHelp(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	/*
	public function addCallForHelpEvent(arg1:Number, arg2:Number, arg3:String, arg4:String, arg5:String):*
	{
	this.CallForHelpEvents.push({"posx":arg1, "posy":arg2, "distressText":arg3, "rewardText":arg4, "username":arg5, "movie":null});
	*/

	obj_Player* plr = gClientLogic().localPlayer_;
	//plr.
	char distressText[128] = {0};
	char rewardText[128] = {0};
	//const char* distressText = args[0].GetString();
	//const char* rewardText = args[1].GetString();
	const char* pName =	plr->CurLoadout.Gamertag;
	r3dscpy(distressText,args[0].GetString());
	r3dscpy(rewardText,args[1].GetString());
	r3dPoint2D pos = getMinimapPos(plr->GetPosition());

	PKT_C2S_SendHelpCall_s n;
	n.CustomerID = plr->CustomerID;
	n.pos.x = pos.x;
	n.pos.y = pos.y;
	sprintf(n.DistressText,distressText);
	sprintf(n.rewardText,rewardText);
	//n.DistressText = distressText;
	//n.rewardText = rewardText;
	p2pSendToHost(plr, &n, sizeof(n), true);

	Deactivate();
	/*Scaleform::GFx::Value var[5];
	var[0].SetNumber(pos.x);
	var[1].SetNumber(pos.y);

	var[2].SetString(distressText);
	var[3].SetString(rewardText);
	var[4].SetString(pName);
	gfxMovie.Invoke("_root.api.Main.Map.addCallForHelpEvent", var, 5);

	r3dOutToLog("User %s called on pos: %f, %f for Help: %s\n", pName, pos.y, pos.y, distressText);*/
}


void HUDPause::reloadBackpackInfo()
{
	// reset backpack
	{
		gfxMovie.Invoke("_root.api.clearBackpack", "");
		gfxMovie.Invoke("_root.api.clearBackpacks", "");
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
				gfxMovie.Invoke("_root.api.addBackpackItem", var, 7);

				const BackpackConfig* bpc = g_pWeaponArmory->getBackpackConfig(slot.Items[a].itemID);
				if(bpc)
				{
					if(std::find<std::vector<uint32_t>::iterator, uint32_t>(uniqueBackpacks.begin(), uniqueBackpacks.end(), slot.Items[a].itemID) != uniqueBackpacks.end())
						continue;

					// add backpack info
					var[0].SetInt(backpackSlotIDInc++);
					var[1].SetUInt(slot.Items[a].itemID);
					gfxMovie.Invoke("_root.api.addBackpack", var, 2);

					uniqueBackpacks.push_back(slot.Items[a].itemID);
				}
			}
		}
	}

	gfxMovie.Invoke("_root.api.Main.Inventory.showBackpack", "");
}

void HUDPause::eventOptionsControlsRequestKeyRemap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 1);

	int remapIndex = (int)args[0].GetNumber();
	r3d_assert(m_waitingForKeyRemap == -1);

	m_waitingForKeyRemap = remapIndex;
}

void HUDPause::eventOptionsControlsReset(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(args);
	r3d_assert(argCount == 0);

	//	InputMappingMngr->resetKeyMappingsToDefault();
	for(int i=0; i<r3dInputMappingMngr::KS_NUM; ++i)
	{
		Scaleform::GFx::Value args[2];
		args[0].SetStringW(gLangMngr.getString(InputMappingMngr->getMapName((r3dInputMappingMngr::KeybordShortcuts)i)));
		args[1].SetString(InputMappingMngr->getKeyName((r3dInputMappingMngr::KeybordShortcuts)i));
		gfxMovie.Invoke("_root.api.setKeyboardMapping", args, 2);
	}
	void writeInputMap();
	writeInputMap();

	// update those to match defaults in Vars.h
	g_vertical_look			->SetBool(false);
	g_mouse_wheel			->SetBool(true);
	g_mouse_sensitivity		->SetFloat(1.0f);
	g_mouse_acceleration	->SetBool(false);
	g_toggle_aim			->SetBool(false);
	g_toggle_crouch			->SetBool(false);

	// write to ini file
	writeGameOptionsFile();

	Scaleform::GFx::Value var[30];
	var[0].SetNumber(0);
	var[1].SetNumber( r_overall_quality->GetInt());
	var[2].SetNumber( ((r_gamma_pow->GetFloat()-2.2f)+1.0f)/2.0f);
	var[3].SetNumber( 0.0f);//r_contrast->GetFloat());
	var[4].SetNumber( s_sound_volume->GetFloat());
	var[5].SetNumber( s_music_volume->GetFloat());
	var[6].SetNumber( s_comm_volume->GetFloat());
	var[7].SetNumber( g_tps_camera_mode->GetInt());
	var[8].SetNumber( g_enable_voice_commands->GetBool());
	var[9].SetNumber( r_antialiasing_quality->GetInt());
	var[10].SetNumber( r_ssao_quality->GetInt());
	var[11].SetNumber( r_terrain_quality->GetInt());
	var[12].SetNumber( r_decoration_quality->GetInt() ); 
	var[13].SetNumber( r_water_quality->GetInt());
	var[14].SetNumber( r_shadows_quality->GetInt());
	var[15].SetNumber( r_lighting_quality->GetInt());
	var[16].SetNumber( r_particles_quality->GetInt());
	var[17].SetNumber( r_mesh_quality->GetInt());
	var[18].SetNumber( r_anisotropy_quality->GetInt());
	var[19].SetNumber( r_postprocess_quality->GetInt());
	var[20].SetNumber( r_texture_quality->GetInt());
	var[21].SetNumber( g_vertical_look->GetBool());
	var[22].SetNumber( 0 ); // not used
	var[23].SetNumber( g_mouse_wheel->GetBool());
	var[24].SetNumber( g_mouse_sensitivity->GetFloat());
	var[25].SetNumber( g_mouse_acceleration->GetBool());
	var[26].SetNumber( g_toggle_aim->GetBool());
	var[27].SetNumber( g_toggle_crouch->GetBool());
	var[28].SetNumber(g_FastLoad->GetInt());
	var[29].SetNumber( r_vsync_enabled->GetInt()+1);

	gfxMovie.Invoke("_root.api.setOptions", var, 30);

	gfxMovie.Invoke("_root.api.reloadOptions", "");
}

void HUDPause::eventOptionsApply(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r_overall_quality		->SetInt( (int)args[6].GetNumber());

	DWORD settingsChangedFlags = 0;
	GraphicSettings settings;

	switch( r_overall_quality->GetInt() )
	{
	case 1:
		FillDefaultSettings( settings, S_WEAK );
		settingsChangedFlags = SetDefaultSettings( S_WEAK );
		break;
	case 2:
		FillDefaultSettings( settings, S_MEDIUM );
		settingsChangedFlags = SetDefaultSettings( S_MEDIUM );
		break;
	case 3:
		FillDefaultSettings( settings, S_STRONG );
		settingsChangedFlags = SetDefaultSettings( S_STRONG );
		break;
	case 4:
		FillDefaultSettings( settings, S_ULTRA );
		settingsChangedFlags = SetDefaultSettings( S_ULTRA );
		break;
	case 5:
		settings = GetCustomSettings();
		settingsChangedFlags = SetCustomSettings( settings );
		break;
	default:
		r3d_assert( false );
	}

	// clamp brightness and contrast, otherwise if user set it to 0 the screen will be white
	//	r_brightness			->SetFloat( R3D_CLAMP((float)args[1].GetNumber(), 0.25f, 0.75f) );
	//	r_contrast				->SetFloat( R3D_CLAMP((float)args[2].GetNumber(), 0.25f, 0.75f) );
	r_gamma_pow->SetFloat(2.2f + (float(args[1].GetNumber())*2.0f-1.0f));

	s_sound_volume			->SetFloat( R3D_CLAMP((float)args[3].GetNumber(), 0.0f, 1.0f) );
	s_music_volume			->SetFloat( R3D_CLAMP((float)args[4].GetNumber(), 0.0f, 1.0f) );
	s_comm_volume			->SetFloat( R3D_CLAMP((float)args[5].GetNumber(), 0.0f, 1.0f) );

	g_tps_camera_mode->SetInt((int)args[7].GetNumber());
	g_enable_voice_commands			->SetBool( !!(int)args[11].GetNumber() );

	g_vertical_look			->SetBool( !!(int)args[8].GetNumber() );
	g_mouse_wheel			->SetBool( !!(int)args[9].GetNumber() );
	g_mouse_sensitivity		->SetFloat( (float)args[0].GetNumber() );
	g_mouse_acceleration	->SetBool( !!(int)args[10].GetNumber() );

	settingsChangedFlags |= GraphSettingsToVars( settings );

	r3dRenderer->UpdateSettings();
	void applyGraphicsOptions( uint32_t settingsFlags );
	applyGraphicsOptions( settingsChangedFlags );

	// write to ini file
	writeGameOptionsFile();
}

