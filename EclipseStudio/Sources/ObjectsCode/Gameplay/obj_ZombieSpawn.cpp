//=========================================================================
//	Module: obj_ZombieSpawn.cpp
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "obj_ZombieSpawn.h"
#include "../../XMLHelpers.h"
#include "../../Editors/LevelEditor.h"
#include "../WEAPONS/WeaponArmory.h"

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS(obj_ZombieSpawn, "obj_ZombieSpawn", "Object");
AUTOREGISTER_CLASS(obj_ZombieSpawn);

extern bool g_bEditMode;

//////////////////////////////////////////////////////////////////////////

namespace
{
//////////////////////////////////////////////////////////////////////////

	struct ZombieSpawnCompositeRenderable: public Renderable
	{
		void Init()
		{
			DrawFunc = Draw;
		}

		static void Draw( Renderable* RThis, const r3dCamera& Cam )
		{
			ZombieSpawnCompositeRenderable *This = static_cast<ZombieSpawnCompositeRenderable*>(RThis);

			r3dRenderer->SetRenderingMode(R3D_BLEND_NZ | R3D_BLEND_PUSH);

			r3dDrawLine3D(This->Parent->GetPosition(), This->Parent->GetPosition() + r3dPoint3D(0, 20.0f, 0), Cam, 0.4f, r3dColor24::grey);
			r3dDrawCircle3D(This->Parent->GetPosition(), This->Parent->spawnRadius, Cam, 0.1f, r3dColor(0, 255, 0));

			r3dRenderer->Flush();
			r3dRenderer->SetRenderingMode(R3D_BLEND_POP);
		}

		obj_ZombieSpawn *Parent;	
	};
}

//////////////////////////////////////////////////////////////////////////

obj_ZombieSpawn::obj_ZombieSpawn()
: spawnRadius(30.0f)
, zombieSpawnDelay(15.0f)
, maxZombieCount(50)
, minDetectionRadius(1)
, maxDetectionRadius(3.0f)
, sleepersRate(10.0f)
, lootBoxID(0)
, fastZombieChance(50.0f)
, speedVariation(5.0f)
{
	m_bEnablePhysics = false;
}

//////////////////////////////////////////////////////////////////////////

obj_ZombieSpawn::~obj_ZombieSpawn()
{

}

//////////////////////////////////////////////////////////////////////////

