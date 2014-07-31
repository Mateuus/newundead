//
//
// Object Manager
//
//
#include "r3dPCH.h"
#include "r3d.h"

#include "GameObj.h"
#include "ObjManag.h"
#include "../../EclipseStudio/Sources/rendering/Deffered/RenderDeffered.h"
#include "../TrueNature/Sun.h"

#include "CollMain.hpp"

#include "ObjectsCode/World/Lamp.h"
#include "ObjectsCode/World/obj_road.h"
#include "obj_Mesh.h"
#include "obj_Dummy.h"
#include "..\..\Eternity\Include\ParallelQuickSort.h"


#include "..\TrueNature\Terrain.h"
#include "..\TrueNature2\Terrain2.h"

#include "JobChief.h"

#include "../../EclipseStudio/Sources/ObjectsCode/weapons/BulletShellManager.h"
#include "../../EclipseStudio/Sources/Editors/CollectionElementProxyObject.h"
#include "../../EclipseStudio/Sources/Editors/CollectionsManager.h"
#include "obj_Apex.hpp"

#ifndef WO_SERVER
#include "../../EclipseStudio/Sources/ObjectsCode/Gameplay/obj_ItemSpawnPoint.h"
#endif

//VMPROTECT_BeginMutation(GameWorld);
bool gDestroyingWorld = false;


void ObjectManagerResourceHelper::D3DReleaseResource()
{
	GameWorld().OnResetDevice();
}

//////////////////////////////////////////////////////////////////////////

