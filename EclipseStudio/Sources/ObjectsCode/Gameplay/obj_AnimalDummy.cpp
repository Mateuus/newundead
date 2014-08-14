//=========================================================================
//	Module: obj_AnimalDummy.cpp
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"
#include "ui/UIimEdit.h"

#include "obj_AnimalDummy.h"
#include "../AI/r3dPhysSkeleton.h"

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS(obj_AnimalDummy, "obj_AnimalDummy", "Object");
AUTOREGISTER_CLASS(obj_AnimalDummy);

extern r3dCamera gCam;

int		g_animalInstances    = 0;
r3dSkeleton*	g_animalBindSkeleton = NULL;
r3dAnimPool*	g_animalAnimPool     = NULL;

//static	int	g_zombieMaxPartIds[3] = {0};

extern r3dPhysSkeleton* AquireCacheSkeleton();
extern void ReleaseCachedPhysSkeleton(r3dPhysSkeleton* skel);

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

		obj_AnimalDummy *Parent;
	};

//////////////////////////////////////////////////////////////////////////

	struct AnimalMeshesShadowRenderable: public MeshShadowRenderable
	{
		void Init()
		{
			DrawFunc = Draw;
		}

		static void Draw( Renderable* RThis, const r3dCamera& Cam )
		{
			R3DPROFILE_FUNCTION("AnimalMeshesShadowRenderable");
			AnimalMeshesShadowRenderable* This = static_cast< AnimalMeshesShadowRenderable* >( RThis );

			r3dApplyPreparedMeshVSConsts(This->Parent->preparedVSConsts);

			This->Parent->OnPreRender();

			This->SubDrawFunc( RThis, Cam );
		}

		obj_AnimalDummy *Parent;
	};
} // unnamed namespace

static const char* GetZombiePartName(int slot, int idx)
{
	const static char* names[] = {"Head", "Body", "Legs"};
	
	static char buf[MAX_PATH];
	sprintf(buf, "%s\\char_deer_02_doe.scb",
		"Data\\ObjectsDepot\\WZ_Animals",
		names[slot], 
		idx);
		
	return buf;
}

/*static void zombie_ScanForMaxPartIds()
{
	for(int slot=0; slot<3; slot++)
	{
		for(int i=1; i<99; i++) 
		{
			if(! r3dFileExists(GetZombiePartName(slot, i)))
				break;
			g_zombieMaxPartIds[slot] = i;
		}
	}
}*/

obj_AnimalDummy::obj_AnimalDummy()
{
	g_animalInstances++;
	if(g_animalBindSkeleton == NULL) 
	{
		g_animalBindSkeleton = new r3dSkeleton();
		g_animalBindSkeleton->LoadBinary("Data\\ObjectsDepot\\WZ_Animals\\Char_Deer_02_Doe.skl");
		
		g_animalAnimPool = new r3dAnimPool();
		
//		zombie_ScanForMaxPartIds();
	}
	
	m_bEnablePhysics = false;
	
	m_isSerializable = true; // for Sergey to make video

	/*for(int i=0; i<3; i++) {
		partIds[i]  = 1 + u_random(g_zombieMaxPartIds[i]);
		zombieParts[i] = NULL;
	}*/
	
	physSkeleton = NULL;

	sAnimSelected[0] = 0;
}

obj_AnimalDummy::~obj_AnimalDummy()
{
	if(--g_animalInstances == 0)
	{
		SAFE_DELETE(g_animalAnimPool);
		SAFE_DELETE(g_animalBindSkeleton);
	}
}

