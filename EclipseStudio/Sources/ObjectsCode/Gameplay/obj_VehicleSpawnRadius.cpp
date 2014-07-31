#ifndef FINAL_BUILD
#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "obj_VehicleSpawnRadius.h"
#include "XMLHelpers.h"
#include "./../GameEngine/gameobjects/obj_Vehicle.h"

extern bool g_bEditMode;

IMPLEMENT_CLASS(obj_VehicleSpawnRadius, "obj_VehicleSpawnRadius", "Object");
AUTOREGISTER_CLASS(obj_VehicleSpawnRadius);

std::vector<obj_VehicleSpawnRadius*> obj_VehicleSpawnRadius::LoadedPostboxes;

namespace
{
	struct obj_VehicleSpawnRadiusRenderable: public Renderable
	{
		void Init()
		{
			DrawFunc = Draw;
		}

		static void Draw( Renderable* RThis, const r3dCamera& Cam )
		{
			obj_VehicleSpawnRadiusRenderable *This = static_cast<obj_VehicleSpawnRadiusRenderable*>(RThis);

			r3dRenderer->SetRenderingMode(R3D_BLEND_NZ | R3D_BLEND_PUSH);

			r3dDrawLine3D(This->Parent->GetPosition(), This->Parent->GetPosition() + r3dPoint3D(0, 20.0f, 0), Cam, 0.4f, r3dColor24::yellow);
			r3dDrawCircle3D(This->Parent->GetPosition(), This->Parent->useRadius, Cam, 0.1f, r3dColor24::red);

			r3dRenderer->Flush();
			r3dRenderer->SetRenderingMode(R3D_BLEND_POP);
		}

		obj_VehicleSpawnRadius *Parent;	
	};
}

obj_VehicleSpawnRadius::obj_VehicleSpawnRadius()
{
	maxVehicle = 1.0f;
	minVehicle = 1.0f;
	useRadius = 10.0f;
}

obj_VehicleSpawnRadius::~obj_VehicleSpawnRadius()
{
}

#define RENDERABLE_OBJ_USER_SORT_VALUE (3*RENDERABLE_USER_SORT_VALUE)
void obj_VehicleSpawnRadius::AppendRenderables(RenderArray (& render_arrays  )[ rsCount ], const r3dCamera& Cam)
{
	parent::AppendRenderables(render_arrays, Cam);
#ifdef FINAL_BUILD
	return;
#else
	if(g_bEditMode)
	{
		obj_VehicleSpawnRadiusRenderable rend;
		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_OBJ_USER_SORT_VALUE;
		render_arrays[ rsDrawDebugData ].PushBack( rend );
	}
#endif
}

void obj_VehicleSpawnRadius::ReadSerializedData(pugi::xml_node& node)
{
	GameObject::ReadSerializedData(node);

	pugi::xml_node objNode = node.child("VehicleSpawnRadius");
	_snprintf_s( vehicle_Model, 64, "%s", objNode.attribute("VehicleModel").value());
	GetXMLVal("useRadius", objNode, &useRadius);
	GetXMLVal("minVehicle", objNode, &minVehicle);
	GetXMLVal("maxVehicle", objNode, &maxVehicle);
}

void obj_VehicleSpawnRadius::WriteSerializedData(pugi::xml_node& node)
{
	GameObject::WriteSerializedData(node);

	pugi::xml_node objNode = node.append_child();
	objNode.set_name("VehicleSpawnRadius");
	SetXMLVal("useRadius", objNode, &useRadius);
	SetXMLVal("minVehicle", objNode, &minVehicle);
	SetXMLVal("maxVehicle", objNode, &maxVehicle);
	objNode.append_attribute("VehicleModel") = vehicle_Model;
}

BOOL obj_VehicleSpawnRadius::Load(const char *fname)
{
	const char* cpMeshName = "Data\\ObjectsDepot\\WZ_Settlement\\ItemDrop_PostalBox.sco";

	if(!parent::Load(cpMeshName)) 
		return FALSE;

	return TRUE;
}

