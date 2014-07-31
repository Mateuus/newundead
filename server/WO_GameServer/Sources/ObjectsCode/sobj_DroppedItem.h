#pragma once

#include "GameCommon.h"
#include "NetworkHelper.h"

class obj_DroppedItem : public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_DroppedItem, GameObject)

public:
	wiInventoryItem	m_Item;
	float		m_expireTime;

public:
	obj_DroppedItem();
	~obj_DroppedItem();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	
	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);
};
