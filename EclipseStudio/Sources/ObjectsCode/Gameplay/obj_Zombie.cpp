//=========================================================================
//	Module: obj_Zombie.cpp
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "obj_Zombie.h"
#include "../AI/r3dPhysSkeleton.h"
#include "obj_ZombieSpawn.h"
#include "ObjectsCode/Weapons/WeaponArmory.h"
#include "ObjectsCode/Weapons/HeroConfig.h"
#include "../../multiplayer/ClientGameLogic.h"
#include "../AI/AI_Player.H"

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS(obj_Zombie, "obj_Zombie", "Object");
AUTOREGISTER_CLASS(obj_Zombie);

extern r3dSkeleton*	g_zombieBindSkeleton;
extern r3dAnimPool*	g_zombieAnimPool;

/////////////////////////////////////////////////////////////////////////

namespace
{
	struct ZombieMeshesDeferredRenderable: public MeshDeferredRenderable
	{
		ZombieMeshesDeferredRenderable()
		: Parent(0)
		{
		}

		void Init()
		{
			DrawFunc = Draw;
		}

		static void Draw(Renderable* RThis, const r3dCamera& Cam)
		{
			R3DPROFILE_FUNCTION("ZombieMeshesDeferredRenderable");

			ZombieMeshesDeferredRenderable* This = static_cast<ZombieMeshesDeferredRenderable*>(RThis);

			r3dApplyPreparedMeshVSConsts(This->Parent->preparedVSConsts);
			This->Parent->OnPreRender();

			MeshDeferredRenderable::Draw(RThis, Cam);
		}

		obj_Zombie *Parent;
	};

//////////////////////////////////////////////////////////////////////////

	struct ZombieMeshesShadowRenderable: public MeshShadowRenderable
	{
		void Init()
		{
			DrawFunc = Draw;
		}

		static void Draw( Renderable* RThis, const r3dCamera& Cam )
		{
			R3DPROFILE_FUNCTION("ZombieMeshesShadowRenderable");
			ZombieMeshesShadowRenderable* This = static_cast< ZombieMeshesShadowRenderable* >( RThis );

			r3dApplyPreparedMeshVSConsts(This->Parent->preparedVSConsts);

			This->Parent->OnPreRender();

			This->SubDrawFunc( RThis, Cam );
		}

		obj_Zombie *Parent;
	};
} // unnamed namespace

//////////////////////////////////////////////////////////////////////////

void obj_Zombie::OnPreRender()
{
	anim_.GetCurrentSkeleton()->SetShaderConstants();
}

struct ZombieDebugRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		ZombieDebugRenderable* This = static_cast<ZombieDebugRenderable*>( RThis );

#ifndef FINAL_BUILD
		This->Parent->DrawDebugInfo();
#endif
	}

	obj_Zombie* Parent;
};
#ifndef FINAL_BUILD
void obj_Zombie::DrawDebugInfo() const
{
	r3dRenderer->SetMaterial(NULL);

	struct PushRestoreStates
	{
		PushRestoreStates()
		{
			r3dRenderer->SetRenderingMode( R3D_BLEND_ALPHA | R3D_BLEND_NZ | R3D_BLEND_PUSH );
		}

		~PushRestoreStates()
		{
			r3dRenderer->SetRenderingMode( R3D_BLEND_POP );
		}
	} pushRestoreStates; (void)pushRestoreStates;

	if(gClientLogic().localPlayer_)
	{
		float dist = (GetPosition() - gClientLogic().localPlayer_->GetPosition()).Length();
		if(dist < 200)
		{
			r3dPoint3D scrCoord;
			if(r3dProjectToScreen(GetPosition() + r3dPoint3D(0, 1, 0), &scrCoord))
			{
				const char* stateTextArr[] = {"Dead", "Sleep", "Awaking", "Idle", "Walk", "Chasing", "Attack", "Barricade"};
				COMPILE_ASSERT(R3D_ARRAYSIZE(stateTextArr) == EZombieStates::ZState_NumStates);

				Font_Label->PrintF(scrCoord.x, scrCoord.y,   r3dColor(255,255,255), "%.1f, %s", dist, stateTextArr[ZombieState]);
			}
		}
	}
	
	if(ZombieState == EZombieStates::ZState_Walk || ZombieState == EZombieStates::ZState_Pursue)
	{
		if(gotDebugMove)
		{
			float w = 0.1f;
			r3dDrawLine3D(dbgAiInfo.to, dbgAiInfo.to + r3dPoint3D(0, 10, 0), gCam, w, r3dColor(255, 255, 0));
			r3dDrawLine3D(dbgAiInfo.from, dbgAiInfo.from + r3dPoint3D(0, 10, 0), gCam, w, r3dColor(0, 0, 255));
			r3dDrawLine3D(dbgAiInfo.from + r3dPoint3D(0, 0.5f, 0), dbgAiInfo.to + r3dPoint3D(0, 0.5f, 0), gCam, w, r3dColor(0, 0, 255));
			r3dDrawLine3D(GetPosition() + r3dPoint3D(0, 1, 0), dbgAiInfo.from, gCam, w, r3dColor(0, 255, 0));
			r3dDrawLine3D(GetPosition() + r3dPoint3D(0, 1, 0), dbgAiInfo.to, gCam, w, r3dColor(0, 255, 0));
		}
	}

	//r3dDrawBoundBox(GetBBoxWorld(), gCam, r3dColor24::green, 0.05f);
}
#endif
//////////////////////////////////////////////////////////////////////////

