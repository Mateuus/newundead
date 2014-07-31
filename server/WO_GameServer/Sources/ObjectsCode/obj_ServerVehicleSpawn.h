#pragma once

#include "GameCommon.h"
#include "../EclipseStudio/Sources/ObjectsCode/Gameplay/BasePlayerSpawnPoint.h"

class obj_ServerVehicleSpawn : public BasePlayerSpawnPoint
{
	DECLARE_CLASS(obj_ServerVehicleSpawn, BasePlayerSpawnPoint)


public:
	obj_ServerVehicleSpawn();
	~obj_ServerVehicleSpawn();
	
	virtual BOOL		Load(const char* name);
	
	virtual BOOL		OnCreate();

};
