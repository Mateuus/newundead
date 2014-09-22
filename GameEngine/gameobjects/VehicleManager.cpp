//=========================================================================
//	Module: VehicleManager.cpp
//	Copyright (C) 2012.
//=========================================================================

#include "r3dPCH.h"

#if VEHICLES_ENABLED

#include "r3d.h"
#include "extensions/PxStringTableExt.h"
#include "PhysXWorld.h"
#include "PhysXRepXHelpers.h"
#include "ObjManag.h"
#include "vehicle/PxVehicleSDK.h"
#include "geometry/PxConvexMeshGeometry.h"
#include "PxBatchQueryDesc.h"

#include "VehicleManager.h"
#include "obj_Vehicle.h"
#include "VehicleDescriptor.h"
#include "../../EclipseStudio/Sources/ui/HUDPause.h"
#include "../../EclipseStudio/Sources/ui/HUDDisplay.h"
#include "../../EclipseStudio/Sources/ObjectsCode/AI/AI_Player.h"
#include "../../EclipseStudio/Sources/multiplayer/ClientGameLogic.h"

extern HUDPause*	hudPause;
extern HUDDisplay*	hudMain;

//////////////////////////////////////////////////////////////////////////

namespace
{
	void ConvertCoordinateSystems(PxRigidDynamic *a);

	physx::PxVehicleDrivableSurfaceType PX_ALIGN(16, drivableSurfaceType);

	const int MAX_NUM_SURFACE_TYPES = 1;
	const int MAX_NUM_TYRE_TYPES = 1;
	PxF32 gTyreFrictionMultipliers[MAX_NUM_SURFACE_TYPES][MAX_NUM_TYRE_TYPES] =
	{
		3.0f//, 1.5f, 1.5f, 1.5f
	};

//////////////////////////////////////////////////////////////////////////

	void DriveVehiclesChangeCallback(int oldI, float oldF)
	{
		/*ObjectManager& GW = GameWorld();
		for (GameObject *obj = GW.GetFirstObject(); obj; obj = GW.GetNextObject(obj))
		{
			if(obj->isObjType(OBJTYPE_Vehicle))
			{
				obj_Vehicle * vh = static_cast<obj_Vehicle*>(obj);
				vh->SwitchToDrivable(d_drive_vehicles->GetBool());
			}
		}*/
		ObjectManager& GW = GameWorld();
		for (GameObject *obj = GW.GetFirstObject(); obj; obj = GW.GetNextObject(obj))
		{
			if(obj->isObjType(OBJTYPE_Vehicle))
			{
				if (gClientLogic().localPlayer_)
				{
					obj_Player* plr = gClientLogic().localPlayer_;
					obj_Vehicle * vh = static_cast<obj_Vehicle*>(obj);
					if (plr->ActualVehicle != NULL && !plr->isPassenger())
					{
						if (vh->GetNetworkID() == plr->ActualVehicle->GetNetworkID())
						{
						    vh->SwitchToDrivable(d_drive_vehicles->GetBool());
							return;
						}
					}
				}
			}
		}
	}

//////////////////////////////////////////////////////////////////////////

	PxVehicleKeySmoothingData gKeySmoothingData=
	{
		{
			3.0f,	//rise rate PX_VEHICLE_ANALOG_INPUT_ACCEL
			3.0f,	//rise rate PX_VEHICLE_ANALOG_INPUT_BRAKE
			10.0f,	//rise rate PX_VEHICLE_ANALOG_INPUT_HANDBRAKE
			2.5f,	//rise rate PX_VEHICLE_ANALOG_INPUT_STEER_LEFT
			2.5f	//rise rate PX_VEHICLE_ANALOG_INPUT_STEER_RIGHT
		},
		{
			5.0f,	//fall rate PX_VEHICLE_ANALOG_INPUT_ACCEL
			5.0f,	//fall rate PX_VEHICLE_ANALOG_INPUT_BRAKE
			10.0f,	//fall rate PX_VEHICLE_ANALOG_INPUT_HANDBRAKE
			5.0f,	//fall rate PX_VEHICLE_ANALOG_INPUT_STEER_LEFT
			5.0f	//fall rate PX_VEHICLE_ANALOG_INPUT_STEER_RIGHT
		}
	};

	PxF32 gSteerVsForwardSpeedData[2*8]=
	{
		0.0f,		0.75f,
		5.0f,		0.75f,
		30.0f,		0.125f,
		120.0f,		0.1f,
		PX_MAX_F32, PX_MAX_F32,
		PX_MAX_F32, PX_MAX_F32,
		PX_MAX_F32, PX_MAX_F32,
		PX_MAX_F32, PX_MAX_F32
	};
	PxFixedSizeLookupTable<8> gSteerVsForwardSpeedTable(gSteerVsForwardSpeedData,4);

//////////////////////////////////////////////////////////////////////////

	void ComputeWheelWidthsAndRadii(const PxConvexMesh** wheelConvexMeshes, PxF32* wheelWidths, PxF32* wheelRadii, PxU32 numWheels)
	{
		for(PxU32 i=0;i<numWheels;i++)
		{
			const PxU32 numWheelVerts=wheelConvexMeshes[i]->getNbVertices();
			const PxVec3* wheelVerts=wheelConvexMeshes[i]->getVertices();
			PxVec3 wheelMin(PX_MAX_F32,PX_MAX_F32,PX_MAX_F32);
			PxVec3 wheelMax(-PX_MAX_F32,-PX_MAX_F32,-PX_MAX_F32);
			for(PxU32 j=0;j<numWheelVerts;j++)
			{
				wheelMin.x=PxMin(wheelMin.x,wheelVerts[j].x);
				wheelMin.y=PxMin(wheelMin.y,wheelVerts[j].y);
				wheelMin.z=PxMin(wheelMin.z,wheelVerts[j].z);
				wheelMax.x=PxMax(wheelMax.x,wheelVerts[j].x);
				wheelMax.y=PxMax(wheelMax.y,wheelVerts[j].y);
				wheelMax.z=PxMax(wheelMax.z,wheelVerts[j].z);
			}
			wheelWidths[i] = wheelMax.x - wheelMin.x;
			wheelRadii[i] = PxMax(wheelMax.y,wheelMax.z) * 0.975f;
		}
	}

//////////////////////////////////////////////////////////////////////////

