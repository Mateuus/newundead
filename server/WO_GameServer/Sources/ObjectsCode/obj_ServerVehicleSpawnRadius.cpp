#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"
#include "sobj_Vehicle.h"
#include "obj_ServerVehicleSpawnRadius.h"
#include "XMLHelpers.h"

IMPLEMENT_CLASS(obj_ServerVehicleSpawnRadius, "obj_VehicleSpawnRadius", "Object");
AUTOREGISTER_CLASS(obj_ServerVehicleSpawnRadius);

obj_ServerVehicleSpawnRadius::obj_ServerVehicleSpawnRadius()
{
}

obj_ServerVehicleSpawnRadius::~obj_ServerVehicleSpawnRadius()
{
}
BOOL obj_ServerVehicleSpawnRadius::Load(const char *fname)
{
	// skip mesh loading
	if(!GameObject::Load("Data\\ObjectsDepot\\Vehicles\\Zombie_killer_car.sco")) 
		return FALSE;

	return TRUE;
}

BOOL obj_ServerVehicleSpawnRadius::OnCreate()
{
	// not do anything
	/*obj_Vehicle* obj = 	(obj_Vehicle*)srv_CreateGameObject("obj_Vehicle", "obj_Vehicle", GetPosition());
	obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	obj->OnCreate();
	OnDestroy();*/
	return 1;
}
void obj_ServerVehicleSpawnRadius::ReadSerializedData(pugi::xml_node& node)
{
	GameObject::ReadSerializedData(node);
	pugi::xml_node objNode = node.child("VehicleSpawnRadius");
	_snprintf_s( vehicle_Model, 64, "%s", objNode.attribute("VehicleModel").value());
	GetXMLVal("useRadius", objNode, &useRadius);
	GetXMLVal("minVehicle", objNode, &minVehicle);
	GetXMLVal("maxVehicle", objNode, &maxVehicle);
	// for spawn 1 vehicles
	r3dOutToLog("obj_ServerVehicleSpawnRadius %p min:%d max:%d model:%s pos:%.2f,%.2f,%.2f\n",this,minVehicle,maxVehicle,vehicle_Model,GetPosition().x,GetPosition().y,GetPosition().z);
	if (minVehicle == 1 && maxVehicle == 1)
	{
		r3dPoint3D pos = r3dPoint3D(GetPosition().x + u_GetRandom(0,useRadius),GetPosition().y,GetPosition().z + u_GetRandom(0,useRadius));
		// spawn on terrain
		if(Terrain)
		{
			float terrHeight = Terrain->GetHeight(pos);
			pos.y = terrHeight;
		}
		obj_Vehicle* obj = 	(obj_Vehicle*)srv_CreateGameObject("obj_Vehicle", "obj_Vehicle", pos);
		sprintf(obj->vehicle_Model,vehicle_Model);
		obj->SetNetworkID(gServerLogic.GetFreeNetId());
		obj->NetworkLocal = true;
		obj->OnCreate();
		r3dOutToLog("obj_ServerVehicleSpawnRadius %p spawned vehicle %p pos:%.2f,%.2f,%.2f\n",this,obj,obj->GetPosition().x,obj->GetPosition().y,obj->GetPosition().z);
		return;
	}
	for (int idx=0; idx < u_GetRandom(minVehicle,maxVehicle); idx++)
	{
		r3dPoint3D pos = r3dPoint3D(GetPosition().x + u_GetRandom(0,useRadius),GetPosition().y,GetPosition().z + u_GetRandom(0,useRadius));
		// spawn on terrain
		if(Terrain)
		{
			float terrHeight = Terrain->GetHeight(pos);
			pos.y = terrHeight;
		}
		obj_Vehicle* obj = 	(obj_Vehicle*)srv_CreateGameObject("obj_Vehicle", "obj_Vehicle", pos);
		sprintf(obj->vehicle_Model,vehicle_Model);
		obj->SetNetworkID(gServerLogic.GetFreeNetId());
		obj->NetworkLocal = true;
		obj->OnCreate();
		r3dOutToLog("obj_ServerVehicleSpawnRadius %p spawned vehicle %p pos:%.2f,%.2f,%.2f\n",this,obj,obj->GetPosition().x,obj->GetPosition().y,obj->GetPosition().z);
	}
	parent::OnDestroy();
}