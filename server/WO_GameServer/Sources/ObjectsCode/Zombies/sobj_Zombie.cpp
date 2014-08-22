#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"
#include "ObjectsCode/Weapons/WeaponArmory.h"
#include "ObjectsCode/Weapons/HeroConfig.h"

#include "sobj_Zombie.h"
#include "sobj_ZombieSpawn.h"
#include "ObjectsCode/obj_ServerPlayer.h"
#include "ObjectsCode/sobj_DroppedItem.h"
#include "ObjectsCode/obj_ServerBarricade.h"
#include "ServerWeapons/ServerWeapon.h"

//Codex Carros
#include "ObjectsCode/Vehicles/obj_ServerVehicle.h"

#include "../../EclipseStudio/Sources/ObjectsCode/Gameplay/ZombieStates.h"

#include "../../GameEngine/ai/AutodeskNav/AutodeskNavMesh.h"

IMPLEMENT_CLASS(obj_Zombie, "obj_Zombie", "Object");
AUTOREGISTER_CLASS(obj_Zombie);

#if 0 //@ 
//DEBUG VARIABLES
int		_zai_DisableDetect       = 1;
float		_zai_IdleStatePatrolPerc = 1.0f;
float		_zai_NoPatrolPlayerDist  = -1.0f;
float		_zai_PlayerSpawnProtect  = 0.0f;
float		_zai_MaxSpawnDelay       = 0.1f;
float		_zai_AttackDamage        = 0.0f;
float		_zai_SZAttackDamage        = 0.0f; //Codex Super Zombie
int		_zai_DebugAI             = 1;
#else
int		_zai_DisableDetect       = 0;
float		_zai_IdleStatePatrolPerc = 0.4f;
float		_zai_NoPatrolPlayerDist  = 500.0;
float		_zai_PlayerSpawnProtect  = 35.0f;	// radius where zombie won't be spawned because of player presense
float		_zai_MaxSpawnDelay       = 9999.0f;
float		_zai_AttackDamage        = 23.0f;
float		_zai_SZAttackDamage        = 69.0f; //Codex Super Zombie
int		_zai_DebugAI             = 0;
#endif
float		_zai_MaxPatrolDistance   = 30.0f;	// distance to search for navpoints when switchint to patrol
float		_zai_MaxPursueDistance   = 300.0f;
float		_zai_AttackRadius        = 1.2f;
float		_zai_SZAttackTimer		 = 0.3f;	//Codex Super Zombie
float		_zai_AttackTimer         = 1.2f;
float		_zai_DistToRecalcPath    = 0.8f; // _zai_AttackRadius / 2
float		_zai_VisionConeCos       = cosf(50.0f); // have 100 degree vision

int		_zstat_NumZombies = 0;
int		_zstat_NavFails   = 0;
int		_zstat_Disabled   = 0;

obj_Zombie::obj_Zombie() : 
netMover(this, 1.0f / 10.0f, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
{
	_zstat_NumZombies++;

	ZombieDisabled = 0;

	spawnObject    = NULL;
	RunSpeed       = -1;
	WalkSpeed      = -1;
	DetectRadius   = -1;

	ZombieState    = EZombieStates::ZState_Idle;
	StateStartTime = r3dGetTime();
	StateTimer     = -1;
	nextDetectTime = r3dGetTime() + 2.0f;

	navAgent       = NULL;
	patrolPntIdx   = -1;
	moveFrameCount = 0;

	staggerTime    = -1;
	animState      = 0;
	ZombieHealth   = u_GetRandom(100.0f, 75.0f);
}

obj_Zombie::~obj_Zombie()
{
	_zstat_NumZombies--;
	if(ZombieDisabled)
		_zstat_Disabled--;
}

BOOL obj_Zombie::OnCreate()
{
	//r3dOutToLog("obj_Zombie %p %s created\n", this, Name.c_str()); CLOG_INDENT;

	ObjTypeFlags |= OBJTYPE_Zombie;

	r3d_assert(NetworkLocal);
	r3d_assert(GetNetworkID());
	r3d_assert(spawnObject);

	if(u_GetRandom() < spawnObject->sleepersRate)
		ZombieState = EZombieStates::ZState_Sleep;

	if(spawnObject->ZombieSpawnSelection.size() == 0)
		HeroItemID = u_GetRandom() >= 0.5f ? 20170 : 20183; // old behaviour. TODO: remove
	else
	{
		uint32_t idx1 = u_random(spawnObject->ZombieSpawnSelection.size());
		r3d_assert(idx1 < spawnObject->ZombieSpawnSelection.size());
		//HeroItemID = spawnObject->ZombieSpawnSelection[idx1];
		HeroItemID = spawnObject->ZombieSpawnSelection[idx1];
	}
	const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(HeroItemID);
	if(!heroConfig) { 
		r3dOutToLog("!!!! unable to spawn zombie - there is no hero config %d\n", HeroItemID);
		return FALSE;
	}

	FastZombie = u_GetRandom() > spawnObject->fastZombieChance;

	HalloweenZombie = false;//true false //NO more special zombies u_GetRandom() < (1.0f / 100) ? true : false; // every 100th zombie is special
	if(HalloweenZombie) FastZombie = 1;

	HeadIdx = u_random(heroConfig->getNumHeads());
	BodyIdx = u_random(heroConfig->getNumBodys());
	LegsIdx = u_random(heroConfig->getNumLegs());

	/*WalkSpeed  = u_GetRandom(-spawnObject->speedVariation, +spawnObject->speedVariation);
	RunSpeed   = u_GetRandom(-spawnObject->speedVariation, +spawnObject->speedVariation);*/

	WalkSpeed  = FastZombie ? 1.0f : 1.8f;
	RunSpeed   = FastZombie ? 2.9f : 3.2f;

	//WalkSpeed += WalkSpeed * u_GetRandom(-spawnObject->speedVariation, +spawnObject->speedVariation);
	//RunSpeed  += RunSpeed  * u_GetRandom(-spawnObject->speedVariation, +spawnObject->speedVariation);

	if (HeroItemID == 20207)//Codex Super Zombie
	{
		WalkSpeed += WalkSpeed * u_GetRandom(-spawnObject->speedVariation, +spawnObject->speedVariation);
		RunSpeed  += RunSpeed  * u_GetRandom(-spawnObject->speedVariation, +spawnObject->speedVariation);
		ZombieHealth = u_GetRandom(200.0f, 400.0f);
		r3dOutToLog("SuperZombie Spawned\n");
	}
	/*else
	{
		WalkSpeed += WalkSpeed * 0.30f;
		RunSpeed  += RunSpeed  * 0.40f;
	}*/

	r3d_assert(WalkSpeed > 0);
	r3d_assert(RunSpeed > 0);
	r3d_assert(DetectRadius >= 0);

	// need to create nav agent so it will be placed to navmesh position
	CreateNavAgent();

	gServerLogic.NetRegisterObjectToPeers(this);

	return parent::OnCreate();
}

BOOL obj_Zombie::OnDestroy()
{
	//r3dOutToLog("obj_Zombie %p destroyed\n", this);

	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));

	DeleteZombieNavAgent(navAgent);
	navAgent = NULL;

	return parent::OnDestroy();
}

