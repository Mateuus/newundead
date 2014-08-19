#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"
#include "ObjectsCode/Weapons/WeaponArmory.h"
#include "ObjectsCode/Weapons/HeroConfig.h"

#include "sobj_Animals.h"
#include "sobj_AnimalSpawn.h"
#include "ObjectsCode/obj_ServerPlayer.h"
#include "ObjectsCode/sobj_DroppedItem.h"
#include "ServerWeapons/ServerWeapon.h"

#include "../../EclipseStudio/Sources/ObjectsCode/Gameplay\AnimalStates.h"

#include "../../GameEngine/ai/AutodeskNav/AutodeskNavMesh.h"

IMPLEMENT_CLASS(obj_Animal, "obj_Animal", "Object");
AUTOREGISTER_CLASS(obj_Animal);

#if 0 //@ 
	//DEBUG VARIABLES
	int		_zai_DisableDetect       = 1;
	float		_zai_IdleStatePatrolPerc = 1.0f;
	float		_zai_NoPatrolPlayerDist  = -1.0f;
	float		_zai_PlayerSpawnProtect  = 0.0f;
	float		_zai_MaxSpawnDelay       = 0.1f;
	//float		_zai_AttackDamage        = 0.0f;
	int		_zai_DebugAI             = 1;
#else
	int		_ai_DisableDetect       = 0;
	float		_ai_IdleStatePatrolPerc = 0.4f;
	float		_ai_NoPatrolPlayerDist  = 500.0;
	float		_ai_PlayerSpawnProtect  = 35.0f;	// radius where zombie won't be spawned because of player presense
	float		_ai_MaxSpawnDelay       = 9999.0f;
	//float		_ai_AttackDamage        = 23.0f;
	int		_ai_DebugAI             = 0;
#endif
	float		_ai_MaxPatrolDistance   = 30.0f;	// distance to search for navpoints when switchint to patrol
	float		_ai_MaxPursueDistance   = 100.0f;
	//float		_ai_AttackRadius        = 1.2f;
	//float		_ai_AttackTimer         = 1.2f;
	float		_ai_DistToRecalcPath    = 0.8f; // _zai_AttackRadius / 2
	float		_ai_VisionConeCos       = cosf(50.0f); // have 100 degree vision
	
	int		_stat_NumZombies = 0;
	int		_stat_NavFails   = 0;
	int		_stat_Disabled   = 0;