void obj_Zombie::AppendRenderables(RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& cam)
{
	for (int i = 0; i < 4; ++i)
	{
		r3dMesh *mesh = zombieParts[i];

		if (!mesh)
			continue;

		uint32_t prevCount = render_arrays[ rsFillGBuffer ].Count();
		mesh->AppendRenderablesDeferred( render_arrays[ rsFillGBuffer ], r3dColor::white );

		for( uint32_t i = prevCount, e = render_arrays[ rsFillGBuffer ].Count(); i < e; i ++ )
		{
			ZombieMeshesDeferredRenderable& rend = static_cast<ZombieMeshesDeferredRenderable&>( render_arrays[ rsFillGBuffer ][ i ] ) ;

			rend.Init();
			rend.Parent = this;
		}
	}

#ifndef FINAL_BUILD
	if(r_show_zombie_stats->GetBool())
	{
		ZombieDebugRenderable rend;
		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= 2*RENDERABLE_USER_SORT_VALUE;
		render_arrays[ rsDrawFlashUI ].PushBack( rend );
	}
#endif

}

//////////////////////////////////////////////////////////////////////////

void obj_Zombie::AppendShadowRenderables(RenderArray &rarr, const r3dCamera& cam)
{
	float distSq = (gCam - GetPosition()).LengthSq();
	float dist = sqrtf( distSq );
	UINT32 idist = (UINT32) R3D_MIN( dist * 64.f, (float)0x3fffffff );

	for (int i = 0; i < 4; ++i)
	{
		r3dMesh *mesh = zombieParts[i];
		
		if (!mesh)
			continue;

		uint32_t prevCount = rarr.Count();

		mesh->AppendShadowRenderables( rarr );

		for( uint32_t i = prevCount, e = rarr.Count(); i < e; i ++ )
		{
			ZombieMeshesShadowRenderable& rend = static_cast<ZombieMeshesShadowRenderable&>( rarr[ i ] );

			rend.Init() ;
			rend.SortValue |= idist;
			rend.Parent = this ;
		}
	}
}