void obj_Zombie::DisableZombie()
{
	AILog(0, "DisableZombie\n");
	StopNavAgent();

	ZombieDisabled = true;

	//DeleteZombieNavAgent(navAgent);
	//navAgent = NULL;

	_zstat_Disabled++;
	_zstat_NavFails++;
}

void obj_Zombie::AILog(int level, const char* fmt, ...)
{
	if(level > _zai_DebugAI)
		return;

	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	StringCbVPrintfA(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	r3dOutToLog("AIZombie%p[%d] %s", this, navAgent->m_navBot->GetVisualDebugId(), buf);
}

bool obj_Zombie::CheckNavPos(r3dPoint3D& pos)
{
	Kaim::TriangleFromPosQuery q;
	q.Initialize(gAutodeskNavMesh.GetDB(), R3D_KY(pos));
	q.PerformQuery();
	switch(q.GetResult())
	{
	default:
		r3dOutToLog("!!!! TriangleFromPosQuery returned %d\n", q.GetResult());
		return false;
	case Kaim::TRIANGLEFROMPOS_DONE_NO_TRIANGLE_FOUND:
		return false;
	case Kaim::TRIANGLEFROMPOS_DONE_TRIANGLE_FOUND:
		pos.y = q.GetAltitudeOfProjectionInTriangle();
		return true;
	}
}

void obj_Zombie::CreateNavAgent()
{
	r3d_assert(!navAgent);

	// there is no checks here, they should be done in ZombieSpawn, so pos is navmesh position
	r3dPoint3D pos = GetPosition();
	navAgent = CreateZombieNavAgent(pos);

	//AILog(3, "created at %f %f %f\n", GetPosition().x, GetPosition().y, GetPosition().z);	

	return;
}

void obj_Zombie::StopNavAgent()
{
	if(!navAgent) return;

	if(patrolPntIdx >= 0)
	{
		spawnObject->ReleaseNavPoint(patrolPntIdx);
		patrolPntIdx = -1;
	}

	navAgent->StopMove();
}


void obj_Zombie::SetNavAgentSpeed(float speed)
{
	if(!navAgent) return;

	navAgent->SetTargetSpeed(speed);
}

bool obj_Zombie::MoveNavAgent(const r3dPoint3D& pos, float maxAstarRange)
{
	AILog(4, "navigating to %f %f %f from %f %f %f\n", pos.x, pos.y, pos.z, GetPosition().x, GetPosition().y, GetPosition().z);

	if(!navAgent->StartMove(pos, maxAstarRange))
		return false;

	moveFrameCount = 0;

	moveWatchTime  = 0;
	moveWatchPos   = GetPosition();
	moveStartPos   = GetPosition();
	moveTargetPos  = pos;
	moveStartTime  = r3dGetTime();
	moveAvoidTime  = 0;
	moveAvoidPos   = GetPosition();

	SendAIStateToNet();

	return true;
}

int obj_Zombie::CheckMoveWatchdog()
{
	if(navAgent->m_status == AutodeskNavAgent::ComputingPath)
		return 1;
	if(navAgent->m_status != AutodeskNavAgent::Moving)
		return 2;
	if(staggerTime > 0)
		return 1;

	// check every 5 sec that we moved somewhere
	moveWatchTime += r3dGetFrameTime();
	if(moveWatchTime < 5)
		return 1;

	if((GetPosition() - moveWatchPos).Length() < 0.5f)
	{
		AILog(1, "!!! move watchdog at %f %f %f\n", GetPosition().x, GetPosition().y, GetPosition().z);
		//DisableZombie();
		return 0;
	}

	moveWatchTime = 0;
	moveWatchPos  = GetPosition();
	return 1;
}

int obj_Zombie::CheckMoveStatus()
{
	float curTime = r3dGetTime();

	switch(navAgent->m_status)
	{
	case AutodeskNavAgent::Idle:
		return 2;

	case AutodeskNavAgent::ComputingPath:
		if(curTime > moveStartTime + 5.0f)
		{
			AILog(0, "!!! ComputingPath for %f\n", curTime - moveStartTime);
			_zstat_NavFails++;
			StopNavAgent();
			return 2;
		}
		return 1;

	case AutodeskNavAgent::PathNotFound:
		AILog(5, "PATH_NOT_FOUND %d to %f,%f,%f from %f,%f,%f\n", 
			navAgent->m_pathFinderQuery->GetResult(),
			moveTargetPos.x, moveTargetPos.z, moveTargetPos.y,
			moveStartPos.x, moveStartPos.z, moveStartPos.y);

		StopNavAgent();
		return 2;

	case AutodeskNavAgent::Moving:
		// break to next check for path status
		break;

	case AutodeskNavAgent::Arrived:
		return 0;

	case AutodeskNavAgent::Failed:
		AILog(5, "!!! Failed with %d\n", navAgent->m_navBot->GetTargetOnLivePathStatus());
		return 2;
	}

	Kaim::TargetOnPathStatus status = navAgent->m_navBot->GetTargetOnLivePathStatus();
	switch(status)
	{
	case Kaim::TargetOnPathNotInitialized:
	case Kaim::TargetOnPathUnknownReachability:
	case Kaim::TargetOnPathInInvalidNavData:
		if(curTime > moveStartTime + 5.0f)
		{
			AILog(0, "!!! GetPathStatus:%d for %f\n", status, curTime - moveStartTime); //@
			_zstat_NavFails++;
			StopNavAgent();
			return 2;
		}
		return 1;

	case Kaim::TargetOnPathNotReachable:
		return 2;
	}

	return 1;
}

GameObject* obj_Zombie::FindBarricade()
{
	for(std::vector<obj_ServerBarricade*>::iterator it = obj_ServerBarricade::allBarricades.begin(); it != obj_ServerBarricade::allBarricades.end(); ++it)
	{
		obj_ServerBarricade* shield = *it;

		// fast discard by radius
		if((GetPosition() - shield->GetPosition()).Length() > shield->m_Radius + _zai_AttackRadius)
			continue;

		// get obstacle, TODO: rework to point vs OBB logic
		Kaim::WorldElement* e = gAutodeskNavMesh.obstacles[shield->m_ObstacleId];
		r3d_assert(e);
		if(e->GetType() != Kaim::TypeBoxObstacle)
			continue;
		Kaim::BoxObstacle* boxObstacle = static_cast<Kaim::BoxObstacle*>(e);

		// search for every spatial cylinder there
		for(KyUInt32 cidx = 0; cidx < boxObstacle->GetSpatializedCylinderCount(); cidx++)
		{
			const Kaim::SpatializedCylinder& cyl = boxObstacle->GetSpatializedCylinder(cidx);
			r3dPoint3D p1 = r3dPoint3D(GetPosition().x, 0, GetPosition().z);
			r3dPoint3D p2 = r3dPoint3D(cyl.GetPosition().x, 0, cyl.GetPosition().y); // KY_R3D
			float dist = (p1 - p2).Length() - cyl.GetRadius();
			if(dist < _zai_AttackRadius * 0.7f)
			{
				return shield;
			}
		}
	}
	return NULL;
}

bool obj_Zombie::CheckForBarricadeBlock()
{
	// we detect barricade by checking for them every sec if we're in avoidance mode.
	Kaim::IAvoidanceComputer::AvoidanceResult ares = navAgent->m_navBot->GetTrajectory()->GetAvoidanceResult();
	if(ares == Kaim::IAvoidanceComputer::NoAvoidance)
	{
		moveAvoidTime = 0;
		moveAvoidPos  = GetPosition();
		return false;
	}

	moveAvoidTime += r3dGetFrameTime();

	float avoidTimeCheck = 1.0f;
	float minDistToMove  = WalkSpeed * avoidTimeCheck * 0.8f;
	//r3dOutToLog("%f %f vs %f\n", moveAvoidTime, (GetPosition() - moveAvoidPos).Length(), minDistToMove);
	if(moveAvoidTime >= avoidTimeCheck)
	{
		float dist = (GetPosition() - moveAvoidPos).Length();
		moveAvoidTime = 0;
		moveAvoidPos  = GetPosition();

		if(dist >= minDistToMove)
			return false;

		GameObject* shield = FindBarricade();
		if(!shield)
			return false;

		StopNavAgent();

		// found barricade, wreck it!
		hardObjLock = shield->GetSafeID();
		////////////////////////////////////////
		//Codex Super Zombie
		if (HeroItemID == 20207)
		attackTimer = _zai_SZAttackTimer / 2;
		else
		////////////////////////////////////////
		attackTimer = _zai_AttackTimer / 2;
		SwitchToState(EZombieStates::ZState_BarricadeAttack);
		return true;
	}

	return false;
}


void obj_Zombie::FaceVector(const r3dPoint3D& v)
{
	float angle = atan2f(-v.z, v.x);
	angle = R3D_RAD2DEG(angle);
	SetRotationVector(r3dVector(angle - 90, 0, 0));
}

GameObject* obj_Zombie::ScanForTarget(bool immidiate)
{
	if(ZombieDisabled || _zai_DisableDetect)
		return NULL;

	const float curTime = r3dGetTime();
	if(!immidiate && curTime < nextDetectTime)
		return NULL;
	nextDetectTime = curTime + 0.1f;

	return GetClosestPlayerBySenses();
}

bool obj_Zombie::IsPlayerDetectable(const obj_ServerPlayer* plr, float dist)
{
	if(plr->loadout_->Alive == 0 || plr->profile_.ProfileData.isGod || plr->loadout_->GameFlags == wiCharDataFull::GAMEFLAG_NearPostBox || plr->loadout_->GameFlags == wiCharDataFull::GAMEFLAG_isSpawnProtected)
		return false;

	// detect by smell
	if(dist < DetectRadius) {
		return true;
	}
	if(plr->loadout_->Alive == 0)
		return false;

	// detect by smell
	if(dist < DetectRadius) {
		return true;
	}

	// vision is disabled in sleep mode
	if(ZombieState == EZombieStates::ZState_Sleep)
		return false;

	// vision check: range
	const static float VisionDetectRangesByState[] = {
		4.0f, //PLAYER_IDLE = 0,
		4.0f, //PLAYER_IDLEAIM,
		7.0f, //PLAYER_MOVE_CROUCH,
		7.0f, //PLAYER_MOVE_CROUCH_AIM,
		10.0f, //PLAYER_MOVE_WALK_AIM,
		15.0f, //PLAYER_MOVE_RUN,
		30.0f, //PLAYER_MOVE_SPRINT,
		15.0f, //PLAYER_MOVE_RUN,
		15.0f, //PLAYER_MOVE_RUN,
		15.0f, //PLAYER_MOVE_RUN,
		4.0f, //PLAYER_MOVE_PRONE,
		4.0f, //PLAYER_MOVE_PRONE_AIM,
		1.8f, //PLAYER_PRONE_UP,
		1.8f, //PLAYER_PRONE_DOWN,
		1.8f, //PLAYER_PRONE_IDLE,
		0.0f, //PLAYER_DIE,
	};
	COMPILE_ASSERT( R3D_ARRAYSIZE(VisionDetectRangesByState) == PLAYER_NUM_STATES);

	if(plr->m_PlayerState < 0 || plr->m_PlayerState >= PLAYER_NUM_STATES) {
		r3dOutToLog("!!! bad state\n");
		return false;
	}
	if(dist > VisionDetectRangesByState[plr->m_PlayerState]) {
		return false;
	}

	// vision check: cone of view
	/*{
	r3dPoint3D v1(0, 0, -1);
	v1.RotateAroundY(GetRotationVector().x);
	r3dPoint3D v2 = (plr->GetPosition() - GetPosition()).NormalizeTo();

	float dot = v1.Dot(v2);
	//float deg = R3D_RAD2DEG(acosf(dot));
	if(dot < _zai_VisionConeCos)
	return false;
	}*/

	// vision check: obstacles
	if(!CheckViewToPlayer(plr))
		return false;

	return true;
}

GameObject* obj_Zombie::GetClosestPlayerBySenses()
{
	obj_ServerPlayer* found   = NULL;
	float             minDist = 9999999;

	// scan for all player
	for(int i=0; i<gServerLogic.MAX_PEERS_COUNT; i++)
	{
		obj_ServerPlayer* plr = gServerLogic.peers_[i].player;
		if(!plr)
			continue;

		float dist = (GetPosition() - plr->GetPosition()).Length();
		if(!IsPlayerDetectable(plr, dist))
			continue;

		if(dist < minDist)
		{
			minDist = dist;
			found   = plr;
		}
	}

	//if(found) r3dOutToLog("zombie%p GetClosestPlayerBySenses %s\n", this, found->userName);
	return found;
}

bool obj_Zombie::CheckViewToPlayer(const GameObject* obj)
{
	const float eyeHeight = 1.6f;

	// Issue raycast query to check visibility occluders
	r3dPoint3D dir = (obj->GetPosition() - GetPosition());
	float dist = dir.Length() - 1.0f;
	if(dist < 0)
		return true;
	dir.Normalize();

	PxVec3 porigin(GetPosition().x, GetPosition().y + eyeHeight, GetPosition().z);
	PxVec3 pdir(dir.x, dir.y, dir.z);
	PxSceneQueryFlags flags = PxSceneQueryFlag::eDISTANCE;
	PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eDYNAMIC | PxSceneQueryFilterFlag::eSTATIC));
	if(g_pPhysicsWorld->PhysXScene->raycastSingle(porigin, pdir, dist, flags, hit, filter))
	{
		/*
		AILog(20, "view obstructed\n");

		PhysicsCallbackObject* target = NULL;
		if(hit.shape && (target = static_cast<PhysicsCallbackObject*>(hit.shape->getActor().userData)))
		{
		GameObject* obj = target->isGameObject();
		if(obj)
		r3dOutToLog("obj: %s\n", obj->Name.c_str());
		}*/

		return false;
	}

	return true;
}

