#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "obj_VaultNPC.h"
#include "UI\HUDVault.h"

IMPLEMENT_CLASS(obj_VaultNPC, "obj_VaultNPC", "Object");
AUTOREGISTER_CLASS(obj_VaultNPC);

extern HUDVault* hudVault;

obj_VaultNPC::obj_VaultNPC()
{
}

obj_VaultNPC::~obj_VaultNPC()
{
}

BOOL obj_VaultNPC::OnCreate()
{
	m_ActionUI_Title = gLangMngr.getString("ActionUI_VaultTitle");
	m_ActionUI_Msg = gLangMngr.getString("HoldEToAccessVault");
	return parent::OnCreate();
}

void obj_VaultNPC::OnAction()
{
	hudVault->Activate();
}