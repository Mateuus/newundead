#ifdef VOIP_ENABLE
//=========================================================================
//	Module: VehicleDescriptor.cpp
//	Copyright (C) 2012.
//=========================================================================

#pragma once
#include "r3dPCH.h"
#include "r3d.h"

#if VEHICLES_ENABLED
#include "VehicleDescriptor.h"
#include "XMLHelpers.h"
#include "PhysXWorld.h"

struct BoneSearcher
{
	const char *toFind;
	explicit BoneSearcher(const char *nameToFind): toFind(nameToFind) {}
	bool operator() (const r3dBone &b)
	{
		return strcmpi(toFind, b.Name) == 0;
	}
};


//////////////////////////////////////////////////////////////////////////

VehicleDescriptor::VehicleDescriptor()
: numWheels(0)
, numHullParts(0)
, vehicle(0)
, skl(0)
, owner(0)
, wheelsData(0)
{
	for (uint32_t i = 0; i < _countof(wheelBonesRemapIndices); ++i)
	{
		wheelBonesRemapIndices[i] = INVALID_INDEX;
	}
	for (uint32_t i = 0; i < _countof(hullBonesRemapIndices); ++i)
	{
		hullBonesRemapIndices[i] = INVALID_INDEX;
	}
	::ZeroMemory(wheelCenterOffsets, sizeof(wheelCenterOffsets));
	::ZeroMemory(hullCenterOffsets, sizeof(hullCenterOffsets));
}

//////////////////////////////////////////////////////////////////////////

