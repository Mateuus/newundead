#pragma once

#include "GameCommon.h"
#include "GameCode/UserProfile.h"
#include "SharedUsableItem.h"

class obj_Grave : public SharedUsableItem
{
	DECLARE_CLASS(obj_Grave, SharedUsableItem)
public:
	bool		m_GotData;
	std::string	m_plr1;
	std::string	m_plr2;
	
public:
	obj_Grave();
	virtual ~obj_Grave();

	virtual	BOOL		Load(const char *name);

	virtual	BOOL		OnCreate();
	virtual	BOOL		OnDestroy();

	virtual	BOOL		Update();
};
