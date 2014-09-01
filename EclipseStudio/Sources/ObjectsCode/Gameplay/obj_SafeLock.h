#pragma once

#include "GameCommon.h"
#include "GameCode/UserProfile.h"
#include "SharedUsableItem.h"

class obj_SafeLock : public SharedUsableItem
{
	DECLARE_CLASS(obj_SafeLock, SharedUsableItem)
public:
	bool		m_GotData;
	std::string	m_plr1;
	std::string	m_plr2;
	
public:
	obj_SafeLock();
	virtual ~obj_SafeLock();
	char Password[512];
	int IDSafe;

	virtual	BOOL		Load(const char *name);

	virtual	BOOL		OnCreate();
	virtual	BOOL		OnDestroy();

	virtual	BOOL		Update();
};
