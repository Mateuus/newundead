#pragma once

#include "GameCommon.h"
#include "NetworkHelper.h"

class obj_ServerStoreNPC: public GameObject
{
	DECLARE_CLASS(obj_ServerStoreNPC, GameObject)

public:
	obj_ServerStoreNPC();
	~obj_ServerStoreNPC();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
};