#pragma once

class CLoginHelper
{
  public:
	char		username[128];
	char		passwd[128];
	char		hardwareid[50];
	char		miniaCrc[50];
	char		warguardCrc[50];
	  
	DWORD		CustomerID;
	DWORD		SessionID;
	DWORD		AccountStatus;
	
	enum {
	  ANS_Unactive,
	  ANS_Processing,
	  ANS_Timeout,
	  ANS_Error,
	  
	  ANS_Logged,
	  ANS_BadPassword,
	  ANS_GameActive,
	  ANS_Deleted, // account deleted 
	  ANS_TimeExpired,
	  ANS_Frozen,
	  ANS_Banned,
	  ANS_Unknown,
	};
	volatile DWORD loginAnswerCode;

  public:
	CLoginHelper()
	{
	  username[0]   = 0;
	  passwd[0]     = 0;
	  CustomerID    = 0;
	  SessionID     = 0;
	  AccountStatus = 0;
	  loginAnswerCode = ANS_Unactive;
	  
	  LoadUserName();
	}
	
	void		StartLogin() 
	{
	  r3d_assert(loginAnswerCode != ANS_Processing);
	  r3d_assert(*username);
	  r3d_assert(*passwd);

	  loginAnswerCode = CLoginHelper::ANS_Processing;
	}
	void		DoLogin();
	
	bool		CheckSteamLogin();

	void		SaveUserName();
	bool		LoadUserName();

	bool		LoadComputerToken();
	void		CreateComputerToken();
	
	void		CreateAuthToken(char* token) const;
	void		CreateLoginToken(char* token) const;
	void		CheckingCRC();
};
