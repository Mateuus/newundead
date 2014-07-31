#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "ObjectsCode\Effects/obj_ParticleSystem.H"

#include "Editors/LevelEditor.h"

#include "obj_Animals.h"
#include "../AI/r3dPhysSkeleton.h"
#include "DamageLib.h"
#include "AnimalsStates.h"
#include "../AI/AI_Player.H"

IMPLEMENT_CLASS(obj_Animals, "obj_Animals", "Object");
AUTOREGISTER_CLASS(obj_Animals);
static float		_ai_NetPosUpdateDelta = 1.0f / 10;	// "30"
extern r3dPhysSkeleton* AquireCacheSkeleton();
//extern r3dSkeleton*	g_zombieBindSkeleton;
 //r3dSkeleton*	ske;
 //r3dAnimation	ani;
obj_Animals::obj_Animals() 
: m_DestructionParticles( NULL )
, m_DestructionSoundID( -1 )
, m_IsSkeletonShared( 0 )
, netMover(this, 0.1f, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
{
	ObjTypeFlags |= OBJTYPE_Building;

	m_sAnimName[0] = 0;
	m_bAnimated = 0;
	m_bGlobalAnimFolder = 0;
	m_BindSkeleton = NULL;
	//m_BindSkeleton2 = NULL;
	m_bEnablePhysics = true;
}

obj_Animals::~obj_Animals()
{
	if( !m_IsSkeletonShared )
		SAFE_DELETE(m_BindSkeleton);

	//SAFE_DELETE(m_BindSkeleton2);
}


//int __RepositionObjectsOnTerrain = 0;
extern float GetTerrainFollowPosition( const r3dBoundBox& BBox);


void obj_Animals::SetPosition(const r3dPoint3D& pos)
{
	parent::SetPosition( pos );
	r3dPoint3D v = pos;

	if (!MeshLOD[0]) 
		return;
}

#ifndef FINAL_BUILD
float obj_Animals::DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected)
{
	float starty = scry;

	starty += parent::DrawPropertyEditor(scrx, scry, scrw,scrh, startClass, selected );

	if( IsParentOrEqual( &ClassData, startClass ) )	
	{
		starty += imgui_Static(scrx, starty, "Animals properties");

		std::string sDir = FileName.c_str();
		int iPos1 = sDir.find_last_of('\\');
		int iPos2 = sDir.find_last_of('/');

		if ( ( iPos1 < iPos2 && iPos2 != std::string::npos ) || ( iPos1 == std::string::npos ) )
			iPos1 = iPos2;

		if ( iPos1 != std::string::npos )
			sDir.erase(iPos1 + 1,sDir.length() - iPos1 - 1 );

		std::string sDirFind = sDir + "*.sco";

		if(m_Animation.pSkeleton && MeshLOD[0]->IsSkeletal() )
		{
			int useAnim = m_bAnimated;
			starty += imgui_Checkbox(scrx, starty, "Animated", &m_bAnimated, 1);
			if(useAnim != m_bAnimated)
			{
				PropagateChange( &obj_Building::ChangeAnim, selected ) ;
			}

			if(m_bAnimated)
			{
				int check = m_bGlobalAnimFolder ;
				starty += imgui_Checkbox(scrx, starty, "Global Anim Folder", &check, 1);

				PropagateChange( 1, &obj_Building::m_bAnimated, selected ) ;
				PropagateChange( check, &obj_Building::m_bGlobalAnimFolder, this, selected ) ;

				static char sAnimSelected[256] = {0};
				static float fAnimListOffset = 0;

				std::string sDirFind ;

				if( m_bGlobalAnimFolder )
				{
					// try global animation folder
					sDirFind = GLOBAL_ANIM_FOLDER "\\*.anm" ;				
				}
				else
				{
					sDirFind = sDir + "Animations\\*.anm";
				}

				r3dscpy(sAnimSelected, m_sAnimName);
				if ( imgui_FileList (scrx, starty, 360, 200, sDirFind.c_str (), sAnimSelected, &fAnimListOffset, true ) )
				{
					r3dscpy(m_sAnimName, sAnimSelected);
					PropagateChange( &obj_Building::ChangeAnim, selected ) ;
				}
				starty += 200;
			}
		}

		if( m_HitPoints > 0 )
		{
			if ( imgui_Button ( scrx, starty, 360, 20, "Destroy" ) )
			{
				PropagateChange( &obj_Building::DestroyBuilding, selected ) ;
			}
		}
		else
		{
			if ( imgui_Button ( scrx, starty, 360, 20, "Ressurect" ) )
			{
				PropagateChange( &obj_Building::FixBuilding, selected ) ;
			}
		}

		starty += 22.f ;

		if( selected.Count() <= 1 )
		{
			if ( imgui_Button ( scrx, starty, 360, 20, m_pDamageLibEntry ? "To Destruction Params" : "Create Destruction Params" ) )
			{
				LevelEditor.ToDamageLib( GetMeshLibKey().c_str() );
			}
		}

		starty += 22.0f;
	}
	return starty - scry ;
}
#endif