BOOL obj_AnimalDummy::OnCreate()
{
	parent::OnCreate();

	anim_.Init(g_animalBindSkeleton, g_animalAnimPool, NULL, (DWORD)this);

	for(int i=0; i<3; i++) {
		ReloadZombiePart(i);
	}

	AnimSpeed = 1.0f;
	WalkSpeed = 0.0;
	Walking   = 0;
	ApplyWalkSpeed = 0;
	WalkIdx   = -1;
	SpawnPos  = GetPosition();
	
	r3dBoundBox bbox;
	bbox.InitForExpansion();
	for (int i = 0; i < 3; ++i)
	{
		if (zombieParts[i])
			bbox.ExpandTo(zombieParts[i]->localBBox);
	}
	SetBBoxLocal(bbox);

	if(sAnimSelected[0] == 0)
		r3dscpy(sAnimSelected, random(2) == 0 ? "Deer_Idle_Eating.anm" : "Deer_Idle.anm");
	fAnimListOffset = 0;
	SwitchToSelectedAnim();

	anim_.Update(0.001f, r3dPoint3D(0,0,0), GetTransformMatrix());

	physSkeleton = AquireCacheSkeleton();
 	physSkeleton->linkParent(anim_.GetCurrentSkeleton(), GetTransformMatrix(), this, PHYSCOLL_NETWORKPLAYER) ;
	physSkeleton->SwitchToRagdoll(false);

	return TRUE;
}

