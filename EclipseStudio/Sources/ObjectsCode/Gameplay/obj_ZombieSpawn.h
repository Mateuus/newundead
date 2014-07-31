//=========================================================================
//	Module: obj_ZombieSpawn.h
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#pragma once

#include "GameCommon.h"
#include "gameobjects/GameObj.h"

//////////////////////////////////////////////////////////////////////////

class obj_ZombieSpawn: public GameObject
{
	DECLARE_CLASS(obj_ZombieSpawn, GameObject)

	/**	Zombie spawn radius. */
	float spawnRadius;

	/**	Max zombie count. */
	uint32_t maxZombieCount;

	/**	Minmax zombie parameters. */
	float minDetectionRadius;
	float maxDetectionRadius;

	/**	 Zombie spawn rate defined as how OFTEN zombies are spawned */
	float zombieSpawnDelay;
	
	float sleepersRate;
	int lootBoxID;
	float fastZombieChance;
	float speedVariation;

	std::vector<uint32_t> ZombieSpawnSelection; // which zombies should be spawned

public:
	obj_ZombieSpawn();
	~obj_ZombieSpawn();

	virtual void AppendRenderables(RenderArray (& render_arrays  )[ rsCount ], const r3dCamera& Cam);
#ifndef FINAL_BUILD
	virtual float DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected);
#endif
	virtual BOOL OnCreate();
	virtual BOOL Update();
	virtual BOOL OnDestroy();
	virtual	void ReadSerializedData(pugi::xml_node& node);
	virtual void WriteSerializedData(pugi::xml_node& node);
};

