#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "obj_ServerBuilding.h"

IMPLEMENT_CLASS(obj_ServerBuilding, "obj_Building", "Object");
AUTOREGISTER_CLASS(obj_ServerBuilding);

obj_ServerBuilding::obj_ServerBuilding()
{
	ObjFlags |= OBJFLAG_AddToDummyWorld;
}

obj_ServerBuilding::~obj_ServerBuilding()
{
}

DefaultPacket* obj_ServerBuilding::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateBuilding_s n;
	n.spawnID = 0;
	n.pos     = GetPosition();
	r3dscpy(n.fname,FileName.c_str());
	n.angle = GetRotationVector();
	n.col = false;
	if(ObjFlags & OBJFLAG_PlayerCollisionOnly)
	n.col = true;

	*out_size = sizeof(n);
	return &n;
}

BOOL obj_ServerBuilding::Load(const char* name)
{
	r3dOutToLog("Building %p\n",this,FileName.c_str());
	if(parent::Load(name))
	{
		// set temp bbox
		r3dBoundBox bboxLocal ;
		bboxLocal.Org	= r3dPoint3D( -0.5, -0.5, -0.5 );
		bboxLocal.Size	= r3dPoint3D( +0.5, +0.5, +0.5 );
		SetBBoxLocal( bboxLocal );

		return TRUE;
	}
	return FALSE;
}
BOOL obj_ServerBuilding::Update()
{
	return parent::Update();
}
BOOL obj_ServerBuilding::OnDestroy()
{	
	return parent::OnDestroy();
}

BOOL obj_ServerBuilding::OnCreate()
{
	//ObjFlags |= OBJFLAG_SkipCastRay;
	distToCreateSq = 512 * 512;
	distToDeleteSq = 600 * 600;
	//SetNetworkID(gServerLogic.GetFreeNetId());
	parent::OnCreate();
	NetworkLocal = true;

	//gServerLogic.NetRegisterObjectToPeers(this);

	return 1;
}
void obj_ServerBuilding::ReadSerializedData(pugi::xml_node& node)
{
	parent::ReadSerializedData(node);
}