obj_Zombie::obj_Zombie() : 
  netMover(this, 1.0f / 10, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
{
	isPhysInRagdoll = false;

	UpdateWarmUp = 0;
	PhysicsOn = 1;

	ObjTypeFlags |= OBJTYPE_Zombie;

	PhysXObstacleIndex = -1;

	m_bEnablePhysics = false;
	m_isSerializable = false;

	if(g_zombieBindSkeleton == NULL) 
	{
		g_zombieBindSkeleton = new r3dSkeleton();
		g_zombieBindSkeleton->LoadBinary("Data\\ObjectsDepot\\Characters\\ProperScale_AndBiped.skl");
		
		g_zombieAnimPool = new r3dAnimPool();
	}
	
	physSkeletonIndex = -1;
	
	lastTimeHitPos   = r3dPoint3D(0, 0, 0);
	lastTimeDmgType  = storecat_ASR;
	lastTimeHitBoneID = 0xFF;
	staggeredTrack   = 0;
	attackTrack      = 0;

	ZeroMemory(zombieParts, _countof(zombieParts) * sizeof(zombieParts[0]));
	ZeroMemory(&CreateParams, sizeof(CreateParams));

	bDead = false;
	m_isFemale = false;

	ZombieSortEntry entry;

	entry.zombie = this;
	entry.distance = 0;
	
	gotDebugMove = false;

	ZombieList.PushBack( entry );
}

//////////////////////////////////////////////////////////////////////////

obj_Zombie::~obj_Zombie()
{
	for( int i = 0, e = (int)ZombieList.Count(); i < e; i ++ )
	{
		if( ZombieList[ i ].zombie == this )
		{
			ZombieList.Erase( i );
			break;
		}
	}
}

BOOL obj_Zombie::OnCreate()
{
	r3d_assert(CreateParams.HeroItemID);
	
	bool HalloweenZombie = false;
	if(CreateParams.HeroItemID > 1000000)
	{
		// special Halloween zombie
		CreateParams.HeroItemID -= 1000000;
		HalloweenZombie = true;
	}

	anim_.Init(g_zombieBindSkeleton, g_zombieAnimPool, NULL, (DWORD)this);

	const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(CreateParams.HeroItemID);
	if(!heroConfig) r3dError("there is no zombie hero %d", CreateParams.HeroItemID);
	
	zombieParts[0] = heroConfig->getHeadMesh(CreateParams.HeadIdx);
	zombieParts[1] = heroConfig->getBodyMesh(CreateParams.HeadIdx, false);
	zombieParts[2] = heroConfig->getLegMesh(CreateParams.LegsIdx);

	/*if(CreateParams.HeroItemID == 20190)//zombies with armor :)
		switch(u_random(6)) {
				default:
		case 0: zombieParts[3] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/HGEAR_M9_Helmet_Urban_01.sco", true, false, true, true); break;
		case 1: zombieParts[3] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/HGEAR_M9_Helmet_Black_01.sco", true, false, true, true); break;
		case 2: zombieParts[3] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/HGEAR_M9_Helmet_Desert_02.sco", true, false, true, true); break;
        case 3: zombieParts[3] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/HGEAR_M9_Helmet_MilGreen_01.sco", true, false, true, true); break;
		case 4: zombieParts[3] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/HGEAR_M9_Helmet_forest_01.sco", true, false, true, true); break;
		case 5: zombieParts[3] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/HGEAR_M9_Helmet_forest_02.sco", true, false, true, true); break;
		case 6: zombieParts[3] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/HGEAR_Maska_M.sco", true, false, true, true); break;
	}
    if(CreateParams.HeroItemID == 20190)
		switch(u_random(4)) {
				default:
		case 0: zombieParts[4] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/ARMOR_Rebel_Heavy.sco", true, false, true, true);  break;
		case 1: zombieParts[4] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/Armor_IBA_01_Sand.sco", true, false, true, true);  break;
		case 2: zombieParts[4] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/Armor_MTV_01_Forest.sco", true, false, true, true);  break;
		case 3: zombieParts[4] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/ARMOR_Light_Forest.sco", true, false, true, true);  break;
		case 4: zombieParts[4] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/ARMOR_Medium_Desert_02.sco", true, false, true, true);  break;
	}*/

	if(HalloweenZombie)
		zombieParts[5] = r3dGOBAddMesh("Data/ObjectsDepot/Characters/hgear_pumpkin_01.sco", true, false, true, true );

	// temporary boundbox until all parts are loaded
	r3dBoundBox bbox;
	bbox.Org	= r3dPoint3D(-0.2f, 0.0f, -0.2f);
	bbox.Size	= r3dPoint3D(+0.2f, 2.0f, +0.2f);
	SetBBoxLocal(bbox);
	gotLocalBbox = false;

	r3d_assert(!NetworkLocal);
	r3d_assert(PhysXObstacleIndex == -1);
	PhysXObstacleIndex = AcquirePlayerObstacle(GetPosition());

	SetRotationVector(r3dVector(CreateParams.spawnDir, 0, 0));
	
	/*PhysicsConfig.group = PHYSCOLL_NETWORKPLAYER;
	PhysicsConfig.type = PHYSICS_TYPE_CONTROLLER_ZOMBIE;
	PhysicsConfig.mass = 100.0f;
	PhysicsConfig.ready = true;
	PhysicsObject = BasePhysicsObject::CreateCharacterController(PhysicsConfig, this);
	PhysicsObject->AdjustControllerSize(0.3f, 1.1f, 0.9f);*/

	// set zombie to starting state.
	ZombieState = -1;
	StateTimer  = -1;
	if(CreateParams.State == EZombieStates::ZState_Dead)
	{
		ZombieState = EZombieStates::ZState_Dead;
		// special case if zombie is dead already, just spawn dead pose anim
		// there is 1-7 dead poses
		char aname[128];
		sprintf(aname, "Zombie_Pose_dead%d", 1 + u_random(7));
		int aid = AddAnimation(aname);
		anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherNow | ANIMFLAG_Looped, 1.0f, 1.0f, 0.0f);
	}
	else
	{
		SetZombieState(CreateParams.State, true);
		
		// remove fading from animation started in SetZombieState
		for(size_t i=0; i<anim_.AnimTracks.size(); i++)
		{
			anim_.AnimTracks[i].dwStatus  &= ~ANIMSTATUS_Fading;
			anim_.AnimTracks[i].fInfluence = 1.0f;
		}
	}

	UpdateAnimations();

	m_isFemale = (CreateParams.HeroItemID == 20183);

	parent::OnCreate();

	m_sndMaxDistIdle = SoundSys.getEventMax3DDistance(SoundSys.GetEventIDByPath("Sounds/WarZ/Zombie sounds/ZOMBIE_IDLE_M"));
	m_sndMaxDistChase = SoundSys.getEventMax3DDistance(SoundSys.GetEventIDByPath("Sounds/WarZ/Zombie sounds/ZOMBIE_CHASING_M"));
	m_sndMaxDistAttack = SoundSys.getEventMax3DDistance(SoundSys.GetEventIDByPath("Sounds/WarZ/Zombie sounds/ZOMBIE_ATTACK_M"));
	m_sndMaxDistAll = R3D_MAX(R3D_MAX(m_sndMaxDistIdle, m_sndMaxDistChase), m_sndMaxDistAttack);
	m_sndIdleHandle = NULL;
	m_sndChaseHandle = NULL;
	m_sndAttackHandle = NULL;
	m_sndHurtHandle = NULL;
	m_sndDeathHandle = NULL;
	
#ifndef FINAL_BUILD
	// send request for AI information updates to server
	PKT_C2S_Zombie_DBG_AIReq_s n;
	p2pSendToHost(this, &n, sizeof(n));
#endif

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void obj_Zombie::UpdateSounds()
{
	if(gClientLogic().localPlayer_ == NULL)
		return;

	if(bDead)
	{
		if(m_sndDeathHandle)
		{
			if(!SoundSys.isPlaying(m_sndDeathHandle))
			{
				SoundSys.Release(m_sndDeathHandle); m_sndDeathHandle = NULL;
			}
		}
		return;
	}

	float distToPlayer = (gClientLogic().localPlayer_->GetPosition()-GetPosition()).Length();

	if(!isSoundAudible())
	{
		DestroySounds();
	}
	else
	{
		if(ZombieState == EZombieStates::ZState_Idle || ZombieState == EZombieStates::ZState_Walk)
		{
			if(m_sndChaseHandle) { SoundSys.Release(m_sndChaseHandle); m_sndChaseHandle = NULL;}
			if(m_sndAttackHandle) { SoundSys.Release(m_sndAttackHandle); m_sndAttackHandle = NULL;}
			if(distToPlayer <= m_sndMaxDistIdle && !m_sndIdleHandle)
			{
				m_sndIdleHandle = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemale?"Sounds/WarZ/Zombie sounds/ZOMBIE_IDLE_F":"Sounds/WarZ/Zombie sounds/ZOMBIE_IDLE_M"), GetPosition());
				//m_sndIdleHandle = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemale?"Sounds/WarZ/Zombie sounds/ZOMBIE_SCREAM_F":"Sounds/WarZ/Zombie sounds/ZOMBIE_SCREAM_M"), GetPosition());
			}
			else if(distToPlayer > m_sndMaxDistIdle && m_sndIdleHandle)
			{
				SoundSys.Release(m_sndIdleHandle); m_sndIdleHandle = NULL;
			}
		}
		else if(ZombieState == EZombieStates::ZState_Pursue)
		{
			if(m_sndIdleHandle) { SoundSys.Release(m_sndIdleHandle); m_sndIdleHandle = NULL;}
			if(m_sndAttackHandle) { SoundSys.Release(m_sndAttackHandle); m_sndAttackHandle = NULL;}
			if(distToPlayer <= m_sndMaxDistChase && !m_sndChaseHandle)
			{
				m_sndChaseHandle = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemale?"Sounds/WarZ/Zombie sounds/ZOMBIE_CHASING_F":"Sounds/WarZ/Zombie sounds/ZOMBIE_CHASING_M"), GetPosition());
			}
			else if(distToPlayer > m_sndMaxDistChase && m_sndChaseHandle)
			{
				SoundSys.Release(m_sndChaseHandle); m_sndChaseHandle = NULL;
			}
		}

		if(m_sndIdleHandle)
			SoundSys.SetSoundPos(m_sndIdleHandle, GetPosition());
		if(m_sndChaseHandle)
			SoundSys.SetSoundPos(m_sndChaseHandle, GetPosition());
		if(m_sndAttackHandle)
		{
			if(m_sndIdleHandle) { SoundSys.Release(m_sndIdleHandle); m_sndIdleHandle = NULL;}
			if(m_sndChaseHandle) { SoundSys.Release(m_sndChaseHandle); m_sndChaseHandle = NULL;}

			if(!SoundSys.isPlaying(m_sndAttackHandle))
			{
				if(r3dGetTime() > m_sndAttackNextPlayTime)
				{
					SoundSys.Start(m_sndAttackHandle);
					m_sndAttackNextPlayTime = r3dGetTime() + m_sndAttackLength;
				}
			}
			SoundSys.SetSoundPos(m_sndAttackHandle, GetPosition());
		}
		if(m_sndHurtHandle)
		{
			if(!SoundSys.isPlaying(m_sndHurtHandle))
			{
				SoundSys.Release(m_sndHurtHandle); m_sndHurtHandle = NULL;
			}
			else
				SoundSys.SetSoundPos(m_sndHurtHandle, GetPosition());
		}
	}
}

