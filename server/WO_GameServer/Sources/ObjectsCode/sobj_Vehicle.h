#pragma once

#include "GameCommon.h"
#include "NetworkHelper.h"
#include "multiplayer/NetCellMover.h"
#include "Backend/ServerUserProfile.h"
#include "multiplayer/P2PMessages.h"
#include "obj_ServerPlayer.h"
class obj_Vehicle : public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_Vehicle, GameObject)

public:
	obj_Vehicle();
	~obj_Vehicle();

	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	virtual	BOOL		OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	INetworkHelper::NetGetCreatePacket(int* out_size);
	bool		moveInited;

	obj_ServerPlayer* owner;
	PxVehicleDrive4WRawInputData* carControlData;
	CNetCellMover	netMover;
	float		moveAccumDist;
	float		moveAccumTime;
	DWORD peerid;
	bool status;
	bool bOn;
	float fuel;
	float lastDriveTime;
	void OnNetPacket(const PKT_C2S_CarSound_s& n);
	//void	OnNetPacket(const PKT_C2C_CarStatus_s& n);
	void OnNetPacket(const PKT_C2S_CarMove_s& n);
	void OnNetPacket(const PKT_C2C_MoveRel_s& n);
	void OnNetPacket(const PKT_C2S_CarControl_s& n);
	void	OnNetPacket(const PKT_C2C_CarSpeed_s& n);
	void	OnNetPacket(const PKT_C2C_MoveSetCell_s& n);
	void OnNetPacket(const PKT_C2C_CarFlashLight_s& n);
	char				vehicle_Model[64];
	void RelayPacket(const DefaultPacket* packetData, int packetSize, bool guaranteedAndOrdered);
};
