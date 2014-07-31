#include "r3dPCH.h"
#include "r3d.h"

#include "CkHttpRequest.h"
#include "CkHttp.h"
#include "CkHttpResponse.h"
#include "CkByteData.h"

#include "UserProfile.h"
#include "UserFriends.h"
#include "UserClans.h"
#include "UserSkills.h"
#include "backend/WOBackendAPI.h"
#ifdef FINAL_BUILD
#include "resource.h"
#include "HShield.h"
#include "HSUpChk.h"
#pragma comment(lib,"HShield.lib")
#pragma comment(lib,"HSUpChk.lib")
#include <assert.h>
#include <stdio.h>
#include <winsock2.h>
#include <process.h>
#endif
#include "ObjectsCode/WEAPONS/WeaponConfig.h"
#include "ObjectsCode/WEAPONS/WeaponArmory.h"

#ifndef WO_SERVER
#include "SteamHelper.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////
#ifndef WO_SERVER
CClientUserProfile	gUserProfile;
#endif

bool storecat_IsItemStackable(uint32_t ItemID)
{
	const BaseItemConfig* itm = g_pWeaponArmory->getConfig(ItemID);
	if(!itm) 
		return true;

	switch(itm->category)
	{
		case storecat_FPSAttachment:
		case storecat_UsableItem:
		case storecat_Food:
		case storecat_Water:
		case storecat_GRENADE:
			return true;
	}
	
	return false;
}

float wiCharDataFull::getTotalWeight() const
{
	#ifdef FINAL_BUILD
	AHNHS_PROTECT_FUNCTION
#endif
	float totalWeight = 0.0f;
	for(int i=0; i<BackpackSize; ++i)
	{
		if(Items[i].itemID != 0 && Items[i].quantity > 0)
		{
			const BaseItemConfig* bic = g_pWeaponArmory->getConfig(Items[i].itemID);
			if(bic)
				totalWeight += bic->m_Weight * Items[i].quantity;
		}
	}
	return totalWeight;
}

CUserProfile::CUserProfile()
{
	memset((void *)&ProfileData, 0, sizeof(ProfileData));

	CustomerID = 0;
	SessionID = 0;
	AccountStatus = 0;
	ProfileDataDirty = 0;

	ProfileData.NumSlots = 0;
}

CUserProfile::~CUserProfile()
{
}

wiInventoryItem* CUserProfile::getInventorySlot(__int64 InventoryID)
{
	if(InventoryID == 0)
		return NULL;

	// todo: make it faster!
	for(uint32_t i=0; i<ProfileData.NumItems; ++i)
	{
		if(ProfileData.Inventory[i].InventoryID == InventoryID)
			return &ProfileData.Inventory[i];
	}

	return NULL;
}