obj_Animal::obj_Animal() : 
	netMover(this, 1.0f / 10.0f, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
{
	_stat_NumZombies++;

	ZombieDisabled = 0;
	
	spawnObject    = NULL;
	RunSpeed       = -1;
	WalkSpeed      = -1;
	SprintJumpSpeed = 5;
	DetectRadius   = -1;

	AnimalState    = EAnimalStates::ZState_Idle;
	StateStartTime = r3dGetTime();
	SprintTime = r3dGetTime();
	SprintJumpTime = r3dGetTime();
	StateTimer     = -1;
	nextDetectTime = r3dGetTime() + 2.0f;
	
	navAgent       = NULL;
	patrolPntIdx   = -1;
	moveFrameCount = 0;
	
	staggerTime    = -1;
	animState      = 0;
	AnimalHealth   = 100;
	SprintStart=0;
	SprintJumpStart=0;
}

obj_Animal::~obj_Animal()
{
	_stat_NumZombies--;
	if(ZombieDisabled)
		_stat_Disabled--;
}

BOOL obj_Animal::OnCreate()
{
	//r3dOutToLog("obj_Animal %p %s created\n", this, Name.c_str()); CLOG_INDENT;
	
	ObjTypeFlags |= OBJTYPE_Animal;
	
	r3d_assert(NetworkLocal);
	r3d_assert(GetNetworkID());
	r3d_assert(spawnObject);

	if(spawnObject->ZombieSpawnSelection.size() == 0)
		HeroItemID = u_GetRandom() >= 0.5f ? 20170 : 20183; // old behaviour. TODO: remove
	else
	{
		uint32_t idx1 = u_random(spawnObject->ZombieSpawnSelection.size());
		r3d_assert(idx1 < spawnObject->ZombieSpawnSelection.size());
		HeroItemID = spawnObject->ZombieSpawnSelection[idx1];
	}
	const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(HeroItemID);
	if(!heroConfig) { 
		r3dOutToLog("!!!! unable to spawn zombie - there is no hero config %d\n", HeroItemID);
		return FALSE;
	}
	
	FastZombie = u_GetRandom() > spawnObject->fastZombieChance;

	HalloweenZombie = false; //NO more special zombies u_GetRandom() < (1.0f / 100) ? true : false; // every 100th zombie is special
	if(HalloweenZombie) FastZombie = 1;

	HeadIdx = u_random(heroConfig->getNumHeads());
	BodyIdx = u_random(heroConfig->getNumBodys());
	LegsIdx = u_random(heroConfig->getNumLegs());

	WalkSpeed  = FastZombie ? 1.0f : 1.8f;
	RunSpeed   = FastZombie ? 2.9f : 3.2f;
	
	WalkSpeed += WalkSpeed * u_GetRandom(-spawnObject->speedVariation, +spawnObject->speedVariation);
	RunSpeed  += RunSpeed  * u_GetRandom(-spawnObject->speedVariation, +spawnObject->speedVariation);
	
	r3d_assert(WalkSpeed > 0);
	r3d_assert(RunSpeed > 0);
	r3d_assert(DetectRadius >= 0);
	
	// need to create nav agent so it will be placed to navmesh position
	CreateNavAgent();
	nextWalkTime = r3dGetTime()+10.0f;

	gServerLogic.NetRegisterObjectToPeers(this);
	
	return parent::OnCreate();
}

BOOL obj_Animal::OnDestroy()
{
	//r3dOutToLog("obj_Animal %p destroyed\n", this);

	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));
	
	DeleteZombieNavAgent(navAgent);
	navAgent = NULL;
	
	return parent::OnDestroy();
}

void obj_Animal::DisableZombie()
{
	AILog(0, "DisableZombie\n");
	StopNavAgent();

	ZombieDisabled = true;

	//DeleteZombieNavAgent(navAgent);
	//navAgent = NULL;

	_stat_Disabled++;
	_stat_NavFails++;
}

void obj_Animal::AILog(int level, const char* fmt, ...)
{
	if(level > _ai_DebugAI)
		return;
		
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	StringCbVPrintfA(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	r3dOutToLog("AIZombie%p[%d] %s", this, navAgent->m_navBot->GetVisualDebugId(), buf);
}

bool obj_Animal::CheckNavPos(r3dPoint3D& pos)
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

void obj_Animal::CreateNavAgent()
{
	r3d_assert(!navAgent);

	// there is no checks here, they should be done in ZombieSpawn, so pos is navmesh position
	r3dPoint3D pos = GetPosition();
	navAgent = CreateZombieNavAgent(pos);

	//AILog(3, "created at %f %f %f\n", GetPosition().x, GetPosition().y, GetPosition().z);	
	
	return;
}

void obj_Animal::StopNavAgent()
{
	if(!navAgent) return;
		
	if(patrolPntIdx >= 0)
	{
		spawnObject->ReleaseNavPoint(patrolPntIdx);
		patrolPntIdx = -1;
	}
	
	navAgent->StopMove();
}


void obj_Animal::SetNavAgentSpeed(float speed)
{
	if(!navAgent) return;

	navAgent->SetTargetSpeed(speed);
}

bool obj_Animal::MoveNavAgent(const r3dPoint3D& pos, float maxAstarRange)
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

int obj_Animal::CheckMoveWatchdog()
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

int obj_Animal::CheckMoveStatus()
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
				_stat_NavFails++;
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
				_stat_NavFails++;
				StopNavAgent();
				return 2;
			}
			return 1;

		case Kaim::TargetOnPathNotReachable:
			return 2;
	}

	return 1;
}


void obj_Animal::FaceVector(const r3dPoint3D& v)
{
	float angle = atan2f(-v.z, v.x);
	angle = R3D_RAD2DEG(angle);
	SetRotationVector(r3dVector(angle - 90, 0, 0));
}

