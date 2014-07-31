#pragma once

// to save\load various user settings that are not compatible with vars (variable number of params for example)
class UserSettings
{
private:
	struct FilterGames
	{
		bool gameworld;
		bool stronghold;
		bool hideempty;
		bool hidefull;

		bool tracers;
		bool nameplates;
		bool crosshair;

		bool region_us;
		bool region_eu;
		bool region_ru;
		char name[512];

		FilterGames() 
		{ 
			gameworld = true;
			stronghold = true;
			hideempty = false;
			hidefull = false;

			tracers = false;
			nameplates = false;
			crosshair = false;

			region_us = true;
			region_eu = false;
			sprintf(name,"");
		}
	};
public:
	UserSettings();
	~UserSettings();

	FilterGames BrowseGames_Filter;
	std::list<DWORD> RecentGames;
	std::list<DWORD> FavoriteGames;

	void loadSettings();
	void saveSettings();

	void addGameToRecent(DWORD gameID);
	void addGameToFavorite(DWORD gameID); // will auto from list if game already in list

	bool isInRecentGamesList(DWORD gameID);
	bool isInFavoriteGamesList(DWORD gameID);

private:
	bool createFullPath( char* dest, bool old );

	void loadXML(const char* file);
	void saveXML(const char* file);
};
extern UserSettings gUserSettings;