#ifndef FINAL_BUILD
void
obj_Animals::DrawSelected( const r3dCamera& Cam, eRenderStageID DrawState )
{
	if( m_bAnimated )
	{
		// do nothing for now..
	}
	else
	{
		MeshGameObject::DrawSelected( Cam, D3DXVECTOR4(0.0f, 1.0f, 0.0f, 0.223f) ) ;
	}
}
#endif

BOOL obj_Animals::OnCreate()
{
	//setSkipOcclusionCheck( true );
	//ObjFlags |= OBJFLAG_PlayerCollisionOnly;
	//physid = AcquirePlayerObstacle(GetPosition());
	//ReadPhysicsConfig();
	PhysicsConfig.group = PHYSCOLL_NETWORKPLAYER;
	PhysicsConfig.type = PHYSICS_TYPE_MESH;
	PhysicsConfig.isKinematic = true;
	PhysicsConfig.isDynamic = true;
    PhysicsConfig.mass = 1;
    PhysicsConfig.needExplicitCollisionMesh = true;
	sprintf(m_sAnimName,"Deer_Idle");
	animalstate = EAnimalsStates::AState_Idie;
	ChangeAnim(ANIMFLAG_Looped);
	//pxSkeleton = new r3dPhysSkeleton("Data\\ObjectsDepot\\WZ_Animals\\char_deer_01.skl");
	//pxSkeleton = AquireCacheSkeleton();
	//if (pxSkeleton->loadSkeleton("Data\\ObjectsDepot\\WZ_Animals\\char_deer_01.skl")
    // r3dOutToLog("loadSkeleton Loaded!\n");

 	//pxSkeleton->linkParent(m_Animation.GetCurrentSkeleton(), GetTransformMatrix(), this, PHYSCOLL_NETWORKPLAYER) ;
	//pxSkeleton->TogglePhysicsSimulation(true);
	//pxSkeleton->SwitchToRagdoll(false);
	//physObj = BasePhysicsObject::CreateDynamicObject(PhysicsConfig, this);
	/*PxRigidDynamic* actor2;
	char cookedMeshFilename[256];
	r3dscpy(cookedMeshFilename, FileName.c_str());
	int len = strlen(cookedMeshFilename);
	r3dscpy(&cookedMeshFilename[len-3], "mpx");

		if(!r3dFileExists(cookedMeshFilename))
		{
#ifdef WO_SERVER
			r3dOutToLog("!!!! no cooked mesh for '%s', server cannot build those!\n", cookedMeshFilename);
			return NULL;
#endif
	}

	PxTriangleMeshGeometry geom(g_pPhysicsWorld->getCookedMesh(cookedMeshFilename));
	actor = actor2->createShape(geom, params.requireNoBounceMaterial?*(g_pPhysicsWorld->noBounceMaterial):*(g_pPhysicsWorld->defaultMaterial))
    g_pPhysicsWorld->AddActor(*actor);*/
	return parent::OnCreate();
}

