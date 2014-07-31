//=========================================================================
//	Module: obj_Zombie.h
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#pragma once

#include "multiplayer/P2PMessages.h"
#include "multiplayer/NetCellMover.h"
#include "ZombieStates.h"

class r3dPhysSkeleton;

class obj_Zombie: public GameObject
{
	DECLARE_CLASS(obj_Zombie, GameObject)
	
private:
	bool	m_isFemale;

	// sounds
	float m_sndMaxDistIdle;
	float m_sndMaxDistChase;
	float m_sndMaxDistAttack;
	float m_sndMaxDistAll;

	void* m_sndIdleHandle;
	void* m_sndChaseHandle;
	void* m_sndAttackHandle;
	float m_sndAttackLength;
	float m_sndAttackNextPlayTime;
	void* m_sndDeathHandle;
	void* m_sndHurtHandle;

	void UpdateSounds();

	void DestroySounds();
	void PlayAttackSound();
	void PlayDeathSound();
	bool isSoundAudible();
	
	bool		gotDebugMove;
	PKT_S2C_Zombie_DBG_AIInfo_s dbgAiInfo;

public:
	void PlayHurtSound();

	PKT_S2C_CreateZombie_s CreateParams;

	/**	Zombie animation system. */
	r3dAnimation	anim_;

	int physSkeletonIndex;
	int isPhysInRagdoll;

	r3dMesh*	zombieParts[5]; //head/body/legs/special
	bool		gotLocalBbox;
	float		walkAnimCoef;
	void		UpdateAnimations();
	int		AddAnimation(const char* anim);

	void		StartWalkAnim(bool run);

	CNetCellMover	netMover;
	void		ProcessMovement();

	int		PhysXObstacleIndex;

	r3dPoint3D	lastTimeHitPos;
	int		lastTimeDmgType;
	int		lastTimeHitBoneID;
	int		staggeredTrack;
	int		attackTrack;
	float		walkSpeed;

	int		ZombieState;
	float		StateTimer;

	bool	bDead;

	int UpdateWarmUp;
	int PhysicsOn;

	struct ZombieSortEntry
	{
		obj_Zombie*	zombie;
		float		distance;
	};

	static r3dTL::TArray< ZombieSortEntry > ZombieList;

	struct CacheEntry
	{
		r3dPhysSkeleton*	skeleton;
		bool				allocated;
	};

	static r3dTL::TArray< CacheEntry > PhysSkeletonCache;

	static void InitPhysSkeletonCache( float progressStart, float progressEnd );

	static void FreePhysSkeletonCache();

	static void ReassignSkeletons();

	void		LinkSkeleton( int cacheIndex );
	void		UnlinkSkeleton();

	void		SwitchPhysToRagdoll( bool ragdollOn, r3dPoint3D* hitForce, int boneID );

	void		SetZombieState(int state, bool atStart);
	
	void		DoDeath();

	r3dPhysSkeleton* GetPhysSkeleton() const;

	virtual	BOOL	OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
	void		 OnNetPacket(const PKT_S2C_MoveTeleport_s& n);
	void		 OnNetPacket(const PKT_C2C_MoveSetCell_s& n);
	void		 OnNetPacket(const PKT_C2C_MoveRel_s& n);
	void		 OnNetPacket(const PKT_S2C_ZombieSetState_s& n);
	void		OnNetAttackStart();

public:
	obj_Zombie();
	~obj_Zombie();

	virtual	BOOL OnCreate();
	virtual BOOL Update();
	virtual BOOL OnDestroy();
	virtual void OnPreRender();

	virtual void AppendShadowRenderables(RenderArray &rarr, const r3dCamera& cam);
	virtual void AppendRenderables(RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& cam);

#ifndef FINAL_BUILD
	void DrawDebugInfo() const;
#endif
};