void obj_Zombie::PlayAttackSound()
{
	if(gClientLogic().localPlayer_ == NULL)
		return;

	if(!isSoundAudible())
		return;

	m_sndAttackHandle = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemale?"Sounds/WarZ/Zombie sounds/ZOMBIE_ATTACK_F":"Sounds/WarZ/Zombie sounds/ZOMBIE_ATTACK_M"), GetPosition());
	m_sndAttackNextPlayTime = r3dGetTime() + m_sndAttackLength;
}

void obj_Zombie::PlayHurtSound()
{
	if(gClientLogic().localPlayer_ == NULL)
		return;
	if(bDead)
		return;

	if(!isSoundAudible())
		return;

	m_sndHurtHandle = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemale?"Sounds/WarZ/Zombie sounds/ZOMBIE_DAMAGE_F":"Sounds/WarZ/Zombie sounds/ZOMBIE_DAMAGE_M"), GetPosition());
}

void obj_Zombie::PlayDeathSound()
{
	if(gClientLogic().localPlayer_ == NULL)
		return;

	if(!isSoundAudible())
		return;

	m_sndDeathHandle = SoundSys.Play(SoundSys.GetEventIDByPath(m_isFemale?"Sounds/WarZ/Zombie sounds/ZOMBIE_DEATH_F":"Sounds/WarZ/Zombie sounds/ZOMBIE_DEATH_M"), GetPosition());
}

bool obj_Zombie::isSoundAudible()
{
	float distToPlayer = (gClientLogic().localPlayer_->GetPosition()-GetPosition()).Length();
	return distToPlayer < m_sndMaxDistAll; 
}

