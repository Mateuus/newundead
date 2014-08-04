#include "r3dPCH.h"
#include "r3d.h"

bool	UPDATER_UPDATER_ENABLED  = 1;
char	UPDATER_VERSION[512]     = "1.1";
char	UPDATER_VERSION_SUFFIX[512] = "";
char	UPDATER_BUILD[512]	 = __DATE__ " " __TIME__;

char	BASE_RESOURSE_NAME[512]  = "WZ";
char	GAME_EXE_NAME[512]       = "MiniA.exe";
char	GAME_TITLE[512]          = "Undead Brasil";

// updater (xml and exe) and game info on our server.
//char	UPDATE_DATA_URL[512]     = "https://api1.thewarinc.com/wz/wz.xml";	// url for data update
//char	UPDATE_UPDATER_URL[512]  = "https://api1.thewarinc.com/wz/updater/woupd.xml";

// HIGHWIND CDN
//char	UPDATE_UPDATER_HOST[512] = "http://arktos-icdn.pandonetworks.com/wz/updater/";

// LOCAL TESTING
//http://arktos.pandonetworks.com/Arktos

bool	UPDATER_STEAM_ENABLED	 = false;
