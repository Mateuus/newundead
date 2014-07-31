#ifndef __P2PMESSAGES_H__
#define __P2PMESSAGES_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "r3dNetwork.h"
#include "./../../src/GameEngine/gameobjects/EventTransport.h"
#include "GameCode/UserProfile.h"
#include "GameCode/UserSkills.h"
#include "../../../ServerNetPackets/NetPacketsGameInfo.h"
#include "../../../ServerNetPackets/NetPacketsWeaponInfo.h"
#include "Gameplay_Params.h"
#include "./../../src/EclipseStudio/Sources/ObjectsCode/WEAPONS/WeaponArmory.h"
//#include "GameObjects/obj_Vehicle.h"
class GameObject;

// all data packets should be as minimal as possible - so, no data aligning
#pragma pack(push)
#pragma pack(1)

#define P2PNET_VERSION	  (0x0000081A + GBWEAPINFO_VERSION + GBGAMEINFO_VERSION + GAMEPLAYPARAM_VERSION) // Allright Net

#define NETID_PLAYERS_START	1		// players [1--255]
#define NETID_OBJECTS_START	300		// various spawned objects [400-0xffff]

static const int NUM_WEAPONS_ON_PLAYER = wiCharDataFull::CHAR_LOADOUT_ITEM4 + 1;

enum pkttype_e
{
  PKT_C2S_ValidateConnectingPeer = r3dNetwork::FIRST_FREE_PACKET_ID,

  PKT_C2C_PacketBarrier,		// server<->client per object packet indicating logical barrier for received packets

  PKT_C2S_GroupNoAccept,
  PKT_C2S_GroupSwapLeader,
  PKT_S2C_UpdateEnvironment,
  PKT_C2S_JoinGameReq,
  PKT_C2S_GroupID,
  //PKT_C2S_SendD3DLog,
  PKT_S2C_JoinGameAns,
  PKT_S2C_ShutdownNote,
  PKT_S2C_SetGamePlayParams,
  PKT_C2S_CarControl,
  PKT_C2S_GroupInvite,
  PKT_C2S_Msg,
  PKT_C2S_GroupAccept,
  PKT_C2S_GroupReInvite,
  PKT_C2S_GroupKick,
  PKT_C2S_TradeAccept2,
  PKT_C2S_TradeCancel,
  
  PKT_C2S_StartGameReq,
  PKT_C2S_ValidateEnvironment,
  PKT_S2C_StartGameAns,
  PKT_S2C_CheatMsg,
  PKT_C2S_DisconnectReq,		// server<-> client disconnect request
  PKT_C2S_UnloadClipReq, // Unload Clip

  PKT_C2S_BulletValidateConfig, // for check cheats.
  PKT_C2S_ValidateBackpack, // for check sync
  //PKT_C2S_ApplyDamageToAnimals,
PKT_C2S_WpnLog,
PKT_C2S_PlayerState,
//PKT_C2C_GroupJoin, // Groups Packet
PKT_S2C_SetPlayerGroupID,
  PKT_S2C_PlayerNameJoined,
  PKT_S2C_PlayerNameLeft,

/*PKT_C2S_SendHelpCall,
  PKT_S2C_SendHelpCall,*/
  PKT_C2C_CarStatus,
  PKT_C2C_CarStatusSv,
 PKT_C2C_CarPass,
   PKT_C2C_CarFuel,
    PKT_C2C_CarFlashLight,
PKT_C2S_SendHelpCall,
PKT_S2C_SendHelpCallData,
  PKT_S2C_CreatePlayer,
  PKT_C2S_CarKill,
   PKT_C2S_CarSound,
    PKT_C2S_CarMove,

	PKT_C2S_HackShieldLog,

  PKT_C2C_PlayerReadyGrenade,
  PKT_C2C_PlayerThrewGrenade,
  PKT_C2C_PlayerReload,
  // weapon fire packets
  PKT_C2C_PlayerFired,		// player fired event. play muzzle effect, etc. actual HIT event is separate event.
							// this will increment server side counter, and HIT packet will decrement it. So, need to make sure that for each FIRE packet
							// we send HIT packet to prevent bullet cheating

  PKT_C2C_PlayerHitNothing,
  PKT_C2C_PlayerHitStatic, // hit static geometry
  PKT_C2C_PlayerHitStaticPierced, // hit static geometry and pierced through it, will be followed up by another HIT event
  PKT_C2C_PlayerHitDynamic, // hit something that can be damaged (player, UAV, etc)

 // PKT_S2C_CreateSafeLock,
PKT_C2S_PlayerAcceptMission,
PKT_C2s_PlayerSetMissionStatus,
PKT_C2S_PlayerSetObStatus,
PKT_C2C_CarSpeed,
  PKT_S2C_SetPlayerVitals,
  PKT_S2C_SetPlayerLoadout,	// indicate loadout change for not local players
  PKT_S2C_SetPlayerAttachments,
  PKT_S2C_SetPlayerWorldFlags,
  PKT_C2S_PlayerEquipAttachment,// equip weapon attachment
  PKT_C2S_PlayerRemoveAttachment,
  PKT_C2C_PlayerSwitchWeapon,
  PKT_C2C_PlayerUseItem,
   PKT_C2C_PlayerCraftItem,
  PKT_S2C_PlayerUsedItemAns, // this packet is sent for immediate action items, like bandages, or morphine shot
  PKT_C2S_PlayerChangeBackpack,
  PKT_C2S_BackpackDrop,		// player backpack operation
  PKT_C2S_BackpackSwap,
  PKT_C2S_BackpackJoin,
  PKT_S2C_BackpackAddNew,	// item added to backpack
  PKT_S2C_BackpackModify,	// item quantity changed in backpack
  PKT_C2S_InventoryOp,		// player inventory operation
  PKT_S2C_CreateNetObject,
  PKT_S2C_CreateVehicles,
  PKT_S2C_DestroyNetObject,
  PKT_C2S_UseNetObject,
  // PKT_C2S_SafeLockUnLock,
  // special packet for special server objects (we still have NetIds so lets use different packets)
  PKT_S2C_CreateDroppedItem,

