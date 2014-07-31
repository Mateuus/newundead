//=========================================================================
//	Module: AutodeskNavAgent.cpp
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#if ENABLE_AUTODESK_NAVIGATION

//////////////////////////////////////////////////////////////////////////

#include "AutodeskNavAgent.h"
#include "../../gameobjects/PhysXWorld.h"
#include "../../gameobjects/PhysObj.h"

using namespace Kaim;
extern r3dCamera gCam;

//////////////////////////////////////////////////////////////////////////

AutodeskNavAgent::AutodeskNavAgent()
{
	m_arrivalPrecisionRadius = 0.2f;
	m_status = Idle;
}

//////////////////////////////////////////////////////////////////////////

AutodeskNavAgent::~AutodeskNavAgent()
{
	if (m_navBot)
		m_navBot->RemoveFromDatabase();
	m_navBot = KY_NULL;
	m_pathFinderQuery = KY_NULL;
}

//////////////////////////////////////////////////////////////////////////

bool AutodeskNavAgent::Init(Kaim::World* world, const r3dVector &pos)
{
	r3d_assert(world);

	Kaim::BotInitConfig botInitConfig;
	botInitConfig.m_userData = this;
	botInitConfig.m_database = world->GetDatabase(0);
	botInitConfig.m_startPosition = Vec3f(pos.x, pos.z, pos.y);
	botInitConfig.m_desiredSpeed = 5.0f;
	botInitConfig.m_enableAvoidance = true;
	botInitConfig.m_enableShortcut  = true;
	m_navBot = *KY_NEW Kaim::Bot(botInitConfig);

	m_pathFinderQuery = *KY_NEW Kaim::AStarQuery<Kaim::AStarCustomizer_Default>;
	m_navBot->SetAStarQuery<Kaim::AStarCustomizer_Default>(m_pathFinderQuery);

	Kaim::AvoidanceComputerConfig& avoidanceComputerConfig = m_navBot->GetAvoidanceComputerConfig();
	avoidanceComputerConfig.m_enableSlowingDown = true;
	avoidanceComputerConfig.m_enableStop        = true; //[unfortunately, does not work - all actors stop colliding] the VelocityObstacleSolver will	never consider stopping the Bot resulting in sometimes making it force its way through colliders
	avoidanceComputerConfig.m_enableBackMove    = true;

	m_navBot->AddToDatabase();

	m_position = botInitConfig.m_startPosition;
	m_velocity = Kaim::Vec3f(0.0f, 0.0f, 0.0f);

	return true;
}

//////////////////////////////////////////////////////////////////////////

void AutodeskNavAgent::Move(KyFloat32 simulationTimeInSeconds)
{
	Kaim::BotUpdateConfig botUpdateConfig;
	botUpdateConfig.m_position = m_navBot->GetPosition();  // by default keep the same position
	botUpdateConfig.m_velocity = Kaim::Vec3f::Zero();     // by default m_velocity = zero

	if(m_navBot->DoComputeTrajectory() == false)
	{
		MoveOnNavGraph(simulationTimeInSeconds, m_velocity, botUpdateConfig);
		m_navBot->SetNextUpdateConfig(botUpdateConfig);
		return;
	}

	/*
	if( m_allowForcePassage )
	{
		if( UpdateForcePassage(simulationTimeInSeconds) )
			return;
	}
	*/	

	/*
	if(Keyboard->IsPressed(kbsT))
	{
		r3dOutToLog("GetTargetOnLivePathStatus(): %d\n", m_navBot->GetTargetOnLivePathStatus());
		r3dOutToLog("GetPathValidityStatus(): %d\n", m_navBot->GetLivePath().GetPathValidityStatus());
		r3dOutToLog("GetPathFinderResult(): %d %d\n", m_pathFinderQuery->GetPathFinderResult(), m_pathFinderQuery->GetResult());
		if(m_navBot->GetPathFinderQuery())
			r3dOutToLog("m_processStatus: %d\n", m_navBot->GetPathFinderQuery()->m_processStatus);
		r3dOutToLog("\n");
	}*/
	
	if ((m_navBot->GetTargetOnLivePathStatus() == Kaim::TargetOnPathNotInitialized) ||
		(m_navBot->GetLivePath().GetPathValidityStatus() == Kaim::PathValidityStatus_NoPath))
	{
		m_navBot->SetNextUpdateConfig(botUpdateConfig);
		return;
	}

	m_velocity = m_navBot->GetTrajectory()->GetVelocity();
	m_status   = Moving;
	
	if (m_velocity.GetSquareLength() > 0.0f)
	{
		switch (m_navBot->GetTargetOnLivePath().GetPathEdgeType())
		{
			case Kaim::PathEdgeType_OnNavMesh:
			{
				MoveOnNavMesh(simulationTimeInSeconds, m_velocity, botUpdateConfig);
				break;
			}

			default:
			{
				MoveOnNavGraph(simulationTimeInSeconds, m_velocity, botUpdateConfig);
				break;
			}
		}
	}

	m_navBot->SetNextUpdateConfig(botUpdateConfig);
	
	m_position = botUpdateConfig.m_position;
}

