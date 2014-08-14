///=========================================================================
//	Module: obj_Animal.cpp
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "obj_Animal.h"
#include "../AI/r3dPhysSkeleton.h"
#include "obj_AnimalSpawn.h"
#include "ObjectsCode/Weapons/WeaponArmory.h"
#include "ObjectsCode/Weapons/HeroConfig.h"
#include "../../multiplayer/ClientGameLogic.h"
#include "../AI/AI_Player.H"

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS(obj_Animal, "obj_Animal", "Object");
AUTOREGISTER_CLASS(obj_Animal);

extern r3dSkeleton*	g_animalBindSkeleton;
extern r3dAnimPool*	g_animalAnimPool;

/////////////////////////////////////////////////////////////////////////

namespace
{
	struct AnimalMeshesDeferredRenderable: public MeshDeferredRenderable
	{
		AnimalMeshesDeferredRenderable()
		: Parent(0)
		{
		}

		void Init()
		{
			DrawFunc = Draw;
		}

		static void Draw(Renderable* RThis, const r3dCamera& Cam)
		{
			R3DPROFILE_FUNCTION("AnimalMeshesDeferredRenderable");

			AnimalMeshesDeferredRenderable* This = static_cast<AnimalMeshesDeferredRenderable*>(RThis);

			r3dApplyPreparedMeshVSConsts(This->Parent->preparedVSConsts);
			This->Parent->OnPreRender();

			MeshDeferredRenderable::Draw(RThis, Cam);
		}

		obj_Animal *Parent;
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

		obj_Animal *Parent;
	};
} // unnamed namespace

//////////////////////////////////////////////////////////////////////////

void obj_Animal::OnPreRender()
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
	}

	obj_Animal* Parent;
};
//////////////////////////////////////////////////////////////////////////

