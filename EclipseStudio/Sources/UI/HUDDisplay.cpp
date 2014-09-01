#include "r3dPCH.h"
#include "r3dProtect.h"

#include "HUDDisplay.h"


#include "ObjectsCode/Gameplay/BasePlayerSpawnPoint.h"
#include "ObjectsCode/Gameplay/obj_Grave.h"
#include "../multiplayer/clientgamelogic.h"
#include "../ObjectsCode/ai/AI_Player.H"
#include "../ObjectsCode/weapons/Weapon.h"
#include "../ObjectsCode/weapons/WeaponArmory.h"

#include "HUDPause.h"

#include "../VoIP/public_definitions.h"
#include "../VoIP/public_errors.h"
#include "../VoIP/clientlib_publicdefinitions.h"
#include "../VoIP/clientlib.h"

/*
* TeamSpeak 3 client sample
*
* Copyright (c) 2007-2012 TeamSpeak Systems GmbH
*/

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#endif

struct NameHashFunc_T
{
	inline int operator () ( const char * szKey )
	{
		return r3dHash::MakeHash( szKey );
	}
};
static HashTableDynamic<const char*, FixedString256, NameHashFunc_T, 1024> dictionaryHash_;

HUDDisplay :: HUDDisplay()
{
	Inited = false;
	chatVisible = false;
	chatInputActive = false;
	chatVisibleUntilTime = 0;
	lastChatMessageSent = 0;
	playersListVisible = false;
	bloodAlpha = 0.0f;
	writeNoteSavedSlotIDFrom = 0;
	timeoutForNotes = 0;
	timeoutNoteReadAbuseReportedHideUI = 0;
	RangeFinderUIVisible = false;

	if(dictionaryHash_.Size() == 0)
	{
		r3dFile* f = r3d_open( "Data/LangPack/dictionary.txt", "rb" );
		if (!f->IsValid())
		{
			f = r3d_open( "Data/LangPack/dictionary.dat", "rb" );
		}

		if (f->IsValid())
		{
			char tmpStr[256];
			while(fgets(tmpStr, 256, f) != NULL)
			{
				size_t len = strlen(tmpStr);
				for(size_t i=0; i<len; ++i)
				{
					if(tmpStr[i]==13 || tmpStr[i]==10)
						tmpStr[i]=0;
				}
				dictionaryHash_.Add(tmpStr, tmpStr);	
			}
			fclose(f);
		}
	}
}

HUDDisplay :: ~HUDDisplay()
{
	dictionaryHash_.Clear();
}

bool HUDDisplay::Init()
{

	status = 5;
	isT = false;
	idietime = 0.0f;
	Cooldown = 0.0f;
	updatetime = 0.0f;
	updatetime2 = 0.0f;
	currentslot = 999999;
	isgroups = false;
	IsNoDrop = false;
	if(!gfxHUD.Load("Data\\Menu\\WarZ_HUD.swf", true)) 
		return false;
	if(!gfxBloodStreak.Load("Data\\Menu\\WarZ_BloodStreak.swf", false))
		return false;
	if(!gfxRangeFinder.Load("Data\\Menu\\WarZ_HUD_RangeFinder.swf", false))
		return false;

	gfxHUD.SetCurentRTViewport( Scaleform::GFx::Movie::SM_ExactFit );
	gfxBloodStreak.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);
	gfxRangeFinder.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);

	//hudGeneralStore = new HUDGeneralStore();

#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDDisplay>(this, &HUDDisplay::FUNC)
	gfxHUD.RegisterEventHandler("eventChatMessage", MAKE_CALLBACK(eventChatMessage));
	gfxHUD.RegisterEventHandler("eventNoteWritePost", MAKE_CALLBACK(eventNoteWritePost));
	gfxHUD.RegisterEventHandler("eventNoteClosed", MAKE_CALLBACK(eventNoteClosed));
	gfxHUD.RegisterEventHandler("eventGraveNoteClosed", MAKE_CALLBACK(eventGraveNoteClosed));
	gfxHUD.RegisterEventHandler("eventNoteReportAbuse", MAKE_CALLBACK(eventNoteReportAbuse));
	gfxHUD.RegisterEventHandler("eventShowPlayerListContextMenu", MAKE_CALLBACK(eventShowPlayerListContextMenu));
	gfxHUD.RegisterEventHandler("eventPlayerListAction", MAKE_CALLBACK(eventPlayerListAction));
	{
		Scaleform::GFx::Value var[4];
		var[0].SetInt(0);
		var[1].SetString("PROXIMITY");
		var[2].SetBoolean(true);
		var[3].SetBoolean(true);
		gfxHUD.Invoke("_root.api.setChatTab", var, 4);
		var[0].SetInt(1);
		var[1].SetString("GLOBAL");
		var[2].SetBoolean(false);
		var[3].SetBoolean(true);
		gfxHUD.Invoke("_root.api.setChatTab", var, 4);
		var[0].SetInt(2);
		var[1].SetString("CLAN");
		var[2].SetBoolean(false);
		var[3].SetBoolean(false);
		gfxHUD.Invoke("_root.api.setChatTab", var, 4);
		var[0].SetInt(3);
		var[1].SetString("GROUP");
		var[2].SetBoolean(false);
		var[3].SetBoolean(true);
		gfxHUD.Invoke("_root.api.setChatTab", var, 4);

		currentChatChannel = 0;
		var[0].SetInt(0);
		gfxHUD.Invoke("_root.api.setChatTabActive", var, 1);
	}

	setChatTransparency(R3D_CLAMP(g_ui_chat_alpha->GetFloat()/100.0f, 0.0f, 1.0f));

	Inited = true;

	weaponInfoVisible = -1;
	SafeZoneWarningVisible = false;

	if (gClientLogic().m_gameInfo.isfarm)
	{
		addChatMessage(0,"<SYSTEM>","You have joined a stronghold server.",0);
	}
	else if (gClientLogic().m_gameInfo.ispass)
	{
		addChatMessage(0,"<SYSTEM>","You have joined a private server.",0);
	}
	else if (gClientLogic().m_gameInfo.ispre)
	{
		addChatMessage(0,"<SYSTEM>","You have joined a premium server.",0);
	}
	else
	{
		addChatMessage(0,"<SYSTEM>","You have joined an official server.",0);
	}
	/*if (gClientLogic().localPlayer_->CurLoadout.Mission1 != 0 && gClientLogic().localPlayer_->CurLoadout.Mission1 != 3)
	{
	addMissionInfo("Lets Play");
	if (gClientLogic().localPlayer_->CurLoadout.Mission1 == 1)
	{
	addMissionObjective(0,"PICK UP ITEM",false,"0/1",true);
	addMissionObjective(0,"KILL 5 ZOMBIES",false,"0/5",true);
	}
	else if (gClientLogic().localPlayer_->CurLoadout.Mission1 == 2)
	{
	addMissionObjective(0,"PICK UP ITEM",true,"0/1",true);
	addMissionObjective(0,"KILL 5 ZOMBIES",false,"0/5",true);
	}
	showMissionInfo(true);
	}*/
	/*addMissionInfo("Lets Play");
	addMissionObjective(0,"PICK UP ITEM",false,"0/1",true);
	addMissionObjective(0,"KILL 5 ZOMBIES",false,"0/5",true);
	//addMissionInfo("kiss jack the ripper");
	//addMissionInfo("kiss Kuriyama Mirai");
	showMissionInfo(true);*/

	return true;
}