	PxVec3 ComputeChassisAABBDimensions(const PxConvexMesh* chassisConvexMesh)
	{
		const PxU32 numChassisVerts=chassisConvexMesh->getNbVertices();
		const PxVec3* chassisVerts=chassisConvexMesh->getVertices();
		PxVec3 chassisMin(PX_MAX_F32,PX_MAX_F32,PX_MAX_F32);
		PxVec3 chassisMax(-PX_MAX_F32,-PX_MAX_F32,-PX_MAX_F32);
		for(PxU32 i=0;i<numChassisVerts;i++)
		{
			chassisMin.x=PxMin(chassisMin.x,chassisVerts[i].x);
			chassisMin.y=PxMin(chassisMin.y,chassisVerts[i].y);
			chassisMin.z=PxMin(chassisMin.z,chassisVerts[i].z);
			chassisMax.x=PxMax(chassisMax.x,chassisVerts[i].x);
			chassisMax.y=PxMax(chassisMax.y,chassisVerts[i].y);
			chassisMax.z=PxMax(chassisMax.z,chassisVerts[i].z);
		}
		const PxVec3 chassisDims=chassisMax-chassisMin;
		return chassisDims;
	}

//////////////////////////////////////////////////////////////////////////

	PxVec3 ComputeChassisMOI(const PxVec3& chassisDims, const PxF32 chassisMass)
	{
		//We can approximately work out the chassis moment of inertia from the aabb.
		PxVec3 chassisMOI
		(
			(chassisDims.y * chassisDims.y + chassisDims.z * chassisDims.z) * chassisMass / 12.0f,
			(chassisDims.x * chassisDims.x + chassisDims.z * chassisDims.z) * chassisMass / 12.0f,
			(chassisDims.x * chassisDims.x + chassisDims.y * chassisDims.y) * chassisMass / 12.0f
		);
		//Well, the AABB moi gives rather sluggish behaviour so lets let the car turn a bit quicker.
		chassisMOI.y *= 0.8f;//
		return chassisMOI;
	}

//////////////////////////////////////////////////////////////////////////

	PxSceneQueryHitType::Enum VehicleWheelRaycastPreFilter
	(
		PxFilterData filterData0,
		PxFilterData filterData1,
		const void* constantBlock,
		PxU32 constantBlockSize,
		PxSceneQueryFilterFlags& filterFlags
	)
	{
		//filterData0 is the vehicle suspension raycast.
		//filterData1 is the shape potentially hit by the raycast.
		PX_UNUSED(filterFlags);
		PX_UNUSED(constantBlockSize);
		PX_UNUSED(constantBlock);
		PX_UNUSED(filterData0);

		PxSceneQueryHitType::Enum ht = ((0 == (filterData1.word3 & VEHICLE_DRIVABLE_SURFACE)) ? PxSceneQueryHitType::eNONE : PxSceneQueryHitType::eBLOCK);
		return ht;
	}

//////////////////////////////////////////////////////////////////////////

	PxConvexMesh* CreateConvexMesh(const PxVec3* verts, const PxU32 numVerts)
	{
		PxPhysics& physics = *g_pPhysicsWorld->PhysXSDK;
		PxCooking& cooking = *g_pPhysicsWorld->Cooking;
		// Create descriptor for convex mesh
		PxConvexMeshDesc convexDesc;
		convexDesc.points.count			= numVerts;
		convexDesc.points.stride		= sizeof(PxVec3);
		convexDesc.points.data			= verts;
		convexDesc.flags				= PxConvexFlag::eCOMPUTE_CONVEX | PxConvexFlag::eINFLATE_CONVEX;

		PxConvexMesh* convexMesh = NULL;
		PhysxUserMemoryWriteStream buf;
		if(cooking.cookConvexMesh(convexDesc, buf))
		{
			convexMesh = physics.createConvexMesh(PhysxUserMemoryReadStream(buf.getData(), buf.getSize()));
		}

		return convexMesh;
	}

//////////////////////////////////////////////////////////////////////////

	PxConvexMesh* CreateCylinderConvexMesh(const PxF32 width, const PxF32 radius, const PxU32 numCirclePoints)
	{
		#define  MAX_NUM_VERTS_IN_CIRCLE 16

		PxPhysics& physics = *g_pPhysicsWorld->PhysXSDK;
		PxCooking& cooking = *g_pPhysicsWorld->Cooking;

		r3d_assert(numCirclePoints<MAX_NUM_VERTS_IN_CIRCLE);
		PxVec3 verts[2*MAX_NUM_VERTS_IN_CIRCLE];
		PxU32 numVerts=2*numCirclePoints;
		const PxF32 dtheta=2*PxPi/(1.0f*numCirclePoints);
		for(PxU32 i=0;i<MAX_NUM_VERTS_IN_CIRCLE;i++)
		{
			const PxF32 theta=dtheta*i;
			const PxF32 cosTheta=radius*PxCos(theta);
			const PxF32 sinTheta=radius*PxSin(theta);
			verts[2*i+0]=PxVec3(-0.5f*width, cosTheta, sinTheta);
			verts[2*i+1]=PxVec3(+0.5f*width, cosTheta, sinTheta);
		}

		return CreateConvexMesh(verts,numVerts);
	}

//////////////////////////////////////////////////////////////////////////

	PxConvexMesh* CreateWheelConvexMesh(const PxVec3* verts, const PxU32 numVerts)
	{
		//Extract the wheel radius and width from the aabb of the wheel convex mesh.
		PxVec3 wheelMin(PX_MAX_F32,PX_MAX_F32,PX_MAX_F32);
		PxVec3 wheelMax(-PX_MAX_F32,-PX_MAX_F32,-PX_MAX_F32);
		for(PxU32 i=0;i<numVerts;i++)
		{
			wheelMin.x=PxMin(wheelMin.x,verts[i].x);
			wheelMin.y=PxMin(wheelMin.y,verts[i].y);
			wheelMin.z=PxMin(wheelMin.z,verts[i].z);
			wheelMax.x=PxMax(wheelMax.x,verts[i].x);
			wheelMax.y=PxMax(wheelMax.y,verts[i].y);
			wheelMax.z=PxMax(wheelMax.z,verts[i].z);
		}
		const PxF32 wheelWidth=wheelMax.x-wheelMin.x;
		const PxF32 wheelRadius=PxMax(wheelMax.y,wheelMax.z);

		return CreateCylinderConvexMesh(wheelWidth,wheelRadius,8);
	}

//////////////////////////////////////////////////////////////////////////

