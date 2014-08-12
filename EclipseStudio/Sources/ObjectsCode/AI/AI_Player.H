#pragma once

#include "r3dProtect.h"
#include "GameCommon.h"
#include "GameCode/UserProfile.h"
#include "multiplayer/P2PMessages.h"
#include "multiplayer/NetCellMover.h"
#include "ObjectsCode/AI/AI_PlayerAnim.H"
#include "./../GameEngine/CamouflageDataManager.h"
#include "APIScaleformGfx.h"
#include "AI_PlayerSenses.h"

class Weapon;
class Gear;
class CUberEquip;
class CUberAnim;
#if VEHICLES_ENABLED//Codex Carros
class obj_Vehicle;
#endif

//int rRet3 = _AhnHS_VerifyProtectedFunction();
class obj_Player : public GameObject
{
public:
	DECLARE_CLASS(obj_Player, GameObject)

	obj_Player();

#if VEHICLES_ENABLED //Codex Carros
	obj_Vehicle* ActualVehicle; // Server Vehicles
    bool StrykerSound;
	float GearsSounds;
	bool IsOnVehicle; // Server Vehicles
	int VehicleID;
	bool ImOwnerCar;
#endif

	enum AuraType
	{
		AT_NONE,
		AT_BERSERK,
		AT_HEALING,
		AT_MORHINE,
		AT_SPAWNPROTECTION,
		AT_COUNT
	} m_AuraType ;

	char				m_MinimapTagIconName[64]; // temp, to be optimized soon

	float   CurrentSpread;

	int	PhysXObstacleIndex;

	float	moveicontime; // time


	DWORD	CustomerID;
	r3dSec_string<64, 0xF7>	m_EncryptedUserName; // utf8 string so use it carefully. Encrypted to make life more difficult for haxorz :)
	// encrypted user name
	void		GetUserName(char* out) const {
		m_EncryptedUserName.get(out);
	}
	void  GetUserNameAndClanTag(char* out) const {
		if(ClanID != 0)
		{
			char tmpUsername[64];
			m_EncryptedUserName.get(tmpUsername);
			const char* tagColor = "#000000";
			switch(ClanTagColor)
			{
			case 1: tagColor = "#aa0000"; break;
			case 2: tagColor = "#a6780c"; break;
			case 3: tagColor = "#10a49e"; break;
			case 4: tagColor = "#20a414"; break;
			case 5: tagColor = "#104ea4"; break;
			case 6: tagColor = "#9610a4"; break;
			case 7: tagColor = "#444444"; break;
			case 8: tagColor = "#a4a4a4"; break;
			default:
				break;
			}
			sprintf(out, "<font color='%s'>[%s]</font> %s", tagColor, ClanTag, tmpUsername);
			return;
		}
		else
			return GetUserName(out);
	}

	void  GetUserName123(char* out) const {
		if(ClanID != 0)
		{
			char tmpUsername[64];
			m_EncryptedUserName.get(tmpUsername);
			const char* tagColor = "#000000";
			switch(ClanTagColor)
			{
			case 1: tagColor = "#aa0000"; break;
			case 2: tagColor = "#a6780c"; break;
			case 3: tagColor = "#10a49e"; break;
			case 4: tagColor = "#20a414"; break;
			case 5: tagColor = "#104ea4"; break;
			case 6: tagColor = "#9610a4"; break;
			case 7: tagColor = "#444444"; break;
			case 8: tagColor = "#a4a4a4"; break;
			default:
				break;
			}
			sprintf(out, "%s",tmpUsername);
			return;
		}
		else
			return GetUserName(out);
	}

	int		ClanID;
	int		GroupID;
	int		ClanTagColor;
	char		ClanTag[5*2];

	Scaleform::GFx::Value m_CharIcon;

	wiCharDataFull	CurLoadout;
	float		m_GearWeight;
	void		OnBackpackChanged(int idx);

	bool	m_enableRendering;

	PlayerLifeProps lifeProperties;
	float			showSlotsInfoTimer;

private:
	bool	m_ItemSimulateLeftClick;
	/**	Memorize bounding box, when switch to ragdoll. We will restore it after switching back. */
	r3dBoundBox m_OrigBBox;

public:
	void CheckVeCam();
	bool bPhys;
	static r3dTexture* s_laserPointerTex;
	static r3dTexture* s_flashlightFlashTex;
	static r3dTexture* s_laserFlashTex;
	static r3dTexture* s_lightHaloTex;

