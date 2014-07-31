// TRADE SYSTEMS
// WRITTEN BY Laenoar
// used in Days after Dead

#include "r3dPCH.h"
#include "r3d.h"

#include "r3dDebug.h"

#include "HUDTrade.h"

#include "FrontendShared.h"

#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/ai/AI_Player.h"


#include "../multiplayer/MasterServerLogic.h"
#include "../multiplayer/ClientGameLogic.h"

extern const char* getReputationString(int Reputation);

HUDTrade::HUDTrade() :
isActive_(false),
isInit_(false),
prevKeyboardCaptureMovie_(NULL)
{
}

HUDTrade::~HUDTrade()
{
}

bool HUDTrade::Init()
{
	if(!gfxMovie_.Load("Data\\Menu\\WarZ_HUD_Trade.swf", false)) 
	{
		return false;
	}

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDTrade>(this, &HUDTrade::FUNC)
	gfxMovie_.RegisterEventHandler("eventReturnToGame", MAKE_CALLBACK(eventReturnToGame));
	gfxMovie_.RegisterEventHandler("eventBackpackToTrade", MAKE_CALLBACK(eventBackpackToTrade));
	gfxMovie_.RegisterEventHandler("eventTradeToBackpack", MAKE_CALLBACK(eventTradeToBackpack));
	gfxMovie_.RegisterEventHandler("eventTradeDecline", MAKE_CALLBACK(eventTradeDecline));
	gfxMovie_.RegisterEventHandler("eventTradeAccept", MAKE_CALLBACK(eventTradeAccept));

	gfxMovie_.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);
	itemInventory_.initialize(&gfxMovie_);

	isActive_ = false;
	isInit_ = true;
	return true;
}

bool HUDTrade::Unload()
{
	gfxMovie_.Unload();
	isActive_ = false;
	isInit_ = false;
	return true;
}

void HUDTrade::Update()
{
	if (!plr)
	{
		Deactivate();
		return;
	}
	if (EnableButton > r3dGetTime())
		enableUserTradeIndicator(false);
	else if (!isAccept && !isAcceptButton)
		enableUserTradeIndicator(true);
}

void HUDTrade::Draw()
{
	gfxMovie_.UpdateAndDraw();
}

