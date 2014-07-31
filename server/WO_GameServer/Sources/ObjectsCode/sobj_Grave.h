#pragma once

#include "GameCommon.h"
#include "NetworkHelper.h"

class obj_Grave : public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_Grave, GameObject)

public:
	struct data_s
	{
	  int		NoteID;
	  __int64	CreateDate;
	  std::string	plr1;
	  std::string	plr2;

	  __int64	SpawnTime;
	  __int64	ExpireMins;
	  
	  r3dPoint3D	in_GamePos;	// used in CJobGetServerNotes::parseNotes to store note position
	};
	data_s		m_Note;

public:
	obj_Grave();
	~obj_Grave();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	
	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);
	void		NetSendGraveData(DWORD peerId);
};
