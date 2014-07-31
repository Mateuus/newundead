#pragma once

#include "GameCommon.h"
#include "GameCode/UserProfile.h"
#include "SharedUsableItem.h"

class obj_Note : public SharedUsableItem
{
	DECLARE_CLASS(obj_Note, SharedUsableItem)
public:
	bool		m_GotData;
	std::string	m_TextFrom;
	std::string	m_Text;
	
public:
	obj_Note();
	virtual ~obj_Note();

	virtual	BOOL		Load(const char *name);

	virtual	BOOL		OnCreate();
	virtual	BOOL		OnDestroy();

	virtual	BOOL		Update();
};
