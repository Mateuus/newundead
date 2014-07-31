#include "r3dPCH.h"
#include "r3d.h"

#include "Gameplay_Params.h"
#include "GameCommon.h"
#include "../ai/AI_Player.H"

#include "obj_ItemSpawnPoint.h"
#include "gameobjects/obj_Mesh.h"

#include "multiplayer/P2PMessages.h"
#include "multiplayer/ClientGameLogic.h"
#include "../WEAPONS/WeaponArmory.h"

#include "Editors/ObjectManipulator3d.h"

IMPLEMENT_CLASS(obj_ItemSpawnPoint, "obj_ItemSpawnPoint", "Object");
AUTOREGISTER_CLASS(obj_ItemSpawnPoint);

extern bool g_bEditMode;

obj_ItemSpawnPoint::obj_ItemSpawnPoint()
{
	ObjTypeFlags |= OBJTYPE_ItemSpawnPoint;
	m_SelectedSpawnPoint = 0;
	m_bEditorCheckSpawnPointAtStart = true;
}

obj_ItemSpawnPoint::~obj_ItemSpawnPoint()
{
}

BOOL obj_ItemSpawnPoint::Load(const char *fname)
{
#ifndef FINAL_BUILD
	if(!g_bEditMode)
#endif
	{
		return TRUE; // do not load meshes in deathmatch mode, not showing control points
	}
	const char* cpMeshName = "Data\\ObjectsDepot\\Capture_Points\\Flag_Pole_01.sco";

	if(!parent::Load(cpMeshName)) 
		return FALSE;

	return TRUE;
}

BOOL obj_ItemSpawnPoint::OnCreate()
{
	parent::OnCreate();

#ifndef FINAL_BUILD
	if(!g_bEditMode)
#endif
		ObjFlags |= OBJFLAG_SkipCastRay;

#ifndef FINAL_BUILD
	if(g_bEditMode) // to make it easier in editor to edit spawn points
	{
		setSkipOcclusionCheck(true);
		ObjFlags |= OBJFLAG_AlwaysDraw | OBJFLAG_ForceSleep ;
	}
#endif

	r3dBoundBox bboxLocal ;
	bboxLocal.Size = r3dPoint3D(2, 2, 2);
	bboxLocal.Org = -bboxLocal.Size * 0.5f;
	SetBBoxLocal(bboxLocal) ;
	UpdateTransform();

	return 1;
}


BOOL obj_ItemSpawnPoint::OnDestroy()
{
	return parent::OnDestroy();
}

BOOL obj_ItemSpawnPoint::Update()
{
	static int delayCheckSpawnPointsInAir = 100;
	if(delayCheckSpawnPointsInAir)
		--delayCheckSpawnPointsInAir;
	if(g_bEditMode && m_bEditorCheckSpawnPointAtStart && delayCheckSpawnPointsInAir ==0)
	{
		m_bEditorCheckSpawnPointAtStart = false;
		for(ITEM_SPAWN_POINT_VECTOR::iterator it=m_SpawnPointsV.begin(); it!=m_SpawnPointsV.end(); ++it)
		{
			PxVec3 from(it->pos.x, it->pos.y+1.0f, it->pos.z);
			PxRaycastHit hit;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC));
			if(g_pPhysicsWorld->raycastSingle(from, PxVec3(0,-1,0), 10000, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter))
			{
				r3dPoint3D hitPos(hit.impact.x, hit.impact.y, hit.impact.z);
				if(R3D_ABS(it->pos.y - hitPos.y) > 2.0f)
				{
					r3dArtBug("Item CP Spawn at %.2f, %.2f, %.2f has spawn position(s) in the air. Please select it and use Check Locations button\n", 
						GetPosition().x, GetPosition().y, GetPosition().z);
					break;
				}
			}
		}
		r3dShowArtBugs();
	}

	return parent::Update();
}

//------------------------------------------------------------------------
struct ItemControlPointShadowGBufferRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		ItemControlPointShadowGBufferRenderable *This = static_cast<ItemControlPointShadowGBufferRenderable*>( RThis );
		This->Parent->MeshGameObject::Draw( Cam, This->DrawState );
	}

	obj_ItemSpawnPoint*	Parent;
	eRenderStageID		DrawState;
};

struct ItemControlPointCompositeRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		ItemControlPointCompositeRenderable *This = static_cast<ItemControlPointCompositeRenderable*>( RThis );

		This->Parent->DoDrawComposite( Cam );
	}

	obj_ItemSpawnPoint*	Parent;	
};



#define RENDERABLE_CTRLPOINT_SORT_VALUE (6*RENDERABLE_USER_SORT_VALUE)