	void SetupShapesUserData(const VehicleDescriptor &vd)
	{
		PxRigidDynamic *a = vd.vehicle->getRigidDynamicActor();
		if (!a)
			return;

		PxU32 numShapes = a->getNbShapes();
		r3d_assert(numShapes <= vd.numWheels + vd.numHullParts);

		for (PxU32 i = 0; i < numShapes; ++i)
		{
			PxShape *s = 0;
			a->getShapes(&s, 1, i);
			if (!s)
				continue;

			if (i < vd.numWheels)
			{
				s->userData = reinterpret_cast<void*>(vd.wheelBonesRemapIndices[i]);
			}
			else
			{
				s->userData = reinterpret_cast<void*>(vd.hullBonesRemapIndices[i - vd.numWheels]);
			}
		}
	}

//////////////////////////////////////////////////////////////////////////

	void DampVec3(const PxVec3& oldPosition, PxVec3& newPosition, PxF32 timestep)
	{
		PxF32 t = 0.7f * timestep * 8.0f;
		t = PxMin(t, 1.0f);
		newPosition = oldPosition * (1 - t) + newPosition * t;
	}

//////////////////////////////////////////////////////////////////////////

	void SetupActor
	(
		PxRigidDynamic* vehActor,
		const PxFilterData& vehQryFilterData,
		const PxConvexMeshGeometry* wheelGeometries, const PxTransform* wheelLocalPoses, const PxU32 numWheelGeometries, const PxMaterial* wheelMaterial, const PxFilterData& wheelCollFilterData,
		const PxConvexMeshGeometry* chassisGeometries, const PxTransform* chassisLocalPoses, const PxU32 numChassisGeometries, const PxMaterial* chassisMaterial, const PxFilterData& chassisCollFilterData,
		const PxVehicleChassisData& chassisData
	)
	{
		//Add all the wheel shapes to the actor.
		for (PxU32 i = 0; i < numWheelGeometries; i++)
		{
			PxShape* wheelShape = vehActor->createShape(wheelGeometries[i], *wheelMaterial);
			wheelShape->setQueryFilterData(vehQryFilterData);
			wheelShape->setSimulationFilterData(wheelCollFilterData);
			wheelShape->setLocalPose(wheelLocalPoses[i]);
		}

		//Add the chassis shapes to the actor.
		for (PxU32 i = 0; i < numChassisGeometries; i++)
		{
			PxShape* chassisShape = vehActor->createShape(chassisGeometries[i], *chassisMaterial);
			chassisShape->setQueryFilterData(vehQryFilterData);
			chassisShape->setSimulationFilterData(chassisCollFilterData);
			chassisShape->setLocalPose(chassisLocalPoses[i]);
		}

		vehActor->setMass(chassisData.mMass);
		vehActor->setMassSpaceInertiaTensor(chassisData.mMOI);
		vehActor->setCMassLocalPose(PxTransform(chassisData.mCMOffset,PxQuat::createIdentity()));
	}

//////////////////////////////////////////////////////////////////////////

	bool CreateVehicleSimData(VehicleDescriptor &vd, const PxConvexMesh &chassisMesh, const PxConvexMesh **wheelMeshes, PxVehicleWheelsSimData &wheelsData, PxVehicleDriveSimData4W &driveData)
	{
		//Extract the chassis AABB dimensions from the chassis convex mesh.
		const PxVec3 chassisDims = ComputeChassisAABBDimensions(&chassisMesh);

		//The origin is at the center of the chassis mesh.
		//Set the center of mass to be below this point and a little towards the front.
		const PxVec3 chassisCMOffset = PxVec3(0.0f, -chassisDims.y * 0.5f + 0.65f, 0.25f);

		//Now compute the chassis mass and moment of inertia.
		//Use the moment of inertia of a cuboid as an approximate value for the chassis moi.
		PxVec3 chassisMOI = ComputeChassisMOI(chassisDims, vd.chassisData.mMass);

		//Let's set up the chassis data structure now.
		vd.chassisData.mMOI = chassisMOI;
		vd.chassisData.mCMOffset = chassisCMOffset;
		const float chassisMass = vd.chassisData.mMass;

		//Work out the front/rear mass split from the cm offset.
		//This is a very approximate calculation with lots of assumptions.
		//massRear*zRear + massFront*zFront = mass*cm		(1)
		//massRear       + massFront        = mass			(2)
		//Rearrange (2)
		//massFront = mass - massRear						(3)
		//Substitute (3) into (1)
		//massRear(zRear - zFront) + mass*zFront = mass*cm	(4)
		//Solve (4) for massRear
		//massRear = mass(cm - zFront)/(zRear-zFront)		(5)
		//Now we also have
		//zFront = (z-cm)/2									(6a)
		//zRear = (-z-cm)/2									(6b)
		//Substituting (6a-b) into (5) gives
		//massRear = 0.5*mass*(z-3cm)/z						(7)
		const PxF32 massRear = 0.5f * chassisMass * (chassisDims.z - 3 * chassisCMOffset.z) / chassisDims.z;
		const PxF32 massFront = chassisMass - massRear;

		//Extract the wheel radius and width from the wheel convex meshes.
		PxF32 wheelWidths[MAX_WHEELS_COUNT];
		PxF32 wheelRadii[MAX_WHEELS_COUNT];

		ComputeWheelWidthsAndRadii(wheelMeshes, wheelWidths, wheelRadii, vd.numWheels);

		//Now compute the wheel masses and inertias components around the axle's axis.
		//http://en.wikipedia.org/wiki/List_of_moments_of_inertia
		PxF32 wheelMOIs[MAX_WHEELS_COUNT];
		for(PxU32 i = 0; i < vd.numWheels; i++)
		{
			wheelMOIs[i] = 0.5f * vd.wheelsData[i].wheelData.mMass * wheelRadii[i] * wheelRadii[i];
		}
		//Let's set up the wheel data structures now with radius, mass, and moi.
		for(PxU32 i = 0; i < vd.numWheels; i++)
		{
			PxVehicleWheelData &wd = vd.wheelsData[i].wheelData;
			wd.mRadius = wheelRadii[i];
			wd.mMOI = wheelMOIs[i];
			wd.mWidth = wheelWidths[i];

			PxVehicleSuspensionData &sd = vd.wheelsData[i].suspensionData;
			sd.mSprungMass = chassisMass / vd.numWheels;
		}

		//We need to set up geometry data for the suspension, wheels, and tires.
		//We already know the wheel centers described as offsets from the rigid body centre of mass.
		//From here we can approximate application points for the tire and suspension forces.
		//Lets assume that the suspension travel directions are absolutely vertical.
		//Also assume that we apply the tire and suspension forces 30cm below the centre of mass.
		PxVec3 wheelCentreCMOffsets[MAX_WHEELS_COUNT];
		PxVec3 suspForceAppCMOffsets[MAX_WHEELS_COUNT];
		PxVec3 tireForceAppCMOffsets[MAX_WHEELS_COUNT];
		const PxVec3 *wheelCenterOffsets = vd.wheelCenterOffsets;
		for(PxU32 i = 0; i < vd.numWheels; i++)
		{
			wheelCentreCMOffsets[i] = wheelCenterOffsets[i] - chassisCMOffset;
			suspForceAppCMOffsets[i] = PxVec3(wheelCentreCMOffsets[i].x, -0.3f, wheelCentreCMOffsets[i].z);
			tireForceAppCMOffsets[i] = PxVec3(wheelCentreCMOffsets[i].x, -0.3f, wheelCentreCMOffsets[i].z);
		}

		//Now add the wheel, tire and suspension data.
		for(PxU32 i = 0; i < vd.numWheels; i++)
		{
			wheelsData.setWheelCentreOffset(i, wheelCentreCMOffsets[i]);
			wheelsData.setSuspForceAppPointOffset(i, suspForceAppCMOffsets[i]);
			wheelsData.setTireForceAppPointOffset(i, tireForceAppCMOffsets[i]);
		}

		//Ackermann steer accuracy
		PxVehicleAckermannGeometryData &ackermann = vd.ackermannData;
		ackermann.mAxleSeparation = std::abs(wheelCenterOffsets[PxVehicleDrive4W::eFRONT_LEFT_WHEEL].z - wheelCenterOffsets[PxVehicleDrive4W::eREAR_LEFT_WHEEL].z);
		ackermann.mFrontWidth = std::abs(wheelCenterOffsets[PxVehicleDrive4W::eFRONT_RIGHT_WHEEL].x - wheelCenterOffsets[PxVehicleDrive4W::eFRONT_LEFT_WHEEL].x);
		ackermann.mRearWidth = std::abs(wheelCenterOffsets[PxVehicleDrive4W::eREAR_RIGHT_WHEEL].x - wheelCenterOffsets[PxVehicleDrive4W::eREAR_LEFT_WHEEL].x);

		vd.ConfigureVehicleSimulationData(&driveData, &wheelsData);

		return true;
	}

//////////////////////////////////////////////////////////////////////////