/*static*/
void
obj_Animals::LoadSkeleton( const char* baseMeshFName, r3dSkeleton** oSkeleton, int * oIsSkeletonShared , const char* len)
{
	const char* fname = baseMeshFName ;

	char skelname[MAX_PATH];
	r3dscpy(skelname, fname);
	r3dscpy(&skelname[strlen(skelname)-3], len);

	bool loadSkel = false ;

	r3dSkeleton* bindSkeleton = 0 ;

	if( oIsSkeletonShared )
		*oIsSkeletonShared = 0 ;

	if(r3d_access(skelname, 0) == 0)
	{
		bindSkeleton = new r3dSkeleton();
		bindSkeleton->LoadBinary(skelname);
	}
#ifndef FINAL_BUILD // required only in editor, to place armor in editor and for armor to automatically load default skeleton. not needed in the game
	else
	{
		bindSkeleton		= GetDefaultSkeleton( fname );

		if( oIsSkeletonShared )
			*oIsSkeletonShared	= 1 ;
	}
#endif

	*oSkeleton = bindSkeleton ;
}

BOOL obj_Animals::Load(const char* fname)
{
	if(!parent::Load(fname))
		return FALSE;

	// try to load default skeleton
	if( MeshLOD[0]->IsSkeletal() )
	{
		LoadSkeleton( fname, &m_BindSkeleton, &m_IsSkeletonShared,"skl" ) ;
		//LoadSkeleton( fname, &ske, NULL,"skl2" ) ;
		//LoadSkeleton2( fname, &m_BindSkeleton2, NULL ) ;

		//m_BindSkeleton->LoadBinary("Data\\ObjectsDepot\\WZ_Animals\\char_deer_01.skl");
		if( m_BindSkeleton )
		{
			m_Animation.Init(m_BindSkeleton, &m_AnimPool);
			m_Animation.Update(0.0f, r3dPoint3D(0, 0, 0), mTransform);
		}
		//ani.Init(ske, NULL);

			//if( m_BindSkeleton2 )
	//	{
	//		m_Animation2.Init(m_BindSkeleton2, NULL);
	//		m_Animation2.Update(0.0f, r3dPoint3D(0, 0, 0), mTransform);
	//	}
	}


	if( m_pDamageLibEntry )
	{
		if( m_pDamageLibEntry->HasSound )
		{
			m_DestructionSoundID = SoundSys.GetEventIDByPath( m_pDamageLibEntry->SoundName.c_str() );
		}
	}

	return TRUE;
}

BOOL obj_Animals::Update()
{
	//bool ragdoll = pxSkeleton && pxSkeleton->IsRagdollMode();
	//if (!ragdoll)
	//{
	if( m_bAnimated || MeshLOD[ 0 ]->IsSkeletal() && m_BindSkeleton )
		m_Animation.Update(r3dGetAveragedFrameTime(), r3dPoint3D(0, 0, 0), mTransform);
	//}
	//if(pxSkeleton)
	//{
		//r3dAnimation ani2 = ani;
	//	pxSkeleton->syncAnimation(ani.GetCurrentSkeleton(), GetTransformMatrix(), ani);
		//r3dOutToLog("syncAnimation\n");
	//}
    if( m_DestructionParticles )
    {
        if( !m_DestructionParticles->isActive() )
        {
            m_DestructionParticles = 0 ;
        }
    }

	//if (!bDead)
	//{
	//UpdateAnimalsObstacle(physid, GetPosition());
	PhysicsObject->SetPosition(GetPosition());
	//}

	return parent::Update();
}
void obj_Animals::ChangeAnim(DWORD Flags)
{
	if(!m_Animation.pSkeleton)
		return;
	m_Animation.StopAll();

	char buf[MAX_PATH];
	sprintf(buf, "Data\\Animations5\\%s.anm", m_sAnimName);
	//int aid = g_zombieAnimPool->Add(anim, buf);

	int aid = m_AnimPool.Add(m_sAnimName, buf);
	if(aid == -1)
	{
		m_sAnimName[0] = 0;
		m_bAnimated = 0;
		return;
	}

	int tr = m_Animation.StartAnimation(aid, Flags, 0.0f, 0.0f, 0.0f);
	// start with randomized frame
	r3dAnimation::r3dAnimInfo* ai = m_Animation.GetTrack(tr);
	//ai->fSpeed = 5.0f;
	ai->fCurFrame = u_GetRandom(0, (float)ai->pAnim->NumFrames - 1);
}