void obj_ItemSpawnPoint::AppendShadowRenderables( RenderArray & rarr, const r3dCamera& Cam ) /*OVERRIDE*/
{
#ifndef FINAL_BUILD
	if(!g_bEditMode)
#endif
		return;

	ItemControlPointShadowGBufferRenderable rend;

	rend.Init();
	rend.Parent		= this;
	rend.SortValue	= RENDERABLE_CTRLPOINT_SORT_VALUE;
	rend.DrawState	= rsCreateSM;

	rarr.PushBack( rend );
}

//------------------------------------------------------------------------
void obj_ItemSpawnPoint::AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam ) /*OVERRIDE*/
{
#ifndef FINAL_BUILD
	if(!g_bEditMode)
#endif
		return;

	// gbuffer
	{
		ItemControlPointShadowGBufferRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_CTRLPOINT_SORT_VALUE;
		rend.DrawState	= rsFillGBuffer;

		render_arrays[ rsFillGBuffer ].PushBack( rend );
	}

#ifndef FINAL_BUILD
	if(r_hide_icons->GetInt())
		return;

	float idd = r_icons_draw_distance->GetFloat();
	idd *= idd;

	// composite
	if( g_bEditMode && ( Cam - GetPosition() ).LengthSq() < idd )
	{
		ItemControlPointCompositeRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_CTRLPOINT_SORT_VALUE;

		render_arrays[ rsDrawDebugData ].PushBack( rend );
	}
#endif
}

//------------------------------------------------------------------------
void obj_ItemSpawnPoint::DoDrawComposite( const r3dCamera& Cam )
{
#ifndef FINAL_BUILD
	// draw them, so we can see them
	r3dPoint3D off(0, 4, 0);
	r3dColor   clr(0, 255, 0);
	
	r3dRenderer->SetRenderingMode(R3D_BLEND_NZ | R3D_BLEND_PUSH);
	
	r3dDrawLine3D(GetPosition(), GetPosition() + r3dPoint3D(0, 20.0f, 0), Cam, 0.4f, clr);

	// draw circles
	for(size_t i=0; i<m_SpawnPointsV.size(); ++i)
	{
		//r3dDrawCircle3D(m_SpawnPointsV[i].pos, 2.0f, Cam, 0.4f, ((i==m_SelectedSpawnPoint&&g_Manipulator3d.PickedObject() == this)?r3dColor24::red:r3dColor24::grey));
        //r3dDrawLine3D(m_SpawnPointsV[i].pos-r3dPoint3D(0.25f,0,0), m_SpawnPointsV[i].pos+r3dPoint3D(0.25f,0,0), Cam, 0.5f, ((i==m_SelectedSpawnPoint&&g_Manipulator3d.PickedObject() == this)?r3dColor24::red:r3dColor24::grey));
		r3dBoundBox box = m_SpawnPointsV[i].GetDebugBBox();
		r3dDrawBoundBox(box, Cam, ((i==m_SelectedSpawnPoint&&g_Manipulator3d.PickedObject() == this)?r3dColor24::red:r3dColor24::grey), 0.01f, false );
	}
	r3dRenderer->Flush();
	r3dRenderer->SetRenderingMode(R3D_BLEND_POP);
#endif
}

