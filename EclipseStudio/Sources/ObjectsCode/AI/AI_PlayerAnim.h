#pragma once

#include "r3dProtect.h"

// !!!sync this with obj_ServerPlayer.cpp on server!!!
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

// class holding shared animation data between all players
class CUberData
{
  public:
	enum
	{
		INVALID_BONE_ID = -1 
	};

	// animation pool
	r3dAnimPool	animPool_;

	// base binding skeleton for animations
	const r3dSkeleton* bindSkeleton_;

	// possible animation variations per each weapon
	enum
	{
		AIDX_UIIdle,
		AIDX_IdleLower,
		AIDX_StandLower,
		// from here all animations is upper body
		AIDX_IdleUpper,
		AIDX_StandUpper,
		AIDX_CrouchBlend,
		AIDX_CrouchAim,
		AIDX_WalkBlend,
		AIDX_WalkAim,
		AIDX_RunBlend,
		AIDX_SprintBlend,
		AIDX_ReloadWalk,
		AIDX_ReloadIdle,
		AIDX_ReloadCrouch,
		AIDX_ShootWalk,
		AIDX_ShootAim,
		AIDX_ShootCrouch,
		AIDX_ProneBlend,
		AIDX_ProneAim,
		AIDX_ReloadProne,
		AIDX_ShootProne,
		AIDX_IdleProne,
		AIDX_COUNT,
	};
	std::string	blendStartBones_[AIDX_COUNT];
	void		LoadUpperBlendStartBones();

	// lower walking animation directions
	enum EAnimDir
	{
		ANIMDIR_Stand,
		ANIMDIR_Str,
		ANIMDIR_StrLeft,
		ANIMDIR_StrRight,
		ANIMDIR_Left,
		ANIMDIR_Right,
		ANIMDIR_Back,
		ANIMDIR_BackLeft,
		ANIMDIR_BackRight,
		ANIMDIR_COUNT,
	};
static	int		GetMoveDirFromAcceleration(const r3dPoint3D& accel);

	struct animIndices_s
	{
	  // lower portions of animations
	  int		crouch[ANIMDIR_COUNT];
	  int		walk[ANIMDIR_COUNT];
	  int		run[ANIMDIR_COUNT];
	  int		sprint[ANIMDIR_COUNT];
	  int		prone[ANIMDIR_COUNT];
	  int		swim[ANIMDIR_COUNT];
	  int		swimm[ANIMDIR_COUNT];
	  int		swimf[ANIMDIR_COUNT];
	   int		swimLEFT[ANIMDIR_COUNT];
	     int		swimRIGHT[ANIMDIR_COUNT];
		 int		swimFORWARD[ANIMDIR_COUNT];
		 int		swimBACKWARD[ANIMDIR_COUNT];
		  int		Driver[ANIMDIR_COUNT];
	  int		prone_up_weapon;
	  int		prone_down_weapon;
	  int		prone_up_noweapon;
	  int		prone_down_noweapon;

	  // misc ones
	  int		turnins[5];
	   int		hands[1];
	  int		grenades_tps[20];
	  int		grenades_fps[20];
	  //int		bombs_tps[20];
	  //int		bombs_fps[20];
	  int		jumps[20];
	  int		deaths[20];
	  int		UI_IdleNoWeapon;

	  int		attmMenuRiseWeapon[6]; //6 - number of weapon types
	  int		attmMenuIdleWeapon[6]; 
	};
	animIndices_s	aid_;
	int		GetGrenadeAnimId(bool IsFPS, int PlayerState, int GrenadeState);
	int		GetJumpAnimId(int PlayerState, int JumpState);
	
	// default weapon animation indices
	int		wpn1[AIDX_COUNT];
	int		wpn1_fps[AIDX_COUNT];

  private:
	friend class obj_Player;
	friend class obj_Zombie;
	friend void AI_Player_LoadStuff();
	friend void AI_Player_FreeStuff();
	CUberData();
	~CUberData();

	void		LoadSkeleton();

	void		LoadAnimations();
	void		 LoadLowerAnimations();
	void		 LoadWeaponAnim(int (&wid)[AIDX_COUNT], int (&wid_fps)[AIDX_COUNT], const char* names[AIDX_COUNT]);
	void		 LoadGrenadeAnim();
	void		 LoadJumpAnim();
	void		 LoadDeathAnim();

	friend class Weapon; // for being able to load weapon specific AIM animations in FPS mode
	int		AddAnimation(const char* name, const char* anim_fname = NULL);
	void		AddAnimationWithFPS(const char* name, int& aid, int& fps_aid);
	
	int		TryToAddAnimation(const char* name, const char* anim_fname = NULL);

	void		LoadWeaponTable();
};

// class for holding and drawing eqipment
class Gear;
class WeaponConfig;
class Weapon;

