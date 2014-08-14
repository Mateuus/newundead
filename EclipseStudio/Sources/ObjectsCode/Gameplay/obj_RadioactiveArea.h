#pragma once

#include "GameCommon.h"
#include "../../EclipseStudio/Sources/ObjectsCode/EFFECTS/obj_ParticleSystem.h"
#include "../../EclipseStudio/Sources/ObjectsCode/world/Lamp.h"

class obj_RadioactiveArea : public MeshGameObject
{
	DECLARE_CLASS(obj_RadioactiveArea, MeshGameObject)
	
public:
	float		useRadius;
	float		RAreaX;
	float		RAreaY;
	float		RAreaZ;
		
	static std::vector<obj_RadioactiveArea*> LoadedRadioactiveArea;
public:
	obj_RadioactiveArea();
	class obj_ParticleSystem* m_ParticleRadioActive;
	virtual ~obj_RadioactiveArea();

	virtual	BOOL		Load(const char *name);

	virtual	BOOL		OnCreate();
	virtual	BOOL		OnDestroy();

	virtual	BOOL		Update();
 #ifndef FINAL_BUILD
 	virtual	float		DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected) OVERRIDE;
 #endif
	virtual	void		AppendRenderables(RenderArray (& render_arrays  )[ rsCount ], const r3dCamera& Cam);
	virtual void		WriteSerializedData(pugi::xml_node& node);
	virtual	void		ReadSerializedData(pugi::xml_node& node);
	virtual void		CreateArea(float x,float y,float z);

};
