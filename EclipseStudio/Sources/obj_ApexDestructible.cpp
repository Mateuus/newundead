/*
#include "r3dPCH.h"
#include "r3d.h"


#include "GameCommon.h"


#include "multiplayer/P2PMessages.h"
#include "multiplayer/ServerGameLogic.h"


#include "obj_ApexDestructible.h"


IMPLEMENT_CLASS(obj_ApexDestructible, "obj_ApexDestructible", "Object");
AUTOREGISTER_CLASS(obj_ApexDestructible);


obj_ApexDestructible::obj_ApexDestructible()
{
#if APEX_ENABLED
ObjTypeFlags |= OBJTYPE_ApexDestructible;
#endif
}


obj_ApexDestructible::~obj_ApexDestructible()
{
#if APEX_ENABLED
ObjTypeFlags |= OBJTYPE_ApexDestructible;
#endif
}


BOOL obj_ApexDestructible::Load()
{
return FALSE;
}


BOOL obj_ApexDestructible::OnDestroy()
{
r3dOutToLog("obj_ApexDestructible %p destroyed\n", this);
return parent::OnDestroy();
}


BOOL obj_ApexDestructible::Update()
{
return parent::Update();

}
*/