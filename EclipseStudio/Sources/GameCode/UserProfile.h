#pragma once
#include "resource.h"
#include "./../../src/External/HShield/Include/HShield.h"
#include "./../../src/External/HShield/Include/HSUpChk.h"
#pragma comment(lib,"HShield.lib")
#pragma comment(lib,"HSUpChk.lib")
#include <assert.h>
#include <stdio.h>
#include <winsock2.h>
#include <process.h>
/// STATS TRACKING ///
//int rRet2 = _AhnHS_VerifyProtectedFunction(); 
struct wiStatsTracking
{
// NOTE: you MUST increase P2PNET_VERSION if you change this structure
	int RewardID;

	int GD; // game dollars
	int GP; // game points
	int XP; // XP
	wiStatsTracking& operator+=(const wiStatsTracking& rhs) 
	{ 
		XP+=rhs.XP;
		GP+=rhs.GP; 
		GD+=rhs.GD;
		return *this;
	}
	wiStatsTracking() { memset(this, 0, sizeof(*this)); }
	wiStatsTracking(int rewardId, int gd, int gp, int xp)
		: RewardID(rewardId), XP(xp), GP(gp), GD(gd) {}
};

// Items categories
// if you make any changes, please update STORE_CATEGORIES_NAMES in UserProfile.cpp
enum STORE_CATEGORIES
{
	storecat_INVALID	= 0,
	storecat_Account	= 1,
	storecat_Boost		= 2,
	storecat_LootBox	= 7,

	storecat_Armor		= 11,
	storecat_Backpack	= 12,	
	storecat_Helmet		= 13,
	storecat_HeroPackage	= 16,

	storecat_FPSAttachment  = 19,

	storecat_ASR		= 20,	// Assault Rifles
	storecat_SNP		= 21,	// Sniper rifles
	storecat_SHTG		= 22,	// Shotguns
	storecat_MG			= 23,	// Machine guns
	storecat_HG			= 25,	// handguns
	storecat_SMG		= 26,	// submachineguns
	storecat_GRENADE	= 27,	// grenades and everything that you can throw. Mines shouldn't be in this group!!!
	storecat_UsableItem  = 28,	// usable items
	storecat_MELEE		 =29,   // melee items (knifes, etc)
	storecat_Food		= 30,	// food 
	storecat_Water		= 33,	// water
	storecat_Vehicle    = 34,   // Server vehicles //Codex Carros
	storecat_punch		= 35,   //Codex Soco
	storecat_CraftRe		= 51,
	storecat_CraftCom		= 50,
	storecat_ShootVehicle    = 51, //Codex Carros
	storecat_ShootAnimal    = 52, //Codex Animal

	storecat_NUM_ITEMS, // should be the last one!!
};
extern const char* STORE_CATEGORIES_NAMES[storecat_NUM_ITEMS];

struct wiStoreItem
{
	uint32_t itemID;

	uint32_t pricePerm;
	uint32_t gd_pricePerm;
	
	bool	isNew;

	bool hasAnyPrice()
	{
		return pricePerm > 0 || gd_pricePerm > 0;
	}
};

static const uint32_t MAX_NUM_STORE_ITEMS = 10000; // fixed at 10000 for now
extern wiStoreItem g_StoreItems[MAX_NUM_STORE_ITEMS]; 
extern uint32_t g_NumStoreItems;

namespace ReputationPoints
{
	enum Enum
	{
		Paragon = 1000,
		Vigilante = 500,
		Guardian = 250,
		Lawman = 80,
		Deputy = 20,
		Constable = 10,
		Civilian = -4,
		Thug = -5,
		Outlaw = -20,
		Bandit = -100,
		Hitman = -300,
		Villain = -600,
		Assassin = -1000,
	};
}

struct wiStats
{
	// character stats
	int		XP;
	int		TimePlayed;
	int		Reputation;
	int		SkillXPPool;

	// generic trackable stats
	int		KilledZombies;	// normal zombie kills
	int		KilledSurvivors;
	int		KilledBandits;

