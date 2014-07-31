//=========================================================================
//	Module: AutodeskNavMesh.h
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#pragma once

#if ENABLE_AUTODESK_NAVIGATION

//////////////////////////////////////////////////////////////////////////

#include "gwnavruntime/world/world.h"
#include "gwnavruntime/world/bot.h"
#include "gwnavruntime/queries/astarquery.h"
#include "gwnavruntime/querysystem/queryqueuearray.h"
#include "gwnavruntime/querysystem/queryqueue.h"
#include "gwnavruntime/database/database.h"
#include "gwnavruntime/navdata/navdata.h"
#include "gwnavruntime/visualdebug/visualdebugserver.h"
#include "gwnavruntime/basesystem/basesystem.h"
#include "gwnavruntime/basesystem/coordsystem.h"
#include "gwnavruntime/basesystem/gamewarekey.h"
#include "gwnavruntime/basesystem/logger.h"
#include "gwnavruntime/math/vec3f.h"
#include "gwnavruntime/world/boxobstacle.h"
#include "gwnavruntime/world/cylinderobstacle.h"

#include "gwnavgeneration.h"

#include "AutodeskCustomBridges.h"

//////////////////////////////////////////////////////////////////////////

class AutodeskCollisionBridge;
class AutodeskDrawBridge;
class AutodeskNavMeshDrawer;
class AutodeskNavAgent;

//////////////////////////////////////////////////////////////////////////

R3D_FORCEINLINE r3dPoint3D KY_R3D(const Kaim::Vec3f& vec) 
{ 
	return r3dPoint3D(vec.x, vec.z, vec.y);
}

R3D_FORCEINLINE Kaim::Vec3f R3D_KY(const r3dPoint3D& v)
{ 
	return Kaim::Vec3f(v.x, v.z, v.y);
}

//////////////////////////////////////////////////////////////////////////

class AutodeskNavMesh
{
//	AutodeskDrawBridge drawBridge;
#ifdef WO_SERVER
public:
	AutodeskPerfCounter2 perfBridge;
#else
private:
	AutodeskPerfCounter perfBridge;
#endif
	enum { BUDGET_NAVQUERIES_MS = 1000, };

	Kaim::Ptr<Kaim::World> world;
	Kaim::Ptr<Kaim::NavData> navData;
	Kaim::CoordSystem m_coordSystem;

	r3dTL::TArray<Kaim::Ptr<Kaim::WorldElement>> obstacles;
	r3dTL::TArray<AutodeskNavAgent*> agents;
	
	int GetFreeObstacleIdx();

	void SetupVisualDebug();
	void ShutdownVisualDebug();

	void ClearNavDataDir();
#ifndef FINAL_BUILD
	AutodeskNavMeshDrawer navDraw;
#endif

public:
	AutodeskNavMesh();
	~AutodeskNavMesh();
	bool Init();
	bool LoadPathData();
	void RemovePathData();
	void Close();
	void Update();
	void BuildForCurrentLevel();
	void DebugDraw();
	void DebugDrawObstacles();
	void ExportToObj();

	void LoadBuildConfig();
	void SaveBuildConfig();
	void SaveNavGenProj();

	/** Return obstacle ID. -1 if failed. */
	int AddObstacle(const r3dBoundBox &bb, float rotX);
	int AddObstacle(const r3dCylinder &c);
	bool RemoveObstacle(int idx);

	AutodeskNavAgent * CreateNavAgent(const r3dVector &pos);
	void AddNavAgent(AutodeskNavAgent *a);
	void DeleteNavAgent(AutodeskNavAgent *a);

	Kaim::World* GetWorld() { return world; }
	Kaim::Database * GetDB() { return world->GetDatabase(0); }

	// get closest position from given point on navmesh
	bool GetClosestNavMeshPoint(r3dPoint3D &inOut, float searchHeightRange, float searchWidthRadius);

	// check if point is inside navmesh
	bool IsNavPointValid(const r3dPoint3D &inOut);

	// check if point is inside navmesh, adjust height
	bool AdjustNavPointHeight(r3dPoint3D &inOut, float searchHeightRange);

	bool doDebugDraw;
	Kaim::GeneratorParameters buildGlobalConfig;

	bool initialized;
};

extern AutodeskNavMesh gAutodeskNavMesh;

//////////////////////////////////////////////////////////////////////////

#endif // ENABLE_AUTODESK_NAVIGATION