  PKT_S2C_CreateGrave,
    PKT_S2C_CreateSafeLock,
  PKT_S2C_SetGraveData,
    PKT_S2C_SetSafeLockData,
	PKT_S2C_CreateBuilding,
  // server notes
  PKT_C2S_TradeBackToOp,
  PKT_C2S_TradeRequest,
  PKT_C2S_CreateNote,
  PKT_S2C_CreateNote,
  PKT_S2C_SetNoteData,
  // server animals
  PKT_S2C_CreateAnimals,
  // server zombies
  PKT_S2C_CreateZombie,
  PKT_S2C_ZombieSetState,
  PKT_S2C_ZombieAttack,
  PKT_C2S_Zombie_DBG_AIReq,
  PKT_S2C_Zombie_DBG_AIInfo,
  
  PKT_S2C_AnimalsMove,
  // movement packets
  PKT_S2C_MoveTeleport,
  PKT_C2C_MoveSetCell,			// set cell origin for PKT_C2C_MoveRel updates
  PKT_C2C_MoveRel,
  PKT_C2C_PlayerJump,
  PKT_C2S_Temp_Damage,			// temporary damage packet, client send them. used for rockets/explosions. This packet will always apply damage, even to friendlies!!!
  PKT_C2S_FallingDamage,	// player fell

  PKT_S2C_Damage,
  
  PKT_S2C_KillPlayer,


  PKT_S2C_AddScore,			// single player message to display hp/gp gain

  PKT_S2C_SpawnExplosion, // spawn visual effect of explosion only

  PKT_C2C_ChatMessage,			// client sends to server chat message, server will relay it to everyone except for sender

    //PKT_C2C_GroupMessage,	

  // data update packets
  PKT_C2S_DataUpdateReq,
  PKT_S2C_UpdateWeaponData,
  PKT_S2C_UpdateGearData,
  
  // light meshes
  PKT_S2C_LightMeshStatus, // sends this when we need to turn off light in light mesh

  // some security
  PKT_C2S_SecurityRep,
  PKT_C2S_PlayerWeapDataRep,
  PKT_S2C_CheatWarning,
  PKT_C2S_ScreenshotData,
  
  // admin
  PKT_C2S_Admin_PlayerKick,
  PKT_C2S_Admin_GiveItem,

  // some test thing

  PKT_C2S_TradeOptoBack,
  PKT_C2S_TradeBacktoOp,
  PKT_S2C_SetPlayerReputation,
  PKT_C2S_TradeAccept,
  PKT_C2S_TradeSuccess,
  PKT_C2S_StackClips,
 // PKT_C2S_SetVelocity,
 PKT_S2C_SetAlreadyLoadMaps,
 PKT_C2S_BuyItemReq,
 PKT_C2S_BuyItemAns,
  PKT_LAST_PACKET_ID,
};

#if PKT_LAST_PACKET_ID > 512
  #error Shit happens, more that 255 packet ids
#endif

#define DEFINE_PACKET_HANDLER(xxx) \
  case xxx: { \
    const xxx##_s&n = *(xxx##_s*)packetData; \
    if(packetSize != sizeof(n)) { \
      r3dOutToLog("!!!!errror!!!! %d packetSize %d != %d\n", xxx, packetSize, sizeof(n)); \
      return TRUE; \
    } \
    bool needPassThru = false; \
    On##xxx(n, fromObj, peerId, needPassThru); \
    return needPassThru ? FALSE : TRUE; \
  }


struct PKT_C2S_ValidateConnectingPeer_s : public DefaultPacketMixin<PKT_C2S_ValidateConnectingPeer>
{
	DWORD CRC;
	DWORD		protocolVersion;
	// must be set by client to correctly connect to game server
	__int64		sessionId;
};

struct PKT_C2C_PacketBarrier_s : public DefaultPacketMixin<PKT_C2C_PacketBarrier>
{
};
struct PKT_C2S_CarKill_s : public DefaultPacketMixin<PKT_C2S_CarKill>
{
DWORD	targetId;
};
struct PKT_C2S_CarSound_s : public DefaultPacketMixin<PKT_C2S_CarSound>
{
int	Id;
};
struct PKT_C2S_JoinGameReq_s : public DefaultPacketMixin<PKT_C2S_JoinGameReq>
{
	DWORD		CustomerID;
	DWORD		SessionID;
	DWORD		CharID;		// character id of 
};

struct PKT_S2C_JoinGameAns_s : public DefaultPacketMixin<PKT_S2C_JoinGameAns>
{
	BYTE		success;
	int		playerIdx;

