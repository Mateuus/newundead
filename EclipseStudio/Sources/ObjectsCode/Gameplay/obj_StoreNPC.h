#pragma once

#include "GameCommon.h"
#include "NPCObject.h"

class obj_StoreNPC : public NPCObject
{	
	DECLARE_CLASS(obj_StoreNPC, NPCObject)

public:
	obj_StoreNPC();
	virtual ~obj_StoreNPC();

	virtual	BOOL OnCreate();

	virtual void OnAction();
};