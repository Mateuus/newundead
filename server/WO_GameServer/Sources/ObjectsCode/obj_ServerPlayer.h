#pragma once

#include "GameCommon.h"
#include "Backend/ServerUserProfile.h"
#include "multiplayer/P2PMessages.h"
#include "multiplayer/NetCellMover.h"
#include "NetworkHelper.h"

#define MAX_UAV_TARGETS 8

class Gear;
class ServerWeapon;

enum Playerstate_e
{
PLAYER_INVALID = -1,

	PLAYER_IDLE = 0,
	PLAYER_IDLEAIM,

	PLAYER_MOVE_CROUCH,
	PLAYER_MOVE_CROUCH_AIM,
	PLAYER_MOVE_WALK_AIM,
	PLAYER_MOVE_RUN,
	PLAYER_MOVE_SPRINT,
	PLAYER_SWIM,
	PLAYER_SWIM_M,
	PLAYER_SWIM_F,
	PLAYER_MOVE_PRONE,
	PLAYER_PRONE_AIM,
	PLAYER_PRONE_UP,
	PLAYER_PRONE_DOWN,
	PLAYER_PRONE_IDLE,

	PLAYER_DIE,

	PLAYER_NUM_STATES,
};

class obj_ServerPlayer : public GameObject, INetworkHelper
{
  public:
	DECLARE_CLASS(obj_ServerPlayer, GameObject)

	// info about player
	HANDLE tThread;
	int		peerId_;
	
	bool		wasDisconnected_;

	bool isDestroy;
	bool isFastLoad;
	bool isHighPing;
	float		startPlayTime_;
	float lastObjectLoad;
	float		m_PlayerRotation;
	int		m_PlayerState; // comes from client
	float		m_PlayerFlyingAntiCheatTimer;
	float		m_PlayerUndergroundAntiCheatTimer;
	char		userName[64];

	float LastHackShieldLog;
	float LastWpnLog;
	float		m_Stamina;

 //
 // equipment and loadouts
 //
	enum ESlot
	{
	  SLOT_Armor = 0,
	  SLOT_Headgear,
	  //SLOT_Char, NOT USED FOR NOW
	  SLOT_Max,
	};
	Gear*		gears_[SLOT_Max];

	ServerWeapon*	m_WeaponArray[NUM_WEAPONS_ON_PLAYER];
	int		m_SelectedWeapon;
	bool		m_ForcedEmptyHands;
	
	wiNetWeaponAttm	GetWeaponNetAttachment(int wid);

	float		CalcWeaponDamage(const r3dPoint3D& shootPos);
	bool		FireWeapon(int wid, bool wasAiming, int executeFire, DWORD targetId, const char* pktName); // should be called only for FIRE event
	float		lastWeapDataRep;	// time when last PKT_C2S_PlayerWeapDataRep_s was received
	bool		weapCheatReported;

	void		SetLoadoutData();
	void		 SetWeaponSlot(int wslot, uint32_t weapId, const wiWeaponAttachment& attm);
	void		 SetGearSlot(int gslot, uint32_t gearId);
	
	int		FireHitCount; // counts how many FIRE and HIT events received. They should be equal. If not - cheating detected
	float		deathTime;

	void		DoDeath();
	
	int TradeNums;
	float		lastTimeHit;
	int		lastHitBodyPart;
	float		ApplyDamage(float damage, GameObject* fromObj, int bodyPart, STORE_CATEGORIES damageSource);
	float		 ReduceDamageByGear(int gslot, float damage);
	void OnPlayerUpdateCharSuccess(wiInventoryItem wi,int slot);
	
	int slot;
	// some old things
	float		Height;

	float SendGroupIDTime;
	float SendVitals;
	
	// precalculated boost vars
	float		boostXPBonus_;
	float		boostWPBonus_;

	// stats
	CServerUserProfile profile_;
	wiCharDataFull*	   loadout_;
	wiCharDataFull	savedLoadout_;	// saved loadout copy after last update
	void		SetProfile(const CServerUserProfile& in_profile);

	int		haveBadBackpack_;
	void		ValidateBackpack();
	void		ValidateAttachments();

	PKT_S2C_SetPlayerVitals_s lastVitals_;
	
	float		lastCharUpdateTime_;
	float		lastWorldUpdateTime_;
	DWORD		lastWorldFlags_;
	float		lastVisUpdateTime_;
	void		UpdateGameWorldFlags();
	
	r3dPoint3D	GetRandomPosForItemDrop();
	bool		BackpackAddItem(const wiInventoryItem& wi);
	void		BackpackDropItem(int slot);
	void		OnBackpackChanged(int slot);
	void		OnLoadoutChanged();
	void		OnAttachmentChanged(int wid, int atype);
	void		OnChangeBackpackSuccess(const std::vector<wiInventoryItem>& droppedItems);
	
	wiStatsTracking	AddReward(const wiStatsTracking& rwd);

	bool		isTargetDummy_;

	float lastCurTime;
    bool firstTime;
	void DoRemoveAllItems(obj_ServerPlayer* plr);
	void DoRemoveItems(int slotid);
	void		UseItem_Barricade(const r3dPoint3D& pos, float rotX, uint32_t itemID);
	void		UseItem_ApplyEffect(const PKT_C2C_PlayerUseItem_s& n, uint32_t itemID);

	float		lastChatTime_;
	int		numChatMessages_;

