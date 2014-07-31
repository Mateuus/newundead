#include "r3dPCH.h"
#include "r3d.h"
#include "shellapi.h"

#include "UserSkills.h"

// there is no data here yet..

const char* CUserSkills::SkillNames[CUserSkills::SKILL_ID_END] = {0};

static int setSkillNames()
{
	#define SET_NAME(xx) CUserSkills::SkillNames[CUserSkills::xx] = #xx
	#undef SET_NAME
	return 1;
}

static int skillNamesInited = setSkillNames();