void HUDTrade::Deactivate()
{
	plr = NULL;
	ClearInventory();
	gfxMovie_.Invoke("_root.api.hideScreen", 0);

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_close");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);

	if(!g_cursor_mode->GetInt())
	{
		r3dMouse::Hide();
	}

	isActive_ = false;
}
void HUDTrade::updateSurvivorTotalWeight(const wiCharDataFull& slot)
{
	float totalWeight = slot.getTotalWeight();


	Scaleform::GFx::Value var[2];
	var[0].SetString(slot.Gamertag);
	var[1].SetNumber(totalWeight);
	gfxMovie_.Invoke("_root.api.updateClientSurvivorWeight", var, 2);
}
void HUDTrade::updateInventoryAndSkillItems()
{
	Scaleform::GFx::Value var[7];
	// clear inventory DB
	gfxMovie_.Invoke("_root.api.clearInventory", NULL, 0);
	gfxMovie_.Invoke("_root.api.Main.clearTradeInfo", NULL, 0);

	// add all items
	for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
	{
		var[0].SetUInt(uint32_t(gUserProfile.ProfileData.Inventory[i].InventoryID));
		var[1].SetUInt(gUserProfile.ProfileData.Inventory[i].itemID);
		var[2].SetNumber(gUserProfile.ProfileData.Inventory[i].quantity);
		var[3].SetNumber(gUserProfile.ProfileData.Inventory[i].Var1);
		var[4].SetNumber(gUserProfile.ProfileData.Inventory[i].Var2);
		bool isConsumable = false;
		{
			const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(gUserProfile.ProfileData.Inventory[i].itemID);
			if(wc && wc->category == storecat_UsableItem && wc->m_isConsumable)
				isConsumable = true;
		}
		var[5].SetBoolean(isConsumable);
		char tmpStr[128] = {0};
		getAdditionalDescForItem(gUserProfile.ProfileData.Inventory[i].itemID, gUserProfile.ProfileData.Inventory[i].Var1, gUserProfile.ProfileData.Inventory[i].Var2, tmpStr);
		var[6].SetString(tmpStr);
		gfxMovie_.Invoke("_root.api.addInventoryItem", var, 7);
	}
}
void HUDTrade::ClearInventory()
{
	for (int i =0;i<72;i++)
	{
		SlotItems[i] = 0;
		Items[i].Reset();
	}
}
void HUDTrade::Activate()
{
	if (!plr) return;

	EnableButton = 0.0f;
	//if (isAccept) return;
	isAccept = false;
	ClearInventory();
	char name[128];
	plr->GetUserName(name);
	setOppositePlayerName(name);
	enableUserTradeIndicator(true);
	addClientSurvivor(gClientLogic().localPlayer_->CurLoadout);
	updateInventoryAndSkillItems();
	reloadBackpackInfo();
	updateSurvivorTotalWeight(gClientLogic().localPlayer_->CurLoadout);
	showUserTrade();
	showOppositeTrade();
	r3d_assert(!isActive_);
	r3dMouse::Show();
	isActive_ = true;

	gfxMovie_.Invoke("_root.api.showScreen", 0);
	setUserTradeIndicator(false);

	Scaleform::GFx::Value var[1];
	var[0].SetString("menu_open");
	gfxMovie_.OnCommandCallback("eventSoundPlay", var, 1);
}
void HUDTrade::eventTradeToBackpack(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	if (isAccept) return;
	EnableButton = r3dGetTime() + 5;
	int m_gridFrom = args[0].GetInt();
	int m_gridTo = args[1].GetInt();
	int amount = args[2].GetInt();
	Scaleform::GFx::Value var[7];
	var[0].SetInt(SlotItems[m_gridFrom]);
	var[1].SetUInt(0);
	var[2].SetUInt(Items[m_gridFrom].itemID);
	var[3].SetInt(Items[m_gridFrom].quantity);
	var[4].SetInt(Items[m_gridFrom].Var1);
	var[5].SetInt(Items[m_gridFrom].Var2);
	char tmpStr[128] = {0};
	getAdditionalDescForItem(Items[m_gridFrom].itemID, Items[m_gridFrom].Var1, Items[m_gridFrom].Var2, tmpStr);
	var[6].SetString(tmpStr);
	gfxMovie_.Invoke("_root.api.addBackpackItem", var, 7);
	gfxMovie_.Invoke("_root.api.Main.removeItemFromUserTrade",m_gridFrom);

	PKT_C2S_TradeOptoBack_s n;
	n.Item = Items[m_gridFrom];
	n.slotfrom = m_gridFrom;
	n.netId = plr->GetNetworkID();
	p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));

	Items[m_gridFrom].Reset();
	SlotItems[m_gridFrom] = 0;

	// special functions : if Opposite Player is accepted , unaccept this in local because server will unaccept when you send this packet to servers
	if (OpTradeIndicator) //
	{
		// lets setting hud up.
		setOppositeTradeIndicator(false);
		// make sure he is unaccepted
		OpTradeIndicator = false;
	}

	showOppositeTrade();
	showBackpack();
	showUserTrade();
}
void HUDTrade::eventBackpackToTrade(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	if (isAccept) return;
	int m_gridFrom = args[0].GetInt();
	int m_gridTo = args[1].GetInt();
	int amount = args[2].GetInt();
	if (m_gridTo > 15) return;

	EnableButton = r3dGetTime() + 5;

	wiCharDataFull& slot = gClientLogic().localPlayer_->CurLoadout;

	Scaleform::GFx::Value var[8];
	var[0].SetInt(m_gridTo);
	var[1].SetUInt(0);
	var[2].SetUInt(slot.Items[m_gridFrom].itemID);
	var[3].SetInt(slot.Items[m_gridFrom].quantity);
	var[4].SetInt(slot.Items[m_gridFrom].Var1);
	var[5].SetInt(slot.Items[m_gridFrom].Var2);
	var[6].SetBoolean(false);
	char tmpStr[128] = {0};
	getAdditionalDescForItem(slot.Items[m_gridFrom].itemID, slot.Items[m_gridFrom].Var1, slot.Items[m_gridFrom].Var2, tmpStr);
	var[7].SetString(tmpStr);

	Items[m_gridTo] = slot.Items[m_gridFrom];
	SlotItems[m_gridTo] = m_gridFrom;
	gfxMovie_.Invoke("_root.api.Main.addItemToUserTrade", var, 8);
	//gfxMovie_.Invoke("_root.api.Main.addItemToOppositeTrade", var, 8);
	gfxMovie_.Invoke("_root.api.survivor.removeBackpackItem",m_gridFrom);

	PKT_C2S_TradeBacktoOp_s n;
	n.Item = slot.Items[m_gridFrom];
	n.slotto = m_gridTo;
	n.netId = plr->GetNetworkID();
	p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));

	// special functions : if Opposite Player is accepted , unaccept this in local because server will unaccept when you send this packet to servers
	if (OpTradeIndicator) //
	{
		// lets setting hud up.
		setOppositeTradeIndicator(false);
		// make sure he is unaccepted
		OpTradeIndicator = false;
	}

	showOppositeTrade();
	showUserTrade();
	showBackpack();
}
// OnNetPacket is Net SYNC Packet for trade.
void HUDTrade::OnNetPacket(const PKT_C2S_TradeOptoBack_s& n)
{
	// special functions hardcoded
	if (isAccept)
	{
		// do it now , unaccept yourselfs.
		setUserTradeIndicator(false);
		enableUserTradeIndicator(true);
		isAccept = false;
	}

	gfxMovie_.Invoke("_root.api.Main.removeItemFromOppositeTrade",n.slotfrom);
	showUserTrade();
	showOppositeTrade();
	showBackpack();
}
void HUDTrade::OnNetPacket(const PKT_C2S_TradeBacktoOp_s& n)
{

	// special functions hardcoded
	if (isAccept)
	{
		// do it now , unaccept yourselfs.
		setUserTradeIndicator(false);
		enableUserTradeIndicator(true);
		isAccept = false;
	}

	Scaleform::GFx::Value var[8];
	var[0].SetInt(n.slotto);
	var[1].SetUInt(0);
	var[2].SetUInt(n.Item.itemID);
	var[3].SetInt(n.Item.quantity);
	var[4].SetInt(n.Item.Var1);
	var[5].SetInt(n.Item.Var2);
	var[6].SetBoolean(false);
	char tmpStr[128] = {0};
	getAdditionalDescForItem(n.Item.itemID, n.Item.Var1, n.Item.Var2, tmpStr);
	var[7].SetString(tmpStr);
	gfxMovie_.Invoke("_root.api.Main.addItemToOppositeTrade", var, 8);
	showUserTrade();
	showOppositeTrade();
	showBackpack();
}
void HUDTrade::addBackpackItems(const wiCharDataFull& slot)
{
	Scaleform::GFx::Value var[7];
	//r3dOutToLog("AddBackPackItems\n");
	for (int a = 0; a < slot.BackpackSize; a++)
	{
		if (slot.Items[a].InventoryID != 0)
		{
			var[0].SetInt(a);
			var[1].SetUInt(uint32_t(slot.Items[a].InventoryID));
			var[2].SetUInt(slot.Items[a].itemID);
			var[3].SetInt(slot.Items[a].quantity);
			var[4].SetInt(slot.Items[a].Var1);
			var[5].SetInt(slot.Items[a].Var2);
			char tmpStr[128] = {0};
			getAdditionalDescForItem(slot.Items[a].itemID, slot.Items[a].Var1, slot.Items[a].Var2, tmpStr);
			var[6].SetString(tmpStr);
			gfxMovie_.Invoke("_root.api.addBackpackItem", var, 7);
		}
	}
}
void HUDTrade::addClientSurvivor(const wiCharDataFull& slot)
{
	Scaleform::GFx::Value var[22];
	char tmpGamertag[128];
	if(slot.ClanID != 0)
		sprintf(tmpGamertag, "[%s] %s", slot.ClanTag, slot.Gamertag);
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
	var[15].SetNumber(0);        // weight
	var[16].SetNumber(slot.Stats.KilledZombies);        // zombies Killed
	var[17].SetNumber(slot.Stats.KilledBandits);        // bandits killed
	var[18].SetNumber(slot.Stats.KilledSurvivors);        // civilians killed
	var[19].SetString(getReputationString(slot.Stats.Reputation));    // alignment
	var[20].SetString("COLORADO");    // last Map
	var[21].SetBoolean(slot.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox);


	gfxMovie_.Invoke("_root.api.addClientSurvivor", var, 22);


	gfxMovie_.Invoke("_root.api.clearBackpack", NULL, 0);


	addBackpackItems(slot);
}
void HUDTrade::setOppositePlayerName(const char* var1)
{
	Scaleform::GFx::Value var[1];
	var[0].SetString(var1);
	gfxMovie_.Invoke("_root.api.Main.setOppositePlayerName", var, 1);
}
void HUDTrade::enableUserTradeIndicator(bool var1)
{
	isAcceptButton = var1;
	Scaleform::GFx::Value var[1];
	var[0].SetBoolean(var1);
	gfxMovie_.Invoke("_root.api.Main.enableUserTradeIndicator", var, 1);
}
void HUDTrade::showOppositeTrade()
{
	gfxMovie_.Invoke("_root.api.Main.showOppositeTrade",0);
}
void HUDTrade::showUserTrade()
{
	gfxMovie_.Invoke("_root.api.Main.showUserTrade",0);
}
void HUDTrade::showBackpack()
{
	gfxMovie_.Invoke("_root.api.Main.showBackpack",0);
}
void HUDTrade::setUserTradeIndicator(bool var1)
{
	Scaleform::GFx::Value var[1];
	var[0].SetBoolean(var1);
	gfxMovie_.Invoke("_root.api.Main.setUserTradeIndicator", var, 1);
}
void HUDTrade::setOppositeTradeIndicator(bool var1)
{
	OpTradeIndicator = var1;

	Scaleform::GFx::Value var[1];
	var[0].SetBoolean(var1);
	gfxMovie_.Invoke("_root.api.Main.setOppositeTradeIndicator", var, 1);
}
void HUDTrade::reloadBackpackInfo()
{
	// reset backpack
	{
		gfxMovie_.Invoke("_root.api.clearBackpack", "");
		gfxMovie_.Invoke("_root.api.clearBackpacks", "");
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
				gfxMovie_.Invoke("_root.api.addBackpackItem", var, 7);


				const BackpackConfig* bpc = g_pWeaponArmory->getBackpackConfig(slot.Items[a].itemID);
				if(bpc)
				{
					if(std::find<std::vector<uint32_t>::iterator, uint32_t>(uniqueBackpacks.begin(), uniqueBackpacks.end(), slot.Items[a].itemID) != uniqueBackpacks.end())
						continue;


					// add backpack info
					var[0].SetInt(backpackSlotIDInc++);
					var[1].SetUInt(slot.Items[a].itemID);
					gfxMovie_.Invoke("_root.api.addBackpack", var, 2);


					uniqueBackpacks.push_back(slot.Items[a].itemID);
				}
			}
		}
	}


	gfxMovie_.Invoke("_root.api.Main.Inventory.showBackpack", "");
}
void HUDTrade::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	PKT_C2S_TradeCancel_s n;
	n.netId = (int)plr->GetNetworkID();
	p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));

	Deactivate();
}

void HUDTrade::eventTradeAccept(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	PKT_C2S_TradeAccept2_s n;
	n.netId = (int)plr->GetNetworkID();
	for (int i=0;i<72;i++)
	{
		// we need to send a slot of item in backpack . use for checking cheats.
		n.Item[i] = Items[i];
		n.slot[i] = SlotItems[i];
	}
	p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
	isAccept = true;
	enableUserTradeIndicator(false);
	setUserTradeIndicator(true);
	// not deactivate() at now. we need to wait other player to accept and wait for tradeing operation complete.
}

void HUDTrade::eventTradeDecline(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	PKT_C2S_TradeCancel_s n;
	n.netId = (int)plr->GetNetworkID();
	p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));

	Deactivate();
}