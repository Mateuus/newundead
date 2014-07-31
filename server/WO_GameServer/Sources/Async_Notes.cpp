#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"

#include "ServerGameLogic.h"
#include "../../EclipseStudio/Sources/backend/WOBackendAPI.h"

#include "Async_Notes.h"

extern	char*	g_ServerApiKey;

void CJobGetServerNotes::parseNotes(pugi::xml_node xmlNote)
{
	AllNotes.clear();

	// enter into items list
	xmlNote = xmlNote.first_child();
	while(!xmlNote.empty())
	{
		obj_Note::data_s note;
		note.NoteID     = xmlNote.attribute("NoteID").as_int();
		note.CreateDate = xmlNote.attribute("CreateDate").as_int64();
		note.ExpireMins = xmlNote.attribute("ExpireMins").as_int64();
		note.TextFrom   = xmlNote.attribute("TextFrom").value();
		note.TextSubj   = xmlNote.attribute("TextSubj").value();

		note.in_GamePos = r3dPoint3D(0, 0, 0);
		sscanf(xmlNote.attribute("GamePos").value(), "%f %f %f", &note.in_GamePos.x, &note.in_GamePos.y, &note.in_GamePos.z);
		
		AllNotes.push_back(note);
		xmlNote = xmlNote.next_sibling();
	}
	
	return;
}

int CJobGetServerNotes::Exec()
{
	CWOBackendReq req("api_SrvNotes.aspx");
	req.AddParam("skey1", g_ServerApiKey);
	
	req.AddParam("func",     "get");
	req.AddParam("GameSID",  GameServerId);
	
	// issue
	if(!req.Issue())
	{
		r3dOutToLog("!!!! CJobGetServerNotes failed, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	
	pugi::xml_document xmlFile;
	req.ParseXML(xmlFile);
	parseNotes(xmlFile.child("notes"));

	return 0;
}

void CJobGetServerNotes::OnSuccess()
{
	r3dOutToLog("Job %s: Creating %d Notes\n", desc, AllNotes.size()); CLOG_INDENT;
	for(size_t i=0; i<AllNotes.size(); ++i)
	{
		const obj_Note::data_s& note = AllNotes[i];
		
		// create network object
		obj_Note* obj = (obj_Note*)srv_CreateGameObject("obj_Note", "obj_Note", note.in_GamePos);
		obj->SetNetworkID(gServerLogic.GetFreeNetId());
		obj->NetworkLocal = true;
		// vars
		obj->m_Note       = note;
	}
}

int CJobAddNote::Exec()
{
	CWOBackendReq req("api_SrvNotes.aspx");
	req.AddSessionInfo(CustomerID, SessionID);
	req.AddParam("CharID", CharID);
	req.AddParam("skey1",  g_ServerApiKey);
	
	char strGamePos[256];
	sprintf(strGamePos, "%.3f %.3f %.3f", GamePos.x, GamePos.y, GamePos.z);
	
	req.AddParam("func",     "add");
	req.AddParam("GameSID",  GameServerId);
	req.AddParam("GamePos",  strGamePos);
	req.AddParam("ExpMins",  ExpMins);
	req.AddParam("TextSubj", TextSubj.c_str());
	req.AddParam("TextFrom", TextFrom.c_str());
	
	// issue
	if(!req.Issue())
	{
		r3dOutToLog("!!!! CJobAddNote failed, code: %d\n", req.resultCode_);
		return req.resultCode_;
	}
	
	// parse returned NoteID
	int nargs = sscanf(req.bodyStr_, "%d", &NoteID);
	if(nargs != 1) 
	{
		r3dOutToLog("!!!! CJobAddNote failed - bad answer %s\n", req.bodyStr_);
		return 1;
	}

	return 0;
}

void CJobAddNote::OnSuccess()
{
	obj_Note* obj = (obj_Note*)GameWorld().GetObject(NoteGameObj);
	if(!obj) {
		r3dOutToLog("!!! note was somehow destroyed\n");
		return;
	}

	r3dOutToLog("Job %s : Note %d created\n", desc, NoteID);
	obj->m_Note.NoteID = NoteID;
	return;
}