bool HUDDisplay::Unload()
{
	gfxHUD.Unload();
	gfxBloodStreak.Unload();
	gfxRangeFinder.Unload();

	Inited = false;
	return true;
}

void HUDDisplay::setCarInfo(int var1 , int var2 , int var3 ,int var4, int var5,int var6 , bool show)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[5];
	var[0].SetInt(var1); // Damage
	var[1].SetInt(100-var2); // RPM
	var[2].SetInt(var3); // SPEED
	var[3].SetInt(var4); // Gasoline
	var[4].SetInt(100-var5); // RotationSpeed
	Scaleform::GFx::Value seat[2];
	if (gClientLogic().localPlayer_->isInVehicle())
		seat[0].SetInt(0); // position where you feel
	else
		seat[0].SetInt(1); // position where you feel
	seat[1].SetString("");
	Scaleform::GFx::Value car[1];
	switch(var6)
	{
	case 1: 	
		  car[0].SetString("BUGGY");
		  break;
	case 2:
		  car[0].SetString("TRUCK");
		  break;
	case 3:
		  car[0].SetString("STRYKER");
		  break;

	}
	gfxHUD.Invoke("_root.api.setCarInfo", var , 5);
	gfxHUD.Invoke("_root.api.setCarTypeInfo",car,1);
	gfxHUD.Invoke("_root.api.setCarSeatInfo",seat,2);
	gfxHUD.Invoke("_root.api.setCarInfoVisibility", show);
}


void HUDDisplay::enableClanChannel()
{
	Scaleform::GFx::Value var[4];
	var[0].SetInt(2);
	var[1].SetString("CLAN");
	var[2].SetBoolean(false);
	var[3].SetBoolean(true);
	gfxHUD.Invoke("_root.api.setChatTab", var, 4);
}

void HUDDisplay::enableGroupChannel()
{
	Scaleform::GFx::Value var[4];
	var[0].SetInt(3);
	var[1].SetString("GROUP");
	var[2].SetBoolean(false);
	var[3].SetBoolean(true);
	gfxHUD.Invoke("_root.api.setChatTab", var, 4);
}

int HUDDisplay::Update()
{

	if(!Inited)
		return 1;

	int value;
	int timeLeft = int(ceilf(Cooldown - r3dGetTime()));
	if(Cooldown > 0)
	{
		//int timeLeftBar = int(ceilf(0.0f + r3dGetTime()));

		if (timeLeft == 9)
		{
			value = 20;
		}
		else if (timeLeft == 8)
		{
			value = 40;
		}
		else if (timeLeft == 7)
		{
			value = 60;
		}
		else if (timeLeft == 6)
		{
			value = 80;
		}
		else if (timeLeft == 5)
		{
			value = 100;
		}
		//			 	 else if (timeLeft == 0)
		// {
		// value = 100;
		//	 }
		if(timeLeft > 0)
		{
			// setCooldown(currentslot,(int)r3dGetFrameTime()*100,timeLeft);
			setCooldown(currentslot,value,10-timeLeft);
		}
		else
		{
			Cooldown = 0.0f;
		}
	}

	//r3dOutToLog("%d",value);
	if (value == 100)
	{
		value = 100;
		timeLeft = 0;
		Cooldown = 0.0f;
		//setCooldown(currentslot,100,timeLeft);r3dGetFrameTime()
		setCooldown(currentslot,100,10-timeLeft);
	}
	//addVoip("123456");
	bool iscancel = false;
	if (idietime > 120.10f)
	{
		iscancel = true;
	}
	if (idietime > 120.0f && !iscancel)
	{
		// disconnectTs3();
		addChatMessage(1,"<voice>","Disconnect : idie",2);
	}
	if (!iscancel)
	{
		//idietime = 1.0f + r3dGetTime();
	}


	const ClientGameLogic& CGL = gClientLogic();

	if(r3dGetTime() > chatVisibleUntilTime && chatVisible && !chatInputActive)
	{
		showChat(false);
	}

	if(r3dGetTime() > timeoutNoteReadAbuseReportedHideUI && timeoutNoteReadAbuseReportedHideUI != 0)
	{
		r3dMouse::Hide();
		writeNoteSavedSlotIDFrom = 0;
		timeoutNoteReadAbuseReportedHideUI = 0;
		timeoutForNotes = r3dGetTime() + 0.5f;
		Scaleform::GFx::Value var[2];
		var[0].SetBoolean(false);
		var[1].SetString("");
		gfxHUD.Invoke("_root.api.showNoteRead", var, 2);
	}

	if(RangeFinderUIVisible)
	{
		r3dPoint3D dir;
		r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);

		PxRaycastHit hit;
		PhysicsCallbackObject* target = NULL;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
		g_pPhysicsWorld->raycastSingle(PxVec3(gCam.x, gCam.y, gCam.z), PxVec3(dir.x, dir.y, dir.z), 2000.0f, PxSceneQueryFlag::eDISTANCE, hit, filter);

		float distance = -1;
		if(hit.shape)
		{
			// sergey's design (range finder shows not real distance... have no idea what it actually shows)
			distance = hit.distance * (1.0f + R3D_MIN(1.0f, (R3D_MAX(0.0f, (hit.distance-200.0f)/1800.0f)))*0.35f);
		}
		gfxRangeFinder.Invoke("_root.Main.Distance.gotoAndStop", distance!=-1?"on":"off");	
		char tmpStr[16];
		sprintf(tmpStr, "%.1f", distance);
		gfxRangeFinder.SetVariable("_root.Main.Distance.Distance.Distance.text", tmpStr);

		const ClientGameLogic& CGL = gClientLogic();
		float compass = atan2f(CGL.localPlayer_->m_vVision.z, CGL.localPlayer_->m_vVision.x)/R3D_PI;
		compass = R3D_CLAMP(compass, -1.0f, 1.0f);

		float cmpVal = -(compass * 820);
		gfxRangeFinder.SetVariable("_root.Main.compass.right.x", cmpVal);
		gfxRangeFinder.SetVariable("_root.Main.compass.left.x", cmpVal-1632);

		if(!CGL.localPlayer_->m_isAiming)
			showRangeFinderUI(false); // in case if player switched weapon or anything happened
	}

	return 1;
}


int HUDDisplay::Draw()
{
	if(!Inited)
		return 1;
	{
		R3DPROFILE_FUNCTION("gfxBloodStreak.UpdateAndDraw");
		if(bloodAlpha > 0.0f)
			gfxBloodStreak.UpdateAndDraw();
	}
	{
		R3DPROFILE_FUNCTION("gfxRangeFinder.UpdateAndDraw");
		if(RangeFinderUIVisible)
			gfxRangeFinder.UpdateAndDraw();
	}
	{
		R3DPROFILE_FUNCTION("gfxHUD.UpdateAndDraw");
//#ifndef FINAL_BUILD
		gfxHUD.UpdateAndDraw(d_disable_render_hud->GetBool());
//#else
	//	gfxHUD.UpdateAndDraw();
//#endif
	}

	return 1;
}