int CUserProfile::GetProfile(int CharID)
{
	CWOBackendReq req(this, "api_GetProfile1.aspx");
	if(CharID)
		req.AddParam("CharID", CharID);
		
	if(!req.Issue())
	{
		r3dOutToLog("GetProfile FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	
	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	pugi::xml_node xmlAccount = xmlFile.child("account");
	uint32_t custID = xmlAccount.attribute("CustomerID").as_uint();
	if(custID == 0) // bad request
	{
		r3dOutToLog("Bad request in GetProfile()\n");
		return 9;
	}
	
	ProfileDataDirty              = xmlAccount.attribute("DataDirty").as_int();
	AccountStatus                 = xmlAccount.attribute("AccountStatus").as_int();
	ProfileData.AccountType       = xmlAccount.attribute("AccountType").as_int();
	ProfileData.GamePoints        = xmlAccount.attribute("GamePoints").as_int();
	ProfileData.GameDollars       = xmlAccount.attribute("GameDollars").as_int();
	ProfileData.isDevAccount      = xmlAccount.attribute("IsDeveloper").as_int();
	ProfileData.WarGuardSession = xmlAccount.attribute("WarGuardSessionID").as_int();
	ProfileData.isPunisher      = xmlAccount.attribute("IsPunisher").as_int();
		ProfileData.isNoDrop      = xmlAccount.attribute("IsNoDrop").as_int();
		ProfileData.isPremium      = xmlAccount.attribute("isPremium").as_int();

	//r3dOutToLog("Punisher : %d\n",ProfileData.isPunisher);

	const char* curTime = xmlAccount.attribute("time").value();
	memset(&ServerTime, 0, sizeof(ServerTime));
	sscanf(curTime, "%d %d %d %d %d", 
		&ServerTime.tm_year, &ServerTime.tm_mon, &ServerTime.tm_mday,
		&ServerTime.tm_hour, &ServerTime.tm_min);
	ServerTime.tm_year -= 1900;
	ServerTime.tm_mon -= 1;
	ServerTime.tm_isdst = 1; // day light saving time

	// fill things
	ParseLoadouts(xmlAccount.child("chars"));
	ParseInventory(xmlAccount.child("inventory"));
	ParseBackpacks(xmlAccount.child("backpacks"));

	return 0;
}

static void parseCharAttachments(const char* slotData, wiWeaponAttachment& attm)
{
	r3d_assert(slotData);
	if(*slotData == 0) 
	{
		memset(&attm, 0, sizeof(attm));
		return;
	}

	// should match arguments of ApiCharModifyAttachments
	int nargs = sscanf(slotData, "%d %d %d %d %d %d %d %d", 
		&attm.attachments[0], 
		&attm.attachments[1], 
		&attm.attachments[2], 
		&attm.attachments[3], 
		&attm.attachments[4], 
		&attm.attachments[5], 
		&attm.attachments[6], 
		&attm.attachments[7]);
	if(nargs != 8)
	{
		r3dOutToLog("Incorrect number of args in attachments %d\n", nargs);
		memset(&attm, 0, sizeof(attm));
	}
	
	return;
}

static void parseInventoryItem(pugi::xml_node xmlItem, wiInventoryItem& itm)
{
	itm.InventoryID = xmlItem.attribute("id").as_int64();
	itm.itemID      = xmlItem.attribute("itm").as_uint();
	itm.quantity    = xmlItem.attribute("qt").as_uint();
	// if Var2/Var2 isn't supplied - set them -1 by default
	if(xmlItem.attribute("v1").value()[0])
		itm.Var1 = xmlItem.attribute("v1").as_int();
	else
		itm.Var1 = -1;
	if(xmlItem.attribute("v2").value()[0])
		itm.Var2 = xmlItem.attribute("v2").as_int();
	else
		itm.Var2 = -1;

	r3d_assert(itm.InventoryID > 0);
	r3d_assert(itm.itemID > 0);
	r3d_assert(itm.quantity > 0);
}

static void parseCharBackpack(pugi::xml_node xmlItem, wiCharDataFull& w)
{
	#ifdef FINAL_BUILD
	AHNHS_PROTECT_FUNCTION
#endif
	// enter into items list
	xmlItem = xmlItem.first_child();
	while(!xmlItem.empty())
	{
		wiInventoryItem itm;
		parseInventoryItem(xmlItem, itm);

		int slot = xmlItem.attribute("s").as_int();

		//r3d_assert(slot >= 0 && slot < w.BackpackSize);
		//r3d_assert(w.Items[slot].InventoryID == 0);
		w.Items[slot] = itm;

		xmlItem = xmlItem.next_sibling();
	}

	
	return;
}

void CUserProfile::ParseLoadouts(pugi::xml_node& xmlItem)
{
#ifdef FINAL_BUILD
	AHNHS_PROTECT_FUNCTION
#endif
	// reset current backpacks
	for(int i=0; i<ProfileData.NumSlots; i++) {
		for(int j=0; j<ProfileData.ArmorySlots[i].BackpackSize; j++) {
			ProfileData.ArmorySlots[i].Items[j].Reset();
		}
	}

	ProfileData.NumSlots = 0;
	
	// parse all slots
	xmlItem = xmlItem.first_child();
	while(!xmlItem.empty())
	{
		wiCharDataFull& w = ProfileData.ArmorySlots[ProfileData.NumSlots++];
		wiStats& st = w.Stats;
		if(ProfileData.NumSlots > wiUserProfile::MAX_LOADOUT_SLOTS)
			r3dError("more that 6 profiles!");

		w.LoadoutID   = xmlItem.attribute("CharID").as_uint();
		r3dscpy(w.Gamertag, xmlItem.attribute("Gamertag").value());
		w.Alive       = xmlItem.attribute("Alive").as_int();
		w.Hardcore    = xmlItem.attribute("Hardcore").as_int();
		st.XP         = xmlItem.attribute("XP").as_int();
		st.TimePlayed = xmlItem.attribute("TimePlayed").as_int();
		w.Health      = xmlItem.attribute("Health").as_float();
		w.Hunger      = xmlItem.attribute("Hunger").as_float();
		w.Thirst      = xmlItem.attribute("Thirst").as_float();
		w.Toxic       = xmlItem.attribute("Toxic").as_float();
		st.Reputation = xmlItem.attribute("Reputation").as_int();
		w.DeathUtcTime= xmlItem.attribute("DeathTime").as_int64();
		w.SecToRevive = xmlItem.attribute("SecToRevive").as_int();

		w.GameMapId   = xmlItem.attribute("GameMapId").as_int();
		w.GameServerId= xmlItem.attribute("GameServerId").as_int();
		w.GamePos = r3dPoint3D(0, 0, 0);
		sscanf(xmlItem.attribute("GamePos").value(), "%f %f %f %f", &w.GamePos.x, &w.GamePos.y, &w.GamePos.z, &w.GameDir);
		w.GameFlags   = xmlItem.attribute("GameFlags").as_int();

		w.HeroItemID  = xmlItem.attribute("HeroItemID").as_int();
		w.HeadIdx     = xmlItem.attribute("HeadIdx").as_int();
		w.BodyIdx     = xmlItem.attribute("BodyIdx").as_int();
		w.LegsIdx     = xmlItem.attribute("LegsIdx").as_int();

		w.ClanID      = xmlItem.attribute("ClanID").as_int();
			w.GroupID      = xmlItem.attribute("GroupID").as_int();
			w.Mission1      = xmlItem.attribute("Mission1").as_int();
				w.bleeding      = xmlItem.attribute("bleeding").as_int();
				w.legfall      = xmlItem.attribute("legfall").as_int();
		w.ClanRank    = xmlItem.attribute("ClanRank").as_int();
		r3dscpy(w.ClanTag, xmlItem.attribute("ClanTag").value());
		w.ClanTagColor= xmlItem.attribute("ClanTagColor").as_int();

		const char* attm1 = xmlItem.attribute("attm1").value();
		const char* attm2 = xmlItem.attribute("attm2").value();
		parseCharAttachments(attm1, w.Attachment[0]);
		parseCharAttachments(attm2, w.Attachment[1]);

		w.BackpackID   = xmlItem.attribute("BackpackID").as_uint();
		w.BackpackSize = xmlItem.attribute("BackpackSize").as_int();
		
		// trackable stats
		st.KilledZombies   = xmlItem.attribute("ts00").as_int();
		st.KilledSurvivors = xmlItem.attribute("ts01").as_int();
		st.KilledBandits   = xmlItem.attribute("ts02").as_int();

		// skill xp
		st.SkillXPPool = xmlItem.attribute("XP").as_int();

// skills
		st.skillid0 = xmlItem.attribute("SkillID0").as_int();
		st.skillid1 = xmlItem.attribute("SkillID1").as_int();
		st.skillid2 = xmlItem.attribute("SkillID2").as_int();
		st.skillid3 = xmlItem.attribute("SkillID3").as_int();
		st.skillid4 = xmlItem.attribute("SkillID4").as_int();
		st.skillid5 = xmlItem.attribute("SkillID5").as_int();
		st.skillid6 = xmlItem.attribute("SkillID6").as_int();
		st.skillid7 = xmlItem.attribute("SkillID7").as_int();
		st.skillid8 = xmlItem.attribute("SkillID8").as_int();
		st.skillid9 = xmlItem.attribute("SkillID9").as_int();
		st.skillid10 = xmlItem.attribute("SkillID10").as_int();
		st.skillid11 = xmlItem.attribute("SkillID11").as_int();
		st.skillid12 = xmlItem.attribute("SkillID12").as_int();
		st.skillid13 = xmlItem.attribute("SkillID13").as_int();
		st.skillid14 = xmlItem.attribute("SkillID14").as_int();
		st.skillid15 = xmlItem.attribute("SkillID15").as_int();
		st.skillid16 = xmlItem.attribute("SkillID16").as_int();
		st.skillid17 = xmlItem.attribute("SkillID17").as_int();
		st.skillid18 = xmlItem.attribute("SkillID18").as_int();
		st.skillid19 = xmlItem.attribute("SkillID19").as_int();
		st.skillid20 = xmlItem.attribute("SkillID20").as_int();
		st.skillid21 = xmlItem.attribute("SkillID21").as_int();
		st.skillid22 = xmlItem.attribute("SkillID22").as_int();
		st.skillid23 = xmlItem.attribute("SkillID23").as_int();
		st.skillid24 = xmlItem.attribute("SkillID24").as_int();
		st.skillid25 = xmlItem.attribute("SkillID25").as_int();
		st.skillid26 = xmlItem.attribute("SkillID26").as_int();
		st.skillid27 = xmlItem.attribute("SkillID27").as_int();
		st.skillid28 = xmlItem.attribute("SkillID28").as_int();
		st.skillid29 = xmlItem.attribute("SkillID29").as_int();
		st.skillid30 = xmlItem.attribute("SkillID30").as_int();
		st.skillid31 = xmlItem.attribute("SkillID31").as_int();
		st.skillid32 = xmlItem.attribute("SkillID32").as_int();
		st.skillid33 = xmlItem.attribute("SkillID33").as_int();

		xmlItem = xmlItem.next_sibling();
	}
}

void CUserProfile::ParseInventory(pugi::xml_node& xmlItem)
{
	ProfileData.NumItems = 0;

	// enter into items list
	xmlItem = xmlItem.first_child();
	while(!xmlItem.empty())
	{
		wiInventoryItem& itm = ProfileData.Inventory[ProfileData.NumItems++];
		r3d_assert(ProfileData.NumItems < wiUserProfile::MAX_INVENTORY_SIZE);

		parseInventoryItem(xmlItem, itm);

		xmlItem = xmlItem.next_sibling();
	}
}

void CUserProfile::ParseBackpacks(pugi::xml_node& xmlItem)
{
	// enter into items list
	xmlItem = xmlItem.first_child();
	while(!xmlItem.empty())
	{
		uint32_t CharID  = xmlItem.attribute("CharID").as_uint();
		r3d_assert(CharID);
		
		bool found = true;
		for(int i=0; i<ProfileData.NumSlots; i++) 
		{
			if(ProfileData.ArmorySlots[i].LoadoutID == CharID) 
			{
				parseCharBackpack(xmlItem, ProfileData.ArmorySlots[i]);
				found = true;
				break;
			}
		}
		
		if(!found)
			r3dError("bad backpack data for charid %d", CharID);

		xmlItem = xmlItem.next_sibling();
	}
}

wiStoreItem g_StoreItems[MAX_NUM_STORE_ITEMS] = {0}; 
uint32_t g_NumStoreItems = 0;

const char* STORE_CATEGORIES_NAMES[storecat_NUM_ITEMS] = 
{
	"$CatInvalid", // 0
	"$CatAccount", //1
	"$CatBoosts", //2
	"$CatInvalid", //3
	"$CatItems", //4
	"$CatAbilities", //5
	"$CatInvalid", //6
	"$CatLootBox", //7
	"$CatInvalid", //8
	"$CatInvalid", //9
	"$CatInvalid", //10
	"$CatGear", //11
	"$CatInvalid", //12
	"$CatHeadGear", //13
	"$CatInvalid", //14
	"$CatInvalid", //15
	"$CatHeroes", //16
	"$CatInvalid", //17
	"$CatInvalid", //18
	"$CatFPSAttachment", //19
	"$CatASR", //20
	"$CatSniper", //21
	"$CatSHTG", //22
	"$CatMG", //23
	"$CatSUP", //24
	"$CatHG", //25
	"$CatSMG", //26
	"$CatGrenade", //27
	"$CatUsableItem", //28
	"$CatMelee", // 29
};

int CUserProfile::ApiGetShopData()
{
	CWOBackendReq req(this, "api_GetShop1.aspx");
	if(!req.Issue())
	{
		r3dOutToLog("GetShopData FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	g_NumStoreItems = 0;

	const char* d = req.bodyStr_;
	const char* p = d;
	if(p[0] != 'S' || p[1] != 'H' || p[2] != 'O' || p[3] != '1') {
		r3dOutToLog("GetShopData: bad answer #1\n");
		return 9;
	}
	p += 4;

	// shop items
	while(1) 
	{
		if((p - d) >= req.bodyLen_) {
			r3dOutToLog("GetShopData: bad answer #2\n");
			return 9;
		}

		// end tag
		if(p[0] == 'S' && p[1] == 'H' && p[2] == 'O' && p[3] == '1')
			break;

		// item
		DWORD itemId       = *(DWORD*)p; p += 4;
		DWORD flags        = *(BYTE*)p;  p += 1;
		DWORD pricePerm    = *(DWORD*)p; p += 4;
		DWORD gd_pricePerm = *(DWORD*)p; p += 4;

		wiStoreItem& itm = g_StoreItems[g_NumStoreItems++];
		r3d_assert(g_NumStoreItems < MAX_NUM_STORE_ITEMS);
		itm.itemID       = itemId;
		itm.pricePerm    = pricePerm;
		itm.gd_pricePerm = gd_pricePerm;
		itm.isNew        = (flags & 0x1) ? true : false;
	}

	DeriveGamePricesFromItems();

	return 0;
}

void CUserProfile::DeriveGamePricesFromItems()
{
	for(uint32_t i = 0; i<g_NumStoreItems; i++) 
	{
		const wiStoreItem& itm = g_StoreItems[i];

		switch(itm.itemID) 
		{
			case 301151: ShopClanCreate = itm.pricePerm; break;
		}
		
		// clan add members items
		if(itm.itemID >= 301152 && itm.itemID <= 301157)
		{
			ShopClanAddMembers_GP[itm.itemID - 301152]  = itm.pricePerm;
			ShopClanAddMembers_Num[itm.itemID - 301152] = itm.gd_pricePerm;
		}
	}
}

#ifndef WO_SERVER

class GameObject;
#include "ObjectsCode/Weapons/WeaponArmory.h"

CClientUserProfile::CClientUserProfile()
{
	steamCallbacks = NULL;
	friends = new CUserFriends();

	for(int i=0; i<wiUserProfile::MAX_LOADOUT_SLOTS; i++)
		clans[i] = new CUserClans();
	
	SelectedCharID = 0;

	ShopClanCreate = 0;
	memset(&ShopClanAddMembers_GP, 0, sizeof(ShopClanAddMembers_GP));
	memset(&ShopClanAddMembers_Num, 0, sizeof(ShopClanAddMembers_Num));
}

CClientUserProfile::~CClientUserProfile()
{
	SAFE_DELETE(friends);

	for(int i=0; i<wiUserProfile::MAX_LOADOUT_SLOTS; i++)
		SAFE_DELETE(clans[i]);
}

void CClientUserProfile::GenerateSessionKey(char* outKey)
{
	char sessionInfo[128];
	sprintf(sessionInfo, "%d:%d", CustomerID, SessionID);
	for(size_t i=0; i<strlen(sessionInfo); ++i)
		sessionInfo[i] = sessionInfo[i]^0x64;
	
	CkString str;
	str = sessionInfo;
	str.base64Encode("utf-8");
	
	strcpy(outKey, str.getUtf8());
	return;
}

// special code that'll replace name/description/icon for specified item
template <class T>
static void replaceItemNameParams(T* itm, pugi::xml_node& xmlNode)
{
	const char* name = xmlNode.attribute("name").value();
	const char* desc = xmlNode.attribute("desc").value();
	const char* fname = xmlNode.attribute("fname").value();
	
	// replace description
	if(strcmp(desc, itm->m_Description) != 0)
	{
		free(itm->m_Description);
		free(itm->m_DescriptionW);

		itm->m_Description = strdup(desc);
		itm->m_DescriptionW = wcsdup(utf8ToWide(itm->m_Description));
	}
	
	// replace name
	if(strcmp(name, itm->m_StoreName) != 0)
	{
		free(itm->m_StoreName);
		free(itm->m_StoreNameW);

		itm->m_StoreName = strdup(name);
		itm->m_StoreNameW = wcsdup(utf8ToWide(itm->m_StoreName));
	}
	
	// replace store icon (FNAME)
	char storeIcon[256];
	sprintf(storeIcon, "$Data/Weapons/StoreIcons/%s.dds", fname);
	if(strcmp(storeIcon, itm->m_StoreIcon) != 0)
	{
		free(itm->m_StoreIcon);
		itm->m_StoreIcon = strdup(storeIcon);
	}
}

int CClientUserProfile::ApiGetItemsInfo()
{
	CWOBackendReq req(this, "api_GetItemsInfo.aspx");
	if(!req.Issue())
	{
		r3dOutToLog("GetItemsInfo FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	
	pugi::xml_node xmlItems = xmlFile.child("items");

	// read gears (in <gears><g>...)
	pugi::xml_node xmlNode = xmlItems.child("gears").first_child();
	while(!xmlNode.empty())
	{
		uint32_t itemId = xmlNode.attribute("ID").as_uint();
		GearConfig* gc = const_cast<GearConfig*>(g_pWeaponArmory->getGearConfig(itemId));
		if(gc)
		{
			gc->m_Weight        = xmlNode.attribute("wg").as_float() / 1000.0f;
			gc->m_damagePerc    = xmlNode.attribute("dp").as_float() / 100.0f;
			gc->m_damageMax     = xmlNode.attribute("dm").as_float();
		}

		xmlNode = xmlNode.next_sibling();
	}

	// read weapons (in <weapons><w>...)
	xmlNode = xmlItems.child("weapons").first_child();
	while(!xmlNode.empty())
	{
		uint32_t itemId = xmlNode.attribute("ID").as_uint();
		WeaponConfig* wc = const_cast<WeaponConfig*>(g_pWeaponArmory->getWeaponConfig(itemId));
		if(wc)
		{
			wc->m_AmmoDamage    = xmlNode.attribute("d1").as_float();
			wc->m_AmmoDecay     = xmlNode.attribute("d2").as_float();
			wc->m_fireDelay     = 60.0f / (xmlNode.attribute("rf").as_float());
			wc->m_spread        = xmlNode.attribute("sp").as_float();
			wc->m_recoil        = xmlNode.attribute("rc").as_float();
		}

		xmlNode = xmlNode.next_sibling();
	}

	// read packages(in <packages><p>...)
	/*xmlNode = xmlItems.child("packages").first_child();
	while(!xmlNode.empty())
	{
		uint32_t itemId = xmlNode.attribute("ID").as_uint();
		PackageConfig* pc = const_cast<PackageConfig*>(g_pWeaponArmory->getPackageConfig(itemId));
		if(pc)
		{
			replaceItemNameParams<PackageConfig>(pc, xmlNode);

			pc->m_addGD = xmlNode.attribute("gd").as_int();
			pc->m_addSP = xmlNode.attribute("sp").as_int();
			pc->m_itemID1    = xmlNode.attribute("i1i").as_int();
			pc->m_itemID1Exp = xmlNode.attribute("i1e").as_int();
			pc->m_itemID2    = xmlNode.attribute("i2i").as_int();
			pc->m_itemID2Exp = xmlNode.attribute("i2e").as_int();
			pc->m_itemID3    = xmlNode.attribute("i3i").as_int();
			pc->m_itemID3Exp = xmlNode.attribute("i3e").as_int();
			pc->m_itemID4    = xmlNode.attribute("i4i").as_int();
			pc->m_itemID4Exp = xmlNode.attribute("i4e").as_int();
			pc->m_itemID5    = xmlNode.attribute("i5i").as_int();
			pc->m_itemID5Exp = xmlNode.attribute("i5e").as_int();
			pc->m_itemID6    = xmlNode.attribute("i6i").as_int();
			pc->m_itemID6Exp = xmlNode.attribute("i6e").as_int();
		}

		xmlNode = xmlNode.next_sibling();
	}*/

	// read mystery crates/loot boxes names
	xmlNode = xmlItems.child("generics").first_child();
	while(!xmlNode.empty())
	{
		uint32_t itemId = xmlNode.attribute("ID").as_uint();
		ModelItemConfig* itm = const_cast<ModelItemConfig*>(g_pWeaponArmory->getItemConfig(itemId));
		if(itm)
		{
			replaceItemNameParams<ModelItemConfig>(itm, xmlNode);
		}

		xmlNode = xmlNode.next_sibling();
	}

	return 0;
}

int CClientUserProfile::ApiCharCreate(const char* Gamertag, int Hardcore, int HeroItemID, int HeadIdx, int BodyIdx, int LegsIdx)
{
	CWOBackendReq req(this, "api_CharSlots.aspx");
	req.AddParam("func",       "create");
	req.AddParam("Gamertag",   Gamertag);
	req.AddParam("Hardcore",   Hardcore);
	req.AddParam("HeroItemID", HeroItemID);
	req.AddParam("HeadIdx",    HeadIdx);
	req.AddParam("BodyIdx",    BodyIdx);
	req.AddParam("LegsIdx",    LegsIdx);
	if(!req.Issue())
	{
		r3dOutToLog("ApiCharCreate failed: %d", req.resultCode_);
		return req.resultCode_;
	}

	// reread profile
	GetProfile();

	return 0;
}

int CClientUserProfile::ApiCharDelete()
{
	r3d_assert(SelectedCharID >= 0 && SelectedCharID < wiUserProfile::MAX_LOADOUT_SLOTS);
	wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];
	r3d_assert(w.LoadoutID > 0);

	CWOBackendReq req(this, "api_CharSlots.aspx");
	req.AddParam("func",   "delete");
	req.AddParam("CharID", w.LoadoutID);
	if(!req.Issue())
	{
		r3dOutToLog("ApiCharDelete failed: %d", req.resultCode_);
		return req.resultCode_;
	}

	w.LoadoutID = 0;

	if(GetProfile() != 0)
		return false;

	return 0;
}

int CClientUserProfile::ApiCharRevive()
{
	r3d_assert(SelectedCharID >= 0 && SelectedCharID < wiUserProfile::MAX_LOADOUT_SLOTS);
	wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];
	r3d_assert(w.LoadoutID > 0);

	CWOBackendReq req(this, "api_CharSlots.aspx");
	req.AddParam("func",   "revive");
	req.AddParam("CharID", w.LoadoutID);

// Health Revive skill
	req.AddParam("Health", "100");
	if(!req.Issue())
	{
		r3dOutToLog("ApiCharRevive failed: %d", req.resultCode_);
		return req.resultCode_;
	}
		r3dOutToLog("Revive Character Health 100\n");

	// reread profile
	GetProfile();
r3dOutToLog("Revive Character\n");
	
	return 0;
}

int CClientUserProfile::ApiBackpackToInventory(int GridFrom, int amount)
{
	r3d_assert(SelectedCharID >= 0 && SelectedCharID < wiUserProfile::MAX_LOADOUT_SLOTS);
	wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];
	r3d_assert(w.LoadoutID > 0);

	r3d_assert(GridFrom >= 0 && GridFrom < w.BackpackSize);
	wiInventoryItem* wi1 = &w.Items[GridFrom];
	r3d_assert(wi1->InventoryID > 0);
	r3d_assert(wi1->quantity >= amount);
	
	// scan for inventory and see if we can stack item there
	__int64 InvInventoryID = 0;
	bool isStackable = storecat_IsItemStackable(wi1->itemID);
	for(uint32_t i=0; i<ProfileData.NumItems; i++)
	{
		// can stack only non-modified items
		const wiInventoryItem* wi2 = &ProfileData.Inventory[i];
		if(isStackable && wi2->itemID == wi1->itemID && wi1->Var1 < 0 && wi2->Var1 < 0)
		{
			InvInventoryID = wi2->InventoryID;
			break;
		}
	}
	char strInventoryID[128];
	sprintf(strInventoryID, "%I64d", InvInventoryID);

	CWOBackendReq req(this, "api_CharBackpack.aspx");
	req.AddParam("CharID", w.LoadoutID);
	req.AddParam("op",     10);		// inventory operation code
	req.AddParam("v1",     strInventoryID);	// value 1
	req.AddParam("v2",     GridFrom);	// value 2
	req.AddParam("v3",     amount);		// value 3
	if(!req.Issue())
	{
		r3dOutToLog("ApiBackpackToInventory failed: %d", req.resultCode_);
		return req.resultCode_;
	}
	
	__int64 InventoryID = 0;
	int nargs = sscanf(req.bodyStr_, "%I64d", &InventoryID);
	r3d_assert(nargs == 1);
	r3d_assert(InventoryID > 0);

	// add one item to inventory
	wiInventoryItem* wi2 = getInventorySlot(InventoryID);
	if(wi2 == NULL) 
	{
		// add new inventory slot with same vars
		wi2 = &ProfileData.Inventory[ProfileData.NumItems++];
		r3d_assert(ProfileData.NumItems < wiUserProfile::MAX_INVENTORY_SIZE);
		
		*wi2 = *wi1;
		wi2->InventoryID = InventoryID;
		wi2->quantity    = amount;
	}
	else
	{
		wi2->quantity += amount;
	}
	
	// remove item
	wi1->quantity -= amount;
	if(wi1->quantity <= 0)
		wi1->Reset();
	
	return 0;
}

int CClientUserProfile::ApiBackpackFromInventory(__int64 InventoryID, int GridTo, int amount)
{
	r3d_assert(SelectedCharID >= 0 && SelectedCharID < wiUserProfile::MAX_LOADOUT_SLOTS);
	wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];
	r3d_assert(w.LoadoutID > 0);

	wiInventoryItem* wi1 = getInventorySlot(InventoryID);
	r3d_assert(wi1);
	r3d_assert(wi1->quantity >= amount);

	int idx_free   = -1;
	int idx_exists = -1;

	bool isStackable = storecat_IsItemStackable(wi1->itemID);
	
	// search for free or existing slot
	if(GridTo >= 0)
	{
		// placing to free slot
		if(w.Items[GridTo].InventoryID == 0)
		{
			idx_free = GridTo;
		}
		else if(isStackable && w.Items[GridTo].itemID == wi1->itemID && wi1->Var1 < 0 && w.Items[GridTo].Var1 < 0)
		{
			idx_exists = GridTo;
		}
		else
		{
			// trying to stack not stackable item
			return 9;
		}
	}
	else
	{
		// search for same or free slot
		for(int i=wiCharDataFull::CHAR_REAL_BACKPACK_IDX_START; i<w.BackpackSize; i++)
		{
			// can stack only non-modified items
			if(isStackable && w.Items[i].itemID == wi1->itemID && wi1->Var1 < 0 && w.Items[i].Var1 < 0)
			{
				idx_exists = i;
				break;
			}
			if(w.Items[i].itemID == 0 && idx_free == -1) 
			{
				idx_free = i;
			}
		}
		if(idx_free == -1 && idx_exists == -1)
		{
			r3dOutToLog("ApiBackpackFromInventory - no free slots");
			return 6;
		}
	}
	GridTo = idx_exists != -1 ? idx_exists : idx_free;
	r3d_assert(GridTo != -1);
	
	char strInventoryID[128];
	sprintf(strInventoryID, "%I64d", InventoryID);

	CWOBackendReq req(this, "api_CharBackpack.aspx");
	req.AddParam("CharID", w.LoadoutID);
	req.AddParam("op",     11);		// inventory operation code
	req.AddParam("v1",     strInventoryID);	// value 1
	req.AddParam("v2",     GridTo);		// value 2
	req.AddParam("v3",     amount);		// value 3
	if(!req.Issue())
	{
		r3dOutToLog("ApiBackpackFromInventory failed: %d", req.resultCode_);
		return req.resultCode_;
	}

	// get new inventory id
	int nargs = sscanf(req.bodyStr_, "%I64d", &InventoryID);
	r3d_assert(nargs == 1);
	r3d_assert(InventoryID > 0);

	if(idx_exists != -1) 
	{
		r3d_assert(w.Items[idx_exists].InventoryID == InventoryID);
		w.Items[idx_exists].quantity += amount;
	} 
	else 
	{
		w.Items[idx_free] = *wi1;
		w.Items[idx_free].InventoryID = InventoryID;
		w.Items[idx_free].quantity    = amount;
	}
	
	wi1->quantity -= amount;
	if(wi1->quantity <= 0)
		wi1->Reset();
		
	return 0;
}

int CClientUserProfile::ApiBackpackGridSwap(int GridFrom, int GridTo)
{
	r3d_assert(SelectedCharID >= 0 && SelectedCharID < wiUserProfile::MAX_LOADOUT_SLOTS);
	wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];
	r3d_assert(w.LoadoutID > 0);

	// check if we can join both slots
	r3d_assert(GridFrom >= 0 && GridFrom < w.BackpackSize);
	r3d_assert(GridTo >= 0 && GridTo < w.BackpackSize);
	wiInventoryItem& wi1 = w.Items[GridFrom];
	wiInventoryItem& wi2 = w.Items[GridTo];
	if(wi1.itemID && wi1.itemID == wi2.itemID)
	{
		if(storecat_IsItemStackable(wi1.itemID) && wi1.Var1 < 0 && wi2.Var1 < 0)
		{
			return ApiBackpackGridJoin(GridFrom, GridTo);
		}
	}
	
	CWOBackendReq req(this, "api_CharBackpack.aspx");
	req.AddParam("CharID", w.LoadoutID);
	req.AddParam("op",     12);		// inventory operation code
	req.AddParam("v1",     GridFrom);	// value 1
	req.AddParam("v2",     GridTo);		// value 2
	req.AddParam("v3",     0);		// value 3
	if(!req.Issue())
	{
		r3dOutToLog("ApiBackpackGridMove failed: %d", req.resultCode_);
		return req.resultCode_;
	}
	
	R3D_SWAP(wi1, wi2);
	
	return 0;
}

int CClientUserProfile::ApiBackpackGridJoin(int GridFrom, int GridTo)
{
	r3d_assert(SelectedCharID >= 0 && SelectedCharID < wiUserProfile::MAX_LOADOUT_SLOTS);
	wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];
	r3d_assert(w.LoadoutID > 0);

	r3d_assert(GridFrom >= 0 && GridFrom < w.BackpackSize);
	r3d_assert(GridTo >= 0 && GridTo < w.BackpackSize);
	wiInventoryItem& wi1 = w.Items[GridFrom];
	wiInventoryItem& wi2 = w.Items[GridTo];
	r3d_assert(wi1.itemID && wi1.itemID == wi2.itemID);
	
	CWOBackendReq req(this, "api_CharBackpack.aspx");
	req.AddParam("CharID", w.LoadoutID);
	req.AddParam("op",     13);		// inventory operation code
	req.AddParam("v1",     GridFrom);	// value 1
	req.AddParam("v2",     GridTo);		// value 2
	req.AddParam("v3",     0);		// value 3
	if(!req.Issue())
	{
		r3dOutToLog("ApiBackpackGridJoin failed: %d", req.resultCode_);
		return req.resultCode_;
	}
	
	wi2.quantity += wi1.quantity;
	wi1.Reset();
	
	return 0;
}


