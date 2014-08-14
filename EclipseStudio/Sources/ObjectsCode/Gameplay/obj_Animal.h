//=========================================================================
//	Module: obj_Animal.h
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#pragma once

#include "multiplayer/P2PMessages.h"
#include "multiplayer/NetCellMover.h"
#include "AnimalStates.h"

class r3dPhysSkeleton;

class obj_Animal: public GameObject
{
	DECLARE_CLASS(obj_Animal, GameObject)
	
private:

	bool		gotDebugMove;
	PKT_S2C_Zombie_DBG_AIInfo_s dbgAiInfo;

public:
	void PlayHurtSound();

	PKT_S2C_CreateZombie_s CreateParams;

	/**	Zombie animation system. */
	r3dAnimation	anim_;
	class GameObject*	Collision;

	int physSkeletonIndex;
	int isPhysInRagdoll;

	r3dMesh*	deerModel; //head/body/legs/special
	bool		gotLocalBbox;
	float		walkAnimCoef;
	void		UpdateAnimations();
	int		AddAnimation(const char* anim);
	int DeerCreated;
	int DeerHaveCollision;

	void		StartWalkAnim(bool run);
	void		StartSprintJumpAnim(bool run);

	CNetCellMover	netMover;
	void		ProcessMovement();

	int		PhysXObstacleIndex;

	r3dPoint3D	lastTimeHitPos;
	int		lastTimeDmgType;
	int		lastTimeHitBoneID;
	int		staggeredTrack;
	int		attackTrack;
	float		walkSpeed;

	int		AnimalState;
	float		StateTimer;

	bool	bDead;

	int UpdateWarmUp;
	int PhysicsOn;

	struct ZombieSortEntry
	{
		obj_Animal*	zombie;
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

	void		SetAnimalState(int state, bool atStart);
	
	void		DoDeath();

	r3dPhysSkeleton* GetPhysSkeleton() const;

	virtual	BOOL	OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
	void		 OnNetPacket(const PKT_S2C_MoveTeleport_s& n);
	void		 OnNetPacket(const PKT_C2C_MoveSetCell_s& n);
	void		 OnNetPacket(const PKT_C2C_MoveRel_s& n);
	void		 OnNetPacket(const PKT_S2C_AnimalSetState_s& n);

public:
	obj_Animal();
	~obj_Animal();

	virtual	BOOL OnCreate();
	virtual BOOL Update();
	virtual BOOL OnDestroy();
	virtual void OnPreRender();

	virtual void AppendShadowRenderables(RenderArray &rarr, const r3dCamera& cam);
	virtual void AppendRenderables(RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& cam);

};