void HUDDisplay::setBloodAlpha(float alpha)
{
	if(!Inited) return;
	if(R3D_ABS(bloodAlpha-alpha)<0.01f) return;

	bloodAlpha = alpha;
	gfxBloodStreak.SetVariable("_root.blood.alpha", alpha);
}


void HUDDisplay::eventShowPlayerListContextMenu(r3dScaleformMovie* pMove, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 1);

	int isDev = gUserProfile.ProfileData.isDevAccount;
	Scaleform::GFx::Value var[4];

	if(isDev)
	{
		var[0].SetInt(1);
		var[1].SetString("TELEPORT TO");
		var[2].SetInt(1);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(2);
		var[1].SetString("TELEPORT PLAYER");
		var[2].SetInt(2);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(3);
		var[1].SetString("KICK PLAYER");
		var[2].SetInt(3);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);

		var[0].SetInt(4);
		var[1].SetString("BAN ACCOUNT");
		var[2].SetInt(4);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
	}
	else 
	{
	for (int i=0; i<8;i++)
	{
		var[0].SetInt(i);
		var[1].SetString("");
		var[2].SetInt(i);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
	}
	if (gClientLogic().localPlayer_)
	{
		char plrUserName[64]; gClientLogic().localPlayer_->GetUserName(plrUserName);
		char username[64]; r3dscpy(username,args[0].GetString());
		if (strcmp(username,plrUserName))
		{
			for(int i=0; i<R3D_ARRAYSIZE(gClientLogic().playerNames); i++)
			{
				if (!strcmp(username,gClientLogic().playerNames[i].Gamertag) && !gClientLogic().playerNames[i].isInvitePending && gClientLogic().playerNames[i].GroupID == 0)
				{
					var[0].SetInt(1);
					var[1].SetString("INVITE INTO GROUPS");
					var[2].SetInt(1);
					gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
					//break;
				}
				else if (!strcmp(username,gClientLogic().playerNames[i].Gamertag) && gClientLogic().playerNames[i].isInvitePending)
				{
					var[0].SetInt(1);
					var[1].SetString("ACCEPT INVITE GROUPS");
					var[2].SetInt(1);
					gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
					var[0].SetInt(2);
					var[1].SetString("DECLINE INVITE GROUPS");
					var[2].SetInt(2);
					gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
					break;
				}
				if (gClientLogic().localPlayer_->CurLoadout.GroupID == gClientLogic().localPlayer_->CurLoadout.GroupID2 && gClientLogic().localPlayer_->CurLoadout.GroupID == gClientLogic().playerNames[i].GroupID && gClientLogic().localPlayer_->CurLoadout.GroupID != 0)
				{
					// leader kicked player.
					var[0].SetInt(3);
					var[1].SetString("KICK FROM GROUP");
					var[2].SetInt(3);
					gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);
				}
			}
		}
	}

		var[0].SetInt(4);
		var[1].SetString("");
		var[2].SetInt(4);
		gfxHUD.Invoke("_root.api.setPlayerListContextMenuButton", var, 3);


	}

	gfxHUD.Invoke("_root.api.showPlayerListContextMenu", "");
}

void HUDDisplay::eventPlayerListAction(r3dScaleformMovie* pMove, const Scaleform::GFx::Value* args, unsigned argCount)
{
	int action = args[0].GetInt();
	const	char* pName = args[1].GetString();

	int isDev = gUserProfile.ProfileData.isDevAccount;
	if(isDev)
	// Developer Tab Key HUD Menu
	{
		if(action == 1) // Teleport To Player
		{
			showChatInput();


			char cmGoto[128];
			sprintf(cmGoto, "/goto %s ", pName);
			//gfxHUD.Invoke("_root.api.setChatActive", ffReport);


			chatVisible = true;
			Scaleform::GFx::Value var[3];
			var[0].SetBoolean(true);
			var[1].SetBoolean(true);
			var[2].SetString(cmGoto);
			gfxHUD.Invoke("_root.api.showChat", var, 3);
			chatVisibleUntilTime = r3dGetTime() + 20.0f;
		}
		if(action == 2) // Teleport Player to You
		{
			showChatInput();

			char cmKick[128];
			sprintf(cmKick, "/tome %s ", pName);
			//gfxHUD.Invoke("_root.api.setChatActive", ffReport);

			chatVisible = true;
			Scaleform::GFx::Value var[3];
			var[0].SetBoolean(true);
			var[1].SetBoolean(true);
			var[2].SetString(cmKick);
			gfxHUD.Invoke("_root.api.showChat", var, 3);
			chatVisibleUntilTime = r3dGetTime() + 20.0f;
		}
		if(action == 3) // Kick Player
		{
			showChatInput();

			char cmKick[128];
			sprintf(cmKick, "/kick %s ", pName);
			//gfxHUD.Invoke("_root.api.setChatActive", ffReport);


			chatVisible = true;
			Scaleform::GFx::Value var[3];
			var[0].SetBoolean(true);
			var[1].SetBoolean(true);
			var[2].SetString(cmKick);
			gfxHUD.Invoke("_root.api.showChat", var, 3);
			chatVisibleUntilTime = r3dGetTime() + 20.0f;
		}
		if(action == 4) // Ban Player
		{
		showChatInput();

		char cmBan[128];
		sprintf(cmBan, "/banp %s ", pName);

		chatVisible = true;
		Scaleform::GFx::Value var[3];
		var[0].SetBoolean(true);
		var[1].SetBoolean(true);
		var[2].SetString(cmBan);
		gfxHUD.Invoke("_root.api.showChat", var, 3);
		chatVisibleUntilTime = r3dGetTime() + 20.0f;
		}
	}
	else // Not Developer Tab Key HUD Menu
	{
    if (gClientLogic().localPlayer_)
	{
		//if (action == 2)
		//{
		char plrUserName[64]; gClientLogic().localPlayer_->GetUserName(plrUserName);
		char username[64]; r3dscpy(username,args[1].GetString());
		if (strcmp(username,plrUserName))
		{
			for(int i=0; i<R3D_ARRAYSIZE(gClientLogic().playerNames); i++)
			{
				if (action == 1)
				{
					if (!strcmp(username,gClientLogic().playerNames[i].Gamertag) && !gClientLogic().playerNames[i].isInvitePending/* && gClientLogic().playerNames[i].GroupID == 0*/)
					{
						if (gClientLogic().localPlayer_->CurLoadout.GroupID != 0 && gClientLogic().localPlayer_->CurLoadout.GroupID == gClientLogic().localPlayer_->CurLoadout.LoadoutID || gClientLogic().localPlayer_->CurLoadout.GroupID == 0 /* for create group*/) // only leader of groups.
						{
							// write group invite packet here..
							PKT_C2S_GroupInvite_s n;
							n.peerId = i;
							p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
							// fucking langmngr
							showMessage(gLangMngr.getString("GroupSentInviteSuccess"));
							showPlayersList(0); // close playerlist window
						}
						else if ( gClientLogic().localPlayer_->CurLoadout.GroupID != 0)// you not a group leader
						{
							showMessage(gLangMngr.getString("GroupOnlyLeaderCanInvite"));
							showPlayersList(0);
						}
						else
						{
							showMessage(gLangMngr.getString("GroupPlayerAlreadyInGroup"));
							showPlayersList(0);
						}
					}
					else if (!strcmp(username,gClientLogic().playerNames[i].Gamertag) && gClientLogic().playerNames[i].isInvitePending) // accept
					{
						PKT_C2S_GroupAccept_s n;
						n.peerId = i;
						p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
						gClientLogic().playerNames[i].isInvitePending = false;
						showPlayersList(0);
					}
				}
				else if( action == 2)
				{
					if (!strcmp(username,gClientLogic().playerNames[i].Gamertag) && gClientLogic().playerNames[i].isInvitePending)
					{
						PKT_C2S_GroupNoAccept_s n;
						n.peerId = i;
                        p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
						gClientLogic().playerNames[i].isInvitePending = false;
						showPlayersList(0);
						break;
					}
				}
				else if( action == 3)
				{
					if (gClientLogic().localPlayer_->CurLoadout.GroupID == gClientLogic().localPlayer_->CurLoadout.GroupID2 && gClientLogic().localPlayer_->CurLoadout.GroupID == gClientLogic().playerNames[i].GroupID && !strcmp(username,gClientLogic().playerNames[i].Gamertag))
					{
						PKT_C2S_GroupKick_s n;
						n.peerId = i;
                        p2pSendToHost(gClientLogic().localPlayer_,&n,sizeof(n));
						showPlayersList(0);
						break;
					}
				}
			}
		}
	}
  }
}