bool obj_Zombie::SenseWeaponFire(const obj_ServerPlayer* plr, const ServerWeapon* wpn)
{
	if(ZombieState == EZombieStates::ZState_Dead)
		return false; // no attack while dead.
	if(ZombieState == EZombieStates::ZState_Waking)
		return false; // give him time to wake!

	if(!wpn)
		return false;

	float range = 0;
	switch(wpn->getCategory())
	{
	default:
	case storecat_SNP:
		range = 15; // DNC changed from 75
		break;

	case storecat_ASR:
	case storecat_SHTG:
	case storecat_MG:
		range = 75; // DNC changed from 50
		break;

	case storecat_HG:
	case storecat_SMG:
		range = 50; // DNC changed from 30
		break;

	case storecat_MELEE:
		range = 15; // sergey's design.
		break;
	}

	// silencer halves range
	if(wpn->m_Attachments[WPN_ATTM_MUZZLE] && wpn->m_Attachments[WPN_ATTM_MUZZLE]->m_itemID == 400013)
	{
		range *= 0.8f;  // DNC changed silence range from range *= 0.5f;
	}

	// override for .50 cal - big range, no silencer
	if(wpn->getConfig()->m_itemID == 101088)
		range = 100; // DNC increased from 75

	float dist = (GetPosition() - plr->GetPosition()).Length();
	if(dist > range)
		return false;

	// if player can't be seen, check agains halved radius
	if(!CheckViewToPlayer(plr))
	{
		if(dist > range * 0.5f)
			return false;
	}

	//r3dOutToLog("zombie%p sensed weapon fire from %s\n", this, plr->userName); CLOG_INDENT;

	// check if new target is closer that current one
	const GameObject* trg = GameWorld().GetObject(hardObjLock);
	if((trg == NULL) || (trg && dist < (trg->GetPosition() - GetPosition()).Length()))
	{
		StartAttack(plr);
		return true;
	}

	// if this is same target, recalculate path if he was moved
	if(ZombieState == EZombieStates::ZState_Pursue && trg == plr && (trg->GetPosition() - lastTargetPos).Length() > _zai_DistToRecalcPath)
	{
		lastTargetPos = trg->GetPosition();
		MoveNavAgent(trg->GetPosition(), _zai_MaxPursueDistance);
	}

	return true;
}

