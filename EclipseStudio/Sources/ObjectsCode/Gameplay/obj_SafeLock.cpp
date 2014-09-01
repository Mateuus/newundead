#include "r3dPCH.h"
#include "r3d.h"

#include "obj_SafeLock.h"
#include "GameCommon.h"

#include "ObjectsCode/weapons/WeaponArmory.h"

IMPLEMENT_CLASS(obj_SafeLock, "obj_SafeLock", "Object");
AUTOREGISTER_CLASS(obj_SafeLock);

obj_SafeLock::obj_SafeLock()
{
	m_GotData = false;
	strcpy(Password,"");
	IDSafe=0;
}

obj_SafeLock::~obj_SafeLock()
{
}

BOOL obj_SafeLock::Load(const char *fname)
{
	const char* cpMeshName = "Data\\ObjectsDepot\\Weapons\\Item_Lockbox_01.sco";
	if(!parent::Load(cpMeshName)) 
		return FALSE;

	return TRUE;
}

BOOL obj_SafeLock::OnCreate()
{
	m_ActionUI_Title = gLangMngr.getString("$FR_SAFELOCK_TITLE");
	m_ActionUI_Msg = gLangMngr.getString("HoldEtoAccessSafeLock");

	m_spawnPos = GetPosition();

	if(!DisablePhysX)
	{
		ReadPhysicsConfig();
		PhysicsConfig.group = PHYSCOLL_STATIC_GEOMETRY; // skip collision with players
		PhysicsConfig.requireNoBounceMaterial = true;
		PhysicsConfig.isFastMoving = true;
	}

	return parent::OnCreate();
}

BOOL obj_SafeLock::OnDestroy()
{
	return parent::OnDestroy();
}

BOOL obj_SafeLock::Update()
{
	return parent::Update();
}