void HUDDisplay::aboutToLeavePlayerFromGroup(const char* var1)
{
    Scaleform::GFx::Value var[1];
	var[0].SetString(var1);
	gfxHUD.Invoke("_root.api.aboutToLeavePlayerFromGroup", var , 1);
}
void HUDDisplay::showMissionInfo(bool var1)
{
	Scaleform::GFx::Value var[1];
	var[0].SetBoolean(var1);
	gfxHUD.Invoke("_root.api.showMissionInfo", var , 1);
}
void HUDDisplay::removeMissionInfo(int var1)
{
	Scaleform::GFx::Value var[1];
	var[0].SetUInt(var1);
	gfxHUD.Invoke("_root.api.removeMissionInfo", var , 1);
}
void HUDDisplay::setMissionObjectiveCompleted(int var1 , int var2)
{
	Scaleform::GFx::Value var[2];
	var[0].SetUInt(var1);
	var[1].SetUInt(var2);
	gfxHUD.Invoke("_root.api.setMissionObjectiveCompleted", var , 2);
}
void HUDDisplay::addMissionObjective(int var1 , const char* var2 , bool var3 , const char* var4 , bool var5)
{
	Scaleform::GFx::Value var[5];
	var[0].SetUInt(var1);
	var[1].SetString(var2);
	var[2].SetBoolean(var3);
	var[3].SetString(var4);
	var[4].SetBoolean(var5);
	gfxHUD.Invoke("_root.api.addMissionObjective", var , 5);
}
void HUDDisplay::setMissionObjectiveNumbers(int var1, int var2 , const char* var3)
{
	Scaleform::GFx::Value var[3];
	var[0].SetUInt(var1);
	var[1].SetUInt(var2);
	var[2].SetString(var3);
	gfxHUD.Invoke("_root.api.setMissionObjectiveNumbers", var , 3);
}
void HUDDisplay::addMissionInfo(const char* var1)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(var1);
	gfxHUD.Invoke("_root.api.addMissionInfo", var , 1);
}
void HUDDisplay::removeplayerfromgroup(const char* gamertag,bool legend)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(gamertag);
	gfxHUD.Invoke("_root.api.removePlayerFromGroup", var , 1);
}