BOOL obj_AnimalDummy::Update()
{
	parent::Update();
	
	const float TimePassed = r3dGetFrameTime();

	if(WalkSpeed > 0.01) 
	{
		r3dPoint3D pos = GetPosition();
		pos.z += r3dGetFrameTime() * WalkSpeed * (Walking ? -1 : 0);
		if(pos.z < SpawnPos.z - 4) pos.z = SpawnPos.z + 4;
		SetPosition(pos);
	}
	
#if ENABLE_RAGDOLL
	bool ragdoll = physSkeleton && physSkeleton->IsRagdollMode();
	if (!ragdoll)
#endif
	{
		D3DXMATRIX CharDrawMatrix;
		D3DXMatrixIdentity(&CharDrawMatrix);
		anim_.Update(TimePassed, r3dPoint3D(0,0,0), CharDrawMatrix);
		anim_.Recalc();
	}

	if(physSkeleton)
		physSkeleton->syncAnimation(anim_.GetCurrentSkeleton(), GetTransformMatrix(), anim_);

#if ENABLE_RAGDOLL
	if (ragdoll)
	{
		r3dBoundBox bbox = physSkeleton->getWorldBBox();
		bbox.Org -= GetPosition();
		SetBBoxLocal(bbox);
	}
#endif

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL obj_AnimalDummy::OnDestroy()
{
	parent::OnDestroy();

	for (int i = 0; i < ZOMBIE_BODY_PARTS_COUNT; ++i)
	{
		if(zombieParts[i])
			r3dGOBReleaseMesh(zombieParts[i]);
	}

	ReleaseCachedPhysSkeleton(physSkeleton);

	return TRUE;
}

void obj_AnimalDummy::OnPreRender()
{
	anim_.GetCurrentSkeleton()->SetShaderConstants();
}

void obj_AnimalDummy::AppendRenderables(RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& cam)
{
	for (int i = 0; i < 3; ++i)
	{
		r3dMesh *mesh = zombieParts[i];

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
}

void obj_AnimalDummy::AppendShadowRenderables(RenderArray &rarr, const r3dCamera& cam)
{
	float distSq = (gCam - GetPosition()).LengthSq();
	float dist = sqrtf( distSq );
	UINT32 idist = (UINT32) R3D_MIN( dist * 64.f, (float)0x3fffffff );

	for (int i = 0; i < 3; ++i)
	{
		r3dMesh *mesh = zombieParts[i];
		
		if (!mesh)
			continue;

		uint32_t prevCount = rarr.Count();

		mesh->AppendShadowRenderables( rarr );

		for( uint32_t i = prevCount, e = rarr.Count(); i < e; i ++ )
		{
			AnimalMeshesShadowRenderable& rend = static_cast<AnimalMeshesShadowRenderable&>( rarr[ i ] );

			rend.Init() ;
			rend.SortValue |= idist;
			rend.Parent = this ;
		}
	}
}

int obj_AnimalDummy::AddAnimation(const char* anim)
{
	char buf[MAX_PATH];
	sprintf(buf, "Data\\ObjectsDepot\\WZ_Animals\\Animations\\%s.anm", anim);
	int aid = g_animalAnimPool->Add(anim, buf);
	
	return aid;
}	

void obj_AnimalDummy::SwitchToSelectedAnim()
{
	char buf[MAX_PATH];
	sprintf(buf, "Data\\ObjectsDepot\\WZ_Animals\\Animations\\%s", sAnimSelected);
	int aid = g_animalAnimPool->Add(sAnimSelected, buf);
	anim_.StartAnimation(aid, ANIMFLAG_RemoveOtherNow | ANIMFLAG_Looped, 1.0f, 1.0f, 0.0f);
	
	anim_.AnimTracks[0].fSpeed = AnimSpeed * ((Walking && ApplyWalkSpeed) ? WalkSpeed : 1.0f);
}

void obj_AnimalDummy::ReloadZombiePart(int slot)
{
	const char* buf = GetZombiePartName(slot, partIds[slot]);
	//r3dOutToLog("%d to %s\n", slot, buf);
	zombieParts[slot] = r3dGOBAddMesh(buf);
}

#ifndef FINAL_BUILD
float obj_AnimalDummy::DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected)
{
	float starty = scry;

	//starty += parent::DrawPropertyEditor(scrx, scry, scrw,scrh, startClass, selected );
	if( ! IsParentOrEqual( &ClassData, startClass ) )	
		return starty - scry ;

	/*for(int i=0; i<3; i++)
	{
		const static char* names[] = {"Head", "Body", "Legs"};

		int idx = partIds[i];
		starty += imgui_Value_SliderI(scrx, starty, names[i], &partIds[i], 1, (float)g_zombieMaxPartIds[i], "%d");
		if(idx != partIds[i]) 
		{
			ReloadZombiePart(i);
		}
	}*/

	starty += imgui_Checkbox(scrx, starty, "Walking", &Walking, 1);
	starty += imgui_Value_Slider(scrx, starty, "WalkSpeed", &WalkSpeed, 0, 6, "%f");
	starty += imgui_Checkbox(scrx, starty, "ApplyWalkSpeed", &ApplyWalkSpeed, 1);
	starty += imgui_Value_Slider(scrx, starty, "AnimSpeed", &AnimSpeed, 0, 5, "%f");
	if(anim_.AnimTracks.size() > 0)
		anim_.AnimTracks[0].fSpeed = AnimSpeed * ((Walking && ApplyWalkSpeed) ? WalkSpeed : 1.0f);

	starty += imgui_Static(scrx, starty, " Select Animation");
	{
		float flh = 250;
		std::string sDirFind = "Data\\ObjectsDepot\\WZ_Animals\\Animations\\*.anm";
		if(imgui_FileList(scrx, starty, 360, flh, sDirFind.c_str(), sAnimSelected, &fAnimListOffset, true))
		{
			SwitchToSelectedAnim();
		}
		starty += flh;
	}

	return starty - scry ;
}
#endif

void obj_AnimalDummy::ReadSerializedData(pugi::xml_node& node)
{
	GameObject::ReadSerializedData(node);

	pugi::xml_node zombiedummy = node.child("zombiedummy");
	partIds[0] = zombiedummy.attribute("part0").as_int();
	partIds[1] = zombiedummy.attribute("part1").as_int();
	partIds[2] = zombiedummy.attribute("part2").as_int();
	r3dscpy(sAnimSelected, zombiedummy.attribute("anim").value());
}

void obj_AnimalDummy::WriteSerializedData(pugi::xml_node& node)
{
	GameObject::WriteSerializedData(node);
	pugi::xml_node zombiedummy = node.append_child();
	zombiedummy.set_name("zombiedummy");
	zombiedummy.append_attribute("part0") = partIds[0];
	zombiedummy.append_attribute("part1") = partIds[1];
	zombiedummy.append_attribute("part2") = partIds[2];
	zombiedummy.append_attribute("anim") = sAnimSelected;
}

void obj_AnimalDummy::DoHit()
{
	if(physSkeleton->IsRagdollMode())
		return;

	static const char* anims[] = {
		"Deer_Walk_Backward.anm",
		"Deer_Walk_Forward.anm",
		"Deer_Walk_to_Eating.anm",
		"Deer_Walk_to_Run.anm",
		"Deer_Run.anm"
		};

	const char* hitAnim = anims[u_random(5)];
	
	char buf[MAX_PATH];
	sprintf(buf, "Data\\ObjectsDepot\\WZ_Animals\\Animations\\%s", hitAnim);
	int aid = g_animalAnimPool->Add(hitAnim, buf);
	anim_.StartAnimation(aid, 0, 0.0f, 1.0f, 0.1f);
}