	// not used stats, here for fun.
	int		Kills;
	int		Deaths;
	int		ShotsFired;
	int		ShotsHits;
	int		skillid0, skillid1, skillid2, skillid3, skillid4, skillid5, skillid6, skillid7,
			skillid8, skillid9, skillid10, skillid11, skillid12, skillid13, skillid14, skillid15,
			skillid16, skillid17, skillid18, skillid19, skillid20, skillid21, skillid22, skillid23,
			skillid24, skillid25, skillid26, skillid27, skillid28, skillid29, skillid30, skillid31,
			skillid32, skillid33;
	wiStats()
	{
		memset(this, 0, sizeof(*this));
	}
};

enum WeaponAttachmentTypeEnum
{
	WPN_ATTM_INVALID=-1,
	WPN_ATTM_MUZZLE=0,
	WPN_ATTM_UPPER_RAIL=1,
	WPN_ATTM_LEFT_RAIL=2,
	WPN_ATTM_BOTTOM_RAIL=3,
	WPN_ATTM_CLIP=4,
	WPN_ATTM_RECEIVER=5, // not visual
	WPN_ATTM_STOCK=6, // not visual
	WPN_ATTM_BARREL=7, // not visual
	WPN_ATTM_MAX
};

struct wiWeaponAttachment
{
	uint32_t attachments[WPN_ATTM_MAX];
	wiWeaponAttachment() { Reset(); }
	void Reset() {memset(this, 0, sizeof(*this)); }

	bool operator==(const wiWeaponAttachment& rhs)
	{
		bool res = true;
		for(int i=0; i<WPN_ATTM_MAX; ++i)
			res = res && (attachments[i]==rhs.attachments[i]);
		return res;
	}
	bool operator!=(const wiWeaponAttachment& rhs) { return !((*this)==rhs); }
};

struct wiInventoryItem
{
	__int64		InventoryID;	// unique identifier for that inventory slot
	uint32_t	itemID;		// ItemID inside that slot
	int		quantity;
	int		Var1;		// inventory specific vars. -1 for default
	int		Var2;		//
	
  public:
	wiInventoryItem() 
	{
		Reset();
	}
	
	void		Reset()
	{
		InventoryID = 0;
		itemID   = 0;
		quantity = 0;
		Var1     = -1;
		Var2     = -1;
	}

	bool operator==(const wiInventoryItem& rhs) const
	{
		if(InventoryID != rhs.InventoryID) return false;
		if(itemID != rhs.itemID) return false;
		if(quantity != rhs.quantity) return false;
		if(Var1 != rhs.Var1) return false;
		if(Var2 != rhs.Var2) return false;
		return true;
	}
	bool operator!=(const wiInventoryItem& rhs) const { return !((*this)==rhs); }
};

struct wiCharDataFull
{
// NOTE: you MUST increase P2PNET_VERSION if you change this structure
	uint32_t	LoadoutID;

	// character stats
	char		Gamertag[32*2];
	int		Hardcore;

	// defined on char creation, can't be modified
	uint32_t	HeroItemID;		// ItemID of base character
	int		HeadIdx;
		DWORD		CustomerID; // Group Loadout CustomerID
	int		BodyIdx;
	int		LegsIdx;
	uint32_t	BackpackID; // itemID of backpack to render

	// vars that is used only on client/server
	int		Alive;	// 0 - dead, 1 - alive, 2 - revived, 3 - new character
	__int64		DeathUtcTime;
	int		SecToRevive;
	float		Health; // 0..100; 0-dead. 100 - healthy
	float		Hunger; // 0..100; 0-not hungry, 100 - starving
	float		Thirst; // 0..100; 0-not thirsty, 100 - super thirsty!
	float		Toxic; // 0..100; 0-no toxic, 100 - high toxicity, slowly dying
 int bleeding;
 int legfall;
	enum
	{
	  GAMEFLAG_NearPostBox = (1 << 0),
	  GAMEFLAG_isSpawnProtected = (1 << 1),//Mateuus Edit
	  GAMEFLAG_RadioactiveArea = (1 << 2), //Codex Radioactive
	};

