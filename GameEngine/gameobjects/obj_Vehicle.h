//=========================================================================
//	Module: obj_Vehicle.hpp
//	Copyright (C) 2011. Online Warmongers Group Inc. All rights reserved
//=========================================================================

#pragma once

#include "GameObj.h"
#include "vehicle/PxVehicleDrive.h"
#include "../../EclipseStudio/Sources/multiplayer/NetCellMover.h"
#include "../../EclipseStudio/Sources/ObjectsCode/Gameplay/SharedUsableItem.h"
#include "../../EclipseStudio/Sources/ObjectsCode/EFFECTS/obj_ParticleSystem.h"
#include "../../EclipseStudio/Sources/ObjectsCode/world/Lamp.h"
//////////////////////////////////////////////////////////////////////////

#if VEHICLES_ENABLED

class obj_VehicleSpawn;
class obj_Player;
struct VehicleDescriptor;

//class obj_Vehicle: public MeshGameObject
class obj_Vehicle: public SharedUsableItem
{
	DECLARE_CLASS(obj_Vehicle, SharedUsableItem)
	//DECLARE_CLASS(obj_Vehicle, MeshGameObject)

	VehicleDescriptor *vd;
	int Occupants;
	int weaponID2;
	float GasolineCar;
	float DamageCar;
	float extime;
	bool ExitVehicle;
	float oldSpeed;
	bool oldInAir;
	float falltime;
	float RPMPlayer;
	float RotationSpeed;
	float curTime;
	r3dVector FirstRotationVector;
	r3dPoint3D FirstPosition;
	obj_Building * CollisionCar;
	bool enablesound;
	bool DestroyOnWater;
	float BombTime;
	bool othershavesound;
	float	m_sndBreathBaseVolume;
	bool Collobject;

	CNetCellMover	netMover;
	r3dPoint3D	netVelocity;
	void SyncPhysicsPoseWithObjectPose();
	void SetBoneMatrices();

public:
	obj_Vehicle();
	~obj_Vehicle();
	virtual BOOL Update();
	virtual BOOL OnCreate();
	virtual BOOL OnDestroy();
	virtual void SetPosition(const r3dPoint3D& pos);
	virtual	void SetRotationVector(const r3dVector& Angles);
	virtual void OnPreRender() { SetBoneMatrices(); }
	void ExternalSounds();
	void LocalSounds();
	void ApplyDamage(int vehicleID, int weaponID);
	void ExplosionBlast(r3dPoint3D pos);
	void LightOnOff();
	bool bOn;
	class obj_ParticleSystem* m_ParticleTracer;
	class obj_ParticleSystem* m_Particlebomb;
	class obj_ParticleSystem* m_ParticleSmoke;
	class obj_ParticleSystem* m_SmallFire;
	class obj_LightHelper* Light;
	void*   m_sndVehicleFire;
	void*	m_VehicleSnd;
	float	rpm;
	bool HaveDriver;

	void SwitchToDrivable(bool doDrive);
	const VehicleDescriptor* getVehicleDescriptor() { return vd; }

#ifndef FINAL_BUILD
	float DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected);
#endif
	const bool getExitSpace( r3dVector& outVector, int exitIndex );
	void setVehicleSpawner( obj_VehicleSpawn* targetSpawner) { spawner = targetSpawner;}
	void UpdatePositionFromRemote();
	void UpdatePositionFromPhysx();
	BOOL OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
private:
	obj_VehicleSpawn* spawner;
	r3dPoint3D lastPos;

};

#endif // VEHICLES_ENABLED