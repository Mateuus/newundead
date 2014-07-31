#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "obj_StoreNPC.h"
#include "UI\HUDGeneralStore.h"

IMPLEMENT_CLASS(obj_StoreNPC, "obj_StoreNPC", "Object");
AUTOREGISTER_CLASS(obj_StoreNPC);

extern HUDGeneralStore*	hudGeneralStore;

obj_StoreNPC::obj_StoreNPC()
{
}

obj_StoreNPC::~obj_StoreNPC()
{
}

BOOL obj_StoreNPC::OnCreate()
{
	m_ActionUI_Title = gLangMngr.getString("ActionUI_StoreTitle");
	m_ActionUI_Msg = gLangMngr.getString("HoldEToAccessStore");
	return parent::OnCreate();
}

void obj_StoreNPC::OnAction()
{
	hudGeneralStore->Activate();
}