bool obj_Zombie::StartAttack(const GameObject* trg)
{
	r3d_assert(ZombieState != EZombieStates::ZState_Dead);
	if(ZombieDisabled || ((obj_ServerPlayer*)trg)->profile_.ProfileData.isGod)
		return false;
	if(hardObjLock == trg->GetSafeID())
		return true;
	if(ZombieDisabled)
		return false;
	if(hardObjLock == trg->GetSafeID())
		return true;

	//Codex Carros
	////////////////////////////////////////////////////////////////////
	if (((obj_ServerPlayer*)trg)->PlayerOnVehicle==true) //server Vehicle
	{
		int ID = ((obj_ServerPlayer*)trg)->IDOFMyVehicle;
		GameObject* from = GameWorld().GetNetworkObject(ID);
		if (from)
			trg=from;
		_zai_AttackRadius        = 3.0f;
	}
	else
		_zai_AttackRadius        = 1.2f;
	//////////////////////////////////////////////////////////////////

	if(ZombieState == EZombieStates::ZState_Sleep)
	{
		// wake up sleeper
		SwitchToState(EZombieStates::ZState_Waking);
		return true;
	}

	StopNavAgent(); // to release current nav point from Walk state

	AILog(2, "attacking %s\n", trg->Name.c_str()); CLOG_INDENT;

	float dist = (GetPosition() - trg->GetPosition()).Length();
	if(dist > _zai_MaxPursueDistance)
		return false;

	// check if we can switch to melee immidiately
	if(dist < _zai_AttackRadius)
	{
		hardObjLock = trg->GetSafeID();

		if(ZombieState != EZombieStates::ZState_Attack)
			SwitchToState(EZombieStates::ZState_Attack);

		//////////////////////////////////////////////////////
		if (HeroItemID == 20207)
		attackTimer   = _zai_SZAttackTimer / 2;
		else
		//////////////////////////////////////////////////////
		attackTimer   = _zai_AttackTimer / 2;
		isFirstAttack = true;
		return true;
	}

	// check if zombie can get to the player within 2 radius of attack
	r3dPoint3D trgPos = trg->GetPosition();
	if(!gAutodeskNavMesh.AdjustNavPointHeight(trgPos, 1.0f))
	{
		if(!gAutodeskNavMesh.GetClosestNavMeshPoint(trgPos, 2.0f, _zai_AttackRadius * 2))
		{
			AILog(5, "player offmesh at %f %f %f\n", trgPos.x, trgPos.y, trgPos.z);
			return true;
		}

		/*if((trgPos - GetPosition()).Length() < 1.0f)
		{
			AILog(5, "player offmesh and we can't reach him\n");
			return false;
		}

		// ok, we'll reach him in end of our path
		AILog(5, "going to offmesh player to %f %f %f\n", trgPos.x, trgPos.y, trgPos.z);*/
	}

	// start pursuing immidiately
	SetNavAgentSpeed(staggerTime < 0 ? RunSpeed : 0.0f);
	if(!MoveNavAgent(trgPos, _zai_MaxPursueDistance))
		return false;

	lastTargetPos = trg->GetPosition();
	hardObjLock   = trg->GetSafeID();

	// switch to pursue mode
	if(ZombieState != EZombieStates::ZState_Pursue) 
	{
		SwitchToState(EZombieStates::ZState_Pursue);
	}

	return true;
}