	PxRigidDynamic * CreateVehicleActor(const VehicleDescriptor &vd, PxConvexMesh &hull, PxConvexMesh **wheels)
	{
		//We need a rigid body actor for the vehicle.
		//Don't forget to add the actor the scene after setting up the associated vehicle.
		PxRigidDynamic* vehActor = g_pPhysicsWorld->PhysXSDK->createRigidDynamic(PxTransform::createIdentity());

		//We need to add wheel collision shapes, a material for the wheels, and a simulation filter for the wheels.
		r3dTL::TFixedArray<PxConvexMeshGeometry, MAX_WHEELS_COUNT> wheelGeoms;
		r3dTL::TFixedArray<PxTransform, MAX_WHEELS_COUNT> wheelLocalPoses;
		for (uint32_t i = 0; i < vd.numWheels; ++i)
		{
			wheelGeoms[i] = PxConvexMeshGeometry(wheels[i]);
			wheelLocalPoses[i] = PxTransform::createIdentity();
		}
		PxMaterial& wheelMaterial = *g_pPhysicsWorld->defaultMaterial;
		PxFilterData wheelCollFilterData;
		wheelCollFilterData.word0 = PHYSCOLL_VEHICLE_WHEEL;

		//We need to add chassis collision shapes, their local poses, a material for the chassis, and a simulation filter for the chassis.
		PxConvexMeshGeometry chassisConvexGeom(&hull);

		//We need to specify the local poses of the chassis composite shapes.
		PxTransform chassisLocalPoses[1] = {PxTransform::createIdentity()};

		PxMaterial& chassisMaterial = *g_pPhysicsWorld->defaultMaterial;
		PxFilterData chassisCollFilterData;
		chassisCollFilterData.word0 = PHYSCOLL_STATIC_GEOMETRY;

		//Create a query filter data for the car to ensure that cars
		//do not attempt to drive on themselves.
		PxFilterData vehQryFilterData;
		VehicleSetupNonDrivableShapeQueryFilterData(vehQryFilterData);


		SetupActor
		(
			vehActor,
			vehQryFilterData,
			&wheelGeoms[0], &wheelLocalPoses[0], vd.numWheels, &wheelMaterial, wheelCollFilterData,
			&chassisConvexGeom, &chassisLocalPoses[0], vd.numHullParts, &chassisMaterial, chassisCollFilterData,
			vd.chassisData
		);
		return vehActor;
	}

} // unnamed namespace

PxF32 &gVehicleTireGroundFriction = gTyreFrictionMultipliers[0][0];

//////////////////////////////////////////////////////////////////////////