	uint32_t		m_needToDrawBarricadePlacement; // itemID of what shield to draw
	r3dMesh*		m_BarricadeMeshPlacement; // mesh to render when you are placing shield, not sure how to implement that better rather than having pointer to mesh directly

	// shared for all placeable items
	PrecalculatedMeshVSConsts	m_ConsumableItemPlacementMeshVSConsts;
	bool			m_canPlaceConsumableItem;

	class r3dPhysSkeleton* m_PhysSkeleton;
	bool m_disablePhysSkeleton; // for character in frontend and respawn

	// anim
	CUberEquip*	uberEquip_;
	CUberAnim*	uberAnim_;
	r3dSkeleton*	GetSkeleton();	// access to r3dSkeleton inside uberAnim
	r3dAnimation&	GetAnimation();	// access to r3dAnimation inside uberAnim
	int		PlayerState;	// player state as Playerstate_e
	int		PlayerMoveDir;	// movement direction as CUberData::EAnimDir

	void UpdateLoadoutSlot(const struct wiCharDataFull& slot); // updates meshes from loadout slot
	void RemoveWpnAttm(int wid, int attmType); // also used to equip default attm
	void EquipWpnAttm(int wid, int backpackSlot);
	bool ChangeBackpack(int slotfrom);
	void DropItem(int slotFrom);
	void loadHeroMeshes(const class HeroConfig* heroConfig);

	void updateWeaponUI();

	float	grenadeReadyTime;

	void switchFPS_TPS();
	void switchToEmptyHands();
	bool forcedEmptyHands;

	void		SyncAnimation(bool force = false);
	void		UpdateAnimations(const D3DXMATRIX& CharDrawMatrix);

	r3dPoint3D	GetHeadPosition();

	Weapon*		m_Weapons[NUM_WEAPONS_ON_PLAYER]; // 0,1 - rifles. 2-item, 3-item, 4-item, 5-item
	//Weapon*		Old_Weapons[NUM_WEAPONS_ON_PLAYER]; // 0,1 - rifles. 2-item, 3-item, 4-item, 5-item
	//int m_WeaponItemID[NUM_WEAPONS_ON_PLAYER];
	int		m_SelectedWeapon;
	int		m_PrevSelectedWeapon;
	bool		hasScopeMode() const;
	bool		hasScopeMode_hidePlayerModel() const;
	bool		m_isPressedFireTrigger;
	bool		m_isAiming;
	bool		m_isChangedAiming1;  // not resetting event about aiming mode change. used in Weapon::Update
	bool		m_isInScope;
	bool		m_isFinishedAiming; // this one will make sure that player is aiming and that he finished playing aim animation, so no insta aim by clicking fire and aim at the same time

	bool		ChangeWeaponByIndex(int index);
	void		UpdateCharWeaponMeshes();
	void		ReloadWeaponFromSlot(int wpnSlot, int ammoSlot, int amount);
    //r3dPoint3D prevPos3[100];
	//r3dPoint3D prevPos4[8192];
	r3dPoint3D prevPosLeng;
	//bool isBug;
	//float LastBugTime;
	//int Posi;
	//int Pos2;
	static void AddBloodOnGround(const r3dPoint3D& posOfShot); // will cast a ray down from posOfShot
	float		m_BloodTimer;

	float proneTime;

	// hit marker
	float		m_HitMarkerFadeout;
	r3dTexture*	m_HitMarkerTex; // only for local player


	// local stats for scoreboard
	struct stats_s
	{
		int		XP;
		int		GD;

		stats_s() 
		{
			XP = 0;
			GD = 0;
		}
	};
	stats_s		GameStats;

	// queue of local player added rewards
	std::vector<PKT_S2C_AddScore_s> rewards_;
	float		lastRewardShowTime_;
	void		ProcessRewardQueue();

	float		bodyAdjust_x;		// rotation
	float		bodyAdjust_y[2];	// bend [cur, target]

	int		boneId_Bip01_Spine1;
	int		boneId_Bip01_Spine2;
	int		boneId_Bip01_Neck;
	int		boneId_Bip01_LUpperArm;
	int		boneId_Bip01_RUpperArm;
	int		boneId_Bip01_Head;

	int		boneId_Bip01_R_Hand;
	int		boneId_Bip01_L_Hand;