void HUDDisplay::addplayertogroup(const char* gamertag,bool legend)
{
	Scaleform::GFx::Value var[3];
	var[0].SetString(gamertag);
	var[1].SetBoolean(legend);
	var[2].SetBoolean(false);
	gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 3);
}
/*void onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
//printf("Connect status changed: %llu %d %d\n", (unsigned long long)serverConnectionHandlerID, newStatus, errorNumber);
/* Failed to connect ? /*
if(newStatus == STATUS_DISCONNECTED && errorNumber == ERROR_failed_connection_initialisation) {
r3dOutToLog("Looks like there is no server running, terminate!\n");
//exit(-1);
}
}*/
void HUDDisplay::eventChatMessage(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	chatInputActive = false;
	lastChatMessageSent = r3dGetTime();

	static char s_chatMsg[2048];
	int currentTabIndex = args[0].GetInt();
	r3dscpy(s_chatMsg, args[1].GetString());

	bool has_anything = false;

	size_t start_text=0;
	size_t argLen = strlen(s_chatMsg);
	if(argLen < 3)
		return;

	/*if(strncmp(s_chatMsg, "/stime", 6) == NULL)
		{
			char buf[256];
			int hour, min;
			if(3 != sscanf(s_chatMsg, "%s %d %d", buf, &hour, &min))
			{
				addChatMessage(0, "<SYSTEM>", "/stime {hour} {min}", 0);
				return;
			}



			__int64 gameUtcTime = gClientLogic().GetServerGameTime();
			struct tm* tm = _gmtime64(&gameUtcTime);
			r3d_assert(tm);

			// adjust server time to match supplied hour
			gClientLogic().gameStartUtcTime_ -= tm->tm_sec;
			gClientLogic().gameStartUtcTime_ -= (tm->tm_min) * 60;
			gClientLogic().gameStartUtcTime_ += (hour - tm->tm_hour) * 60 * 60;
			gClientLogic().gameStartUtcTime_ += (min) * 60;
			gClientLogic().lastShadowCacheReset_ = -1;

			addChatMessage(0, "<SYSTEM>", "time changed", 0);
			return;
		}*/
	if (gUserProfile.ProfileData.isDevAccount)
	{
		if(strncmp(s_chatMsg, "/nodrop", 6) == NULL)
		{
			if (IsNoDrop)
			{
				IsNoDrop = false;
				addChatMessage(0, "<SYSTEM>", "No Drop Disabled", 0);
				return;
			}

			IsNoDrop = true;
			addChatMessage(0, "<SYSTEM>", "No Drop Enabled", 0);
			return;

		}
		/*if(strncmp(s_chatMsg, "/accept", 6) == NULL)
		{
			char buf[256];
			char name[256];
			if(2 != sscanf(s_chatMsg, "%s %s", buf, &name))
			{
				addChatMessage(0, "<SYSTEM>", "/accept {name}", 0);
				return;
			}
			obj_Player* plr = gClientLogic().localPlayer_;
			PKT_S2C_SendGroupAccept_s n1;

			n1.FromCustomerID = plr->CustomerID;
			r3dscpy(n1.fromgamertag,plr->CurLoadout.Gamertag);

			r3dscpy(n1.intogamertag,name);
			p2pSendToHost(plr, &n1, sizeof(n1));
			//gClientLogic().playerNames[i].isInvitePending = false;
		}
		if(strncmp(s_chatMsg, "/spawngrave", 6) == NULL)
		{
			obj_Player* plr = gClientLogic().localPlayer_;
			r3d_assert(plr);
			obj_Grave* obj = (obj_Grave*)srv_CreateGameObject("obj_Grave", "obj_Grave", plr->GetPosition());
			obj->SetNetworkID(10000);
			obj->OnCreate();
		}
		if(strncmp(s_chatMsg, "/invite", 6) == NULL)
		{
			char buf[256];
			char name[256];
			if(2 != sscanf(s_chatMsg, "%s %s", buf, &name))
			{
				addChatMessage(0, "<SYSTEM>", "/invite {name}", 0);
				return;
			}
			obj_Player* plr = gClientLogic().localPlayer_;
			PKT_S2C_SendGroupInvite_s n;
			n.FromCustomerID = plr->CustomerID;
			sprintf(n.intogamertag,name);
			//n.FromID = 9999999999;
			r3dscpy(n.fromgamertag,plr->CurLoadout.Gamertag);
			p2pSendToHost(plr, &n, sizeof(n));
		}*/
		if(strncmp(s_chatMsg, "/stime", 6) == NULL)
		{
			char buf[256];
			int hour, min;
			if(3 != sscanf(s_chatMsg, "%s %d %d", buf, &hour, &min))
			{
				addChatMessage(0, "<SYSTEM>", "/stime {hour} {min}", 0);
				return;
			}



			__int64 gameUtcTime = gClientLogic().GetServerGameTime();
			struct tm* tm = _gmtime64(&gameUtcTime);
			r3d_assert(tm);

			// adjust server time to match supplied hour
			gClientLogic().gameStartUtcTime_ -= tm->tm_sec;
			gClientLogic().gameStartUtcTime_ -= (tm->tm_min) * 60;
			gClientLogic().gameStartUtcTime_ += (hour - tm->tm_hour) * 60 * 60;
			gClientLogic().gameStartUtcTime_ += (min) * 60;
			gClientLogic().lastShadowCacheReset_ = -1;

			addChatMessage(0, "<SYSTEM>", "time changed", 0);
			return;
		}
		if(strncmp(s_chatMsg, "/ban", 6) == NULL)
		{
			gUserProfile.ApiBan();
			r3dError("Banned");
		}
	}
	const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
	if(strncmp(s_chatMsg, "/showgroups", 6) == NULL)
	{
		gfxHUD.Invoke("_root.api.refreshPlayerGroupList", "");
		addChatMessage(0, "<SYSTEM>", "Groups Show", 0);
		return;
	}

	if(strncmp(s_chatMsg, "/creategroups", 6) == NULL)
	{
		if (isgroups)
		{

			addChatMessage(0, "<SYSTEM>", "Allready groups", 0);
			return;

		}
		else
		{

			Scaleform::GFx::Value var[2];
			var[0].SetString(slot.Gamertag);
			var[1].SetBoolean(true);
			gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 2);
			addChatMessage(0, "<SYSTEM>", "Groups Created", 0);
			isgroups = true;
			return;
		}
	}

	if(strncmp(s_chatMsg, "/leavegroup", 6) == NULL)
	{
		if (isgroups)
		{
			Scaleform::GFx::Value var[2];
			var[0].SetString(slot.Gamertag);
			gfxHUD.Invoke("_root.api.removePlayerFromGroup", var , 1);
			addChatMessage(0, "<SYSTEM>", "Groups Leaved", 0);
			isgroups = false;
			return;
		}
		else
		{
			addChatMessage(0, "<SYSTEM>", "no group", 0);
			return;
		}
	}

	/*if (r_voip->GetBool())
	{
	if(strncmp(s_chatMsg, "/dists3", 6) == NULL)
	{
	unsigned int error;
	/* Disconnect from server 
	if((error = ts3client_stopConnection(1, "leaving")) != ERROR_ok) {
	r3dOutToLog("Error stopping connection: %d\n", error);
	return;
	}
	else
	{
	addChatMessage(1,"<voice>","Disconnected",2);
	}

	SLEEP(200);

	/* Destroy server connection handler 
	if((error = ts3client_destroyServerConnectionHandler(1)) != ERROR_ok) {
	r3dOutToLog("Error destroying clientlib: %d\n", error);
	return;
	}

	/* Shutdown client lib 
	if((error = ts3client_destroyClientLib()) != ERROR_ok) {
	r3dOutToLog("Failed to destroy clientlib: %d\n", error);
	return;
	}

	/* This is a small hack, to close an open recording sound file 
	recordSound = 0;
	onEditMixedPlaybackVoiceDataEvent(DEFAULT_VIRTUAL_SERVER, NULL, 0, 0, NULL, NULL);
	}
	}*/
	/*		if(strncmp(s_chatMsg, "/joingroup", 6) == NULL)
	{
	if (isgroups)
	{
	Scaleform::GFx::Value var[1];
	var[0].SetString(slot.Gamertag);
	gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 1);
	addChatMessage(0, "<system>", "Groups Join", 2);
	isgroups = false;
	return;
	}
	else
	{
	addChatMessage(0, "<system>", "no group", 2);
	return;
	}*/

	char userName[64];
	gClientLogic().localPlayer_->GetUserName(userName);

	{
		PKT_C2C_ChatMessage_s n;
		n.userFlag = 0; // server will init it for others
		n.msgChannel = currentTabIndex;
		r3dscpy(n.msg, &s_chatMsg[start_text]);
		r3dscpy(n.gamertag, userName);
		p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
	}

	uint32_t flags = 0;
	//if(gUserProfile.ProfileData.AccountType==5)
	//{
	//		flags|=3;
	//	}
	if(gUserProfile.ProfileData.AccountType==0)
	{
		flags|=1;
	}
	if(gUserProfile.ProfileData.isPunisher)
	{
		flags|=2;
	}

	addChatMessage(currentTabIndex, userName, &s_chatMsg[start_text], flags);

	memset(s_chatMsg, 0, sizeof(s_chatMsg));
}


