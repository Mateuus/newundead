//=========================================================================
//	Module: VehicleManager.cpp
//	Copyright (C) 2012.
//=========================================================================

#include "r3dPCH.h"

#if VEHICLES_ENABLED

#include "r3d.h"
#include "extensions/PxStringTableExt.h"
#include "ObjManag.h"
#include "vehicle/PxVehicleSDK.h"
#include "geometry/PxConvexMeshGeometry.h"
#include "PxBatchQueryDesc.h"

#include "ServerVehicleManager.h"

void VehicleSetupDrivableShapeQueryFilterData(PxFilterData &qryFilterData)
{
	r3d_assert(qryFilterData.word3 == 0 || qryFilterData.word3 == VEHICLE_DRIVABLE_SURFACE || qryFilterData.word3 == VEHICLE_NONDRIVABLE_SURFACE);
	qryFilterData.word3 = VEHICLE_DRIVABLE_SURFACE;
}

#endif // VEHICLES_ENABLED