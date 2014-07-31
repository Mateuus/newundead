#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "multiplayer/NetCellMover.h"
#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "sobj_Vehicle.h"

IMPLEMENT_CLASS(obj_Vehicle, "obj_Vehicle", "Object");
AUTOREGISTER_CLASS(obj_Vehicle);

obj_Vehicle::obj_Vehicle() :
netMover(this, 0.2f, (float)PKT_C2C_MoveSetCell_s::VEHICLE_CELL_RADIUS)
{
}

obj_Vehicle::~obj_Vehicle()
{
}

BOOL obj_Vehicle::OnCreate()
{

	r3d_assert(NetworkLocal);
	r3d_assert(GetNetworkID());
	//NoteID can be 0 for newly created notes

	//_time64(&m_Note.SpawnTime);

	// raycast down to earth in case world was changed
	//r3dPoint3D pos = gServerLogic.AdjustPositionToFloor(GetPosition());
	SetPosition(GetPosition());
	status = false;
	fuel = u_GetRandom(25.0f,100.0f);
	gServerLogic.NetRegisterObjectToPeers(this);
	ObjTypeFlags |= 65536;
	owner = NULL;
	r3dOutToLog("obj_Vehicle %p created. fuel:%.2f\n", this,fuel);
	lastDriveTime = r3dGetTime();
	distToCreateSq = 512 * 512;
	distToDeleteSq = 513 * 513;
	return parent::OnCreate();
}

BOOL obj_Vehicle::OnDestroy()
{
	r3dOutToLog("obj_Vehicle %p destroyed\n", this);

	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));

	return parent::OnDestroy();
}

BOOL obj_Vehicle::Update()
{
	if (owner)
		status = true;
	else
		status = false;

	if (!status)
	{
		if ((r3dGetTime() - lastDriveTime) > 1800)
		{
			r3dOutToLog("obj_Vehicle %p expired respawning...\n",this);
			setActiveFlag(0);
			obj_Vehicle* obj = 	(obj_Vehicle*)srv_CreateGameObject("obj_Vehicle", "obj_Vehicle", r3dPoint3D(u_GetRandom(0,GameWorld().m_MinimapSize.x),0,u_GetRandom(0,GameWorld().m_MinimapSize.z)));
			obj->SetNetworkID(gServerLogic.GetFreeNetId());
			obj->NetworkLocal = true;
			obj->OnCreate();
			r3dOutToLog("Before On Terrain pos=%.2f,%.2f,%.2f\n",obj->GetPosition().x,obj->GetPosition().y,obj->GetPosition().z);
			float terrHeight = Terrain->GetHeight(obj->GetPosition()); // always spawn on terrain !!!!!!!
			obj->SetPosition(r3dPoint3D(obj->GetPosition().x,terrHeight,obj->GetPosition().z));
			r3dOutToLog("On Terrain pos=%.2f,%.2f,%.2f\n",obj->GetPosition().x,obj->GetPosition().y,obj->GetPosition().z);
		}
	}
	if (!owner)
	{
		PKT_C2C_CarSpeed_s n1;
		n1.speed = 0;
		n1.rpm = 0;
		n1.fuel = fuel;
		RelayPacket(&n1,sizeof(n1),true);
	}
	if (status)
     fuel -= 0.0005f;

	PKT_C2C_CarStatusSv_s n1;
	n1.status = status;
	RelayPacket(&n1,sizeof(n1),true);

	if (carControlData)
	{
	PKT_C2S_CarControl_s n3;
	n3.carControlData = *carControlData;
	RelayPacket(&n3,sizeof(n3),true);
	}
	/*PKT_C2S_CarMove_s n2;
	n2.pos = GetPosition();
	n2.angle = GetRotationVector();
	RelayPacket(&n1,sizeof(n1),true);*/
	return parent::Update();
}