bool AutodeskNavAgent::MoveOnNavMesh(KyFloat32 simulationTimeInSeconds, const Kaim::Vec3f& velocity, Kaim::BotUpdateConfig& botUpdateConfig)
{
	// Keep current position. But don't take reference, we will need this value after it has been changed.
	const Kaim::Vec3f previousPosition = botUpdateConfig.m_position;

	// compute new position
	Kaim::Vec3f move = velocity * simulationTimeInSeconds; // z=0.0f

	// query the navMesh to ensure that we are moving to a valid location
	Kaim::Vec2f move2d;
	KyFloat32 dist = move.GetNormalized2d(move2d);
	Kaim::MoveOnNavMeshQuery query;
	query.Initialize(m_navBot->GetDatabase(), previousPosition, move2d, dist);
	query.SetStartTrianglePtr(m_navBot->GetNavTrianglePtr()); // initialize the StartTrianglePtr with the triangle the bot is standing upon
	query.PerformQuery();
	switch (query.GetResult())
	{
	default:
	case Kaim::MOVEONNAVMESH_NOT_INITIALIZED:
	case Kaim::MOVEONNAVMESH_NOT_PROCESSED:
		return false;

	case Kaim::MOVEONNAVMESH_DONE_ARRIVALPOS_NOT_FOUND:
		//KY_DEBUG_WARNINGN(("[LabEngine] [NavMeshPhysics] Bot %u physics could not find arrival position!", m_navBot->GetVisualDebugId()));
		botUpdateConfig.m_position = query.GetArrivalPos();
		break;
	case Kaim::MOVEONNAVMESH_DONE_ARRIVALPOS_FOUND_COLLISION:
		//KY_DEBUG_WARNINGN(("[LabEngine] [NavMeshPhysics] Bot %u physics collided with navmesh!", m_navBot->GetVisualDebugId()));
		botUpdateConfig.m_position = query.GetArrivalPos();
		break;
	case Kaim::MOVEONNAVMESH_DONE_ARRIVALPOS_FOUND_NO_COLLISION:
		botUpdateConfig.m_position = query.GetArrivalPos();
		break;
	}

	// Compute exact velocity here
	botUpdateConfig.m_velocity = (botUpdateConfig.m_position - previousPosition) / simulationTimeInSeconds;
	return true;
}

void AutodeskNavAgent::MoveOnNavGraph(KyFloat32 simulationTimeInSeconds, const Kaim::Vec3f& velocity, Kaim::BotUpdateConfig& botUpdateConfig)
{
	botUpdateConfig.m_velocity = velocity;
	botUpdateConfig.m_position += botUpdateConfig.m_velocity * simulationTimeInSeconds;
}

void AutodeskNavAgent::HandleArrivalAndUpperBound()
{
	if (m_navBot->GetFollowedPath() == KY_NULL)
		return;

	switch(m_navBot->GetLivePath().GetUpperBoundType())
	{
		case Kaim::PathLastNode:
		{
			HandleArrival();
			break;
		}

		case Kaim::ValidityUpperBound:
		{
			HandleUpperBound();
			break;
		}

		case Kaim::ValidityTemporaryUpperBound:
			break; // Do nothing, just wait for time-sliced validity check completion
	}
}

void AutodeskNavAgent::HandleArrival()
{
	r3d_assert(m_navBot->GetFollowedPath() != KY_NULL);

	const Kaim::PositionOnLivePath& targetOnPath = m_navBot->GetTargetOnLivePath();
	if (targetOnPath.IsAtLastNodeOfPath() == false)
		return;

	const Kaim::Vec3f& destination = targetOnPath.GetPosition();
	if (m_navBot->HasReachedPosition(destination, m_arrivalPrecisionRadius))
	{
		m_navBot->ClearFollowedPath();
		m_status = Arrived;
	}
}

void AutodeskNavAgent::HandleUpperBound()
{
	float m_validityCheckDistance = 1.6f;

	const Kaim::Vec3f& livePathUpperBoundPosition = m_navBot->GetPathEventList().GetLastPathEvent().m_positionOnPath.GetPosition();
	if (Kaim::SquareDistance(m_navBot->GetPosition(), livePathUpperBoundPosition) < m_validityCheckDistance * m_validityCheckDistance)
	{
		// we don't know what the that "UpperBound" yet - need to investigate!
		r3dOutToLog("!!!!! got HandleUpperBound %f,%f,%f pos: %f,%f,%f\n", 
			livePathUpperBoundPosition.x, livePathUpperBoundPosition.z, livePathUpperBoundPosition.y, 
			m_position.x, m_position.z, m_position.y);
		m_status = Failed;
	}
}

