#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"

#include "LauncherConfig.h"

CLauncherConfig gLauncherConfig;

CLauncherConfig::CLauncherConfig()
{

	serialBuyURL = "http://localhost/buy?ref=WarL";
	serialExpiredBuyURL = "http://localhost/buy?ref=WarE";

	accountForgotPasswordURL = "https://localhost/account_check";
	accountRegisterURL = "http://undeadbrasil.com/index.php?pagina=cadastro";

	myAccountURL = "https://localhost/account_check";
	forumsURL =    "http://localhost/forum";
	supportURL =   "http://localhost/support";
	youtubeURL =   "http://www.youtube.com";
	facebookURL =  "http://www.facebook.com";
	twitterURL =   "http://twitter.com/";

	accountUnknownStatusMessage = "Unknown account status, please contact support@localhost";
	accountDeletedMessage = "Your account was deleted because your payment was refunded or cancelled\n\nPlease contact your payment provider";
	accountBannedMessage = "Your account has been permanently banned";
	accountHWBannedMessage = "Your account has been banned by HardwareID";
	accountFrozenMessage = "Your account has been temporarily frozen because of violation of the Terms of Service ( Paragraph 2 )\n\nYou will be able to continue to use the service in %d hours";
  
	accountCreateFailedMessage = "Account creation failed, please try again later";
	accountCreateEmailTakenMessage = "There is already registered account with that email!\nPlease note that you must use unique email per The War Z account";
	accountCreateInvalidSerialMessage = "Serial Key is not valid after Serial Key Check\ncontact support@localhost";

	webAPIDomainIP = "198.50.173.40";
	webAPIDomainBaseURL = "/api/";
	webAPIDomainPort = 443;
	webAPIDomainUseSSL =  1 ? true : false;

	ToSURL =  "http://198.50.173.40/doc/EULA.rtf";
	EULAURL = "http://198.50.173.40/doc/TOS.rtf";

	updateGameDataURL = "http://198.50.173.40/wz/data/wz.xml";
	updateLauncherDataURL = "http://198.50.173.40/wz/updater/woupd.xml";
	// updateLauncherDataHostURL Used by -generate cmdline arg to output a woupd.xml file.
	updateLauncherDataHostURL = "http://198.50.173.40/wz/updater/";

	serverInfoURL = "http://198.50.173.40/api_getserverinfo.xml";

	
	#define CHECK_I(xx) if(xx == 0)  r3dError("missing %s value", #xx);
	#define CHECK_S(xx) if(xx == "") r3dError("missing %s value", #xx);
	CHECK_I(webAPIDomainPort);
	CHECK_S(webAPIDomainIP);
	CHECK_S(webAPIDomainBaseURL);

	CHECK_S(updateGameDataURL);
	CHECK_S(updateLauncherDataURL);
	CHECK_S(updateLauncherDataHostURL);

	CHECK_S(serverInfoURL);
	#undef CHECK_I
	#undef CHECK_S
 
	return;
}