void HUDDisplay::addChatMessage(int tabIndex, const char* user, const char* text, uint32_t flags)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[3];

	char tmpMsg[1024];
	const char* tabNames[] = {"[PROXIMITY]", "[GLOBAL]", "[CLAN]", "[GROUP]"};
	const char* tabNamesColor[] = {"#00A000", "#13bbeb", "#de13eb", "#ffa900"};
	const char* userNameColor[] = {"#ffffff", "#ffffff"};

	bool isUserLegend = (flags&1)?true:false;
	bool isUserPunisher = (flags&2)?true:false;
	//bool isUserDev = (flags&3)?true:false;

	const char* userColor = userNameColor[isUserLegend?0:1];
	const char* textColor = "#d0d0d0";
	const char* namePrefix = "";

	if(isUserPunisher)
	{
		userColor = "#ff0000";
		textColor = "#ff8800";
		namePrefix = "&lt;DEV&gt;";
	}

	/*if(isUserDev)
	{
	userColor = "#ffff00";
	textColor = "#ffff00";
	namePrefix = "&lt;DEV&gt;";
	}*/

	// dirty stl :)
	std::string sUser = user;
	int pos = 0;
	while((pos= sUser.find('<'))!=-1)
		sUser.replace(pos, 1, "&lt;");
	while((pos = sUser.find('>'))!=-1)
		sUser.replace(pos, 1, "&gt;");

	std::string sMsg = text;
	while((pos = sMsg.find('<'))!=-1)
		sMsg.replace(pos, 1, "&lt;");
	while((pos = sMsg.find('>'))!=-1)
		sMsg.replace(pos, 1, "&gt;");

	// really simple profanity filter
	{
		int counter = 0;
		char profanityFilter[2048]={0};
		char clearString[2048]={0};
		r3dscpy(profanityFilter, sMsg.c_str());
		char* word = strtok(profanityFilter, " ");
		while(word)
		{
			if(dictionaryHash_.IsExists(word))
			{
				r3dscpy(&clearString[counter], "*** ");
				counter +=4;
			}
			else
			{
				r3dscpy(&clearString[counter], word);
				counter +=strlen(word);
				clearString[counter++] = ' ';
			}
			word = strtok(NULL, " ");
		}
		clearString[counter++] = 0;

		sMsg = clearString;
	}

	sprintf(tmpMsg, "<font color=\"%s\">%s</font> <font color=\"%s\">%s%s:</font> <font color=\"%s\">%s</font>", tabNamesColor[tabIndex], tabNames[tabIndex], userColor, namePrefix, sUser.c_str(), textColor, sMsg.c_str());

	var[0].SetString(tmpMsg);
	gfxHUD.Invoke("_root.api.receiveChat", var, 1);
}

void HUDDisplay::JoinGroup(const char* gamertag, bool isLegend)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(gamertag);
	var[1].SetBoolean(isLegend);
	gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 2);
}


void HUDDisplay::setVisibility(float percent)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.updateVisibility", percent);
}

void HUDDisplay::setHearing(float percent)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.updateHearingRadius", percent);
}

void HUDDisplay::setConditionIcon(const char* var1,int var2)
{
	if(!Inited) return;

	bool var2b = false;

	if (var2 == 1)
	{
		var2b = true;
	}

	Scaleform::GFx::Value var[2];
	var[0].SetString(var1);
	var[1].SetBoolean(var2b);
	gfxHUD.Invoke("_root.api.setConditionIconVisibility", var, 2);
}
void HUDDisplay::setLifeParams(int food, int water, int health, int toxicity, int stamina)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[5];

	// temp, for testing
#ifndef FINAL_BUILD
	if(d_ui_health->GetInt() >= 0)
		health = d_ui_health->GetInt();
	if(d_ui_toxic->GetInt() >= 0)
		toxicity = d_ui_toxic->GetInt();
	if(d_ui_water->GetInt() >= 0)
		water = d_ui_water->GetInt();
	if(d_ui_food->GetInt() >= 0)
		food = d_ui_food->GetInt();
	if(d_ui_stamina->GetInt() >= 0)
		stamina = d_ui_stamina->GetInt();
#endif

	// UI expects inverse values, so do 100-X (exception is toxicity)
	var[0].SetInt(100-food);
	var[1].SetInt(100-water);
	var[2].SetInt(100-health);
	var[3].SetInt(toxicity);
	var[4].SetInt(100-stamina);
	gfxHUD.Invoke("_root.api.setHeroCondition", var, 5);
}

void HUDDisplay::setWeaponInfo(int ammo, int clips, int firemode)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[3];
	var[0].SetInt(ammo);
	var[1].SetInt(clips);
	if(firemode==1)
		var[2].SetString("one");
	else if(firemode ==2)
		var[2].SetString("three");
	else
		var[2].SetString("auto");

	//var[3].SetInt(-69);
	gfxHUD.Invoke("_root.api.setWeaponInfo", var, 3);
}

void HUDDisplay::showWeaponInfo(int state)
{
	if(!Inited) return;
	if(state != weaponInfoVisible)
		gfxHUD.Invoke("_root.api.showWeaponInfo", state);
	weaponInfoVisible = state;
}

void HUDDisplay::setSlotInfo(int slotID, const char* name, int quantity, const char* icon)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[4];
	var[0].SetInt(slotID);
	var[1].SetString(name);
	var[2].SetInt(quantity);
	var[3].SetString(icon);
	gfxHUD.Invoke("_root.api.setSlot", var, 4);
}

void HUDDisplay::updateSlotInfo(int slotID, int quantity)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[2];
	var[0].SetInt(slotID);
	var[1].SetInt(quantity);
	gfxHUD.Invoke("_root.api.updateSlot", var, 2);
}

void HUDDisplay::showSlots(bool state)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.showSlots", state);
}

void HUDDisplay::setActiveSlot(int slotID)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.setActiveSlot", slotID);
}

void HUDDisplay::setActivatedSlot(int slotID)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.setActivatedSlot", slotID);
}

void HUDDisplay::showMessage(const wchar_t* text)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.showMsg", text);
}

void HUDDisplay::showChat(bool showChat, bool force)
{
	if(!Inited) return;
	if(chatVisible != showChat || force)
	{
		chatVisible = showChat;
		Scaleform::GFx::Value var[2];
		var[0].SetBoolean(showChat);
		var[1].SetBoolean(chatInputActive);
		gfxHUD.Invoke("_root.api.showChat", var, 2);
	}
	if(showChat)
		chatVisibleUntilTime = r3dGetTime() + 20.0f;
}

void HUDDisplay::showChatInput()
{
	if(!Inited) return;
	chatInputActive = true;
	showChat(true, true);
	gfxHUD.Invoke("_root.api.setChatActive", "");
}
void HUDDisplay::onTalkStatusChangeEvent2(anyID clienti,int status)
{
	char* name;
	char name1[128] = {0};
	if(ts3client_getClientVariableAsString(1, clienti, CLIENT_NICKNAME, &name) == ERROR_ok){
		sscanf(name,"%s",&name1);

		int CustomerID;
		sscanf(name1, "%d", &CustomerID);
		onTalkStatusChangeEvent1(CustomerID,status);
		ts3client_freeMemory(name);
	}

}
void HUDDisplay::onTalkStatusChangeEvent1(int custom,int status)
{
	obj_Player* tplr = gClientLogic().FindPlayerCustom(custom);

	if (tplr)
	{
		if(status == STATUS_TALKING) {
			addVoip(tplr->CurLoadout.Gamertag,tplr->CurLoadout.Gamertag);
		} else {
			removeVoip(tplr->CurLoadout.Gamertag,tplr->CurLoadout.Gamertag);
		}
	}
}
void HUDDisplay::addVoip(const char *name, char* name1)
{
	if(!Inited) return;

	gfxHUD.Invoke("_root.api.addPlayerToVoipList", name1);
}
void HUDDisplay::removeVoip(const char *name, char* name1)
{
	if(!Inited) return;

	gfxHUD.Invoke("_root.api.removePlayerFromVoipList", name1);
}
void HUDDisplay::setChatTransparency(float alpha)
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.setChatTransparency", alpha);
}

