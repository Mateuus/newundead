//	Module: VehicleManager.h
//	Copyright (C) 2012.
//=========================================================================

#pragma once

class VehicleManager;

#if VEHICLES_ENABLED

#include "PxBatchQuery.h"
#include "vehicle/PxVehicleDrive.h"
#include "vehicle/PxVehicleUtilSetup.h"

#include "VehicleDescriptor.h"

//////////////////////////////////////////////////////////////////////////

const uint32_t VEHICLE_DRIVABLE_SURFACE = 0xffff0000;
const uint32_t VEHICLE_NONDRIVABLE_SURFACE = 0xffff;

//Set up query filter data so that vehicles can drive on shapes with this filter data.
//Note that we have reserved word3 of the PxFilterData for vehicle raycast query filtering.
void VehicleSetupDrivableShapeQueryFilterData(PxFilterData &qryFilterData);

//////////////////////////////////////////////////////////////////////////

#endif // VEHICLES_ENABLED