	GBGameInfo	gameInfo;
	__int64		gameTime;	// UTC game time
};
struct PKT_C2C_CarSpeed_s : public DefaultPacketMixin<PKT_C2C_CarSpeed>
{
	float speed;
	float rpm;
	float fuel;
};
struct PKT_S2C_ShutdownNote_s : public DefaultPacketMixin<PKT_S2C_ShutdownNote>
{
	BYTE		reason;
	float		timeLeft;
};

struct PKT_S2C_CheatMsg_s : public DefaultPacketMixin<PKT_S2C_CheatMsg>
{
	char	cheatreason[128];
};

struct PKT_S2C_SetGamePlayParams_s : public DefaultPacketMixin<PKT_S2C_SetGamePlayParams>
{
	DWORD		GPP_Seed;	// per-session value used to xor crc of gpp
	CGamePlayParams	GPP_Data;
};

struct PKT_C2S_StartGameReq_s : public DefaultPacketMixin<PKT_C2S_StartGameReq>
{
	DWORD		lastNetID; // to check sync
	DWORD		ArmoryItems;
	bool FastLoad;
};

struct PKT_S2C_StartGameAns_s : public DefaultPacketMixin<PKT_S2C_StartGameAns>
{
	enum EResult {
	  RES_Unactive,
	  RES_Ok      = 1,
	  RES_Pending = 2,		// server still getting your profile
	  RES_Timeout = 3,		// server was unable to get your profile
	  RES_Failed  = 4,
	  RES_UNSYNC  = 5,
	  RES_InvalidLogin = 6,
	  RES_StillInGame  = 7,
	  RES_IsCheat  = 8,
	};
	BYTE		result;		// status of joining
};

struct PKT_C2S_DisconnectReq_s : public DefaultPacketMixin<PKT_C2S_DisconnectReq>
{
};

struct PKT_C2S_UnloadClipReq_s : public DefaultPacketMixin<PKT_C2S_UnloadClipReq>
{
	int slotID;
};
struct PKT_S2C_SendHelpCallData_s : public DefaultPacketMixin<PKT_S2C_SendHelpCallData>
{ 
char		rewardText[128*2];
	char		DistressText[128*2];
	const char* pName;
	r3dPoint3D pos;
	int peerid;
};
struct PKT_C2S_SendHelpCall_s : public DefaultPacketMixin<PKT_C2S_SendHelpCall>
{ 
	char		rewardText[128*2];
	char		DistressText[128*2];
	//const char* DistressText;
	//const char* rewardText;
	const char* pName;
	int CustomerID;
	r3dPoint3D pos;
};

struct PKT_S2C_PlayerNameJoined_s : public DefaultPacketMixin<PKT_S2C_PlayerNameJoined>
{
	bool isPremium;
	int		peerId;
	char		gamertag[32*2];
	BYTE		flags; // 1=isLegend
	int			Reputation;
	DWORD CustomerID;
	char ip[64];
};

struct PKT_S2C_PlayerNameLeft_s : public DefaultPacketMixin<PKT_S2C_PlayerNameLeft>
{
	int		peerId;
};


// struct used to pass network weapon attachments that affect remote player
struct wiNetWeaponAttm
{
	DWORD		LeftRailID;
	DWORD		MuzzleID;
	
	wiNetWeaponAttm()
	{
	  LeftRailID = 0;
	  MuzzleID   = 0;
	}
};

struct PKT_S2C_CreatePlayer_s : public DefaultPacketMixin<PKT_S2C_CreatePlayer>
{
	int		playerIdx;
	char		gamertag[32*2];
	r3dPoint3D	spawnPos;
	float		spawnDir;
	r3dPoint3D	moveCell;	// cell position from PKT_C2C_MoveSetCell
	int		weapIndex;	// index of equipped weapon (-1 for default)
	BYTE		isFreeHands;

	DWORD		HeroItemID;	// ItemID of base character
	BYTE		HeadIdx;
	BYTE		BodyIdx;
	BYTE		LegsIdx;
	
	// equipped things
	DWORD		WeaponID0;
	DWORD UNARMED;
	DWORD		WeaponID1;
	DWORD		ArmorID;
	DWORD		HeadGearID;
	DWORD		Item0;
	DWORD		Item1;
	DWORD		Item2;
	DWORD		Item3;
	DWORD		BackpackID;

	char ip[64];
	
	// used for remote player creation
	wiNetWeaponAttm	Attm0;
	wiNetWeaponAttm	Attm1;

	DWORD		CustomerID;	// for our in-game admin purposes

	int		ClanID;
	int		GroupID;
	char		ClanTag[5*2]; // utf8
	int		ClanTagColor;

	int Reputation;
};

struct PKT_C2C_PlayerReadyGrenade_s : public DefaultPacketMixin<PKT_C2C_PlayerReadyGrenade>
{
	BYTE		wid;
};

struct PKT_C2C_PlayerThrewGrenade_s : public DefaultPacketMixin<PKT_C2C_PlayerThrewGrenade>
{
	r3dPoint3D	fire_from; // position of character when he is firing
	r3dPoint3D	fire_to;
	float		holding_delay; // if any, used for grenades. THIS IS TEMP until grenades are moved to server(!!)
	BYTE		slotFrom;	// backpack slot
	BYTE		debug_wid;
};

struct PKT_C2C_PlayerReload_s : public DefaultPacketMixin<PKT_C2C_PlayerReload>
{
	BYTE		WeaponSlot;
	BYTE		AmmoSlot;
	BYTE		dbg_Amount;	// actual number of bullets reloaded
};