	// current game data
	int		GameMapId;
char		fromgamertag[128];
char		groupgamertag[128];
	bool isInvite;
	bool isVisible;
	bool isGroupShow;
	//bool isTrade;
	bool isVoipShow;
	int Mission1; // 0 = not accpet 1 = PICKED UP ITEM  2 = KILL 5 ZOMBIE 3 = Complete
	int Mission1ZKill;
	bool PKT_S2C_CreatePlayerisTele;
	bool isTradeReq;
	//bool isDisconnect;
	//bool isSend;
	//int ZomStatus;
//	bool isgroup1;
//	bool isgroup2;
//	bool isgroup3;
//	bool isgroup4;
//	bool isgroup5;
	bool isInvitePending;
	int GroupID2;
	DWORD		GameServerId;
	r3dPoint3D	GamePos;
	float		GameDir;
	DWORD		GameFlags;
#ifdef WO_SERVER
    int distOfLastUpdateTime;
	DWORD OldGameServerId;
#endif
	wiStats		Stats;

	// clan info
	int		ClanID;
	int		GroupID;
	bool		IsCallForHelp;
	int		ClanRank;
	char		ClanTag[5*2]; //utf8
	int		ClanTagColor;

	// backpack content, including loadout
	enum {
	  // indices of equipped items inside backpack
	   CHAR_LOADOUT_WEAPON1   = 0,
	  CHAR_LOADOUT_WEAPON2,
	  CHAR_LOADOUT_ITEM1,
	  CHAR_LOADOUT_ITEM2,
	  CHAR_LOADOUT_ITEM3,
	  CHAR_LOADOUT_ITEM4,
	  CHAR_LOADOUT_ARMOR,
	  CHAR_LOADOUT_HEADGEAR,
	  CHAR_REAL_BACKPACK_IDX_START,
	  
	  CHAR_MAX_BACKPACK_SIZE = 64 + CHAR_REAL_BACKPACK_IDX_START
	};
	int		BackpackSize;
	wiInventoryItem Items[CHAR_MAX_BACKPACK_SIZE];

	// installed attachments for weapon.
	wiWeaponAttachment Attachment[2];
	
	wiCharDataFull()
	{
		memset(this, 0, sizeof(*this));
	}

	bool hasItem(uint32_t itemID)
	{
		for(int i=0; i<BackpackSize; ++i)
			if(Items[i].itemID == itemID && Items[i].quantity > 0)
				return true;
		return false;
	}

	float getTotalWeight() const;
};

struct wiUserProfile
{
	int		isDevAccount;
	int		isPunisher;
	int WarGuardSession;
	int		isNoDrop;
	  int        isGod;
	  int isPremium;
	int		AccountType; // 0 - legend, 1 - pioneer, 2 - survivor, 3 - guest

	int		GamePoints;
	int		GameDollars;

	enum { MAX_LOADOUT_SLOTS = 5, };
	wiCharDataFull	ArmorySlots[MAX_LOADOUT_SLOTS];
	int		NumSlots;

	enum { MAX_INVENTORY_SIZE = 2048, };
	uint32_t	NumItems;
	wiInventoryItem	Inventory[MAX_INVENTORY_SIZE];
};

class CUserProfile
{
  public:
	wiUserProfile	ProfileData;

	DWORD		CustomerID;
	DWORD		SessionID;
	int		AccountStatus;

	struct tm	ServerTime;
	int		ProfileDataDirty; // seconds after last game update with not closed game session

	int		ShopClanCreate;
	int		ShopClanAddMembers_GP[6];	// price for adding clan members
	int		ShopClanAddMembers_Num[6];	// number of adding members
	void		DeriveGamePricesFromItems();

  public:
	CUserProfile();
	virtual ~CUserProfile();

	int 		GetProfile(int CharID = 0);
	void		 ParseLoadouts(pugi::xml_node& xmlItem);
	void		 ParseInventory(pugi::xml_node& xmlItem);
	void		 ParseBackpacks(pugi::xml_node& xmlItem);

	wiInventoryItem* getInventorySlot(__int64 InventoryID);

	int		ApiGetShopData();
};

#ifndef WO_SERVER	

class CSteamClientCallbacks;
class CUserFriends;
class CUserClans;