	// for adrenaline and epinephrine shots
	float		m_SpeedBoost;
	float		m_SpeedBoostTime;

	float		m_MorhineShotTime;

	float		m_BandagesEffectTime;

	// var where we will keep velocity, regardless of physx
	// this is needed, because animation and network moving player code
	// depends on GetVelocity()
	// and velocity doesn't do a thing in ControllerPhysObj
	r3dPoint3D	plr_local_Velocity;	
	virtual	void 			SetVelocity(const r3dPoint3D& vel) { plr_local_Velocity = vel; }
	virtual	const r3dPoint3D&	GetVelocity() const { return plr_local_Velocity; }
	float	plr_local_moving_speed;

	// info about player
	float		Height;
	float		getPlayerHeightForCamera() const;

	// sprint energy vars
	/*r3dSec_type<float, 0xA95A3BA1> */ float m_Stamina;
	float m_StaminaPenaltyTime;
	float		getMaxStamina();

	// sniper & breath vars
	r3dVector SniperViewMod; // to simulate rifle movement when trying to snipe
	float		getMaxBreath();
	bool		m_isHoldingBreath;
	r3dSec_type<float, 0x683A2AB9> m_Breath;

	float		lastDamageRcvd;
	float		lastTimeHit;
	int		lastTimeHitBone;
	r3dPoint3D	lastTimeHitForce;
	void		ApplyDamage(const r3dPoint3D& dmgPos, float damage, GameObject* fromObj, int bodyBone, int dmgType);
	float		BloodEffect; // timer
	int			bDead;
	float		TimeOfDeath; // for animations after dying
	STORE_CATEGORIES DeathDamageSource;
	r3dPoint3D	PosOfDeath;
	float		TimeOfLastRespawn;
	float nextSendD3D;
	int			Dead_KillerID; // killer ID of a person who killed you
	bool		DisableKillerView; // if user clicks on a mouse, show respawn right away
	bool		bCrouch;
	bool		bProne;
	bool		bSwim;
	bool		bSwimShift;
	float lastSendStateLog;
	float recoil;
	float spread;
	float recoil2;
	// jump stuff
	void		DetectIfOnGround();
	bool		bOnGround; // true if player is touching any geometry
	float		fHeightAboveGround;
	void		StartJump();
	bool		IsJumpActive();
	float		JumpVelocity;
	float		JumpStartTime;	// time when we actually need to apply JumpVelocity
	float		StartFallingHeight;
	float		StartFallingTime;
	r3dPoint3D prevPos2;

	r3dSec_type<float,		0x5Fc3cdAa>	RecoilCooldown;


	r3dVector RecoilViewMod; // adds recoil to ViewAngle
	float		m_LastKillTime;
	r3dVector RecoilViewModTarget;
	r3dVector RecoilViewModTarget2; // used for when you are being shot

	r3dVector ViewAngle;		// mouse controlled view angle, in degrees. 
	r3dPoint3D RealAcceleration; // actual player speed, after weight mod, jump, gravity, etc
	r3dPoint3D InputAcceleration; // that is the desired accel based on what buttons player pressed
	float m_RemainingSecondWindTime;  // for the second wind Ability

	r3dVector m_vVision;		// current vector of vision view

	D3DXMATRIX 	DrawRotMatrix;	// character rotation matrix
	D3DXMATRIX 	DrawFullMatrix;
	D3DXMATRIX 	DrawFullMatrix_Localized;

	D3DXMATRIX	MoveMatrix;
	r3dVector	GetvForw() const { return r3dVector(MoveMatrix._31, MoveMatrix._32, MoveMatrix._33); }
	r3dVector	GetvRight() const { return r3dVector(MoveMatrix._11, MoveMatrix._12, MoveMatrix._13); }
	r3dVector	GetvRightDraw() const { return r3dVector(DrawRotMatrix._11, DrawRotMatrix._12, DrawRotMatrix._13); }

	float		m_fPlayerRotation;
	float		m_fPlayerRotationTarget;
	void		UpdateRotation();
	void		 UpdateUpperBodyAngLegs();
	void		 UpdateRotationToTarget(float fY, float fTurnSpeed);

	CNetCellMover	netMover;

