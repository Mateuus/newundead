#pragma once

#include "GameCommon.h"
#include "Ammo.h"
#include "APIScaleformGfx.h"

// data driven class. It is created by Ammo class, for bullets that are not immediate action, for example rockets.
class obj_ChemLight : public AmmoShared
{
	DECLARE_CLASS(obj_ChemLight, AmmoShared)
	friend Ammo;
public:
	obj_ChemLight();

	virtual	BOOL		OnCreate();
	virtual BOOL		OnDestroy();
	virtual	BOOL		Update();

	virtual r3dMesh*	GetObjectMesh();
	virtual r3dMesh*	GetObjectLodMesh() OVERRIDE;
};