#define RENDERABLE_OBJ_USER_SORT_VALUE (3*RENDERABLE_USER_SORT_VALUE)
void obj_ZombieSpawn::AppendRenderables(RenderArray (& render_arrays  )[ rsCount ], const r3dCamera& Cam)
{
#ifdef FINAL_BUILD
	return;
#else
	if ( !g_bEditMode )
		return;

	if(r_hide_icons->GetInt())
		return;

	float idd = r_icons_draw_distance->GetFloat();
	idd *= idd;

	if( ( Cam - GetPosition() ).LengthSq() > idd )
		return;

	ZombieSpawnCompositeRenderable rend;

	rend.Init();
	rend.Parent		= this;
	rend.SortValue	= RENDERABLE_OBJ_USER_SORT_VALUE;

	render_arrays[ rsDrawDebugData ].PushBack( rend );
#endif
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_ZombieSpawn::OnCreate()
{
	parent::OnCreate();

	DrawOrder = OBJ_DRAWORDER_LAST;

	r3dBoundBox bboxLocal ;
	bboxLocal.Size = r3dPoint3D(2, 2, 2);
	bboxLocal.Org = -bboxLocal.Size * 0.5f;
	SetBBoxLocal(bboxLocal) ;
	UpdateTransform();

	return 1;
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_ZombieSpawn::Update()
{
	return parent::Update();
}

//////////////////////////////////////////////////////////////////////////

BOOL obj_ZombieSpawn::OnDestroy()
{
	return parent::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////

#ifndef FINAL_BUILD
float obj_ZombieSpawn::DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected)
{
	float y = scry;

	y += parent::DrawPropertyEditor(scrx, scry, scrw, scrh, startClass, selected);
	y += 5.0f;

	y += imgui_Static(scrx, y, "Spawn parameters:");
	int zc = maxZombieCount;
	y += imgui_Value_SliderI(scrx, y, "Max zombies", &zc, 0, 100.0f, "%d");
	maxZombieCount = zc;
	y += imgui_Value_Slider(scrx, y, "%% Sleepers", &sleepersRate, 0, 100.0f, "%.0f");
	y += imgui_Value_Slider(scrx, y, "Spawn delay (sec)", &zombieSpawnDelay, 1.0f, 240.0f, "%-02.3f");
	y += imgui_Value_Slider(scrx, y, "Spawn radius", &spawnRadius, 10.0f, 300.0f, "%-02.3f");

	y += imgui_Value_Slider(scrx, y, "%% FastZombie", &fastZombieChance, 0.0f, 100.0f, "%-02.3f");
	y += imgui_Value_Slider(scrx, y, "%% Speed Variation", &speedVariation, 0.0f, 50.0f, "%-02.3f");

	y += 5.0f;
	y += imgui_Static(scrx, y, "Zombie senses parameters:");
	
	y += imgui_Value_Slider(scrx, y, "Min detect radius", &minDetectionRadius, 1.0f, 100.0f, "%-02.3f");
	maxDetectionRadius = std::max(minDetectionRadius, maxDetectionRadius);
	y += imgui_Value_Slider(scrx, y, "Max detect radius", &maxDetectionRadius, 1.0f, 100.0f, "%-02.3f");
	minDetectionRadius = std::min(minDetectionRadius, maxDetectionRadius);

	y += 5.0f;
	y += imgui_Static(scrx, y, "Zombies to spawn:");
	// zombie selection
	{
		static int numZombies = 0;
		struct tempS
		{
			char* name;
			uint32_t id;
		};
		static std::vector<tempS> zombieBoxes;
		if(numZombies == 0)
		{
			g_pWeaponArmory->startItemSearch();
			while(g_pWeaponArmory->searchNextItem())
			{
				uint32_t itemID = g_pWeaponArmory->getCurrentSearchItemID();
				const HeroConfig* cfg = g_pWeaponArmory->getHeroConfig(itemID);
				if( cfg && cfg->m_Weight < 0.0f ) // weight -1 is for zombies only
				{
					tempS holder;
					holder.name = cfg->m_StoreName;
					holder.id = cfg->m_itemID;
					zombieBoxes.push_back(holder);						 
				}
			}
			numZombies = (int)zombieBoxes.size();
		}

		for(int i=0; i<numZombies; ++i)
		{
			int isselected = 0;
			int wasIsSelected = 0;
			isselected = wasIsSelected = (std::find(ZombieSpawnSelection.begin(), ZombieSpawnSelection.end(), zombieBoxes[i].id)!=ZombieSpawnSelection.end())?1:0;
			y += imgui_Checkbox(scrx, y, zombieBoxes[i].name, &isselected, 1);
			if(isselected != wasIsSelected)
			{
				if(wasIsSelected && !isselected)
					ZombieSpawnSelection.erase(std::find(ZombieSpawnSelection.begin(), ZombieSpawnSelection.end(), zombieBoxes[i].id));
				else if(!wasIsSelected && isselected)
					ZombieSpawnSelection.push_back(zombieBoxes[i].id);
			}
		}
	}

	// loot box selection
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
			if(lootBoxID == lootBoxIDs[i])
				sel = i;
		y += imgui_Static ( scrx, y, "Loot box:" );
		if(imgui_DrawList(scrx, y, 360, 122, lootBoxNames, &offset, &sel))
		{
			lootBoxID = lootBoxIDs[sel];
		}
		y += 122;
	}

	return y - scry;
}
#endif

//////////////////////////////////////////////////////////////////////////

void obj_ZombieSpawn::WriteSerializedData(pugi::xml_node& node)
{
	parent::WriteSerializedData(node);
	pugi::xml_node zombieSpawnNode = node.append_child();
	zombieSpawnNode.set_name("spawn_parameters");
	SetXMLVal("spawn_radius", zombieSpawnNode, &spawnRadius);
	int mzc = maxZombieCount;
	SetXMLVal("max_zombies", zombieSpawnNode, &mzc);
	SetXMLVal("sleepers_rate", zombieSpawnNode, &sleepersRate);
	SetXMLVal("zombies_spawn_delay", zombieSpawnNode, &zombieSpawnDelay);
	SetXMLVal("min_detection_radius", zombieSpawnNode, &minDetectionRadius);
	SetXMLVal("max_detection_radius", zombieSpawnNode, &maxDetectionRadius);
	SetXMLVal("lootBoxID", zombieSpawnNode, &lootBoxID);
	SetXMLVal("fastZombieChance", zombieSpawnNode, &fastZombieChance);
	SetXMLVal("speedVariation", zombieSpawnNode, &speedVariation);
	zombieSpawnNode.append_attribute("numZombieSelected") = ZombieSpawnSelection.size();
	for(size_t i=0; i<ZombieSpawnSelection.size(); ++i)
	{
		pugi::xml_node zNode = zombieSpawnNode.append_child();
		char tempStr[32];
		sprintf(tempStr, "z%d", i);
		zNode.set_name(tempStr);
		zNode.append_attribute("id") = ZombieSpawnSelection[i];
	}
}

// NOTE: this function must stay in sync with server version
void obj_ZombieSpawn::ReadSerializedData(pugi::xml_node& node)
{
	parent::ReadSerializedData(node);
	pugi::xml_node zombieSpawnNode = node.child("spawn_parameters");
	GetXMLVal("spawn_radius", zombieSpawnNode, &spawnRadius);
	int mzc = maxZombieCount;
	GetXMLVal("max_zombies", zombieSpawnNode, &mzc);
	maxZombieCount = mzc;
	GetXMLVal("sleepers_rate", zombieSpawnNode, &sleepersRate);
	GetXMLVal("zombies_spawn_delay", zombieSpawnNode, &zombieSpawnDelay);
	GetXMLVal("min_detection_radius", zombieSpawnNode, &minDetectionRadius);
	GetXMLVal("max_detection_radius", zombieSpawnNode, &maxDetectionRadius);
	GetXMLVal("lootBoxID", zombieSpawnNode, &lootBoxID);
	GetXMLVal("fastZombieChance", zombieSpawnNode, &fastZombieChance);
	GetXMLVal("speedVariation", zombieSpawnNode, &speedVariation);
	uint32_t numZombies = zombieSpawnNode.attribute("numZombieSelected").as_uint();
	for(uint32_t i=0; i<numZombies; ++i)
	{
		char tempStr[32];
		sprintf(tempStr, "z%d", i);
		pugi::xml_node zNode = zombieSpawnNode.child(tempStr);
		r3d_assert(!zNode.empty());
		ZombieSpawnSelection.push_back(zNode.attribute("id").as_uint());
	}
}
