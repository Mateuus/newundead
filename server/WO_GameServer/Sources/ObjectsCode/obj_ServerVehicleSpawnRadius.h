#pragma once

#include "GameCommon.h"
#include "../EclipseStudio/Sources/ObjectsCode/Gameplay/BasePlayerSpawnPoint.h"

class obj_ServerVehicleSpawnRadius : public BasePlayerSpawnPoint
{
	DECLARE_CLASS(obj_ServerVehicleSpawnRadius, BasePlayerSpawnPoint)


public:
	obj_ServerVehicleSpawnRadius();
	~obj_ServerVehicleSpawnRadius();
	
	virtual BOOL		Load(const char* name);
	virtual	void		ReadSerializedData(pugi::xml_node& node);
	virtual BOOL		OnCreate();
	char				vehicle_Model[64];
	float		useRadius;
	float minVehicle;
	float maxVehicle;
};