void
obj_Animals::DestroyBuilding()
{
	this->m_HitPoints = 0 ;

	if( m_pDamageLibEntry )
	{
		if( m_pDamageLibEntry->HasParicles && !m_DestructionParticles )
		{
			m_DestructionParticles = (obj_ParticleSystem*)srv_CreateGameObject("obj_ParticleSystem", m_pDamageLibEntry->ParticleName.c_str(), GetPosition() );
		}
		else
		{
			if( m_DestructionParticles && m_DestructionParticles->isActive() )
				m_DestructionParticles->Restart();
		}

		if( m_pDamageLibEntry->HasSound && m_DestructionSoundID != -1 )
		{
			SoundSys.Play( m_DestructionSoundID, GetPosition() );
		}
	}
}

void
obj_Animals::FixBuilding()
{
	m_HitPoints = 0XA107 ;

	if( m_pDamageLibEntry )
	{
		m_HitPoints = m_pDamageLibEntry->HitPoints ;
	}

	if( m_DestructionParticles )
	{
		GameWorld().DeleteObject( m_DestructionParticles );
		m_DestructionParticles = 0 ;
	}
}

GameObject *obj_Animals::Clone ()
{
	obj_Animals * pNew = (obj_Animals*)srv_CreateGameObject("obj_Animals", FileName.c_str(), GetPosition () );

	PostCloneParamsCopy(pNew);

	return pNew;
}

//////////////////////////////////////////////////////////////////////////