VehicleDescriptor::~VehicleDescriptor()
{
	if (vehicle)
		vehicle->free();
	delete skl;
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::Init(const r3dMesh *m)
{
	if (!Load(m))
	{
		r3dArtBug("Mesh does not have associated vehicle drive information");
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::InitSkeleton(const r3dSkeleton *s)
{
	for (uint32_t i = 0; i < MAX_WHEELS_COUNT; ++i)
	{
		wheelBonesRemapIndices[i] = INCORRECT_INDEX;
		wheelCenterOffsets[i] = PxVec3(0, 0, 0);
	}

	for (uint32_t i = 0; i < MAX_HULL_PARTS_COUNT; ++i)
	{
		hullBonesRemapIndices[i] = INCORRECT_INDEX;
		hullCenterOffsets[i] = PxVec3(0, 0, 0);
	}

	//	4 wheels + 1 hull part at least
	if (!s || s->NumBones < 5)
		return false;


	const r3dBone *bonesStart = s->Bones;
	const r3dBone *bonesEnd = s->Bones + s->NumBones;

	numWheels = 0;
	numHullParts = 0;

	for (uint32_t i = 0; i < MAX_WHEELS_COUNT; ++i)
	{
		const r3dBone *partName = std::find_if(bonesStart, bonesEnd, BoneSearcher(VEHICLE_PART_NAMES[i + MAX_HULL_PARTS_COUNT]));
		if (partName != bonesEnd)
		{
			size_t idx = std::distance(bonesStart, partName);
			wheelBonesRemapIndices[i] = idx;
			++numWheels;
		}
	}

	for (uint32_t i = 0; i < MAX_HULL_PARTS_COUNT; ++i)
	{
		const r3dBone *partName = std::find_if(bonesStart, bonesEnd, BoneSearcher(VEHICLE_PART_NAMES[i]));
		if (partName != bonesEnd)
		{
			size_t idx = std::distance(bonesStart, partName);
			hullBonesRemapIndices[i] = idx;
			++numHullParts;
		}
	}

	//	Calculate center offsets
	r3dVector tmp = s->Bones[hullBonesRemapIndices[0]].vRelPlacement;
	PxVec3 centerPos(tmp.x, tmp.y, tmp.z);
	for (uint32_t i = 0; i < MAX_WHEELS_COUNT; ++i)
	{
		uint32_t boneIdx = wheelBonesRemapIndices[i];
		if (boneIdx == INCORRECT_INDEX)
			continue;

		r3dVector tmp = s->Bones[boneIdx].vRelPlacement;
		PxVec3 bonePos(tmp.x, tmp.y, tmp.z);
		wheelCenterOffsets[i] = bonePos;
		std::swap(wheelCenterOffsets[i].x, wheelCenterOffsets[i].z);
		wheelCenterOffsets[i].z *= -1;
	}
 	std::swap(wheelCenterOffsets[0], wheelCenterOffsets[2]);
 	std::swap(wheelCenterOffsets[1], wheelCenterOffsets[3]);

	for (uint32_t i = 0; i < MAX_HULL_PARTS_COUNT; ++i)
	{
		uint32_t boneIdx = hullBonesRemapIndices[i];
		if (boneIdx == INCORRECT_INDEX)
			continue;

		r3dVector tmp = s->Bones[boneIdx].vRelPlacement;
		PxVec3 bonePos(tmp.x, tmp.y, tmp.z);
		hullCenterOffsets[i] = bonePos;
		std::swap(hullCenterOffsets[i].x, hullCenterOffsets[i].z);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

uint32_t VehicleDescriptor::GetIndex(const uint32_t *arr, uint32_t arrCount, uint32_t boneIndex) const
{
	for (uint32_t i = 0; i < arrCount; ++i)
	{
		if (arr[i] == boneIndex)
			return i;
	}
	return INVALID_INDEX;
}

//////////////////////////////////////////////////////////////////////////

uint32_t VehicleDescriptor::GetWheelIndex(uint32_t boneId) const
{
	return GetIndex(wheelBonesRemapIndices, MAX_WHEELS_COUNT, boneId);
}

//////////////////////////////////////////////////////////////////////////

uint32_t VehicleDescriptor::GetHullIndex(uint32_t boneId) const
{
	return GetIndex(hullBonesRemapIndices, MAX_HULL_PARTS_COUNT, boneId);
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::Load(const r3dMesh *m)
{
	std::string skeletonPath = m->FileName.c_str();
	if (skeletonPath.size() < 3)
		return false;

	skeletonPath.replace(skeletonPath.size() - 3, 3, "skl");

	driveFileDefinitionPath = m->FileName.c_str();
	driveFileDefinitionPath.erase(driveFileDefinitionPath.size() - 4);
	driveFileDefinitionPath += "_DriveData.xml";

	r3dFile* f = r3d_open(driveFileDefinitionPath.c_str(), "rb");
	if ( !f )
	{
		r3dOutToLog("Failed to open vehicle definition file: %s\n", driveFileDefinitionPath.c_str());
		return false;
	}

	char* fileBuffer = new char[f->size + 1];
	fread(fileBuffer, f->size, 1, f);
	fileBuffer[f->size] = 0;

	pugi::xml_document xmlDoc;
	pugi::xml_parse_result parseResult = xmlDoc.load_buffer_inplace(fileBuffer, f->size);
	fclose(f);
	if (!parseResult)
		r3dError("Failed to parse XML, error: %s", parseResult.description());

	pugi::xml_node xml = xmlDoc.child("vehicle");

	skl = new r3dSkeleton;
	skl->LoadBinary(skeletonPath.c_str());

	if (!InitSkeleton(skl))
		return false;

	wheelsData.Resize(numWheels);
	InitToDefault();

	//	Ackerman data
	GetXMLVal("ackermann_accuracy", xml, &ackermannData.mAccuracy);

	//	Gears data
	GetXMLVal("gears_switch_time", xml, &gearsData.mSwitchTime);

	// chassis data
	pugi::xml_node chassisNode = xml.child("chassis");
	GetXMLVal("mass", chassisNode, &chassisData.mMass);
	r3dVector v1(chassisData.mMOI.x, chassisData.mMOI.y, chassisData.mMOI.z);
	GetXMLVal("moi_offset", chassisNode, &v1);
	chassisData.mMOI = PxVec3(v1.x, v1.y, v1.z);
	r3dVector v2(chassisData.mCMOffset.x, chassisData.mCMOffset.y, chassisData.mCMOffset.z);
	GetXMLVal("cm_offset", chassisNode, &v2);
	chassisData.mCMOffset = PxVec3(v2.x, v2.y, v2.z);;

	//	Engine data
	pugi::xml_node engineNode = xml.child("engine");
	GetXMLVal("peak_torque", engineNode, &engineData.mPeakTorque);
	GetXMLVal("max_omega", engineNode, &engineData.mMaxOmega);
// 	GetXMLVal("damping_rate_full_throttle", engineNode, &engineData.mDampingRateFullThrottle);
// 	GetXMLVal("damping_rate_zero_throttle_clutch_disengaged", engineNode, &engineData.mDampingRateZeroThrottleClutchDisengaged);
// 	GetXMLVal("damping_rate_zero_throttle_clutch_engaged", engineNode, &engineData.mDampingRateZeroThrottleClutchEngaged);

	// Differential data
	pugi::xml_node diffNode = xml.child("differential");
	int type = diffData.mType;
	GetXMLVal("type", diffNode, &type);
	diffData.mType = type;
	GetXMLVal("front_rear_torque_split", diffNode, &diffData.mFrontRearSplit);
// 	GetXMLVal("front_left_right_torque_split", diffNode, &diffData.mFrontLeftRightSplit);
// 	GetXMLVal("rear_left_right_torque_split", diffNode, &diffData.mRearLeftRightSplit);

	//	Wheels data
	pugi::xml_node wheelNode = xml.child("wheel");

	struct WheelIndexSearcher
	{
		const char *nameToFind;
		WheelIndexSearcher(const char *n): nameToFind(n) {}
		bool operator() (const char *a)
		{
			return strcmpi(a, nameToFind) == 0;
		}
	};

	const char * const * start = VEHICLE_PART_NAMES + MAX_HULL_PARTS_COUNT;
	const char * const * end = start + MAX_WHEELS_COUNT;

	while (wheelNode)
	{
		const char * const *n = std::find_if(start, end, WheelIndexSearcher(wheelNode.attribute("val").value()));
		if (n != end)
		{
			size_t idx = std::distance(start, n);
			WheelData &wd = wheelsData[idx];

			PxVehicleWheelData &wheelData = wd.wheelData;
			GetXMLVal("radius_multiplier", wheelNode, &wheelData.mRadius);
			GetXMLVal("mass", wheelNode, &wheelData.mMass);
			GetXMLVal("damping_rate", wheelNode, &wheelData.mDampingRate);
			GetXMLVal("max_break_torque", wheelNode, &wheelData.mMaxBrakeTorque);
			GetXMLVal("max_hand_break_torque", wheelNode, &wheelData.mMaxHandBrakeTorque);
			GetXMLVal("max_steer_angle", wheelNode, &wheelData.mMaxSteer);
			wheelData.mMaxSteer = R3D_DEG2RAD(wheelData.mMaxSteer);
			GetXMLVal("toe_angle", wheelNode, &wheelData.mToeAngle);
			wheelData.mToeAngle = R3D_DEG2RAD(wheelData.mToeAngle);

			PxVehicleSuspensionData &suspensionData = wd.suspensionData;
			GetXMLVal("suspension_spring_strength", wheelNode, &suspensionData.mSpringStrength);
			GetXMLVal("suspension_spring_damper_rate", wheelNode, &suspensionData.mSpringDamperRate);
			GetXMLVal("suspension_max_compression", wheelNode, &suspensionData.mMaxCompression);
			GetXMLVal("suspension_max_droop", wheelNode, &suspensionData.mMaxDroop);

			r3dVector v1(wd.suspensionTravelDir.x, wd.suspensionTravelDir.y, wd.suspensionTravelDir.z);
			GetXMLVal("suspension_travel_dir", wheelNode, &v1);
			wd.suspensionTravelDir = PxVec3(v1.x, v1.y, v1.z);

			//GetXMLVal("tire_logitudal_stiffness_per_unit_gravity", wheelNode, &wd.tireData.mLongitudinalStiffnessPerUnitGravity);
		}
		wheelNode = wheelNode.next_sibling();
	}
	delete [] fileBuffer;
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::Save(const char *fileName)
{
	if (!fileName)
	{
		fileName = driveFileDefinitionPath.c_str();
	}

	pugi::xml_document xmlDoc;
	pugi::xml_node v = xmlDoc.append_child();
	v.set_name("vehicle");
	
	SetXMLVal("ackermann_accuracy", v, &ackermannData.mAccuracy);
	SetXMLVal("gears_switch_time", v, &gearsData.mSwitchTime);

	pugi::xml_node chassis = v.append_child();
	chassis.set_name("chassis");
	SetXMLVal("mass", chassis, &chassisData.mMass);

	pugi::xml_node engine = v.append_child();
	engine.set_name("engine");
	SetXMLVal("peak_torque", engine, &engineData.mPeakTorque);
	SetXMLVal("max_omega", engine, &engineData.mMaxOmega);

	pugi::xml_node differential = v.append_child();
	differential.set_name("differential");
	SetXMLVal("front_rear_torque_split", differential, &diffData.mFrontRearSplit);

	for (PxU32 i = 0; i < numWheels; ++i)
	{
		WheelData &wd = wheelsData[i];
		pugi::xml_node wheel = v.append_child();
		wheel.set_name("wheel");
		wheel.append_attribute("val").set_value(VEHICLE_PART_NAMES[i + MAX_HULL_PARTS_COUNT]);

		SetXMLVal("mass", wheel, &wd.wheelData.mMass);
		SetXMLVal("damping_rate", wheel, &wd.wheelData.mDampingRate);
		SetXMLVal("max_break_torque", wheel, &wd.wheelData.mMaxBrakeTorque);
		SetXMLVal("max_hand_break_torque", wheel, &wd.wheelData.mMaxHandBrakeTorque);
		float f2 = R3D_RAD2DEG(wd.wheelData.mMaxSteer);
		SetXMLVal("max_steer_angle", wheel, &f2);
		float f1 = R3D_RAD2DEG(wd.wheelData.mToeAngle);
		SetXMLVal("toe_angle", wheel, &f1);
		SetXMLVal("suspension_spring_strength", wheel, &wd.suspensionData.mSpringStrength);
		SetXMLVal("suspension_spring_damper_rate", wheel, &wd.suspensionData.mSpringDamperRate);
		SetXMLVal("suspension_max_compression", wheel, &wd.suspensionData.mMaxCompression);
		SetXMLVal("suspension_max_droop", wheel, &wd.suspensionData.mMaxDroop);
	}

	xmlDoc.save_file(fileName);
	extern float g_ShowSaveSign ;
	g_ShowSaveSign = r3dGetTime() + 2.2f ;

	return true;
}

//////////////////////////////////////////////////////////////////////////

void VehicleDescriptor::ConfigureVehicleSimulationData(PxVehicleDriveSimData4W *dd, PxVehicleWheelsSimData *wd)
{
	if (!vehicle && (!dd || !wd))
		return;

	if (!dd)
		dd = &vehicle->mDriveSimData;

	dd->setDiffData(diffData);
	dd->setAckermannGeometryData(ackermannData);
	dd->setClutchData(clutchData);
	dd->setEngineData(engineData);
	dd->setGearsData(gearsData);

	if (!wd)
		wd = &vehicle->mWheelsSimData;

	for (uint32_t i = 0; i < numWheels; ++i)
	{
		wd->setSuspensionData(i, wheelsData[i].suspensionData);
		wd->setSuspTravelDirection(i, wheelsData[i].suspensionTravelDir);
		wd->setTireData(i, wheelsData[i].tireData);
		wd->setWheelData(i, wheelsData[i].wheelData);
	}
}

//////////////////////////////////////////////////////////////////////////

void VehicleDescriptor::InitToDefault()
{
	gearsData.mNumRatios = 3;
	diffData.mType = PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_4WD;

	for (PxU32 i = 0; i < numWheels; ++i)
	{
		PxVehicleWheelData &wd = wheelsData[i].wheelData;
		wd.mMaxBrakeTorque = 25000.0f;

		PxVehicleSuspensionData &sd = wheelsData[i].suspensionData;
		sd.mSpringStrength = 40000.0f;
		sd.mSpringDamperRate = 3000.0f;
	}
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::GetExitIndex( r3dVector& outVector, uint32_t exitIndex ) const
{
	const r3dBone *bonesStart = skl->Bones;
	const r3dBone *bonesEnd = skl->Bones + skl->NumBones;
	
	const r3dBone *partName = std::find_if(bonesStart, bonesEnd, BoneSearcher(VEHICLE_EXIT_NAMES[exitIndex]));
	if (partName != bonesEnd)
	{
		size_t idx = std::distance(bonesStart, partName);
		
		outVector = skl->Bones[idx].vRelPlacement;
		return true;
	}
	return false;
}

#endif VEHICLES_ENABLED
#endif

//=========================================================================
//	Module: VehicleDescriptor.cpp
//	Copyright (C) 2012.
//=========================================================================

#pragma once
#include "r3dPCH.h"
#include "r3d.h"

#if VEHICLES_ENABLED
#include "VehicleDescriptor.h"
#include "XMLHelpers.h"
#include "PhysXWorld.h"

struct BoneSearcher
{
	const char *toFind;
	explicit BoneSearcher(const char *nameToFind): toFind(nameToFind) {}
	bool operator() (const r3dBone &b)
	{
		return strcmpi(toFind, b.Name) == 0;
	}
};


//////////////////////////////////////////////////////////////////////////

VehicleDescriptor::VehicleDescriptor()
: numWheels(0)
, numHullParts(0)
, vehicle(0)
, skl(0)
, owner(0)
, wheelsData(0)
{
	for (uint32_t i = 0; i < _countof(wheelBonesRemapIndices); ++i)
	{
		wheelBonesRemapIndices[i] = INVALID_INDEX;
	}
	for (uint32_t i = 0; i < _countof(hullBonesRemapIndices); ++i)
	{
		hullBonesRemapIndices[i] = INVALID_INDEX;
	}
	::ZeroMemory(wheelCenterOffsets, sizeof(wheelCenterOffsets));
	::ZeroMemory(hullCenterOffsets, sizeof(hullCenterOffsets));
}

//////////////////////////////////////////////////////////////////////////

VehicleDescriptor::~VehicleDescriptor()
{
	if (vehicle)
		vehicle->free();
	delete skl;
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::Init(const r3dMesh *m)
{
	if (!Load(m))
	{
		r3dArtBug("Mesh does not have associated vehicle drive information");
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::InitSkeleton(const r3dSkeleton *s)
{
	for (uint32_t i = 0; i < MAX_WHEELS_COUNT; ++i)
	{
		wheelBonesRemapIndices[i] = INCORRECT_INDEX;
		wheelCenterOffsets[i] = PxVec3(0, 0, 0);
	}

	for (uint32_t i = 0; i < MAX_HULL_PARTS_COUNT; ++i)
	{
		hullBonesRemapIndices[i] = INCORRECT_INDEX;
		hullCenterOffsets[i] = PxVec3(0, 0, 0);
	}

	//	4 wheels + 1 hull part at least
	if (!s || s->NumBones < 5)
		return false;


	const r3dBone *bonesStart = s->Bones;
	const r3dBone *bonesEnd = s->Bones + s->NumBones;

	numWheels = 0;
	numHullParts = 0;

	for (uint32_t i = 0; i < MAX_WHEELS_COUNT; ++i)
	{
		const r3dBone *partName = std::find_if(bonesStart, bonesEnd, BoneSearcher(VEHICLE_PART_NAMES[i + MAX_HULL_PARTS_COUNT]));
		if (partName != bonesEnd)
		{
			size_t idx = std::distance(bonesStart, partName);
			wheelBonesRemapIndices[i] = idx;
			++numWheels;
		}
	}

	for (uint32_t i = 0; i < MAX_HULL_PARTS_COUNT; ++i)
	{
		const r3dBone *partName = std::find_if(bonesStart, bonesEnd, BoneSearcher(VEHICLE_PART_NAMES[i]));
		if (partName != bonesEnd)
		{
			size_t idx = std::distance(bonesStart, partName);
			hullBonesRemapIndices[i] = idx;
			++numHullParts;
		}
	}

	//	Calculate center offsets
	r3dVector tmp = s->Bones[hullBonesRemapIndices[0]].vRelPlacement;
	PxVec3 centerPos(tmp.x, tmp.y, tmp.z);
	for (uint32_t i = 0; i < MAX_WHEELS_COUNT; ++i)
	{
		uint32_t boneIdx = wheelBonesRemapIndices[i];
		if (boneIdx == INCORRECT_INDEX)
			continue;

		r3dVector tmp = s->Bones[boneIdx].vRelPlacement;
		PxVec3 bonePos(tmp.x, tmp.y, tmp.z);
		wheelCenterOffsets[i] = bonePos;
		std::swap(wheelCenterOffsets[i].x, wheelCenterOffsets[i].z);
		wheelCenterOffsets[i].z *= -1;
	}
 	std::swap(wheelCenterOffsets[0], wheelCenterOffsets[2]);
 	std::swap(wheelCenterOffsets[1], wheelCenterOffsets[3]);

	for (uint32_t i = 0; i < MAX_HULL_PARTS_COUNT; ++i)
	{
		uint32_t boneIdx = hullBonesRemapIndices[i];
		if (boneIdx == INCORRECT_INDEX)
			continue;

		r3dVector tmp = s->Bones[boneIdx].vRelPlacement;
		PxVec3 bonePos(tmp.x, tmp.y, tmp.z);
		hullCenterOffsets[i] = bonePos;
		std::swap(hullCenterOffsets[i].x, hullCenterOffsets[i].z);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

uint32_t VehicleDescriptor::GetIndex(const uint32_t *arr, uint32_t arrCount, uint32_t boneIndex) const
{
	for (uint32_t i = 0; i < arrCount; ++i)
	{
		if (arr[i] == boneIndex)
			return i;
	}
	return INVALID_INDEX;
}

//////////////////////////////////////////////////////////////////////////

uint32_t VehicleDescriptor::GetWheelIndex(uint32_t boneId) const
{
	return GetIndex(wheelBonesRemapIndices, MAX_WHEELS_COUNT, boneId);
}

//////////////////////////////////////////////////////////////////////////

uint32_t VehicleDescriptor::GetHullIndex(uint32_t boneId) const
{
	return GetIndex(hullBonesRemapIndices, MAX_HULL_PARTS_COUNT, boneId);
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::Load(const r3dMesh *m)
{
	std::string skeletonPath = m->FileName.c_str();
	if (skeletonPath.size() < 3)
		return false;

	skeletonPath.replace(skeletonPath.size() - 3, 3, "skl");

	driveFileDefinitionPath = m->FileName.c_str();
	driveFileDefinitionPath.erase(driveFileDefinitionPath.size() - 4);
	driveFileDefinitionPath += "_DriveData.xml";

	r3dFile* f = r3d_open(driveFileDefinitionPath.c_str(), "rb");
	if ( !f )
	{
		r3dOutToLog("Failed to open vehicle definition file: %s\n", driveFileDefinitionPath.c_str());
		return false;
	}

	char* fileBuffer = new char[f->size + 1];
	fread(fileBuffer, f->size, 1, f);
	fileBuffer[f->size] = 0;

	pugi::xml_document xmlDoc;
	pugi::xml_parse_result parseResult = xmlDoc.load_buffer_inplace(fileBuffer, f->size);
	fclose(f);
	if (!parseResult)
		r3dError("Failed to parse XML, error: %s", parseResult.description());

	pugi::xml_node xml = xmlDoc.child("vehicle");

	skl = new r3dSkeleton;
	skl->LoadBinary(skeletonPath.c_str());

	if (!InitSkeleton(skl))
		return false;

	wheelsData.Resize(numWheels);
	InitToDefault();

	//	Ackerman data
	GetXMLVal("ackermann_accuracy", xml, &ackermannData.mAccuracy);

	//	Gears data
	GetXMLVal("gears_switch_time", xml, &gearsData.mSwitchTime);

	// chassis data
	pugi::xml_node chassisNode = xml.child("chassis");
	GetXMLVal("mass", chassisNode, &chassisData.mMass);
	r3dVector v1(chassisData.mMOI.x, chassisData.mMOI.y, chassisData.mMOI.z);
	GetXMLVal("moi_offset", chassisNode, &v1);
	chassisData.mMOI = PxVec3(v1.x, v1.y, v1.z);
	r3dVector v2(chassisData.mCMOffset.x, chassisData.mCMOffset.y, chassisData.mCMOffset.z);
	GetXMLVal("cm_offset", chassisNode, &v2);
	chassisData.mCMOffset = PxVec3(v2.x, v2.y, v2.z);;
	//chassisData.mMass = 1700;

	//	Engine data
	pugi::xml_node engineNode = xml.child("engine");
	GetXMLVal("peak_torque", engineNode, &engineData.mPeakTorque);
	GetXMLVal("max_omega", engineNode, &engineData.mMaxOmega);
	engineData.mPeakTorque = 250;
	engineData.mMaxOmega = 1500;
// 	GetXMLVal("damping_rate_full_throttle", engineNode, &engineData.mDampingRateFullThrottle);
// 	GetXMLVal("damping_rate_zero_throttle_clutch_disengaged", engineNode, &engineData.mDampingRateZeroThrottleClutchDisengaged);
// 	GetXMLVal("damping_rate_zero_throttle_clutch_engaged", engineNode, &engineData.mDampingRateZeroThrottleClutchEngaged);

	// Differential data
	pugi::xml_node diffNode = xml.child("differential");
	int type = diffData.mType;
	GetXMLVal("type", diffNode, &type);
	diffData.mType = type;
	GetXMLVal("front_rear_torque_split", diffNode, &diffData.mFrontRearSplit);
// 	GetXMLVal("front_left_right_torque_split", diffNode, &diffData.mFrontLeftRightSplit);
// 	GetXMLVal("rear_left_right_torque_split", diffNode, &diffData.mRearLeftRightSplit);

	//	Wheels data
	pugi::xml_node wheelNode = xml.child("wheel");

	struct WheelIndexSearcher
	{
		const char *nameToFind;
		WheelIndexSearcher(const char *n): nameToFind(n) {}
		bool operator() (const char *a)
		{
			return strcmpi(a, nameToFind) == 0;
		}
	};

	const char * const * start = VEHICLE_PART_NAMES + MAX_HULL_PARTS_COUNT;
	const char * const * end = start + MAX_WHEELS_COUNT;

	while (wheelNode)
	{
		const char * const *n = std::find_if(start, end, WheelIndexSearcher(wheelNode.attribute("val").value()));
		if (n != end)
		{
			size_t idx = std::distance(start, n);
			WheelData &wd = wheelsData[idx];

			PxVehicleWheelData &wheelData = wd.wheelData;
			GetXMLVal("radius_multiplier", wheelNode, &wheelData.mRadius);
			GetXMLVal("mass", wheelNode, &wheelData.mMass);
			GetXMLVal("damping_rate", wheelNode, &wheelData.mDampingRate);
			GetXMLVal("max_break_torque", wheelNode, &wheelData.mMaxBrakeTorque);
			GetXMLVal("max_hand_break_torque", wheelNode, &wheelData.mMaxHandBrakeTorque);
			GetXMLVal("max_steer_angle", wheelNode, &wheelData.mMaxSteer);
			wheelData.mMaxSteer = R3D_DEG2RAD(wheelData.mMaxSteer);
			GetXMLVal("toe_angle", wheelNode, &wheelData.mToeAngle);
			wheelData.mToeAngle = R3D_DEG2RAD(wheelData.mToeAngle);

			PxVehicleSuspensionData &suspensionData = wd.suspensionData;
			GetXMLVal("suspension_spring_strength", wheelNode, &suspensionData.mSpringStrength);
			GetXMLVal("suspension_spring_damper_rate", wheelNode, &suspensionData.mSpringDamperRate);
			GetXMLVal("suspension_max_compression", wheelNode, &suspensionData.mMaxCompression);
			GetXMLVal("suspension_max_droop", wheelNode, &suspensionData.mMaxDroop);

			r3dVector v1(wd.suspensionTravelDir.x, wd.suspensionTravelDir.y, wd.suspensionTravelDir.z);
			GetXMLVal("suspension_travel_dir", wheelNode, &v1);
			wd.suspensionTravelDir = PxVec3(v1.x, v1.y, v1.z);
			wd.wheelData.mMass = 40;

			//GetXMLVal("tire_logitudal_stiffness_per_unit_gravity", wheelNode, &wd.tireData.mLongitudinalStiffnessPerUnitGravity);
		}
		wheelNode = wheelNode.next_sibling();
	}
	delete [] fileBuffer;
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::Save(const char *fileName)
{
	if (!fileName)
	{
		fileName = driveFileDefinitionPath.c_str();
	}

	pugi::xml_document xmlDoc;
	pugi::xml_node v = xmlDoc.append_child();
	v.set_name("vehicle");
	
	SetXMLVal("ackermann_accuracy", v, &ackermannData.mAccuracy);
	SetXMLVal("gears_switch_time", v, &gearsData.mSwitchTime);

	pugi::xml_node chassis = v.append_child();
	chassis.set_name("chassis");
	SetXMLVal("mass", chassis, &chassisData.mMass);

	pugi::xml_node engine = v.append_child();
	engine.set_name("engine");
	SetXMLVal("peak_torque", engine, &engineData.mPeakTorque);
	SetXMLVal("max_omega", engine, &engineData.mMaxOmega);

	pugi::xml_node differential = v.append_child();
	differential.set_name("differential");
	SetXMLVal("front_rear_torque_split", differential, &diffData.mFrontRearSplit);

	for (PxU32 i = 0; i < numWheels; ++i)
	{
		WheelData &wd = wheelsData[i];
		pugi::xml_node wheel = v.append_child();
		wheel.set_name("wheel");
		wheel.append_attribute("val").set_value(VEHICLE_PART_NAMES[i + MAX_HULL_PARTS_COUNT]);

		SetXMLVal("mass", wheel, &wd.wheelData.mMass);
		SetXMLVal("damping_rate", wheel, &wd.wheelData.mDampingRate);
		SetXMLVal("max_break_torque", wheel, &wd.wheelData.mMaxBrakeTorque);
		SetXMLVal("max_hand_break_torque", wheel, &wd.wheelData.mMaxHandBrakeTorque);
		float f2 = R3D_RAD2DEG(wd.wheelData.mMaxSteer);
		SetXMLVal("max_steer_angle", wheel, &f2);
		float f1 = R3D_RAD2DEG(wd.wheelData.mToeAngle);
		SetXMLVal("toe_angle", wheel, &f1);
		SetXMLVal("suspension_spring_strength", wheel, &wd.suspensionData.mSpringStrength);
		SetXMLVal("suspension_spring_damper_rate", wheel, &wd.suspensionData.mSpringDamperRate);
		SetXMLVal("suspension_max_compression", wheel, &wd.suspensionData.mMaxCompression);
		SetXMLVal("suspension_max_droop", wheel, &wd.suspensionData.mMaxDroop);
	}

	xmlDoc.save_file(fileName);
	extern float g_ShowSaveSign ;
	g_ShowSaveSign = r3dGetTime() + 2.2f ;

	return true;
}

//////////////////////////////////////////////////////////////////////////

void VehicleDescriptor::ConfigureVehicleSimulationData(PxVehicleDriveSimData4W *dd, PxVehicleWheelsSimData *wd)
{
	if (!vehicle && (!dd || !wd))
		return;

	if (!dd)
		dd = &vehicle->mDriveSimData;

	dd->setDiffData(diffData);
	dd->setAckermannGeometryData(ackermannData);
	dd->setClutchData(clutchData);
	dd->setEngineData(engineData);
	dd->setGearsData(gearsData);

	if (!wd)
		wd = &vehicle->mWheelsSimData;

	for (uint32_t i = 0; i < numWheels; ++i)
	{
		wd->setSuspensionData(i, wheelsData[i].suspensionData);
		wd->setSuspTravelDirection(i, wheelsData[i].suspensionTravelDir);
		wd->setTireData(i, wheelsData[i].tireData);
		wd->setWheelData(i, wheelsData[i].wheelData);
	}
}

//////////////////////////////////////////////////////////////////////////

void VehicleDescriptor::InitToDefault()
{
	gearsData.mNumRatios = 3;
	diffData.mType = PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_4WD;

	for (PxU32 i = 0; i < numWheels; ++i)
	{
		PxVehicleWheelData &wd = wheelsData[i].wheelData;
		wd.mMaxBrakeTorque = 25000.0f;

		PxVehicleSuspensionData &sd = wheelsData[i].suspensionData;
		sd.mSpringStrength = 40000.0f;
		sd.mSpringDamperRate = 3000.0f;
	}
}

//////////////////////////////////////////////////////////////////////////

bool VehicleDescriptor::GetExitIndex( r3dVector& outVector, uint32_t exitIndex ) const
{
	const r3dBone *bonesStart = skl->Bones;
	const r3dBone *bonesEnd = skl->Bones + skl->NumBones;
	
	const r3dBone *partName = std::find_if(bonesStart, bonesEnd, BoneSearcher(VEHICLE_EXIT_NAMES[exitIndex]));
	if (partName != bonesEnd)
	{
		size_t idx = std::distance(bonesStart, partName);
		
		outVector = skl->Bones[idx].vRelPlacement;
		return true;
	}
	return false;
}

#endif VEHICLES_ENABLED