//------------------------------------------------------------------------
#ifndef FINAL_BUILD
float obj_ItemSpawnPoint::DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected)
{
	float starty = scry;

	starty += parent::DrawPropertyEditor(scrx, scry, scrw,scrh, startClass, selected );

	if( IsParentOrEqual( &ClassData, startClass ) )
	{		
		starty += imgui_Static ( scrx, starty, "Item Spawn Point Parameters" );
		starty += imgui_Value_Slider(scrx, starty, "Tick Period (sec)", &m_TickPeriod, 1.0f, 50000.0f, "%.2f");
		starty += imgui_Value_Slider(scrx, starty, "Cooldown (sec)", &m_Cooldown, 1.0f, 50000.0f, "%.2f");
		starty += imgui_Value_Slider(scrx, starty, "De-spawn (sec)", &m_DestroyItemTimer, 0.0f, 50000.0f, "%.2f");
		int isOneItemSpawn = m_OneItemSpawn?1:0;
		starty += imgui_Checkbox(scrx, starty, "One Item Spawn", &isOneItemSpawn, 1);
		m_OneItemSpawn = isOneItemSpawn?true:false;
		
		if(!m_OneItemSpawn)
		{
			static stringlist_t lootBoxNames;
			static int* lootBoxIDs = NULL;
			static int numLootBoxes = 0;
			if(numLootBoxes == 0)
			{
				struct tempS
				{
					char* name;
					uint32_t id;
				};
				std::vector<tempS> lootBoxes;
				{
					tempS holder;
					holder.name = "EMPTY";
					holder.id = 0;
					lootBoxes.push_back(holder);		
				}

				g_pWeaponArmory->startItemSearch();
				while(g_pWeaponArmory->searchNextItem())
				{
					uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
					const BaseItemConfig* cfg = g_pWeaponArmory->getConfig(itemID);
					if( cfg->category == storecat_LootBox )
					{
						tempS holder;
						holder.name = cfg->m_StoreName;
						holder.id = cfg->m_itemID;
						lootBoxes.push_back(holder);						 
					}
				}
				numLootBoxes = (int)lootBoxes.size();
				lootBoxIDs = new int[numLootBoxes];
				for(int i=0; i<numLootBoxes; ++i)
				{
					lootBoxNames.push_back(lootBoxes[i].name);
					lootBoxIDs[i] = lootBoxes[i].id;
				}
			}

			int sel = 0;
			static float offset = 0;
			for(int i=0; i<numLootBoxes; ++i)
				if(m_LootBoxID == lootBoxIDs[i])
					sel = i;
			starty += imgui_Static ( scrx, starty, "Loot box:" );
			if(imgui_DrawList(scrx, starty, 360, 122, lootBoxNames, &offset, &sel))
			{
				m_LootBoxID = lootBoxIDs[sel];
				PropagateChange( m_LootBoxID, &obj_ItemSpawnPoint::m_LootBoxID, this, selected ) ;
			}
			starty += 122;
		}
		else
		{
			starty += imgui_Value_SliderI(scrx, starty, "ItemID", (int*)&m_LootBoxID, 0, 1000000, "%d", false);
			PropagateChange( m_LootBoxID, &obj_ItemSpawnPoint::m_LootBoxID, this, selected ) ;
		}
		
		// don't allow multi edit of this
		if( selected.Count() <= 1 )
		{
			{
				if(imgui_Button(scrx, starty, 200, 25, "Check locations"))
				{
					for(ITEM_SPAWN_POINT_VECTOR::iterator it=m_SpawnPointsV.begin(); it!=m_SpawnPointsV.end(); ++it)
					{
						PxVec3 from(it->pos.x, it->pos.y+1.0f, it->pos.z);
						PxRaycastHit hit;
						PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK,0,0,0), PxSceneQueryFilterFlags(PxSceneQueryFilterFlag::eSTATIC));
						if(g_pPhysicsWorld->raycastSingle(from, PxVec3(0,-1,0), 10000, PxSceneQueryFlags(PxSceneQueryFlag::eIMPACT), hit, filter))
						{
							r3dPoint3D hitPos(hit.impact.x, hit.impact.y, hit.impact.z);
							if(R3D_ABS(it->pos.y - hitPos.y) > 2.0f)
							{
								gCam = it->pos + r3dPoint3D(0,1,0);
								break;
							}
						}
					}
				}
				starty += 30;
			}

			if(imgui_Button(scrx+110, starty, 100, 25, "Add Location"))
			{
				ItemSpawn itemSpawn;
				itemSpawn.pos = GetPosition() + r3dPoint3D(2, 0, 2);
				m_SpawnPointsV.push_back(itemSpawn);
				m_SelectedSpawnPoint = m_SpawnPointsV.size()-1;
			}

			starty += 25;

			int i=0;
			for(ITEM_SPAWN_POINT_VECTOR::iterator it=m_SpawnPointsV.begin(); it!=m_SpawnPointsV.end(); )
			{
				// selection button
				char tempStr[32];
				sprintf(tempStr, "Location %d", i+1);
				if(imgui_Button(scrx, starty, 100, 25, tempStr, i==m_SelectedSpawnPoint))
				{
					// shift click on location will set camera to it
					if(Keyboard->IsPressed(kbsLeftShift))
					{
						extern BaseHUD* HudArray[6];
						extern int CurHUDID;
						HudArray[CurHUDID]->FPS_Position = m_SpawnPointsV[i].pos;
						HudArray[CurHUDID]->FPS_Position.y += 0.1f;
					}
					
					m_SelectedSpawnPoint = i;
				}

				// delete button
				if(m_SpawnPointsV.size() > 1)
				{
					if(imgui_Button(scrx + 110, starty, 100, 25, "DEL"))
					{
						it = m_SpawnPointsV.erase(it);
						continue;
					}
					m_SelectedSpawnPoint = R3D_CLAMP(m_SelectedSpawnPoint, 0, (int)m_SpawnPointsV.size()-1);
				}

				starty += 25;

				++it;
				++i;
			}

			extern r3dPoint3D UI_TargetPos;
			if((Mouse->IsPressed(r3dMouse::mLeftButton)) && Keyboard->IsPressed(kbsLeftControl))
				m_SpawnPointsV[m_SelectedSpawnPoint].pos = UI_TargetPos;

		}
	}

	return starty-scry;
}
#endif

BOOL obj_ItemSpawnPoint::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	return TRUE;
}