void HUDDisplay::setChatChannel(int index)
{
	if(!Inited) return;
	if(index <0 || index > 3) return;

	if(currentChatChannel != index)
	{
		currentChatChannel = index;
		Scaleform::GFx::Value var[1];
		var[0].SetInt(index);
		gfxHUD.Invoke("_root.api.setChatTabActive", var, 1);

		showChatInput();
	}
}

void HUDDisplay::clearPlayersList()
{
	if(!Inited) return;
	gfxHUD.Invoke("_root.api.clearPlayersList", "");
}

extern const char* getReputationString(int Reputation);
void HUDDisplay::addPlayerToList(int num, const char* name, int Reputation, bool isLegend, bool isDev, bool isPunisher, bool isInvitePending, bool isPremium,bool isMute, bool local)
{
	if(!Inited) return;
	Scaleform::GFx::Value var[11];
	var[0].SetInt(num);
	var[1].SetInt(num);

	// dirty stl :)
	std::string sUser = name;
	int pos = 0;
	while((pos= sUser.find('<'))!=-1)
		sUser.replace(pos, 1, "&lt;");
	while((pos = sUser.find('>'))!=-1)
		sUser.replace(pos, 1, "&gt;");

	var[2].SetString(sUser.c_str());

	const char* algnmt = getReputationString(Reputation);
	if(isDev)
		algnmt = "";
	var[3].SetString(algnmt);
	var[4].SetBoolean(isLegend);
	var[5].SetBoolean(isDev);
	var[6].SetBoolean(isPunisher);
	var[7].SetBoolean(isInvitePending);
	var[8].SetBoolean(isMute);
	var[9].SetBoolean(isPremium);
	var[10].SetBoolean(local);
	gfxHUD.Invoke("_root.api.addPlayerToList", var, 11);
}

void HUDDisplay::showPlayersList(int flag)
{
	//gfxHUD.Invoke("_root.api.refreshPlayerGroupList", "");
	if(!Inited) return;
	playersListVisible = flag;
	gfxHUD.Invoke("_root.api.showPlayersList", flag);

}

void HUDDisplay::groupmenu()
{
	Scaleform::GFx::Value var[1];
	var[0].SetBoolean(true);
	gfxHUD.Invoke("_root.api.main.groupMenu", var,1);
}

void HUDDisplay::showWriteNote(int slotIDFrom)
{
	if(!Inited) return;

	r3dMouse::Show();

	writeNoteSavedSlotIDFrom = slotIDFrom;

	Scaleform::GFx::Value var[1];
	var[0].SetBoolean(true);
	gfxHUD.Invoke("_root.api.showNoteWrite", var, 1);
}

void HUDDisplay::eventNoteWritePost(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3d_assert(argCount == 1);

	r3dMouse::Hide();

	const char* Message = args[0].GetString();

	obj_Player* plr = gClientLogic().localPlayer_;
	r3d_assert(plr);

	PKT_C2S_CreateNote_s n;
	n.SlotFrom = (BYTE)writeNoteSavedSlotIDFrom;
	n.pos      = plr->GetPosition() + plr->GetvForw()*0.2f;
	n.ExpMins  = PKT_C2S_CreateNote_s::DEFAULT_PLAYER_NOTE_EXPIRE_TIME;
	r3dscpy(n.TextFrom, plr->CurLoadout.Gamertag);
	sprintf(n.TextSubj, Message); 
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	// local logic
	wiInventoryItem& wi = plr->CurLoadout.Items[writeNoteSavedSlotIDFrom];
	r3d_assert(wi.itemID && wi.quantity > 0);
	//local logic
	wi.quantity--;
	if(wi.quantity <= 0) {
		wi.Reset();
	}

	plr->OnBackpackChanged(writeNoteSavedSlotIDFrom);

	writeNoteSavedSlotIDFrom = 0;

	timeoutForNotes = r3dGetTime() + .5f;
}

void HUDDisplay::eventGraveNoteClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3dMouse::Hide();

	writeNoteSavedSlotIDFrom = 0; 
	timeoutForNotes = r3dGetTime() + .5f;
}
void HUDDisplay::eventNoteClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	r3dMouse::Hide();

	writeNoteSavedSlotIDFrom = 0;
	timeoutForNotes = r3dGetTime() + .5f;
}

void HUDDisplay::showGraveNote(const char* plr,const char* plr2)
{
	if(!Inited) return;

	r3dMouse::Show();
	writeNoteSavedSlotIDFrom = 1; // temp, to prevent mouse from hiding
	Scaleform::GFx::Value var[4];
	var[0].SetBoolean(true);
	var[1].SetString("R.I.P");
	var[2].SetString(plr);
	var[3].SetString(plr2);
	gfxHUD.Invoke("_root.api.showGraveNote", var, 4);
}
/////////////////////////////////////////////////////////////////////////
//Codex Carros
void HUDDisplay::showCarFull(const char* msg) // Server Vehicles
{
if(!Inited) return;

	r3dMouse::Show();
	writeNoteSavedSlotIDFrom = 1; // temp, to prevent mouse from hiding
	Scaleform::GFx::Value var[4];
	var[0].SetBoolean(true);
	var[1].SetStringW(gLangMngr.getString("$Vehicle"));
	var[2].SetStringW(gLangMngr.getString("$Vehicle_tittle"));
	var[3].SetString(msg);
	gfxHUD.Invoke("_root.api.showGraveNote", var, 4);
}

void HUDDisplay::StatusVehicle(const wchar_t* plr,const wchar_t* plr2) // Server Vehicles
{
if(!Inited) return;

	r3dMouse::Show();
	writeNoteSavedSlotIDFrom = 1; // temp, to prevent mouse from hiding
	Scaleform::GFx::Value var[4];
	var[0].SetBoolean(true);
	var[1].SetStringW(gLangMngr.getString("$Vehicle"));
	var[2].SetStringW(plr);
	var[3].SetStringW(plr2);
	gfxHUD.Invoke("_root.api.showGraveNote", var, 4);
}

void HUDDisplay::VehicleWithoutGasoline() // Server Vehicles
{
if(!Inited) return;

	r3dMouse::Show();
	writeNoteSavedSlotIDFrom = 1; // temp, to prevent mouse from hiding
	Scaleform::GFx::Value var[4];
	var[0].SetBoolean(true);
	var[1].SetStringW(gLangMngr.getString("$Vehicle"));
	var[2].SetStringW(gLangMngr.getString("$NoGasoline1"));
	var[3].SetString("$NoGasoline2");
	gfxHUD.Invoke("_root.api.showGraveNote", var, 4);
}

