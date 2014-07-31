#pragma once

#include "GameCommon.h"
#include "Ammo.h"
#include "APIScaleformGfx.h"

// data driven class. It is created by Ammo class, for bullets that are not immediate action, for example rockets.
class obj_Flare : public AmmoShared
{
	DECLARE_CLASS(obj_Flare, AmmoShared)
	friend Ammo;

	void*	m_sndThrow;
	void*	m_sndBurn;
public:
	obj_Flare();

	virtual	BOOL		OnCreate();
	virtual BOOL		OnDestroy();
	virtual	BOOL		Update();

	virtual r3dMesh*	GetObjectMesh();
	virtual r3dMesh*	GetObjectLodMesh() OVERRIDE;
};