BOOL obj_Zombie::Update()
{
	parent::Update();
	
	const float curTime = r3dGetTime();

	UpdateSounds();
	
	if(!gotLocalBbox)
	{
		r3dBoundBox bbox;
		bbox.InitForExpansion();
		for(int i=0; i<3; i++) { // not 4 - special mesh isn't counted for bbox
			if(zombieParts[i] && zombieParts[i]->IsLoaded()) {
				bbox.ExpandTo(zombieParts[i]->localBBox);
				gotLocalBbox = true;
			} else {
				gotLocalBbox = false;
				break;
			}
		}
		if(gotLocalBbox) SetBBoxLocal(bbox);
	}

	if(ZombieState == EZombieStates::ZState_Walk || ZombieState == EZombieStates::ZState_Pursue)
	{
		float spd = GetVelocity().Length();

		//@HACK. because of huge delay on network game creation, there will be BIG zombie teleportation on first actual game frame when all movement packets will be proceeded
		//and animation will be fucked up, so limit speed here
		if(spd > 10) spd = 10;
		
		/*// check if speed difference from base speed is less that 20%. In that case don't modify animation speed
		if(ZombieState == EZombieStates::ZState_Walk)
		{
			if(fabs(spd - CreateParams.WalkSpeed) / CreateParams.WalkSpeed < 0.2f)
				spd = CreateParams.WalkSpeed;
		}
		else
		{
			if(fabs(spd - CreateParams.RunSpeed) / CreateParams.RunSpeed < 0.2f)
				spd = CreateParams.RunSpeed;
		}*/

		//anim_.AnimTracks[0].fSpeed = walkAnimCoef * spd;
	}
	
	UpdateAnimations();
	ProcessMovement();

	if(!bDead)
	{
		UpdatePlayerObstacle(PhysXObstacleIndex, GetPosition());
	}

	return TRUE;
}

void obj_Zombie::ProcessMovement()
{
	r3d_assert(!NetworkLocal);
	
	float timePassed = r3dGetFrameTime();
	
	if(GetVelocity().LengthSq() > 0.0001f)
	{
		R3DPROFILE_FUNCTION("NetZombie_Move");
		r3dPoint3D nextPos = GetPosition() + GetVelocity() * timePassed;
		
		// check if we overmoved to target position
		r3dPoint3D v = netMover.GetNetPos() - nextPos;
		float d = GetVelocity().Dot(v);
		if(d < 0) {
			nextPos = netMover.GetNetPos();
			SetVelocity(r3dPoint3D(0, 0, 0));
		}

#if 0
		// adjust zombie to geometry, as it is walking on navmesh on server side - looking jerky right now...
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eDYNAMIC|PxSceneQueryFilterFlag::eSTATIC));
		PxSceneQueryFlags queryFlags(PxSceneQueryFlag::eIMPACT);
		PxRaycastHit hit;
		if (g_pPhysicsWorld->raycastSingle(PxVec3(nextPos.x, nextPos.y + 0.5f, nextPos.z), PxVec3(0, -1, 0), 1, queryFlags, hit, filter))
		{
			nextPos.y = hit.impact.y;
		}
#endif
		SetPosition(nextPos);
	}
}

int obj_Zombie::AddAnimation(const char* anim)
{
	char buf[MAX_PATH];
	sprintf(buf, "Data\\ObjectsDepot\\Zombie_Test\\Animations\\%s.anm", anim);
	int aid = g_zombieAnimPool->Add(anim, buf);
	
	return aid;
}	

void obj_Zombie::UpdateAnimations()
{
	const float TimePassed = r3dGetFrameTime();

	float zombRadiusSqr = g_zombie_update_radius->GetFloat();
	zombRadiusSqr *= zombRadiusSqr;

	r3dPhysSkeleton* phySkeleton = GetPhysSkeleton();

	if( ( gCam - GetPosition() ).LengthSq() > zombRadiusSqr )
	{
		if( UpdateWarmUp > 5 )
		{
			if( PhysicsOn )
			{
				if( phySkeleton ) 
					phySkeleton->SetBonesActive( false );

				PhysicsOn = 0;
			}
			return;
		}
	}

	if( !PhysicsOn )
	{
		if( phySkeleton ) phySkeleton->SetBonesActive( true );
		PhysicsOn = 1;
	}

	UpdateWarmUp ++;

#if ENABLE_RAGDOLL
	bool ragdoll = phySkeleton && phySkeleton->IsRagdollMode();
	if (!ragdoll)
#endif
	{
		D3DXMATRIX CharDrawMatrix;
		D3DXMatrixIdentity(&CharDrawMatrix);
		anim_.Update(TimePassed, r3dPoint3D(0,0,0), CharDrawMatrix);
		anim_.Recalc();
	}

	if( phySkeleton && PhysicsOn )
		phySkeleton->syncAnimation(anim_.GetCurrentSkeleton(), GetTransformMatrix(), anim_);

#if ENABLE_RAGDOLL
	if (ragdoll)
	{
		r3dBoundBox bbox = phySkeleton->getWorldBBox();
		bbox.Org -= GetPosition();
		SetBBoxLocal(bbox);
	}
#endif

}

//////////////////////////////////////////////////////////////////////////

BOOL obj_Zombie::OnDestroy()
{
	parent::OnDestroy();

	ReleasePlayerObstacle(&PhysXObstacleIndex);

	UnlinkSkeleton();

	DestroySounds();

	return TRUE;
}