DefaultPacket* obj_Vehicle::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateVehicles_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	n.pos = GetPosition();
	sprintf(n.vehicle_Model,"Data\\ObjectsDepot\\Vehicles\\drivable_buggy_02.sco");
	n.bOn = bOn;
	*out_size = sizeof(n);

	/*PKT_C2C_CarFlashLight_s n1;
	n1.bOn = bOn;
	gServerLogic.p2pBroadcastToActive(this, &n1, sizeof(n1));*/

	return &n;
}
#undef DEFINE_GAMEOBJ_PACKET_HANDLER
#define DEFINE_GAMEOBJ_PACKET_HANDLER(xxx) \
	case xxx: { \
	const xxx##_s&n = *(xxx##_s*)packetData; \
	if(packetSize != sizeof(n)) { \
	r3dOutToLog("!!!!errror!!!! %s packetSize %d != %d\n", #xxx, packetSize, sizeof(n)); \
	return TRUE; \
	} \
	OnNetPacket(n); \
	return TRUE; \
}

BOOL obj_Vehicle::OnNetReceive(DWORD EventID, const void* packetData, int packetSize)
{
	//@TODO somehow check that originator of that packet have playerIdx that match peer

	// packets that ignore packet sequence

	/*if(myPacketSequence != clientPacketSequence)
	{
	// we get late packet after late packet barrier, skip it
	r3dOutToLog("peer%02d, CustomerID:%d LatePacket %d %s\n", peerId_, profile_.CustomerID, EventID, packetBarrierReason);
	return TRUE;
	}*/

	// packets while dead
	switch(EventID)
	{
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveSetCell);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveRel);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_CarMove);
		//DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_CarStatus);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_CarSound);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_CarControl);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_CarSpeed);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_CarFlashLight);
		//DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2S_DisconnectReq);
	}


	return FALSE;
}
#undef DEFINE_GAMEOBJ_PACKET_HANDLER
/*void obj_Vehicle::OnNetPacket(const PKT_C2C_CarStatus_s& n)
{
status = n.status;
fuel = n.fuel;
}*/
void obj_Vehicle::OnNetPacket(const PKT_C2S_CarControl_s& n)
{
	*carControlData = n.carControlData;
}
void obj_Vehicle::OnNetPacket(const PKT_C2C_CarSpeed_s& n)
{
	//fuel = n.fuel;
	PKT_C2C_CarSpeed_s n1;
	n1.speed = n.speed;
	n1.rpm = n.rpm;
	n1.fuel = fuel;
	RelayPacket(&n1,sizeof(n1),true);
}
void obj_Vehicle::OnNetPacket(const PKT_C2C_CarFlashLight_s& n)
{
	bOn = n.bOn;

	PKT_C2C_CarFlashLight_s n1;
	n1.bOn = bOn;
	RelayPacket(&n1,sizeof(n1),true);
}
void obj_Vehicle::OnNetPacket(const PKT_C2S_CarSound_s& n)
{
	PKT_C2S_CarSound_s n1;
	n1.Id = n.Id;
	RelayPacket(&n1,sizeof(n1),true);
}
void obj_Vehicle::OnNetPacket(const PKT_C2S_CarMove_s& n)
{
	//r3dOutToLog("CarMovePKT\n");
	SetPosition(n.pos);
	SetRotationVector(n.angle);
	PKT_C2S_CarMove_s n1;
	n1.pos = GetPosition();
	n1.angle = n.angle;
	RelayPacket(&n1,sizeof(n1),true);
}
void obj_Vehicle::OnNetPacket(const PKT_C2C_MoveRel_s& n)
{
	//r3dOutToLog("PKT_C2C_MoveRel Rev\n");
	// decode move
	const CNetCellMover::moveData_s& md = netMover.DecodeMove(n);

	if(moveInited)
	{
		// for now we will check ONLY ZX, because somehow players is able to fall down
		r3dPoint3D p1 = GetPosition();
		r3dPoint3D p2 = md.pos;
		p1.y = 0;
		p2.y = 0;
		float dist = (p1 - p2).Length();
		moveAccumDist += dist;
	}

	// check if we need to reset accomulated speed
	if(!moveInited) 
	{
		moveInited    = true;
		moveAccumTime = 0.0f;
		moveAccumDist = 0.0f;
	}

	// update last action if we moved or rotated
	/*if((GetPosition() - md.pos).Length() > 0.01f || m_PlayerRotation != md.turnAngle)
	{
	lastPlayerAction_ = r3dGetTime();
	}
	*/
	SetPosition(md.pos);

	RelayPacket(&n, sizeof(n), false);
}
void obj_Vehicle::OnNetPacket(const PKT_C2C_MoveSetCell_s& n)
{
	// if by some fucking unknown method you appeared at 0,0,0 - don't do that!

	if(moveInited)
	{
		// for now we will check ONLY ZX, because somehow players is able to fall down
		r3dPoint3D p1 = netMover.SrvGetCell();
		r3dPoint3D p2 = n.pos;
		p1.y = 0;
		p2.y = 0;
		float dist = (p1 - p2).Length();

		// check for teleport - more that 3 sec of sprint speed. MAKE sure that max dist is more that current netMover.cellSize
		/*if(loadout_->Alive && dist > GPP_Data.AI_SPRINT_SPEED * 3.0f)
		{
		gServerLogic.LogCheat(peerId_, PKT_S2C_CheatWarning_s::CHEAT_FastMove, true, (dist > 500.0f ? "huge_teleport" : "teleport"),
		"%f, srvGetCell: %.2f, %.2f, %.2f; n.pos: %.2f, %.2f, %.2f", 
		dist, 
		netMover.SrvGetCell().x, netMover.SrvGetCell().y, netMover.SrvGetCell().z, 
		n.pos.x, n.pos.y, n.pos.z
		);
		return;
		}*/
	}

	netMover.SetCell(n);
	SetPosition(n.pos);

	// keep them guaranteed
	RelayPacket(&n, sizeof(n), true);
}
void obj_Vehicle::RelayPacket(const DefaultPacket* packetData, int packetSize, bool guaranteedAndOrdered)
{
	for (int i=0;i<gServerLogic.MAX_PEERS_COUNT;i++)
	{
		obj_ServerPlayer* plr = gServerLogic.peers_[i].player;
		if(!plr)
			continue;
		if((plr->GetPosition() - GetPosition()).LengthSq() < 512)
		{
			gServerLogic.net_->SendToPeer(packetData,packetSize,plr->peerId_);
			gServerLogic.netSentPktSize[packetData->EventID] += packetSize;
		}
	}
	//gServerLogic.RelayPacket(peerid, this, packetData, packetSize, guaranteedAndOrdered);
}