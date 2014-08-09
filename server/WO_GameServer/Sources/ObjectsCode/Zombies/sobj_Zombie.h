#pragma once

#include "GameCommon.h"
#include "multiplayer/NetCellMover.h"
#include "NetworkHelper.h"

class obj_ZombieSpawn;
class obj_ServerPlayer;
class ServerWeapon;
class ZombieNavAgent;

#include "ZombieNavAgent.h"

class obj_Zombie : public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_Zombie, GameObject)

public:
	obj_ZombieSpawn* spawnObject;
	CNetCellMover	netMover;
	
	r3dMesh*	zombieParts[5];
	bool		HalloweenZombie;
	int		HeroItemID;
	int		HeadIdx;
	int		BodyIdx;
	int		LegsIdx;

	int		ZombieDisabled;	// zombie can't move, stuck
	void		DisableZombie();
	
	int		ZombieState;
	float		StateStartTime;
	float		StateTimer;
	void		SwitchToState(int in_state);

	r3dPoint3D VehicleLastPost;//Codex Carros

	float		ZombieHealth;

	int		FastZombie;
	float		WalkSpeed;
	float		RunSpeed;
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
	bool		CheckForBarricadeBlock();
	GameObject*	  FindBarricade();
	
	float		staggerTime;
	int		animState;

	float		DetectRadius;		// smell detection radius
	gobjid_t	hardObjLock;
	r3dPoint3D	lastTargetPos;
	float		nextDetectTime;
	float		attackTimer;
	int		isFirstAttack;
	GameObject*	ScanForTarget(bool immidiate = false);
	bool		CheckViewToPlayer(const GameObject* obj);
	GameObject*	GetClosestPlayerBySenses();
	bool		  IsPlayerDetectable(const obj_ServerPlayer* plr, float dist);
	bool		SenseWeaponFire(const obj_ServerPlayer* plr, const ServerWeapon* wpn);
	bool		StartAttack(const GameObject* trg);
	void		StopAttack();
	
	void		DoDeath();
	
	void		AILog(int level, const char* fmt, ...);
	void		SendAIStateToNet();
	void		DebugSingleZombie();
public:
	obj_Zombie();
	~obj_Zombie();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	
	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);

virtual	BOOL		OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
	
	bool		ApplyDamage(GameObject* fromObj, float damage, int bodyPart, STORE_CATEGORIES damageSource,bool isSpecialBool);
};
