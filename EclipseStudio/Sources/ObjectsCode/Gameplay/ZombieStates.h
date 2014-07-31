//=========================================================================
//	Module: ZombieStates.h
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#pragma once

class EZombieStates
{
  public:
	// global (game wide) zombie state
	enum EGlobalStates
	{
		ZState_Dead = 0,
		ZState_Sleep,
		ZState_Waking,
		ZState_Idle,
		ZState_Walk,
		ZState_Pursue,
		ZState_Attack,
		ZState_BarricadeAttack,

		ZState_NumStates,
	};
};

/*
#include "../ai/StateMachine.h"
#include "obj_Zombie.h"

//////////////////////////////////////////////////////////////////////////
class obj_Zombie;
class ZombieLookForTargetState: public r3dState<obj_Zombie>
{
public:
	virtual void Enter(obj_Zombie *o);
	virtual void Execute(obj_Zombie *o);
	virtual void Exit(obj_Zombie *o);
};

//////////////////////////////////////////////////////////////////////////

class ZombieMoveToTargetState: public r3dState<obj_Zombie>
{
public:
	virtual void Enter(obj_Zombie *o);
	virtual void Execute(obj_Zombie *o);
	virtual void Exit(obj_Zombie *o);
};

//////////////////////////////////////////////////////////////////////////

class ZombieAttackTargetState: public r3dState<obj_Zombie>
{
public:
	virtual void Enter(obj_Zombie *o);
	virtual void Execute(obj_Zombie *o);
	virtual void Exit(obj_Zombie *o);
};

//////////////////////////////////////////////////////////////////////////

class ZombieDieState: public r3dState<obj_Zombie>
{
public:
	virtual void Enter(obj_Zombie *o);
	virtual void Execute(obj_Zombie *o);
	virtual void Exit(obj_Zombie *o);
};

//////////////////////////////////////////////////////////////////////////

class ZombieGlobalState: public r3dState<obj_Zombie>
{
public:
	virtual void Enter(obj_Zombie *o);
	virtual void Execute(obj_Zombie *o);
	virtual void Exit(obj_Zombie *o);
};

//////////////////////////////////////////////////////////////////////////

extern ZombieLookForTargetState gZombieLookForTargetState;
extern ZombieMoveToTargetState gZombieMoveToTargetState;
extern ZombieAttackTargetState gZombieAttackTargetState;
extern ZombieDieState gZombieDieState;
extern ZombieGlobalState gZombieGlobalState;

//////////////////////////////////////////////////////////////////////////

*/