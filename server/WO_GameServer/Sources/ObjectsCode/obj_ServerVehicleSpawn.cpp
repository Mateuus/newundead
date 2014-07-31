#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"
#include "ObjectsCode/sobj_Vehicle.h"
#include "obj_ServerVehicleSpawn.h"

IMPLEMENT_CLASS(obj_ServerVehicleSpawn, "obj_VehicleSpawn", "Object");
AUTOREGISTER_CLASS(obj_ServerVehicleSpawn);

obj_ServerVehicleSpawn::obj_ServerVehicleSpawn()
{
}

obj_ServerVehicleSpawn::~obj_ServerVehicleSpawn()
{
}
BOOL obj_ServerVehicleSpawn::Load(const char *fname)
{
	// skip mesh loading
	if(!GameObject::Load("Data\\ObjectsDepot\\Vehicles\\Zombie_killer_car.sco")) 
		return FALSE;

	return TRUE;
}

BOOL obj_ServerVehicleSpawn::OnCreate()
{
obj_Vehicle* obj = 	(obj_Vehicle*)srv_CreateGameObject("obj_Vehicle", "obj_Vehicle", GetPosition());
obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	obj->OnCreate();
	OnDestroy();
	return 1;
}