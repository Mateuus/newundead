#if VEHICLES_ENABLED
#include "r3dPCH.h"

#include "r3d.h"
//#include "PhysXWorld.h"
//#include "ObjManag.h"
#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"

#include "obj_ServerVehicle.h"
#include "obj_ServerVehicleSpawn.h"

#if VEHICLES_ENABLED
IMPLEMENT_CLASS(obj_Vehicle, "obj_Vehicle", "Object");
AUTOREGISTER_CLASS(obj_Vehicle);

obj_Vehicle::obj_Vehicle() :
 netMover(this, 0.2f, (float)PKT_C2C_MoveSetCell_s::VEHICLE_CELL_RADIUS)
{
#if VEHICLES_ENABLED
 ObjTypeFlags = OBJTYPE_Vehicle;
#endif
 peerId_    = -1;
 spawner    = NULL;
 OccupantsVehicle=0;
 Gasolinecar = u_GetRandom(54.0f,100.0f);
 DamageCar = u_GetRandom(2.3f,5.0f);
 FirstGasolinecar = Gasolinecar;
 FirstDamageCar = DamageCar;
 curTime=0;
 return;
}

obj_Vehicle::~obj_Vehicle()
{

}

BOOL obj_Vehicle::OnCreate()
{
	for (int i=0;i<=8;i++)
	{
		PlayersOnVehicle[i]=0;
	}
	return parent::OnCreate();
}

BOOL obj_Vehicle::Load(const char *fname)
{
	if(!parent::Load(fname))
		return FALSE;

	return TRUE;
}

BOOL obj_Vehicle::Update()
{
	if (this->DamageCar<1)
	{
		this->DamageCar-=0.003f;
	}
	if (this->DamageCar<=0)
	{
		SetPosition(r3dPoint3D(this->GetPosition().x,1.5f,this->GetPosition().z));
		SetRotationVector(r3dPoint3D(GetRotationVector().x,168,GetRotationVector().z));
		if (curTime!=0)  // ResPawnCar
		{
			if ((r3dGetTime() - curTime) > 60) // Respawn in Secconds 60 = 1 Minute
			{
				curTime=0;
				this->Gasolinecar = FirstGasolinecar;
				this->DamageCar = FirstDamageCar;
				this->OccupantsVehicle=0;
				this->SetPosition(FirstPosition);
				this->SetRotationVector(FirstRotationVector);
				if (spawner)
				{
				this->spawner->SetPosition(FirstPosition);
				this->spawner->SetRotationVector(FirstRotationVector);
				}
				PKT_S2C_PositionVehicle_s n; // Server Vehicles
				n.spawnPos=FirstPosition; 
				n.RotationPos = FirstRotationVector;
				n.OccupantsVehicle=0;
				n.GasolineCar=58.0f;
				n.DamageCar=4.0f;
				n.RPMPlayer=0.0f;
				n.RotationSpeed=0.0f;
				n.RespawnCar=true;
				n.bOn = false;
				n.spawnID = this->GetNetworkID();
				r3dOutToLog("############################\nRespanCar ID: %i\nPosition X: %f\nPosition Y: %f\nPosition Z: %f\nRotation X: %f\nRotation Y: %f\nRotation Z: %f\n###########################\n",this->GetNetworkID(),FirstPosition.x,FirstPosition.y,FirstPosition.z,FirstRotationVector.x,FirstRotationVector.y,FirstRotationVector.z);
				gServerLogic.p2pBroadcastToAll(this, &n, sizeof(n), true);
			}
		}
	}
	else if (this->DamageCar>0) 
	{
		if (curTime!=0)
		{
			if ((r3dGetTime() - curTime) > 1200) // Respawn in Secconds 1200 = 20 mins
			{
				curTime=0;
				this->OccupantsVehicle=0;
				this->SetPosition(FirstPosition);
				this->SetRotationVector(FirstRotationVector);
				if (spawner)
				{
				this->spawner->SetPosition(FirstPosition);
				this->spawner->SetRotationVector(FirstRotationVector);
				}
				PKT_S2C_PositionVehicle_s n; // Server Vehicles
				n.spawnPos=FirstPosition; 
				n.RotationPos = FirstRotationVector;
				n.OccupantsVehicle=0;
				n.GasolineCar=this->Gasolinecar;
				n.DamageCar=this->DamageCar;
				n.RPMPlayer=0.0f;
				n.RotationSpeed=0.0f;
				n.RespawnCar=true;
				n.bOn = false;
				n.spawnID = this->GetNetworkID();
				r3dOutToLog("############################\nRespanCar ID: %i\nPosition X: %f\nPosition Y: %f\nPosition Z: %f\nRotation X: %f\nRotation Y: %f\nRotation Z: %f\n###########################\n",this->GetNetworkID(),FirstPosition.x,FirstPosition.y,FirstPosition.z,FirstRotationVector.x,FirstRotationVector.y,FirstRotationVector.z);
				gServerLogic.p2pBroadcastToAll(this, &n, sizeof(n), true);
			}
		}
	}
	return TRUE;
}