struct PKT_C2C_PlayerFired_s : public DefaultPacketMixin<PKT_C2C_PlayerFired>
{
	r3dPoint3D	fire_from; // position of character when he is firing
	r3dPoint3D	fire_to;
	float		holding_delay; // if any, used for grenades. THIS IS TEMP until grenades are moved to server(!!)
	BYTE		debug_wid;	// weapon index for debug
	BYTE		wasAiming; // if player was aiming when shot. needed for some achievements
	BYTE		execWeaponFire;
};

struct PKT_C2C_PlayerHitNothing_s : public DefaultPacketMixin<PKT_C2C_PlayerHitNothing>
{
	// empty
};

struct PKT_C2C_PlayerHitStatic_s : public DefaultPacketMixin<PKT_C2C_PlayerHitStatic>
{
	r3dPoint3D	hit_pos;
	r3dPoint3D	hit_norm;
	uint32_t	hash_obj;
	BYTE		decalIdx;
	BYTE		particleIdx;
};

//IMPORTANT: This packet should be equal to PKT_C2C_PlayerHitStatic_s!!!
struct PKT_C2C_PlayerHitStaticPierced_s : public DefaultPacketMixin<PKT_C2C_PlayerHitStaticPierced>
{
	r3dPoint3D	hit_pos;
	r3dPoint3D	hit_norm;
	uint32_t	hash_obj;
	BYTE		decalIdx;
	BYTE		particleIdx;
};

struct PKT_C2C_PlayerHitDynamic_s : public DefaultPacketMixin<PKT_C2C_PlayerHitDynamic>
{
	r3dPoint3D	FirePos;
	r3dPoint3D	muzzler_pos; // for anti cheat
	r3dPoint3D	hit_pos; // where your bullet hit
	gp2pnetid_t	targetId; // what you hit
	BYTE		hit_body_bone; // which bone we hit (for ragdoll)
	BYTE		state; // [0] - from player in air [1] target player in air
	
	// NEEDED FOR SERVER ONLY. TEMP. WILL BE REFACTORED AND MOVED TO SERVER.
	BYTE		hit_body_part;// where we hit player (head, body, etc)
	BYTE		damageFromPiercing; // 0 - no reduction, 100 - no damage at all
};

struct PKT_S2C_MoveTeleport_s : public DefaultPacketMixin<PKT_S2C_MoveTeleport>
{
	r3dPoint3D	teleport_pos; // don't forget to PKT_C2C_PacketBarrier ir
};
struct PKT_C2S_CarMove_s : public DefaultPacketMixin<PKT_C2S_CarMove>
{
	r3dPoint3D	pos;
	r3dVector angle;
};

struct PKT_C2C_MoveSetCell_s : public DefaultPacketMixin<PKT_C2C_MoveSetCell>
{
	// define radius where relative packets will be sent
	const static int PLAYER_CELL_RADIUS = 5;  // packed difference: 0.04m
	const static int UAV_CELL_RADIUS    = 10;
	const static int VEHICLE_CELL_RADIUS = 10;

	r3dPoint3D	pos;
};
struct PKT_C2C_CarFlashLight_s : public DefaultPacketMixin<PKT_C2C_CarFlashLight>
{
	bool bOn;
};
struct PKT_C2C_CarStatusSv_s : public DefaultPacketMixin<PKT_C2C_CarStatusSv>
{
	bool status;
};
struct PKT_C2C_CarStatus_s : public DefaultPacketMixin<PKT_C2C_CarStatus>
{
	//bool status;
	//float fuel;
	gp2pnetid_t	CarID;
};
struct PKT_C2C_CarPass_s : public DefaultPacketMixin<PKT_C2C_CarPass>
{
gp2pnetid_t	NetID;
};
struct PKT_C2C_CarFuel_s : public DefaultPacketMixin<PKT_C2C_CarFuel>
{
	float fuel;
};
struct PKT_C2C_MoveRel_s : public DefaultPacketMixin<PKT_C2C_MoveRel>
{
	// (CELL_RADIUS*2)/[0-255] offset from previously received absolute position
	BYTE		rel_x;
	BYTE		rel_y;
	BYTE		rel_z;

	BYTE		turnAngle;	// [0..360] packed to [0..255]
	BYTE		bendAngle;	// [-PI/2..PI/2] packet to [0..255]
	BYTE		state;		// reflected PlayerState. [0..3] bits - state, [4,7] - dir
};

struct PKT_C2S_HackShieldLog_s : public DefaultPacketMixin<PKT_C2S_HackShieldLog>
{
	int lCode;
	int Status;
	char pName[512];
	float time;
};

struct PKT_C2C_PlayerJump_s : public DefaultPacketMixin<PKT_C2C_PlayerJump>
{
};

