#pragma once

#include "GameCommon.h"
#include "NetworkHelper.h"

class obj_Note : public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_Note, GameObject)

public:
	struct data_s
	{
	  int		NoteID;
	  __int64	CreateDate;
	  std::string	TextFrom;
	  std::string	TextSubj;

	  __int64	SpawnTime;
	  __int64	ExpireMins;
	  
	  r3dPoint3D	in_GamePos;	// used in CJobGetServerNotes::parseNotes to store note position
	};
	data_s		m_Note;

public:
	obj_Note();
	~obj_Note();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	
	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);
	void		NetSendNoteData(DWORD peerId);
};