void obj_Zombie::StopAttack()
{
	r3d_assert(ZombieState == EZombieStates::ZState_Attack || ZombieState == EZombieStates::ZState_BarricadeAttack);

	hardObjLock = invalidGameObjectID;

	// scan for new target immiiately
	if(GameObject* trg = ScanForTarget(true))
	{
		if(StartAttack(trg))
			return;
	}

	// start attack failed or no target to attack, switch to idle
	SwitchToState(EZombieStates::ZState_Idle);
}

BOOL obj_Zombie::Update()
{
	parent::Update();

	if(!isActive())
		return TRUE;

	const float curTime = r3dGetTime();

	if(ZombieState == EZombieStates::ZState_Dead)
	{
		// deactivate zombie after some time
		if(curTime > StateStartTime + 60.0f)
			setActiveFlag(0);
		return TRUE;
	}

	DebugSingleZombie();

	// Propagate AI agent position to zombie position
	if(navAgent)
	{
		SetPosition(navAgent->GetPosition());

		if(navAgent->m_status == AutodeskNavAgent::Moving)
		{
			Kaim::Vec3f rot  = navAgent->m_velocity;
			if(rot.GetSquareLength2d() > 0.001f)
				FaceVector(r3dPoint3D(rot[0], rot[2], rot[1]));
		}
	}
	moveFrameCount++;

	// check for stagger
	if(staggerTime > 0)
	{
		animState = 1;
		staggerTime -= r3dGetFrameTime();
		if(staggerTime <= 0.001f)
		{
			animState = 0;
			staggerTime = -1;
		}
	}

	// send network position update
	{
		CNetCellMover::moveData_s md;
		md.pos       = GetPosition();
		md.turnAngle = GetRotationVector().x;
		md.bendAngle = 0;
		md.state     = (BYTE)animState;

		PKT_C2C_MoveSetCell_s n1;
		PKT_C2C_MoveRel_s     n2;
		DWORD pktFlags = netMover.SendPosUpdate(md, &n1, &n2);
		if(pktFlags & 0x1)
			gServerLogic.p2pBroadcastToActive(this, &n1, sizeof(n1),true);
		if(pktFlags & 0x2)
			gServerLogic.p2pBroadcastToActive(this, &n2, sizeof(n2),true);
	}

	switch(ZombieState)
	{
	default:
		break;

	case EZombieStates::ZState_Sleep:
		{
			// if detected, wake zombie up, but do not set target
			if(GameObject* trg = ScanForTarget())
			{
				SwitchToState(EZombieStates::ZState_Waking);
			}
			break;
		}

	case EZombieStates::ZState_Waking:
		{
			if(curTime < StateStartTime + 3.0f) // wait for client "wake up" animation finish
				break;

			// perform immidiate surrounding check if we don't have target yet
			if(hardObjLock == invalidGameObjectID)
			{
				if(GameObject* trg = ScanForTarget(true))
				{
					StartAttack(trg);
					break;
				}
			}

			GameObject* trg = GameWorld().GetObject(hardObjLock);
			if(!trg)
			{
				// no target, switch to idle
				SwitchToState(EZombieStates::ZState_Idle);
				break;
			}
			else
			{
				StartAttack(trg);
			}
			break;
		}

	case EZombieStates::ZState_Idle:
		{
			// try to find someone to attack, do not switch to patrol if we have anyone
			if(GameObject* trg = ScanForTarget())
			{
				StartAttack(trg);
				break;
			}

			// check for idle finish	
			if(curTime < StateTimer || staggerTime > 0) 
				break;

			// do not switch to patrol if there is no players around
			bool doPatrol = u_GetRandom() < _zai_IdleStatePatrolPerc && !ZombieDisabled;
			if(doPatrol && _zai_NoPatrolPlayerDist > 0 && gServerLogic.CheckForPlayersAround(GetPosition(), _zai_NoPatrolPlayerDist) == false)
			{
				StateTimer = curTime + u_GetRandom(3, 5);
				break;
			}

			if(doPatrol)
			{
				r3dPoint3D out_pos;
				r3dPoint3D cur_pos = GetPosition();
				patrolPntIdx = spawnObject->GetFreeNavPointIdx(&out_pos, true, _zai_MaxPatrolDistance, &cur_pos);
				if(patrolPntIdx >= 0)
				{
					SetNavAgentSpeed(WalkSpeed);
					MoveNavAgent(out_pos, _zai_MaxPatrolDistance * 2);
					SwitchToState(EZombieStates::ZState_Walk);
					break;
				}
				else
				{
					// navigation failed, keep idle time
					StateTimer = r3dGetTime() + u_GetRandom(5, 60);
					AILog(5, "patrol navigation failed around pos %f %f %f\n", spawnObject->GetPosition().x, spawnObject->GetPosition().y, spawnObject->GetPosition().z);
				}
			}
			else
			{
				// continuing idle time for some more
				StateTimer = curTime + u_GetRandom(5, 60);
			}
			break;
		}

	case EZombieStates::ZState_Walk:
		{
			r3d_assert(!ZombieDisabled && navAgent);

			// try to find someone to attack
			if(GameObject* trg = ScanForTarget())
			{
				StartAttack(trg);
				break;
			}

			if(staggerTime < 0)
				SetNavAgentSpeed(WalkSpeed);

			// check if we have barricade around
			if(CheckForBarricadeBlock())
			{
				break;
			}

			switch(CheckMoveStatus())
			{
			default: r3d_assert(false);
			case 0: // completed
			case 2: // failed
				SwitchToState(EZombieStates::ZState_Idle);
				break;
			case 1: // in progress
				if(!CheckMoveWatchdog())
				{
					SwitchToState(EZombieStates::ZState_Idle);
				}
				break;
			}

			break;
		}

	case EZombieStates::ZState_Pursue:
		{
			r3d_assert(!ZombieDisabled && navAgent);

			GameObject* trg = GameWorld().GetObject(hardObjLock);
			if(trg)
			{
				// check if we're within melee range
				float dist = (trg->GetPosition() - GetPosition()).Length();
				if(dist < _zai_AttackRadius && staggerTime < 0)
				{
					StopNavAgent();
					SwitchToState(EZombieStates::ZState_Attack);
					///////////////////////////////////////////////
					//Codex Super Zombie
					if (HeroItemID == 20207)
					attackTimer   = _zai_SZAttackTimer / 2;
					else
					///////////////////////////////////////////////
					attackTimer   = _zai_AttackTimer / 2;
					isFirstAttack = true;
					break;
				}
			}

			if(staggerTime < 0)
				SetNavAgentSpeed(RunSpeed);

			// check if we still have target in our visibility
			if(GameObject* trg = ScanForTarget())
			{
				if(trg->GetSafeID() != hardObjLock)
				{
					// new target, switch to him
					StartAttack(trg);
					break;
				}

				// if player went off mesh - do nothing, continue what we was doing
				if(!gAutodeskNavMesh.IsNavPointValid(trg->GetPosition()))
				{
					break;
				}

				// recalculate paths sometime
				if((trg->GetPosition() - lastTargetPos).Length() > _zai_DistToRecalcPath)
				{
					lastTargetPos = trg->GetPosition();
					MoveNavAgent(trg->GetPosition(), _zai_MaxPursueDistance);
				}
			}

			// check if we have barricade around
			if(CheckForBarricadeBlock())
			{
				break;
			}

			switch(CheckMoveStatus())
			{
			default: r3d_assert(false);
			case 0: // completed
			case 2: // failed
				hardObjLock = invalidGameObjectID;
				SwitchToState(EZombieStates::ZState_Idle);
				break;
			case 1: // in progress
				break;
			}

			break;
		}

	case EZombieStates::ZState_Attack:
		{
			//Codex Carros
            /////////////////////////////////////////////////////////////////////////////////////////
			GameObject* from = GameWorld().GetObject(hardObjLock); // Server Vehicles
			if (from && from->Class->Name == "obj_Vehicle")
			{
				obj_Vehicle* Vehicle= static_cast< obj_Vehicle* > ( from );
				if (Vehicle && Vehicle->OccupantsVehicle>0)
				{
					float dist = (Vehicle->GetPosition() - GetPosition()).Length();
					if (dist>6)
					{
						StopAttack();
						break;
					}

					PKT_C2S_DamageCar_s n2;
					n2.WeaponID = 9999999;
					n2.CarID = Vehicle->GetNetworkID();
					gServerLogic.p2pBroadcastToAll(NULL, &n2, sizeof(n2), true);
					break;
				}
			}
			/////////////////////////////////////////////////////////////////////////////////////////




			obj_ServerPlayer* trg = IsServerPlayer(GameWorld().GetObject(hardObjLock));
			if(!trg || trg->loadout_->Alive == 0)
			{
				StopAttack();
				break;
			}

			FaceVector(trg->GetPosition() - GetPosition());

			if(staggerTime > 0)
			{
				attackTimer   = 0;
				isFirstAttack = false;
				break;
			}

			attackTimer += r3dGetFrameTime();
			if(attackTimer >= _zai_AttackTimer)
			{
				attackTimer = 0;

				// first attack always land
				bool canAttack = true;
				if(!isFirstAttack)
				{
					float dist = (trg->GetPosition() - GetPosition()).Length();
					if(dist > _zai_AttackRadius * 1.1f || !CheckViewToPlayer(trg))
						canAttack = false;
				}

				if(!canAttack)
				{
					StopAttack();
					break;
				}
				else
				{
					if (trg->loadout_->GameFlags == wiCharDataFull::GAMEFLAG_isSpawnProtected || trg->loadout_->GameFlags == wiCharDataFull::GAMEFLAG_NearPostBox)
					{
					}
					else
					{
						isFirstAttack = false;

						PKT_S2C_ZombieAttack_s n;
						n.targetId = toP2pNetId(trg->GetNetworkID());
						gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n),true);

						// temp code for applying damage from zombie to player
						if (trg->profile_.ProfileData.isPunisher || trg->profile_.ProfileData.isDevAccount)
						{
							////////////////////////////////////////
							//Codex Super Zombie
							if (HeroItemID == 20207)
							{
                               _zai_SZAttackDamage = 58.0f;
							}
							else
							{
							/////////////////////////////////////
							   _zai_AttackDamage = 15.0f;
							}
						}

						////////////////////////////////////////////
						//Codex Super Zombie
						if (HeroItemID == 20207)
						 {
                               trg->loadout_->Health -= _zai_SZAttackDamage;
						 }
						else
						 {
						////////////////////////////////////////////
							   trg->loadout_->Health -= _zai_AttackDamage;
						 }
				

						if (!trg->loadout_->bleeding)
						{
							trg->loadout_->bleeding = u_GetRandom() >= 0.20f ? false : true;
						}

                        ///////////////////////////////////////////////////////////////////////
						//Codex Super Zombie
						if(HeroItemID == 20207)
						{
							if(u_GetRandom(0.0f, 100.0f) < 20.0f) // chance of infecting
							{
								trg->loadout_->Toxic += 1.0f;
							}
						}else{
                        ////////////////////////////////////////////////////////////////////////
							if(u_GetRandom(0.0f, 100.0f) < 7.0f) // chance of infecting
							{
								trg->loadout_->Toxic += 1.0f;
							}
						}

						if(trg->loadout_->Health <= 0)
						{
							trg->loadout_->Health = 0;
							gServerLogic.DoKillPlayer(this, trg, storecat_MELEE);

							StopAttack();
							break;
						}
					}
				}
			}
			break;
		}

	case EZombieStates::ZState_BarricadeAttack:
		{
			obj_ServerBarricade* shield = (obj_ServerBarricade*)GameWorld().GetObject(hardObjLock);
			if(!shield)
			{
				StopAttack();
				break;
			}

			// check if we have player in melee range
			if(GameObject* trg = ScanForTarget())
			{
				if((trg->GetPosition() - GetPosition()).Length() < _zai_AttackRadius)
				{
					StartAttack(trg);
					break;
				}
			}

			FaceVector(shield->GetPosition() - GetPosition());

			if(staggerTime > 0)
			{
				attackTimer   = 0;
				break;
			}

			attackTimer += r3dGetFrameTime();
		    ////////////////////////////////////////
			//Codex Super Zombie
			if (HeroItemID == 20207)
			{
			if(attackTimer >= _zai_SZAttackTimer)
			{
				attackTimer = 0;
				shield->DoDamage(_zai_SZAttackDamage);
			}
			break;      
			}
			else
			{
			/////////////////////////////////////
			if(attackTimer >= _zai_AttackTimer)
			{
				attackTimer = 0;
				shield->DoDamage(_zai_AttackDamage);
			}
		     	break;      
			}
			
		}

	case EZombieStates::ZState_Dead:
		break;
	}

	return TRUE;
}

