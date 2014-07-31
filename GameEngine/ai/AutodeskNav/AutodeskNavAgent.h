//=========================================================================
//	Module: AutodeskNavAgent.h
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#pragma once

#if ENABLE_AUTODESK_NAVIGATION

//////////////////////////////////////////////////////////////////////////

#include "gwnavruntime/math/vec3f.h"
#include "gwnavruntime/kernel/SF_RefCount.h"
#include "gwnavruntime/world/bot.h"
#include "gwnavruntime/queries/astarquery.h"
#include "gwnavruntime/queries/raycastquery.h"
#include "gwnavruntime/queries/moveonnavmeshquery.h"
#include "gwnavruntime/queries/trianglefromposquery.h"
#include "gwnavruntime/queries/insideposfromoutsideposquery.h"

#include "AutodeskNavMesh.h"

class AutodeskNavAgent
{
public:
	enum EStatus
	{
		Idle = 0,
		ComputingPath,
		Moving,
		Arrived,
		PathNotFound,
		Failed,
	};
	EStatus		m_status;

	Kaim::Vec3f	m_velocity;
	Kaim::Vec3f	m_position;
	float		m_arrivalPrecisionRadius;

	Kaim::Ptr<Kaim::Bot> m_navBot;
	Kaim::Ptr<Kaim::AStarQuery<Kaim::AStarCustomizer_Default>> m_pathFinderQuery;
	
	void Move(KyFloat32 simulationTimeInSeconds);
	void  MoveOnNavGraph(KyFloat32 simulationTimeInSeconds, const Kaim::Vec3f& velocity, Kaim::BotUpdateConfig& botUpdateConfig);
	bool  MoveOnNavMesh(KyFloat32 simulationTimeInSeconds, const Kaim::Vec3f& velocity, Kaim::BotUpdateConfig& botUpdateConfig);
	
	void HandleArrivalAndUpperBound();
	void  HandleArrival();
	void  HandleUpperBound();

public:
	AutodeskNavAgent();
	virtual ~AutodeskNavAgent();

	bool Init(Kaim::World* world, const r3dVector &pos);
	void Update(float timeStep);
	bool StartMove(const r3dPoint3D &target, float maxAstarDist = 999999);
	void StopMove();
	void SetTargetSpeed(float speed);
	r3dVector GetPosition() const;
#ifndef FINAL_BUILD
	void DebugDraw();
#endif
}; 

#endif