void obj_Zombie::DoDeath()
{
	bDead = true;

	ReleasePlayerObstacle(&PhysXObstacleIndex);

	r3dPoint3D hitForce = lastTimeHitPos;//(GetPosition() - lastTimeHitPos).NormalizeTo();

	float dmgForce = 10.0f;
	switch(lastTimeDmgType)
	{
	case storecat_ASR:
		dmgForce = 30.0f;
		break;
	case storecat_SNP:
		dmgForce = 60.0f;
		break;
	case storecat_SHTG:
		dmgForce = 60.0f;
		break;
	case storecat_MG:
		dmgForce = 40.0f;
		break;
	case storecat_HG:
		dmgForce = 10.0f;
		break;
	case storecat_SMG:
		dmgForce = 20.0f;
		break;
	}
	hitForce *= dmgForce*2;

	//PhysicsObject->AdjustControllerSize(0.01f, 0.01f, 0.0f);
	// switch to ragdoll with random bone

	SwitchPhysToRagdoll( true, &hitForce, lastTimeHitBoneID );
	
	// clear rotation so it won't affect ragdoll bbox
	SetRotationVector(r3dPoint3D(0, 0, 0));
	
	DestroySounds();
	PlayDeathSound();

	anim_.StopAll();
}

r3dPhysSkeleton* obj_Zombie::GetPhysSkeleton() const
{
	if( physSkeletonIndex >= 0 )
		return PhysSkeletonCache[ physSkeletonIndex ].skeleton;
	else
		return NULL;
}

void obj_Zombie::StartWalkAnim(bool run)
{
	int aid = 0;
	float wsk = 1.0f;	// walk animation speed coefficient
	if(!run && !CreateParams.FastZombie) {
		aid = AddAnimation("Zombie_walk_wander");
		wsk = .8f; //2.2f;
	} else if(!run && CreateParams.FastZombie) {
		aid = AddAnimation("Zombie_walk_01");
		wsk = 1.0f;
	} else if(run && !CreateParams.FastZombie) {
		aid = AddAnimation("Zombie_walk_legdrag");
		wsk = 0.425f; //1.3f;
	} else if(run && CreateParams.FastZombie) {
		aid = AddAnimation("Zombie_sprint_02");
		wsk = 0.5f; //0.3f;
	} else r3d_assert(false);
	
	walkAnimCoef = wsk;
	int tr = anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.1f);

	// start with randomized frame
	r3dAnimation::r3dAnimInfo* ai = anim_.GetTrack(tr);
	ai->fCurFrame = u_GetRandom(0, (float)ai->pAnim->NumFrames - 1);
}

r3dTL::TArray< obj_Zombie::ZombieSortEntry > obj_Zombie::ZombieList;
r3dTL::TArray< obj_Zombie::CacheEntry > obj_Zombie::PhysSkeletonCache;

void obj_Zombie::InitPhysSkeletonCache( float progressStart, float progressEnd )
{
	r3dOutToLog( "obj_Zombie::InitPhysSkeletonCache..\n" );

	const int PHYS_SKELETON_CACHE_SIZE = 256;

	if( PhysSkeletonCache.Count() < PHYS_SKELETON_CACHE_SIZE )
	{
		PhysSkeletonCache.Resize( PHYS_SKELETON_CACHE_SIZE );

		for( int i = 0; i < PHYS_SKELETON_CACHE_SIZE; i ++ )
		{
			void SetLoadingProgress( float progress );
			SetLoadingProgress( progressStart + ( progressEnd - progressStart ) * i / ( PHYS_SKELETON_CACHE_SIZE - 1 ) );

			//if( !( i & 0x07 ) )
			//	r3dOutToLog( "." );

			PhysSkeletonCache[ i ].skeleton = new r3dPhysSkeleton( "data/ObjectsDepot/Characters/RagDoll.RepX" );
			PhysSkeletonCache[ i ].allocated = false;
		}
	}

	r3dOutToLog( "done\n" );
}

void obj_Zombie::FreePhysSkeletonCache()
{
	r3dOutToLog( "obj_Zombie::FreePhysSkeletonCache:" );

	float start = r3dGetTime();

	for( int i = 0, e = PhysSkeletonCache.Count(); i < e ; i ++ )
	{
		delete PhysSkeletonCache[ i ].skeleton;

		if( !(i & 0x07) )
			r3dOutToLog(".");
	}

	r3dOutToLog( "done in %.2f seconds\n", r3dGetTime() - start );

	PhysSkeletonCache.Clear();
}

static r3dTL::TArray< int > TempPhysSkelIndexArray;

void obj_Zombie::ReassignSkeletons()
{
	if( !ZombieList.Count() )
		return;

	R3DPROFILE_FUNCTION( "obj_Zombie::ReassignSkeletons" );

	TempPhysSkelIndexArray.Clear();

	struct sortfunc
	{
		bool operator()( const ZombieSortEntry& e0, const ZombieSortEntry& e1 )
		{
			return e0.distance < e1.distance;
		}
	};

	for( int i = 0, e = ZombieList.Count(); i < e; i ++ )
	{
		ZombieSortEntry& entry = ZombieList[ i ];
		entry.distance = ( entry.zombie->GetPosition() - gCam ).LengthSq();
	}

	std::sort( &ZombieList[ 0 ], &ZombieList[ 0 ] + ZombieList.Count(), sortfunc() );

	for( int i = PhysSkeletonCache.Count(), e = ZombieList.Count(); i < e; i ++ )
	{
		ZombieSortEntry& entry = ZombieList[ i ];

		if( entry.zombie->physSkeletonIndex >= 0  )
		{
			TempPhysSkelIndexArray.PushBack( entry.zombie->physSkeletonIndex );

			entry.zombie->UnlinkSkeleton();
		}
	}

	for( int i = 0, e = R3D_MIN( PhysSkeletonCache.Count(), ZombieList.Count() ); i < e; i ++ )
	{
		ZombieSortEntry& entry = ZombieList[ i ];

		if( entry.zombie->physSkeletonIndex < 0 && entry.zombie->PhysicsOn)
		{
			if( TempPhysSkelIndexArray.Count() )
			{
				entry.zombie->LinkSkeleton( TempPhysSkelIndexArray.GetLast() );
				TempPhysSkelIndexArray.Resize( TempPhysSkelIndexArray.Count() - 1 );
			}
			else
			{
				for( int i = 0, e = PhysSkeletonCache.Count(); i < e; i ++ )
				{
					if( !PhysSkeletonCache[ i ].allocated )
					{
						entry.zombie->LinkSkeleton( i );
						break;
					}
				}
			}
		}
	}
}