DefaultPacket* obj_Zombie::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateZombie_s n;
	n.spawnID    = toP2pNetId(GetNetworkID());
	n.spawnPos   = GetPosition();
	n.spawnDir   = GetRotationVector().x;
	n.moveCell   = netMover.SrvGetCell();
	n.HeroItemID = HeroItemID;
	n.HeadIdx    = (BYTE)HeadIdx;
	n.BodyIdx    = (BYTE)BodyIdx;
	n.LegsIdx    = (BYTE)LegsIdx;
	n.State      = (BYTE)ZombieState;
	n.FastZombie = (BYTE)FastZombie;
	n.WalkSpeed  = WalkSpeed;
	n.RunSpeed   = RunSpeed;
	//if(HalloweenZombie) n.HeroItemID += 1000000;

	*out_size = sizeof(n);
	return &n;
}

void obj_Zombie::SendAIStateToNet()
{
	if(!_zai_DebugAI)
		return;

	if(navAgent->m_status == AutodeskNavAgent::Moving)
	{
		PKT_S2C_Zombie_DBG_AIInfo_s n;
		n.from = moveStartPos;
		n.to   = moveTargetPos;
		gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n),true);
	}
}

void obj_Zombie::DebugSingleZombie()
{
	if(!_zai_DebugAI)
		return;

	static KyUInt32 debugVisualId = 0;
	FILE* f = fopen("zdebug.txt", "rt");
	if(!f) return;
	fscanf(f, "%d", &debugVisualId);
	fclose(f);

	if(navAgent->m_navBot->GetVisualDebugId() != debugVisualId)
		return;

	if(ZombieDisabled)
	{
		AILog(0, "zombie disabled\n");
		return;
	}

	Kaim::Bot* m_navBot = navAgent->m_navBot;
	Kaim::AStarQuery<Kaim::AStarCustomizer_Default>* m_pathFinderQuery = navAgent->m_pathFinderQuery;
	AILog(0, "state: %d, time: %f\n", ZombieState, r3dGetTime() - StateStartTime);
	AILog(0, "GetTargetOnLivePathStatus(): %d\n", m_navBot->GetTargetOnLivePathStatus());
	AILog(0, "GetPathValidityStatus(): %d\n", m_navBot->GetLivePath().GetPathValidityStatus());
	AILog(0, "GetPathFinderResult(): %d %d\n", m_pathFinderQuery->GetPathFinderResult(), m_pathFinderQuery->GetResult());
	if(m_navBot->GetPathFinderQuery())
		AILog(0, "m_processStatus: %d\n", m_navBot->GetPathFinderQuery()->m_processStatus);
}