VehicleManager::VehicleManager()
: batchSuspensionRaycasts(0)
, drivableCar(0)
, clearInputData(true)
, mAtRestUnderBraking(true)
, mTimeElapsedSinceAtRestUnderBraking(0.0f)
, mInReverseMode(false)
, surfaceTypePairs(0)
{
	r3d_assert(g_pPhysicsWorld);
	r3d_assert(g_pPhysicsWorld->PhysXSDK);
	r3d_assert(g_pPhysicsWorld->PhysXScene);

	d_drive_vehicles->SetChangeCallback(&DriveVehiclesChangeCallback);

	PxInitVehicleSDK(*g_pPhysicsWorld->PhysXSDK);

    surfaceTypePairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(MAX_NUM_TYRE_TYPES,MAX_NUM_SURFACE_TYPES);
    const PxMaterial *mats[] = {g_pPhysicsWorld->defaultMaterial};
    surfaceTypePairs->setup(MAX_NUM_TYRE_TYPES, MAX_NUM_SURFACE_TYPES, mats, &drivableSurfaceType);

	for (PxU32 i = 0; i < MAX_NUM_SURFACE_TYPES; i++)
	{
		for (PxU32 j = 0; j < MAX_NUM_TYRE_TYPES; j++)
		{
			surfaceTypePairs->setTypePairFriction(i, j, gTyreFrictionMultipliers[i][j]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

VehicleManager::~VehicleManager()
{
	ClearSuspensionRaycatsQuery();

	while (vehicles.Count() > 0)
	{
		DeleteCar(vehicles.GetFirst());
	}

	if (surfaceTypePairs)
	{
		surfaceTypePairs->release();
	}
	PxCloseVehicleSDK();
	stopcar=true;
}

//////////////////////////////////////////////////////////////////////////

void VehicleManager::Update(float timeStep)
{
	if (vehicles.Count() == 0) // || !d_drive_vehicles->GetBool())
		return;

	if (!batchSuspensionRaycasts)
	{
		batchSuspensionRaycasts = SetUpBatchedSceneQuery();
	}

	physxVehs.Resize(vehicles.Count());
	for (uint32_t i = 0, i_end = physxVehs.Count(); i < i_end; ++i)
	{
			physxVehs[i] = vehicles[i]->vehicle;
	}


		if (gClientLogic().localPlayer_)
		{
			obj_Player* plr = gClientLogic().localPlayer_;
			if (plr->ActualVehicle != NULL && !plr->isPassenger())
			{
				if (plr->ActualVehicle->GetNetworkID() == this->getRealDrivenVehicle()->GetNetworkID())
				{
					DoUserCarControl(timeStep,false, carControlData,0);
				}
			}
		}

	for (uint32_t i = 0, i_end = physxVehs.Count(); i < i_end; ++i)
	{
		if (vehicles[i]->owner)
		{
			if (vehicles[i]->owner->Occupants>0)
			{
				obj_Player* plr = gClientLogic().localPlayer_;
			  if (gClientLogic().localPlayer_)
			  {
				  if (plr->ActualVehicle == vehicles[i]->owner && plr->isInVehicle())
				{
					PxVehicleSuspensionRaycasts(batchSuspensionRaycasts, vehicles.Count(), &physxVehs.GetFirst(), batchQueryResults.Count(), &batchQueryResults.GetFirst());
					PxVec3 gravity = g_pPhysicsWorld->PhysXScene->getGravity();
					PxVehicleUpdates(timeStep, gravity, *surfaceTypePairs, 1, &physxVehs[i]);
				}
				else {
					PxVehicleSuspensionRaycasts(batchSuspensionRaycasts, 1, &physxVehs[i], 0, 0);
					PxVec3 gravity = g_pPhysicsWorld->PhysXScene->getGravity();
					PxVehicleUpdates(timeStep, gravity, *surfaceTypePairs, 1, &physxVehs[i]);
				}
			  }
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void VehicleManager::DoUserCarControl(float timeStep,bool enable, PxVehicleDrive4WRawInputData OthercontrolData,int othercar)
{
 if (enable==true)
 {
	    GameObject* from = GameWorld().GetNetworkObject(othercar);
		if (from)
		{
			obj_Vehicle* VehicleID= static_cast< obj_Vehicle* > ( from );
			PxVehicleDrive4W &VehiD = *VehicleID->vd->vehicle;
			//r3dOutToLog("######## timeStep %f\n",timeStep);
			if (OthercontrolData.getDigitalAccel())
			{
				VehiD.mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
			}
			else if (OthercontrolData.getDigitalBrake())
			{
				VehiD.mDriveDynData.forceGearChange(PxVehicleGearsData::eREVERSE);
			}
			
			PxVehicleDrive4WSmoothDigitalRawInputsAndSetAnalogInputs(gKeySmoothingData,  gSteerVsForwardSpeedTable, OthercontrolData, timeStep, VehiD);
			return;
		}
 }

if (!drivableCar)
  return;
if (!gClientLogic().localPlayer_)
	return;	
obj_Player* plr = gClientLogic().localPlayer_;

if (plr->ActualVehicle == NULL)
	 return;
 timeStepGet=timeStep;
 PxVehicleDrive4W &car = *drivableCar->vehicle;

 //Work out if the car is to flip from reverse to forward gear or from forward gear to reverse.
 bool toggleAutoReverse = true;

 if (plr->ActualVehicle->GasolineCar<=0.9 || hudPause->isActive() || hudMain->isPlayersListVisible() || hudMain->isChatInputActive()) // Server Vehicles
 {
        carControlData.setDigitalAccel(false);
        carControlData.setDigitalBrake(false);
		if (hudPause->isActive() || hudMain->isPlayersListVisible() || hudMain->isChatInputActive())
		{
			carControlData.setDigitalSteerLeft(false);
			carControlData.setDigitalSteerRight(false);
		}
        PxVehicleDrive4WSmoothDigitalRawInputsAndSetAnalogInputs(gKeySmoothingData,  gSteerVsForwardSpeedTable, carControlData, timeStep, car);
        clearInputData = true;
	    return;
 }
 if (plr->ActualVehicle->DamageCar<=0.99) // Server Vehicles
 {
        carControlData.setDigitalAccel(false);
        carControlData.setDigitalBrake(false);
		carControlData.setDigitalSteerLeft(false);
		carControlData.setDigitalSteerRight(false);
        PxVehicleDrive4WSmoothDigitalRawInputsAndSetAnalogInputs(gKeySmoothingData,  gSteerVsForwardSpeedTable, carControlData, timeStep, car);
        clearInputData = true;
	    return;
 }
 /*if (car.mDriveDynData.getUseAutoGears())
 {
  toggleAutoReverse = ProcessAutoReverse(timeStep);
 }

 //If the car is to flip gear direction then switch gear as appropriate.
 if(toggleAutoReverse)
 {
        mInReverseMode = !mInReverseMode;
 }*/

  if (Keyboard->IsPressed(kbsS)) // Reverse - AomBE Edit
  {
	  if ((plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) <=0)
	  {
        car.mDriveDynData.forceGearChange(PxVehicleGearsData::eREVERSE);
        carControlData.setDigitalAccel(Keyboard->IsPressed(kbsS));
        carControlData.setDigitalBrake(Keyboard->IsPressed(kbsW));
	  }
	  else {
        carControlData.setDigitalAccel(false);
        carControlData.setDigitalBrake(Keyboard->IsPressed(kbsS));
		if (!SoundSys.isPlaying(m_sndVehicleDrive))
		{
			if ((plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) > 15 || (plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) < -0.15 )
			{
				if (stopcar == true)
				{
				  if (!plr->ActualVehicle->vd->vehicle->isInAir())
				  {
						m_sndVehicleDrive = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/Braking/Brake_Concrete"), plr->ActualVehicle->GetPosition(),true);
						SoundSys.Start(m_sndVehicleDrive);
				  }
				 stopcar=false;
				}
			}
		}
	  }
  }
  else if (Keyboard->IsPressed(kbsW)) // Forward Gear
  {
	  if ((plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) >=0)
	  {
        car.mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
        carControlData.setDigitalAccel(Keyboard->IsPressed(kbsW));
        carControlData.setDigitalBrake(Keyboard->IsPressed(kbsS));
	  }
	  else {
        carControlData.setDigitalAccel(false);
        carControlData.setDigitalBrake(Keyboard->IsPressed(kbsW));
		if (!SoundSys.isPlaying(m_sndVehicleDrive))
		{
			if ((plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) > 15 || (plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) < -0.15 )
			{
				if (stopcar == true)
				{
				  if (!plr->ActualVehicle->vd->vehicle->isInAir())
				  {
						m_sndVehicleDrive = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/Braking/Brake_Concrete"), plr->ActualVehicle->GetPosition(),true);
						SoundSys.Start(m_sndVehicleDrive);
				  }
				 stopcar=false;
				}
			}
		}
	  }
  }
 else if (Keyboard->IsPressed(kbsSpace))
 {
        carControlData.setDigitalAccel(false);
        carControlData.setDigitalBrake(Keyboard->IsPressed(kbsSpace));
		if (!SoundSys.isPlaying(m_sndVehicleDrive))
		{
			if ((plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) > 15 || (plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) < -0.15 )
			{
				if (stopcar == true)
				{
				  if (!plr->ActualVehicle->vd->vehicle->isInAir())
				  {
						m_sndVehicleDrive = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/Vehicles/Braking/Brake_Concrete"), plr->ActualVehicle->GetPosition(),true);
						SoundSys.Start(m_sndVehicleDrive);
				  }
				 stopcar=false;
				}
			}
		}
 }
   else {

  stopcar=true;
  }
if ((plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2)> -0.5 && (plr->ActualVehicle->vd->vehicle->computeForwardSpeed()*2) < 5)
	 stopcar=true;

    PxVehicleDrive4WSmoothDigitalRawInputsAndSetAnalogInputs(gKeySmoothingData,  gSteerVsForwardSpeedTable, carControlData, timeStep, car);
    clearInputData = true;
}

//////////////////////////////////////////////////////////////////////////

#define THRESHOLD_FORWARD_SPEED (5e-2f)
#define THRESHOLD_SIDEWAYS_SPEED (1e-2f)

bool VehicleManager::ProcessAutoReverse(float timestep)
{
	if (!drivableCar || !drivableCar->vehicle)
		return false;

	PxVehicleDrive4W &car = *drivableCar->vehicle;

	bool accelRaw, brakeRaw;
	accelRaw = carControlData.getDigitalAccel();
	brakeRaw = carControlData.getDigitalBrake();

	const bool accel = mInReverseMode ? brakeRaw : accelRaw;
	const bool brake = mInReverseMode ? accelRaw : brakeRaw;

	//If the car has been brought to rest by pressing the brake then raise a flag.
	bool justRaisedFlag = false;
	if(brake && !mAtRestUnderBraking)
	{
		bool isInAir = car.isInAir();
		PxF32 forwardSpeed = PxAbs(car.computeForwardSpeed());
		PxF32 sidewaysSpeed = PxAbs(car.computeSidewaysSpeed());

		if(!isInAir && forwardSpeed < THRESHOLD_FORWARD_SPEED && sidewaysSpeed < THRESHOLD_SIDEWAYS_SPEED)
		{
			//justRaisedFlag = true;
			mAtRestUnderBraking = true;
			mTimeElapsedSinceAtRestUnderBraking = 0.0f;
		}
	}

	//If the flag is raised and the player pressed accelerate then lower the flag.
	if(mAtRestUnderBraking && accel)
	{
		mAtRestUnderBraking = false;
		mTimeElapsedSinceAtRestUnderBraking = 0.0f;
	}

	//If the flag is raised and the player doesn't press brake then increment the timer.
	if(mAtRestUnderBraking && !justRaisedFlag)
	{
		mTimeElapsedSinceAtRestUnderBraking += timestep;
	}

	//If the flag is raised and the player pressed brake again then switch auto-reverse.
	if(mAtRestUnderBraking && !justRaisedFlag && mTimeElapsedSinceAtRestUnderBraking > 0.0f)
	{
		mAtRestUnderBraking = false;
		mTimeElapsedSinceAtRestUnderBraking = 0.0f;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void VehicleManager::UpdateInput()
{
	if (clearInputData)
	{
		carControlData.setDigitalAccel(false);
		carControlData.setDigitalBrake(false);
		carControlData.setDigitalSteerLeft(false);
		carControlData.setDigitalSteerRight(false);
		clearInputData = false;
	}
	carControlData.setDigitalAccel(carControlData.getDigitalAccel() || Keyboard->IsPressed(kbsW));
	carControlData.setDigitalBrake(carControlData.getDigitalBrake() || Keyboard->IsPressed(kbsS));
	//	Left and right are switched intentionally
	carControlData.setDigitalSteerLeft(carControlData.getDigitalSteerLeft() || Keyboard->IsPressed(kbsD));
	carControlData.setDigitalSteerRight(carControlData.getDigitalSteerRight() || Keyboard->IsPressed(kbsA));
}

//////////////////////////////////////////////////////////////////////////

VehicleDescriptor * VehicleManager::CreateVehicle(const r3dMesh *m)
{
	if (!m)
		return 0;

	typedef r3dTL::TArray<PxVec3> Vertices;
	typedef r3dTL::TArray<Vertices> Meshes;

	std::auto_ptr<VehicleDescriptor> vd(new VehicleDescriptor);
	if (!vd->Init(m))
	{
		return 0;
	}

	Meshes wheelMeshes;
	wheelMeshes.Resize(vd->numWheels);
	Meshes hullMeshes;
	hullMeshes.Resize(vd->numHullParts);

	D3DXQUATERNION test;
	D3DXQuaternionRotationYawPitchRoll(&test, D3DX_PI / 2, 0, 0); // rotY
	PxQuat rotQ(test.x, test.y, test.z, test.w);

	for (int i = 0; i < m->NumVertices; ++i)
	{
		const r3dMesh::r3dWeight &w = m->pWeights[i];
		BYTE boneId = w.BoneID[0];
		const r3dBone &bone = vd->skl->Bones[boneId];
		Meshes *meshArr = 0;
		uint32_t meshIdx = INVALID_INDEX;
		uint32_t wheelIdx = vd->GetWheelIndex(boneId);
		uint32_t hullIdx = vd->GetHullIndex(boneId);

		if (wheelIdx != INCORRECT_INDEX)
		{
			meshArr = &wheelMeshes;
			meshIdx = wheelIdx;
		}
		else if (hullIdx != INCORRECT_INDEX)
		{
			meshArr = &hullMeshes;
			meshIdx = hullIdx;
		}
		else
		{
			continue;
		}

		Vertices &v = (*meshArr)[meshIdx];
		r3dPoint3D tmp = m->VertexPositions[i];
		PxVec3 bonePos(bone.vRelPlacement.x, bone.vRelPlacement.y, bone.vRelPlacement.z);
		PxVec3 pt(tmp.x, tmp.y, tmp.z);
		pt -= bonePos;
		pt = rotQ.rotate(pt);
		v.PushBack(pt);
	}

	// Hull mesh
	PxConvexMesh *hull = CreateConvexMesh(&hullMeshes[0][0], hullMeshes[0].Count());

	// Wheel meshes

	PxConvexMesh **wheels = reinterpret_cast<PxConvexMesh**>(_alloca(sizeof(PxConvexMesh*) * wheelMeshes.Count()));
	for (uint32_t i = 0; i < wheelMeshes.Count(); ++i)
	{
		Vertices &v = wheelMeshes[i];
		wheels[i] = CreateWheelConvexMesh(&v[0], v.Count());
	}

	PxVehicleWheelsSimData *wheelsData = PxVehicleWheelsSimData::allocate(vd->numWheels);
	PxVehicleDriveSimData4W driveData;

	bool result = CreateVehicleSimData(*vd, *hull, const_cast<const PxConvexMesh**>(wheels), *wheelsData, driveData); result;
	r3d_assert(result);

	PxRigidDynamic *vehActor = CreateVehicleActor(*vd, *hull, wheels);
	r3d_assert(vehActor);

	vd->vehicle = PxVehicleDrive4W::allocate(vd->numWheels);

	if ((int)vd->numWheels>4)
		vd->vehicle->setup(g_pPhysicsWorld->PhysXSDK, vehActor, *wheelsData, driveData, std::max<int>(vd->numWheels, 0));
	else
		vd->vehicle->setup(g_pPhysicsWorld->PhysXSDK, vehActor, *wheelsData, driveData, std::max<int>(vd->numWheels-4, 0));

	for(int i=0;i<(int)vd->numWheels;i++)
	{
		vd->vehicle->setWheelShapeMapping(i, i);
	}

	//Don't forget to add the actor to the scene.
	g_pPhysicsWorld->PhysXScene->addActor(*vehActor);

	vd->vehicle->setToRestState();
	vd->vehicle->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
	ConfigureSuspensionRaycasts(*vd);

	SetupShapesUserData(*vd);

	VehicleDescriptor *rv = vd.release();
	vehicles.PushBack(rv);

	wheelsData->free();

	return rv;
}

//////////////////////////////////////////////////////////////////////////

void VehicleManager::ConfigureSuspensionRaycasts(const VehicleDescriptor &car)
{
	//	Increase suspension raycasts buffers
	batchHits.Resize(batchHits.Count() + car.numWheels);
	batchQueryResults.Resize(batchQueryResults.Count() + car.numWheels);

	//	Clear current batch query, it will be recreated in next update with new data
	ClearSuspensionRaycatsQuery();
}

//////////////////////////////////////////////////////////////////////////

PxBatchQuery * VehicleManager::SetUpBatchedSceneQuery()
{
	PxBatchQueryDesc bqd;
	bqd.userRaycastHitBuffer = &batchHits.GetFirst();
	bqd.userRaycastResultBuffer = &batchQueryResults.GetFirst();
	bqd.raycastHitBufferSize = batchHits.Count();
	bqd.preFilterShader = &VehicleWheelRaycastPreFilter;
	return g_pPhysicsWorld->PhysXScene->createBatchQuery(bqd);
}

//////////////////////////////////////////////////////////////////////////

void VehicleManager::DeleteCar(VehicleDescriptor *car)
{
	if (!car)
		return;

	for (uint32_t i = 0; i < vehicles.Count(); ++i)
	{
		if (car == vehicles[i])
		{
			vehicles.Erase(i);
			PxU32 numWheels = car->numWheels;
			r3d_assert(batchHits.Count() >= numWheels);
			r3d_assert(batchQueryResults.Count() >= numWheels);
			r3d_assert(batchQueryResults.Count() == batchHits.Count());
			batchHits.Resize(batchHits.Count() - numWheels);
			batchQueryResults.Resize(batchQueryResults.Count() - numWheels);
			ClearSuspensionRaycatsQuery();
			PxRigidDynamic *a = car->vehicle->getRigidDynamicActor();
			g_pPhysicsWorld->PhysXScene->removeActor(*a);
			a->release();

			//	Clear drivable car deleted car, clear pointer either
			if (drivableCar == car)
				drivableCar = 0;

			delete car;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void VehicleManager::ClearSuspensionRaycatsQuery()
{
	if (batchSuspensionRaycasts)
	{
		batchSuspensionRaycasts->release();
		batchSuspensionRaycasts = 0;
	}
}

//////////////////////////////////////////////////////////////////////////

void VehicleManager::DriveCar(VehicleDescriptor *car)
{
	drivableCar = car;
	/*if (car)
		cameraContoller.SetDrivenVehicle(car->vehicle->getRigidDynamicActor());
	else
		cameraContoller.SetDrivenVehicle(0);*/
}

//////////////////////////////////////////////////////////////////////////

void VehicleManager::UpdateVehiclePoses()
{
	for (uint32_t i = 0; i < vehicles.Count(); ++i)
	{
		VehicleDescriptor *vd = vehicles[i];
		if (vd && vd->owner)
		{
			vd->owner->UpdatePositionFromPhysx();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

obj_Vehicle* VehicleManager::getRealDrivenVehicle()
{
	ObjectManager& GW = GameWorld();
	for (GameObject *obj = GW.GetFirstObject(); obj; obj = GW.GetNextObject(obj))
	{
		if(obj->isObjType(OBJTYPE_Vehicle))
		{
			obj_Vehicle * vh = static_cast<obj_Vehicle*>(obj);
			if ( vh->getVehicleDescriptor() == drivableCar ){
				return vh;
			}

		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

bool VehicleManager::ConfigureCamera(r3dCamera &cam)
{
	return cameraContoller.ConfigureCamera(cam);
}

//-------------------------------------------------------------------------
//	Vehicle camera controller
//-------------------------------------------------------------------------

VehicleCameraController::VehicleCameraController()
: mCameraPos(0, 0, 0)
, mCameraTargetPos(0, 0, 0)
, mLastCarPos(0, 0, 0)
, mLastCarVelocity(0, 0, 0)
, mCameraInit(false)
, mLastFocusVehTransform(PxTransform::createIdentity())
, mCameraRotateAngleY(0)
, mCameraRotateAngleZ(0.38f)
, mCounter(0)
{
	ResetHistory(mAccelXHistory, 0);
	ResetHistory(mAccelZHistory, 0);
}

//////////////////////////////////////////////////////////////////////////

bool VehicleCameraController::ConfigureCamera(r3dCamera &Cam)
{
	if (!actor)
		return false;

	Cam.SetPosition(r3dVector(mCameraPos.x, mCameraPos.y, mCameraPos.z));
	Cam.PointTo(r3dVector(mCameraTargetPos.x, mCameraTargetPos.y, mCameraTargetPos.z));
	Cam.vUP = r3dPoint3D(0, 1, 0);
	return true;
}

//////////////////////////////////////////////////////////////////////////

void VehicleCameraController::SetDrivenVehicle(PxRigidDynamic *a)
{
	actor = a;
	mCameraInit = false;
	mLastCarPos = a->getGlobalPose().p;
}

//////////////////////////////////////////////////////////////////////////


void VehicleCameraController::Update(float dtime)
{
	if (!actor)
		return;

	PxTransform carChassisTransfm(actor->getGlobalPose());

	PxF32 camDist = 10.0f;
	PxF32 cameraYRotExtra = 0.0f;

	PxVec3 velocity = mLastCarPos - carChassisTransfm.p;

	int idx = mCounter % FloatHistory::COUNT;
	mCounter++;
	if (mCameraInit)
	{
		//Work out the forward and sideways directions.
		PxVec3 unitZ(0,0,1);
		PxVec3 carDirection = carChassisTransfm.q.rotate(unitZ);
		PxVec3 unitX(1,0,0);
		PxVec3 carSideDirection = carChassisTransfm.q.rotate(unitX);

		//Acceleration (note that is not scaled by time).
		PxVec3 acclVec = mLastCarVelocity - velocity;

		//Higher forward accelerations allow the car to speed away from the camera.
		PxF32 acclZ = carDirection.dot(acclVec);
		mAccelZHistory[idx] = acclZ;
		acclZ = HistoryAverage(mAccelZHistory);
		camDist = PxMax(camDist+acclZ*400.0f, 5.0f);

		//Higher sideways accelerations allow the car's rotation to speed away from the camera's rotation.
		PxF32 acclX = carSideDirection.dot(acclVec);
		mAccelXHistory[idx] = acclX;
		acclX = HistoryAverage(mAccelXHistory);
		cameraYRotExtra = -acclX * 20.0f;

		//At very small sideways speeds the camera greatly amplifies any numeric error in the body and leads to a slight jitter.
		//Scale cameraYRotExtra by a value in range (0,1) for side speeds in range (0.1,1.0) and by zero for side speeds less than 0.1.
		PxFixedSizeLookupTable<4> table;
		table.addPair(0.0f, 0.0f);
		table.addPair(0.1f * dtime, 0);
		table.addPair(1.0f * dtime, 1);
		PxF32 velX = carSideDirection.dot(velocity);
		cameraYRotExtra *= table.getYVal(PxAbs(velX));
	}

	mCameraRotateAngleY=physx::intrinsics::fsel(mCameraRotateAngleY-10*PxPi, mCameraRotateAngleY-10*PxPi, physx::intrinsics::fsel(-mCameraRotateAngleY-10*PxPi, mCameraRotateAngleY + 10*PxPi, mCameraRotateAngleY));
	mCameraRotateAngleZ=PxClamp(mCameraRotateAngleZ,-PxPi*0.05f,PxPi*0.45f);

	PxVec3 cameraDir=PxVec3(0,0,1)*PxCos(mCameraRotateAngleY+cameraYRotExtra) + PxVec3(1,0,0) * PxSin(cameraYRotExtra);

	cameraDir=cameraDir*PxCos(mCameraRotateAngleZ)-PxVec3(0,1,0)*PxSin(mCameraRotateAngleZ);

	const PxVec3 direction = carChassisTransfm.q.rotate(cameraDir);
	PxVec3 target = carChassisTransfm.p;
	target.y += 0.5f;

	camDist = PxMax(5.0f, PxMin(camDist, 50.0f));

	PxVec3 position = target-direction*camDist;

	if (mCameraInit)
	{
		DampVec3(mCameraPos, position, dtime);
		DampVec3(mCameraTargetPos, target, dtime);
	}

	mCameraPos = position;
	mCameraTargetPos = target;
	mCameraInit = true;

	mLastCarVelocity = velocity;
	mLastCarPos = carChassisTransfm.p;
}

//////////////////////////////////////////////////////////////////////////

float VehicleCameraController::HistoryAverage(const FloatHistory &fh) const
{
	float rv = 0;
	for (int i = 0; i < FloatHistory::COUNT; ++i)
	{
		rv += fh[i];
	}
	rv /= FloatHistory::COUNT;
	return rv;
}

//////////////////////////////////////////////////////////////////////////

void VehicleCameraController::ResetHistory(FloatHistory &fh, float v)
{
	for (size_t i = 0; i < FloatHistory::COUNT; ++i)
	{
		fh[i] = v;
	}
}

//-------------------------------------------------------------------------
//	Standalone helper functions
//-------------------------------------------------------------------------

void VehicleSetupDrivableShapeQueryFilterData(PxFilterData &qryFilterData)
{
	r3d_assert(qryFilterData.word3 == 0 || qryFilterData.word3 == VEHICLE_DRIVABLE_SURFACE || qryFilterData.word3 == VEHICLE_NONDRIVABLE_SURFACE);
	qryFilterData.word3 = VEHICLE_DRIVABLE_SURFACE;
}

//////////////////////////////////////////////////////////////////////////

void VehicleSetupNonDrivableShapeQueryFilterData(PxFilterData &qryFilterData)
{
	r3d_assert(qryFilterData.word3 == 0 || qryFilterData.word3 == VEHICLE_NONDRIVABLE_SURFACE || qryFilterData.word3 == VEHICLE_NONDRIVABLE_SURFACE);
	qryFilterData.word3 = VEHICLE_NONDRIVABLE_SURFACE;
}

#endif // VEHICLES_ENABLED