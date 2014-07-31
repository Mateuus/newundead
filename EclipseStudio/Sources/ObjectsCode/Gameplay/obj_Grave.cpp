#include "r3dPCH.h"
#include "r3d.h"

#include "obj_Grave.h"
#include "GameCommon.h"

#include "ObjectsCode/weapons/WeaponArmory.h"

IMPLEMENT_CLASS(obj_Grave, "obj_Grave", "Object");
AUTOREGISTER_CLASS(obj_Grave);

obj_Grave::obj_Grave()
{
	DisablePhysX = true;
	m_bEnablePhysics = false;
	m_GotData = false;
}

obj_Grave::~obj_Grave()
{
}

BOOL obj_Grave::Load(const char *fname)
{
	const char* cpMeshName = u_GetRandom() >= 0.5f ? "Data\\ObjectsDepot\\Weapons\\item_gravestone_03.sco" : "Data\\ObjectsDepot\\Weapons\\item_gravestone_02.sco"; ;
	if(!parent::Load(cpMeshName)) 
		return FALSE;

	return TRUE;
}

BOOL obj_Grave::OnCreate()
{
	m_ActionUI_Title = gLangMngr.getString("GraveStone");
	m_ActionUI_Msg = gLangMngr.getString("Hold Y To Read GraveStone");

	m_spawnPos = GetPosition();

	if(!DisablePhysX)
	{
		ReadPhysicsConfig();
		PhysicsConfig.group = PHYSCOLL_TINY_GEOMETRY; // skip collision with players
		PhysicsConfig.requireNoBounceMaterial = true;
		PhysicsConfig.isFastMoving = true;
	}

	return parent::OnCreate();
}

BOOL obj_Grave::OnDestroy()
{
	return parent::OnDestroy();
}

BOOL obj_Grave::Update()
{
	return parent::Update();
}