GameObject* obj_Animal::ScanForTarget(bool immidiate)
{
	if(ZombieDisabled || _ai_DisableDetect)
		return NULL;
		
	const float curTime = r3dGetTime();
	if(!immidiate && curTime < nextDetectTime)
		return NULL;
	nextDetectTime = curTime + 0.1f;
	
	return GetClosestPlayerBySenses();
}

bool obj_Animal::IsPlayerDetectable(const obj_ServerPlayer* plr, float dist)
{
	 if(plr->loadout_->Alive == 0 || (plr->loadout_->GameFlags & wiCharDataFull::GAMEFLAG_isSpawnProtected) || plr->profile_.ProfileData.isGod)
        return false;
		
	// detect by smell
	if(dist < DetectRadius) {
		return true;
	}

	// vision check: range
	const static float VisionDetectRangesByState[] = {
		 4.0f, //PLAYER_IDLE = 0,
		 4.0f, //PLAYER_IDLEAIM,
		 7.0f, //PLAYER_MOVE_CROUCH,
		 7.0f, //PLAYER_MOVE_CROUCH_AIM,
		10.0f, //PLAYER_MOVE_WALK_AIM,
		15.0f, //PLAYER_MOVE_RUN,
		30.0f, //PLAYER_MOVE_SPRINT,
		 4.0f, //PLAYER_MOVE_PRONE,
		 4.0f, //PLAYER_MOVE_PRONE_AIM,
		 1.8f, //PLAYER_PRONE_UP,
		 1.8f, //PLAYER_PRONE_DOWN,
		 1.8f, //PLAYER_PRONE_IDLE,
		 0.0f, //PLAYER_DIE,
	};
//	COMPILE_ASSERT( R3D_ARRAYSIZE(VisionDetectRangesByState) == PLAYER_NUM_STATES);
	
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

GameObject* obj_Animal::GetClosestPlayerBySenses()
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
	
	if(found) r3dOutToLog("Animal%p GetClosestPlayerBySenses %s\n", this, found->userName);
	return found;
}

bool obj_Animal::CheckViewToPlayer(const GameObject* obj)
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

bool obj_Animal::SenseWeaponFire(const obj_ServerPlayer* plr, const ServerWeapon* wpn)
{
	if(AnimalState == EAnimalStates::ZState_Dead)
		return false; // no attack while dead.
		
	if(!wpn)
		return false;

	float range = 0;
	switch(wpn->getCategory())
	{
		default:
		case storecat_SNP:
			range = 75;
			break;

		case storecat_ASR:
		case storecat_SHTG:
		case storecat_MG:
			range = 50;
			break;

		case storecat_HG:
		case storecat_SMG:
			range = 30;
			break;
			
		case storecat_MELEE:
			range = 15; // sergey's design.
			break;
	}

	
	// override for .50 cal - big range, no silencer
	if(wpn->getConfig()->m_itemID == 101088)
		range = 75;

	float dist = (GetPosition() - plr->GetPosition()).Length();
	if(dist > range)
		return false;
	
	// if player can't be seen, check agains halved radius
	if(!CheckViewToPlayer(plr))
	{
		if(dist > range * 0.5f)
			return false;
	}

	//r3dOutToLog("Animal%p sensed weapon fire from %s\n", this, plr->userName); CLOG_INDENT;

	// if this is same target, recalculate path if he was moved
	// switch to pursue mode
	//if(AnimalState != EAnimalStates::AState_SprintJump)// || AnimalState == EAnimalStates::AState_Sprint) 
	//{
	//	SwitchToState(EAnimalStates::AState_SprintJump);
	//}
	const GameObject* trg = GameWorld().GetObject(hardObjLock);
	if((trg == NULL) || (trg && dist < (trg->GetPosition() - GetPosition()).Length()))
	{
		if(AnimalState != EAnimalStates::AState_SprintJump)
		{
		   SwitchToState(EAnimalStates::AState_SprintJump);
		   return true;
		}
	}
	if(AnimalState == EAnimalStates::AState_SprintJump && trg == plr && (trg->GetPosition() - lastTargetPos).Length() > _ai_DistToRecalcPath)
	{
		//lastTargetPos = trg->GetPosition();
		//MoveNavAgent(trg->GetPosition(), _ai_MaxPursueDistance);
		SwitchToState(EAnimalStates::ZState_Idle);
	}
	
	return true;
}


