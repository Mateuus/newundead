#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "sobj_Note.h"

IMPLEMENT_CLASS(obj_Note, "obj_Note", "Object");
AUTOREGISTER_CLASS(obj_Note);

obj_Note::obj_Note()
{
}

obj_Note::~obj_Note()
{
}

BOOL obj_Note::OnCreate()
{
	r3dOutToLog("obj_Note %p created. [%d]%s, %I64d mins left\n", this, m_Note.NoteID, m_Note.TextSubj.c_str(), m_Note.ExpireMins);
	
	r3d_assert(NetworkLocal);
	r3d_assert(GetNetworkID());
	//NoteID can be 0 for newly created notes
	
	_time64(&m_Note.SpawnTime);
	
	// raycast down to earth in case world was changed
	r3dPoint3D pos = gServerLogic.AdjustPositionToFloor(GetPosition());
	SetPosition(pos);

	gServerLogic.NetRegisterObjectToPeers(this);
	
	return parent::OnCreate();
}

BOOL obj_Note::OnDestroy()
{
	r3dOutToLog("obj_Note %p destroyed\n", this);

	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));
	
	return parent::OnDestroy();
}

BOOL obj_Note::Update()
{
	__int64 utcTime;
	if(_time64(&utcTime) > m_Note.SpawnTime + (m_Note.ExpireMins * 60))
	{
		r3dOutToLog("obj_Note %p expired\n", this);
		setActiveFlag(0);
	}

	return parent::Update();
}

DefaultPacket* obj_Note::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateNote_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	n.pos     = GetPosition();
	
	*out_size = sizeof(n);
	return &n;
}

void obj_Note::NetSendNoteData(DWORD peerId)
{
	PKT_S2C_SetNoteData_s n;
	r3dscpy(n.TextFrom, m_Note.TextFrom.c_str());
	r3dscpy(n.TextSubj, m_Note.TextSubj.c_str());
	
	gServerLogic.p2pSendToPeer(peerId, this, &n, sizeof(n));
}