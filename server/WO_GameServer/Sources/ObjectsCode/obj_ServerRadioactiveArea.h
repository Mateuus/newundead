#pragma once

#include "GameCommon.h"

class obj_ServerRadioactiveArea : public GameObject
{
	DECLARE_CLASS(obj_ServerRadioactiveArea, GameObject)

public:
	float		useRadius;

public:
	obj_ServerRadioactiveArea();
	~obj_ServerRadioactiveArea();

	virtual BOOL	OnCreate();
	virtual	void	ReadSerializedData(pugi::xml_node& node);
};

class RadiactiveAreaMgr
{
public:
	enum { MAX_RADIOACTIVE_AREA = 256 }; // 256 should be more than enough, if not, will redo into vector
	obj_ServerRadioactiveArea* RadioactiveArea_[MAX_RADIOACTIVE_AREA];
	int		numRadioactiveArea_;

	void RegisterRadioactiveArea(obj_ServerRadioactiveArea* rbox) 
	{
	r3d_assert(numRadioactiveArea_ < MAX_RADIOACTIVE_AREA);
		RadioactiveArea_[numRadioactiveArea_++] = rbox;
	}

public:
	RadiactiveAreaMgr() { numRadioactiveArea_ = 0; }
	~RadiactiveAreaMgr() {}
};

extern	RadiactiveAreaMgr gRadioactiveAreaMngr;