BOOL obj_Animal::Update()
{
	parent::Update();
	
	if(!isActive())
		return TRUE;

	const float curTime = r3dGetTime();
	
	if(AnimalState == EAnimalStates::ZState_Dead)
	{
		// deactivate zombie after some time
		if(curTime > StateStartTime + 60.0f)
			setActiveFlag(0);
		return TRUE;
	}
	

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
			gServerLogic.p2pBroadcastToActive(this, &n1, sizeof(n1));
		if(pktFlags & 0x2)
			gServerLogic.p2pBroadcastToActive(this, &n2, sizeof(n2));
	}
	
	switch(AnimalState)
	{
		default:
			break;
			
		case EAnimalStates::ZState_Idle:
		{
			if(GameObject* trg = ScanForTarget())
			{
				SwitchToState(EAnimalStates::AState_Sprint);
				break;
			}
			
			// check for idle finish	
			if(curTime < StateTimer || staggerTime > 0) 
				break;

			
			// do not switch to patrol if there is no players around
			bool doPatrol = u_GetRandom() < _ai_IdleStatePatrolPerc && !ZombieDisabled;
			if(doPatrol && _ai_NoPatrolPlayerDist > 0 && gServerLogic.CheckForPlayersAround(GetPosition(), _ai_NoPatrolPlayerDist) == false)
			{
				StateTimer = curTime + u_GetRandom(0, 2);
				break;
			}
			
			if(doPatrol)
			{
				r3dPoint3D out_pos;
				r3dPoint3D cur_pos = GetPosition();
				patrolPntIdx = spawnObject->GetFreeNavPointIdx(&out_pos, true, _ai_MaxPatrolDistance, &cur_pos);
				if(patrolPntIdx >= 0)
				{
					SetNavAgentSpeed(WalkSpeed);
					MoveNavAgent(out_pos, _ai_MaxPatrolDistance * 2);
					SwitchToState(EAnimalStates::ZState_Walk);
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
		case EAnimalStates::AState_IdleEating:
		{
			if(GameObject* trg = ScanForTarget())
			{
				SwitchToState(EAnimalStates::AState_Sprint);
				break;
			}
			
			// check for idle finish	
			if(curTime < StateTimer || staggerTime > 0) 
				break;

			
			// do not switch to patrol if there is no players around
			bool doPatrol = u_GetRandom() < _ai_IdleStatePatrolPerc && !ZombieDisabled;
			if(doPatrol && _ai_NoPatrolPlayerDist > 0 && gServerLogic.CheckForPlayersAround(GetPosition(), _ai_NoPatrolPlayerDist) == false)
			{
				StateTimer = curTime + u_GetRandom(3, 5);
				break;
			}
			
			if(doPatrol)
			{
				r3dPoint3D out_pos;
				r3dPoint3D cur_pos = GetPosition();
				patrolPntIdx = spawnObject->GetFreeNavPointIdx(&out_pos, true, _ai_MaxPatrolDistance, &cur_pos);
				if(patrolPntIdx >= 0)
				{
					SetNavAgentSpeed(WalkSpeed);
					MoveNavAgent(out_pos, _ai_MaxPatrolDistance * 2);
					SwitchToState(EAnimalStates::ZState_Walk);
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

		case EAnimalStates::ZState_Walk:
		{
			r3d_assert(!ZombieDisabled && navAgent);

			if(GameObject* trg = ScanForTarget())
			{
				SwitchToState(EAnimalStates::AState_Sprint);
				break;
			}

			
			if(staggerTime < 0)
				SetNavAgentSpeed(WalkSpeed);
				

			switch(CheckMoveStatus())
			{
				default: r3d_assert(false);
				case 0: // completed
				case 2: // failed
				switch(u_random(2)) {
							  case 0: SwitchToState(EAnimalStates::ZState_Idle); break;
							  case 1: SwitchToState(EAnimalStates::AState_IdleEating); break;
					}
					break;
				case 1: // in progress
					if(!CheckMoveWatchdog())
					{
						switch(u_random(2)) {
							  case 0: SwitchToState(EAnimalStates::ZState_Idle); break;
							  case 1: SwitchToState(EAnimalStates::AState_IdleEating); break;
					}
					}
					break;
			}
			
			break;
		}

		case EAnimalStates::AState_Sprint:
		{
			GameObject* trg = GameWorld().GetObject(hardObjLock);
			if(!navAgent) break;
			r3dPoint3D pos;
			//navAgent->SetTargetSpeed(RunSpeed);//work
			SetNavAgentSpeed(RunSpeed);
			if (navAgent->m_status == navAgent->Idle || navAgent->m_status == navAgent->Arrived)
			{
				if (GetFreePoint(&pos,70))//work
					//navAgent->StartMove(pos);// + _ai_MaxPursueDistance); //work
				//lastTargetPos = trg->GetPosition();
				MoveNavAgent(pos, _ai_MaxPursueDistance);
				SprintTime = r3dGetTime();
				SprintStart=true;
			}
			if(SprintStart && (r3dGetTime() - SprintTime) > 6)//(navAgent->m_status == navAgent->Moving && EAnimalStates::AState_Sprint)
			{
					StopNavAgent();
					SprintStart=false;
					switch(u_random(3)) {
							  case 0: SwitchToState(EAnimalStates::ZState_Idle); break;
							  case 1: SwitchToState(EAnimalStates::ZState_Walk); break;
							  case 2: SwitchToState(EAnimalStates::AState_IdleEating); break;
					}
					break;
			}
			break;
		}
		case EAnimalStates::AState_SprintJump:
		{
			GameObject* trg = GameWorld().GetObject(hardObjLock);
			if(!navAgent) break;
			r3dPoint3D pos;
			//navAgent->SetTargetSpeed(RunSpeed);//work
			SetNavAgentSpeed(RunSpeed+2); // 7f speed
			if (navAgent->m_status == navAgent->Idle || navAgent->m_status == navAgent->Arrived)
			{
				if (GetFreePoint(&pos,70))//work
					//navAgent->StartMove(pos);// + _ai_MaxPursueDistance); //work
				//lastTargetPos = trg->GetPosition();
				MoveNavAgent(pos, _ai_MaxPursueDistance);
				SprintJumpTime = r3dGetTime();
				SprintJumpStart=true;
			}
			if(SprintJumpStart && (r3dGetTime() - SprintJumpTime) > 10)//(navAgent->m_status == navAgent->Moving && EAnimalStates::AState_Sprint)
			{
					StopNavAgent();
					SprintJumpStart=false;
					switch(u_random(3)) {
							  case 0: SwitchToState(EAnimalStates::ZState_Idle); break;
							  case 1: SwitchToState(EAnimalStates::ZState_Walk); break;
							  case 2: SwitchToState(EAnimalStates::AState_IdleEating); break;
					}
					break;
			}
			break;
		}
		
		case EAnimalStates::ZState_Dead:
			break;
	}
	
	return TRUE;
}

DefaultPacket* obj_Animal::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateAnimal_s n;
	n.spawnID    = toP2pNetId(GetNetworkID());
	n.spawnPos   = GetPosition();
	n.spawnDir   = GetRotationVector().x;
	n.moveCell   = netMover.SrvGetCell();
	n.HeroItemID = HeroItemID;
	n.HeadIdx    = (BYTE)HeadIdx;
	n.BodyIdx    = (BYTE)BodyIdx;
	n.LegsIdx    = (BYTE)LegsIdx;
	n.State      = (BYTE)AnimalState;
	n.FastZombie = (BYTE)FastZombie;
	n.WalkSpeed  = WalkSpeed;
	n.RunSpeed   = RunSpeed;
	//if(HalloweenZombie) n.HeroItemID += 1000000;
	
	*out_size = sizeof(n);
	return &n;
}

void obj_Animal::SendAIStateToNet()
{
	if(!_ai_DebugAI)
		return;
		
	if(navAgent->m_status == AutodeskNavAgent::Moving)
	{
		PKT_S2C_Zombie_DBG_AIInfo_s n;
		n.from = moveStartPos;
		n.to   = moveTargetPos;
		gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));
	}
}


BOOL obj_Animal::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	switch(EventID)
	{
		case PKT_C2S_Zombie_DBG_AIReq:
			SendAIStateToNet();
			break;
	}
	
	return TRUE;
}

void obj_Animal::SwitchToState(int in_state)
{
	//r3d_assert(AnimalState != EAnimalStates::ZState_Dead); // death should be final
	//r3d_assert(AnimalState != in_state);
	AnimalState    = in_state;
	StateStartTime = r3dGetTime();
	
	// duration of idle state
	if(in_state == EAnimalStates::ZState_Idle)
	{
		StopNavAgent(); // to release current nav point from Walk state
		hardObjLock = invalidGameObjectID;
		StateTimer  = r3dGetTime() + u_GetRandom(3, 10);
	}
	if(in_state == EAnimalStates::AState_IdleEating)
	{
		StopNavAgent(); // to release current nav point from Walk state
		hardObjLock = invalidGameObjectID;
		StateTimer  = r3dGetTime() + u_GetRandom(3, 10);
	}
	
	//r3dOutToLog("zombie%p SwitchToState %d\n", this, ZombieState); CLOG_INDENT;

	PKT_S2C_AnimalSetState_s n;
	n.State    = (BYTE)AnimalState;
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));
}

