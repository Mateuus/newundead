#pragma once

class CUserSkills
{
  public:
	//
	// hardcoded vars used to generate skill Ids.
	//
	
	static const int NUM_TIERS = 3;
	static const int NUM_SKILLS_PER_TIER = 10;
	static const int SKILL_CLASS_MULT = 100;	// multiplier used in SkillID to detect skill class
	static const int NUM_RANKS = 5;

	// some random numbers. amount of SP we need to spend on tier to unlock next one
	static const int TIER2_UNLOCK = 20;
	static const int TIER3_UNLOCK = 20;
	
	enum EClassID
	{
		CLASS_Assault = 0,
		CLASS_Spec    = 1,
		CLASS_Recon   = 2,
		CLASS_Medic   = 3,
		CLASS_MAX,
	};

	// note: adjust setSkillNames when changing skill ids
	enum ESkillIDs
	{
		SKILL_ID_END           = 400,
	};
	
	const static char* SkillNames[SKILL_ID_END];
};