void obj_Zombie::LinkSkeleton( int cacheIndex )
{
	r3dPhysSkeleton* skeleton = PhysSkeletonCache[ cacheIndex ].skeleton;
	PhysSkeletonCache[ cacheIndex ].allocated = true;

	physSkeletonIndex = cacheIndex;

	skeleton->linkParent(anim_.GetCurrentSkeleton(), GetTransformMatrix(), this, PHYSCOLL_NETWORKPLAYER) ;

	SwitchPhysToRagdoll( isPhysInRagdoll ? true : false, NULL, 0xFF );
}

void obj_Zombie::UnlinkSkeleton()
{
	if( physSkeletonIndex >= 0 )
	{
		PhysSkeletonCache[ physSkeletonIndex ].skeleton->unlink();
		PhysSkeletonCache[ physSkeletonIndex ].allocated = false;

		physSkeletonIndex = -1;
	}
}

void obj_Zombie::SwitchPhysToRagdoll( bool ragdollOn, r3dPoint3D* hitForce, int BoneID )
{
	isPhysInRagdoll = ragdollOn;

	if( physSkeletonIndex >= 0 )
	{
		r3dPhysSkeleton* physSkel = GetPhysSkeleton();

		if( ragdollOn )
			physSkel->SwitchToRagdollWithForce( true, BoneID, hitForce );
		else
		{
			if( physSkel->IsRagdollMode() )
			{
				physSkel->SwitchToRagdoll( false );
			}
		}
	}
}

void obj_Zombie::SetZombieState(int state, bool atStart)
{
	r3d_assert(ZombieState != state);
	ZombieState = state;
	
	switch(ZombieState)
	{
		default:
			r3dError("unknown zombie state %d", ZombieState);
			return;

		case EZombieStates::ZState_Sleep:
		{
			int aid = 0;
			switch(u_random(3)) {
				default:
				case 0:	aid = AddAnimation("Zombie_Prone_Idle_1"); break;
				case 1:	aid = AddAnimation("Zombie_Prone_Idle_2"); break;
				case 2:	aid = AddAnimation("Zombie_Prone_Idle_3"); break;
			}
			anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherNow | ANIMFLAG_PauseOnEnd | ANIMSTATUS_Paused, 1.0f, 1.0f, 1.0f);
			break;
		}
		
		// wake up zombie, unpause animation from Sleep
		case EZombieStates::ZState_Waking:
			if(!atStart)
			{
				r3d_assert(anim_.AnimTracks.size() == 1);
				if(anim_.AnimTracks.size() > 0)
					anim_.AnimTracks[0].dwStatus &= ~ANIMSTATUS_Paused;
				break;
			}
			// note - waking zombies created at object creation phase will fallthru to idle phase.

		case EZombieStates::ZState_Idle:
		{
			// select new idle animation
			int aid = 0;
			switch(u_random(2)) {
				default:
				case 0:	aid = AddAnimation("Zombie_Idle_03"); break;
				case 1:	aid = AddAnimation("Zombie_Idle_02"); break;
			}
			if(u_random(10) == 0)
				aid = AddAnimation("Zombie_Eat_Crouched_03");

			int tr = anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.2f);

			// start with randomized frame
			r3dAnimation::r3dAnimInfo* ai = anim_.GetTrack(tr);
			ai->fCurFrame = u_GetRandom(0, (float)ai->pAnim->NumFrames - 1);
			break;
		}

		case EZombieStates::ZState_Walk:
			StartWalkAnim(false);
			break;
		
		case EZombieStates::ZState_Pursue:
			StartWalkAnim(true);
			break;

		case EZombieStates::ZState_Dead:
			DoDeath();
			return;

		case EZombieStates::ZState_Attack:
			{
			// select new attack animation
			int aid = 0;
			switch(u_random(2)) {
				default:
				case 0:	aid = AddAnimation("Zombie_Bite_Attack_1"); break;
				case 1:	aid = AddAnimation("Zombie_Swing_Attack03"); break;
			}
			attackTrack = anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.2f);

			SetVelocity(r3dPoint3D(0, 0, 0));
			break;
			}
		case EZombieStates::ZState_BarricadeAttack:
			{
			// select new attack animation
			int aid = 0;
			switch(u_random(3)) {
				default:
				case 0:	aid = AddAnimation("Zombie_Swing_Attack01"); break;
				case 1:	aid = AddAnimation("Zombie_Swing_Attack02"); break;
				case 2:	aid = AddAnimation("Zombie_Swing_Attack03"); break;
			}
			attackTrack = anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.2f);

			SetVelocity(r3dPoint3D(0, 0, 0));
			break;
		}
	}
}


