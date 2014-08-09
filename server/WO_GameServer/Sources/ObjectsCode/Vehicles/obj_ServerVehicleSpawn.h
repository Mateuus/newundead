#pragma once

#include "GameCommon.h"

class obj_Vehicle;

// class to drop permanent weapon drops on server
// client will ignore this object, it is only loaded on server!
class obj_ServerVehicleSpawn : public GameObject
{
	DECLARE_CLASS(obj_ServerVehicleSpawn, GameObject)

char				vehicle_Model[64],PositionX[64],PositionY[64],PositionZ[64];
r3dPoint3D pos;
r3dVector	VehicleRot;

public:
	obj_ServerVehicleSpawn();
	virtual ~obj_ServerVehicleSpawn();

	virtual	BOOL		OnCreate();
	virtual	BOOL		Load(const char *name);

	virtual	void		ReadSerializedData(pugi::xml_node& node);

	virtual BOOL		Update();

    void RespawnCar();

	void SetVehicleType( const std::string& preset );


private:

      obj_Vehicle*		spawnedVehicle;

};