int CClientUserProfile::ApiChangeBackpack(__int64 InventoryID)
{
	r3d_assert(SelectedCharID >= 0 && SelectedCharID < wiUserProfile::MAX_LOADOUT_SLOTS);
	wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];
	r3d_assert(w.LoadoutID > 0);

	// no need to validate InventoryID - server will do that
	char strInventoryID[128];
	sprintf(strInventoryID, "%I64d", InventoryID);

	CWOBackendReq req(this, "api_CharBackpack.aspx");
	req.AddParam("CharID", w.LoadoutID);
	req.AddParam("op",     16);		// inventory operation code
	req.AddParam("v1",     strInventoryID);	// value 1
	req.AddParam("v2",     0);
	req.AddParam("v3",     0);
	if(!req.Issue())
	{
		r3dOutToLog("ApiChangeBackpack failed: %d", req.resultCode_);
		return req.resultCode_;
	}

	// reread profile, as inventory/backpack is changed
	GetProfile();
	
	return 0;
}
int CClientUserProfile::ApiChangeOutfit(int headIdx, int bodyIdx, int legsIdx)
{
    r3d_assert(SelectedCharID >= 0 && SelectedCharID < wiUserProfile::MAX_LOADOUT_SLOTS);
    wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];
    r3d_assert(w.LoadoutID > 0);

    char headStr[128], bodyStr[128], legsStr[128];
    sprintf(headStr, "%d", headIdx);
    sprintf(bodyStr, "%d", bodyIdx);
    sprintf(legsStr, "%d", legsIdx);

    CWOBackendReq req(this, "api_CharOutfit.aspx");
    req.AddParam("CharID", w.LoadoutID);
    req.AddParam("HeadIdx",     headStr);
    req.AddParam("BodyIdx",     bodyStr);
    req.AddParam("LegsIdx",     legsStr);
    if(!req.Issue())
    {
        r3dOutToLog("ApiChangeOutfit failed: %d", req.resultCode_);
        return req.resultCode_;
    }

    GetProfile(w.LoadoutID);
    // Here needs to be added something that properly updates
    // your survivors, otherwise when you swap survivor and back
    // the new outfit is not applied in the menu :S
    
    return 0;
}
int CClientUserProfile::ApiBuyItem(int itemId, int buyIdx, __int64* out_InventoryID)
{
	r3d_assert(buyIdx > 0);

	CWOBackendReq req(this, "api_BuyItem3.aspx");
	req.AddParam("ItemID", itemId);
	req.AddParam("BuyIdx", buyIdx);
	if(!req.Issue())
	{
		r3dOutToLog("BuyItem FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	int balance = 0;
	int nargs = sscanf(req.bodyStr_, "%d %I64d", &balance, out_InventoryID);
	if(nargs != 2)
	{
		r3dError("wrong answer for ApiBuyItem");
		return 9;
	}

	// update balance
	if(buyIdx >= 5 && buyIdx <= 8)
		ProfileData.GameDollars = balance;
	else
		ProfileData.GamePoints  = balance;

	return 0;
}

//
//
// Steam part of the code
//
//
#include "steam_api_dyn.h"

class CSteamClientCallbacks
{
  public:
	STEAM_CALLBACK( CSteamClientCallbacks, OnMicroTxnAuth, MicroTxnAuthorizationResponse_t, m_MicroTxnAuth);

	CSteamClientCallbacks() : 
		m_MicroTxnAuth(this, &CSteamClientCallbacks::OnMicroTxnAuth)
	{
	}
};

void CSteamClientCallbacks::OnMicroTxnAuth(MicroTxnAuthorizationResponse_t *pCallback)
{
	gUserProfile.steamAuthResp.gotResp     = true;
	gUserProfile.steamAuthResp.bAuthorized = pCallback->m_bAuthorized;
	gUserProfile.steamAuthResp.ulOrderID   = pCallback->m_ulOrderID;
}

void CClientUserProfile::RegisterSteamCallbacks()
{
	r3d_assert(steamCallbacks == NULL);
	steamCallbacks = new CSteamClientCallbacks();
}

void CClientUserProfile::DeregisterSteamCallbacks()
{
	SAFE_DELETE(steamCallbacks);
}

int CClientUserProfile::ApiSteamGetShop()
{
	steamGPShopData.Clear();

	CWOBackendReq req(this, "api_SteamBuyGP.aspx");
	req.AddParam("func", "shop");

	if(!req.Issue())
	{
		r3dOutToLog("ApiSteamGetShop FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	
	pugi::xml_node xmlItem = xmlFile.child("SteamGPShop").first_child();
	while(!xmlItem.empty())
	{
		SteamGPShop_s d;
		d.gpItemID = xmlItem.attribute("ID").as_uint();
		d.GP       = xmlItem.attribute("GP").as_uint();
		d.BonusGP  = xmlItem.attribute("BonusGP").as_uint();
		d.PriceUSD = xmlItem.attribute("Price").as_uint();
		steamGPShopData.PushBack(d);

		xmlItem = xmlItem.next_sibling();
	}
	
	return 0;
}

int CClientUserProfile::ApiSteamStartBuyGP(int gpItemId)
{
	r3d_assert(gSteam.steamID);
	steamAuthResp.gotResp = false;
	
	char	strSteamId[1024];
	sprintf(strSteamId, "%I64u", gSteam.steamID);

	CWOBackendReq req(this, "api_SteamBuyGP.aspx");
	req.AddParam("func", "auth");
	req.AddParam("steamId",  strSteamId);
	req.AddParam("gpItemId", gpItemId);
	req.AddParam("country",  gSteam.country);

	if(!req.Issue())
	{
		r3dOutToLog("ApiSteamStartBuyGP FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	return 0;
}

int CClientUserProfile::ApiSteamFinishBuyGP(__int64 orderId)
{
	char	strOrderId[1024];
	sprintf(strOrderId, "%I64d", orderId);

	CWOBackendReq req(this, "api_SteamBuyGP.aspx");
	req.AddParam("func", "fin");
	req.AddParam("orderId", strOrderId);

	if(!req.Issue())
	{
		r3dOutToLog("ApiSteamFinishBuyGP FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	// update balance
	int balance = 0;
	int nargs = sscanf(req.bodyStr_, "%d", &balance);
	r3d_assert(nargs == 1);
	
	ProfileData.GamePoints = balance;
	return 0;
}

//
// friends APIs
//
int CClientUserProfile::ApiFriendAddReq(const char* gamertag, int* outFriendStatus)
{
	CWOBackendReq req(this, "api_Friends.aspx");
	req.AddParam("func", "addReq");
	req.AddParam("name", gamertag);

	if(!req.Issue())
	{
		r3dOutToLog("ApiFriendAddReq FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	int friendStatus;
	int nargs = sscanf(req.bodyStr_, "%d", &friendStatus);
	r3d_assert(nargs == 1);
	*outFriendStatus = friendStatus;
	
	return 0;
}

int CClientUserProfile::ApiFriendAddAns(DWORD friendId, bool allow)
{
	CWOBackendReq req(this, "api_Friends.aspx");
	req.AddParam("func", "addAns");
	req.AddParam("FriendID", friendId);
	req.AddParam("Allow", allow ? "1" : "0");

	if(!req.Issue())
	{
		r3dOutToLog("ApiFriendAddAns FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	
	return 0;
}

int CClientUserProfile::ApiFriendRemove(DWORD friendId)
{
	CWOBackendReq req(this, "api_Friends.aspx");
	req.AddParam("func", "remove");
	req.AddParam("FriendID", friendId);

	if(!req.Issue())
	{
		r3dOutToLog("ApiFriendRemove FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	
	return 0;
}

int CClientUserProfile::ApiGetTransactions()
{
	CWOBackendReq req(this, "api_GetTransactions.aspx");
	req.AddParam("CustomerID", CustomerID);
	if(!req.Issue())
	{
		r3dOutToLog("ApiGetTransactions FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}

	tscount = 0;
	m_tsData.reserve(1024*1024);
	m_tsData.clear();


	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	
	pugi::xml_node xml = xmlFile.child("trans");

	pugi::xml_node xmlItem = xml.first_child();
	while(!xmlItem.empty())
	{
		TSEntry_s ts;
		r3dscpy(ts.type, xmlItem.attribute("TId").value());
		r3dscpy(ts.time, xmlItem.attribute("Time").value());
		ts.itemID = xmlItem.attribute("ItemID").as_int();
		ts.amount = xmlItem.attribute("amount").as_float();
        ts.balance = xmlItem.attribute("bl").as_float();
		ts.id = xmlItem.attribute("id").as_int();
		m_tsData.push_back(ts);

		tscount++;
		xmlItem = xmlItem.next_sibling();
	}

	return 0;
}

int CClientUserProfile::ApiFriendGetStats(DWORD friendId)
{
	CWOBackendReq req(this, "api_Friends.aspx");
	req.AddParam("func", "stats");
	req.AddParam("FriendID", friendId);

	if(!req.Issue())
	{
		r3dOutToLog("ApiFriendGetStats FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	
	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	friends->SetCurrentStats(xmlFile);
	
	return 0;
}

int CClientUserProfile::ApiGetLeaderboard(int hardcore, int type, int page, int& out_startPos, int& out_pageCount)
{
	r3d_assert(type >= 0 && type <= 6);

	CWOBackendReq req(this, "api_LeaderboardGet.aspx");
	req.AddParam("Type", type);
	req.AddParam("Hardcore", hardcore);
	req.AddParam("Page", page);

	if(!req.Issue())
	{
		r3dOutToLog("ApiGetLeaderboard FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	
	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	
	pugi::xml_node xmlLeaderboard = xmlFile.child("leaderboard");
	out_startPos = xmlLeaderboard.attribute("pos").as_int();
	out_pageCount = xmlLeaderboard.attribute("pc").as_int();

	m_lbData[type].reserve(100);
	m_lbData[type].clear();

	pugi::xml_node xmlItem = xmlLeaderboard.first_child();
	while(!xmlItem.empty())
	{
		LBEntry_s lb;
		r3dscpy(lb.gamertag, xmlItem.attribute("gt").value());
		lb.alive = xmlItem.attribute("a").as_int();
		lb.data = xmlItem.attribute("d").as_int();
		lb.ClanId = xmlItem.attribute("cid").as_int();

		m_lbData[type].push_back(lb);

		xmlItem = xmlItem.next_sibling();
	}
	
	return 0;
}

int CClientUserProfile::ApiMysteryBoxGetContent(int itemId, const MysteryBox_s** out_box)
{
	*out_box = NULL;

	// see if we already have that box
	for(size_t i=0, size=mysteryBoxes_.size(); i<size; i++) {
		if(mysteryBoxes_[i].ItemID == itemId) {
			*out_box = &mysteryBoxes_[i];
			return 0;
		}
	}

	CWOBackendReq req(this, "api_MysteryBox.aspx");
	req.AddParam("func", "info");
	req.AddParam("LootID", itemId);
	if(!req.Issue())
	{
		r3dOutToLog("ApiMysteryBoxGetContent FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	
	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	
	// save data for new mystery box
	MysteryBox_s box;
	box.ItemID = itemId;
	
	pugi::xml_node xmlItem = xmlFile.child("box").first_child();
	while(!xmlItem.empty())
	{
		MysteryLoot_s loot;
		loot.ItemID = xmlItem.attribute("ID").as_uint();
		if(loot.ItemID == 0) {
			loot.GDMin = xmlItem.attribute("v1").as_uint();
			loot.GDMax = xmlItem.attribute("v2").as_uint();
		} else {
			loot.ExpDaysMin = xmlItem.attribute("v1").as_uint();
			loot.ExpDaysMax = xmlItem.attribute("v2").as_uint();
		}

		box.content.push_back(loot);
		xmlItem = xmlItem.next_sibling();
	}
	
	mysteryBoxes_.push_back(box);
	*out_box = &mysteryBoxes_.back();
	return 0;
}
int CClientUserProfile::ApiBan()
{
	CWOBackendReq req(this, "api_BanPlayer.aspx");
	req.AddParam("CustomerID", CustomerID);
	if(!req.Issue())
	{
		return 50;
	}
	GetProfile();
	return 0;
}
int CClientUserProfile::ApiChangeName(const char* Name)
{
	wiCharDataFull& w = ProfileData.ArmorySlots[SelectedCharID];


	CWOBackendReq req(this, "api_ChangeName.aspx");
	req.AddParam("CustomerID", CustomerID);
	req.AddParam("Name", Name);
	req.AddParam("CharID", w.LoadoutID);
	if(!req.Issue())
	{
		return 50;
	}
	GetProfile();
	return 0;
}
int CClientUserProfile::ApiBuyPremium()
{
	CWOBackendReq req(this, "api_BuyPremium.aspx");
	req.AddParam("CustomerID", CustomerID);
	if(!req.Issue())
	{
		r3dOutToLog("ApiBuyPremium FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	GetProfile();
	return 0;
}
int CClientUserProfile::ApiConvertGCToGD(int currentvalue,int convertvalue)
{
	GetProfile();
	CWOBackendReq req(this, "api_ConvertM.aspx");
	req.AddParam("CustomerID", CustomerID);
	req.AddParam("Var1", currentvalue);
	req.AddParam("Var2", convertvalue);
	if(!req.Issue())
	{
		r3dOutToLog("ApiConvertGCToGD FAILED, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	GetProfile();
	return 0;
}
int CClientUserProfile::ApiLearnSkill(uint32_t skillid, int CharID)
{
	CWOBackendReq req(this, "api_SrvSkills.aspx");
	req.AddParam("func", "add");
	req.AddParam("CharID", CharID);
	req.AddParam("SkillID", skillid);
	if(!req.Issue())
	{
		return 50;
	}
	GetProfile();
	return skillid;
}

#endif // ! WO_SERVER
