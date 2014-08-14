#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "XMLHelpers.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "obj_ServerRadioactiveArea.h"

IMPLEMENT_CLASS(obj_ServerRadioactiveArea, "obj_RadioactiveArea", "Object");
AUTOREGISTER_CLASS(obj_ServerRadioactiveArea);

RadiactiveAreaMgr gRadioactiveAreaMngr;

obj_ServerRadioactiveArea::obj_ServerRadioactiveArea()
{
	useRadius = 2.0f;
}

obj_ServerRadioactiveArea::~obj_ServerRadioactiveArea()
{
}

BOOL obj_ServerRadioactiveArea::OnCreate()
{
	parent::OnCreate();

	gRadioactiveAreaMngr.RegisterRadioactiveArea(this);
	return 1;
}

// copy from client version
void obj_ServerRadioactiveArea::ReadSerializedData(pugi::xml_node& node)
{
	parent::ReadSerializedData(node);
	pugi::xml_node objNode = node.child("RadioActive_Area");
	GetXMLVal("useRadius", objNode, &useRadius);
}
