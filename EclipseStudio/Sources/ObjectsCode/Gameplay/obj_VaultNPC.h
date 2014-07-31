#pragma once

#include "GameCommon.h"
#include "NPCObject.h"
//#include "../../ui/FrontendShared.h"

class obj_VaultNPC : public NPCObject
{
	DECLARE_CLASS(obj_VaultNPC, NPCObject)

public:
	obj_VaultNPC();
	virtual ~obj_VaultNPC();

	virtual	BOOL OnCreate();

	virtual void OnAction();
};