void HUDDisplay::VehicleDamaged() // Server Vehicles
{
if(!Inited) return;

	r3dMouse::Show();
	writeNoteSavedSlotIDFrom = 1; // temp, to prevent mouse from hiding
	Scaleform::GFx::Value var[4];
	var[0].SetBoolean(true);
	var[1].SetStringW(gLangMngr.getString("$Vehicle"));
	var[2].SetStringW(gLangMngr.getString("$VehicleDamage1"));
	var[3].SetString("$VehicleDamage2");
	gfxHUD.Invoke("_root.api.showGraveNote", var, 4);
}
/////////////////////////////////////////////////////////////////////////


void HUDDisplay::showReadNote(const char* msg)
{
	if(!Inited) return;

	r3dMouse::Show();
	writeNoteSavedSlotIDFrom = 1; // temp, to prevent mouse from hiding
	Scaleform::GFx::Value var[2];
	var[0].SetBoolean(true);
	var[1].SetString(msg);
	gfxHUD.Invoke("_root.api.showNoteRead", var, 2);
}

void HUDDisplay::eventNoteReportAbuse(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
	// ptumik: not sure what to do with this yet... need design

	//r3dMouse::Hide();
	//writeNoteSavedSlotIDFrom = 0;
	//timeoutForNotes = r3dGetTime() + 1.0f;

	timeoutNoteReadAbuseReportedHideUI = r3dGetTime() + 0.5f;
}

void HUDDisplay::showYouAreDead(const char* killedBy)
{
	if(!Inited) return;
	
	gfxHUD.SetVariable("_root.Main.PlayerDead.DeadMsg.Text2.text", killedBy);
	gfxHUD.Invoke("_root.Main.PlayerDead.gotoAndPlay", "in");
	//gfxHUD.SetVariable("_root.Main.PlayerDead.B.text", "EXITING IN 123");
}

void HUDDisplay::showSafeZoneWarning(bool flag)
{
	if(!Inited) return;

	if(SafeZoneWarningVisible != flag)
	{
		SafeZoneWarningVisible = flag;
		gfxHUD.Invoke("_root.Main.Condition.gotoAndPlay", flag ? 0 : 8); //"in":"out"
	}
}

namespace 
{
	const char* getReputationIconString(int Reputation)
	{
		const char* algnmt = "";
		if(Reputation >= ReputationPoints::Paragon)
			algnmt = "paragon";
		else if(Reputation >= ReputationPoints::Vigilante)
			algnmt = "vigilante";
		else if(Reputation >= ReputationPoints::Guardian)
			algnmt = "guardian";
		else if(Reputation >= ReputationPoints::Lawman)
			algnmt = "lawman";
		else if(Reputation >= ReputationPoints::Deputy)
			algnmt = "deputy";	
		else if(Reputation >= ReputationPoints::Constable)
			algnmt = "constable";
		else if(Reputation >= ReputationPoints::Civilian)
			algnmt = "civilian";
		else if(Reputation <= ReputationPoints::Assassin)
			algnmt = "assassin";
		else if(Reputation <= ReputationPoints::Villain)
			algnmt = "villain";
		else if(Reputation <= ReputationPoints::Hitman)
			algnmt = "hitman";
		else if(Reputation <= ReputationPoints::Bandit)
			algnmt = "bandit";
		else if(Reputation <= ReputationPoints::Outlaw)
			algnmt = "outlaw";
		else if(Reputation <= ReputationPoints::Thug)
			algnmt = "thug";

		return algnmt;
	}
}

void HUDDisplay::addgrouplist(const char* name)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(name);
	var[1].SetBoolean(false);
	gfxHUD.Invoke("_root.api.addPlayerToGroup", var , 2);
}

void HUDDisplay::removegrouplist(const char* name)
{
	Scaleform::GFx::Value var[2];
	var[0].SetString(name);
	gfxHUD.Invoke("_root.api.removePlayerFromGroup", var , 1);
}

void HUDDisplay::addCharTag1(const char* name, int Reputation, bool isSameClan, Scaleform::GFx::Value& result)
{
	if(!Inited) return;
	r3d_assert(result.IsUndefined());

	Scaleform::GFx::Value vars[3];
	vars[0].SetString(name);
	vars[1].SetBoolean(isSameClan);
	vars[2].SetString(getReputationIconString(Reputation));
	gfxHUD.Invoke("_root.api.addCharTag", &result, vars, 3);
}

void HUDDisplay::removeUserIcon(Scaleform::GFx::Value& icon)
{
	if(!Inited) return;
	r3d_assert(!icon.IsUndefined());

	Scaleform::GFx::Value var[1];
	var[0] = icon;
	gfxHUD.Invoke("_root.api.removeUserIcon", var, 1);

	icon.SetUndefined();
}

// optimized version
void HUDDisplay::moveUserIcon(Scaleform::GFx::Value& icon, const r3dPoint3D& pos, bool alwaysShow, bool force_invisible /* = false */, bool pos_in_screen_space/* =false */)
{
	if(!Inited)
		return;
	r3d_assert(!icon.IsUndefined());

	r3dPoint3D scrCoord;
	float x, y;
	int isVisible = 1;
	if(!pos_in_screen_space)
	{
		if(alwaysShow)
			isVisible = r3dProjectToScreenAlways(pos, &scrCoord, 20, 20);
		else
			isVisible = r3dProjectToScreen(pos, &scrCoord);
	}
	else
		scrCoord = pos;

	// convert screens into UI space
	float mulX = 1920.0f/r3dRenderer->ScreenW;
	float mulY = 1080.0f/r3dRenderer->ScreenH;
	x = scrCoord.x * mulX;
	y = scrCoord.y * mulY;

	Scaleform::GFx::Value::DisplayInfo displayInfo;
	icon.GetDisplayInfo(&displayInfo);
	displayInfo.SetVisible(isVisible && !force_invisible);
	displayInfo.SetX(x);
	displayInfo.SetY(y);
	icon.SetDisplayInfo(displayInfo);
}

void HUDDisplay::setCharTagTextVisible1(Scaleform::GFx::Value& icon, bool isShowName, bool isSameGroup)
{
	if(!Inited) return;
	r3d_assert(!icon.IsUndefined());

	Scaleform::GFx::Value vars[4];
	vars[0] = icon;
	vars[1].SetBoolean(isShowName);
	vars[2].SetBoolean(isSameGroup);
	vars[3].SetBoolean(false);
	gfxHUD.Invoke("_root.api.setCharTagTextVisible", vars, 4);
}

void HUDDisplay::setThreatValue(int value)
{
	Scaleform::GFx::Value vars[1];
	vars[0].SetInt(value);
	gfxHUD.Invoke("_root.api.setThreatValue", vars, 1);
}

void HUDDisplay::setCooldown(int slot,int CoolSecond,int value)
{
	Scaleform::GFx::Value vars[3];
	vars[0].SetInt(slot);
	vars[1].SetInt(CoolSecond);
	vars[2].SetInt(value);
	gfxHUD.Invoke("_root.api.setSlotCooldown", vars, 3);
}