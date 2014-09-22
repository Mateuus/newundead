//=========================================================================
//	Module: obj_Vehicle.hpp
//	Copyright (C) 2011. Online Warmongers Group Inc. All rights reserved
//=========================================================================

#pragma once

#include "GameObj.h"
#include "multiplayer/NetCellMover.h"
#include "NetworkHelper.h"
#include "obj_ServerVehicleSpawn.h"
#include "multiplayer/P2PMessages.h"
//////////////////////////////////////////////////////////////////////////

#if VEHICLES_ENABLED

class obj_ServerVehicleSpawn;
//class VehicleManager;

struct VehicleDescriptor;

class obj_Vehicle: public MeshGameObject, INetworkHelper // Server Vehicles
{
	DECLARE_CLASS(obj_Vehicle, MeshGameObject)

	CNetCellMover	netMover;
	r3dPoint3D	netVelocity;
	obj_ServerVehicleSpawn* spawnObject;
	int OccupantsVehicle;
	float Gasolinecar;
	float DamageCar;
	float FirstGasolinecar;
	float FirstDamageCar;
	r3dVector FirstRotationVector;
	r3dPoint3D FirstPosition;
	float curTime;
		obj_ServerVehicleSpawn* spawner;
	int PlayersOnVehicle[8];
	int HaveDriver;

public:
	obj_Vehicle();
	~obj_Vehicle();

	DWORD		peerId_;

    virtual	BOOL Load(const char *name);
	virtual BOOL Update();
	virtual BOOL OnCreate();
	bool bOn;

	void setVehicleSpawner( obj_ServerVehicleSpawn* targetSpawner) { spawner = targetSpawner;}
	void OnNetPacket(const PKT_C2C_MoveSetCell_s& n);
	void OnNetPacket(const PKT_C2C_MoveRel_s& n);
	BOOL OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
	void RelayPacket(const DefaultPacket* packetData, int packetSize, bool guaranteedAndOrdered = true);
	void OnNetPacket(const PKT_S2C_PositionVehicle_s& n); // Server Vehicles

	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); } // Server Vehicles
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size); // Server Vehicles
    float		m_VehicleRotation;

private:


};

#endif // VEHICLES_ENABLED