void obj_Animals::PostCloneParamsCopy(obj_Animals *pNew)
{
	if (!pNew) return;
	pNew->SetRotationVector(GetRotationVector());

	r3dscpy(pNew->m_sAnimName, m_sAnimName);
	pNew->m_bAnimated = m_bAnimated;
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_Animals::GetObjStat ( char * sStr, int iLen)
{
	char sAddStr [MAX_PATH];
	sAddStr[0] = 0;
	if (MeshGameObject::GetObjStat (sStr, iLen))
		r3dscpy(sAddStr, sStr);

	return TRUE;
}

void obj_Animals::ReadSerializedData(pugi::xml_node& node)
{
	parent::ReadSerializedData(node);
	pugi::xml_node buildingNode = node.child("Animals");

	r3dscpy(m_sAnimName, buildingNode.attribute("AnimName").value());
	m_bAnimated = buildingNode.attribute("Animated").as_int();

	pugi::xml_attribute gloanfol = buildingNode.attribute("GlobalAnimFolder") ;

	if( !gloanfol.empty() )
	{
		m_bGlobalAnimFolder = !!gloanfol.as_int();
	}
}

void obj_Animals::WriteSerializedData(pugi::xml_node& node)
{
	parent::WriteSerializedData(node);
	pugi::xml_node buildingNode = node.append_child();
	buildingNode.set_name("Animals");

	if(m_sAnimName[0])
	{
		buildingNode.append_attribute("AnimName") = m_sAnimName;
		buildingNode.append_attribute("Animated") = m_bAnimated;

		// don't spam it..
		if( m_bGlobalAnimFolder )
		{
			buildingNode.append_attribute("GlobalAnimFolder") = m_bGlobalAnimFolder;
		}
	}
}

/*virtual*/
void
obj_Animals::UpdateDestructionData()
{
	MeshGameObject::UpdateDestructionData();

	if( !m_pDamageLibEntry )
	{
		if( m_DestructionParticles )
		{
			GameWorld().DeleteObject( m_DestructionParticles );
			m_DestructionParticles = NULL ;
		}

		m_DestructionSoundID = -1 ;
	}
	else
	{
		if( !m_pDamageLibEntry->HasParicles  )
		{
			GameWorld().DeleteObject( m_DestructionParticles );
			m_DestructionParticles = NULL ;
		}

		if( !m_pDamageLibEntry->HasSound )
		{
			m_DestructionSoundID = -1 ;
		}
		else
		{
			m_DestructionSoundID = SoundSys.GetEventIDByPath( m_pDamageLibEntry->SoundName.c_str() );
		}
	}
}

int obj_Animals::IsStatic()
{
	return true;
}

bool obj_Animals::NeedDrawAnimated(const r3dCamera& Cam)
{
	float distSq = (Cam - GetPosition()).LengthSq();
	int meshLodIndex = ChoseMeshLOD( distSq, MeshLOD );

	if( MeshLOD[ meshLodIndex ]->IsSkeletal() && m_BindSkeleton )
		return true ;

	if(!m_bAnimated)
		return false;

	return true;
}

void obj_Animals::DrawAnimated(const r3dCamera& Cam, bool shadow_pass)
{
	r3d_assert(m_Animation.pSkeleton);

	// recalc animation if it dirty
	m_Animation.Recalc();

	int oldVsId = r3dRenderer->GetCurrentVertexShaderIdx();

	m_Animation.pSkeleton->SetShaderConstants();

	r3dMesh* mesh = MeshLOD[0];
	r3d_assert( mesh->IsSkeletal() );

	D3DXMATRIX world;
	r3dPoint3D pos = GetPosition();
	D3DXMatrixTranslation(&world, pos.x, pos.y, pos.z );
	r3dMeshSetVSConsts( world, NULL, NULL );

	if(!shadow_pass)
	{
		mesh->DrawMeshDeferred( m_ObjectColor, 0 );
	}
	else
	{
		mesh->DrawMeshShadows();
	}

	r3dRenderer->SetVertexShader(oldVsId);
}

struct BuildingAniDeferredRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		BuildingAniDeferredRenderable* This = static_cast<BuildingAniDeferredRenderable*>( RThis );
		This->Parent->DrawAnimated(Cam, false);
	}

	obj_Animals* Parent;
};

struct BuildingAniShadowRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		BuildingAniShadowRenderable* This = static_cast<BuildingAniShadowRenderable*>( RThis );
		This->Parent->DrawAnimated(Cam, true);
	}

	obj_Animals* Parent;
};

struct BuildingAniDebugRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		BuildingAniDebugRenderable* This = static_cast<BuildingAniDebugRenderable*>( RThis );
		r3dRenderer->SetTex(NULL);
		r3dRenderer->SetMaterial(NULL);
		r3dRenderer->SetRenderingMode(R3D_BLEND_NOALPHA | R3D_BLEND_NZ);

		This->Parent->m_Animation.pSkeleton->DrawSkeleton(Cam, This->Parent->GetPosition());
		//This->Parent->m_Animation2.pSkeleton->DrawSkeleton(Cam, This->Parent->GetPosition());
	}

	obj_Animals* Parent;
};

#define	RENDERABLE_BUILDING_SORT_VALUE (31*RENDERABLE_USER_SORT_VALUE)

