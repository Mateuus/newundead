#pragma once

#include "GameCommon.h"
#include "NetworkHelper.h"

class obj_ServerVaultNPC: public GameObject
{
	DECLARE_CLASS(obj_ServerVaultNPC, GameObject)

public:
	obj_ServerVaultNPC();
	~obj_ServerVaultNPC();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
};