BOOL obj_VehicleSpawnRadius::OnCreate()
{
	parent::OnCreate();
    spawnedVehicle = NULL;
	LoadedPostboxes.push_back(this);
	return 1;
}


BOOL obj_VehicleSpawnRadius::OnDestroy()
{
	LoadedPostboxes.erase(std::find(LoadedPostboxes.begin(), LoadedPostboxes.end(), this));
	return parent::OnDestroy();
}

BOOL obj_VehicleSpawnRadius::Update()
{
	return parent::Update();
}
void obj_VehicleSpawnRadius::SetVehicleType( const std::string& preset )
{
	_snprintf_s( vehicle_Model, 64, "%s", preset.c_str() );
	RespawnCar();
}
void obj_VehicleSpawnRadius::RespawnCar()
{
#if VEHICLES_ENABLED
	if( spawnedVehicle ) {
		GameWorld().DeleteObject( spawnedVehicle );
		spawnedVehicle = NULL;
	} 

	/*if( vehicle_Model[0] != '\0' ) {
		spawnedVehicle = static_cast<obj_Building*> ( srv_CreateGameObject("obj_Building", vehicle_Model, GetPosition()));
	}*/
#endif
}
//------------------------------------------------------------------------
#ifndef FINAL_BUILD
float obj_VehicleSpawnRadius::DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected)
{
	float starty = scry;

	starty += parent::DrawPropertyEditor(scrx, scry, scrw,scrh, startClass, selected );

	if( IsParentOrEqual( &ClassData, startClass ) )
	{		
		starty += imgui_Static ( scrx, starty, "Vehicle Spawn Parameters" );
		starty += imgui_Value_Slider(scrx, starty, "Vehicle Spawn Radius", &useRadius, 10.0f, 500.0f, "%.0f");
		starty += imgui_Value_Slider(scrx, starty, "MinVehicle", &minVehicle, 1.0f, maxVehicle, "%.0f");
		starty += imgui_Value_Slider(scrx, starty, "MaxVehicle", &maxVehicle, 1.0f, 100.0f, "%.0f");
			static stringlist_t vehicle_nameList;
		static stringlist_t trueVehicleName;
		uint32_t* vehicle_idList = 0;
		if( vehicle_nameList.empty() )
		{

			vehicle_nameList.push_back( "None" );
			trueVehicleName.push_back( "\0" );
			FILE *f = fopen("Data/ObjectsDepot/Vehicles/DrivableVehiclesList.dat","rt");
			while (!feof(f))
			{
				char name[64], fileName[64];
				fscanf(f,"%s %s", name, fileName );
				vehicle_nameList.push_back( name );
				trueVehicleName.push_back( fileName );
			}
			fclose(f);
		}

		static const int width = 250;
		static const int shift = 25;

		static float primVehicleOffset = 0;
		static int selectedVehicleIndex = 0;
		// should we find the current index?
		if( trueVehicleName[selectedVehicleIndex].compare(vehicle_Model) != 0 ) // restore selected item after loading level
		{
			for(uint32_t i=0; i<trueVehicleName.size(); ++i)
			{
				
				if( trueVehicleName[i].compare(vehicle_Model) == 0 )
				{
					selectedVehicleIndex = i;
					break;
				}
			}
		}

		int lastValue = selectedVehicleIndex;

		imgui_DrawList(scrx, starty, (float)width, 150.0f, vehicle_nameList, &primVehicleOffset, &selectedVehicleIndex);
		starty += 150;

		if( lastValue != selectedVehicleIndex ) {

			PropagateChange(trueVehicleName[selectedVehicleIndex], &obj_VehicleSpawnRadius::SetVehicleType, selected ) ;
		}
	}

		if (spawnedVehicle)
		spawnedVehicle->SetPosition(GetPosition());

	return starty-scry;
}
#endif
#endif