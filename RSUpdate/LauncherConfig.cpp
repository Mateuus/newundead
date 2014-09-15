#include "r3dPCH.h"
#include "r3d.h"
#include "r3dNetwork.h"

#include "LauncherConfig.h"

CLauncherConfig gLauncherConfig;

CLauncherConfig::CLauncherConfig()
{

	serialBuyURL = "http://www.undeadbrasil.com";
	serialExpiredBuyURL = "http://www.undeadbrasil.com";

	accountForgotPasswordURL = "http://www.undeadbrasil.com/?pagina=recuperar";
	accountRegisterURL = "http://www.undeadbrasil.com/?pagina=cadastro";

	myAccountURL = "http://undeadbrasil.com/index.php?pagina=painel";
	forumsURL =    "http://forum.undeadbrasil.com/";
	supportURL =   "http://forum.undeadbrasil.com/index.php?/forum/19-suporte/";
	youtubeURL =   "http://www.youtube.com";
	facebookURL =  "https://www.facebook.com/equipeundeadbrasil";
	twitterURL =   "http://twitter.com/";

	accountUnknownStatusMessage = "Unknown account status, please contact support@localhost";
	accountDeletedMessage = "Your account was deleted because your payment was refunded or cancelled\n\nPlease contact your payment provider";
	accountBannedMessage = "Your account has been permanently banned";
	accountHWBannedMessage = "Your account has been banned by HardwareID";
	accountFrozenMessage = "Your account has been temporarily frozen because of violation of the Terms of Service ( Paragraph 2 )\n\nYou will be able to continue to use the service in %d hours";
  
	accountCreateFailedMessage = "Account creation failed, please try again later";
	accountCreateEmailTakenMessage = "There is already registered account with that email!\nPlease note that you must use unique email per The War Z account";
	accountCreateInvalidSerialMessage = "Serial Key is not valid after Serial Key Check\ncontact support@localhost";

	webAPIDomainIP = "167.114.32.63";
	webAPIDomainBaseURL = "/conexao/api/";
	webAPIDomainPort = 80;
	webAPIDomainUseSSL = false;

	ToSURL =  "http://undeadbrasil.com/conexao/other/eula-en.htm";
	EULAURL = "http://undeadbrasil.com/conexao/other/eula-en.rtf";

	updateGameDataURL = "http://167.114.32.63/conexao/wz/wz.xml";
	updateLauncherDataURL = "http://167.114.32.63/conexao/wz/updater/woupd.xml";
 // updateLauncherDataHostURL Used by -generate cmdline arg to output a woupd.xml file.
	updateLauncherDataHostURL = "http://167.114.32.63/conexao/wz/updater/";
	serverInfoURL = "http://167.114.32.63/conexao/api_getserverinfo1.xml";



	
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