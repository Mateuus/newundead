//=========================================================================
// Animals
// Create by NooKud
//=========================================================================

#pragma once

class EAnimalsStates
{
  public:
	enum EAnimalsGlobalStates
	{
		AState_Dead = 0,
		AState_Idie,
		AState_Eating,
        AState_Walk,
		AState_Run,
		AState_JumpSprint,


		AState_NumStates,
	};
};