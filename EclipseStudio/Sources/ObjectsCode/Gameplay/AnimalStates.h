//=========================================================================
//	Module: AnimalStates.h
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#pragma once

class EAnimalStates
{
  public:
	// global (game wide) zombie state
	enum EGlobalStates
	{
		ZState_Dead = 0,
		ZState_Idle,
		ZState_Walk,
		AState_Sprint,
		AState_SprintJump,
		AState_IdleEating,
		//ZState_Crawling,

		ZState_NumStates,
	};
};

