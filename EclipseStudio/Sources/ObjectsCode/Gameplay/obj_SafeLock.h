#pragma once

#include "GameCommon.h"
#include "GameCode/UserProfile.h"
#include "SharedUsableItem.h"

class obj_SafeLock : public SharedUsableItem
{
	DECLARE_CLASS(obj_SafeLock, SharedUsableItem)
public:
//	bool		m_GotData;
	int pass;
	//bool isLock;
public:
	obj_SafeLock();
	virtual ~obj_SafeLock();

	virtual	BOOL		Load(const char *name);

	virtual	BOOL		OnCreate();
	virtual	BOOL		OnDestroy();

	virtual	BOOL		Update();
};