	r3dPoint3D prevpos;
	r3dPoint3D tpos;
	DWORD Tradetargetid;
	wiInventoryItem TradeItems[72];
	
	CNetCellMover	netMover;
	float		moveAccumDist;
	float		moveAccumTime;
	bool		moveInited;
	bool		CheckForFastMove();
	
	float		lastPlayerAction_;	// used for AFK checks

	// packet sequences, used to skip late packets after logic reset
	// for example teleports, game state reset, etc
	DWORD		myPacketSequence;	// expected packets sequence ID
	DWORD		clientPacketSequence;	// received packets sequence ID
	const char*	packetBarrierReason;
	void		SetLatePacketsBarrier(const char* reason);
	
	void		UseItem_CreateNote(const PKT_C2S_CreateNote_s& n);

private: // disable access to SetPosition directly, use TeleportPlayer
	void		SetPosition(const r3dPoint3D& pos)
	{
		__super::SetPosition(pos);
	}
public:
	void		TeleportPlayer(const r3dPoint3D& pos);
	
	//void OnNetPacket(const PKT_C2S_SetVelocity_s& n);
	void        OnNetPacket(const PKT_C2S_CarKill_s& n);
	void		OnNetPacket(const PKT_C2C_PacketBarrier_s& n);
	void		OnNetPacket(const PKT_C2C_MoveSetCell_s& n);
	void		OnNetPacket(const PKT_C2C_MoveRel_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerJump_s& n);
	void OnNetPacket(const PKT_C2S_StackClips_s& n);
	
	void		OnNetPacket(const PKT_C2S_PlayerEquipAttachment_s& n);
	void		OnNetPacket(const PKT_C2S_PlayerRemoveAttachment_s& n);
	void OnNetPacket(const PKT_C2C_CarPass_s& n);
	void OnNetPacket(const PKT_C2S_PlayerState_s& n);
	//void OnNetPacket(const PKT_C2S_SendD3DLog_s& n);
	void OnNetPacket(const PKT_C2S_TradeAccept_s& n);
	void OnNetPacket(const PKT_C2S_ValidateBackpack_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerSwitchWeapon_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerUseItem_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerReload_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerFired_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerHitNothing_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerHitStatic_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerHitStaticPierced_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerHitDynamic_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerReadyGrenade_s& n);
	void		OnNetPacket(const PKT_C2C_PlayerThrewGrenade_s& n);
	void		OnNetPacket(const PKT_C2S_PlayerChangeBackpack_s& n);
	void		OnNetPacket(const PKT_C2S_BackpackDrop_s& n);
	void		OnNetPacket(const PKT_C2S_BackpackSwap_s& n);
	void		OnNetPacket(const PKT_C2S_BackpackJoin_s& n);
	void		OnNetPacket(const PKT_C2S_InventoryOp_s& n);
	void		OnNetPacket(const PKT_C2S_DisconnectReq_s& n);
	void		OnNetPacket(const PKT_C2S_UnloadClipReq_s& n); //Unload Clip
	void OnNetPacket(const PKT_C2S_GroupInvite_s& n);
	void OnNetPacket(const PKT_C2S_TradeRequest_s& n);
	void OnNetPacket(const PKT_C2S_TradeAccept2_s& n);
	void OnNetPacket(const PKT_C2S_TradeCancel_s& n);
	void OnNetPacket(const PKT_C2S_TradeBacktoOp_s& n);
	void OnNetPacket(const PKT_C2S_GroupAccept_s& n);
	void OnNetPacket(const PKT_C2S_GroupKick_s& n);
	void OnNetPacket(const PKT_C2S_SendHelpCall_s& n);
	void OnNetPacket(const PKT_C2S_HackShieldLog_s& n);
	void		OnNetPacket(const PKT_C2S_FallingDamage_s& n);
	void OnNetPacket(const PKT_C2C_PlayerCraftItem_s& n);
	void OnNetPacket(const PKT_C2C_CarStatus_s& n);
	void OnNetPacket(const PKT_C2S_TradeOptoBack_s& n);
	void		OnNetPacket(const PKT_C2S_PlayerWeapDataRep_s& n);
	void OnNetPacket(const PKT_C2S_WpnLog_s& n);
	void OnNetPacket(const PKT_C2S_ValidateEnvironment_s& n);
	void OnNetPacket(const PKT_C2S_BulletValidateConfig_s& n);
	void OnNetPacket(const PKT_C2S_GroupNoAccept_s& n);
	void OnNetPacket(const PKT_C2S_BuyItemReq_s& n);
	void		RelayPacket(const DefaultPacket* packetData, int packetSize, bool guaranteedAndOrdered = false);
	float LeaveGroupTime;
	bool isLeaveGroup;
	bool isTradeAccept;
	void DoTrade(obj_ServerPlayer* plr , obj_ServerPlayer* plr2);
	bool IsItemCanAddToInventory(const wiInventoryItem& wi1);
	int TradeSlot[72]; // for check cheat engine;
	float lastUpdate;
	HANDLE UpdateVisThread;

  public:
	obj_ServerPlayer();
	
virtual	BOOL		Load(const char *name);
virtual	BOOL		OnCreate();			// ObjMan: called before objman take control of this object
virtual	BOOL		OnDestroy();			// ObjMan: just before destructor called

virtual	void		RecalcBoundBox();

virtual	BOOL		Update();			// ObjMan: tick update

virtual	BOOL	 	OnCollide(GameObject *obj, CollisionInfo &trace);

	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);

virtual	BOOL		OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
};

