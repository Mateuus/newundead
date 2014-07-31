#pragma once

#include "GameCommon.h"
#include "../EclipseStudio/Sources/ObjectsCode/Gameplay/BaseItemSpawnPoint.h"

class LootBoxConfig;

class obj_ServerItemSpawnPoint : public BaseItemSpawnPoint
{
	DECLARE_CLASS(obj_ServerItemSpawnPoint, BaseItemSpawnPoint)

	float m_spawnAllItemsAtTime;
	float m_nextUpdateTime;
	LootBoxConfig*	m_LootBoxConfig;
public:
	obj_ServerItemSpawnPoint();
	~obj_ServerItemSpawnPoint();
	
	virtual BOOL	Load(const char* name);
	virtual BOOL	OnCreate();
	virtual BOOL	Update();
	
	void		SpawnItem(int spawnIndex);

	void		PickUpObject(int spawnIndex);
};