struct PKT_S2C_SetPlayerGroupID_s : public DefaultPacketMixin<PKT_S2C_SetPlayerGroupID>
{
	int GroupID;
};
struct PKT_C2S_PlayerAcceptMission_s : public DefaultPacketMixin<PKT_C2S_PlayerAcceptMission>
{
	int id;
};
struct PKT_C2S_PlayerSetObStatus_s : public DefaultPacketMixin<PKT_C2S_PlayerSetObStatus>
{
	int id;
	int ob;
	int status;
};
struct PKT_C2s_PlayerSetMissionStatus_s : public DefaultPacketMixin<PKT_C2s_PlayerSetMissionStatus>
{
	int id;
	int status;
};
struct PKT_S2C_SetPlayerVitals_s : public DefaultPacketMixin<PKT_S2C_SetPlayerVitals>
{
	BYTE		Health;
	BYTE		Thirst;
	BYTE		Hunger;
	BYTE		Toxic;
	bool IsCallForHelp;
	int GroupID;
	bool isVisible;
	int bleeding;
	int legfall;
	int GroupID2;
	void FromChar(const wiCharDataFull* slot)
	{
		Health = (BYTE)slot->Health;
		Thirst = (BYTE)slot->Thirst;
		Hunger = (BYTE)slot->Hunger;
		Toxic  = (BYTE)slot->Toxic;
	}
	bool operator==(const PKT_S2C_SetPlayerVitals_s& rhs) const
	{
		if(Health != rhs.Health) return false;
		if(Thirst != rhs.Thirst) return false;
		if(Hunger != rhs.Hunger) return false;
		if(Toxic  != rhs.Toxic)  return false;
		return true;
	}
	bool operator!=(const PKT_S2C_SetPlayerVitals_s& rhs) const { return !((*this)==rhs); }
};

struct PKT_S2C_SetPlayerLoadout_s : public DefaultPacketMixin<PKT_S2C_SetPlayerLoadout>
{
	DWORD		WeaponID0;
	DWORD		WeaponID1;
	DWORD		QuickSlot1;
	DWORD		QuickSlot2;
	DWORD		QuickSlot3;
	DWORD		QuickSlot4;
	DWORD		ArmorID;
	DWORD		HeadGearID;
	DWORD		BackpackID;
};

struct PKT_S2C_SetPlayerAttachments_s : public DefaultPacketMixin<PKT_S2C_SetPlayerAttachments>
{
	BYTE		wid;
	wiNetWeaponAttm	Attm;
};

struct PKT_S2C_SetPlayerWorldFlags_s : public DefaultPacketMixin<PKT_S2C_SetPlayerWorldFlags>
{
	DWORD		flags;
};

struct PKT_C2S_PlayerEquipAttachment_s : public DefaultPacketMixin<PKT_C2S_PlayerEquipAttachment>
{
	BYTE		wid;
	BYTE		AttmSlot;
	DWORD		dbg_WeaponID;
	DWORD		dbg_AttmID;
	BYTE		dbg_Amount;	// in case of clip attachment - number of bullets
};

struct PKT_C2S_PlayerRemoveAttachment_s : public DefaultPacketMixin<PKT_C2S_PlayerRemoveAttachment>
{
	BYTE		wid;
	BYTE		WpnAttmType;	// of WeaponAttachmentTypeEnum
};

struct PKT_C2C_PlayerSwitchWeapon_s : public DefaultPacketMixin<PKT_C2C_PlayerSwitchWeapon>
{
	BYTE		wid; // 255 - means empty hands
};
struct PKT_C2C_PlayerCraftItem_s : public DefaultPacketMixin<PKT_C2C_PlayerCraftItem>
{
	int slotid1;
	int slotid2;
	int slotid1q;
	int slotid2q;
	int slotid3;
	int slotid4;
	int slotid3q;
	int slotid4q;
	int itemid;
};
struct PKT_C2C_PlayerUseItem_s : public DefaultPacketMixin<PKT_C2C_PlayerUseItem>
{
	BYTE		SlotFrom;	// backpack slot
	DWORD		dbg_ItemID;
	r3dPoint3D	pos;
	// various parameters for items
	float		var1;
	float		var2;
	float		var3;
	DWORD		var4;
};

struct PKT_S2C_PlayerUsedItemAns_s : public DefaultPacketMixin<PKT_S2C_PlayerUsedItemAns>
{
	DWORD		itemId;
	// various parameters for items
	float		var1;
	float		var2;
	float		var3;
	float		var4;
};

struct PKT_C2S_PlayerChangeBackpack_s : public DefaultPacketMixin<PKT_C2S_PlayerChangeBackpack>
{
	BYTE		SlotFrom;
	BYTE		BackpackSize;	// to verify against server
};

struct PKT_C2S_BackpackDrop_s : public DefaultPacketMixin<PKT_C2S_BackpackDrop>
{
	BYTE		SlotFrom;
	r3dPoint3D	pos;
};

struct PKT_C2S_BackpackSwap_s : public DefaultPacketMixin<PKT_C2S_BackpackSwap>
{
	BYTE		SlotFrom;
	BYTE		SlotTo;
};

struct PKT_C2S_BackpackJoin_s : public DefaultPacketMixin<PKT_C2S_BackpackJoin>
{
	BYTE		SlotFrom;
	BYTE		SlotTo;
};
struct PKT_C2S_TradeBackToOp_s : public DefaultPacketMixin<PKT_C2S_TradeBackToOp>
{
	int slotto;
	int slotfrom;
	wiInventoryItem	Item;
};
struct PKT_S2C_BackpackAddNew_s : public DefaultPacketMixin<PKT_S2C_BackpackAddNew>
{
	BYTE		SlotTo;
	wiInventoryItem	Item;
};

struct PKT_S2C_BackpackModify_s : public DefaultPacketMixin<PKT_S2C_BackpackModify>
{
	BYTE		SlotTo;		// or 0xFF when there is NO free slot
	WORD		Quantity;	// target quantity, 0 for removing item
	