void obj_Animal::AppendRenderables(RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& cam)
{
	for (int i = 0; i < 1; ++i)
	{
		r3dMesh *mesh = deerModel;

		if (!mesh)
			continue;

		uint32_t prevCount = render_arrays[ rsFillGBuffer ].Count();
		mesh->AppendRenderablesDeferred( render_arrays[ rsFillGBuffer ], r3dColor::white );

		for( uint32_t i = prevCount, e = render_arrays[ rsFillGBuffer ].Count(); i < e; i ++ )
		{
			AnimalMeshesDeferredRenderable& rend = static_cast<AnimalMeshesDeferredRenderable&>( render_arrays[ rsFillGBuffer ][ i ] ) ;

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

void obj_Animal::AppendShadowRenderables(RenderArray &rarr, const r3dCamera& cam)
{
	float distSq = (gCam - GetPosition()).LengthSq();
	float dist = sqrtf( distSq );
	UINT32 idist = (UINT32) R3D_MIN( dist * 64.f, (float)0x3fffffff );

	for (int i = 0; i < 1; ++i)
	{
		r3dMesh *mesh = deerModel;
		
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

obj_Animal::obj_Animal() : // this is the prob
  netMover(this, 1.0f / 10, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
{
	isPhysInRagdoll = false;

	UpdateWarmUp = 0;
	PhysicsOn = 1;

	ObjTypeFlags |= OBJTYPE_Animal;

	PhysXObstacleIndex = -1;

	m_bEnablePhysics = false;
	m_isSerializable = false;
	DeerCreated=false;
	DeerHaveCollision=false;

	if(g_animalBindSkeleton == NULL) 
	{
		g_animalBindSkeleton = new r3dSkeleton();
		g_animalBindSkeleton->LoadBinary("Data\\ObjectsDepot\\WZ_Animals\\Char_Deer_01.skl");
		
		g_animalAnimPool = new r3dAnimPool();
	}
	
	physSkeletonIndex = -1;
	
	lastTimeHitPos   = r3dPoint3D(0, 0, 0);
	lastTimeDmgType  = storecat_ASR;
	lastTimeHitBoneID = 0xFF;
	staggeredTrack   = 0;
	attackTrack      = 0;

//	ZeroMemory(deerModel, _countof(deerModel) * sizeof(deerModel[0]));
	ZeroMemory(&CreateParams, sizeof(CreateParams));

	bDead = false;

	ZombieSortEntry entry;

	entry.zombie = this;
	entry.distance = 0;
	
	gotDebugMove = false;

	ZombieList.PushBack( entry );
}

//////////////////////////////////////////////////////////////////////////

obj_Animal::~obj_Animal()
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

BOOL obj_Animal::OnCreate()
{
	r3d_assert(CreateParams.HeroItemID);
	

	anim_.Init(g_animalBindSkeleton, g_animalAnimPool, NULL, (DWORD)this);

	const HeroConfig* heroConfig = g_pWeaponArmory->getHeroConfig(CreateParams.HeroItemID);
	if(!heroConfig) r3dError("there is no animal hero %d", CreateParams.HeroItemID);
	
	//zombieParts[0] = heroConfig->getHeadMesh(CreateParams.HeadIdx);
	//zombieParts[1] = heroConfig->getBodyMesh(CreateParams.HeadIdx, false);
	//zombieParts[2] = heroConfig->getLegMesh(CreateParams.LegsIdx); 
	int DeerModelSelect = (int)u_GetRandom(0,4);
	switch(DeerModelSelect)
	{
		  case 0: deerModel = r3dGOBAddMesh("Data/ObjectsDepot/WZ_Animals/char_deer_01.sco", true, false, true, true ); break;
		  case 1: deerModel = r3dGOBAddMesh("Data/ObjectsDepot/WZ_Animals/char_deer_02_doe.sco", true, false, true, true ); break;
		  case 2: deerModel = r3dGOBAddMesh("Data/ObjectsDepot/WZ_Animals/char_deer_01.sco", true, false, true, true ); break;
		  case 3: deerModel = r3dGOBAddMesh("Data/ObjectsDepot/WZ_Animals/char_deer_02_doe.sco", true, false, true, true ); break;
	}
	DeerCreated=true;
	if(DeerCreated && !DeerHaveCollision)
	{
	   // Collision = (obj_Building*)srv_CreateGameObject("obj_Building", "data/objectsdepot/WZ_Animals/Collision.sco", GetPosition () );
	    Collision = (obj_Building*)srv_CreateGameObject("obj_Building", "data/objectsdepot/env_collision/collision_wall_b_3x3.sco", GetPosition () );
		Collision->SetRotationVector(GetRotationVector());// + r3dPoint3D(90,0,0));
		Collision->SetScale(r3dPoint3D(0.10f,0.27f,0.90f));
		Collision->SetObjFlags(OBJFLAG_SkipDraw);
		DeerHaveCollision=true;
	}
	//collision = (obj_Building*)srv_CreateGameObject("obj_Building", "data/objectsdepot/env_collision/CollisionCar.sco", GetPosition () );



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

	///*PhysicsConfig.group = PHYSCOLL_NETWORKPLAYER;
	//PhysicsConfig.type = PHYSICS_TYPE_CONTROLLER_ZOMBIE;
	//PhysicsConfig.mass = 100.0f;
	//PhysicsConfig.ready = true;
	//PhysicsObject = BasePhysicsObject::CreateCharacterController(PhysicsConfig, this);

	//PhysicsObject->AdjustControllerSize(0.3f, 1.1f, 0.9f);

	// set Animal to starting state.
	AnimalState = -1;
	StateTimer  = -1;
	if(CreateParams.State == EAnimalStates::ZState_Dead)
	{
		AnimalState = EAnimalStates::ZState_Dead;
		int aid = AddAnimation("Deer_Death_Fullbody");
		anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherNow | ANIMFLAG_Looped, 1.0f, 1.0f, 0.0f);
	}
	else
	{
		SetAnimalState(CreateParams.State, true);
		
		// remove fading from animation started in SetAnimalState
		for(size_t i=0; i<anim_.AnimTracks.size(); i++)
		{
			anim_.AnimTracks[i].dwStatus  &= ~ANIMSTATUS_Fading;
			anim_.AnimTracks[i].fInfluence = 1.0f;
		}
	}

	UpdateAnimations();

	parent::OnCreate();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_Animal::Update()
{
	parent::Update();
	
  //if (Collision==NULL)
  //{
		//Collision = (obj_Building*)srv_CreateGameObject("obj_Building", "data/objectsdepot/env_collision/CollisionCar.sco", GetPosition () );
		//Collision->SetRotationVector(GetRotationVector() + r3dPoint3D(90,0,0));
		//Collision->SetScale(r3dPoint3D(0.31f,0.49f,0.69f));
		////CollisionCar->SetObjFlags(OBJFLAG_PlayerCollisionOnly);
		//Collision->SetObjFlags(OBJFLAG_SkipDraw);
  //}

	if(DeerHaveCollision)
	{
		Collision->SetPosition(GetPosition());
		Collision->SetRotationVector(GetRotationVector());
		//Collision->SetRotationVector(GetRotationVector());
	}
  
	
	const float curTime = r3dGetTime();

//	UpdateSounds();
	
	if(!gotLocalBbox)
	{
		r3dBoundBox bbox;
		bbox.InitForExpansion();
		for(int i=0; i<3; i++) { // not 4 - special mesh isn't counted for bbox
			if(deerModel->IsLoaded()) {
				bbox.ExpandTo(deerModel->localBBox);
				gotLocalBbox = true;
				//deerModel->SetFlag(OBJTYPEBuilding,true);
			} else {
				gotLocalBbox = false;
				break;
			}
		}
		if(gotLocalBbox) SetBBoxLocal(bbox);
	}

	if(AnimalState == EAnimalStates::ZState_Walk || AnimalState == EAnimalStates::AState_Sprint || AnimalState == EAnimalStates::AState_SprintJump)
	{
		float spd = GetVelocity().Length();

		//@HACK. because of huge delay on network game creation, there will be BIG Animal teleportation on first actual game frame when all movement packets will be proceeded
		//and animation will be fucked up, so limit speed here
		if(spd > 10) spd = 10;
		
		/*// check if speed difference from base speed is less that 20%. In that case don't modify animation speed
		if(ZombieState == EAnimalStates::ZState_Walk)
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
		UpdateAnimalObstacle(PhysXObstacleIndex, GetPosition());
	}

	return TRUE;
}

void obj_Animal::ProcessMovement()
{
	r3d_assert(!NetworkLocal);
	
	float timePassed = r3dGetFrameTime();
	
	if(GetVelocity().LengthSq() > 0.0001f)
	{
		R3DPROFILE_FUNCTION("NetAnimal_Move");
		r3dPoint3D nextPos = GetPosition() + GetVelocity() * timePassed;
		
		// check if we overmoved to target position
		r3dPoint3D v = netMover.GetNetPos() - nextPos;
		float d = GetVelocity().Dot(v);
		if(d < 0) {
			nextPos = netMover.GetNetPos();
			SetVelocity(r3dPoint3D(0, 0, 0));
		}

#if 0
		// adjust animal to geometry, as it is walking on navmesh on server side - looking jerky right now...
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

int obj_Animal::AddAnimation(const char* anim)
{
	char buf[MAX_PATH];
	sprintf(buf, "Data\\ObjectsDepot\\WZ_Animals\\Animations\\%s.anm", anim);
	int aid = g_animalAnimPool->Add(anim, buf);
	
	return aid;
}	

void obj_Animal::UpdateAnimations()
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

BOOL obj_Animal::OnDestroy()
{
	parent::OnDestroy();

	ReleasePlayerObstacle(&PhysXObstacleIndex);

	UnlinkSkeleton();

//	DestroySounds();

	return TRUE;
}

void obj_Animal::DoDeath()
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
    if(bDead)
	{
		AnimalState = EAnimalStates::ZState_Dead;
		int aid = AddAnimation("Deer_Death_Fullbody");
		anim_.StartAnimation(aid, ANIMFLAG_PauseOnEnd, 1.0f, 1.0f, 0.0f);
	    GameWorld().DeleteObject( Collision );
	    Collision = NULL;
	    DeerHaveCollision=false;
		
	}
		
			
			//CollisionCar=NULL;

	//PhysicsObject->AdjustControllerSize(0.01f, 0.01f, 0.0f);
	// switch to ragdoll with random bone

	//SwitchPhysToRagdoll( true, &hitForce, lastTimeHitBoneID );
	
	// clear rotation so it won't affect ragdoll bbox
	SetRotationVector(r3dPoint3D(0, 0, 0));

	//anim_.StopAll();
}

r3dPhysSkeleton* obj_Animal::GetPhysSkeleton() const
{
	if( physSkeletonIndex >= 0 )
		return PhysSkeletonCache[ physSkeletonIndex ].skeleton;
	else
		return NULL;
}

void obj_Animal::StartWalkAnim(bool run)
{
	int aid = 0;
	float wsk = 1.0f;	// walk animtion speed coefficient
	if(!run && !CreateParams.FastZombie) {
		aid = AddAnimation("Deer_Walk_Forward");
		wsk = .8f; //2.2f;
	} else if(!run && CreateParams.FastZombie) {
		aid = AddAnimation("Deer_Walk_Grazing");
		wsk = 1.0f;
	} else if(run && !CreateParams.FastZombie) {
		//aid = AddAnimation("Deer_JumpSprint");
		aid = AddAnimation("Deer_Run");
		wsk = 0.425f; //1.3f;
	} else if(run && CreateParams.FastZombie) {
		aid = AddAnimation("Deer_Run");
		wsk = 0.5f; //0.3f;
	} else r3d_assert(false);
	
	walkAnimCoef = wsk;
	int tr = anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.1f);

	// start with randomized frame
	r3dAnimation::r3dAnimInfo* ai = anim_.GetTrack(tr);
	ai->fCurFrame = u_GetRandom(0, (float)ai->pAnim->NumFrames - 1);
}
void obj_Animal::StartSprintJumpAnim(bool run)
{
	int aid = 0;
	float wsk = 1.0f;	// walk animtion speed coefficient
	if(!run && !CreateParams.FastZombie) {
		aid = AddAnimation("Deer_JumpSprint");
		wsk = .8f; //2.2f;
	} else if(!run && CreateParams.FastZombie) {
		aid = AddAnimation("Deer_JumpSprint");
		wsk = 1.0f;
	} else if(run && !CreateParams.FastZombie) {
		//aid = AddAnimation("Deer_JumpSprint");
		aid = AddAnimation("Deer_JumpSprint");
		wsk = 0.7f; //1.3f;
	} else if(run && CreateParams.FastZombie) {
		aid = AddAnimation("Deer_JumpSprint");
		wsk = 0.7f; //0.3f;
	} else r3d_assert(false);
	
	walkAnimCoef = wsk;
	int tr = anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.1f);

	// start with randomized frame
	r3dAnimation::r3dAnimInfo* ai = anim_.GetTrack(tr);
	ai->fCurFrame = u_GetRandom(0, (float)ai->pAnim->NumFrames - 1);
}

r3dTL::TArray< obj_Animal::ZombieSortEntry > obj_Animal::ZombieList;
r3dTL::TArray< obj_Animal::CacheEntry > obj_Animal::PhysSkeletonCache;

void obj_Animal::InitPhysSkeletonCache( float progressStart, float progressEnd )
{
	r3dOutToLog( "obj_Animal::InitPhysSkeletonCache..\n" );

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

void obj_Animal::FreePhysSkeletonCache()
{
	r3dOutToLog( "obj_Animal::FreePhysSkeletonCache:" );

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

void obj_Animal::ReassignSkeletons()
{
	if( !ZombieList.Count() )
		return;

	R3DPROFILE_FUNCTION( "obj_Animal::ReassignSkeletons" );

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

void obj_Animal::LinkSkeleton( int cacheIndex )
{
	r3dPhysSkeleton* skeleton = PhysSkeletonCache[ cacheIndex ].skeleton;
	PhysSkeletonCache[ cacheIndex ].allocated = true;

	physSkeletonIndex = cacheIndex;

	skeleton->linkParent(anim_.GetCurrentSkeleton(), GetTransformMatrix(), this, PHYSCOLL_NETWORKPLAYER) ;

	SwitchPhysToRagdoll( isPhysInRagdoll ? true : false, NULL, 0xFF );
}

void obj_Animal::UnlinkSkeleton()
{
	if( physSkeletonIndex >= 0 )
	{
		PhysSkeletonCache[ physSkeletonIndex ].skeleton->unlink();
		PhysSkeletonCache[ physSkeletonIndex ].allocated = false;

		physSkeletonIndex = -1;
	}
}

void obj_Animal::SwitchPhysToRagdoll( bool ragdollOn, r3dPoint3D* hitForce, int BoneID )
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

void obj_Animal::SetAnimalState(int state, bool atStart)
{
	//r3d_assert(AnimalState != state);
	AnimalState = state;
	
	switch(AnimalState)
	{
		default:
			r3dError("unknown Animal state %d", AnimalState);
			return;


		case EAnimalStates::ZState_Idle:
		{
			// select new idle animation
			int aid = 0;
			/*switch(u_random(2)) {
				default:
				case 0:	 break;
				case 1:	aid = AddAnimation("Deer_Idle_Eating"); break;
			}*/
			 switch(u_random(2)) {
			            	default:
							case 0: aid = AddAnimation("Deer_Idle"); break;
							case 1: aid = AddAnimation("Deer_Idle_Eating"); break;
			 }

			int tr = anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.2f);

			// start with randomized frame
			r3dAnimation::r3dAnimInfo* ai = anim_.GetTrack(tr);
			ai->fCurFrame = u_GetRandom(0, (float)ai->pAnim->NumFrames - 1);
			break;
		}
		case EAnimalStates::AState_IdleEating:
		{
			// select new idle animation
			int aid = 0;
			/*switch(u_random(2)) {
				default:
				case 0:	 break;
				case 1:	aid = AddAnimation("Deer_Idle_Eating"); break;
			}*/
			aid = AddAnimation("Deer_Idle_Eating");

			int tr = anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherFade | ANIMFLAG_Looped, 0.0f, 1.0f, 0.2f);

			// start with randomized frame
			r3dAnimation::r3dAnimInfo* ai = anim_.GetTrack(tr);
			ai->fCurFrame = u_GetRandom(0, (float)ai->pAnim->NumFrames - 1);
			break;
		}

		case EAnimalStates::ZState_Walk:
			StartWalkAnim(false);
			break;
		
		case EAnimalStates::AState_Sprint:
			StartWalkAnim(true);
			break;
		case EAnimalStates::AState_SprintJump:
			StartSprintJumpAnim(true);
			break;

		case EAnimalStates::ZState_Dead:
			DoDeath();
			return;
	}
}

void obj_Animal::OnNetPacket(const PKT_S2C_MoveTeleport_s& n)
{
	// set player position and reset cell base, so it'll be updated on next movement
	SetPosition(n.teleport_pos);
	netMover.Teleport(n.teleport_pos);
}

void obj_Animal::OnNetPacket(const PKT_C2C_MoveSetCell_s& n)
{
	netMover.SetCell(n);
}

void obj_Animal::OnNetPacket(const PKT_C2C_MoveRel_s& n)
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
			"Deer_Walk_Backward",
			"Deer_Turn_Left",
			"Deer_Turn_Right",
			"Deer_Walk_Backward",
			"Deer_Walk_Forward"
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

void obj_Animal::OnNetPacket(const PKT_S2C_AnimalSetState_s& n)
{
	SetAnimalState(n.State, false);
}

BOOL obj_Animal::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	R3DPROFILE_FUNCTION("obj_Animal::OnNetReceive");
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
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_AnimalSetState);
		
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

