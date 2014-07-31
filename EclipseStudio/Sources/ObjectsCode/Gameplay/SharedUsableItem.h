#pragma once

#include "GameCommon.h"
#include "GameCode/UserProfile.h"

// object that is lying on the ground and player can interact with it. 
// this is base class for such objects
class SharedUsableItem : public MeshGameObject
{
public:
	bool		DisablePhysX; // for permanent notes

	float			m_lastPickupAttempt; // time until you cannot try to pick up item again
	float		m_StayStillTime;
	float		m_NotSleepTime;

	const wchar_t*	 m_ActionUI_Title;
	const wchar_t*	 m_ActionUI_Msg;

public:
	SharedUsableItem();
	virtual ~SharedUsableItem();

	virtual	BOOL		Update();

protected:
	r3dPoint3D			m_spawnPos;
};

extern std::list<SharedUsableItem*> m_SharedUsableItemList;