void obj_Animal::DoDeath()
{
  for(int i=0; i<3; i++)
	{
		// create drop objects.
	    wiInventoryItem wi;
	    wi.itemID = 101415; // ready-to-eat meat.
	    wi.quantity = 1;
	    r3dPoint3D pos = GetPosition();
	    //pos.y += 0.4f;
	    pos.x += u_GetRandom(-1, 2);
	    pos.z += u_GetRandom(-1, 2);

	    // create network object
	    obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", pos);
	    obj->SetNetworkID(gServerLogic.GetFreeNetId());
	    obj->NetworkLocal = true;
	    // vars
	    obj->m_Item       = wi;
	}
	

	// remove from active zombies, but keep object - so it'll be visible for some time on all clients
	StopNavAgent();
	
	DeleteZombieNavAgent(navAgent);
	navAgent = NULL;

	spawnObject->OnZombieKilled(this);
		
	SwitchToState(EAnimalStates::ZState_Dead);
}
bool obj_Animal::GetFreePoint(r3dPoint3D* out_pos , float length)
{
	ObjectManager& GW = GameWorld();
	for (GameObject *targetObj = GW.GetFirstObject(); targetObj; targetObj = GW.GetNextObject(targetObj))
	{
		if (targetObj->Class->Name == "obj_AnimalSpawn")
		{
			obj_AnimalSpawn* spawn = (obj_AnimalSpawn*)targetObj;
			r3dPoint3D pos;
			r3dPoint3D curpos = GetPosition();
			if (spawn->GetFreeNavPointIdx(&pos,true,length,&curpos) != -1)
			{
				*out_pos = pos;
				return true;
			}
		}
	}
	return false;
}
bool obj_Animal::ApplyDamage(GameObject* fromObj, float damage, int bodyPart, STORE_CATEGORIES damageSource,bool isSpecialbool)
{
	if(AnimalState == EAnimalStates::ZState_Dead)
		return false;
	

	//damage *= 0.20f;
	float dmg = damage;
	
	if (damageSource == storecat_Vehicle || damageSource == storecat_ShootVehicle) // Server Vehicles
        dmg = 1000000;

	AnimalHealth -= dmg;

	if(AnimalState != EAnimalStates::AState_SprintJump)
	 {
		SwitchToState(EAnimalStates::AState_SprintJump);
		//return true;
	 }

	if(AnimalHealth <= 0.0f)
	{
		DoDeath();

		if(fromObj->Class->Name == "obj_ServerPlayer")
		{
			obj_ServerPlayer* plr = (obj_ServerPlayer*)fromObj;
			gServerLogic.AddPlayerReward(plr, RWD_AnimalKill, 0);
		}

		return true;
	}

		
	
	return false; // false as zombie wasn't killed
}
