#pragma once

#include "AsyncFuncs.h"
#include "ObjectsCode/sobj_Note.h"

class CJobGetServerNotes : public CAsyncApiJob
{
  public:
	DWORD		GameServerId;

	std::vector<obj_Note::data_s> AllNotes;
	void		parseNotes(pugi::xml_node xmlNote);
	
  public:
	CJobGetServerNotes() : CAsyncApiJob()
	{
		sprintf(desc, "CJobGetServerNotes %p", this);
	}

	int		Exec();
	void		OnSuccess();
};

class CJobAddNote : public CAsyncApiJob
{
  public:
	DWORD		GameServerId;
	r3dPoint3D	GamePos;
	std::string	TextSubj;
	std::string	TextFrom;
	int		ExpMins;
	gobjid_t	NoteGameObj;

	int		NoteID;
  
  public:
	CJobAddNote(const obj_ServerPlayer* plr) : CAsyncApiJob(plr)
	{
		sprintf(desc, "CJobAddNote[%d] %p", CustomerID, this);
		ExpMins      = 60 * 60;
		NoteID       = 0;
	}

	int		Exec();
	void		OnSuccess();
};