void PrecalculateWorldMatrices(void* Data, size_t ItemStart, size_t ItemCount)
{
	if (!Data) return;
	draw_s *d = reinterpret_cast<draw_s*>(Data);
	draw_s *dStart = d + ItemStart;
	draw_s *dEnd = dStart + ItemCount;
	for (draw_s *i = dStart; i < dEnd; ++i)
	{
		if (i)
		{
			GameObject *o = i->obj;
			r3d_assert(!IsBadReadPtr(o, sizeof(GameObject)));
			if (o)
				o->PrecalculateMatrices();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void PrecalculateShadowWorldMatrices(void* Data, size_t ItemStart, size_t ItemCount)
{
	if (!Data) return;
	draw_s *d = reinterpret_cast<draw_s*>(Data);
	draw_s *dStart = d + ItemStart;
	draw_s *dEnd = dStart + ItemCount;
	for (draw_s *i = dStart; i < dEnd; ++i)
	{
		if (i)
		{
			GameObject *o = i->obj;
			r3d_assert(!IsBadReadPtr(o, sizeof(GameObject)));
			if (o && (o->ObjFlags & DISABLE_SHADOWS_FLAGS ) == 0)
				o->PrecalculateMatrices();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CastRayToSpawnPoints(GameObject *obj, const r3dPoint3D &vStart, const r3dPoint3D &vRay, float RayLen, float &dst)
{
#ifndef WO_SERVER
	if (obj->isObjType(OBJTYPE_ItemSpawnPoint))
	{
		obj_ItemSpawnPoint *sp = static_cast<obj_ItemSpawnPoint*>(obj);
		bool found = false;
		for (size_t i = 0; i < sp->m_SpawnPointsV.size(); ++i)
		{
			r3dBoundBox spawnPtBB = sp->m_SpawnPointsV[i].GetDebugBBox();
			if (spawnPtBB.ContainsRay(vStart, vRay, RayLen, &dst))
			{
				found = true;
				r3dPoint3D collision;
				if (terra_FindIntersection(vStart, vStart + vRay * RayLen, collision, 20000))
				{
					dst = (collision - vStart).Length() - 0.01f;
				}
			}
		}
		return found;
	}
#endif //WO_SERVER
	return false;
}

//////////////////////////////////////////////////////////////////////////

#define COLL_DEBUG if(0) r3dOutToLog(

GameObject *srv_CreateGameObject(int class_type, const char* load_name, const r3dPoint3D& Pos, long flags, void *data)
{
	r3d_assert(gDestroyingWorld==false); // no object creation when destroying game world

	GameObject	*obj;

	obj         = (GameObject *)AObjectTable_CreateObject(class_type);

	if(!(flags & OBJ_CREATE_SKIP_LOAD)) {
		if(!obj->Load(load_name)) {
			r3dOutToLog("WARNING: Object %s can't be loaded!\n", load_name);
			obj->setActiveFlag(0);
		}
	}

	if(!(flags & OBJ_CREATE_SKIP_POS))
		obj->SetPosition(Pos);

	obj->ObjFlags |= OBJFLAG_JustCreated;

	//
	// add to world.
	//

	if(flags & OBJ_CREATE_LOCAL) {
		;
#ifdef WO_SERVER
	} else if(obj->ObjFlags & OBJFLAG_AddToDummyWorld) {
		extern ObjectManager ServerDummyWorld;
		ServerDummyWorld.AddObject(obj);
#endif
	} else {
		GameWorld().AddObject(obj);
	}

	return obj;
}

GameObject *srv_CreateGameObject(const char* class_name, const char* load_name, const r3dPoint3D& Pos, long flags, void *data)
{
	int class_id = AObjectTable_GetClassID(class_name, "Object");
	if(class_id == -1)
	{
		r3dOutToLog("srv_CreateGameObject: class %s isn't present", class_name);
		return NULL;
	}

	return srv_CreateGameObject(class_id, load_name, Pos, flags, data);
}

// global gameworld accessors
#if USE_VMPROTECT
#pragma optimize("gff", off)
#endif
static r3dSec_type<ObjectManager*, 0xAC8DA94A> g_pGameWorld = NULL;
void GameWorld_Create()
{
	VMPROTECT_BeginVirtualization("GameWorld_Create()");
	r3d_assert(g_pGameWorld == NULL);
	g_pGameWorld = new ObjectManager();
	VMPROTECT_End();
}

void GameWorld_Destroy()
{
	VMPROTECT_BeginVirtualization("GameWorld_Destroy()");
	SAFE_DELETE(g_pGameWorld);
	VMPROTECT_End();
}

ObjectManager& GameWorld()
{
	VMPROTECT_BeginVirtualization("GameWorld()444");
	r3d_assert(g_pGameWorld);
	return *g_pGameWorld;
	VMPROTECT_End();
}
#if USE_VMPROTECT
#pragma optimize("g", on)
#endif

//ObjectManager
ObjectManager::ObjectManager()
{
	pObjectAddEvent = NULL;
	pObjectDeleteEvent = NULL;
	bInited = 0;
	m_FrameId = 0;
	m_pRootBox = 0;
#ifndef WO_SERVER
	m_BulletMngr = 0;
#endif

	LastStaticUpdateIdx = 0;

	TransparentShadowCasterCount = 0;

	JustLoaded = 1;
}

ObjectManager::~ObjectManager()
{
	Destroy();
}

static unsigned int WINAPI UThread( void * param )
{
	ObjectManager* GW = (ObjectManager*)param;
	if (GW)
	{
		while (1)
		{
			if (GW->TemporaryObjects.Count() && GW->ReadyToUpdate)
			{

			}
		}
	}
	return 0;
}

int ObjectManager::Init(int _MaxObjects, int _MaxStaticObjects)
{
	r3d_assert(!bInited);
	if(bInited) return 0;
	bInited        = 1;

	ReadyToUpdate = false;
	//UpdateThread = (HANDLE)_beginthreadex( NULL, 0, UThread, (void*)this, 0, NULL );
	TemporaryObjects.Reserve( 1024*1024 );
	TObjects.Reserve( 1024*1024 );

	m_pRootBox = new SceneBox();
	m_ResourceHelper = 0;
#ifndef WO_SERVER
	m_ResourceHelper = new ObjectManagerResourceHelper;
#endif

	MaxObjects           = _MaxObjects;
	MaxStaticObjects     = _MaxStaticObjects;
	LastFreeObject       = 0;
	NumObjects           = 0;
	NumStaticObjects     = 0;
	CurObjID             = 1;

	m_MinimapOrigin.Assign(0,0,0);
	m_MinimapSize.Assign(100,1,100);

	pObjectArray   = new GameObject* [MaxObjects];
	pFirstObject   = NULL;
	pLastObject    = NULL;

	memset(pObjectArray, 0, sizeof(GameObject*)*MaxObjects);

	pStaticObjectArray = new GameObject* [MaxStaticObjects];

	memset(pStaticObjectArray, 0, sizeof(GameObject*)*MaxStaticObjects);

	TransparentShadowCasterCount = 0 ;

#ifndef WO_SERVER
#if !R3D_NO_BULLET_MGR
	m_BulletMngr = new BulletShellMngr();
#endif
#endif

	InitializeCriticalSection( &m_CS ) ;
	return 1;
}

int ObjectManager::Destroy()
{
	int	i;

	if(!bInited)
		return 1;

#ifndef FINAL_BUILD
#ifndef WO_SERVER
	CleanOrphanedRoadFiles();
#endif	
#endif

	SAFE_DELETE(m_ResourceHelper);

	gDestroyingWorld = true;

#ifndef WO_SERVER
#if !R3D_NO_BULLET_MGR
	SAFE_DELETE(m_BulletMngr);
#endif
#endif

	// do delete in two passes. firstly call OnDestroy on all objects, and then only call delete
	// otherwise some objects have pointers to other objects and they might point to dead pointer and fuck up memory
	for(i=0; i<MaxObjects; i++) 
		if(pObjectArray[i])
			DeleteObject(pObjectArray[i], false);

	for(i=0; i<MaxObjects; i++) 
	{
		if(pObjectArray[i])
		{
			delete pObjectArray[i];
			pObjectArray[i] = NULL;
			NumObjects = NumObjects-1;
		}
	}

	for(i=0; i<MaxStaticObjects; i++) 
		if(pStaticObjectArray[i])
			DeleteObject(pStaticObjectArray[i], false);

	for(i=0; i<MaxStaticObjects; i++) 
	{
		if(pStaticObjectArray[i])
		{
			delete pStaticObjectArray[i];
			pStaticObjectArray[i] = NULL;
			NumStaticObjects = NumStaticObjects-1;
		}
	}

	gSkipOcclusionCheckGameObjects.clear();

	extern std::list<SceneBox*> PrevFrameQueryList;
	while(!PrevFrameQueryList.empty())
		PrevFrameQueryList.pop_front();

	delete[] pObjectArray;
	delete[] pStaticObjectArray;

	MaxObjects = 0;
	bInited    = 0;

	delete m_pRootBox;
	m_pRootBox = 0;

	gDestroyingWorld = false;

	return 1;
}

void ObjectManager::SetAddObjectEvent (ObjectManager::GameObjEvent_fn pEvent)
{
	r3d_assert ( !pObjectAddEvent || !pEvent ); 
	pObjectAddEvent = pEvent;
}

void ObjectManager::SetDeleteObjectEvent (ObjectManager::GameObjEvent_fn pEvent)
{
	r3d_assert ( !pObjectDeleteEvent || !pEvent ); 
	pObjectDeleteEvent = pEvent;
}

int ObjectManager::AddObject(GameObject *obj)
{
	int indexForAdding = -1;

	if( obj->IsStatic() )
	{
		indexForAdding = NumStaticObjects;
	}
	else
	{
		if( LastFreeObject == -1 )
		{
			// can't add object - table is full
			r3dError( "failed to add object - increase ObjectManager number of objects" );
		}

		indexForAdding = LastFreeObject;
	}

	// Create Object ID:
	// low word is index in ObjectManager, high is obj ID

	UINT staticBit = obj->IsStatic() ? 0x80000000 : 0;

	obj->ID.set(((CurObjID & 0xFFFF) << 16) | indexForAdding | staticBit );

	CurObjID = CurObjID + 1;

	if( CurObjID >= 0x7fff )
		CurObjID = 0;

	//r3dOutToLog ("OBJCREATE %s  %d\n", obj->Name.c_str(), obj->ID);

	if( obj->IsStatic() )
	{
		pStaticObjectArray[ NumStaticObjects ] = obj;
		NumStaticObjects = NumStaticObjects + 1;
	}
	else
	{
		pObjectArray[ LastFreeObject ] = obj;
		NumObjects = NumObjects + 1;
	}

	// add object to linked list
	{
		VMPROTECT_BeginMutation("ObjectManager::AddObject-LList");

		if( !obj->IsStatic() )
		{
			if(pFirstObject == NULL) 
			{
				pFirstObject = obj;
			}

			if(pLastObject) 
			{
				r3d_assert(pLastObject->pNextObject == NULL);
				r3d_assert(obj->pPrevObject == NULL);
				r3d_assert(obj->pNextObject == NULL);

				pLastObject->pNextObject = obj;
				obj->pPrevObject = pLastObject;
			}
			pLastObject = obj;
		}

		VMPROTECT_End();
	}

#ifndef WO_SERVER
	m_pRootBox->Add( obj, false );
#endif

	AddToTransparentShadowCasters( obj );

	if ( pObjectAddEvent ) pObjectAddEvent ( obj );

	if( !obj->IsStatic() )
	{
		int found = 0;

		// detect next free object
		int i;
		for(i = LastFreeObject + 1; i < MaxObjects; i++)
		{
			if( pObjectArray[i] == NULL )
			{
				LastFreeObject = i;
				return 1;
			}
		}

		for(i = 0; i < LastFreeObject; i++)
		{
			if( pObjectArray[i] == NULL )
			{
				LastFreeObject = i;
				return 1;
			}
		}
	}



	return 1;
}

int ObjectManager::DeleteObject(GameObject* obj, bool call_delete)
{
	if ( ! obj )
		return 0;

	int n = (obj->ID.get() & 0xFFFF);

	if( obj->IsStatic() )
	{
		r3d_assert(n >= 0 && n < MaxStaticObjects);
	}
	else
	{
		r3d_assert(n >= 0 && n < MaxObjects);
	}

	if(obj->m_SceneBox)
		obj->m_SceneBox->Remove(obj);

	if( !obj->IsStatic() )
	{
		LastFreeObject = n;
	}

	RemoveFromTransparentShadowCasters( obj );

	if ( pObjectDeleteEvent ) pObjectDeleteEvent ( obj );

	// ptumik: firstly remove from the list, then call OnDestroy, and then delete object
	// otherwise from ondestroy object is still in gameworld

	// remove from linked list
	{
		VMPROTECT_BeginMutation("ObjectManager::DeleteObject-LList");

		if( !obj->IsStatic() )
		{
			if(pFirstObject == obj)
				pFirstObject = obj->pNextObject;
			if(pLastObject == obj)
				pLastObject = obj->pPrevObject;
			if(obj->pPrevObject) {
				r3d_assert(obj->pPrevObject->pNextObject == obj);
				obj->pPrevObject->pNextObject = obj->pNextObject;
			}
			if(obj->pNextObject) {
				r3d_assert(obj->pNextObject->pPrevObject == obj);
				obj->pNextObject->pPrevObject = obj->pPrevObject;
			}
			obj->pNextObject = NULL;
			obj->pPrevObject = NULL;
		}

		VMPROTECT_End();
	}

	obj->OnDestroy();

	if( call_delete )
	{
		if( obj->IsStatic() )
		{
			int idx = n;
			for( ; idx >= 0; idx -- )
			{
				if( pStaticObjectArray[ idx ] == obj )
					break;
			}

			r3d_assert( idx >= 0 );

			memmove( pStaticObjectArray + idx, pStaticObjectArray + idx + 1, ( NumStaticObjects - idx - 1 ) * sizeof( GameObject * ) );
			delete obj;
			NumStaticObjects = NumStaticObjects-1;
			pStaticObjectArray[ NumStaticObjects] = NULL;
		}
		else
		{
			pObjectArray[n] = NULL;
			delete obj;
			NumObjects = NumObjects-1;
		}
	}

	return 1;
}

//--------------------------------------------------------------------------------------------------------
void ObjectManager::LinkObject( GameObject* obj )
{
	r3d_assert( obj );
	obj->ObjFlags &= ~OBJFLAG_Removed;

	if ( obj->Class->Name == "obj_LightHelper" )
	{
		obj_LightHelper * pLight = ( obj_LightHelper * )obj;
		WorldLightSystem.Add( &pLight->LT );
	}
}

//--------------------------------------------------------------------------------------------------------
void ObjectManager::UnlinkObject( GameObject* obj )
{
	r3d_assert( obj );
	obj->ObjFlags |= OBJFLAG_Removed;

	if ( obj->Class->Name == "obj_LightHelper" )
	{
		obj_LightHelper * pLight = ( obj_LightHelper * )obj;
		WorldLightSystem.Remove( &pLight->LT );
	}
}



int ObjectManager::SetDrawingOrder(GameObject *obj, int order)
{
	obj->DrawOrder = order;
	return 1;
}

GameObject* ObjectManager::GetObject(const char* name)
{
	for(GameObject* obj = pFirstObject; obj; obj = obj->pNextObject)
	{
		if(obj->Name == name)
			return obj;
	}

	for( int i = 0, e = NumStaticObjects; i < e; i ++ )
	{
		GameObject* obj = pStaticObjectArray[ i ];

		if( obj->Name == name )
			return obj;
	}

	return NULL;
}

GameObject* ObjectManager::GetObjectByHash(uint32_t hash)
{
	if(hash == 0)
		return NULL;

	for(GameObject* obj = pFirstObject; obj; obj = obj->pNextObject)
	{
		if(obj->GetHashID()==hash)
			return obj;
	}

	for( int i = 0, e = NumStaticObjects; i < e; i ++ )
	{
		GameObject* obj = pStaticObjectArray[ i ];

		if(obj->GetHashID()==hash)
			return obj;
	}

	return NULL;
}

GameObject* ObjectManager::GetObject(gobjid_t ID)
{
	if(ID == invalidGameObjectID)
		return NULL;

	r3d_assert(ID.get() != -1);

	int isStatic = ID.get() & OBJECTMANAGER_STATICBIT;

	int n = (ID.get() & 0x7FFF);

	if( isStatic )
	{
		r3d_assert(n >= 0 && n < MaxStaticObjects);

		int idx = n;
		for( ; idx >= 0; idx -- )
		{
			GameObject* obj = pStaticObjectArray[ idx ];
			if( obj && obj->ID == ID )
				return obj;
		}
	}
	else
	{
		r3d_assert(n >= 0 && n < MaxObjects);

		if(pObjectArray[ n ] && pObjectArray[ n ]->ID == ID)
			return pObjectArray[ n ];
	}

	return NULL;
}

GameObject* ObjectManager::GetNetworkObject(DWORD netID)
{
	R3DPROFILE_FUNCTION("ObjectManager::GetNetworkObject");

	if(netID == invalidGameObjectID)
		return NULL;

	NetMapType::iterator it = NetworkIDMap.find(netID);
	if(it!=NetworkIDMap.end())
		return (*it).second;

	// 	for(GameObject* obj = pFirstObject; obj; obj = obj->pNextObject)
	// 		if(obj->NetworkID == netID)
	// 			return obj;

	return NULL;
}

void ObjectManager::StartFrame()
{
}

void ObjectManager_Copy_OldState(GameObject *obj)
{
	obj->oldstate.Position     = obj->GetPosition();
	obj->oldstate.Orientation  = obj->GetRotationVector();
	obj->oldstate.Velocity     = obj->GetVelocity();
}

void CallUpdateSingle( void* Data, size_t ItemStart, size_t ItemCount )
{
	GameObject* obj = (GameObject*)Data;
	if (obj)
	{
		if(!obj->Update())
		{
			obj->setActiveFlag(0);
		}
	}
}
void CallUpdate( void* Data, size_t ItemStart, size_t ItemCount )
{
	GameObject** objs = (GameObject**) Data + ItemStart;

	for( size_t i = 0, e = ItemCount ; i < e; i ++ )
	{
		GameObject* obj = objs[ i ];

		if (obj)
		{
			//r3dOutToLog("%s\n",obj->Class->Name.c_str());
			if(!obj->Update())
			{
				obj->setActiveFlag(0);
				continue;
			}
		}
	}
}

//int temp_fix_object_position_after_physics_integration = false;
void ObjectManager::Update()
{
	R3DPROFILE_FUNCTION("ObjectManager::Update");
	static	std::vector<GameObject*> vObjs;

	vObjs.clear();
	vObjs.reserve(MaxObjects);
	SceneBoxOnUpdate();

	R3DPROFILE_START("SceneBox::Update");
	m_pRootBox->Update();
	R3DPROFILE_END("SceneBox::Update");

	m_FrameId ++ ;

	// fill objects array to be processed
	R3DPROFILE_START("UpdateLoading");
	for(GameObject* obj = pFirstObject; obj; obj = obj->pNextObject)
	{
		if(!obj->isActive())
			continue;

		// async loaded stray child
		if(!obj->bLoaded)
		{
			obj->bLoaded = obj->UpdateLoading() ;

			if( !obj->bLoaded )
				continue;
		}

		vObjs.push_back(obj);
	}
	R3DPROFILE_END("UpdateLoading");

	TemporaryObjects.Clear();

	TObjects.Clear();

	int index = 0;
	// process OnCreated objects first
	R3DPROFILE_START("OnCreate");
	for(int i=0, iEnd = vObjs.size(); i<iEnd; ++i) {
		GameObject *obj = vObjs[i];
		if(obj->ObjFlags & OBJFLAG_JustCreated) {
			BOOL res = obj->OnCreate();
			obj->ObjFlags &= ~OBJFLAG_JustCreated;
			if(!res)
			{
				r3dOutToLog("!!! OnCreate failed for obj=%s, filename=%s\n", obj->Class->Name.c_str(), obj->FileName.c_str());
				obj->setActiveFlag(0);
			}
		}
		//else
		//TObjects.PushBack( obj );
	}
	R3DPROFILE_END("OnCreate");

	// update & move objects
	R3DPROFILE_START("Update&Move");
	for(int i=0, iEnd = vObjs.size(); i<iEnd; ++i) 
	{
		GameObject *obj = vObjs[i];

		//if(/*!(obj->ObjTypeFlags <= OBJTYPE_Terrain && obj->ObjTypeFlags >= OBJTYPE_Animals)*/ 1)
		//{
			//TemporaryObjects.PushBack( obj );
			if(!obj->Update())
			{
				obj->setActiveFlag(0);
			}
		//}
		//else
		//{
		//	TemporaryObjects.PushBack( obj );
		//}

	}

	R3DPROFILE_END("Update&Move");

	//r3dOutToLog("%d\n",vObjs.size());

	R3DPROFILE_START("Update Static");

	// it's not crucial to update them every frame, thus we update 2048 objects tops,
	// distributing load between frames on large levels

	int staticUpdateEnd = R3D_MIN( 32, (int)NumStaticObjects );

	if( JustLoaded )
	{
		LastStaticUpdateIdx = 0;
		staticUpdateEnd = NumStaticObjects;
		JustLoaded = 0;
	}

	for( int i = 0; i < staticUpdateEnd; i ++ )
	{
		if( LastStaticUpdateIdx >= NumStaticObjects )
			LastStaticUpdateIdx = 0;

		GameObject* obj = pStaticObjectArray[ LastStaticUpdateIdx ++ ];

		if(obj->ObjFlags & OBJFLAG_JustCreated)
		{
			BOOL res = obj->OnCreate();
			obj->ObjFlags &= ~OBJFLAG_JustCreated;
			if(!res)
			{
				r3dOutToLog("!!! OnCreate failed for obj=%s, filename=%s\n", obj->Class->Name.c_str(), obj->FileName.c_str());
				obj->setActiveFlag(0);
			}
		}
		else
			TObjects.PushBack( obj );
		//TObjects.PushBack( obj );
		//TemporaryObjects.PushBack( obj );
	}

	R3DPROFILE_END("Update Static");

#ifndef WO_SERVER
#if !R3D_NO_BULLET_MGR
	R3DPROFILE_START("BulletMgr");
	m_BulletMngr->Update();
	R3DPROFILE_END("BulletMgr");
#endif
#endif

	//SwitchToThread();

	if( TemporaryObjects.Count() )
	{
		R3DPROFILE_START("Particles - Update");
		g_pJobChief->Exec( CallUpdate, &TemporaryObjects[ 0 ], TemporaryObjects.Count() );
		R3DPROFILE_END("Particles - Update");
	}
	if( TObjects.Count() )
	{
		r3dTL::TArray< GameObject* > TObject = TObjects;
		//R3DPROFILE_START("Particles - Update");
		g_pJobChief->Exec( CallUpdate, &TObject[ 0 ], TObject.Count() );
		//R3DPROFILE_END("Particles - Update");
	}

	m_pRootBox->PrepareForRender();
}


void ObjectManager::EndFrame()
{
	// Update all objects, remove all inactive
	for(int i = 0; i < MaxObjects; i++)
	{
		GameObject* obj = pObjectArray[i];
		if(obj == NULL)
			continue;
		int activeFlag = obj->getActiveFlag();
		if(activeFlag<0)
		{
			obj->setActiveFlag(--activeFlag);
			if(activeFlag == -4)
			{
				DeleteObject(obj);
			}
			continue;
		}
		if(obj->getActiveFlag() == 0) // schedule for deletion
		{
			// remove it from scene rendering
			if(obj->m_SceneBox)
				obj->m_SceneBox->Remove(obj);
			// remove it from physics
			if(obj->PhysicsObject)
			{
				delete obj->PhysicsObject;
				obj->PhysicsObject = 0;
			}
			obj->setActiveFlag(-1);
			continue;
		}

		obj->time_LifeTime += r3dGetFrameTime();

		if(obj->GetPosition() != obj->oldstate.Position)
			obj->OnPositionChanged();

		if(obj->GetRotationVector() != obj->oldstate.Orientation)
			obj->OnOrientationChanged();

		ObjectManager_Copy_OldState(obj);
	}

}

#if USE_VMPROTECT
#pragma optimize("g", off)
#endif
GameObject* ObjectManager::GetFirstObject()
{
	VMPROTECT_BeginMutation("ObjectManager::GetFirstObject");

	for(GameObject* obj = pFirstObject; obj; obj = obj->pNextObject)
	{
		if(obj->ObjFlags & OBJFLAG_Removed)
			continue;
		return obj;
	}
	return NULL;

	VMPROTECT_End();
}

GameObject* ObjectManager::GetNextObject(const GameObject* in_obj)
{
	VMPROTECT_BeginMutation("ObjectManager::GetNextObject");

	for(GameObject* obj = in_obj->pNextObject; obj; obj = obj->pNextObject)
	{
		if(obj->ObjFlags & OBJFLAG_Removed)
			continue;
		return obj;
	}
	return NULL;

	VMPROTECT_End();
}

#if USE_VMPROTECT
#pragma optimize("g", on)
#endif

//------------------------------------------------------------------------

ObjectIterator ObjectManager::GetFirstOfAllObjects()
{
	ObjectIterator iter;

	iter.current = pFirstObject;
	iter.staticIndex = -1;

	if( !iter.current )
	{
		iter.current = pStaticObjectArray[ 0 ];
		iter.staticIndex = 0;
	}

	return iter;
}

//------------------------------------------------------------------------

ObjectIterator ObjectManager::GetNextOfAllObjects( const ObjectIterator& it )
{
	ObjectIterator iter = it;

	if( iter.staticIndex >= 0 )
	{
		iter.staticIndex ++;

		if( iter.staticIndex < NumStaticObjects )
			iter.current = pStaticObjectArray[ iter.staticIndex ];
		else
			iter.current = 0;
	}
	else
	{
		if( iter.current->pNextObject )
		{
			iter.current = iter.current->pNextObject;
		}
		else
		{
			iter.current = pStaticObjectArray[ 0 ];
			iter.staticIndex = 0;
		}
	}

	return iter;
}

//------------------------------------------------------------------------

GameObject* ObjectManager::GetStaticObject( int idx )
{
	r3d_assert( idx >= 0 && idx < NumStaticObjects );
	return pStaticObjectArray[ idx ];
}

int ObjectManager::GetStaticObjectCount() const
{
	return NumStaticObjects;
}

GameObject* ObjectManager::CastRay(const r3dPoint3D& vStart, const r3dPoint3D& vRay, float RayLen, CollisionInfo *cInfo, int bboxonly /* = false */)
{
	R3DPROFILE_FUNCTION( "ObjectManager::CastRay" ) ;

	GameObject	*FoundObj    = NULL;
	r3dMaterial *FoundMtrl = NULL;
	r3dMaterial *mtrl = NULL;
	float	     MinDistSoFar = 999999.0f;
	float		 dst = 999999;

	int MinFace = -1;

	GameObject* obj_i = pFirstObject; 
	GameObject* nextObj = NULL;

	int sidx = 0, se = NumStaticObjects;

	for( ; ; obj_i ? ( obj_i = nextObj, 0 ) : ++sidx )
	{
		GameObject* obj;

		if( obj_i )
		{
			nextObj = obj_i->pNextObject;
			obj = obj_i;
		}
		else
		{
			if( sidx >= se )
				break;

			obj = pStaticObjectArray[ sidx ];
		}

		if(!obj->isActive() || (obj->ObjFlags & OBJFLAG_SkipCastRay) || (obj->ObjFlags & OBJFLAG_Removed))
			continue;

		int OutMinFace = -1;

		r3dBoundBox bbox = obj->GetBBoxWorld();
		extern int CurHUDID;
		extern bool g_bEditMode;
		if (g_bEditMode && CurHUDID == 0)
		{
			if (obj->isObjType(OBJTYPE_Particle) || obj->isObjType(OBJTYPE_Sound))	// in editor mode, force small bbox for easier selection
			{
				bbox.Org = obj->GetPosition() - r3dPoint3D(1,1,1); // PT: particle should be in the center of bbox
				bbox.Size.Assign(2, 2, 2);
			}
#ifndef WO_SERVER
			else if (obj->isObjType(OBJTYPE_ItemSpawnPoint))
			{
				BaseItemSpawnPoint *bisp = static_cast<BaseItemSpawnPoint*>(obj);
				for (size_t i = 0; i < bisp->m_SpawnPointsV.size(); ++i)
				{
					bbox.ExpandTo(bisp->m_SpawnPointsV[i].GetDebugBBox());
				}
			}
#endif // WO_SERVER
		}

		// check bound box
		if(!bbox.ContainsRay(vStart, vRay, RayLen, &dst))
			continue;
		else if(obj->isObjType(OBJTYPE_Mesh)&& (!bboxonly ) )
		{
			// bp, only check bounding box if flag is set

			// check mesh if it's there
			r3dMesh *mesh = ((MeshGameObject *)obj)->MeshLOD[0];

			dst = 999999;

			D3DXMATRIX rotation = obj->GetRotationMatrix();
			if(!mesh->ContainsRay(vStart, vRay, RayLen, &dst, &mtrl, obj->GetPosition(), rotation, &OutMinFace ) )
			{
				if (!CastRayToSpawnPoints(obj, vStart, vRay, RayLen, dst))
					continue;
			}
		}
		else if(obj->isObjType(OBJTYPE_Road)&& (!bboxonly ) )
		{
			// bp, only check bounding box if flag is set

			// check mesh if it's there
			r3dMesh *mesh = ((obj_Road *)obj)->mesh_;

			dst = 999999;

			D3DXMATRIX rotation;// = obj->GetRotationMatrix();
			D3DXMatrixIdentity ( &rotation );
			r3dVector vPos ( 0,0,0 );
			if(!mesh->ContainsRay(vStart, vRay, RayLen, &dst, &mtrl, vPos, rotation, &OutMinFace)) 
				continue;

		}
#ifndef WO_SERVER
#ifndef FINAL_BUILD
		else if (obj->isObjType(OBJTYPE_CollectionProxy) && !bboxonly)
		{
			CollectionElementObjectProxy *cp = static_cast<CollectionElementObjectProxy*>(obj);
			r3dMesh *mesh = cp->GetMesh();

			CollectionElement *ce = cp->GetCollectionElement();
			if (ce)
			{
				dst = 999999;

				D3DXMATRIX rotation, scaling;
				D3DXMatrixRotationY(&rotation, R3D_DEG2RAD(ce->angle));
				D3DXMatrixScaling(&scaling, ce->scale, ce->scale, ce->scale);
				D3DXMatrixMultiply(&rotation, &scaling, &rotation);

				if(!mesh->ContainsRay(vStart, vRay, RayLen, &dst, &mtrl, obj->GetPosition(), rotation, &OutMinFace)) 
					continue;
			}
		}
#endif
#endif
#if APEX_ENABLED
		else if (obj->isObjType(OBJTYPE_ApexDestructible))
		{
			obj_ApexDestructible *oad = reinterpret_cast<obj_ApexDestructible*>(obj);
			mtrl = oad->GetMaterial(0);
		}
#endif

		if(dst >= MinDistSoFar)
			continue; 

		FoundObj     = obj;
		MinDistSoFar = dst;
		MinFace	= OutMinFace;
		FoundMtrl = mtrl;
		dst = 999999;
	}

	// FIXME
	// fill collisionInfo
	cInfo->Type 	 	= CLT_Nothing;

	if(!FoundObj)
		return NULL;

	cInfo->Type        = CLT_Box;
	cInfo->pObj	   = FoundObj;
	cInfo->ClipCoef	   = MinDistSoFar;
	cInfo->Distance	   = MinDistSoFar;
	cInfo->NewPosition = vStart + vRay*MinDistSoFar;
	cInfo->Material = mtrl;

	if ( ( FoundObj->isObjType(OBJTYPE_Mesh) || FoundObj->isObjType( OBJTYPE_Road ) ) && (!bboxonly) )
	{
		r3dMesh* mesh = FoundObj->isObjType( OBJTYPE_Road ) ? ((obj_Road *)FoundObj)->mesh_ : ((MeshGameObject *)FoundObj)->MeshLOD[0];

		cInfo->Type 	= CLT_Face;

		if( MinFace >= 0 && mesh->VertexPositions )
		{
			int idx0 = mesh->Indices[ MinFace * 3 + 0 ];
			int idx1 = mesh->Indices[ MinFace * 3 + 1 ];
			int idx2 = mesh->Indices[ MinFace * 3 + 2 ];

			r3dPoint3D pos0 = mesh->VertexPositions[ idx0 ];
			r3dPoint3D pos1 = mesh->VertexPositions[ idx1 ];
			r3dPoint3D pos2 = mesh->VertexPositions[ idx2 ];

			r3dPoint3D v0 = pos1 - pos0;
			r3dPoint3D v1 = pos2 - pos0;

			cInfo->Normal = v0.Cross( v1 );
			cInfo->Normal.Normalize();

			const D3DXMATRIX& rotMat = FoundObj->GetRotationMatrix();

			D3DXVECTOR4 norm( cInfo->Normal.x, cInfo->Normal.y, cInfo->Normal.z, 0.f );
			D3DXVec4Transform( &norm, &norm, &rotMat );

			cInfo->Normal.x = norm.x;
			cInfo->Normal.y = norm.y;
			cInfo->Normal.z = norm.z;
		}
		else
		{
			cInfo->Normal	= r3dPoint3D( 0, 1, 0 );
		}

		cInfo->pMeshObj	= mesh;
		cInfo->Material = FoundMtrl;
	} 

	return FoundObj;
}

GameObject* ObjectManager::CastMeshRay(const r3dPoint3D& vStart, const r3dPoint3D& vRay, float RayLen, CollisionInfo *cInfo)
{
	GameObject	*FoundObj    = NULL;
	r3dMaterial *FoundMtrl = NULL;
	r3dMaterial *mtrl = NULL;
	float	     	MinDistSoFar = 999999.0f;
	int	     	facen = 0;
	int		CFaceN = 0;
	float 		dst = 999999;

	GameObject* obj_i = pFirstObject; 
	GameObject* nextObj = NULL;

	int sidx = 0, se = NumStaticObjects;

	for( ; ; obj_i ? ( obj_i = nextObj, 0 ) : ++sidx )
	{
		GameObject* obj;

		if( obj_i )
		{
			nextObj = obj_i->pNextObject;
			obj = obj_i;
		}
		else
		{
			if( sidx >= se )
				break;

			obj = pStaticObjectArray[ sidx ];
		}

		if(!obj->isActive() || (obj->ObjFlags & OBJFLAG_SkipCastRay) || (obj->ObjFlags & OBJFLAG_Removed))
			continue;

		// check bound box
		if(!obj->GetBBoxWorld().ContainsRay(vStart, vRay, RayLen, &dst))
			continue;

		if(obj->isObjType(OBJTYPE_Mesh))
		{
			// check mesh if it's there
			r3dMesh *mesh = ((MeshGameObject *)obj)->MeshLOD[0];

			facen = 0;
			dst = 999999;

			if(!mesh->ContainsRay(vStart, vRay, RayLen, &dst, &mtrl, obj->GetPosition(), obj->GetRotationMatrix())) 
				continue;
		} 
		else // skip all other objects
			continue;

		if(dst >= MinDistSoFar)
			continue;

		FoundObj     = obj;
		FoundMtrl = mtrl;
		MinDistSoFar = dst;
		CFaceN 	 = facen;
		dst 	 = 999999;
	}

	// FIXME
	// fill collisionInfo
	cInfo->Type 	 	= CLT_Nothing;

	if(!FoundObj)
		return NULL;

	cInfo->Type        = CLT_Box;
	cInfo->pObj	   = FoundObj;
	cInfo->ClipCoef	   = MinDistSoFar;
	cInfo->Distance	   = MinDistSoFar;
	cInfo->NewPosition = vStart + vRay*MinDistSoFar;

	if(FoundObj && FoundObj->isObjType(OBJTYPE_Mesh))
	{
		r3dMesh* mesh = ((MeshGameObject *)FoundObj)->MeshLOD[0];

		cInfo->Type 	= CLT_Face;
		cInfo->Normal	= r3dVector(0,0,0);
		cInfo->pMeshObj	= mesh;
		cInfo->Material = FoundMtrl;
	}
	return FoundObj;
}

void ObjectManager :: DumpObjects()
{
	int Idx = 1;

	for( ObjectIterator iter = GetFirstOfAllObjects(); iter.current; iter = GetNextOfAllObjects(iter)) 
	{
		GameObject *obj = iter.current;

		if(!obj->isActive())
			continue;

		r3dOutToLog("OBJ %d\t\t%s\n", Idx++, obj->Name.c_str());

	}

}

draw_s		draw[ OBJECTMANAGER_MAXOBJECTS ];
int			n_draw;

draw_s		draw_interm[ OBJECTMANAGER_MAXOBJECTS ];
int			n_draw_interm;

GameObject* box_scene_query[ OBJECTMANAGER_MAXOBJECTS ];
int			n_box_scene_query;


RenderArray	g_render_arrays[ rsCount ];


int renderable_Comparator( void const * p0, void const* p1 )
{
	Renderable* r0 = (Renderable*)p0;
	Renderable* r1 = (Renderable*)p1;

	INT64 diff = r0->SortValue - r1->SortValue;

	// preserve effect of bit 31 then append effect of bits 32..63 (sign bit 63 will get into sign bit 31)
	return int( ( diff | diff >> 1 ) & 0x7fffffff | diff >> 32 );
}


int _render_World = 1;
static r3dVector prevCamPos = r3dVector(0,0,0);
static r3dVector prevCamDir = r3dVector(0,0,0);

float gShadowSunMultiplier = 0;
uint8_t getShadowSliceBit(GameObject* userObject, const r3dCamera& Cam )
{
	// find in which slice out object is
	// squared distances are not working in some edge cases, so using normal distances
	uint8_t shadow_slice = 0;
	float dist = (Cam-userObject->GetPosition()).Length();
	float objRadius = userObject->GetObjectsRadius() + userObject->GetBBoxWorld().Size.y*gShadowSunMultiplier;

	//objRadius *= objRadius;
	float shadowSlice0 = ShadowSplitDistancesOpaque[0];// shadowSlice0 *= shadowSlice0;
	float shadowSlice1 = ShadowSplitDistancesOpaque[1];// shadowSlice1 *= shadowSlice1;
	float shadowSlice2 = ShadowSplitDistancesOpaque[2];// shadowSlice2 *= shadowSlice2;
	float shadowSlice3 = ShadowSplitDistancesOpaque[3];// shadowSlice3 *= shadowSlice3;
	float shadowsq_max = dist/*distsq*/ + objRadius;
	float shadowsq_min = R3D_MAX(dist/*distsq*/ - objRadius, shadowSlice0);
	const float sC1 = 0.8f; // some overlapping required
	const float sC2 = 1.0f;
	if( (shadowsq_min >= shadowSlice0*sC1 && shadowsq_min < shadowSlice1*sC2) || (shadowsq_max > shadowSlice0*sC1 && shadowsq_max < shadowSlice1*sC2) )
		shadow_slice |= 0x1;
	if( (shadowsq_min >= shadowSlice1*sC1 && shadowsq_min < shadowSlice2*sC2) || (shadowsq_max > shadowSlice1*sC1 && shadowsq_max < shadowSlice2*sC2) || (shadowsq_min <= shadowSlice1*sC1 && shadowsq_max > shadowSlice2*sC2) )
		shadow_slice |= 0x2;
	if( (shadowsq_min >= shadowSlice2*sC1 && shadowsq_min < shadowSlice3*sC2) || (shadowsq_max > shadowSlice2*sC1 && shadowsq_max < shadowSlice3*sC2) || (shadowsq_min <= shadowSlice2*sC1 && shadowsq_max > shadowSlice3*sC2)  )
		shadow_slice |= 0x4;

	return shadow_slice;
}

GameObject* TreeObject = 0; // declare it here because of server

void ObjectManager::DrawDebug(const r3dCamera& Cam)
{
	r3dRenderer->SetRenderingMode(R3D_BLEND_NOALPHA | R3D_BLEND_ZC);
	m_pRootBox->TraverseDebug(Cam);
}

void ObjectManager::AppendDebugBoxes()
{
	m_pRootBox->AppendDebugBoxes();
}

int
ObjectManager::GetFrameId()
{
	return m_FrameId ;
}

void
ObjectManager::ResetObjFlags()
{
	for( int i = 0, e = n_draw; i < e; i ++ )
	{
		draw[ i ].obj->InMainFrustum = 0;
	}
}

void
ObjectManager::IssueOcclusionQueries()
{
	if( r_use_oq->GetInt() && r3dRenderer->SupportsOQ )
	{
		m_pRootBox->DoOcclusionQueries( PrepCam );
	}
}

void ObjectManager::GetObjectsInCube(const r3dBoundBox& box, GameObject**& result, int& objectsCount)
{
	m_pRootBox->TraverseTree(box, box_scene_query, objectsCount);
	result = box_scene_query;
}

static inline D3DXVECTOR2 ToVec2( const D3DXVECTOR4& v )
{
	return D3DXVECTOR2( v.x, v.y );
}

static inline r3dPoint3D ToPoint3D( const D3DXVECTOR4& v )
{
	return r3dPoint3D( v.x, v.y, v.z );
}

static inline float saturate( float val )
{
	return R3D_MIN( R3D_MAX( val, 0.f ), 1.f );
}

static D3DXMATRIX vCachedExtrusionCamera ;

void CalcShadowExtrusionData( ShadowExtrusionData* oData, const r3dBoundBox& bbox, const D3DXMATRIX &objMtx, const D3DXMATRIX& lightMtx, float extrude, D3DXPLANE (&mainFrustumPlanes)[ 6 ], float* ResultingExtrude )
{
	R3DPROFILE_FUNCTION("CheckDirShadowVisibility");

	// use local bbox for better efficiency
	const r3dPoint3D& org = bbox.Org;
	const r3dPoint3D& sz = bbox.Size;

	D3DXVECTOR4 bboxPts[] = 
	{
		D3DXVECTOR4( org.x	+ 0		, org.y + 0		, org.z + 0		, 1 ),
		D3DXVECTOR4( org.x	+ sz.x	, org.y + 0		, org.z + 0		, 1 ),
		D3DXVECTOR4( org.x	+ 0		, org.y + sz.y	, org.z + 0		, 1 ),
		D3DXVECTOR4( org.x	+ sz.x	, org.y + sz.y	, org.z + 0		, 1 ),
		D3DXVECTOR4( org.x	+ 0		, org.y + 0		, org.z + sz.z	, 1 ),
		D3DXVECTOR4( org.x	+ sz.x	, org.y + 0		, org.z + sz.z	, 1 ),
		D3DXVECTOR4( org.x	+ 0		, org.y + sz.y	, org.z + sz.z	, 1 ),
		D3DXVECTOR4( org.x	+ sz.x	, org.y + sz.y	, org.z + sz.z	, 1 )
	};

	D3DXMATRIX toLight;

	D3DXMatrixMultiply( &toLight, &objMtx, &lightMtx );

	for( size_t i = 0, e = sizeof bboxPts / sizeof bboxPts[ 0 ]; i < e; i ++ )
	{
		D3DXVec4Transform( &bboxPts[ i ], &bboxPts[ i ], &toLight );
	}

	// wrap bbox light 'xy' projection with a rectangle

	// which projected segment is the longest, sX, sY or sZ?

	D3DXVECTOR4 sX = bboxPts[ 1 ] - bboxPts[ 0 ];
	D3DXVECTOR4 sY = bboxPts[ 2 ] - bboxPts[ 0 ];
	D3DXVECTOR4 sZ = bboxPts[ 4 ] - bboxPts[ 0 ];

	float lx = sX.x * sX.x + sX.y * sX.y;
	float ly = sY.x * sY.x + sY.y * sY.y;
	float lz = sZ.x * sZ.x + sZ.y * sZ.y;

	// construct rotation which would make the longest segment align with X axis.

	D3DXVECTOR2 alignTo;

	if( lx >= ly && lx >= lz )
	{
		alignTo = ToVec2( sX );
	}
	else
	{
		if( ly >= lz )
		{
			alignTo = ToVec2( sY );
		}
		else
		{
			alignTo = ToVec2( sZ );
		}
	}

	D3DXVec2Normalize( &alignTo, &alignTo );

	D3DXMATRIX rotZ
		(
		+alignTo.x	, -alignTo.y	, 0, 0,
		+alignTo.y	, +alignTo.x	, 0, 0,
		0			, 0				, 1, 0,
		0			, 0				, 0, 1
		);

	float miX = +FLT_MAX;
	float maX = -FLT_MAX;
	float miY = +FLT_MAX;
	float maY = -FLT_MAX;
	float miZ = +FLT_MAX;
	float maZ = -FLT_MAX;

	for( size_t i = 0, e = sizeof bboxPts / sizeof bboxPts[ 0 ]; i < e; i ++ )
	{
		D3DXVECTOR4 pt;
		D3DXVec4Transform( &pt, &bboxPts[ i ], &rotZ );

		miX = R3D_MIN( pt.x, miX );
		miY = R3D_MIN( pt.y, miY );
		miZ = R3D_MIN( pt.z, miZ );
		maX = R3D_MAX( pt.x, maX );
		maY = R3D_MAX( pt.y, maY );
		maZ = R3D_MAX( pt.z, maZ );
	}

	D3DXMATRIX toBox;
	D3DXMatrixMultiply( &toBox, &lightMtx, &rotZ );

	D3DXMATRIX fromBox;

	r3dOrthoInverse( fromBox, toBox );

	// limit extrusion by terrain
	D3DXVECTOR4 samplePoint( (miX + maX) * 0.5f, (miY + maY) * 0.5f, miZ, 1.f );

	D3DXVECTOR4 worldPoint0;
	D3DXVECTOR4 worldPoint1;

	D3DXVec4Transform( &worldPoint0, &samplePoint, &fromBox );

	samplePoint.z += r_shadowcull_extrude->GetFloat();
	D3DXVec4Transform( &worldPoint1, &samplePoint, &fromBox );

	if( fabs( worldPoint0.y - worldPoint1.y ) > 16 * FLT_EPSILON )
	{
		float h = r_shadow_extrusion_limit->GetFloat();

		if (Terrain)
		{
			float h0 = Terrain->GetHeight( ToPoint3D( worldPoint0 ) );
			float h1 = Terrain->GetHeight( ToPoint3D( worldPoint1 ) );
			h = R3D_MIN( h0, h1 );
		}

		D3DXVECTOR4 resExtrude;

		float factor = saturate( ( h - worldPoint0.y ) / ( worldPoint1.y - worldPoint0.y ) );

		D3DXVec4Lerp( &resExtrude, &worldPoint0, &worldPoint1, factor );

		D3DXVec4Transform( &resExtrude, &resExtrude, &toBox );

		float dl = ( maY - miY + maX - miX ) * 0.5f;
		float abs_tan = fabs( sqrtf( lightMtx._33 * lightMtx._33 + lightMtx._13 * lightMtx._13 ) / lightMtx._23 );

		extrude = resExtrude.z + dl * abs_tan;
	}

	oData->MinX = miX ;
	oData->MinY = miY ;
	oData->MinZ = miZ ;

	oData->MaxX = maX ;
	oData->MaxY = maY ;

	extrude = R3D_MIN( oData->MinZ + r_shadowcull_extrude->GetFloat(), extrude );

	oData->Extrude = extrude ;

	D3DXMatrixTranspose( &oData->ToExtrusionBox, &fromBox );
}

int CheckDirShadowVisibility( ShadowExtrusionData& data, bool updateExData, const r3dBoundBox& bbox, const D3DXMATRIX &objMtx, const D3DXMATRIX& lightMtx, float extrude, D3DXPLANE (&mainFrustumPlanes)[ 6 ], float* ResultingExtrude )
{
	if( updateExData )
	{
		CalcShadowExtrusionData( &data, bbox, objMtx, lightMtx, extrude, mainFrustumPlanes, ResultingExtrude );
	}

	if( data.Extrude > extrude )
		return false;

	extrude = data.Extrude ;

	D3DXPLANE tplanes[ 6 ];

	for( size_t i = 0, e = sizeof tplanes / sizeof tplanes[ 0 ] ; i < e; i ++ )
	{
		D3DXPlaneTransform( tplanes + i, mainFrustumPlanes + i, &data.ToExtrusionBox );
	}

	if( ResultingExtrude )
		*ResultingExtrude = extrude - data.MinZ ;

	r3dBoundBox bb;
	bb.Org.Assign(data.MinX, data.MinY, data.MinZ);
	bb.Size.Assign(data.MaxX - data.MinX, data.MaxY - data.MinY, data.Extrude - data.MinZ);
	int res = r3dRenderer->IsBoxInsideCustomFrustum(bb, tplanes);

#ifndef FINAL_BUILD
	if( res && r_show_shadow_extrusions->GetBool())
	{
		D3DXMATRIX fromBox;

		D3DXMatrixTranspose( &fromBox, &data.ToExtrusionBox );

		r3dPoint3D pts[ 8 ] ;

		int pi = 0 ;

		{
			D3DXVECTOR4 vec( data.MinX, data.MinY, data.MinZ, 1.f );
			D3DXVec4Transform( &vec, &vec, &fromBox );
			pts[ pi ++ ] = r3dPoint3D( vec.x, vec.y, vec.z ) ;
		}

		{
			D3DXVECTOR4 vec( data.MaxX, data.MinY, data.MinZ, 1.f );
			D3DXVec4Transform( &vec, &vec, &fromBox );
			pts[ pi ++ ] = r3dPoint3D( vec.x, vec.y, vec.z ) ;
		}

		{
			D3DXVECTOR4 vec( data.MinX, data.MaxY, data.MinZ, 1.f );
			D3DXVec4Transform( &vec, &vec, &fromBox );
			pts[ pi ++ ] = r3dPoint3D( vec.x, vec.y, vec.z ) ;
		}

		{
			D3DXVECTOR4 vec( data.MaxX, data.MaxY, data.MinZ, 1.f );
			D3DXVec4Transform( &vec, &vec, &fromBox );
			pts[ pi ++ ] = r3dPoint3D( vec.x, vec.y, vec.z ) ;
		}

		{
			D3DXVECTOR4 vec( data.MinX, data.MinY, data.Extrude, 1.f );
			D3DXVec4Transform( &vec, &vec, &fromBox );
			pts[ pi ++ ] = r3dPoint3D( vec.x, vec.y, vec.z ) ;
		}

		{
			D3DXVECTOR4 vec( data.MaxX, data.MinY, data.Extrude, 1.f );
			D3DXVec4Transform( &vec, &vec, &fromBox );
			pts[ pi ++ ] = r3dPoint3D( vec.x, vec.y, vec.z ) ;
		}

		{
			D3DXVECTOR4 vec( data.MinX, data.MaxY, data.Extrude, 1.f );
			D3DXVec4Transform( &vec, &vec, &fromBox );
			pts[ pi ++ ] = r3dPoint3D( vec.x, vec.y, vec.z ) ;
		}

		{
			D3DXVECTOR4 vec( data.MaxX, data.MaxY, data.Extrude, 1.f );
			D3DXVec4Transform( &vec, &vec, &fromBox );
			pts[ pi ++ ] = r3dPoint3D( vec.x, vec.y, vec.z ) ;
		}

#ifndef WO_SERVER
		void PushDebugBox(	r3dPoint3D p0, r3dPoint3D p1, r3dPoint3D p2, r3dPoint3D p3,
			r3dPoint3D p4, r3dPoint3D p5, r3dPoint3D p6, r3dPoint3D p7,
			r3dColor color ) ;


		PushDebugBox(	pts[ 0 ], pts[ 1 ], pts[ 2 ], pts[ 3 ],
			pts[ 4 ], pts[ 5 ], pts[ 6 ], pts[ 7 ],
			r3dColor::red ) ;
#endif

	}
#endif

	return res;
}

int CheckDirShadowVisibility( GameObject* obj, bool updateExData, const D3DXMATRIX& lightMtx, float extrude, D3DXPLANE (&mainFrustumPlanes)[ 6 ] )
{
	const r3dBoundBox& bbox = obj->GetBBoxLocal();
	const D3DXMATRIX &objMtx = obj->GetTransformMatrix();

	ShadowExtrusionData* shdata( NULL );

	return CheckDirShadowVisibility( obj->ShadowExData, updateExData, bbox, objMtx, lightMtx, extrude, mainFrustumPlanes, &obj->LastShadowExtrusion );
}

static bool ShadowCullNeedsRecalc ;

bool DoesShadowCullNeedRecalc()
{
	return ShadowCullNeedsRecalc ;
}

void
ObjectManager::PrepareSlicedShadowsInterm( const r3dCamera& Cam, D3DXPLANE (&mainFrustumPlanes)[ 6 ] )
{
	n_draw_interm = 0;

	m_pRootBox->TraverseTree( Cam, draw_interm, n_draw_interm );

	bool newRecalcShadowExData = false ;
	ShadowCullNeedsRecalc = false ;

	for( uint32_t j = 0, e = 3; j < e; j ++ )
	{
		for( uint32_t i = 0, e = 3; i < e; i ++ )
		{
			if( fabs( vCachedExtrusionCamera.m[j][i] - r3dRenderer->ViewMatrix.m[j][i] ) > FLT_EPSILON * 16.f )
			{
				vCachedExtrusionCamera = r3dRenderer->ViewMatrix ;
				newRecalcShadowExData = true ;
				ShadowCullNeedsRecalc = true ;
				break ;
			}
		}
	}

	g_render_arrays[ rsCreateSM + 0 ].Clear();
	g_render_arrays[ rsCreateSM + 1 ].Clear();
	g_render_arrays[ rsCreateSM + 2 ].Clear();

	int shadowCull = r_shadowcull->GetBool();

	if( !r_precalc_shadowcull->GetInt() )
		newRecalcShadowExData = true ;

	for( int i = 0, e = n_draw_interm; i < e; i ++ )
	{
#ifdef WO_SERVER
		const float MAX_DIR_SHADOW_LENGTH = 1;
#endif
		GameObject* obj = draw_interm[ i ].obj ;

		if( !( obj->ObjFlags & DISABLE_SHADOWS_FLAGS ) )
		{
			int inMainFrustum = CheckDirShadowVisibility( obj, newRecalcShadowExData | obj->ShadowExDirty, r3dRenderer->ViewMatrix, MAX_DIR_SHADOW_LENGTH, mainFrustumPlanes ) ;

			draw_interm[ i ].obj->AppendSlicedShadowRenderables( g_render_arrays, !shadowCull || inMainFrustum, Cam );

			if( shadowCull )
				obj->ShadowExDirty = false ;
		}
	}

	// tree hack
	if( TreeObject )
	{
		TreeObject->AppendShadowRenderables( g_render_arrays[ rsCreateSM + 0 ], Cam );
		TreeObject->AppendShadowRenderables( g_render_arrays[ rsCreateSM + 1 ], Cam );
		TreeObject->AppendShadowRenderables( g_render_arrays[ rsCreateSM + 2 ], Cam );
	}

	PrepCamInterm = Cam;
}

void
ObjectManager::PrepareShadowsInterm( const r3dCamera& Cam )
{
	n_draw_interm = 0;

	m_pRootBox->TraverseTree( Cam, draw_interm, n_draw_interm );

	g_render_arrays[ rsCreateSM ].Clear();

	for( int i = 0, e = n_draw_interm; i < e; i ++ )
	{
		GameObject* obj = draw_interm[ i ].obj;

		if( !( obj->ObjFlags & DISABLE_SHADOWS_FLAGS ) )
		{
			obj->AppendShadowRenderables( g_render_arrays[ rsCreateSM ], Cam );
		}
	}

#ifndef WO_SERVER
	// tree hack
	if( TreeObject )
	{
		gCollectionsManager.ComputeVisibility(true, false);
		TreeObject->AppendShadowRenderables( g_render_arrays[ rsCreateSM ], Cam );
	}
#endif

	PrepCamInterm = Cam;
}

void
ObjectManager::PrepareTransparentShadowsInterm( const r3dCamera& Cam )
{
	n_draw_interm = 0;

	for( int i = 0, e = TransparentShadowCasterCount ; i < e ; i ++ )
	{
		GameObject* obj = TransparentShadowCasters[ i ] ;

		if( !r3dRenderer->IsBoxInsideFrustum( obj->GetBBoxWorld() ) )
			continue ;

		draw_s& ds = draw_interm[ n_draw_interm ++ ] ;

		ds.obj	= obj ;
		ds.distSq = 0.f ;
		ds.shadow_slice = 0 ;

		obj->AppendTransparentShadowRenderables( g_render_arrays[ rsCreateTransparentSM ], Cam ) ;
	}

	PrepCamInterm = Cam;
}

void SortRenderArray( RenderArray& rarr, int start )
{
	if(rarr.Count() >= 2 && (int)rarr.Count() > start)
	{
		size_t numElems = static_cast<size_t>(rarr.Count() - start);
		ParallelQSort pqs(numElems / g_pJobChief->GetThreadCount());
		pqs.Sort(&rarr[start], numElems, RenderArray::TAB_SIZE, renderable_Comparator);
	}
}

void ClearMaterialBits( RenderArray& rarr, int start )
{
	for( int i = start, e = rarr.Count(); i < e; i++ )
	{
		rarr[ i ].SortValue &= 0xff00000000000000ull;
	}
}

void SortDrawSlots()
{
	R3DPROFILE_FUNCTION( "Sort Draw Slots" );

	struct SortComparator
	{
		R3D_FORCEINLINE
			int operator()( const draw_s& s0, const draw_s& s1 )
		{
			if ( s0.obj->DrawOrder == s1.obj->DrawOrder )
			{
				if(s0.obj->DrawOrder != OBJ_DRAWORDER_LAST)
					return s0.distSq < s1.distSq;
				else
					return s1.distSq < s0.distSq;
			}
			return s0.obj->DrawOrder < s1.obj->DrawOrder;
		}
	} comp ;
	std::sort( &draw[ 0 ], &draw[ 0 ] + n_draw, comp );
}

void AppendSkippOcclusionCheckObject( const r3dCamera& Cam )
{
	R3DPROFILE_FUNCTION( "AppendSkippOcclusionCheckRenderables" ) ;

	float defDrawDistanceSq = r_default_draw_distance->GetFloat() * r_default_draw_distance->GetFloat();

	r3dPoint3D cullRefPos = r3dRenderer->DistanceCullRefPos;

	ObjectManager& GW = GameWorld();
	std::vector<GameObject*>::iterator it;
	//for(GameObject *obj = GW.GetFirstObject(); obj; obj = GW.GetNextObject(obj))
	for(it=gSkipOcclusionCheckGameObjects.begin(); it!=gSkipOcclusionCheckGameObjects.end(); ++it)
	{
		GameObject* obj = *it;
		{
			if(!obj->isActive() || obj->ObjFlags & OBJFLAG_SkipDraw || obj->ObjFlags & OBJFLAG_JustCreated || obj->ObjFlags & OBJFLAG_Removed || !obj->isDetailedVisible())
				continue;

			int always_draw = obj->ObjFlags & OBJFLAG_AlwaysDraw;

			float cullDistSq = ( cullRefPos - obj->GetPosition() ).LengthSq();

			if(	always_draw 
				|| 
				(r3dRenderer->IsBoxInsideFrustum(obj->GetBBoxWorld()) && CheckObjectDistance( obj, cullDistSq, defDrawDistanceSq ))	
				)
			{
				draw_s& d		= draw[ n_draw ++ ];
				d.obj			= obj ;
				d.distSq		= ( Cam - obj->GetPosition() ).LengthSq();
				d.shadow_slice	= 0;
			}
		}
	}
}

void AppendSkippOcclusionCheckRenderables( const r3dCamera& Cam )
{
	R3DPROFILE_FUNCTION( "AppendSkippOcclusionCheckRenderables" ) ;

	AppendSkippOcclusionCheckObject( Cam );

	SortDrawSlots();

	if(n_draw>0)
	{
		int prevDrawOrder		= draw[ 0 ].obj->DrawOrder;
		int prevDrawOrderIdx	= g_render_arrays[ rsFillGBuffer ].Count();

		for( int i = 0, e = n_draw; i < e; i ++ )
		{
			draw[ i ].obj->InMainFrustum = 1;

			if( prevDrawOrder != draw[ i ].obj->DrawOrder )
			{
				R3DPROFILE_START("Sort Render Array");

				SortRenderArray( g_render_arrays[ rsFillGBuffer ], prevDrawOrderIdx );

				R3DPROFILE_END("Sort Render Array");

				prevDrawOrder		= draw[ i ].obj->DrawOrder;
				prevDrawOrderIdx	= g_render_arrays[ rsFillGBuffer ].Count();
			}

			//if (draw[ i ].obj->Class->Name != "obj_Building")
			draw[ i ].obj->AppendRenderables( g_render_arrays, Cam );
		}
	}
	n_draw = 0;

}

static void AppendRenderArrayMatScoreInfo( RenderArray& rarr )
{
	for( uint32_t i = 0, e = rarr.Count() ; i < e; i ++ )
	{
		Renderable& ren = rarr[ i ] ;

		int MatId = ren.SortValue >> 32 & 0xFFFF ;

		if( MatId )
		{
			ren.SortValue |= (UINT64)(0xffff - gMatFrameScores[ MatId & MAT_FRAME_SCORE_MASK ]) << 48ull ;
		}
	}
}

void
ObjectManager::Prepare( const r3dCamera& Cam )
{
	R3DPROFILE_FUNCTION("ObjectManager::Prepare");
#ifndef WO_SERVER
	extern int g_SortByMaterial;
#else
	int g_SortByMaterial = 0;
#endif

	for( uint32_t i = 0, e = rsCount; i < e; i ++ )
	{
		g_render_arrays[ i ].Clear();
	}

	n_draw = 0;

	if( r_use_oq->GetInt() && r3dRenderer->SupportsOQ )
	{
		AppendSkippOcclusionCheckRenderables( Cam ) ;
       // CreateThread(NULL,NULL,LPTHREAD_START_ROUTINE(AppendSkippOcclusionCheckRenderables),(void*)&Cam,0,0);
		m_pRootBox->TraverseAndPrepareForOcclusionQueries( Cam, m_FrameId );
	}
	else
	{
		//CreateThread(NULL,NULL,LPTHREAD_START_ROUTINE(AppendSkippOcclusionCheckRenderables),(void*)&Cam,0,0);
		AppendSkippOcclusionCheckRenderables( Cam ) ;

		m_pRootBox->TraverseTree( Cam, draw, n_draw );
	}

	UpdateSceneTraversalStats();
#ifndef WO_SERVER
#if !R3D_NO_BULLET_MGR
	m_BulletMngr->AppendRenderables(g_render_arrays, Cam);
#endif
#endif

#ifndef WO_SERVER
	// tree hack
	if( TreeObject )
	{
		gCollectionsManager.ComputeVisibility(false, false);

		draw[ n_draw ].obj = TreeObject;
		draw[ n_draw ].distSq = 10;
		draw[ n_draw ].shadow_slice = 0xff;

		n_draw ++;
	}
#endif

	//	Prepare matrices for render. Do it her in parallel manner, to speedup calculation.
	RecalcObjectMatrices();

	if( n_draw )
	{
		SortDrawSlots();

		R3DPROFILE_START( "Append Renderables" );

		int prevDrawOrder		= draw[ 0 ].obj->DrawOrder;
		int prevDrawOrderIdx	= g_render_arrays[ rsFillGBuffer ].Count();

		for( int i = 0, e = n_draw; i < e; i ++ )
		{
			draw[ i ].obj->InMainFrustum = 1;

			if( prevDrawOrder != draw[ i ].obj->DrawOrder )
			{
				if( !g_SortByMaterial )
				{
					ClearMaterialBits( g_render_arrays[ rsFillGBuffer ], prevDrawOrderIdx );
				}

				if( r_score_sort->GetInt() )
				{
					AppendRenderArrayMatScoreInfo( g_render_arrays[ rsFillGBuffer ] );
				}

				R3DPROFILE_START("Sort Render Array");

				SortRenderArray( g_render_arrays[ rsFillGBuffer ], prevDrawOrderIdx );

				R3DPROFILE_END("Sort Render Array");

				prevDrawOrder		= draw[ i ].obj->DrawOrder;
				prevDrawOrderIdx	= g_render_arrays[ rsFillGBuffer ].Count();
			}

			//if (draw[ i ].obj->Class->Name != "obj_Building")
			draw[ i ].obj->AppendRenderables( g_render_arrays, Cam );
		}

		if( g_render_arrays[ rsFillGBufferEffects ].Count() )
		{
			SortRenderArray( g_render_arrays[ rsFillGBufferEffects ], 0 );
		}

		if( g_render_arrays[ rsFillGBufferAfterEffects ].Count() )
		{
			SortRenderArray( g_render_arrays[ rsFillGBufferAfterEffects ], 0 );
		}

		r_last_def_rend_count->SetInt( g_render_arrays[ rsFillGBuffer ].Count() ) ;

		if (g_render_arrays[rsDrawTransparents].Count())
		{
			SortRenderArray(g_render_arrays[rsDrawTransparents], 0);
		}

		// don't care about DrawOrder in shadow maps
		if( g_render_arrays[ rsCreateSM ].Count() )
		{
			if( !g_SortByMaterial )
			{
				ClearMaterialBits( g_render_arrays[ rsCreateSM ], 0 );
			}

			SortRenderArray( g_render_arrays[ rsCreateSM ], 0 );
		}

		R3DPROFILE_END( "Append Renderables" );
	}

	PrepCam = Cam;
}

static void DrawClear( ObjectManager* manager, eRenderStageID stageID )
{
	manager->Draw( stageID );
	g_render_arrays[ stageID ].Clear();
}

void
ObjectManager::WarmUp()
{
	struct EnableDisableDistanceCull
	{
		EnableDisableDistanceCull()
		{
			oldValue = r_allow_distance_cull->GetInt();
			r_allow_distance_cull->SetInt( 0 );
		}

		~EnableDisableDistanceCull()
		{
			r_allow_distance_cull->SetInt( oldValue );
		}

		int oldValue;

	} enableDisableDistanceCull; (void)enableDisableDistanceCull;

	extern r3dCamera gCam ;
	r3dCamera prevCam = gCam ;

	r3dCamera fakeCam;

	fakeCam.NearClip		= 1.f;
	fakeCam.FarClip			= 32000.f;

	fakeCam.vPointTo		= r3dPoint3D( +0.f, -1.f, +0.f );
	fakeCam.vUP				= r3dPoint3D( +0.f, +0.f, +1.f );

	fakeCam.FOV				= 90.f;

	fakeCam.ProjectionType	= r3dCamera::PROJTYPE_ORTHO;

	if( Terrain )
	{
		const r3dTerrainDesc& desc = Terrain->GetDesc() ;

		fakeCam.Width	= desc.XSize ;
		fakeCam.Height	= desc.ZSize ;
	}
	else
	{
		fakeCam.Width	= 1000.f;
		fakeCam.Height	= 1000.f;
	}

	fakeCam.SetPosition( r3dPoint3D( fakeCam.Width * 0.5f, 8000.f, fakeCam.Height * 0.5f ) );

	extern float __r3dGlobalAspect;
	fakeCam.Aspect		= __r3dGlobalAspect ;

	r3dRenderer->SetCamera( fakeCam, false );	

	std::vector<GameObject*>::iterator it;
	//for(GameObject *obj = GW.GetFirstObject(); obj; obj = GW.GetNextObject(obj))
	for(it=gSkipOcclusionCheckGameObjects.begin(); it!=gSkipOcclusionCheckGameObjects.end(); ++it)
	{
		GameObject* obj = *it;
		{
			if(!obj->isActive() || obj->ObjFlags & OBJFLAG_SkipDraw || obj->ObjFlags & OBJFLAG_JustCreated || obj->ObjFlags & OBJFLAG_Removed || !obj->isDetailedVisible())
				continue;

			if (obj->Class->Name != "obj_Building")
				obj->AppendRenderables( g_render_arrays, fakeCam );
		}
	}

	if( Terrain1
#ifndef WO_SERVER
		&& !( Terrain2 && r_terrain2->GetInt() )
#endif // #ifndef WO_SERVER
		)
	{
		Terrain1->DrawDeferredMultipassInitial();
		Terrain1->DrawDeferredMultipass();
	}

	DrawClear( this, rsFillGBuffer					);
	DrawClear( this, rsFillGBufferFirstPerson	);
	DrawClear( this, rsFillGBufferEffects	);
	DrawClear( this, rsFillGBufferAfterEffects	);
	DrawClear( this, rsDrawComposite1		);
	DrawClear( this, rsDrawComposite2	);
	DrawClear( this, rsDrawBloomGlow	);
	DrawClear( this, rsDrawDistortion	);
	DrawClear( this, rsDrawTransparents	);
	DrawClear( this, rsDrawBoundBox		);
	DrawClear( this, rsDrawDepthEffect	);
	DrawClear( this, rsDrawFlashUI		);

#if 0
	IDirect3DQuery9* query;
	D3D_V( r3dRenderer->pd3ddev->CreateQuery( D3DQUERYTYPE_EVENT, &query ) );

	for( ; query->GetData( NULL, 0, D3DGETDATA_FLUSH ) != S_OK  ; ) ;
	// TODO : device lost...

	query->Release();
#endif

	r3dRenderer->SetCamera( prevCam, false );
}

void
ObjectManager::Draw( eRenderStageID DrawState )
{
	DoPreparedDraw( PrepCam, DrawState );
}

void
ObjectManager::DrawIntermediate( eRenderStageID DrawState )
{
	DoPreparedDraw( PrepCamInterm, DrawState );
}

void
ObjectManager::UpdateTransparentShadowCaster( GameObject* obj )
{
	// not added yet - will be resolved when added
	if( obj->ID == invalidGameObjectID )
		return ;

	int present = 0 ;
	for( int i = 0, e = TransparentShadowCasterCount ; i < e; i ++ )
	{
		if( TransparentShadowCasters[ i ] == obj )
		{
			present = 1 ;
			break ;
		}
	}

	if( obj->IsTransparentShadowCaster() )
	{
		if( !present )
			AddToTransparentShadowCasters( obj ) ;
	}
	else
	{
		if( present )
			RemoveFromTransparentShadowCasters( obj ) ;
	}
}

void
ObjectManager::DoPreparedDraw( const r3dCamera& Cam, eRenderStageID DrawState )
{
	R3DPROFILE_FUNCTION("ObjectManager::DoPreparedDraw");
	RenderArray& arr = g_render_arrays[ DrawState ];

	for( uint32_t i = 0, e = arr.Count(); i < e; i ++ )
	{
		Renderable& rend = arr[ i ];
		rend.DrawFunc( &rend, Cam );
	}
}

void
ObjectManager::AddToTransparentShadowCasters( GameObject* obj )
{
	if( obj->IsTransparentShadowCaster() )
	{
		r3dCSHolder cs_holder( m_CS ) ; (void) cs_holder ;

		if( TransparentShadowCasterCount < TransparentShadowCasters.COUNT )
		{
			TransparentShadowCasters[ TransparentShadowCasterCount++ ] = obj ;
		}		
	}
}

void
ObjectManager::RemoveFromTransparentShadowCasters( GameObject* obj )
{
	if( obj->IsTransparentShadowCaster() )
	{
		r3dCSHolder cs_holder( m_CS ) ; (void) cs_holder ;

		for( int i = 0, e = TransparentShadowCasterCount ; i < e ; i ++ )
		{
			if( TransparentShadowCasters[ i ] == obj )
			{
				TransparentShadowCasters.Move( i, i + 1, TransparentShadowCasterCount - i - 1 ) ;
				TransparentShadowCasterCount -- ;
				break ;
			}
		}
	}
#ifdef _DEBUG
	else
	{
		r3dCSHolder cs_holder( m_CS ) ; (void) cs_holder ;

		// search anyway...
		for( int i = 0, e = TransparentShadowCasterCount ; i < e ; i ++ )
		{
			if( TransparentShadowCasters[ i ] == obj )
			{
				r3dError( "Object with transparent shadow caster flag not set, found among transparent shadow casters array!" ) ;
			}
		}
	}
#endif
}

int ObjectManager::SendEvent_to_ObjClass(const char* name, int event, void *data)
{
	GameObject	*obj;

	for(obj = GetFirstObject(); obj; obj = GetNextObject(obj)) {
		if(obj->Class->Name == name)
			return obj->OnEvent(event, data);
	}

	return 0;
}


int ObjectManager::SendEvent_to_ObjName(const char* name, int event, void *data)
{
	GameObject	*obj;

	for(obj = GetFirstObject(); obj; obj = GetNextObject(obj)) {
		if(obj->Name == name)
			return obj->OnEvent(event, data);
	}

	return 0;
}


extern	r3dCamera	gCam;

//--------------------------------------------------------------------------------------------------------
bool ObjectManager::GetSnapPoint( const r3dPoint2D & vCursor, const SnapInfo_t &tInfo, SnapPointResult_t &tRes )
{
	float fDistTrace = FLT_MAX;


	r3dPoint3D vDir;
	r3dScreenTo3D( vCursor.x, vCursor.y, &vDir );


	D3DXMATRIX mVP;
	D3DXVECTOR3 vT;

	D3DXMatrixMultiply( &mVP, &r3dRenderer->ViewMatrix, &r3dRenderer->ProjMatrix );


	if ( tInfo.eType != eSnapType_Vertex )
	{
		r3dError( "current snap mode(%d) not implemented\n", tInfo.eType );
		return false;
	}

	float fRadiusSq = tInfo.fRadius * tInfo.fRadius;


	for( ObjectIterator iter = GetFirstOfAllObjects(); iter.current; iter = GetNextOfAllObjects( iter ) )
	{
		GameObject* pObj = iter.current;

		if( ! pObj->isActive() || pObj->ObjFlags & ( OBJFLAG_SkipCastRay | OBJFLAG_Removed ) )
			continue;

		if ( ! pObj->isObjType(OBJTYPE_Mesh) )
			continue;


		const D3DXMATRIX &m = pObj->GetTransformMatrix();

		// check bound box 
		r3dBoundBox bbox = pObj->GetBBoxWorld();

		r3dPoint2D vMin;
		r3dPoint2D vMax;
		vMin = r3dPoint2D( FLT_MAX, FLT_MAX );
		vMax = r3dPoint2D( -FLT_MAX, -FLT_MAX );

		r3dPoint3D dPoints[8];

		dPoints[ 0 ] = r3dPoint3D( bbox.Org.x, bbox.Org.y, bbox.Org.z ), 
			dPoints[ 1 ] = r3dPoint3D( bbox.Org.x, bbox.Org.y + bbox.Size.y, bbox.Org.z );
		dPoints[ 2 ] = r3dPoint3D( bbox.Org.x, bbox.Org.y + bbox.Size.y, bbox.Org.z + bbox.Size.z );
		dPoints[ 3 ] = r3dPoint3D( bbox.Org.x, bbox.Org.y, bbox.Org.z + bbox.Size.z );

		dPoints[ 4 ] = r3dPoint3D( bbox.Org.x + bbox.Size.x, bbox.Org.y, bbox.Org.z );
		dPoints[ 5 ] = r3dPoint3D( bbox.Org.x + bbox.Size.x, bbox.Org.y + bbox.Size.y, bbox.Org.z );
		dPoints[ 6 ] = r3dPoint3D( bbox.Org.x + bbox.Size.x, bbox.Org.y + bbox.Size.y, bbox.Org.z + bbox.Size.z );
		dPoints[ 7 ] = r3dPoint3D( bbox.Org.x + bbox.Size.x, bbox.Org.y, bbox.Org.z + bbox.Size.z );

		for ( int i = 0; i < 8; i++ )
		{
			r3dPoint3D vPos;
			D3DXVec3TransformCoord( ( D3DXVECTOR3 * ) &vPos, ( D3DXVECTOR3 * ) &dPoints[ i ], ( D3DXMATRIX *) &mVP );

			r3dPoint3D v;

			v.x = ( vPos.x + 1 ) * r3dRenderer->ScreenW * 0.5f;
			v.y = ( -vPos.y + 1 ) * r3dRenderer->ScreenH * 0.5f;

			vMin = r3dPoint2D( R3D_MIN( vMin.x, v.x ), R3D_MIN( vMin.y, v.y ) );
			vMax = r3dPoint2D( R3D_MAX( vMax.x, v.x ), R3D_MAX( vMax.y, v.y ) );
		}


		vMin -= r3dPoint2D( tInfo.fRadius, tInfo.fRadius );
		vMax += r3dPoint2D( tInfo.fRadius, tInfo.fRadius );


		if ( vMin.x > vCursor.x || vMin.y > vCursor.y )
			continue;


		if ( vMax.x < vCursor.x || vMax.y< vCursor.y )
			continue;


		/*float fSizeX =  vMax.x - vMin.x;
		float fSizeY =  vMax.y - vMin.y;

		r3dDrawBox2DWireframe( vMin.x, vMin.y, fSizeX, fSizeY, r3dColor( 0xff2020ff ) );*/


		// against mesh

		float fRadius =  tInfo.fRadius / ( r3dRenderer->ScreenW * 0.5f );

		r3dMesh * pMesh = ((MeshGameObject *)pObj)->MeshLOD[ 0 ];
		for ( int i = 0; i < pMesh->GetNumVertexes(); i++ )
		{		
			r3dVector vRes;
			r3dVector vTransf;

			const r3dPoint3D &vPos = pMesh->GetVertexPos( i );

			D3DXVec3TransformCoord( ( D3DXVECTOR3 * )&vTransf, ( D3DXVECTOR3 * ) &vPos, &m );
			D3DXVec3TransformCoord( ( D3DXVECTOR3 * )&vRes, ( D3DXVECTOR3 * )&vTransf, &mVP );

			r3dPoint2D vVP;
			vVP.x = ( vRes.x + 1 ) * r3dRenderer->ScreenW * 0.5f;
			vVP.y = ( -vRes.y + 1 ) * r3dRenderer->ScreenH * 0.5f;

			r3dPoint2D v = vCursor - r3dPoint2D( vVP.x, vVP.y );


			if ( fRadiusSq < v.LengthSq() )
				continue;

			if ( fDistTrace <  vRes.z )
				continue;

			// do trace
			r3dVector vRayF = vTransf - gCam;
			float f = vRayF.Length();
			r3dVector vRayDir = vRayF / f;
			f -= 0.001f;

			CollisionInfo cl;
			GameObject * pObjTr = CastRay( gCam, vRayDir, f, &cl );
			if ( pObjTr && f >= ( cl.NewPosition - gCam ).Length() )
				continue;


			tRes.pObj = pObj;
			tRes.vPos = vTransf;

			fDistTrace = vRes.z;
		}


	}
	return tRes.pObj != NULL;
}

void ObjectManager::OnResetDevice()
{
	m_pRootBox->onResetDevice();
}

//////////////////////////////////////////////////////////////////////////

void ObjectManager::RecalcIntermObjectMatrices()
{
	//	Prepare matrices for render. Do it here in parallel manner, to speedup calculation.
	R3DPROFILE_FUNCTION("RecalcIntermObjectMatrices");
	g_pJobChief->Exec(&PrecalculateShadowWorldMatrices, &draw_interm[0], n_draw_interm);
}

//////////////////////////////////////////////////////////////////////////

void ObjectManager::RecalcObjectMatrices()
{
	//	Prepare matrices for render. Do it here in parallel manner, to speedup calculation.
	R3DPROFILE_FUNCTION("RecalcObjectMatrices");
	g_pJobChief->Exec(&PrecalculateWorldMatrices, &draw[0], n_draw);
}

SceneBox* ObjectManager::GetRoot() const
{
	return m_pRootBox ;
}

void ObjectManager::OnGameEnded()
{
	for(GameObject* obj = pFirstObject; obj; obj = obj->pNextObject)
	{
		obj->OnGameEnded();
	}

	for( int i = 0, e = NumStaticObjects; i < e; i ++ )
	{
		pStaticObjectArray[ i ]->OnGameEnded();
	}

}

ObjectManagerResourceHelper::ObjectManagerResourceHelper()
: r3dIResource( r3dIntegrityGuardian() )
{

}

ObjectManagerResourceHelper::~ObjectManagerResourceHelper()
{

}

void DumpPhysStats()
{
	int totalCount = 0 ;
	int sleeping = 0 ;
	int controversialWakers = 0 ;
	int controversialSleepers = 0 ;
	int physCount = 0 ;

	for( ObjectIterator iter = GameWorld().GetFirstOfAllObjects(); iter.current ; iter = GameWorld().GetNextOfAllObjects( iter ) )
	{
		GameObject* obj = iter.current;
		if( obj->PhysicsObject )
		{
			physCount ++ ;

			if( obj->ObjFlags & OBJFLAG_ForceSleep )
			{
				sleeping ++ ;

				if( !obj->PhysicsObject->IsSleeping() )
				{
					controversialSleepers ++ ;
				}
			}
			else
			{
				if( obj->PhysicsObject->IsStatic() )
				{
					controversialWakers ++ ;
				}
			}
		}

		totalCount ++ ;
	}

	void ConPrint( const char* , ... );
	ConPrint( "Total objects: %d\n", totalCount ) ;
	ConPrint( "Physics objects: %d\n", physCount ) ;
	ConPrint( "Sleeping objects: %d\n", sleeping ) ;
	ConPrint( "Controversial wakers: %d\n", controversialWakers ) ;
	ConPrint( "Controversial sleepers: %d\n", controversialSleepers ) ;
}

//VMPROTECT_End();