	DWORD		dbg_ItemID;	// to check
};

struct PKT_C2S_InventoryOp_s : public DefaultPacketMixin<PKT_C2S_InventoryOp>
{
	enum EOps
	{
	  OP_TOINV,
	  OP_FROMINV,
	};

	BYTE		op;
	BYTE		Slot;
	__int64		InventoryID;
	int		Quantity;
};
struct PKT_S2C_AnimalsMove_s : public DefaultPacketMixin<PKT_S2C_AnimalsMove>
{
	r3dVector angle;
	r3dPoint3D pos;
	int state;
};
struct PKT_S2C_CreateVehicles_s : public DefaultPacketMixin<PKT_S2C_CreateVehicles>
{
	gp2pnetid_t	spawnID;
	r3dPoint3D	pos;
	bool bOn;
	char				vehicle_Model[64];
};
struct PKT_S2C_CreateBuilding_s : public DefaultPacketMixin<PKT_S2C_CreateBuilding>
{
	gp2pnetid_t	spawnID;
	char		fname[512];
	r3dPoint3D pos;
	r3dVector angle;
	bool col;
};
struct PKT_S2C_CreateNetObject_s : public DefaultPacketMixin<PKT_S2C_CreateNetObject>
{
	gp2pnetid_t	spawnID;
	DWORD		itemID;

	r3dPoint3D	pos;
	// various parameters for items
	float		var1;
	float		var2;
	float		var3;
	float		var4;
	float		var5;
};

struct PKT_S2C_DestroyNetObject_s : public DefaultPacketMixin<PKT_S2C_DestroyNetObject>
{
	gp2pnetid_t	spawnID;
};
/*struct PKT_C2S_UseNetObject_s : public DefaultPacketMixin<PKT_C2S_UseNetObject>
{
};*/
struct PKT_C2S_UseNetObject_s : public DefaultPacketMixin<PKT_C2S_UseNetObject>
{
	gp2pnetid_t	spawnID;
};

struct PKT_S2C_CreateDroppedItem_s : public DefaultPacketMixin<PKT_S2C_CreateDroppedItem>
{
	gp2pnetid_t	spawnID;
	r3dPoint3D	pos;
	
	wiInventoryItem	Item;
};

struct PKT_C2S_CreateNote_s : public DefaultPacketMixin<PKT_C2S_CreateNote>
{
	static const int DEFAULT_PLAYER_NOTE_EXPIRE_TIME = 60*24; // in minutes (24 hours of real time (not in-game) expire time)
	BYTE		SlotFrom;	// backpack slot
	r3dPoint3D	pos;
	int		ExpMins;
	char		TextFrom[128];
	char		TextSubj[1024];
};

struct PKT_S2C_CreateSafeLock_s : public DefaultPacketMixin<PKT_S2C_CreateSafeLock>
{
	gp2pnetid_t	spawnID;
	r3dPoint3D	pos;
};
struct PKT_S2C_CreateGrave_s : public DefaultPacketMixin<PKT_S2C_CreateGrave>
{
	gp2pnetid_t	spawnID;
	r3dPoint3D	pos;
};

struct PKT_S2C_CreateNote_s : public DefaultPacketMixin<PKT_S2C_CreateNote>
{
	gp2pnetid_t	spawnID;
	r3dPoint3D	pos;
};
struct PKT_S2C_SetSafeLockData_s : public DefaultPacketMixin<PKT_S2C_SetSafeLockData>
{
	int pass;
};
struct PKT_S2C_SetGraveData_s : public DefaultPacketMixin<PKT_S2C_SetGraveData>
{
	char		plr1[128];
	char		plr2[1024];
};

struct PKT_S2C_SetNoteData_s : public DefaultPacketMixin<PKT_S2C_SetNoteData>
{
	char		TextFrom[128];
	char		TextSubj[1024];
};
struct PKT_S2C_CreateAnimals_s : public DefaultPacketMixin<PKT_S2C_CreateAnimals>
{
gp2pnetid_t	spawnID;
r3dPoint3D	spawnPos;
};
struct PKT_S2C_CreateZombie_s : public DefaultPacketMixin<PKT_S2C_CreateZombie>
{
	gp2pnetid_t	spawnID;
	r3dPoint3D	spawnPos;
	float		spawnDir;
	r3dPoint3D	moveCell;	// cell position from PKT_C2C_MoveSetCell
	DWORD		HeroItemID;	// ItemID of base character
	BYTE		HeadIdx;
	BYTE		BodyIdx;
	BYTE		LegsIdx;
	BYTE		State;		// ZombieStates::EZombieStates
	BYTE		FastZombie;
	float		WalkSpeed;
	float		RunSpeed;
};

struct PKT_S2C_ZombieSetState_s : public DefaultPacketMixin<PKT_S2C_ZombieSetState>
{
	BYTE		State;		// ZombieStates::EZombieStates
};

struct PKT_S2C_ZombieAttack_s : public DefaultPacketMixin<PKT_S2C_ZombieAttack>
{
	gp2pnetid_t	targetId;
};

struct PKT_C2S_Zombie_DBG_AIReq_s : public DefaultPacketMixin<PKT_C2S_Zombie_DBG_AIReq>
{
};