	int GetInventoryIdByItemId(int ItemId,const wiCharDataFull& Loadout);
	// set player position and reset cell base, so it'll be updated on next movement
	void TeleportPlayer(const r3dPoint3D& pos, const char* reason)
	{
		//r3dOutToLog("... %p: TeleportPlayer %s\n", this, reason);
		prevPos = pos;
		SetPosition(pos);
		netMover.Teleport(pos);
	}

	/**	Turn on or off physics simulation for character physics (basically remove/add actors from scene). */
	void TogglePhysicsSimulation(bool on);


	float		afterRespawnTimer;
	float		lastWeapDataRep;	// time when weapon data report was sent
	float		lastTimeWeaponSwitch;	// to prevent switching weapon and keep firing

	r3dPoint3D		viewTargetPos;
	r3dPoint3D		viewTargetNorm;

	//float	CoolDown; // time

	void		UpdateTargetting();
	void		PlayerFired(int hitId, const r3dPoint3D& pos);
	r3dPoint3D	GetMuzzlerPosition();

	void		DoDeath(int networdIDKiller, bool forced_by_server, STORE_CATEGORIES damageSource); // not passing the actual pointer, to make sure that we will not later access dead pointer, in case if killer left the game
	void		Undie();
	void		DoRagdoll(bool toRagdoll, int boneId = 0, const r3dPoint3D* impulse = NULL);

	void		DrawLabel();
	void		DrawReticle();
	float		m_ReticleTargetScale;
	float		m_ReticleCurrentScale;

	bool		m_isFemaleHero;
public:

	virtual	BOOL		Load(const char *name);
	virtual	BOOL		OnCreate();			
	virtual	BOOL		OnDestroy();		

	virtual	BOOL		Update();			

	void				UpdateLocalPlayerMovement();

	virtual void		AppendShadowRenderables( RenderArray & rarr, const r3dCamera& Cam ) OVERRIDE;
	virtual void		AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam )  OVERRIDE;

	virtual	BOOL		OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
	void			 OnNetPacket(const PKT_S2C_MoveTeleport_s& n);
	void			 OnNetPacket(const PKT_C2C_MoveSetCell_s& n);
	void			 OnNetPacket(const PKT_C2C_MoveRel_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerJump_s& n);
	void OnNetPacket(const PKT_C2S_TradeOptoBack_s& n);
	void OnNetPacket(const PKT_C2S_GroupReInvite_s& n);
	void OnNetPacket(const PKT_C2S_GroupKick_s& n);
	void OnNetPacket(const PKT_C2S_GroupID_s& n);
	void OnNetPacket(const PKT_C2S_GroupNoAccept_s& n);
	void OnNetPacket(const PKT_C2S_GroupSwapLeader_s& n);
	void OnNetPacket(const PKT_C2S_TradeRequest_s& n);
	void OnNetPacket(const PKT_C2S_TradeSuccess_s& n);
	void OnNetPacket(const PKT_C2S_TradeAccept2_s& n);
	void			 OnNetPacket(const PKT_S2C_AddScore_s& n);
	void			 OnNetPacket(const PKT_S2C_SetPlayerVitals_s& n);
	void OnNetPacket(const PKT_S2C_SetPlayerGroupID_s& n);
	void OnNetPacket(const PKT_C2S_TradeCancel_s& n);
	void OnNetPacket(const PKT_C2S_TradeBacktoOp_s& n);
	void			 OnNetPacket(const PKT_S2C_SetPlayerLoadout_s& n);
	void			 OnNetPacket(const PKT_S2C_SetPlayerAttachments_s& n);
	void			 OnNetPacket(const PKT_S2C_SetPlayerWorldFlags_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerSwitchWeapon_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerReadyGrenade_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerThrewGrenade_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerReload_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerFired_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerHitNothing_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerHitStatic_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerHitStaticPierced_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerHitDynamic_s& n);
	void OnNetPacket(const PKT_C2S_TradeAccept_s& n);
	//void OnNetPacket(const PKT_C2S_SetVelocity_s& n);
	void			 OnNetPacket(const PKT_S2C_SpawnExplosion_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerUseItem_s& n);
	void			 OnNetPacket(const PKT_S2C_PlayerUsedItemAns_s& n);
	void			 OnNetPacket(const PKT_S2C_BackpackAddNew_s& n);
	void			 OnNetPacket(const PKT_S2C_BackpackModify_s& n);
	void			 OnNetPacket(const PKT_S2C_SetPlayerReputation_s& n);
	void			 OnNetPacket(const PKT_C2C_PlayerOnVehicle_s& n);  // Server Vehicles //Codex Carros
	void			 OnNetPacket(const PKT_C2C_CarSeat_s& n);  // Server Vehicles //Codex Carros

	virtual void		UpdateTransform();

	void		stopFootstepsSound();

	bool		bAllowToUseWeapons;
	void		CheckFireWeapon();
	bool		CheckVehicle();//Codex Carros
	float		GetGrenadeLaunchDelay();
	void		GrenadeCallbackFromWeapon(const wiInventoryItem& wi);

	void		OnGameEnded();
	BYTE		getBodyNameFromBoneName(const char* boneName);
	void UpdateHud();
	float updatehudtime;
	void		localPlayer_UseItem(int slotID, uint32_t itemID, const r3dPoint3D& pos, float var1=0.0f, float var2=0.0f, float var3=0.0f, DWORD var4=0);