class CClientUserProfile : public CUserProfile
{
  public:
	void		GenerateSessionKey(char* outKey);
	
	int		SelectedCharID;	// currently selected INDEX inside of ArmorySlots
	CUserFriends*	friends;

  public:
	CClientUserProfile();
	~CClientUserProfile();
	
	int ApiGetTransactions();
	int		ApiGetItemsInfo();
	int		ApiBuyItem(int itemId, int buyIdx, __int64* out_InventoryID);

	int		ApiCharCreate(const char* Gamertag, int Hardcore, int HeroItemID, int HeadIdx, int BodyIdx, int LegsIdx);
	int		ApiCharDelete();
	int		ApiCharRevive();
//Skillsystem
	int		ApiLearnSkill(uint32_t skillid, int CharID);
	int CClientUserProfile::ApiBuyPremium();
	int CClientUserProfile::ApiConvertGCToGD(int currentvalue,int convertvalue);
	int CClientUserProfile::ApiBuyServerGC(int valor);//BuyServer

	int ApiChangeName(const char* Name);
	int CClientUserProfile::ApiBan();
	// client backpack APIs
	int		ApiBackpackToInventory(int GridFrom, int amount);
	int		ApiBackpackFromInventory(__int64 InventoryID, int GridTo/* or -1 for FREE slot*/, int amount);
	int		ApiBackpackGridSwap(int GridFrom, int GridTo);
	int		ApiBackpackGridJoin(int GridFrom, int GridTo);
	int		ApiChangeBackpack(__int64 InventoryID);
	int        ApiChangeOutfit(int headIdx, int bodyIdx, int legsIdx);

	// friends APIs
	int		ApiFriendAddReq(const char* gamertag, int* outFriendStatus);
	int		ApiFriendAddAns(DWORD friendId, bool allow);
	int		ApiFriendRemove(DWORD friendId);
	int		ApiFriendGetStats(DWORD friendId);
	
	// leaderboard
	struct LBEntry_s
	{
	  char		gamertag[64];
	  int		alive;
	  int		data;
	  int ClanId;
	};

	struct TSEntry_s
	{
	  float amount;
	  int itemID;
	  //int type;
	  char time[128];
	  char type[128];
	  int id;
	  float balance;
	};

	int tscount;
	std::vector<TSEntry_s> m_tsData;
	std::vector<LBEntry_s> m_lbData[7];
	int		ApiGetLeaderboard(int hardcore, int type, int page, int& out_startPos, int& out_pageCount);
	
	// mystery box
	struct MysteryLoot_s
	{
	  uint32_t	ItemID;	// 0 for GD
	  int		GDMin;
	  int		GDMax;
	  int		ExpDaysMin;
	  int		ExpDaysMax;
	};
	struct MysteryBox_s
	{
	  uint32_t	ItemID;
	  std::vector<MysteryLoot_s> content;
	};
	std::vector<MysteryBox_s> mysteryBoxes_;	// we'll live with copy overhead of struct...
	int		ApiMysteryBoxGetContent(int itemId, const MysteryBox_s** out_box);
	
	//
	// Clans API is inside this class
	//
	CUserClans*	clans[wiUserProfile::MAX_LOADOUT_SLOTS];

	//
	// steam APIs
	//
	struct SteamGPShop_s {
		int	gpItemID;
		int	GP;
		int	BonusGP;
		int	PriceUSD;	// in CENTS
	};
	r3dTL::TArray<SteamGPShop_s> steamGPShopData;
	CSteamClientCallbacks* steamCallbacks;
	void		RegisterSteamCallbacks();
	void		DeregisterSteamCallbacks();
	
	int		ApiSteamGetShop();

	struct SteamAuthResp_s {
 	  bool		gotResp;
	  int		bAuthorized;
	  __int64	ulOrderID;
	}		steamAuthResp;
	int		ApiSteamStartBuyGP(int gpItemId);

	int		ApiSteamFinishBuyGP(__int64 orderId);
};

// gUserProfile should be defined only in game mode, server must not use this global
extern  CClientUserProfile gUserProfile;	 
#endif	
