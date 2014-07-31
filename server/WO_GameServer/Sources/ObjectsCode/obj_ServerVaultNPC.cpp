#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "obj_ServerVaultNPC.h"

IMPLEMENT_CLASS(obj_ServerVaultNPC, "obj_VaultNPC", "Object");
AUTOREGISTER_CLASS(obj_ServerVaultNPC);

obj_ServerVaultNPC::obj_ServerVaultNPC()
{
}

obj_ServerVaultNPC::~obj_ServerVaultNPC()
{
}

BOOL obj_ServerVaultNPC::OnCreate()
{
	r3dOutToLog("obj_ServerVaultNPC %p created.\n", this);	
	return parent::OnCreate();
}

BOOL obj_ServerVaultNPC::OnDestroy()
{
	r3dOutToLog("obj_ServerVaultNPC %p destroyed\n", this);
	return parent::OnDestroy();
}

BOOL obj_ServerVaultNPC::Update()
{
	return parent::Update();
}