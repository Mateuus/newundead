#pragma once

#include "multiplayer/P2PMessages.h"

class CNetCellMover
{
  private:
	GameObject*	owner;
	
	float		updateDelta;
	float		nextUpdate;
	float		cellSize;	// radius of cell
	
	// last sended values of moveData_s
	struct netMoveData_s {
	  r3dPoint3D	cell;		// center position of last sended cell
	  r3dPoint3D	pos;
	  BYTE		turnAngle;
	  BYTE		bendAngle;
	  BYTE		state;
	};
	netMoveData_s	lastMd;
	bool		IsDataChanged(const netMoveData_s& md);
	
  public:
	// input-output move data
	struct moveData_s {
	  r3dPoint3D	pos;
	  float		turnAngle;
	  float		bendAngle;
	  BYTE		state;
	  int plstate;
	};
		
  public:	  
	CNetCellMover(GameObject* in_owner, float in_updateDelta, float in_cellSize)
	{
		owner	    = in_owner;
		updateDelta = in_updateDelta;
		cellSize    = in_cellSize;
		nextUpdate  = 0;
		lastRecv    = 0;

		lastMd.pos        = r3dPoint3D(-99999, -99999, -99999);
		lastMd.cell       = r3dPoint3D(-99999, -99999, -99999);
		lastMd.turnAngle  = 0;
		lastMd.bendAngle  = 0;
		lastMd.state      = 0xFF;
	};

	// local functions, build movement update packet
	DWORD		SendPosUpdate(const moveData_s& in_data, PKT_C2C_MoveSetCell_s* n1, PKT_C2C_MoveRel_s* n2);

	// remote functions
	float		lastRecv;	// last time network update was received
	void		SetCell(const PKT_C2C_MoveSetCell_s& n);
	const moveData_s& DecodeMove(const PKT_C2C_MoveRel_s& n);
  	moveData_s	lastMove;	// last data from network update
	const moveData_s& NetData() {
		r3d_assert(!owner->NetworkLocal);
		return lastMove;
	}
	const r3dPoint3D& GetNetPos() {
		return NetData().pos;
	}
	void		SetStartCell(const r3dPoint3D& pos) {
		r3d_assert(!owner->NetworkLocal);
		lastMd.cell = pos;
	}
	
	r3dPoint3D	GetVelocityToNetTarget(const r3dPoint3D& pos, float chase_speed, float teleport_delta_sec);

	// reset mover state to specific position	
	void		Teleport(const r3dPoint3D& pos) {
		lastMd.pos   = r3dPoint3D(-99999, -99999, -99999);
		lastMd.cell  = r3dPoint3D(-99999, -99999, -99999);
		lastMove.pos = pos;
	}

	
#ifdef WO_SERVER	
	// server helper functions - only it can directly modify cell
	const r3dPoint3D&	SrvGetCell() const { 
		return lastMd.cell;
	}
	void			SrvSetCell(const r3dPoint3D& pos) { 
		lastMd.cell = pos;
	}
#endif
};
	