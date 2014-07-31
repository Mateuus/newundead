#pragma once

#include "GameCommon.h"
#include "multiplayer/NetCellMover.h"
#include "NetworkHelper.h"

class obj_ZombieSpawn;
class obj_ServerPlayer;
class ServerWeapon;
class ZombieNavAgent;

#include "Zombies/ZombieNavAgent.h"

class obj_Animals : public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_Animals, GameObject)

public:
		CNetCellMover	netMover;
		obj_Animals();
	~obj_Animals();
	ZombieNavAgent* navAgent;
	int animalstate;
	float nextWalkTime;
	r3dPoint3D moveTargetPos;
	gobjid_t	hardObjLock;
	r3dPoint3D LastNetPos;
	float dietime;
	obj_ZombieSpawn* z;
	int laststate;
	r3dVector lastangle;
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	float Health;
	float lastHit;
	bool bDead;
	bool GetFreePoint(r3dPoint3D* out_pos , float length);
	void ApplyDamage(float damage , GameObject* fromObj);
	void DoDeath(GameObject* fromObj);
	
	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);

//virtual	BOOL		OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
};