void obj_Animals::AppendShadowRenderables( RenderArray & rarr, const r3dCamera& Cam )
{
	// [from MeshGameObject code] always use main camera to determine shadow LOD
	if(!NeedDrawAnimated(gCam))
	{
		parent::AppendShadowRenderables(rarr, Cam);
		return;
	}

	if( !gDisableDynamicObjectShadows )
	{
		BuildingAniShadowRenderable rend;
		rend.Init();
		rend.Parent	= this;
		rend.SortValue	= RENDERABLE_BUILDING_SORT_VALUE;

		rarr.PushBack( rend );
	}
}
BOOL obj_Animals::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	R3DPROFILE_FUNCTION("obj_Animals::OnNetReceive");
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
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_AnimalsMove);
	}

	return FALSE;
}
void obj_Animals::OnNetPacket(const PKT_S2C_AnimalsMove_s& n)
{
	SetPosition(n.pos);
	//PhysicsObject->SetPosition(n.pos);
	SetRotationVector(n.angle);
	if (n.state != animalstate)
	{
		//r3dOutToLog("Change State Animals %d\n",n.state);
		animalstate = n.state;
		if (animalstate == EAnimalsStates::AState_Idie)
		{
			sprintf(m_sAnimName,"Deer_Idle");
			ChangeAnim(ANIMFLAG_Looped);
		}
		else if (animalstate == EAnimalsStates::AState_Walk)
		{
			sprintf(m_sAnimName,"Deer_Walk_Forward");
			ChangeAnim(ANIMFLAG_Looped);
		}
		else if (animalstate == EAnimalsStates::AState_JumpSprint)
		{
		    sprintf(m_sAnimName,"Deer_JumpSprint");
			ChangeAnim(ANIMFLAG_Looped);
		}
		else if (animalstate == EAnimalsStates::AState_Dead)
		{
			m_Animation.StopAll();
            sprintf(m_sAnimName,"Deer_Death_Fullbody");
			ChangeAnim(ANIMFLAG_PauseOnEnd);
			bDead = true;
			//ReleasePlayerObstacle(&physid); // clear obstacle when die
			// clear phys colision
			//PhysicsObject->SetPosition(r3dPoint3D(0,0,0));
			//g_pPhysicsWorld->RemoveActor(*PhysicsObject->getPhysicsActor());
            //PhysicsObject = NULL;
		}
	}
}
void obj_Animals::OnNetPacket(const PKT_S2C_MoveTeleport_s& n)
{
	// set player position and reset cell base, so it'll be updated on next movement
	SetPosition(n.teleport_pos);
	netMover.Teleport(n.teleport_pos);
}

void obj_Animals::OnNetPacket(const PKT_C2C_MoveSetCell_s& n)
{
	netMover.SetCell(n);
}

void obj_Animals::OnNetPacket(const PKT_C2C_MoveRel_s& n)
{
	r3dOutToLog("AnimalMoveRel\n");
	const CNetCellMover::moveData_s& md = netMover.DecodeMove(n);

	SetRotationVector(r3dVector(md.turnAngle, 0, 0));

	// we are ignoring md.state & md.bendAngle for now

	// calc velocity to reach position on time for next update
	r3dPoint3D vel = netMover.GetVelocityToNetTarget(
		GetPosition(),
		3.0f,
		1.0f);
	SetVelocity(vel);
}
void obj_Animals::AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam )
{
	if(!NeedDrawAnimated(Cam))
	{
		parent::AppendRenderables(render_arrays, Cam);
		return;
	}

	// deferred
	{
		BuildingAniDeferredRenderable rend;
		rend.Init();
		rend.Parent	= this;
		rend.SortValue	= RENDERABLE_BUILDING_SORT_VALUE;

		render_arrays[ rsFillGBuffer ].PushBack( rend );
	}

	/*// skeleton draw
	{
	BuildingAniDebugRenderable rend;
	rend.Init();
	rend.Parent	= this;
	rend.SortValue	= RENDERABLE_BUILDING_SORT_VALUE;

	render_arrays[ rsDrawComposite1 ].PushBack( rend );
	}*/
}


