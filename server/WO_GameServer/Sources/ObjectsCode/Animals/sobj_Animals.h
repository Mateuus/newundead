#pragma once

#include "GameCommon.h"
#include "multiplayer/NetCellMover.h"
#include "NetworkHelper.h"

class obj_AnimalSpawn;
class obj_ServerPlayer;
class ServerWeapon;
class ZombieNavAgent;

#include "C:\UndeadBrasil\src\server\WO_GameServer\Sources\ObjectsCode\Zombies\ZombieNavAgent.h"

class obj_Animal : public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_Animal, GameObject)

public:
	obj_AnimalSpawn* spawnObject;
	CNetCellMover	netMover;

	bool		HalloweenZombie;
	int		HeroItemID;
	int		HeadIdx;
	int		BodyIdx;
	int		LegsIdx;
	float lastHit;
	float nextWalkTime;

	int		ZombieDisabled;	// zombie can't move, stuck
	void		DisableZombie();
	
	int		AnimalState;
	float		StateStartTime;
	float		SprintTime;
	float		SprintJumpTime;
	float		StateTimer;
	bool SprintStart;
	bool SprintJumpStart;
	void		SwitchToState(int in_state);

	float		AnimalHealth;

	int		FastZombie;
	float		WalkSpeed;
	float		RunSpeed;
	float		SprintJumpSpeed;
	ZombieNavAgent* navAgent;
	int		patrolPntIdx;
	float		moveWatchTime;
	r3dPoint3D	moveWatchPos;
	float		moveStartTime;
	r3dPoint3D	moveStartPos;
	float		moveAvoidTime;
	r3dPoint3D	moveAvoidPos;
	r3dPoint3D	moveTargetPos;
	int		moveFrameCount;

	void		CreateNavAgent();
	void		StopNavAgent();
	void		SetNavAgentSpeed(float speed);
	bool		MoveNavAgent(const r3dPoint3D& pos, float maxAstarRange);
	void		FaceVector(const r3dPoint3D& v);
	int		CheckMoveStatus();
	int		CheckMoveWatchdog();
	static bool	CheckNavPos(r3dPoint3D& pos);
	
	float		staggerTime;
	int		animState;

	float		DetectRadius;		// smell detection radius
	gobjid_t	hardObjLock;
	r3dPoint3D	lastTargetPos;
	float		nextDetectTime;
	GameObject*	ScanForTarget(bool immidiate = false);
	bool		CheckViewToPlayer(const GameObject* obj);
	GameObject*	GetClosestPlayerBySenses();
	bool		  IsPlayerDetectable(const obj_ServerPlayer* plr, float dist);
	bool		SenseWeaponFire(const obj_ServerPlayer* plr, const ServerWeapon* wpn);
	bool GetFreePoint(r3dPoint3D* out_pos , float length);
	
	void		DoDeath();
	
	void		AILog(int level, const char* fmt, ...);
	void		SendAIStateToNet();

public:
	obj_Animal();
	~obj_Animal();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	
	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);

virtual	BOOL		OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
	
	bool		ApplyDamage(GameObject* fromObj, float damage, int bodyPart, STORE_CATEGORIES damageSource,bool isSpecialbool);
};