BOOL obj_Zombie::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	switch(EventID)
	{
	case PKT_C2S_Zombie_DBG_AIReq:
		SendAIStateToNet();
		break;
	}

	return TRUE;
}

void obj_Zombie::SwitchToState(int in_state)
{
	r3d_assert(ZombieState != EZombieStates::ZState_Dead); // death should be final
	r3d_assert(ZombieState != in_state);
	ZombieState    = in_state;
	StateStartTime = r3dGetTime();

	// duration of idle state
	if(in_state == EZombieStates::ZState_Idle)
	{
		StopNavAgent(); // to release current nav point from Walk state
		hardObjLock = invalidGameObjectID;
		StateTimer  = r3dGetTime() + u_GetRandom(3, 10);
	}

	//r3dOutToLog("zombie%p SwitchToState %d\n", this, ZombieState); CLOG_INDENT;

	PKT_S2C_ZombieSetState_s n;
	n.State    = (BYTE)ZombieState;
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n),true);
}

void obj_Zombie::DoDeath()
{
	extern wiInventoryItem RollItem(const LootBoxConfig* lootCfg, int depth);

	// drop loot
	if(spawnObject->lootBoxCfg && HeroItemID != 20207) //Codex Super Zombie
	{
		wiInventoryItem wi = RollItem(spawnObject->lootBoxCfg, 0);
		if(wi.itemID > 0)
		{
			// create random position around zombie
			r3dPoint3D pos = GetPosition();
			pos.y += 0.4f;
			pos.x += u_GetRandom(-1, 1);
			pos.z += u_GetRandom(-1, 1);

			// create network object
			obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", pos);
			obj->SetNetworkID(gServerLogic.GetFreeNetId());
			obj->NetworkLocal = true;
			// vars
			obj->m_Item       = wi;
		}
	}


if(HeroItemID == 20207)
	{
		for(int i=0; i<10; i++)
		{
			if(spawnObject->lootBoxCfg)
			{
				wiInventoryItem wi = RollItem(spawnObject->lootBoxCfg, 0);
				if(wi.itemID > 0)
				{
					// create random position around zombie
					r3dPoint3D pos = GetPosition();
					pos.y += 0.4f;
					pos.x += u_GetRandom(-1, 1);
					pos.z += u_GetRandom(-1, 1);

					// create network object
					obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", pos);
					obj->SetNetworkID(gServerLogic.GetFreeNetId());
					obj->NetworkLocal = true;
					// vars
					obj->m_Item       = wi;
				}
			}

		}
	}

	if(HalloweenZombie && u_GetRandom() < 0.3f) // 30% to drop that helmet
	{

		ZombieHealth = u_GetRandom(150.0f, 155.0f);
		r3dOutToLog("Geared zombie spawned\n");

		// create random position around zombie
		r3dPoint3D pos = GetPosition();
		pos.y += 0.4f;
		pos.x += u_GetRandom(-1, 1);
		pos.z += u_GetRandom(-1, 1);

		// create network object
		obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", pos);
		obj->SetNetworkID(gServerLogic.GetFreeNetId());
		obj->NetworkLocal = true;
		// vars
		obj->m_Item.itemID   = 20048; // M9 Helmet Urban
		obj->m_Item.quantity = 1;
	}

	// remove from active zombies, but keep object - so it'll be visible for some time on all clients
	StopNavAgent();

	DeleteZombieNavAgent(navAgent);
	navAgent = NULL;

	spawnObject->OnZombieKilled(this);

	SwitchToState(EZombieStates::ZState_Dead);
}

