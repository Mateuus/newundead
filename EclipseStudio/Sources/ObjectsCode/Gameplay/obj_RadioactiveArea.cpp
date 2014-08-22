#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "obj_RadioactiveArea.h"
#include "XMLHelpers.h"
#include "../EFFECTS/obj_ParticleSystem.h"

extern bool g_bEditMode;
IMPLEMENT_CLASS(obj_RadioactiveArea, "obj_RadioactiveArea", "Object");
AUTOREGISTER_CLASS(obj_RadioactiveArea);

std::vector<obj_RadioactiveArea*> obj_RadioactiveArea::LoadedRadioactiveArea;

namespace
{
	struct obj_RadioactiveAreaCompositeRenderable: public Renderable
	{
		void Init()
		{
			DrawFunc = Draw;
		}

		static void Draw( Renderable* RThis, const r3dCamera& Cam )
		{
			obj_RadioactiveAreaCompositeRenderable *This = static_cast<obj_RadioactiveAreaCompositeRenderable*>(RThis);

			r3dRenderer->SetRenderingMode(R3D_BLEND_NZ | R3D_BLEND_PUSH);

			r3dDrawLine3D(This->Parent->GetPosition(), This->Parent->GetPosition() + r3dPoint3D(0, 20.0f, 0), Cam, 0.4f, r3dColor24::green);
			r3dDrawCircle3D(This->Parent->GetPosition(), This->Parent->useRadius, Cam, 0.1f, r3dColor24::green);
			r3dRenderer->Flush();
			r3dRenderer->SetRenderingMode(R3D_BLEND_POP);
		}

		obj_RadioactiveArea *Parent;
	};
}

obj_RadioactiveArea::obj_RadioactiveArea()
{
	useRadius = 10.0f;
}

obj_RadioactiveArea::~obj_RadioactiveArea()
{
}

#define RENDERABLE_OBJ_USER_SORT_VALUE (3*RENDERABLE_USER_SORT_VALUE)
void obj_RadioactiveArea::AppendRenderables(RenderArray (& render_arrays  )[ rsCount ], const r3dCamera& Cam)
{
	parent::AppendRenderables(render_arrays, Cam);
#ifdef FINAL_BUILD
	return;
#else
	if(g_bEditMode)
	{
		obj_RadioactiveAreaCompositeRenderable rend;

		rend.Init();
		rend.Parent		= this;
		rend.SortValue	= RENDERABLE_OBJ_USER_SORT_VALUE;

		render_arrays[ rsDrawDebugData ].PushBack( rend );
	}
#endif
}

void obj_RadioactiveArea::ReadSerializedData(pugi::xml_node& node)
{
	GameObject::ReadSerializedData(node);

	pugi::xml_node objNode = node.child("RadioActive_Area");
	GetXMLVal("useRadius", objNode, &useRadius);
	GetXMLVal("RAreaX", objNode, &RAreaX);
	GetXMLVal("RAreaY", objNode, &RAreaY);
	GetXMLVal("RAreaZ", objNode, &RAreaZ);
	if(!g_bEditMode)
	{
		CreateArea(RAreaX,RAreaY,RAreaZ);
	}
}

void obj_RadioactiveArea::WriteSerializedData(pugi::xml_node& node)
{
	GameObject::WriteSerializedData(node);

	pugi::xml_node objNode = node.append_child();
	objNode.set_name("RadioActive_Area");
	SetXMLVal("useRadius", objNode, &useRadius);
	SetXMLVal("RAreaX", objNode, &GetPosition().x);
	SetXMLVal("RAreaY", objNode, &GetPosition().y);
	SetXMLVal("RAreaZ", objNode, &GetPosition().z);
}

void obj_RadioactiveArea::CreateArea(float x,float y,float z)
{
	obj_ParticleSystem* m_RadiactiveArea;
	m_RadiactiveArea = (obj_ParticleSystem*)srv_CreateGameObject("obj_ParticleSystem", "Radiactive", r3dPoint3D(x,y,z) + r3dPoint3D(0, -1.0f, 0) );
}

BOOL obj_RadioactiveArea::Load(const char *fname)
{
	const char* cpMeshName = "Data\\ObjectsDepot\\WZ_TownProps\\bld_gas_barrel_02.sco";

	if(!parent::Load(cpMeshName)) 
		return FALSE;

	return TRUE;
}

BOOL obj_RadioactiveArea::OnCreate()
{
	parent::OnCreate();

	LoadedRadioactiveArea.push_back(this);
	return 1;
}


BOOL obj_RadioactiveArea::OnDestroy()
{
	LoadedRadioactiveArea.erase(std::find(LoadedRadioactiveArea.begin(), LoadedRadioactiveArea.end(), this));
	return parent::OnDestroy();
}

BOOL obj_RadioactiveArea::Update()
{
	return parent::Update();
}

//------------------------------------------------------------------------
#ifndef FINAL_BUILD
float obj_RadioactiveArea::DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected)
{
	float starty = scry;

	starty += parent::DrawPropertyEditor(scrx, scry, scrw,scrh, startClass, selected );

	if( IsParentOrEqual( &ClassData, startClass ) )
	{		
		starty += imgui_Static ( scrx, starty, "Radioactive Area Parameters" );
		starty += imgui_Value_Slider(scrx, starty, "Radioactive Radius", &useRadius, 0, 500.0f, "%.0f");
	}

	return starty-scry;
}
#endif