void AutodeskNavAgent::Update(float timeStep)
{
	switch(m_status)
	{
		default: r3dError("bad status %d", m_status);

		case ComputingPath:
			if (m_navBot->GetPathFinderQuery()->m_processStatus == Kaim::QueryCanceled)
				m_status = Idle;
			else if (m_navBot->GetPathFinderQuery()->m_processStatus == Kaim::QueryDone)
				m_status = (m_navBot->GetFollowedPath() != KY_NULL) ? Moving : PathNotFound;
			break;

		case Moving:
			break;

		case Idle:
		case Arrived:
		case PathNotFound:
		case Failed:
			return;
	}

	Move(timeStep);

	if ((m_navBot->GetTargetOnLivePathStatus() == Kaim::TargetOnPathInInvalidNavData) ||
		(m_navBot->GetTargetOnLivePathStatus() == Kaim::TargetOnPathNotReachable))
	{
		m_status = Failed;
		return;
	}

	// Check arrival & upper bound
	HandleArrivalAndUpperBound();

	return;
}

bool AutodeskNavAgent::StartMove(const r3dPoint3D &target, float maxAstarDist)
{
	StopMove();
	
	Kaim::Vec3f kTarget(target.x, target.z, target.y);

	/*
	r3dOutToLog("bot pos: %f %f %f\n", m_navBot->GetPosition().x, m_navBot->GetPosition().y, m_navBot->GetPosition().z);	
	r3dOutToLog("bot trg: %f %f %f\n", kTarget.x, kTarget.y, kTarget.z);
	Kaim::TriangleFromPosQuery query;
	query.Initialize(m_navBot->GetDatabase(), kTarget);
	query.PerformQuery();
	r3dOutToLog("TriangleFromPosQuery, result:%d, triidx:%d query.GetAltitudeOfProjectionInTriangle():%f\n", query.GetResult(), query.GetResultTrianglePtr().GetTriangleIdx(), query.GetAltitudeOfProjectionInTriangle());
	*/
	
	// this is copy-paste from Bot::ComputeNewPathToDestination (need it because of SetPathMaxCost after initialize)
	if(m_pathFinderQuery->CanBeInitialized() == false)
	{
		r3dOutToLog("!!! !CanBeInitialized()\n");
		return false;
	}
	
	m_pathFinderQuery->Initialize(m_navBot->GetDatabase(), m_navBot->GetPosition(), kTarget);
	m_pathFinderQuery->SetStartTrianglePtr(m_navBot->GetNavTrianglePtr());
	m_pathFinderQuery->SetPathMaxCost(maxAstarDist);
	m_pathFinderQuery->SetNumberOfProcessedNodePerFrame(600); // default value is 30, it's not enough for us
	m_navBot->ComputeNewPathAsync(m_pathFinderQuery);
	
	m_status = ComputingPath;
	return true;
}

void AutodeskNavAgent::StopMove()
{
	if (m_navBot->IsComputingNewPath())
		m_navBot->CancelAsynPathComputation();

	if (m_navBot->GetFollowedPath() != KY_NULL)
		m_navBot->ClearFollowedPath();

	m_status = Idle;
}

void AutodeskNavAgent::SetTargetSpeed(float speed)
{
	m_navBot->SetDesiredSpeed(speed);
}


//////////////////////////////////////////////////////////////////////////

r3dVector AutodeskNavAgent::GetPosition() const
{
	return r3dVector(m_position.x, m_position.z, m_position.y);
}

//////////////////////////////////////////////////////////////////////////

#ifndef FINAL_BUILD
#ifndef WO_SERVER
void AutodeskNavAgent::DebugDraw()
{
	r3dBoundBox bb;
	bb.Org = GetPosition();
	bb.Size.Assign(m_navBot->GetRadius() * 2, m_navBot->GetHeight(), m_navBot->GetRadius() * 2);
	bb.Org.x -= bb.Size.x / 2;
	bb.Org.z -= bb.Size.z / 2;
	r3dDrawBoundBox(bb, gCam, r3dColor::white, 0.02f);

	r3dPoint3D scrCoord;
	if(r3dProjectToScreen(GetPosition() + r3dPoint3D(0, 1.8f, 0), &scrCoord))
	{
		extern CD3DFont* Font_Editor;
		r3dRenderer->SetRenderingMode(R3D_BLEND_PUSH | R3D_BLEND_NZ);
		Font_Editor->PrintF(scrCoord.x, scrCoord.y, r3dColor(255,255,255), "%d %d", m_status, m_navBot->GetTargetOnLivePathStatus());
		r3dRenderer->SetRenderingMode(R3D_BLEND_POP);
	}
}
#endif
#endif

//////////////////////////////////////////////////////////////////////////

#endif