void obj_Zombie::OnNetPacket(const PKT_S2C_MoveTeleport_s& n)
{
	// set player position and reset cell base, so it'll be updated on next movement
	SetPosition(n.teleport_pos);
	netMover.Teleport(n.teleport_pos);
}

void obj_Zombie::OnNetPacket(const PKT_C2C_MoveSetCell_s& n)
{
	netMover.SetCell(n);
}

void obj_Zombie::OnNetPacket(const PKT_C2C_MoveRel_s& n)
{
	const CNetCellMover::moveData_s& md = netMover.DecodeMove(n);
	
	// might be unsafe, ignore movement while we staggered
	if(!staggeredTrack)
	{
		SetRotationVector(r3dVector(md.turnAngle, 0, 0));
	
		// we are ignoring md.state & md.bendAngle for now
	
		// calc velocity to reach position on time for next update
		r3dPoint3D vel = netMover.GetVelocityToNetTarget(
			GetPosition(),
			CreateParams.RunSpeed * 1.5f,
			1.0f);
		SetVelocity(vel);
	}
	
	if(md.state == 1 && !staggeredTrack)
	{
		static const char* anims[] = {
			"Zombie_Staggered_01_B",
			"Zombie_Staggered_01_L",
			"Zombie_Staggered_01_R",
			"Zombie_Staggered_Small_01_B",
			"Zombie_Staggered_Small_01_F"
			};

		const char* hitAnim = anims[u_random(5)];
		int aid = AddAnimation(hitAnim);
		staggeredTrack = anim_.StartAnimation(aid, ANIMFLAG_PauseOnEnd, 0.0f, 1.0f, 0.1f);

		SetVelocity(r3dPoint3D(0, 0, 0));
	}

	if(md.state == 0 && staggeredTrack)
	{
		anim_.FadeOut(staggeredTrack, 0.1f);
		staggeredTrack = 0;
	}

	/*if(md.state == 2 && !attackTrack)
	{
		static const char* anims[] = {
			"Zombie_Swing_Attack01",
			"Zombie_Swing_Attack02",
			};

		const char* atkAnim = anims[u_random(2)];
		int aid = AddAnimation(atkAnim);
		attackTrack = anim_.StartAnimation(aid, ANIMFLAG_Looped, 0.0f, 1.0f, 0.1f);

		m_sndAttackLength = float(anim_.GetTrack(attackTrack)->pAnim->GetNumFrames())/anim_.GetTrack(attackTrack)->pAnim->GetFrameRate();
		PlayAttackSound();

		SetVelocity(r3dPoint3D(0, 0, 0));
	}

	if(md.state == 0 && attackTrack)
	{
		anim_.FadeOut(attackTrack, 0.1f);
		attackTrack = 0;
	}*/
}

void obj_Zombie::OnNetPacket(const PKT_S2C_ZombieSetState_s& n)
{
	SetZombieState(n.State, false);
}

BOOL obj_Zombie::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	R3DPROFILE_FUNCTION("obj_Zombie::OnNetReceive");
	r3d_assert(!(ObjFlags & OBJFLAG_JustCreated)); // make sure that object was actually created before processing net commands

#undef DEFINE_GAMEOBJ_PACKET_HANDLER
#define DEFINE_GAMEOBJ_PACKET_HANDLER(xxx) \
	case xxx: { \
		const xxx##_s&n = *(xxx##_s*)packetData; \
		r3d_assert(packetSize == sizeof(n)); \
		OnNetPacket(n); \
		return TRUE; \
	}

	switch(EventID)
	{
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_MoveTeleport);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveSetCell);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveRel);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_ZombieSetState);
		
		case PKT_S2C_Zombie_DBG_AIInfo:
		{
			const PKT_S2C_Zombie_DBG_AIInfo_s&n = *(PKT_S2C_Zombie_DBG_AIInfo_s*)packetData;
			gotDebugMove = true;
			memcpy(&dbgAiInfo, &n, sizeof(n));
			return TRUE;
		}
	}

	return FALSE;
}

void obj_Zombie::OnNetAttackStart()
{
	m_sndAttackLength = float(anim_.GetTrack(attackTrack)->pAnim->GetNumFrames())/anim_.GetTrack(attackTrack)->pAnim->GetFrameRate();
	PlayAttackSound();
}

void obj_Zombie::DestroySounds()
{
	if(m_sndIdleHandle) { SoundSys.Release(m_sndIdleHandle); m_sndIdleHandle = NULL;}
	if(m_sndChaseHandle) { SoundSys.Release(m_sndChaseHandle); m_sndChaseHandle = NULL;}
	if(m_sndAttackHandle) { SoundSys.Release(m_sndAttackHandle); m_sndAttackHandle = NULL;}
	if(m_sndHurtHandle) { SoundSys.Release(m_sndHurtHandle); m_sndHurtHandle = NULL;}
	if(m_sndDeathHandle) { SoundSys.Release(m_sndDeathHandle); m_sndDeathHandle = NULL;}
}