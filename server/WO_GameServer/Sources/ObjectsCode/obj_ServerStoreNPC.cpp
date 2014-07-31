#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "obj_ServerStoreNPC.h"

IMPLEMENT_CLASS(obj_ServerStoreNPC, "obj_StoreNPC", "Object");
AUTOREGISTER_CLASS(obj_ServerStoreNPC);

obj_ServerStoreNPC::obj_ServerStoreNPC()
{
}

obj_ServerStoreNPC::~obj_ServerStoreNPC()
{
}

BOOL obj_ServerStoreNPC::OnCreate()
{
	r3dOutToLog("obj_ServerStoreNPC %p created.\n", this);	
	return parent::OnCreate();
}

BOOL obj_ServerStoreNPC::OnDestroy()
{
	r3dOutToLog("obj_ServerStoreNPC %p destroyed\n", this);
	return parent::OnDestroy();
}

BOOL obj_ServerStoreNPC::Update()
{
	return parent::Update();
}