bool obj_Zombie::ApplyDamage(GameObject* fromObj, float damage, int bodyPart, STORE_CATEGORIES damageSource,bool isSpecialbool)
{
	if(ZombieState == EZombieStates::ZState_Dead)
		return false;

	float dmg = damage;

	if (damageSource == storecat_Vehicle || damageSource == storecat_ShootVehicle) // Server Vehicles //Codex Carros
		  ZombieHealth= 0.0f;


	if(damageSource != storecat_MELEE && bodyPart == 1 && damageSource != storecat_punch)//Codex Soco // everything except for melee: one shot in head = kill 
		dmg = 1000; 

	if (HeroItemID == 20207 && damageSource != storecat_MELEE && bodyPart == 1)
	{
		dmg = u_GetRandom(20.0f, 30.0f);
	}

	if (HeroItemID == 20207 && damageSource == storecat_MELEE && bodyPart == 1)
	{
		dmg = u_GetRandom(20.0f, 25.0f);
	}

	if (HeroItemID == 20207 && damageSource == storecat_SNP && bodyPart == 1)
	{
		dmg = u_GetRandom(20.0f, 45.0f);
	}

	if (HeroItemID == 20207 && damageSource == storecat_SHTG && bodyPart == 1)
	{
		dmg = u_GetRandom(30.0f, 50.0f);
	}

	if (!isSpecialbool)
	{
		if(bodyPart!=1) // only hitting head will lower zombie's health
			dmg = 0;
	}

	if(HeroItemID == 20207 && bodyPart!=1) // only hitting head will lower zombie's health
		dmg = 0;

	//////////////////////////////////////////
	//Codex Super Zombie
	if (HeroItemID == 20207)
	    ZombieHealth -= dmg*15/100;
	else
	//////////////////////////////////////////
	    ZombieHealth -= dmg;

	if(ZombieHealth <= 0.0f)
	{
		DoDeath();

		//Codex Carros
		////////////////////////////////////////////////////////////////////////////////////////////
		/*if(fromObj->Class->Name == "obj_ServerPlayer" && damageSource != storecat_ShootVehicle)
		{
			obj_ServerPlayer* plr = (obj_ServerPlayer*)fromObj;
			gServerLogic.AddPlayerReward(plr, RWD_ZombieKill);
		}*/
		/////////////////////////////////////////////////////////////////////////////////////////////

		if(fromObj->Class->Name == "obj_ServerPlayer" && damageSource != storecat_ShootVehicle)
		{
			obj_ServerPlayer* plr = (obj_ServerPlayer*)fromObj;
			if(plr->profile_.ProfileData.isPremium)
			{
				gServerLogic.AddPlayerReward(plr, RWD_ZombieKillP,0);
			}
			else
			{
				gServerLogic.AddPlayerReward(plr, RWD_ZombieKill,0);
			}

			if (plr->loadout_->Mission1 == 2)
			{
				plr->loadout_->Mission1ZKill += 1;
				PKT_C2S_PlayerSetObStatus_s n;
				n.id = 1;
				n.ob = 1;
				n.status = plr->loadout_->Mission1ZKill;
				gServerLogic.p2pSendToPeer(plr->peerId_, plr, &n, sizeof(n));
			}
		}

		return true;
	}

	// stagger code
	if(staggerTime < 0)
	{
		if(ZombieState != EZombieStates::ZState_Sleep && ZombieState != EZombieStates::ZState_Waking)
		{
			SetNavAgentSpeed(0.0);
			staggerTime = 1.0f;
		}
	}

	// waking zombies can't be switch to attack
	if(ZombieState == EZombieStates::ZState_Waking)
		return false;

	// direct hit, switch to that player anyway if it is new or closer that current one
	float distSq = (fromObj->GetPosition() - GetPosition()).LengthSq();
	const GameObject* trg = GameWorld().GetObject(hardObjLock);
	if((trg == NULL) || (trg && distSq < (trg->GetPosition() - GetPosition()).LengthSq()))
	{
		StartAttack(fromObj);
	}

	return false; // false as zombie wasn't killed
}