#if VEHICLES_ENABLED //Codex Carros

	enum VehicleViews{
		VehicleView_None,
		VehicleView_Driver,
		VehicleView_Passenger,
	};

	VehicleViews vehicleViewActive_;


	obj_Vehicle* canEnterVehicle();
	void EnterVehicleHowPassenger( obj_Vehicle* target_Vehicle ); //server vehicles
	void enterVehicle( obj_Vehicle* target_Vehicle );
	bool isDriving();
	bool isPassenger();
	void exitVehicleHowPassenger(); //server vehicles
	void exitVehicle();
	bool isInVehicle();
	bool TestExitPosition( r3dVector &exitPosition, bool ignoreGround = false );
    bool exitsound;
#endif

	bool Cheats;
	bool ESPItems;
	bool ESPZombies;

	bool ESPHackKill;

	Weapon* oldWpn;
	char ip[64];
	bool isntvi;


private:
	void		executeWeaponFireLocalPlayer(const r3dPoint3D& from, float recoil, float spread, float recoilCoolDownMod, float holdingDelay, bool executeWeaponFireCode, bool checkAgainsMuzzler, int damagePercFromPiercing);

	/**	Footsteps sound. */
	float		footsteps_timeSinceLastMissingHitResult;
	void		*footStepsSnd;
	r3dPoint3D prevPos;
	struct footStepEntry_s {
		int		id;
		void*		snd;
		footStepEntry_s() : id(-1), snd(0) {}
		footStepEntry_s(int i, void* s):id(i), snd(s) {}
	};
	r3dTL::TStaticArray<footStepEntry_s, 128> footStepsArr;
	int		currentFootStepsSoundID;
	void		UpdateFootsteps();

	void		UpdateSounds();

	// sprinting sounds
	void*	m_sndBreathSprint;
	void*	m_bswim;
	void*	m_bswimf;


	float	m_sndBreathBaseVolume;

	void*	m_sndInPain;

	void*	m_sndClothMovementSprint;

	void*	m_sndSniperBreath;
	void*	m_sndSniperHeartSlow;
	void*	m_sndSniperExhale;

	void		UpdateActionUI();
public:
	void*   m_sndVehicleDrive;//Codex Carros
	CamoTimeLine camoTimeLine;
};

extern void InitializePhysSkeletonCache( int count, float progressStart, float progressEnd ) ;
extern void DestroyPhysSkeletonCache() ;
extern r3dPhysSkeleton* AquireCacheSkeleton();
extern void ReleaseCachedPhysSkeleton(r3dPhysSkeleton* skel);
extern void UpdateAnimalObstacle(int idx, const r3dPoint3D& pos);//Codex Animal

extern void InitializePhysObstacleCache(int count);
extern void DestroyPhysObstacleCache();
extern int AcquirePlayerObstacle(const r3dPoint3D& pos);
extern void ReleasePlayerObstacle(int* out_idx);
extern void UpdatePlayerObstacle(int idx, const r3dPoint3D& pos);



// for use when processing a bullet hit by obj_bullet or obj_Player::executeWeaponFireLocalPlayer
bool ProcessBulletHit( int &damageFromPiercable, GameObject* owner, const r3dPoint3D &hitPoint, const r3dPoint3D &hitNormal, GameObject* shootTarget, const r3dMaterial* shootMaterial, const char* hitActorName, const WeaponConfig *weaponInfo, const r3dPoint3D& muzzlerPosAtFireStart, const r3dPoint3D& FirePos, int category);
