#include "r3dPCH.h"
#include "r3d.h"

#include "Gameplay_Params.h"
#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "obj_ServerVehicleSpawn.h"
#include "obj_ServerVehicle.h"

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
 if(!parent::Load(fname))
  return FALSE;

 return TRUE;
}

BOOL obj_ServerVehicleSpawn::OnCreate()
{
	parent::OnCreate();

	UpdateTransform();

	return 1;
}

BOOL obj_ServerVehicleSpawn::Update()
{
	return parent::Update();
}

void obj_ServerVehicleSpawn::ReadSerializedData(pugi::xml_node& node)
{
	GameObject::ReadSerializedData(node);
	pugi::xml_node myNode = node.child("VehicleSpawn");
	_snprintf_s( vehicle_Model, 64, "%s", myNode.attribute("VehicleModel").value());
	pugi::xml_node PositionNode =  node.child("position");

	pos.x = PositionNode.attribute("x").as_float();
	pos.y = PositionNode.attribute("y").as_float();
	pos.z = PositionNode.attribute("z").as_float();
	RespawnCar();

}

void obj_ServerVehicleSpawn::SetVehicleType( const std::string& preset )
{
	_snprintf_s( vehicle_Model, 64, "%s", preset.c_str() );
	RespawnCar();
}

void obj_ServerVehicleSpawn::RespawnCar()
{
#if VEHICLES_ENABLED

	/*if( spawnedVehicle ) {
		spawnedVehicle->setVehicleSpawner( NULL );
		GameWorld().DeleteObject( spawnedVehicle );
		spawnedVehicle = NULL;
	}*/

	if( vehicle_Model[0] != '\0' ) {

	    /*r3dOutToLog("////////////////////////////////\n");
	    r3dOutToLog("Vehicle Model: %s\n",vehicle_Model);
	    r3dOutToLog("   Position X: %f\n",pos.x);
	    r3dOutToLog("   Position Y: %f\n",pos.y);
	    r3dOutToLog("   Position Z: %f\n",pos.z);
	    r3dOutToLog("////////////////////////////////\n");*/
		r3dOutToLog("Vehicle Model Created: %s\n",vehicle_Model);
		spawnedVehicle = static_cast<obj_Vehicle*> ( srv_CreateGameObject("obj_Vehicle", vehicle_Model, pos ));
		spawnedVehicle->NetworkLocal = true;  // Server Vehicles
		spawnedVehicle->setVehicleSpawner( this );
		spawnedVehicle->m_isSerializable = false;
		spawnedVehicle->FirstPosition = pos;
		spawnedVehicle->FirstRotationVector = GetRotationVector();
		spawnedVehicle->OnCreate();
		spawnedVehicle->SetNetworkID(gServerLogic.net_lastFreeId++);
	}
#endif
}


