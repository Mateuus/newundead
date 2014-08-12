#pragma once

#include "GameCommon.h"

class obj_Animal;
class LootBoxConfig;
class obj_AnimalSpawn : public GameObject
{
	DECLARE_CLASS(obj_AnimalSpawn, GameObject)

public:
	int		numSpawnedZombies;
	std::vector<obj_Animal*> zombies;
	float		timeSinceLastSpawn;

	float		spawnRadius;
	size_t		maxZombieCount;
	float		zombieSpawnDelay; // Zombie spawn rate defined as how OFTEN zombies are spawned
	//float		sleepersRate;

	std::vector<uint32_t> ZombieSpawnSelection; // which zombies should be spawned

	float		minDetectionRadius;
	float		maxDetectionRadius;

	//float		minSpeed;
	//float		maxSpeed;
	float		fastZombieChance;
	float		speedVariation;
	
	int		lootBoxID;
	const LootBoxConfig* lootBoxCfg;
	
	struct navPnt_s
	{
	  r3dPoint3D	pos;
	  int		marked;
	};
	std::vector<navPnt_s> navPoints;
	
	void		GenerateNavPoints();
	int		GetFreeNavPointIdx(r3dPoint3D* out_pos, bool mark, float maxDist = -1, const r3dPoint3D* cur_pos = NULL);
	void		ReleaseNavPoint(int idx);

	bool		GetZombieSpawnPosition(r3dPoint3D* out_pos);
	bool		GetZombieRandomNavMeshPosition(r3dPoint3D* out_pos);

	int		CheckForZombieAtPos(const r3dPoint3D& pos, float distSq);

public:
	obj_AnimalSpawn();
	~obj_AnimalSpawn();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	virtual	void	ReadSerializedData(pugi::xml_node& node);
	
	void		OnZombieKilled(const obj_Animal* d);
};
