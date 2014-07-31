#pragma once

#include "../../ServerNetPackets/NetPacketsGameInfo.h"

class CMasterServerConfig
{
  public:
	int		masterPort_;
	int		clientPort_;
	int		serverId_;
	int		masterCCU_;	// max number of connected peers

	float supervisorCoolDownSeconds_;

	//
	// permanent games groups
	//
	struct permGame_s
	{
	  GBGameInfo	ginfo;
	  
	  permGame_s()
	  {
	  }
	};
	permGame_s	permGames_[4096];
	int		numPermGames_;

	void		LoadConfig();

	void		Temp_Load_WarZGames();

	void		LoadPermGamesConfig();
	void LoadRentConfig();
	void SetRentPwd(int gameServerId, const char* pwd);
	void AddRentGame(int customerid , const char* name , const char* pwd , int mapid , int slot , int period);
	void		 ParsePermamentGame(int gameServerId, const char* name, const char* map, const char* data,const char* pwdchar, bool ispass,bool ispre,bool isfarm , int owner);
	void		 AddPermanentGame(int gameServerId, const GBGameInfo& ginfo, EGBGameRegion region);
	
  public:
	CMasterServerConfig();
};
extern CMasterServerConfig* gServerConfig;