enum ESlot
{
	SLOT_UpperBody = 0,
	SLOT_LowerBody,
	SLOT_Head,
	SLOT_Armor,
	SLOT_Helmet,
	SLOT_Backpack,
	SLOT_Weapon,
	SLOT_WeaponBackRight,
	SLOT_WeaponSide,
	SLOT_Max,
};

class CUberEquip
{
public:
	obj_Player* player;
	// equipped things
	class slot_s
	{
	public:
		// only one of those should be non null
		Weapon*		wpn;	// NOT owning pointer, obj_Player own it.
		Gear*		gear;	// owning pointer to gear
		r3dMesh*	mesh;

		slot_s() { 
			gear = NULL;
			wpn = NULL;
			mesh = NULL;
		}
	};
	slot_s		slots_[SLOT_Max];

	enum DrawType
	{
		DT_DEFERRED,
		DT_SHADOWS,
		DT_AURA,
		DT_DEPTH
	};
	
private:
	void		DrawSlot(ESlot slotId, const D3DXMATRIX& world, DrawType dt, bool skin, bool draw_firstperson, const r3dSkeleton* wpnSkeleton);
	void		DrawSlotMesh(r3dMesh* mesh, const D3DXMATRIX& world, DrawType dt, bool skin);

public:
	CUberEquip(obj_Player* plr);
	~CUberEquip();

	void		SetSlot(ESlot slotId, Gear* gear);
	void		SetSlot(ESlot slotId, Weapon* wpn);
	void		SetSlot(ESlot slotId, r3dMesh* mesh);

	D3DXMATRIX	getWeaponBone(const r3dSkeleton* skel, const D3DXMATRIX& offset);
	r3dPoint3D	getBonePos(int BoneID, const r3dSkeleton* skel, const D3DXMATRIX& offset);
	r3dMesh*	getSlotMesh(ESlot slotId);


	void		Draw(const r3dSkeleton* skel, const D3DXMATRIX& CharMat, bool draw_weapon, DrawType dt, bool first_person );

	int			IsLoaded() ;
};


class CUberAnim
{
  public:
	CUberData*	data_;
	obj_Player* player;
	r3dAnimation	anim;
	
	bool		IsFPSMode();

	enum
	{
		INVALID_TRACK_ID = -1
	};

	int		reloadAnimTrackID;
	int		recoilAnimTrackID;
	int		turnInPlaceTrackID;
	int		grenadePinPullTrackID;
	int		grenadeThrowTrackID;
	int		shootAnimTrackID;

	int		jumpTrackID;
	int		jumpState;
	int		jumpPlayerState; // state when player started to jump (keep to avoid starting jump idle then switch to run)
	bool		jumpWeInAir;     // flag that we finally in air (started to jump upward)
	float		jumpAirTime;
	int		jumpMoveTrackID; // track id of underlying lower body anim
	int		jumpMoveTrackID2;// track id of underlying upper body anim

	//int		bombPlantingTrackID;

	int		AnimPlayerState;
	int		AnimMoveDir;
	
	// right now we have same speed for all-direction movement, so only one speed per state
	float		AnimSpeedStates[PLAYER_NUM_STATES];
	float		AnimSpeedRunFwd;
	void		FillAnimStatesSpeed();
	
	// grenade throw frames for each anim
	float		GetGrenadeLaunchFrame();
	
	// current weapon
	const Weapon* CurrentWeapon;
	
	// special flag for UI idles...
	int		IsInUI;

	void		SwitchToState(int PlayerState, int MoveDir);
	
  public:
	CUberAnim(obj_Player* in_player, CUberData* in_data);
	~CUberAnim();

	int		GetBoneID(const char* Name) const;

	void		SyncAnimation(int PlayerState, int MoveDir, bool force, const Weapon* weap, bool isInAttmMenu);
	void		StartGrenadePinPullAnimation();
	void		StartGrenadeThrowAnimation();
	void		StopGrenadeAnimations();
	bool		IsGrenadeLaunched();
	int			GetGrenadeAnimState();
	//void		StartBombPlantingAnimation(int bombState); // bombState: 0-start, 1-loop, 2-end, 3-stop everything
	void		StartTurnInPlaceAnim();
	void		StopTurnInPlaceAnim();
	void		UpdateTurnInPlaceAnim();

	bool		scaleReloadAnimTime;
	void		StartReloadAnim();
	void		StopReloadAnim();

	void		StartShootAnim();
	void		StopShootAnim();
	void		updateShootAnim(bool disable_loop);

	void		StartRecoilAnim();
	
	float		jumpAnimSpeed;
	float		jumpStartTimeByState[2];
	float		jumpStartTime;
	void		StartJump();
		void		StartHands();
	void		UpdateJump(bool bOnGround);
	
	void		StartDeathAnim();

	bool		IsGrenadePinPullActive();
	bool		IsGrenadePinPullFinished();
};
