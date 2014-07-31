#ifndef FINAL_BUILD
#pragma once

#include "GameCommon.h"
class obj_Building;
class obj_VehicleSpawnRadius : public MeshGameObject
{
	DECLARE_CLASS(obj_VehicleSpawnRadius, MeshGameObject)
	
public:
	float		useRadius;
		float minVehicle;
	float maxVehicle;
	static std::vector<obj_VehicleSpawnRadius*> LoadedPostboxes;
public:
	obj_VehicleSpawnRadius();
	virtual ~obj_VehicleSpawnRadius();

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

	void RespawnCar();
	void SetVehicleType( const std::string& preset );
	char				vehicle_Model[64];
	obj_Building*		spawnedVehicle;
};
#endif
