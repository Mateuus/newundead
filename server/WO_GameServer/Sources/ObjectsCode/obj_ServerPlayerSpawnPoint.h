#pragma once

#include "GameCommon.h"
#include "../EclipseStudio/Sources/ObjectsCode/Gameplay/BasePlayerSpawnPoint.h"

class obj_ServerPlayerSpawnPoint : public BasePlayerSpawnPoint
{
	DECLARE_CLASS(obj_ServerPlayerSpawnPoint, BasePlayerSpawnPoint)

public:
	obj_ServerPlayerSpawnPoint();
	~obj_ServerPlayerSpawnPoint();

	virtual BOOL		Load(const char* name);
	
	virtual BOOL		OnCreate();
};


class ControlPointsMgr
{
  public:
	// control points stuff
	enum { MAX_CONTROL_POINTS = 50 };
	BasePlayerSpawnPoint* controlPoints_[MAX_CONTROL_POINTS];
	int		numControlPoints_;
	BasePlayerSpawnPoint* neutralControlPoints[MAX_CONTROL_POINTS];
	int		numNeutralControlPoints;
	BasePlayerSpawnPoint* baseControlPoints[MAX_CONTROL_POINTS];
	int		numBaseControlPoints;

	int		RegisterControlPoint(BasePlayerSpawnPoint* cp) 
	{
	  r3d_assert(numControlPoints_ < MAX_CONTROL_POINTS);
	  controlPoints_[numControlPoints_++] = cp;

	  if(cp->spawnType_ == BasePlayerSpawnPoint::SPAWN_NEUTRAL)
		neutralControlPoints[numNeutralControlPoints++] = cp;
	  else
		baseControlPoints[numBaseControlPoints++] = cp;

	  return numControlPoints_ - 1;
	}

  public:
	ControlPointsMgr()
	{
		numControlPoints_       = 0;
		numNeutralControlPoints = 0;
		numBaseControlPoints    = 0;
	}
	~ControlPointsMgr()
	{
	}
};

extern	ControlPointsMgr gCPMgr;
