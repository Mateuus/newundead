#pragma once

class CLauncherConfig
{
public:
	CLauncherConfig();

	std::string serialBuyURL;
	std::string serialExpiredBuyURL;

	std::string accountForgotPasswordURL;
	std::string accountRegisterURL;

	std::string myAccountURL;
	std::string forumsURL;
	std::string supportURL;
	std::string youtubeURL;
	std::string facebookURL;
	std::string twitterURL;

	std::string accountUnknownStatusMessage;
	std::string accountDeletedMessage;
	std::string accountBannedMessage;
	std::string accountHWBannedMessage;
	std::string accountFrozenMessage;

	std::string accountCreateFailedMessage;
	std::string accountCreateEmailTakenMessage;
	std::string accountCreateInvalidSerialMessage;

	std::string ToSURL;
	std::string EULAURL;

	std::string updateGameDataURL;
	std::string updateLauncherDataURL;
	std::string updateLauncherDataHostURL;

	std::string serverInfoURL;

	std::string webAPIDomainIP;
	std::string webAPIDomainBaseURL;

	std::string updateGameDataURL2;
	std::string updateLauncherDataURL2;
	std::string serverInfoURL2;

	int webAPIDomainPort;
	bool webAPIDomainUseSSL;
};

extern CLauncherConfig gLauncherConfig;