struct PKT_S2C_Zombie_DBG_AIInfo_s : public DefaultPacketMixin<PKT_S2C_Zombie_DBG_AIInfo>
{
	r3dPoint3D	from;
	r3dPoint3D	to;
};

//This packet will always apply damage, even to friendlies!!!
struct PKT_C2S_Temp_Damage_s : public DefaultPacketMixin<PKT_C2S_Temp_Damage>
{
	gp2pnetid_t	targetId;
	BYTE		wpnIdx;
	BYTE		damagePercentage;
	r3dPoint3D	explosion_pos;
};

struct PKT_C2S_FallingDamage_s : public DefaultPacketMixin<PKT_C2S_FallingDamage>
{
	float		damage;
};

struct PKT_S2C_Damage_s : public DefaultPacketMixin<PKT_S2C_Damage>
{
	r3dPoint3D	dmgPos; // position of the damage. for bullets: player position. for grenades\mines\rpg - position of explosion
	gp2pnetid_t	targetId;
	BYTE		damage;
	BYTE		dmgType;
	BYTE		bodyBone;
};

struct PKT_S2C_KillPlayer_s : public DefaultPacketMixin<PKT_S2C_KillPlayer>
{
	// FromID in packet header will be a killer object
	gp2pnetid_t	targetId;
	BYTE		killerWeaponCat; // with what weapon player was killed
	bool		forced_by_server;
};

struct PKT_S2C_AddScore_s : public DefaultPacketMixin<PKT_S2C_AddScore>
{
	PKT_S2C_AddScore_s& operator= (const PKT_S2C_AddScore_s& rhs) {
	  memcpy(this, &rhs, sizeof(*this));
	  return *this;
	}

	WORD		ID;	// id of reward (defined in UserRewards.h for now)
	signed short	XP;
	WORD		GD;
};

struct PKT_C2C_ChatMessage_s : public DefaultPacketMixin<PKT_C2C_ChatMessage>
{
	BYTE		msgChannel; // 0-proximity, 1-global
	BYTE		userFlag; // 1-Legend
	char		gamertag[32*2];
	char		msg[512]; // actual text
};

/*struct PKT_C2C_GroupMessage_s : public DefaultPacketMixin<PKT_C2C_GroupMessage>
{
	BYTE		msgChannel; // 0-proximity, 1-global
	BYTE		userFlag; // 1-Legend
	char		Fromgamertag[32*2];
	char		Togamertag[32*2];
	char		msg[128]; // actual text
	
};*/








/*struct PKT_C2C_GroupJoin_s : public DefaultPacketMixin<PKT_C2C_GroupJoin>
{
	//char		targetgamertag[32*2]; // 0-proximity, 1-global
	BYTE		isLegend; // true-false
	char		gamertag[32*2];
};*/

struct PKT_C2S_DataUpdateReq_s : public DefaultPacketMixin<PKT_C2S_DataUpdateReq>
{
};

struct PKT_S2C_UpdateWeaponData_s : public DefaultPacketMixin<PKT_S2C_UpdateWeaponData>
{
	DWORD		itemId;
	GBWeaponInfo	wi;
};

struct PKT_S2C_UpdateGearData_s : public DefaultPacketMixin<PKT_S2C_UpdateGearData>
{
	DWORD		itemId;
	GBGearInfo	gi;
};

struct PKT_S2C_SpawnExplosion_s : public DefaultPacketMixin<PKT_S2C_SpawnExplosion>
{
	r3dPoint3D pos;
	float radius;
};

struct PKT_C2S_SecurityRep_s : public DefaultPacketMixin<PKT_C2S_SecurityRep>
{
	static const int REPORT_PERIOD = 15; // we should send report every <this> sec

	float		gameTime;
	bool		detectedWireframeCheat;
	DWORD		GPP_Crc32;
};

struct PKT_C2S_PlayerWeapDataRep_s : public DefaultPacketMixin<PKT_C2S_PlayerWeapDataRep>
{
	static const int REPORT_PERIOD = 15; // we should send report every <this> sec
	
	// for CHAR_LOADOUT_WEAPON1 and CHAR_LOADOUT_WEAPON2
	DWORD		weaponsDataHash[2];
	DWORD		debug_wid[2];
	GBWeaponInfo	debug_winfo[2];
};

struct PKT_S2C_CheatWarning_s : public DefaultPacketMixin<PKT_S2C_CheatWarning>
{
	enum ECheatId {
		CHEAT_Protocol = 1,
		CHEAT_Data,
		CHEAT_SpeedHack,
		CHEAT_NumShots,
		CHEAT_BadWeapData,
		CHEAT_Wireframe,
		CHEAT_WeaponPickup,
		CHEAT_GPP,	// game player parameters mismatch
		CHEAT_ShootDistance,
		CHEAT_FastMove,
		CHEAT_AFK,
		CHEAT_UseItem,
		CHEAT_Api,
		CHEAT_Flying,
		CHEAT_NoGeomtryFiring,
		CHEAT_Stamina,
		CHEAT_Network,
	};

	BYTE		cheatId;
};

struct PKT_C2S_ScreenshotData_s : public DefaultPacketMixin<PKT_C2S_ScreenshotData>
{
	//FILE* data;
	//char* data;
//	BYTE		errorCode;	// 0 if success
	WORD		dataSize;
	// this packet will have attached screenshot data after header
	// char		data[0x7FFF];
};

struct PKT_C2S_Admin_PlayerKick_s : public DefaultPacketMixin<PKT_C2S_Admin_PlayerKick>
{
	gp2pnetid_t netID; // netID of who to kick
};

