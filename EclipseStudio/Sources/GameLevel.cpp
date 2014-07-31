#include "r3dPCH.h"
#include "r3d.h"

#include "GameLevel.h"

namespace r3dGameLevel
{
	static char	HomeDirectory[256];
	static char	ColorDirectory[256];
	static char	SaveDirectory[256];
	float		StartLevelTime = 0;

	r3dAtmosphere	Environment;

	const char*	GetHome2Dir() { return ColorDirectory; }
	const char*	GetHomeDir() { return HomeDirectory; }
	const char* GetSaveDir() { return SaveDirectory; }
	void		SetHomeDir(const char* MapName);
	void		SetStartGameTime(float Time) { StartLevelTime = Time; }

};


void r3dGameLevel :: SetHomeDir(const char* MapName)
{
	sprintf(HomeDirectory,  "Levels\\%s", MapName);
	sprintf(ColorDirectory,  "Levels\\WZ_Colorado");
	strcpy( SaveDirectory, HomeDirectory ) ;
	//strcpy( SaveDirectory, ColorDirectory ) ;
	r3dOutToLog("SetHomeDir: %s\n", HomeDirectory);
}

void r3dGameLevel :: SetSaveDir(const char* dir )
{
	strcpy(SaveDirectory,  dir);
	r3dOutToLog("SetSaveDir: %s\n", SaveDirectory);
}