DefaultPacket* obj_Vehicle::NetGetCreatePacket(int* out_size) // Server Vehicles
{
		static PKT_S2C_CreateVehicle_s n;
		n.spawnID      = toP2pNetId(GetNetworkID());
		n.spawnPos     = this->GetPosition();
		n.spawnDir     = this->GetRotationVector();
		n.moveCell     = netMover.SrvGetCell();
		n.Ocuppants    = 0;
		n.Gasolinecar  = Gasolinecar;
		n.DamageCar    = DamageCar;
		n.FirstRotationVector=FirstRotationVector;
		n.FirstPosition=FirstPosition;
		sprintf(n.vehicle, spawner->vehicle_Model);

		*out_size = sizeof(n);
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
	switch(EventID)
	{
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveSetCell);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_C2C_MoveRel);
		DEFINE_GAMEOBJ_PACKET_HANDLER(PKT_S2C_PositionVehicle);
	}
	return TRUE;
}
#undef DEFINE_GAMEOBJ_PACKET_HANDLER

void obj_Vehicle::OnNetPacket(const PKT_C2C_MoveSetCell_s& n)
{
}

void obj_Vehicle::OnNetPacket(const PKT_C2C_MoveRel_s& n)
{

}

void obj_Vehicle::OnNetPacket(const PKT_S2C_PositionVehicle_s& n)  // Server Vehicles
{
	    	GameObject* from = GameWorld().GetNetworkObject(n.spawnID);
			if(from)
			{
             // Placing and rotating moving vehicle.
			     if (n.spawnID == this->GetNetworkID())
			     {
					Gasolinecar=n.GasolineCar;
					DamageCar=n.DamageCar;
					from->SetPosition(n.spawnPos);
		            from->SetRotationVector(n.RotationPos);
			        this->SetPosition(n.spawnPos);
			        this->SetRotationVector(n.RotationPos);
					curTime=r3dGetTime();

					PKT_S2C_PositionVehicle_s n2; // Server Vehicles
					n2.spawnPos=n.spawnPos; 
					n2.RotationPos = n.RotationPos;
					n2.OccupantsVehicle=OccupantsVehicle;
					n2.GasolineCar=n.GasolineCar;
					n2.DamageCar=n.DamageCar;
					n2.RPMPlayer=n.RPMPlayer;
					n2.RotationSpeed=n.RotationSpeed;
					n2.RespawnCar=n.RespawnCar;
					n2.bOn = n.bOn;
					n2.spawnID=n.spawnID;
					n2.controlData=n.controlData;
					n2.timeStep=n.timeStep;
					gServerLogic.p2pBroadcastToAll(this, &n2, sizeof(n2), true);
			     }
	       }
}

void obj_Vehicle::RelayPacket(const DefaultPacket* packetData, int packetSize, bool guaranteedAndOrdered)
{
	for(int i=0; i<gServerLogic.MAX_PEERS_COUNT; i++)
	{
		if(gServerLogic.peers_[i].status_ >= gServerLogic.PEER_VALIDATED1)
		{
			gServerLogic.RelayPacket(gServerLogic.peers_[i].CharID, this, packetData, packetSize, guaranteedAndOrdered);
		}
	}
}
#endif