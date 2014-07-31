#include "r3dPCH.h"
#include "r3d.h"

#include "UserSettings.h"

UserSettings gUserSettings;

#define USERSETTINGS_FILE "userSettings.xml"

UserSettings::UserSettings()
{

}

UserSettings::~UserSettings()
{

}

bool UserSettings::createFullPath( char* dest, bool old )
{
	bool res = old ? CreateWorkPath(dest) : CreateConfigPath(dest);
	if(res)
		strcat( dest, USERSETTINGS_FILE );
	return res;
}

void UserSettings::loadSettings()
{
	char FullIniPath[ MAX_PATH * 2 ];
	bool createdPath = createFullPath( FullIniPath, false );
	if( createdPath && r3d_access( FullIniPath, 4 ) == 0 )
	{
		r3dOutToLog( "userSettings: found file at %s\n", FullIniPath );
		loadXML(FullIniPath);
	}
	else
	{
		createdPath = createFullPath( FullIniPath, true );

		if( createdPath && r3d_access( FullIniPath, 4 ) == 0 )
		{
			r3dOutToLog( "userSettings: found file at %s\n", FullIniPath );
			loadXML(FullIniPath);
		}
		else
		{
			if( !createdPath )
			{
				r3dOutToLog( "userSettings: Error: couldn't get local app path! Using defaults!\n" );
			}
			else
			{
				r3dOutToLog( "userSettings: couldn't open both %s and %s! Using defaults.\n", USERSETTINGS_FILE, FullIniPath );
			}
		}
	}
}

void UserSettings::saveSettings()
{
	char fullPath[ MAX_PATH * 2 ];

	if( createFullPath( fullPath, false ) )
	{
		r3dOutToLog( "userSettings: Saving settings to %s\n", fullPath );
		saveXML(fullPath);
	}
	else
	{
		r3dOutToLog( "userSettings: couldn't create path to %s\n", fullPath );
	}
}

static const int userSettings_file_version = 2;
void UserSettings::saveXML(const char* file)
{
	// save into xml
	pugi::xml_document xmlFile;
	xmlFile.append_child(pugi::node_comment).set_value("User Settings");

	pugi::xml_node xmlGlobal = xmlFile.append_child();
	xmlGlobal.set_name("Global");

	xmlGlobal.append_attribute("version") = userSettings_file_version;

	pugi::xml_node xmlBrowseFilters = xmlFile.append_child();
	xmlBrowseFilters.set_name("BrowseGamesFilter");

	xmlBrowseFilters.append_attribute("gameworld") = BrowseGames_Filter.gameworld;
	xmlBrowseFilters.append_attribute("stronghold") = BrowseGames_Filter.stronghold;
	xmlBrowseFilters.append_attribute("hideempty") = BrowseGames_Filter.hideempty;
	xmlBrowseFilters.append_attribute("hidefull") = BrowseGames_Filter.hidefull;

	xmlBrowseFilters.append_attribute("tracers") = BrowseGames_Filter.tracers;
	xmlBrowseFilters.append_attribute("nameplates") = BrowseGames_Filter.nameplates;
	xmlBrowseFilters.append_attribute("crosshair") = BrowseGames_Filter.crosshair;

	xmlBrowseFilters.append_attribute("region_us") = BrowseGames_Filter.region_us;
	xmlBrowseFilters.append_attribute("region_eu") = BrowseGames_Filter.region_eu;

	xmlBrowseFilters.append_attribute("name") = BrowseGames_Filter.name;

	pugi::xml_node xmlRecentGames = xmlFile.append_child();
	xmlRecentGames.set_name("RecentGames");
	xmlRecentGames.append_attribute("count") = RecentGames.size();
	for(std::list<DWORD>::iterator it = RecentGames.begin(); it!=RecentGames.end(); ++it)
	{
		pugi::xml_node xmlGame = xmlRecentGames.append_child();
		xmlGame.set_name("Game");
		DWORD id = *it;
		xmlGame.append_attribute("id") = (unsigned int)id;
	}

	pugi::xml_node xmlFavGames = xmlFile.append_child();
	xmlFavGames.set_name("FavoriteGames");
	xmlFavGames.append_attribute("count") = FavoriteGames.size();
	for(std::list<DWORD>::iterator it = FavoriteGames.begin(); it!=FavoriteGames.end(); ++it)
	{
		pugi::xml_node xmlGame = xmlFavGames.append_child();
		xmlGame.set_name("Game");
		DWORD id = *it;
		xmlGame.append_attribute("id") = (unsigned int)id;
	}

	xmlFile.save_file(file);
}