struct PKT_C2S_Admin_GiveItem_s : public DefaultPacketMixin<PKT_C2S_Admin_GiveItem>
{
	DWORD		ItemID;;
};


struct PKT_S2C_LightMeshStatus_s : public DefaultPacketMixin<PKT_S2C_LightMeshStatus>
{
};


struct PKT_S2C_SetPlayerReputation_s : public DefaultPacketMixin<PKT_S2C_SetPlayerReputation>
{
	int			Reputation;
};

struct PKT_C2S_WpnLog_s : public DefaultPacketMixin<PKT_C2S_WpnLog>
{
	float m_spread;
	float m_recoil;
	float m_AmmoSpeed;
	float m_AmmoMass;
	float m_AmmoDamage;
	DWORD itemid;
	DWORD slot;
	float m_recoilattm;
	float m_spreadattm;
	float m_rateoffire;
	float m_rateattm;
	float m_spread2;
	float m_recoil2;
};

struct PKT_C2S_BulletValidateConfig_s : public DefaultPacketMixin<PKT_C2S_BulletValidateConfig>
{
	int m_itemId;
	float m_AmmoMass;
	float m_AmmoSpeed;
};

struct PKT_C2S_ValidateEnvironment_s : public DefaultPacketMixin<PKT_C2S_ValidateEnvironment>
{
	float CurTime;
};
struct PKT_S2C_UpdateEnvironment_s : public DefaultPacketMixin<PKT_S2C_UpdateEnvironment>
{
	bool rain;
};
struct PKT_C2S_PlayerState_s : public DefaultPacketMixin<PKT_C2S_PlayerState>
{
	r3dPoint3D accel;
	int state;
};
struct PKT_C2S_CarControl_s : public DefaultPacketMixin<PKT_C2S_CarControl>
{
	PxVehicleDrive4WRawInputData carControlData;
};
struct PKT_C2S_GroupInvite_s : public DefaultPacketMixin<PKT_C2S_GroupInvite>
{
    int	peerId;
};
struct PKT_C2S_GroupAccept_s : public DefaultPacketMixin<PKT_C2S_GroupAccept>
{
    int	peerId;
};
struct PKT_C2S_GroupReInvite_s : public DefaultPacketMixin<PKT_C2S_GroupReInvite>
{
	int peerId;
};
struct PKT_C2S_Msg_s : public DefaultPacketMixin<PKT_C2S_Msg>
{
    char msg[512];
};
struct PKT_C2S_ValidateBackpack_s : public DefaultPacketMixin<PKT_C2S_ValidateBackpack>
{
	wiInventoryItem Items[72];
};
struct PKT_C2S_GroupKick_s : public DefaultPacketMixin<PKT_C2S_GroupKick>
{
	int peerId;
};
struct PKT_C2S_GroupSwapLeader_s : public DefaultPacketMixin<PKT_C2S_GroupSwapLeader>
{
	int peerId;
};
struct PKT_C2S_GroupNoAccept_s : public DefaultPacketMixin<PKT_C2S_GroupNoAccept>
{
	int peerId;
};
struct PKT_C2S_GroupID_s : public DefaultPacketMixin<PKT_C2S_GroupID>
{
	int GroupID;
	int peerId;
};
struct PKT_C2S_TradeCancel_s : public DefaultPacketMixin<PKT_C2S_TradeCancel>
{
	int netId;
};
struct PKT_C2S_TradeAccept_s : public DefaultPacketMixin<PKT_C2S_TradeAccept>
{
	int netId;
};
struct PKT_C2S_TradeBacktoOp_s : public DefaultPacketMixin<PKT_C2S_TradeBacktoOp>
{
	int netId;
	wiInventoryItem	Item;
	int slotto;
};
struct PKT_C2S_TradeOptoBack_s : public DefaultPacketMixin<PKT_C2S_TradeOptoBack>
{
	int netId;
	wiInventoryItem	Item;
	int slotfrom;
};
struct PKT_C2S_TradeAccept2_s : public DefaultPacketMixin<PKT_C2S_TradeAccept2>
{
	wiInventoryItem	Item[72];
	int netId;
	int slot[72];
};
struct PKT_C2S_TradeSuccess_s : public DefaultPacketMixin<PKT_C2S_TradeSuccess>
{
};
struct PKT_C2S_StackClips_s : public DefaultPacketMixin<PKT_C2S_StackClips>
{
	int slotId;
};
struct PKT_C2S_TradeRequest_s : public DefaultPacketMixin<PKT_C2S_TradeRequest>
{
	int netId;
};

struct PKT_S2C_SetAlreadyLoadMaps_s : public DefaultPacketMixin<PKT_S2C_SetAlreadyLoadMaps>
{
	
};

struct PKT_C2S_BuyItemReq_s : public DefaultPacketMixin<PKT_C2S_BuyItemReq>
{
	int ItemID;
};

struct PKT_C2S_BuyItemAns_s : public DefaultPacketMixin<PKT_C2S_BuyItemAns>
{
	int ansCode;
};

/*struct PKT_C2S_SetVelocity_s : public DefaultPacketMixin<PKT_C2S_SetVelocity>
{
	r3dPoint3D pos;
	r3dPoint3D vel;
};*/
#pragma pack(pop)

#endif	// __P2PMESSAGES_H__

