#pragma once

#include "GameCommon.h"
#include "NetworkHelper.h"

class obj_ServerBarricade: public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_ServerBarricade, GameObject)
public:
	uint32_t	m_ItemID;
	int		m_ObstacleId;
	float		m_Radius;
	bool		requestKill;
	float		Health;
	
	static std::vector<obj_ServerBarricade*> allBarricades;

public:
	obj_ServerBarricade();
	~obj_ServerBarricade();

	virtual	BOOL		OnCreate();
	virtual	BOOL		OnDestroy();

	virtual	BOOL		Update();

	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);

	void				DoDamage(float dmg);
};