void UserSettings::loadXML(const char* file)
{
	r3dFile* f = r3d_open(file, "rb");
	if ( !f )
	{
		r3dOutToLog("Failed to open user settings xml file: %s\n", file);
		return;
	}

	char* fileBuffer = new char[f->size + 1];
	fread(fileBuffer, f->size, 1, f);
	fileBuffer[f->size] = 0;

	pugi::xml_document xmlFile;
	pugi::xml_parse_result parseResult = xmlFile.load_buffer_inplace(fileBuffer, f->size);
	fclose(f);
	if(!parseResult)
		r3dError("Failed to parse XML, error: %s", parseResult.description());

	// check for version
	pugi::xml_node xmlGlobal = xmlFile.child("Global");
	if(xmlGlobal.attribute("version").as_int() != userSettings_file_version)
	{
		r3dOutToLog("userSettings: old version file. reverting to default\n");
		delete [] fileBuffer;
		return;
	}


	pugi::xml_node xmlBrowseFilters = xmlFile.child("BrowseGamesFilter");

	BrowseGames_Filter.gameworld = xmlBrowseFilters.attribute("gameworld").as_bool();
	BrowseGames_Filter.stronghold = xmlBrowseFilters.attribute("stronghold").as_bool();
	BrowseGames_Filter.hideempty = xmlBrowseFilters.attribute("hideempty").as_bool();
	BrowseGames_Filter.hidefull = xmlBrowseFilters.attribute("hidefull").as_bool();

	BrowseGames_Filter.tracers = xmlBrowseFilters.attribute("tracers").as_bool();
	BrowseGames_Filter.nameplates = xmlBrowseFilters.attribute("nameplates").as_bool();
	BrowseGames_Filter.crosshair = xmlBrowseFilters.attribute("crosshair").as_bool();

	BrowseGames_Filter.region_us = xmlBrowseFilters.attribute("region_us").as_bool();
	BrowseGames_Filter.region_eu = xmlBrowseFilters.attribute("region_eu").as_bool();

	sprintf(BrowseGames_Filter.name,"");
	//sprintf(BrowseGames_Filter.name,xmlBrowseFilters.attribute("name").value());
	//r3dOutToLog("name %s\n",BrowseGames_Filter.name);

	pugi::xml_node xmlRecentGames = xmlFile.child("RecentGames");
	uint32_t count = xmlRecentGames.attribute("count").as_uint();
	for(uint32_t i=0; i<count; ++i)
	{
		pugi::xml_node xmlGame = xmlRecentGames.child("Game");
		RecentGames.push_back(xmlGame.attribute("id").as_uint());
	}

	pugi::xml_node xmlFavGames = xmlFile.child("FavoriteGames");
	count = xmlFavGames.attribute("count").as_uint();
	for(uint32_t i=0; i<count; ++i)
	{
		pugi::xml_node xmlGame = xmlFavGames.child("Game");
		FavoriteGames.push_back(xmlGame.attribute("id").as_uint());
	}

	// delete only after we are done parsing xml!
	delete [] fileBuffer;
}
void UserSettings::addGameToRecent(DWORD gameID)
{
	std::list<DWORD>::iterator it = std::find(RecentGames.begin(), RecentGames.end(), gameID);
	if(it != RecentGames.end())
	{
		RecentGames.push_front(gameID);
		while(RecentGames.size() > 13) // limit to one screen in UI
			RecentGames.pop_back();
	}
}
void UserSettings::addGameToFavorite(DWORD gameID)
{
	std::list<DWORD>::iterator it = std::find(FavoriteGames.begin(), FavoriteGames.end(), gameID);
	if(it != FavoriteGames.end())
		FavoriteGames.erase(it);
	else
		FavoriteGames.push_front(gameID);
}
bool UserSettings::isInRecentGamesList(DWORD gameID)
{
	return std::find(RecentGames.begin(), RecentGames.end(), gameID)!=RecentGames.end();
}
bool UserSettings::isInFavoriteGamesList(DWORD gameID)
{
	return std::find(FavoriteGames.begin(), FavoriteGames.end(), gameID)!=FavoriteGames.end();
}
