#include "r3dPCH.h"
#include "r3d.h"

#include "r3dBackgroundTaskDispatcher.h"
#include <ShellAPI.h>
#include <psapi.h>
#pragma comment(lib,"Psapi.lib")
#pragma comment(lib,"ts3client_win32.lib")
#pragma comment(lib,"ts3server_win32.lib")
#include "d3dfont.h"
#include "GameCommon.h"
#include "Gameplay_Params.h"

#include "UI\HUD_TPSGame.h"
#include "ObjectsCode/AI/AI_Player.h"
#include "ObjectsCode/AI/AI_PlayerAnim.h"
#include "ObjectsCode/Gameplay/BasePlayerSpawnPoint.h"
#include "ObjectsCode/weapons/WeaponArmory.h"

#include "APIScaleformGfx.h"
#include "../VoIP/public_definitions.h"
#include "../VoIP/public_errors.h"
#include "../VoIP/clientlib_publicdefinitions.h"
#include "../VoIP/clientlib.h"
#include "../VoIP/serverlib_publicdefinitions.h"
#include "../VoIP/serverlib.h"
#include "multiplayer/ClientGameLogic.h"

#include "HUDCameraEffects.h"

#include "UI\HUDDisplay.h"
#include "UI\HUDPause.h"
#include "UI\HUDAttachments.h"
#include "UI\HUDActionUI.h"
#include "UI\HUDGeneralStore.h"
#include "UI\HUDVault.h"
#include "UI\HUDCraft.h"//Codex Craft
#include "UI\HUDTrade.h"
#include "UI\HUDRepair.h"

#include "..\GameEngine\gameobjects\obj_Vehicle.h"//Codex Carros

#include "rendering/Deffered/D3DMiscFunctions.h"

extern float GameFOV;
int externalcam; // Server Vehicles //Codex Carros

bool isDi;

float swimt;

HUDDisplay*	hudMain = NULL;
HUDPause*	hudPause = NULL;
HUDAttachments*	hudAttm = NULL;
HUDActionUI*	hudActionUI = NULL;
HUDGeneralStore* hudGeneralStore = NULL;
HUDVault* hudVault = NULL;
HUDTrade* hudTrade = NULL;
HUDCraft* hudCraft = NULL;//Codex Craft
HUDRepair* hudRepair = NULL;

#define VEHICLE_CINEMATIC_MODE 0 //Codex Carros

static float LastHSLog;
static uint64 scHandlerID;
static uint64 svHandlerID;
char ip[64];

void InitTs3Server()
{
	ts3server_createVirtualServer(9987, "127.0.0.1", "voipserver", "", 32, &svHandlerID);
	ts3server_setVirtualServerVariableAsString(svHandlerID, VIRTUALSERVER_PASSWORD, "voippassword");
	ts3server_flushVirtualServerVariable(svHandlerID);
}

void TPSGameHUD_AddHitEffect(GameObject* from)
{
	obj_Player* pl = gClientLogic().localPlayer_;
	if(!pl) return;
	if(pl->bDead) return;

	pl->BloodEffect = 3.0f;
}



TPSGameHUD::TPSGameHUD()
{
	FPS_Acceleration.Assign(0,0,0);
	FPS_vViewOrig.Assign(0,0,0);
	FPS_ViewAngle.Assign(0,0,0);
	FPS_vVision.Assign(0,0,0);
	FPS_vRight.Assign(0,0,0);
	FPS_vUp.Assign(0,0,0);
	FPS_vForw.Assign(0,0,0);
	cameraRayLen = 20000.0f;
}

TPSGameHUD::~TPSGameHUD()
{
}

static bool TPSGameHud_Inited;
void UpdateObjects()
{
	ObjectManager& GW = GameWorld();
	for (GameObject *targetObj = GW.GetFirstObject(); targetObj; targetObj = GW.GetNextObject(targetObj))
	{
		if (targetObj->isObjType(OBJTYPE_Building))
		{
			r3dOutToLog("Building %p\n",targetObj);
			if ((gCam - targetObj->GetPosition()).Length() > 201.0f) // if building far 500m. Destroy it.
			{
				r3dOutToLog("Delete Building %p\n",targetObj);
				targetObj->setActiveFlag(0);
				targetObj->OnDestroy();
				SAFE_DELETE(targetObj);
				targetObj = NULL;
			}
		}
	}
}
void UpdateTs3()
{
	// this is fucking and hard code..
	if (r_voip->GetBool())
	{
		bool talking;
		if (!hudMain->isChatInputActive())
		{
			talking = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_PUSH_TO_TALK);
		}
		else
		{
			talking = false;
		}
		if (r_voipShowBall->GetBool())
		{
			if (talking && !gClientLogic().localPlayer_->CurLoadout.isVoipShow)
			{
				char plrUserName[64]; gClientLogic().localPlayer_->GetUserName(plrUserName);
				gClientLogic().localPlayer_->CurLoadout.isVoipShow = true;
				hudMain->addVoip(plrUserName,plrUserName);
			}
			else if (!talking && gClientLogic().localPlayer_->CurLoadout.isVoipShow)
			{
				char plrUserName[64]; gClientLogic().localPlayer_->GetUserName(plrUserName);
				gClientLogic().localPlayer_->CurLoadout.isVoipShow = false;
				hudMain->removeVoip(plrUserName,plrUserName);
			}
		}
		ts3client_setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED,  talking ? INPUT_ACTIVE : INPUT_DEACTIVATED);
		ts3client_flushClientSelfUpdates(scHandlerID, NULL);
		unsigned int error;
		for(int i=0; i<128; i++)
		{
			char* name123;
			if(ts3client_getClientVariableAsString(scHandlerID, i, CLIENT_NICKNAME, &name123) != ERROR_ok)
			{

			}
			else
			{
				int CustomerID;
				int talkStatus;
				sscanf(name123, "%d", &CustomerID);
				ts3client_freeMemory(name123);
				obj_Player* tplr = gClientLogic().FindPlayerCustom(CustomerID);
				obj_Player* plr = gClientLogic().localPlayer_;
				if (tplr)
				{
					if (r_voipShowBall->GetBool())
					{
						ts3client_getClientVariableAsInt(scHandlerID, i, CLIENT_FLAG_TALKING, &talkStatus);
						if (talkStatus == 1 && !tplr->CurLoadout.isVoipShow && tplr != gClientLogic().localPlayer_)
						{
							tplr->CurLoadout.isVoipShow = true;
							char plrUserName[64]; tplr->GetUserName(plrUserName);
							hudMain->addVoip(plrUserName,plrUserName);
						}
						else if (talkStatus == 0 && tplr->CurLoadout.isVoipShow && tplr != gClientLogic().localPlayer_)
						{
							tplr->CurLoadout.isVoipShow = false;
							char plrUserName[64]; tplr->GetUserName(plrUserName);
							hudMain->removeVoip(plrUserName,plrUserName);
						}

						float dist = (gCam - tplr->GetPosition()).Length();
						if (dist > 25.0f && tplr->CurLoadout.isVoipShow && tplr != gClientLogic().localPlayer_)
						{
							tplr->CurLoadout.isVoipShow = false;
							char plrUserName[64]; tplr->GetUserName(plrUserName);
							hudMain->removeVoip(plrUserName,plrUserName);
						}
					}
					ts3client_setClientVolumeModifier(scHandlerID, i, 5.0f); // Set Client Volume + 3.0db
					float distx = (gCam - tplr->GetPosition()).x;
					float disty = (gCam - tplr->GetPosition()).y;
					float distz = (gCam - tplr->GetPosition()).z;

					TS3_VECTOR position;
					position.x = distx;
					position.y = disty;
					position.z = distz;

					if((error = ts3client_channelset3DAttributes(scHandlerID,i,&position)) != ERROR_ok) {

					}

				}
				else
				{
					TS3_VECTOR position;
					position.x = -5000.0f;
					position.y = -5000.0f;
					position.z = -5000.0f;
					if((error = ts3client_channelset3DAttributes(scHandlerID,i,&position)) != ERROR_ok) {
					}
				}
			}
		}
	}
}
void onUserLoggingMessageEvent(const char* logMessage, int logLevel, const char* logChannel, uint64 logID, const char* logTime, const char* completeLogString) {
	r3dOutToLog("%s\n",completeLogString);
}
void onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
	//r3dOutToLog("Status %d\n",newStatus);
	if(newStatus == STATUS_CONNECTION_ESTABLISHED)
	{
		hudMain->addChatMessage(0,"<voice>","Connected to voice server",2);
	}

	if(newStatus == STATUS_DISCONNECTED && errorNumber == ERROR_failed_connection_initialisation) {
		hudMain->addChatMessage(0,"<voice>","Connect Failed",2);
	}
}
int readIdentity(char* identity) {
	FILE *file;

	if((file = fopen("identity.txt", "r")) == NULL) {
		printf("Could not open file 'identity.txt' for reading.\n");
		return -1;
	}

	fgets(identity, 10000, file);
	if(ferror(file) != 0) {
		fclose (file);
		printf("Error reading identity from file 'identity.txt'.\n");
		return -1;
	}
	fclose (file);
	return 0;
}
int writeIdentity(const char* identity) {
	FILE *file;

	if((file = fopen("identity.txt", "w")) == NULL) {
		printf("Could not open file 'identity.txt' for writing.\n");
		return -1;
	}

	fputs(identity, file);
	if(ferror(file) != 0) {
		fclose (file);
		printf("Error writing identity to file 'identity.txt'.\n");
		return -1;
	}
	fclose (file);
	return 0;
}
void InitTs3()
{
	char identity[10000];
	if (r_voip->GetBool())
	{ 

		ts3client_spawnNewServerConnectionHandler(0,&scHandlerID);
		if(readIdentity(identity) != 0) {
			char* id;
			ts3client_createIdentity(&id);
			if(strlen(id) >= 10000) {
				printf("Not enough bufsize for identity string\n");
			}
			strcpy(identity, id);
			ts3client_freeMemory(id);
			writeIdentity(identity);
		}

		//r3dOutToLog("Using identity: %s\n", identity);

		char plr3msg[128] = {0};
		sprintf(plr3msg,"%d",gUserProfile.CustomerID);
		ts3client_startConnection(scHandlerID, identity, "127.0.0.1", 9987,plr3msg, NULL, "", "voippassword");

		::Sleep(3000);
		int status;
		ts3client_getConnectionStatus(scHandlerID,&status);
		if (status == STATUS_CONNECTION_ESTABLISHED)
			hudMain->addChatMessage(0,"<voice>","Connected to voice server",1);
		else // failed
		{
			hudMain->addChatMessage(0,"<voice>","Disconnected from voice server",1);
			// destroy connection and return it
			//ts3client_stopConnection(1, "leaving");
			ts3client_destroyServerConnectionHandler(scHandlerID);
			return;
		}
		char* mode;
		char* mode2;
		ts3client_getDefaultPlayBackMode(&mode);

		ts3client_getDefaultCaptureMode(&mode2);
		char*** device;
		char*** device2;
		ts3client_getPlaybackDeviceList(mode,&device);
		ts3client_getCaptureDeviceList(mode2,&device2);
		// Fix Crash because not have a input / output device
		//ts3client_closePlaybackDevice(scHandlerID);
		//ts3client_closeCaptureDevice(scHandlerID);
		if (device[r_voipPlayBack->GetInt()] != NULL)
		{

			char *devicee = device[r_voipPlayBack->GetInt()][1];
			ts3client_openPlaybackDevice(scHandlerID, mode, devicee);
			ts3client_freeMemory(device);
			ts3client_freeMemory(mode);
		}
		if (device2[r_voipCapture->GetInt()] != NULL)
		{

			char *devicee2 = device2[r_voipCapture->GetInt()][1];
			ts3client_openCaptureDevice(scHandlerID, mode2, devicee2);
			ts3client_freeMemory(device2);
			ts3client_freeMemory(mode2);
		}
	}
}
void TPSGameHUD_OnStartGame()
{
	hs_callback->SetInt(0);
	hs_pName->SetString("");
	LastHSLog = r3dGetTime();
	/*//AHNHS_PROTECT_FUNCTION

	if ( != HS_ERR_OK)
	{
	gClientLogic().ischeat = 1;
	sprintf(gClientLogic().cheatmsg,"Disconnect From Server. HackShield Service Start Failed.");
	gClientLogic().Disconnect();
	//_AhnHS_StopService();
	return;
	}*/

	const GBGameInfo& ginfo = gClientLogic().m_gameInfo;
	hudMain = new HUDDisplay();
	hudPause = new HUDPause();
	hudActionUI = new HUDActionUI();
	hudAttm = new HUDAttachments();
	hudGeneralStore = new HUDGeneralStore();
	hudVault = new HUDVault();
	hudCraft = new HUDCraft();//Codex Craft
	hudTrade = new HUDTrade();
	hudRepair = new HUDRepair();

	isDi = false;
	swimt = 0.0f;
	hudMain->Init();
	hudPause->Init();
	hudActionUI->Init();
	hudAttm->Init();
	hudGeneralStore->Init();
	hudVault->Init();
	hudCraft->Init();//Codex Craft
	hudTrade->Init();
	hudRepair->Init();
#ifdef VOIP_ENABLED
	//InitTs3();
	//InitTs3Server();
#endif

	Mouse->Hide(true);
	// lock mouse to a window when playing a game
	d_mouse_window_lock->SetBool(true);
	Mouse->SetRange(r3dRenderer->HLibWin);


	extern int g_CCBlackWhite;
	extern float g_fCCBlackWhitePwr;
	g_CCBlackWhite = false;
	g_fCCBlackWhitePwr = 0.0f;

	TPSGameHud_Inited = true;

	//hudMain->addChatMessage(0,"<voice>","Connected to voice server",1);
}

void TPSGameHUD :: DestroyPure()
{
#ifdef VOIP_ENABLED
	if (r_voip->GetBool())
	{
		ts3client_closePlaybackDevice(r_voipPlayBack->GetInt());
		ts3client_closeCaptureDevice(r_voipCapture->GetInt());
		ts3client_stopConnection(scHandlerID, "leaving");
		ts3client_destroyServerConnectionHandler(scHandlerID);

		ts3server_stopVirtualServer(svHandlerID);
	}
	//ts3client_destroyClientLib();
#endif

	//_AhnHS_StopService();
	if(TPSGameHud_Inited)
	{
		TPSGameHud_Inited = false;

		hudPause->Unload();
		hudMain->Unload();
		hudActionUI->Unload();
		hudAttm->Unload();
		hudGeneralStore->Unload();
		hudVault->Unload();
		hudCraft->Unload();//Codex Craft
		hudTrade->Unload();
		hudRepair->Unload();

		SAFE_DELETE(hudMain);
		SAFE_DELETE(hudPause);
		SAFE_DELETE(hudActionUI);
		SAFE_DELETE(hudAttm);
		SAFE_DELETE(hudGeneralStore);
		SAFE_DELETE(hudVault);
		SAFE_DELETE(hudTrade);
		SAFE_DELETE(hudRepair);
		SAFE_DELETE(hudCraft);//Codex Craft
	}
}

void TPSGameHUD :: SetCameraDir (r3dPoint3D vPos )
{

}

r3dPoint3D TPSGameHUD :: GetCameraDir () const
{
	return r3dVector(1,0,0);
}


extern	PlayerStateVars_s TPSHudCameras[3][PLAYER_NUM_STATES];
extern	Playerstate_e ActiveCameraRigID;
extern	PlayerStateVars_s ActiveCameraRig;

extern 	Playerstate_e CurrentState;
extern 	PlayerStateVars_s CurrentRig;
extern 	PlayerStateVars_s SourceRig;
extern 	PlayerStateVars_s TargetRig;
extern 	float LerpValue;
extern	r3dPoint3D TPSHudCameraTarget;

extern  float	TPSCameraPointToAdj[3];
extern  float   TPSCameraPointToAdjCrouch[3];

void TPSGameHUD :: InitPure()
{
	// reinit hud rigs based on camera mode
	CurrentRig = TPSHudCameras[g_camera_mode->GetInt()][PLAYER_IDLE];
	SourceRig  = CurrentRig;
	TargetRig  = CurrentRig;
}


// camPos = current camera pos. target = player's head pos
bool CheckCameraCollision(r3dPoint3D& camPos, const r3dPoint3D& target, bool checkCamera)
{
	R3DPROFILE_FUNCTION("CheckCameraCollision");

	r3dPoint3D origCamPos = camPos;
	int LoopBreaker = 0;

	r3dPoint3D motion = (camPos - target);
	float motionLen = motion.Length();
	int MaxLoopBreaker = 10;
	if(motionLen > 0.1f)
	{
		motion.Normalize();
		MaxLoopBreaker = int(ceilf(motionLen/0.05f));

		PxSphereGeometry camSphere(0.3f);
		PxTransform camPose(PxVec3(target.x, target.y, target.z), PxQuat(0,0,0,1));

		PxSweepHit sweepResults[32];
		bool blockingHit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_PLAYER_COLLIDABLE_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
		while(int numRes=g_pPhysicsWorld->PhysXScene->sweepMultiple(camSphere, camPose, PxVec3(motion.x, motion.y, motion.z), motionLen, PxSceneQueryFlag::eINITIAL_OVERLAP|PxSceneQueryFlag::eNORMAL, sweepResults, 32, blockingHit, filter) && LoopBreaker<MaxLoopBreaker)
		{
			if(numRes == -1)
			{
				r3d_assert(false);
				break;
			}
			/* PxVec3 collNormal = PxVec3(0,0,0);
			for(int i=0; i<numRes; ++i)
			{
			collNormal += sweepResults[i].normal;
			}

			collNormal.normalize();*/

			//r3dPoint3D tmp(collNormal.x, collNormal.y, collNormal.z);
			r3dPoint3D tmp = -motion;
			tmp.Normalize();
			camPos += tmp * 0.05f;

			LoopBreaker++;

			motion = (camPos - target);
			motionLen = motion.Length();
			if(motionLen < 0.005f)
			{
				LoopBreaker = MaxLoopBreaker;
				break;
			}
			motion.Normalize();
		}
	}

	if(checkCamera)
	{
		extern bool g_CameraInsidePlayer;
		if((camPos - target).Length() < 0.6f)
			g_CameraInsidePlayer = true;
		else
			g_CameraInsidePlayer = false;

		if(g_camera_mode->GetInt()==2) // in FPS mode this check not needed
			g_CameraInsidePlayer = false;
	}

	return (LoopBreaker == MaxLoopBreaker);
}

float g_shootCameraShakeTimer = 0.0f;
void Get_Camera_Bob(r3dPoint3D& camBob, r3dPoint3D& camUp, const obj_Player* player)
{
        r3d_assert(player);
        camBob.Assign(0,0,0);
        camUp.Assign(0,1,0);
 
        static float accumul = 0.0f;
        accumul += r3dGetFrameTime()*1.0f*u_GetRandom(0.75f, 1.25f);
 
       
        // only use this in FPS, but calculate out here.
 
        float wave = r3dSin(accumul) * g_shootCameraShakeTimer;
        if(g_shootCameraShakeTimer>0)
        {
                g_shootCameraShakeTimer = R3D_MAX(g_shootCameraShakeTimer-r3dGetFrameTime()*3.f, 0.0f);
        }
 
        if(g_camera_mode->GetInt()==2)
        {
                r3dPoint3D up(0,1,0);
                r3dPoint3D rightVector = player->m_vVision.Cross( up );
                up.RotateAroundVector(rightVector, wave*20.0f );
 
                camUp = up;
               
                float mStepDist = 0.1f;
                r3dPoint3D mCurPos = player->GetPosition();
                mCurPos -= player->oldstate.Position;
                float len = mCurPos.Length();
 
                //BP ok, make step distance the lenth of the anim
                // then len, is the current frame converted to percent :)
                std::vector<r3dAnimation::r3dAnimInfo>::iterator it;
                float curframe = 0;
                float numframe = 0;
                for(it = player->uberAnim_->anim.AnimTracks.begin(); it != player->uberAnim_->anim.AnimTracks.end(); ++it)
                {
                        r3dAnimation::r3dAnimInfo &ai = *it;
                        if(!(ai.dwStatus & ANIMSTATUS_Playing))
                                continue;
                        if(!(ai.dwStatus & ANIMSTATUS_Paused))
                        {
                                if(ai.pAnim && ai.pAnim->NumFrames < 60)
                                {
                                        curframe = ai.fCurFrame;
                                        numframe = (float)ai.pAnim->NumFrames;
                                        break;
                                }
                        }
                }
                if(player->PlayerState == PLAYER_MOVE_SPRINT)
                        mStepDist *=2;
 
                float mWave = 0;
                bool rightlean = true;
                if(numframe > 0)
                {
                        mWave = curframe / numframe;
                       
                        // want to go 0-1-0 at 0,50,100
                        // want to go 0-1-0-(-1)-0 at 0,25,50,75,100
                        if(mWave >= 0.5f)
                        {      
                                mWave -= 0.5f;
                                rightlean = false;
                        } else
                        {
                                rightlean = true;
 
                        }
 
                        mWave *=2;
                        // if greater than .5, subtract .5 to make 0.5
                        // now go back down to zero if > .5
                        if(mWave >= 0.5f)
                                mWave = 1.0f - mWave;
                        mWave *=2;
                }
 
                float boba = 0.1f;
                float rolla = 0.2f;
 
                // in aim mode no bob
                switch ( player->PlayerState)
                {
                case PLAYER_MOVE_WALK_AIM:
                        {
                                mWave = 0;
                                boba = 0.0f;
                        }
                        // crouch mode no bob
                case PLAYER_MOVE_CROUCH: // intentional fallthrough
                        {
                                mWave = 0;
                                boba = 0.0f;
                        }
                case PLAYER_MOVE_CROUCH_AIM:// intentional fallthrough
                        {
                                mWave = 0;
                                boba = 0.0f;
                        }
                case PLAYER_MOVE_PRONE: // intentional fallthrough
                        {
                                mWave = 0;
                                boba = 0.0f;
                        }
                case PLAYER_PRONE_AIM:// intentional fallthrough
                        {
                                mWave = 0;
                                boba = 0.0f;
                        }
                case PLAYER_PRONE_IDLE:// intentional fallthrough
                        {
                                mWave = 0;
                                boba = 0.0f;
                        }
                case PLAYER_IDLE:// intentional fallthrough
                        {
                                mWave = 0;
                                boba = 0.0f;
                        }
                case PLAYER_IDLEAIM:// intentional fallthrough
                        // no bob if turn in place or idle
                        {
                                mWave = 0;
                                boba = 0.0f;
                        }
                        break;
                // lower bob on low speed
                case PLAYER_MOVE_RUN:
                        {
                                boba *= .6f; // yeah this doesn't do anything, it's here in case we want to tweak it.
                                rolla = 1;
                        }
                        break;
                case PLAYER_MOVE_SPRINT:
 
                        {
                                boba *=1.0f;
                                rolla = 2;
                        }
                        break;
                }
 
                //boba = 0; // disable bob
                camBob.y = boba * sin(mWave * R3D_PI_2);
 
                r3dPoint3D p(0,1,0);
 
                float _angle = sin(mWave * R3D_PI_2 ) * rolla;
                if(_angle < 0)
                        _angle += 360.0f;
                else if(_angle > 360.0f)
                        _angle -=360.0f;
       
                if ( rightlean == false )
                {
                        _angle = -(_angle);
                }
 
                p.RotateAroundZ(_angle);
                p.Normalize();
               
                // this currently will half the camUp's lean.  But the system works with this.
                camUp = camUp + p;
                camUp.Normalize();
        }
 
        return;
 
}

static bool g_CameraPointToAdj_HasAdjustedVec = false;
static r3dPoint3D g_CameraPointToAdj_adjVec(0,0,0);
static r3dPoint3D g_CameraPointToAdj_nextAdjVec(0,0,0);

float		g_CameraLeftSideSource = -0.7f;
float		g_CameraLeftSideTarget = 1.0f;
float		g_CameraLeftSideLerp = 1.0f;

float		getCameraLeftSide()
{
	return R3D_LERP(g_CameraLeftSideSource, g_CameraLeftSideTarget, g_CameraLeftSideLerp);
}

void		updateCameraLeftSide()
{
	if(g_CameraLeftSideLerp < 1.0f)
		g_CameraLeftSideLerp = R3D_CLAMP(g_CameraLeftSideLerp+r3dGetFrameTime()*5.0f, 0.0f, 1.0f);
}

r3dPoint3D getAdjustedPointTo(obj_Player* pl, const r3dPoint3D& PointTo, const r3dPoint3D& CamPos)
{
	if(g_camera_mode->GetInt()==2)
		return R3D_ZERO_VECTOR;

	static r3dPoint3D currentLookAt(0,0,0);
	static float	  currentLookAtDist = 0.0f;
	if(LerpValue == 1.0f)
	{
		//r3dOutToLog("Lerp finished\n");
		g_CameraPointToAdj_adjVec = g_CameraPointToAdj_nextAdjVec;
	}
	else if(!g_CameraPointToAdj_HasAdjustedVec)
	{
		{
			r3dPoint3D dir;
			if(pl->m_isInScope || g_camera_mode->GetInt() != 1 )
				r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH2, &dir);
			else
				r3dScreenTo3D(r3dRenderer->ScreenW2, r3dRenderer->ScreenH*0.32f, &dir);

			PxRaycastHit hit;
			PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK|(1<<PHYSCOLL_NETWORKPLAYER), 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC|PxSceneQueryFilterFlag::eDYNAMIC);
			if(g_pPhysicsWorld->raycastSingle(PxVec3(gCam.x, gCam.y, gCam.z), PxVec3(dir.x, dir.y, dir.z), 2000.0f, PxSceneQueryFlag::eIMPACT|PxSceneQueryFlag::eDISTANCE, hit, filter))
			{
				currentLookAt.Assign(hit.impact.x, hit.impact.y, hit.impact.z);
				currentLookAtDist = hit.distance;
			}
			else
			{
				currentLookAt = CamPos + dir * 1000.0f;
				currentLookAtDist = 1000.0f;
			}
		}


		g_CameraPointToAdj_HasAdjustedVec = true;
		r3dPoint3D DestCamPos = pl->GetPosition();
		r3dPoint3D offset;
		if(pl->hasScopeMode())
		{
			offset =  r3dPoint3D( 0, (pl->getPlayerHeightForCamera() +  TargetRig.ScopePosition.Y), 0 );
			offset += pl->GetvRight() * TargetRig.ScopePosition.X;
			offset += pl->m_vVision * (TargetRig.ScopePosition.Z);
		}
		else
		{
			offset =  r3dPoint3D( 0, (pl->getPlayerHeightForCamera() +  TargetRig.Position.Y), 0 );
			offset += pl->GetvRight() * TargetRig.Position.X * getCameraLeftSide();
			offset += pl->m_vVision * (TargetRig.Position.Z);
		}

		DestCamPos += offset;

		r3dPoint3D playerPosHead = pl->GetPosition(); playerPosHead.y += pl->getPlayerHeightForCamera();
		CheckCameraCollision(DestCamPos, playerPosHead, false);

		r3dPoint3D curViewVec = PointTo - CamPos;
		curViewVec.Normalize();

		r3dPoint3D destViewVec = PointTo - DestCamPos;
		destViewVec.Normalize();

		r3dPoint3D curLookAt = currentLookAt;
		if(!pl->hasScopeMode() &&  g_camera_mode->GetInt() == 1 ) // we only need this offset in offcenter mode.
		{
			float fHeight = currentLookAtDist * tan(R3D_DEG2RAD(TargetRig.FOV) * 0.5f);
			curLookAt.y -= fHeight * 0.35f;
		}
		r3dPoint3D destViewVec2 = curLookAt - DestCamPos;
		destViewVec2.Normalize();

		/* float d1 = pl->m_vVision.Dot(destViewVec);
		float d2 = pl->m_vVision.Dot(destViewVec2);
		if(!pl->hasScopeMode())
		if(d2 < 0.99f)
		destViewVec2 = destViewVec;*/

		//r3dOutToLog("Lerp=%.2f, pl_state=%d, aiming=%d\n", LerpValue, pl->PlayerState, pl->m_isAiming);

		static bool wasAiming = false;
		if(pl->m_isAiming)
		{
			if(!wasAiming)
			{
				wasAiming = true;
				if(currentLookAtDist < 5.0f && pl->hasScopeMode())
					g_CameraPointToAdj_nextAdjVec = r3dPoint3D(0,0.25f,0);
				else
					g_CameraPointToAdj_nextAdjVec = destViewVec2 - destViewVec;
			}
		}
		else
		{
			wasAiming = false;
			g_CameraPointToAdj_nextAdjVec = r3dPoint3D(0,0,0);
		}

		//r3dOutToLog("(%d): %.2f, %.2f\n", pl->m_isAiming, d1, d2);
		//r3dOutToLog("switching (%d): %.2f, %.2f, %.2f; %.2f\n", pl->m_isAiming, g_CameraPointToAdj_nextAdjVec.x, g_CameraPointToAdj_nextAdjVec.y, g_CameraPointToAdj_nextAdjVec.z, currentLookAtDist);
		//r3dOutToLog("vec: %.2f, %.2f, %.2f; %.2f, %.2f, %.2f\n", destViewVec.x, destViewVec.y, destViewVec.z, destViewVec2.x, destViewVec2.y, destViewVec2.z);
	}

	return R3D_LERP(g_CameraPointToAdj_adjVec, g_CameraPointToAdj_nextAdjVec, LerpValue);
}

extern float DepthOfField_NearStart;
extern float DepthOfField_NearEnd;
extern float DepthOfField_FarStart;
extern float DepthOfField_FarEnd;
extern int _FAR_DOF;
extern int _NEAR_DOF;
extern int LevelDOF;

// runs in actual game
int spectator_observingPlrIdx = 0;
r3dPoint3D spectator_cameraPos(0,0,0);
void TPSGameHUD :: SetCameraPure ( r3dCamera &Cam)
{
	if(!TPSGameHud_Inited) return;
#ifndef FINAL_BUILD
	if(d_video_spectator_mode->GetBool() || d_observer_mode->GetBool())
	{
		r3dPoint3D CamPos = FPS_Position;
		CamPos.Y += 1.8f;
		r3dPoint3D ViewPos = CamPos + FPS_vVision*10.0f;

		Cam.FOV = r_video_fov->GetFloat();
		Cam.SetPosition( CamPos );
		Cam.PointTo(ViewPos);

		LevelDOF = r_video_DOF_enable->GetBool();
		_NEAR_DOF = 1;
		_FAR_DOF = 1;
		DepthOfField_NearStart = r_video_nearDOF_start->GetFloat();
		DepthOfField_NearEnd = r_video_nearDOF_end->GetFloat();
		DepthOfField_FarStart = r_video_farDOF_start->GetFloat();
		DepthOfField_FarEnd = r_video_farDOF_end->GetFloat();

		return;
	}
#endif

	const ClientGameLogic& CGL = gClientLogic();
	obj_Player* pl = CGL.localPlayer_;
	//if(!pl) return;
	if(pl == 0)
	{
		return;
	}

	
	//Codex Carros
	extern bool SetCameraPlayerVehicle(const obj_Player* pl, r3dCamera &Cam);
	if(SetCameraPlayerVehicle(pl, Cam) || pl->isInVehicle() || pl->isPassenger()) // server vehicles
	{
		FPS_Position = Cam;
		return;
	}

	if(pl->bDead && !hudPause->isActive() && !hudMain->isPlayersListVisible() && !hudPause->isActive() && !hudTrade->isActive()
		&& !hudRepair->isActive())
	{
		r3dPoint3D camPos, camPointTo;
		bool do_camera = false;
		bool check_cam_collision = true;
		{
			static r3dPoint3D oldPlayerPos(0,0,0);
			static r3dPoint3D camPosOffset(0,0,0);
			camPointTo = pl->GetPosition();

			// find a cam position
			if(!oldPlayerPos.AlmostEqual(pl->GetPosition())) // make sure to do that check only once
			{
				oldPlayerPos = pl->GetPosition();
				r3dPoint3D possible_cam_offset[4] = {r3dPoint3D(-3, 5, -3), r3dPoint3D(3, 5, -3), r3dPoint3D(-3, 5, 3), r3dPoint3D(3, 5, 3)};
				int found=-1;
				for(int i=0; i<4; ++i)
				{
					r3dPoint3D raydir = ((pl->GetPosition()+possible_cam_offset[i]) - camPointTo);
					float rayLen = raydir.Length();
					if(rayLen > 0)
					{
						raydir.Normalize();
						PxRaycastHit hit;
						PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
						if(!g_pPhysicsWorld->raycastSingle(PxVec3(camPointTo.x, camPointTo.y, camPointTo.z), PxVec3(raydir.x, raydir.y, raydir.z), rayLen, PxSceneQueryFlag::eIMPACT, hit, filter))
						{
							found = i;
							break;
						}
					}
				}
				if(found!=-1)
				{
					camPosOffset = possible_cam_offset[found];
				}
				else
				{
					camPosOffset = r3dPoint3D(-0.1f, 5, -0.1f);
				}
			}

			camPos = pl->GetPosition() + camPosOffset;
			do_camera = true;
			check_cam_collision = false;
		}

		if(do_camera)
		{
			extern int g_CCBlackWhite;
			extern float g_fCCBlackWhitePwr;
			g_CCBlackWhite = 1;
			g_fCCBlackWhitePwr = R3D_CLAMP((r3dGetTime() - pl->TimeOfDeath)/2.0f, 0.0f, 1.0f); // go to black and white while look at our dead body

			// check for collision
			if(check_cam_collision)
				CheckCameraCollision(camPos, camPointTo, false);


			r3dVector CamPos = pl->GetPosition();
			CamPos += r3dPoint3D( 0, ( 5 ), 0 );

			int mMX=Mouse->m_MouseMoveX, mMY=Mouse->m_MouseMoveY;
			float  glb_MouseSensAdj = CurrentRig.MouseSensetivity * g_mouse_sensitivity->GetFloat();

			static float camangle = 0;
			static float camangle2 = 0;
			camangle += float(-mMX) * glb_MouseSensAdj;
			camangle2 += float(-mMY) * glb_MouseSensAdj;

			if(camangle > 360.0f ) camangle = camangle - 360.0f;
			if(camangle < 0.0f )   camangle = camangle + 360.0f;

			if(camangle2 > 30.0f ) camangle2 = 30.0f;
			if(camangle2 < -25.0f )   camangle2 = -25.0f;


			D3DXMATRIX mr;
			D3DXMatrixRotationYawPitchRoll(&mr, R3D_DEG2RAD(-camangle), R3D_DEG2RAD(-camangle2), 0);
			r3dVector plForwardVector = r3dVector(mr ._31, mr ._32, mr ._33);

			CamPos += -plForwardVector * 8 ;

			Cam.SetPosition(CamPos);
			Cam.PointTo( CamPos + plForwardVector * 3 + r3dVector ( 0, -1, 0) );
			Cam.vUP = r3dPoint3D(0, 1, 0);
			return;
		}
	}

	/*if(do_camera)
	{
	extern int g_CCBlackWhite;
	extern float g_fCCBlackWhitePwr;
	g_CCBlackWhite = 1;
	g_fCCBlackWhitePwr = R3D_CLAMP((r3dGetTime() - pl->TimeOfDeath)/2.0f, 0.0f, 1.0f); // go to black and white while look at our dead body

	// check for collision
	if(check_cam_collision)
	CheckCameraCollision(camPos, camPointTo, false);

	Cam.FOV = 60;
	Cam.SetPosition( camPos );
	Cam.PointTo( camPointTo );
	FPS_Position = Cam;
	return;
	}*/

	if(pl->bDead && hudAttm->isActive())
		hudAttm->Deactivate();

	if(hudPause->isActive())
		return;
	if(hudAttm->isActive())
		return;
	if(hudGeneralStore->isActive())
		return;
	if(hudVault->isActive())
		return;
	if(hudCraft->isActive())//Codex Craft
		return;
	if(hudTrade->isActive())
		return;

	int mMX=Mouse->m_MouseMoveX, mMY=Mouse->m_MouseMoveY;
	if(g_vertical_look->GetBool()) // invert mouse
		mMY = -mMY;

	/*if (pl->uberAnim_->IsFPSMode() && !pl->m_isAiming)
	GameFOV = 90;
	else if (pl->m_isAiming)
	GameFOV = CurrentRig.FOV;
	else
	GameFOV = CurrentRig.FOV+10;*/
	GameFOV = CurrentRig.FOV;

	float CharacterHeight = pl->getPlayerHeightForCamera();
	r3dPoint3D CamPos = pl->GetPosition();
	CamPos.Y +=  (CharacterHeight+  CurrentRig.Position.Y);
	updateCameraLeftSide();
	CamPos += pl->GetvRight() * CurrentRig.Position.X * getCameraLeftSide();
	CamPos += pl->m_vVision * CurrentRig.Position.Z;

	r3dPoint3D playerPos = pl->GetPosition();
	r3dPoint3D playerPosHead = playerPos; playerPosHead.y += CharacterHeight;
	r3dPoint3D PointTo = playerPos;
	PointTo.Y += (CharacterHeight + TPSHudCameraTarget.Y);
	PointTo += pl->GetvRight() * TPSHudCameraTarget.X;

	// check for collision
	{
		r3dPoint3D savedCamPos = CamPos;
		if(CheckCameraCollision(CamPos, playerPosHead, true) && (pl->PlayerState == PLAYER_MOVE_CROUCH || pl->PlayerState == PLAYER_MOVE_CROUCH_AIM || pl->PlayerState == PLAYER_MOVE_PRONE || pl->PlayerState == PLAYER_PRONE_AIM || pl->PlayerState == PLAYER_PRONE_IDLE)) 
		{
			CamPos = savedCamPos;
			playerPosHead = playerPos;
			playerPosHead.y += CharacterHeight-0.8f;
			CheckCameraCollision(CamPos, playerPosHead, true);
		}
	}

	PointTo += (pl->m_vVision) * 50;//cameraRayLen;//CurrentRig.Target.Z;

	r3dPoint3D adjPointTo(0,0,0);
	adjPointTo = getAdjustedPointTo(pl, PointTo, CamPos);

	r3dPoint3D camBob, camUp;
	Get_Camera_Bob(camBob, camUp, pl);

	Cam.FOV = GameFOV;
	Cam.SetPosition( CamPos + camBob );
	Cam.PointTo(PointTo + camBob);
	Cam.vUP = camUp;

	Cam.vPointTo += adjPointTo;

#ifndef FINAL_BUILD
	if( g_pHUDCameraEffects )
	{
		g_pHUDCameraEffects->Update( &Cam, CamPos ) ;
	}
#endif

	FPS_Position = Cam;
}
extern IDirect3DSurface9* _r3d_screenshot_copy2;
static void DrawMenus()
{
#ifndef FINAL_BUILD
	if(d_video_spectator_mode->GetBool() && !d_observer_mode->GetBool()) // no UI in spectator mode
		return;
	if(d_disable_game_hud->GetBool())
		return;
#endif

#if 0
	typedef std::vector<std::string> stringlist_t;
	extern stringlist_t currentMovies ;

	typedef std::vector< float > floats ;
	extern floats movieDurations ;

	char buff[ 512 ] ;
	sprintf( buff, "%d - Num Drawcalls", r3dRenderer->Stats.NumDraws );

	currentMovies.push_back( buff );
	movieDurations.push_back( 0.1f );

	typedef std::vector< int > sorties ;
	static sorties ss ;

	ss.resize( movieDurations.size() );

	for( int i = 0, e = movieDurations.size(); i < e; i ++ )
	{
		ss[ i ] = i ;
	}

	for( int i = 0, e = movieDurations.size(); i < e; i ++ )
	{
		for( int j = 0, e = movieDurations.size() - 1 ; j < e; j ++ )
		{
			if( movieDurations[ ss[ j ] ] > movieDurations[ ss[ j + 1 ] ] )
			{
				std::swap( ss[ j ], ss[ j + 1 ] );
			}
		}
	}

	r3dSetFiltering( R3D_POINT );

	r3dRenderer->pd3ddev->SetRenderState( D3DRS_ALPHATESTENABLE, 	FALSE );
	r3dRenderer->pd3ddev->SetRenderState( D3DRS_ALPHAREF,        	1 );

	r3dRenderer->SetMaterial(NULL);
	r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA);

	for( int i = 0, e = (int)currentMovies.size(); i < e; i ++ )
	{
		Font_Label->PrintF(r3dRenderer->ScreenW - 330, r3dRenderer->ScreenH-e*22 - 220 + i*22,r3dColor(255,255,255), "%.1f - %s", movieDurations[ ss[ i ] ] * 1000.f, currentMovies[ ss[ i ] ].c_str() );
	}

	currentMovies.clear();
	movieDurations.clear();

	r3dRenderer->pd3ddev->SetRenderState( D3DRS_ALPHATESTENABLE, 	FALSE );
	r3dRenderer->pd3ddev->SetRenderState( D3DRS_ALPHAREF,        	1 );

	r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_NZ);
#endif

	if(!win::bSuspended && !hudMain->isChatInputActive() && !hudMain->isPlayersListVisible()) 
	{
		bool showHudPause = Keyboard->WasPressed(kbsEsc) || InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_SWITCH_MINIMAP) || InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_INVENTORY);
		if(showHudPause && !hudAttm->isActive() && !hudGeneralStore->isActive() && !hudVault->isActive())
		{
			if(!hudPause->isActive())
			{
				gClientLogic().localPlayer_->ShowCrosshair=false;
				hudPause->Activate();
				if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_SWITCH_MINIMAP))
					hudPause->showMap();
				else if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_INVENTORY))
					hudPause->showInventory();
			}
			/*else
				hudPause->Deactivate();*/
			else {
				if (Keyboard->WasPressed(kbsEsc))
				{
						hudPause->Deactivate();
						gClientLogic().localPlayer_->ShowCrosshair=true;//Codex Mira
				}
			}
		}
		// toggle hud - gay using this!
		if (InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_TOGGLEHUD))
			d_disable_render_hud->SetBool(!d_disable_render_hud->GetBool());

		// autorun 
		if (InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_MOVE_FORWARD_AUTORUN))
			d_autorun->SetBool(!d_autorun->GetBool());

		/*if (Keyboard->WasPressed(kbsU) && !hudTrade->isActive())
		{
		hudTrade->Activate();
		}*/

		// for debuging
		/*if (Keyboard->WasPressed(kbsB))
		{
			if(!hudGeneralStore->isActive())
			{
				hudGeneralStore->Activate();
				//PKT_C2S_BuyItemReq_s n;
				//p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
			}
		}*/

		if (Keyboard->WasPressed(kbsK))
		{
			if(!hudRepair->isActive())
				hudRepair->Activate();
			else
				hudRepair->Deactivate();
		}

		bool showAttachment = InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_SHOW_ATTACHMENTS);
		if(showAttachment)
		{
			if(!hudAttm->isActive())
			{
				hudAttm->Activate();
			}
			else
				hudAttm->Deactivate();
		}
		//////////////////////////////////////////////////////////////////////////////////////////////
		//Codex Craft
		bool showCraft = InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_CRAFT);
		if (!gClientLogic().localPlayer_)
			showCraft=false;
		if (gClientLogic().localPlayer_->bDead)
			showCraft=false;
		if(showCraft)
		{
			if(!hudCraft->isActive())
			{
				gClientLogic().localPlayer_->ShowCrosshair=false;
				hudCraft->Activate();
			}
			else {
				hudCraft->Deactivate();
				gClientLogic().localPlayer_->ShowCrosshair=true;
			}
		}
		//////////////////////////////////////////////////////////////////////////////////////////////

		if(hudAttm->isActive() && Keyboard->WasPressed(kbsEsc))
		{
			hudAttm->Deactivate();
			gClientLogic().localPlayer_->ShowCrosshair=true;
		}

		if(hudGeneralStore->isActive() && Keyboard->WasPressed(kbsEsc))
		{
			gClientLogic().localPlayer_->ShowCrosshair=true;
			hudGeneralStore->Deactivate();
		}

		if(hudVault->isActive() && Keyboard->WasPressed(kbsEsc))
		{
			gClientLogic().localPlayer_->ShowCrosshair=true;
			hudVault->Deactivate();
		}
		if(hudCraft->isActive() && Keyboard->WasPressed(kbsEsc))//Codex Craft
		{
			hudCraft->Deactivate();
			gClientLogic().localPlayer_->ShowCrosshair=true;
		}
	}

	if(hudPause->isActive())
	{
		r3dMouse::Show(); // make sure that mouse is visible

		R3DPROFILE_START( "hudPause->" );

		hudPause->Update();
		hudPause->Draw();

		R3DPROFILE_END( "hudPause->" );

		return;
	}

	if(hudAttm->isActive())
	{
		r3dMouse::Show(); // make sure that mouse is visible

		R3DPROFILE_START( "hudAttm->" );

		hudAttm->Update();
		hudAttm->Draw();

		R3DPROFILE_END( "hudAttm->" );

		return;
	}

	if(hudGeneralStore->isActive())
	{
		r3dMouse::Show(); // make sure that mouse is visible

		R3DPROFILE_START( "hudGeneralStore->" );

		hudGeneralStore->Update();
		hudGeneralStore->Draw();

		R3DPROFILE_END( "hudGeneralStore->" );

		return;
	}

	if(hudVault->isActive())
	{
		r3dMouse::Show(); // make sure that mouse is visible

		R3DPROFILE_START( "hudVault->" );

		hudVault->Update();
		hudVault->Draw();

		R3DPROFILE_END( "hudVault->" );

		return;
	}


	if(hudTrade->isActive())
	{
		r3dMouse::Show(); // make sure that mouse is visible

		R3DPROFILE_START( "hudTrade->" );

		hudTrade->Update();
		hudTrade->Draw();

		R3DPROFILE_END( "hudTrade->" );

		return;
	}

	if(hudRepair->isActive())
	{
		r3dMouse::Show(); // make sure that mouse is visible

		R3DPROFILE_START( "hudRepair->" );

		hudRepair->Update();
		hudRepair->Draw();

		R3DPROFILE_END( "hudRepair->" );

		return;
	}

	if(hudActionUI->isActive())
	{
		R3DPROFILE_START( "hudActionUI->" );
		hudActionUI->Update();
		hudActionUI->Draw();
		R3DPROFILE_END( "hudActionUI->" );
	}

	if(hudCraft->isActive())//Codex Craft
	{
		r3dMouse::Show(); // make sure that mouse is visible

		R3DPROFILE_START( "hudCraft->" );

		hudCraft->Update();
		hudCraft->Draw();

		R3DPROFILE_END( "hudCraft->" );

		return;
	}


	bool ChatWindowSwitch = InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_CHAT);

	const ClientGameLogic& CGL = gClientLogic();
	const obj_Player* pl = CGL.localPlayer_; // can be null
	if(pl == NULL) // no player, we need to show respawn menu and let player enter game
	{
	}
	else
	{
		// check for respawn screen
		if(pl->bDead) 
		{
		}
		else
		{
		}

		if(ChatWindowSwitch && hudMain && !hudMain->isChatInputActive())
		{
			hudMain->showChatInput();
		}

		if(hudMain)
		{
			if(InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_CHAT_CHANNEL1))
				hudMain->setChatChannel(0);
			if(InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_CHAT_CHANNEL2))
				hudMain->setChatChannel(1);
			if(InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_CHAT_CHANNEL3))
				hudMain->setChatChannel(2);
			if(InputMappingMngr->wasReleased(r3dInputMappingMngr::KS_CHAT_CHANNEL4))
				hudMain->setChatChannel(3);
		}

		if(hudMain && !hudMain->isChatInputActive())
		{
			bool showPlayerList = InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_SHOW_PLAYERS);
			if(showPlayerList)
			{
				if (!hudMain->isPlayersListVisible())
				{
					hudMain->clearPlayersList();
					int index = 0;
					for(int i=0; i<R3D_ARRAYSIZE(CGL.playerNames); i++)
					{
						if(CGL.playerNames[i].Gamertag[0])
						{
							char plrUserName[64]; CGL.localPlayer_->GetUserName(plrUserName);
							hudMain->addPlayerToList(index++, CGL.playerNames[i].Gamertag, CGL.playerNames[i].plrRep, CGL.playerNames[i].isLegend, CGL.playerNames[i].isPunisher, CGL.playerNames[i].isPunisher, CGL.playerNames[i].isInvitePending,CGL.playerNames[i].isPremium,CGL.playerNames[i].isMute,strcmp(CGL.playerNames[i].Gamertag, plrUserName)==0);
						}
					}
					hudMain->showPlayersList(1);
					//gfxHUD.Invoke("_root.api.Main.refreshPlayerGroupList", "");
					//hudMain->groupmenu();
					r3dMouse::Show();
				}
				else
				{
					hudMain->showPlayersList(0);
					r3dMouse::Hide();
				}
			}
			if(hudMain->isPlayersListVisible() && Keyboard->WasPressed(kbsEsc))
			{
				hudMain->showPlayersList(0);
				r3dMouse::Hide();
			}
		}

		// render flash UI for objects

		R3DPROFILE_START( "GameWorld().Draw(rsDrawFlashUI)" );
		GameWorld().Draw(rsDrawFlashUI);
		R3DPROFILE_END( "GameWorld().Draw(rsDrawFlashUI)" );

		{
			{
				R3DPROFILE_START( "hudMain->" );

				hudMain->Update();
				hudMain->Draw();

				R3DPROFILE_END( "hudMain->" );
			}

			// issue d3d cheat check on some frames (will stop issuing anti cheat if caught cheat)
			// wait 5 minute before doing check. after that, do check every other 2-5 minutes
			//extern void UpdateD3DAntiCheat_WallHack();
			//extern static bool D3DCheaters[ ANTICHEAT_COUNT ];
			//if (D3DCheaters[ ANTICHEAT_WALLHACK ])
			//hudMain->addChatMessage(0,"<SYSTEM>","Wallhack Detected",0);

			//UpdateD3DAntiCheat_WallHack();

			/*if(!pl->bDead && (r3dGetTime() - pl->TimeOfLastRespawn)>300.0f && !hudAttm->isActive() &&
			!hudPause->isActive() && !hudActionUI->isActive() && !hudGeneralStore->isActive() &&
			!hudVault->isActive())
			{*/
			static float nextCheck = r3dGetTime() + 1;
			if(r3dGetTime() > nextCheck)
			{ 
#ifndef CHEAT_ENABLED
				issueD3DAntiCheatCodepath( ANTICHEAT_WALLHACK );
#endif
				nextCheck = r3dGetTime() + 120;
			}
			static float nextCheck2 = r3dGetTime() + 0.05f;
			if(r3dGetTime() > nextCheck2)
			{
				//issueD3DAntiCheatCodepath( ANTICHEAT_SCREEN_HELPERS2 );
				nextCheck2 = r3dGetTime() + 10;
			}
			/*static float nextCheck2 = r3dGetTime() + 0.5f;
			if(r3dGetTime() > nextCheck2)
			{
			issueD3DAntiCheatCodepath( ANTICHEAT_SCREEN_HELPERS2 );
			nextCheck2 = r3dGetTime() + 0.5f;
			}*/
		}

		if(hudMain && (hudMain->isChatInputActive() || hudMain->isPlayersListVisible())) // also checks for write note, so do not hide mouse
			return;

		// draw main hud with hidden mouse
		// this call is FREE if mouse was hidden already
		// [denis]: do not remove for now, this is minor hack for situation when app was started inactive. 
		// [pavel]: that fucks up controls, when big map is on screen, or scoreboard is, you shouldn't be able to move character, as in that mode you are actually using mouse
		//			if app was started inactive, just press M twice and that's it. 
		// [pavel]: ok, that should fix a problem. If non of modal windows are active, then hide mouse.
		if(!win::bSuspended && !g_cursor_mode->GetInt())
			r3dMouse::Hide();
	}
}

void UpdateTs3Server()
{
	ClientGameLogic& CGL = gClientLogic();
	for(int i=0; i<R3D_ARRAYSIZE(gClientLogic().playerNames); i++)
	{
		for(int i2=0; i2<ClientGameLogic::MAX_NUM_PLAYERS; i2++)
		{
			obj_Player* plr = CGL.GetPlayer(i2);
			obj_Player* local = CGL.localPlayer_;
			if (plr && local)
			{
				char plrUserName[64]; plr->GetUserName(plrUserName);
				if (strcmp(CGL.playerNames[i].Gamertag, plrUserName)==0)
				{
					if ( (local->GetPosition() - plr->GetPosition()).Length() < 50 && strcmp(ip, plr->ip)!=0)
					{
						//ts3client_closePlaybackDevice(r_voipPlayBack->GetInt());
						//ts3client_closeCaptureDevice(r_voipCapture->GetInt());

						//ts3client_stopConnection(scHandlerID, "leaving");
						//ts3client_destroyServerConnectionHandler(scHandlerID);
						ts3client_spawnNewServerConnectionHandler(0,&scHandlerID);
						char identity[10000];
						if(readIdentity(identity) != 0) {
							char* id;
							ts3client_createIdentity(&id);
							if(strlen(id) >= 10000) {
								printf("Not enough bufsize for identity string\n");
							}
							strcpy(identity, id);
							ts3client_freeMemory(id);
							writeIdentity(identity);
						}
						char plr3msg[128] = {0};
						sprintf(plr3msg,"%d",gUserProfile.CustomerID);
						ts3client_startConnection(scHandlerID, identity, "127.0.0.1", 9987,plr3msg, NULL, "", "voippassword");
						sprintf(ip,plr->ip);

						char* mode;
						char* mode2;
						ts3client_getDefaultPlayBackMode(&mode);

						ts3client_getDefaultCaptureMode(&mode2);
						char*** device;
						char*** device2;
						ts3client_getPlaybackDeviceList(mode,&device);
						ts3client_getCaptureDeviceList(mode2,&device2);
						// Fix Crash because not have a input / output device
						//ts3client_closePlaybackDevice(scHandlerID);
						//ts3client_closeCaptureDevice(scHandlerID);
						if (device[r_voipPlayBack->GetInt()] != NULL)
						{

							char *devicee = device[r_voipPlayBack->GetInt()][1];
							ts3client_openPlaybackDevice(scHandlerID, mode, devicee);
							ts3client_freeMemory(device);
							ts3client_freeMemory(mode);
						}
						if (device2[r_voipCapture->GetInt()] != NULL)
						{

							char *devicee2 = device2[r_voipCapture->GetInt()][1];
							ts3client_openCaptureDevice(scHandlerID, mode2, devicee2);
							ts3client_freeMemory(device2);
							ts3client_freeMemory(mode2);
						}
						r3dOutToLog("Voip connect to %s\n",ip);
						return;
					}
				}
			}
		}
	}
}
void TPSGameHUD :: Draw()
{
	/*// Inject Check
	DWORD dwModuleSize = 0;
	HMODULE hModules[10000] = { 0 };

	// Get all modules for a process 


	EnumProcessModules( GetCurrentProcess(), 
	hModules, 
	sizeof( hModules ), 
	&dwModuleSize );
	dwModuleSize /= sizeof( HMODULE );
	MODULEINFO ppModuleInfo;
	for( DWORD dwModIndex = 1; dwModIndex < dwModuleSize; ++ dwModIndex )
	{
	GetModuleInformation( GetCurrentProcess(), 
	hModules[dwModIndex], 
	&ppModuleInfo, 
	sizeof( MODULEINFO ));
	//r3dOutToLog("%p\n", ppModuleInfo.SizeOfImage);
	if (ppModuleInfo.SizeOfImage == 0x1A6000) // EliteVision
	{
	char chatmessage[128] = {0};
	obj_Player* plr = gClientLogic().localPlayer_;
	PKT_C2C_ChatMessage_s n;
	n.userFlag = 2; // server will init it for others
	n.msgChannel = 1;
	sprintf(chatmessage, "Kicked %s From Server Inject Dll EliteVision",plr->CurLoadout.Gamertag);
	r3dscpy(n.gamertag, "<System>");
	r3dscpy(n.msg, chatmessage);
	p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));

	ShowWindow(r3dRenderer->HLibWin, SW_MINIMIZE);
	r3dError("Inject Found : EliteVision\n");
	ExitProcess(-1);
	}
	}*/

	/*DWORD dwModuleSize = 0;
	HMODULE hModules[10000] = { 0 };

	// Get all modules for a process 


	EnumProcessModules( GetCurrentProcess(), 
	hModules, 
	sizeof( hModules ), 
	&dwModuleSize );
	dwModuleSize /= sizeof( HMODULE );
	MODULEINFO ppModuleInfo;
	for( DWORD dwModIndex = 1; dwModIndex < dwModuleSize; ++ dwModIndex )
	{
	GetModuleInformation( GetCurrentProcess(), 
	hModules[dwModIndex], 
	&ppModuleInfo, 
	sizeof( MODULEINFO ));
	}*/


	if(!TPSGameHud_Inited) r3dError("!TPSGameHud_Inited");
#ifdef VOIP_ENABLED 
	if (gClientLogic().localPlayer_ && r_voip->GetBool())
	{
		//UpdateTs3Server();
		//UpdateTs3();
	}
#endif

	/*DWORD version;
	_AhnHS_GetSDKVersion(&version);
	_AhnHS_Direct3DCreate9(version);*/

	if (gClientLogic().localPlayer_ && !gUserProfile.ProfileData.isDevAccount)
	{
		obj_Player* pl = gClientLogic().localPlayer_;
		if (/*pl->InputAcceleration.z > 5.5f || */pl->RealAcceleration.z > 5.50f)
			hs_callback->SetInt((int)AHNHS_ACTAPC_DETECT_SPEEDHACK);
	}
#ifdef HS_ENABLED
	if (_AhnHS_CheckHackShieldRunningStatus() == 0 && gClientLogic().localPlayer_) // send only hackshield is running if not server will kicked you.
	{
#endif
		if (r3dGetTime() > LastHSLog + 5.0f) // send hs log every 5 sec.
		{
			LastHSLog = r3dGetTime();
			//float time = (float)_time64(&time);
			PKT_C2S_HackShieldLog_s n;
			n.lCode = hs_callback->GetInt();
			n.Status = 0;
			sprintf(n.pName,hs_pName->GetString());
			// n.time = GetTickCount() / 1000.0f;
			p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
			/*// WpnLog
			{
			PKT_C2S_WpnLog_s n;
			n.itemid = 0;
			Weapon* wpn = gClientLogic().localPlayer_->m_Weapons[gClientLogic().localPlayer_->m_SelectedWeapon];
			if (wpn)
			{
			const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(wpn->getItemID());
			n.itemid = wpn->getItemID();
			if (wc)
			{
			n.m_AmmoDamage = wc->m_AmmoDamage;
			n.m_AmmoMass = wc->m_AmmoMass;
			n.m_AmmoSpeed = wc->m_AmmoSpeed;
			n.m_recoil = wc->m_recoil;
			n.m_spread = wc->m_spread;
			}
			}
			p2pSendToHost(gClientLogic().localPlayer_, &n, sizeof(n));
			}*/
		}
#ifdef HS_ENABLED
	}
#endif
	assert(bInited);
	if ( !bInited ) return;

	R3DPROFILE_D3DSTART( D3DPROFILE_SCALEFORM ) ;

	r3dSetFiltering( R3D_POINT );

	r3dRenderer->pd3ddev->SetRenderState( D3DRS_ALPHATESTENABLE, 	FALSE );
	r3dRenderer->pd3ddev->SetRenderState( D3DRS_ALPHAREF,        	1 );

	r3dRenderer->SetMaterial(NULL);
	r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_NZ);

	DrawMenus();

	//UpdateObjects();

	R3DPROFILE_D3DEND( D3DPROFILE_SCALEFORM ) ;

	return;  
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Codex Carros
bool SetCameraPlayerVehicle(const obj_Player* pl, r3dCamera &Cam) // Server Vehicles
{
	static bool wasDrivenByPlayer = false;
#if VEHICLES_ENABLED
	if ( g_pPhysicsWorld )// && g_pPhysicsWorld->m_VehicleManager->GetDrivenCar())// && d_drive_vehicles->GetBool() == true)// && !hudPause->isActive())
	{
		obj_Vehicle* vehicle = pl->ActualVehicle;//g_pPhysicsWorld->m_VehicleManager->getRealDrivenVehicle();
		if( vehicle )
		{
//#if	VEHICLE_CINEMATIC_MODE
			r3dVector CamPos = vehicle->GetPosition();
			CamPos += r3dPoint3D( 0, ( 5 ), 0 );

			int mMX=Mouse->m_MouseMoveX, mMY=Mouse->m_MouseMoveY;
			float  glb_MouseSensAdj = CurrentRig.MouseSensetivity * g_mouse_sensitivity->GetFloat();

			static float camangle = 0;
			static float camangle2 = 0;
			if (!hudPause->isActive() && !hudMain->isPlayersListVisible())
			{
			camangle += float(-mMX) * glb_MouseSensAdj;
			camangle2 += float(-mMY) * glb_MouseSensAdj;


			if(camangle > 360.0f ) camangle = camangle - 360.0f;
			if(camangle < 0.0f )   camangle = camangle + 360.0f;

			if(camangle2 > 5.0f ) camangle2 = 5.0f;
			if(camangle2 < -30.0f )   camangle2 = -30.0f;
			}


			D3DXMATRIX mr;
			D3DXMatrixRotationYawPitchRoll(&mr, R3D_DEG2RAD(-camangle), R3D_DEG2RAD(-camangle2), 0);
			//D3DXMatrixRotationYawPitchRoll(&mr, R3D_DEG2RAD(-camangle), 0.0f, 0);
			r3dVector vehicleForwardVector = r3dVector(mr ._31, mr ._32, mr ._33);


			if(Keyboard->WasPressed(kbsC) && !hudMain->isChatInputActive())
			{
                    externalcam++;
				if (externalcam>5)
					externalcam=1;
			}

          if (externalcam==1)
		  {
			        CamPos += -vehicleForwardVector * 8 ;
			        Cam.SetPosition(CamPos);
			        Cam.PointTo( CamPos + vehicleForwardVector * 3 + r3dVector ( 0, -1, 0) );
			        Cam.vUP = r3dPoint3D(0, 1, 0);
		  }
		  else if (externalcam==2)
		  {
			        r3dPoint3D InCarCam=vehicle->GetPosition();
			        InCarCam += r3dPoint3D( 0, ( 5 ), 0 );
			        D3DXMATRIX mr2;
			        D3DXMatrixRotationYawPitchRoll(&mr2, R3D_DEG2RAD(vehicle->GetRotationVector().x), R3D_DEG2RAD(vehicle->GetRotationVector().y), 0);
			        r3dVector vehicleForwardVector2 = r3dVector(mr2 ._31, mr2 ._32, mr2 ._33);

			        InCarCam += -vehicleForwardVector2 *8;

			        Cam.SetPosition(InCarCam);

			        Cam.PointTo(InCarCam + vehicleForwardVector2 * 3 + r3dVector ( 0, -1, 0) );
			        Cam.vUP = r3dPoint3D(0, 1, 0);
		  }
          else if (externalcam==3)
		  {
			        CamPos += -vehicleForwardVector * 15 ;
			        Cam.SetPosition(CamPos);
			        Cam.PointTo( CamPos + vehicleForwardVector * 3 + r3dVector ( 0, -1, 0) );
			        Cam.vUP = r3dPoint3D(0, 1, 0);
		  }
		  else if (externalcam==4)
		  {
			        r3dPoint3D InCarCam=vehicle->GetPosition();
			        InCarCam += r3dPoint3D( 0, ( 5 ), 0 );
			        D3DXMATRIX mr2;
			        D3DXMatrixRotationYawPitchRoll(&mr2, R3D_DEG2RAD(vehicle->GetRotationVector().x), R3D_DEG2RAD(vehicle->GetRotationVector().y), 0);
			        r3dVector vehicleForwardVector2 = r3dVector(mr2 ._31, mr2 ._32, mr2 ._33);

			        InCarCam += -vehicleForwardVector2 *15;

			        Cam.SetPosition(InCarCam);

			        Cam.PointTo(InCarCam + vehicleForwardVector2 * 3 + r3dVector ( 0, -1, 0) );
			        Cam.vUP = r3dPoint3D(0, 1, 0);
		  }
		  else if (externalcam==5) // No funciona bien
		  {
			        r3dPoint3D InCarCam=vehicle->GetPosition();
					r3dPoint3D Rotation=r3dVector(vehicle->GetRotationMatrix()._31,vehicle->GetRotationMatrix()._32,vehicle->GetRotationMatrix()._33);

					if (vehicle->FileName=="data/objectsdepot/vehicles/drivable_buggy_02.sco")
					{
                                InCarCam += r3dPoint3D( 0, ( 1.6 ), 0 );
								InCarCam +=-Rotation/1.4;
					}
					else if (vehicle->FileName=="data/objectsdepot/vehicles/drivable_buggy_01.sco")
					{
                                InCarCam += r3dPoint3D( 0, ( 1.4 ), 0 );
								InCarCam +=-Rotation/1.4;
					}
					else if (vehicle->FileName=="data/objectsdepot/vehicles/drivable_stryker.sco")
					{
			                    InCarCam += r3dPoint3D( 0, ( 2.5 ), 0 );
								InCarCam +=-Rotation/2;
					}
					else if (vehicle->FileName=="data/objectsdepot/vehicles/zombie_killer_car.sco")
					{
			                    InCarCam += r3dPoint3D( 0, ( 2.1 ), 0 );
								InCarCam +=-Rotation/2;
					}


			        Cam.SetPosition(InCarCam);
			        Cam.PointTo(InCarCam + Rotation*3 + r3dVector ( 0 , -1, 0));
			        Cam.vUP = r3dPoint3D( 0, 1, 0);
		  }

//#else
//			g_pPhysicsWorld->m_VehicleManager->ConfigureCamera(Cam);
//#endif
			if(Keyboard->WasPressed(kbsE) && wasDrivenByPlayer==false)
			   wasDrivenByPlayer = true;
		}
		else
		{
			wasDrivenByPlayer = false;
		}
	}
	else
	{
		wasDrivenByPlayer = false;
	}
#endif
	return wasDrivenByPlayer;

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static float g_lastAimAnimTime = -1.f;

void ShowMsgAndExitWithTimer(TCHAR *szMsg)
{
	UINT nThreadID = 0;

	//HANDLE hThread = ( HANDLE ) _beginthreadex ( NULL, 0, ShowMsgAndExitWithTimer_ThreadFunc, NULL, 0, &nThreadID );
	//_AhnHS_StopService();
	r3dError(szMsg);
	TerminateProcess(r3d_CurrentProcess,0);
}
int __stdcall AhnHS_Callback(long lCode, long lParamSize, void* pParam)
{
	//win::HsCallBack = lCode;
	hs_callback->SetInt((int)lCode);
	hs_pName->SetString((char*)pParam);
	//r3dOutToLog("HSCallBack %x\n",lCode);
	/*r3dOutToLog("HSCallBack %d\n",lCode);
	switch(lCode)
	{
	//Engine Callback
	case AHNHS_ENGINE_DETECT_GAME_HACK:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("Game Hack found\n%s"), (char*)pParam);
	ShowMsgAndExitWithTimer(szMsg);

	break;
	}
	case AHNHS_ENGINE_DETECT_WINDOWED_HACK:
	{
	ShowMsgAndExitWithTimer(_T("Windowed Hack found."));

	break;
	}
	case AHNHS_ACTAPC_DETECT_SPEEDHACK:
	{
	ShowMsgAndExitWithTimer(_T("Speed Hack found."));
	break;
	}


	case AHNHS_ACTAPC_DETECT_KDTRACE:	
	case AHNHS_ACTAPC_DETECT_KDTRACE_CHANGED:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("AHNHS_ACTAPC_DETECT_KDTRACE_CHANGED"), lCode);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}

	case AHNHS_ACTAPC_DETECT_AUTOMACRO:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("AHNHS_ACTAPC_DETECT_AUTOMACRO"), lCode);
	ShowMsgAndExitWithTimer(szMsg);

	break;
	}

	case AHNHS_ACTAPC_DETECT_ABNORMAL_FUNCTION_CALL:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("Detect Abnormal Memory Access\n%s"), (char*)pParam);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_ABNORMAL_MEMORY_ACCESS:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("Detect Memory Access\n%s"), (char*)pParam);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}


	case AHNHS_ACTAPC_DETECT_AUTOMOUSE:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield DETECT_AUTOMOUSE."), lCode);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_DRIVERFAILED:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield DETECT_DRIVERFAILED."), lCode);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_ALREADYHOOKED:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield Detect D3D Hack2."));
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_HOOKFUNCTION:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield Detect D3D Hack.\n Module Name:%s"), ACTAPCPARAM_DETECT_HOOKFUNCTION().szModuleName);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_MESSAGEHOOK:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield DETECT_MESSAGEHOOK."), lCode);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_MODULE_CHANGE:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield DETECT_MODULE_CHANGE."), lCode);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_ENGINEFAILED:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield DETECT_ENGINEFAILED."), lCode);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_CODEMISMATCH:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield CODEMISMATCH."), lCode);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_MEM_MODIFY_FROM_LMP:
	case AHNHS_ACTAPC_DETECT_LMP_FAILED:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield Detect memory modify."), lCode);
	ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	case AHNHS_ACTAPC_DETECT_ABNORMAL_HACKSHIELD_STATUS:
	{
	TCHAR szMsg[255];
	if (lCode != HS_ERR_ALREADY_GAME_STARTED)
	{
	_stprintf(szMsg, _T("HackShield Service already started by other game"), lCode);
	//ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	else
	{
	_stprintf(szMsg, _T("HackShield Service Error"), lCode);
	//ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	}
	case AHNHS_ACTAPC_DETECT_PROTECTSCREENFAILED:
	{
	TCHAR szMsg[255];
	_stprintf(szMsg, _T("HackShield PROTECTSCREENFAILED."), lCode);
	//ShowMsgAndExitWithTimer(szMsg);
	break;
	}
	}*/
	return 1;
}
void ProcessPlayerMovement(obj_Player* pl, bool editor_debug )
{
	//AHNHS_PROTECT_FUNCTION
	r3d_assert(pl->NetworkLocal);

	//Codex Carros
#if VEHICLES_ENABLED
	if (pl->isInVehicle() && !InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_INTERACT))
	{
		pl->PlayerState = PLAYER_MOVE_RUN;
		pl->UpdateLocalPlayerMovement();
		   return;
	}
	else if (pl->isPassenger() && !InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_INTERACT))
	{
		pl->PlayerState = PLAYER_MOVE_RUN;
		pl->UpdateLocalPlayerMovement();
		   return;
	}
#endif

	// check fire weapon should be called all the time, as it will reset weapon fire in case if you are sitting on the menu, etc
	{
		R3DPROFILE_FUNCTION("update fire");
		pl->CheckFireWeapon();
	}

	if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_SWITCH_FPS_TPS) && !(hudAttm && hudAttm->isActive()) && !(hudMain && hudMain->isChatInputActive()) && !Mouse->GetMouseVisibility() && !pl->bSwim && pl->PlayerState != PLAYER_PRONE_UP && pl->PlayerState != PLAYER_PRONE_DOWN)
	{
		if (!pl->isInVehicle() && !pl->isPassenger())//Codex Carros
		pl->switchFPS_TPS();
	}

	if (pl->bSwim && g_camera_mode->GetInt() == 2)
	{
		pl->switchFPS_TPS();
	}

	r3dPoint3D prevAccel = pl->InputAcceleration;
	pl->InputAcceleration.Assign(0, 0, 0);

	static int shiftWasPressed = 0;
	float movingSpeed = pl->plr_local_moving_speed * (1.0f/r3dGetFrameTime());

	// query mouse distance, so it will not be accumulated
	int mMX=Mouse->m_MouseMoveX, mMY=Mouse->m_MouseMoveY;
	if(g_vertical_look->GetBool()) // invert mouse
		mMY = -mMY;

	bool disablePlayerRotation = false;
	bool disablePlayerMovement = false;
	if(Mouse->GetMouseVisibility() || (hudMain && hudMain->isChatInputActive())) // do not update player if we are in menu control mode!
	{
		disablePlayerMovement = true;
		disablePlayerRotation = true;
	}
	if(pl->bDead)
		return;

	const Weapon* wpn = pl->m_Weapons[pl->m_SelectedWeapon];

	if(!Mouse->GetMouseVisibility())//Codex Carros
	{
		// vehicles
		if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_INTERACT) && !(hudMain &&hudMain->isChatInputActive())) // Server Vehicles
		{
#if VEHICLES_ENABLED

				if (pl->isPassenger())
				{
				   pl->exitVehicleHowPassenger();
				}
				else if (pl->isInVehicle())
				{
				   pl->exitVehicle();
				}
				externalcam=1;
#endif
		}
	}



	if(Keyboard->WasPressed(kbsE) && !hudMain->isChatInputActive())
	{
	
	}
	/*	if(pl->m_isFinishedAiming && !pl->m_isInScope)
	{
	if(Keyboard->WasPressed(kbsLeftShift))
	{
	R3D_SWAP(g_CameraLeftSideSource, g_CameraLeftSideTarget);
	g_CameraLeftSideLerp = 0.0f;
	}
	} */
	if(Keyboard->WasPressed(kbsLeftAlt) && !pl->m_isInScope && !gUserProfile.ProfileData.isDevAccount)
	{
		R3D_SWAP(g_CameraLeftSideSource, g_CameraLeftSideTarget);
		g_CameraLeftSideLerp = 0.0f;
	}

	bool  aiming      = pl->m_isAiming;
	int   playerState = aiming ? PLAYER_IDLEAIM : PLAYER_IDLE;

	if( g_lastAimAnimTime < 0 )
		g_lastAimAnimTime = r3dGetTime();

	float newAimTime = r3dGetTime();
	float deltaAimTime = newAimTime - g_lastAimAnimTime;
	g_lastAimAnimTime = newAimTime;

	const float AIM_LERP_SPEED = 8.0f;

	if( aiming )
	{
		r_grass_zoom_coef->SetFloat( R3D_LERP( r_grass_zoom_coef->GetFloat(), 2.0f, AIM_LERP_SPEED * deltaAimTime ) );
	}
	else
	{
		r_grass_zoom_coef->SetFloat( R3D_LERP( r_grass_zoom_coef->GetFloat(), 1.0f, AIM_LERP_SPEED * deltaAimTime ) );
	}

	if(!(g_camera_mode->GetInt()==2 && pl->NetworkLocal))
	{
		if(pl->IsJumpActive()) 
		{
			// in jump, keep current state  (so strafe will stay, for example) and disable movement
			playerState = pl->PlayerState;
		}
	}

	// not able to sprint with equipped RPG
	bool disableSprint = false;
	r3dAnimation::r3dAnimInfo* animInfo = pl->uberAnim_->anim.GetTrack(pl->uberAnim_->grenadeThrowTrackID);
	if(!(pl->uberAnim_->grenadePinPullTrackID==CUberAnim::INVALID_TRACK_ID && !(animInfo && (animInfo->GetStatus()&ANIMSTATUS_Playing))))
		disableSprint = true;

	if(pl->CurLoadout.Health < 10.0f)
		disableSprint = true;

	// check if player can straighten up, in case if there is something above his head he will not be able to stop crouching
	bool force_crouch = false;
	if(pl->bCrouch)
	{
		PxBoxGeometry bbox(0.2f, 0.9f, 0.2f);
		PxTransform pose(PxVec3(pl->GetPosition().x, pl->GetPosition().y+1.1f, pl->GetPosition().z), PxQuat(0,0,0,1));
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
		PxShape* shape;
		force_crouch = g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, shape, filter);
	}
	bool crouching = pl->bCrouch;
	if(pl->fHeightAboveGround < 0.5f)
	{
		if(g_toggle_crouch->GetBool())
		{
			if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_CROUCH) || Gamepad->WasReleased(gpB))
				crouching = !crouching;
		}
		else
			crouching = InputMappingMngr->isPressed(r3dInputMappingMngr::KS_CROUCH) || Gamepad->IsPressed(gpB);
	}
	else
		crouching = false;

	if(disablePlayerMovement)
		crouching = false;

	if(force_crouch)
		crouching = true;

	if(crouching) 
		playerState = aiming ? PLAYER_MOVE_CROUCH_AIM : PLAYER_MOVE_CROUCH;

	// check if player can straighten up, in case if there is something above his head he will not be able to stop proning
	bool force_prone = false;
	if(pl->bProne)
	{
		PxBoxGeometry bbox(0.2f, 0.9f, 0.2f);
		PxTransform pose(PxVec3(pl->GetPosition().x, pl->GetPosition().y+1.1f, pl->GetPosition().z), PxQuat(0,0,0,1));
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
		PxShape* shape;
		force_prone = g_pPhysicsWorld->PhysXScene->overlapAny(bbox, pose, shape, filter);
	}
	bool wasProning = pl->bProne;
	bool proning = pl->bProne;
	if(pl->fHeightAboveGround < 0.5f)
	{
		if(InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_PRONE) && !disablePlayerMovement && pl->proneTime+1 < r3dGetTime())
			proning = !proning;
	}
	else
		proning = false;

	if(force_prone)
		proning = true;

	extern float getWaterDepthAtPos(const r3dPoint3D& pos);
	float waterDepth = getWaterDepthAtPos(pl->GetPosition());

	{
		if(waterDepth > 1.4f)
		{
			proning = false;
			crouching = false;
		}
	}

	if(proning) 
	{
		if(!wasProning)
		{
			playerState = PLAYER_PRONE_DOWN;
			pl->proneTime = r3dGetTime();
		}
		else if(wasProning && pl->PlayerState == PLAYER_PRONE_DOWN) // check if we are still playing anim
		{
			bool stillPlayingAnim = false;
			std::vector<r3dAnimation::r3dAnimInfo>::iterator it;
			for(it=pl->uberAnim_->anim.AnimTracks.begin(); it!=pl->uberAnim_->anim.AnimTracks.end(); ++it) 
			{
				const r3dAnimation::r3dAnimInfo& ai = *it;
				if(ai.pAnim->iAnimId == pl->uberAnim_->data_->aid_.prone_down_weapon || ai.pAnim->iAnimId == pl->uberAnim_->data_->aid_.prone_down_noweapon)
				{
					if(!(ai.dwStatus & ANIMSTATUS_Finished))
						stillPlayingAnim = true;
					break;
				}
			}
			if(stillPlayingAnim || pl->proneTime+1 > r3dGetTime())
				playerState = PLAYER_PRONE_DOWN;
			else
			{
				playerState = aiming ? PLAYER_PRONE_AIM : PLAYER_PRONE_IDLE;
				//r3dOutToLog("Time %.2f\n",r3dGetTime() - pl->proneTime);
			}
		}
		else
			playerState = aiming ? PLAYER_PRONE_AIM : PLAYER_PRONE_IDLE;
	}
	else
	{
		if(wasProning)
		{
			playerState = PLAYER_PRONE_UP;
			pl->proneTime = r3dGetTime();
		}
		else if(pl->PlayerState == PLAYER_PRONE_UP) // check if we are still playing anim
		{
			bool stillPlayingAnim = false;
			std::vector<r3dAnimation::r3dAnimInfo>::iterator it;
			for(it=pl->uberAnim_->anim.AnimTracks.begin(); it!=pl->uberAnim_->anim.AnimTracks.end(); ++it) 
			{
				const r3dAnimation::r3dAnimInfo& ai = *it;
				if(ai.pAnim->iAnimId == pl->uberAnim_->data_->aid_.prone_up_weapon || ai.pAnim->iAnimId == pl->uberAnim_->data_->aid_.prone_up_noweapon)
				{
					if(!(ai.dwStatus & ANIMSTATUS_Finished))
						stillPlayingAnim = true;
					break;
				}
			}
			if(stillPlayingAnim || pl->proneTime+1 > r3dGetTime())
				playerState = PLAYER_PRONE_UP;
		}
	}

	if(playerState == PLAYER_PRONE_UP || playerState == PLAYER_PRONE_DOWN)
	{
		disablePlayerMovement = true;
		disablePlayerRotation = true;
	}

	if(proning && aiming)
		disablePlayerMovement = true;

	VMPROTECT_BeginMutation("ProcessPlayerMovement_Accel");	
	{

		//AHNHS_PROTECT_FUNCTION
		r3dPoint3D accelaration(0,0,0);
		if(!disablePlayerMovement)
		{
			// if facing a wall and cannot sprint - stop sprint
			bool canSprint = (shiftWasPressed<3) || (shiftWasPressed>=3 && movingSpeed > 1.0f);
			if(InputMappingMngr->isPressed(r3dInputMappingMngr::KS_SPRINT) || Gamepad->IsPressed(gpLeftShoulder))
				shiftWasPressed++;
			else
				shiftWasPressed = 0;

			if(aiming || pl->m_isHoldingBreath) // cannot spring and aim. also, in default key binding spring and hold breath are on the same key
				shiftWasPressed = 0;

			// due to animation, firstly check left and right movement, so that if you move diagonally we will play moving forward animation
			float thumbX, thumbY;
			Gamepad->GetLeftThumb(thumbX, thumbY);
			if(InputMappingMngr->isPressed(r3dInputMappingMngr::KS_MOVE_LEFT)) 
			{
				accelaration += (aiming)?r3dPoint3D(-GPP->AI_WALK_SPEED*GPP->AI_BASE_MOD_SIDE,0,0):r3dPoint3D(-GPP->AI_RUN_SPEED*GPP->AI_BASE_MOD_SIDE,0,0);
				if (pl->bSwim)
				{
					swimt = r3dGetTime() + 1.0f;
				}
			}
			else if(InputMappingMngr->isPressed(r3dInputMappingMngr::KS_MOVE_RIGHT)) 
			{
				accelaration += (aiming)?r3dPoint3D(GPP->AI_WALK_SPEED*GPP->AI_BASE_MOD_SIDE,0,0):r3dPoint3D(GPP->AI_RUN_SPEED*GPP->AI_BASE_MOD_SIDE,0,0);
				if (pl->bSwim)
				{
					swimt = r3dGetTime() + 1.0f;
				}
			}
			else if(thumbX!=0.0f)
			{
				accelaration += (aiming)?r3dPoint3D(GPP->AI_WALK_SPEED*GPP->AI_BASE_MOD_SIDE*thumbX,0,0):r3dPoint3D(GPP->AI_RUN_SPEED*GPP->AI_BASE_MOD_SIDE*thumbX,0,0);
			}
			else
			{
				swimt = 0.0f;
			}
			//r3dOutToLog("sprint: %d, canSprint: %d, speed: %.3f\n", (int)shiftWasPressed, (int)canSprint, movingSpeed);
			if(shiftWasPressed && canSprint /*&& pl->bOnGround*/ && !crouching && !proning && !disableSprint && (pl->m_Stamina>0.0f) && pl->m_StaminaPenaltyTime<=0 && !pl->bSwim && waterDepth < 1.2f) 
			{
				playerState = PLAYER_MOVE_SPRINT;
				accelaration *= 0.5f; // half side movement when sprinting
				accelaration += r3dPoint3D(0,0,GPP->AI_SPRINT_SPEED);
				accelaration  = accelaration.NormalizeTo() * GPP->AI_SPRINT_SPEED;
			}
			else
			{
				if(InputMappingMngr->isPressed(r3dInputMappingMngr::KS_MOVE_FORWARD) || shiftWasPressed)  // you cheating!
				{
					float spd = GPP->AI_BASE_MOD_FORWARD * (aiming ? GPP->AI_WALK_SPEED : GPP->AI_RUN_SPEED);
					accelaration += r3dPoint3D(0,0,spd);
					accelaration  = accelaration.NormalizeTo() * spd;
					if (pl->bSwim)
					{
						swimt = r3dGetTime() + 1.0f;
					}
				}
				else if(InputMappingMngr->isPressed(r3dInputMappingMngr::KS_MOVE_BACKWARD))
				{
					float spd = GPP->AI_BASE_MOD_BACKWARD * (aiming ? GPP->AI_WALK_SPEED : GPP->AI_RUN_SPEED);
					accelaration += r3dPoint3D(0,0,-spd);
					accelaration  = accelaration.NormalizeTo() * spd;
					if (pl->bSwim)
					{
						swimt = r3dGetTime() + 1.0f;
					}
				}
				else if(thumbY!=0.0f)
				{
					if(thumbY>0)
						accelaration += (aiming)?r3dPoint3D(0,0,GPP->AI_WALK_SPEED*GPP->AI_BASE_MOD_FORWARD*thumbY):r3dPoint3D(0,0,GPP->AI_RUN_SPEED*GPP->AI_BASE_MOD_FORWARD*thumbY);
					else
						accelaration += (aiming)?r3dPoint3D(0,0,GPP->AI_WALK_SPEED*GPP->AI_BASE_MOD_BACKWARD*thumbY):r3dPoint3D(0,0,GPP->AI_RUN_SPEED*GPP->AI_BASE_MOD_BACKWARD*thumbY);
				}
				else
				{
					swimt = 0.0f;
				}
				if(waterDepth > 1.4f)
				{
					pl->bSwim = true;
					if(shiftWasPressed && canSprint && (pl->m_Stamina>0.0f) && pl->m_StaminaPenaltyTime<=0) 
					{
						pl->bSwimShift = true;
						accelaration *= 1.5f; // half side movement when sprinting
						playerState = PLAYER_SWIM_F;
					}
					else
					{
						pl->bSwimShift = false;
					}
				}
				else if (pl->bSwim && waterDepth < 1.2f)
				{
					pl->bSwim = false;
				}

				if (!pl->bSwim) // Fix Bugs Stamina
				{
					pl->bSwimShift = false;
				}
				pl->SyncAnimation(true);
			}
			if (pl->bSwim && InputMappingMngr->isPressed(r3dInputMappingMngr::KS_CROUCH) && !pl->bSwimShift) // slow swiming
			{
				playerState = PLAYER_SWIM;
				accelaration *= 0.5f;
			}
			else if (pl->bSwim && swimt > 2.9f&& !pl->bSwimShift)
			{
				playerState = PLAYER_SWIM_M;
			}
			else if (!pl->bSwim)
			{
				// set walk/run state
				if(playerState != PLAYER_MOVE_SPRINT && !crouching && !proning && (accelaration.x || accelaration.z))
					playerState = aiming ? PLAYER_MOVE_WALK_AIM : PLAYER_MOVE_RUN;
				// set prone walk state
				if(playerState != PLAYER_MOVE_SPRINT && !crouching && proning && (accelaration.x || accelaration.z))
					playerState = aiming ? PLAYER_PRONE_AIM : PLAYER_MOVE_PRONE;
			}

			if((playerState == PLAYER_MOVE_RUN || playerState == PLAYER_MOVE_SPRINT) && (r3dGetTime() < pl->m_SpeedBoostTime))
			{
				accelaration *= pl->m_SpeedBoost;
			}

			if(gUserProfile.ProfileData.isDevAccount && Keyboard->IsPressed(kbsLeftAlt))
			{
				accelaration *= 8.0f;
			}

			//accelaration *= 1.4f;

			if (gUserProfile.ProfileData.isPunisher || gUserProfile.ProfileData.isDevAccount)
			{
				accelaration *= 1.3f;
			}

			// 		STORE_CATEGORIES equippedItemCat = wpn ? wpn->getCategory() : storecat_INVALID;;
			// 		if(equippedItemCat == storecat_SUPPORT || equippedItemCat == storecat_MG)
			// 		accelaration *= 0.8f; // 20% slow down
		}


		if(pl->CurLoadout.Health < GPP->c_fSpeedMultiplier_LowHealthLevel)
			accelaration *= GPP->c_fSpeedMultiplier_LowHealthValue;
		if(pl->CurLoadout.Thirst > GPP->c_fSpeedMultiplier_HighThirstLevel)
			accelaration *= GPP->c_fSpeedMultiplier_HighThirstValue;
		if(pl->CurLoadout.Hunger > GPP->c_fSpeedMultiplier_HighHungerLevel)
			accelaration *= GPP->c_fSpeedMultiplier_HighHungerValue;

		if(crouching)
			accelaration *= 0.4f;
		if(proning)
		{
			accelaration *= 0.2f;
		}

		if (pl->CurLoadout.legfall && !pl->bSwim)
		{
			proning = true;
			crouching = false;
			//pl->bSwim = false;
		}

		if (pl->bSwim && pl->CurLoadout.legfall)
		{
			accelaration *= 0.5f;
		}

		/*if(pl->IsJumpActive()) // don't allow to change direction when jumping
		pl->InputAcceleration = prevAccel;
		else*/
		pl->InputAcceleration = accelaration;

		// process jump after assigning InputAcceleration, so that we can predict where player will jump
		if(!disablePlayerMovement)
		{
			if(pl->bOnGround && (InputMappingMngr->wasPressed(r3dInputMappingMngr::KS_JUMP)||Gamepad->WasPressed(gpA)) 
				&& !crouching 
				&& !proning
				&& !pl->IsJumpActive()
				&& prevAccel.z >= 0 /* prevent jump backward*/
				)
			{
				pl->StartJump();
			}

		}

		if(!editor_debug)
			pl->PlayerState   = playerState;

		pl->PlayerMoveDir = CUberData::GetMoveDirFromAcceleration(pl->InputAcceleration);

	} VMPROTECT_End();	

	// adjust player physx controller size
	// TODO: we need to adjust size only when animation blending was finished! ask Denis how.
	if(crouching != pl->bCrouch || proning!=pl->bProne)
	{
		// GetPosition()/SetPosition() to keep player on the ground.
		// because capsule controller height offset will be changed in AdjustControllerSize()
		r3dPoint3D pos = pl->PhysicsObject->GetPosition();
		if(crouching || proning)
			pl->PhysicsObject->AdjustControllerSize(0.5f, 0.2f, 0.55f);
		else
			pl->PhysicsObject->AdjustControllerSize(0.5f, 1.1f, 1);
		pl->PhysicsObject->SetPosition(pos + r3dPoint3D(0, 0.01f, 0));
	}

	pl->bCrouch = crouching;
	pl->bProne = proning;

	ActiveCameraRigID = (Playerstate_e)pl->PlayerState;
	ActiveCameraRig   = TPSHudCameras[g_camera_mode->GetInt()][ActiveCameraRigID];

	// use this to update the camera from the options. 
	static int currentCameraMode = g_camera_mode->GetInt();

	// if we arn't in the correct view mode currently.   And we are not doing a aim zoom, or the previous lerp is done. 
	if ( ( CurrentState != pl->PlayerState || currentCameraMode != g_camera_mode->GetInt()) && (LerpValue >= 1.0f || ( !pl->m_isAiming) ) )
	{
		currentCameraMode = g_camera_mode->GetInt();
		//set new target
		SourceRig = CurrentRig;
		TargetRig = ActiveCameraRig;

		if(SourceRig.Position.AlmostEqual(TargetRig.Position) && SourceRig.ScopePosition.AlmostEqual(TargetRig.ScopePosition))
		{
			// workaround for a quickscoping and firing at the same time and causing a camera to tilt up.
			if((TPSHudCameras[g_camera_mode->GetInt()][CurrentState].allowScope && !TargetRig.allowScope) || (!TPSHudCameras[g_camera_mode->GetInt()][CurrentState].allowScope && TargetRig.allowScope))
			{
				g_CameraPointToAdj_HasAdjustedVec = false;
			}
		}
		else
		{
			g_CameraPointToAdj_adjVec = R3D_LERP(g_CameraPointToAdj_adjVec, g_CameraPointToAdj_nextAdjVec, LerpValue);
			g_CameraPointToAdj_HasAdjustedVec = false;
		}

		LerpValue = 0;

		CurrentState = (Playerstate_e)pl->PlayerState;	
	}
	else
	{
		// just lerp
		if (LerpValue < 1.0f)
		{
			float lerpMOD = 1.0f;
			/*	STORE_CATEGORIES equippedItemCat = wpn ? wpn->getCategory() : storecat_INVALID;;
			if(TargetRig.allowScope) // slow down aiming for those categories
			if(equippedItemCat == storecat_MG)
			lerpMOD = 0.5f;*/

			LerpValue += r3dGetFrameTime()*6.5f*lerpMOD;
			if (LerpValue >1.0f) LerpValue = 1.0f;

			CurrentRig.Lerp(pl, SourceRig, TargetRig, LerpValue);
		}
		else
			CurrentRig.Lerp(pl, SourceRig, TargetRig, 1.0f);
	}

	if(!disablePlayerRotation)
	{
		/*pl->ViewAngle.x = 5.0f;
		pl->ViewAngle.y = 8.0f;
		pl->ViewAngle.z = 10.0f;*/
		float  glb_MouseSensAdj = CurrentRig.MouseSensetivity * g_mouse_sensitivity->GetFloat();	
		//  Mouse controls are here

		float mmoveX = float(-mMX) * glb_MouseSensAdj;
		float mmoveY = float(-mMY) * glb_MouseSensAdj;

		// fight only vertical recoil, apply adjustment leftover to viewvector
		if(pl->RecoilViewModTarget.y > 0.01f && mmoveY < 0) {
			pl->RecoilViewModTarget.y += mmoveY;
			if(pl->RecoilViewModTarget.y < 0) {
				mmoveY = pl->RecoilViewModTarget.y;
				pl->RecoilViewModTarget.y = 0;
			} else {
				mmoveY = 0;
			}
		}

		pl->ViewAngle.x += mmoveX;
		pl->ViewAngle.y += mmoveY;

		if(Gamepad->IsConnected()) // overwrite mouse
		{
			float X, Y;
			Gamepad->GetRightThumb(X, Y);
			pl->ViewAngle.x += float(-X) * glb_MouseSensAdj * r_gamepad_view_sens->GetFloat();
			pl->ViewAngle.y += float(Y) * glb_MouseSensAdj * r_gamepad_view_sens->GetFloat() * (g_vertical_look->GetBool()?-1.0f:1.0f);
		}

		if(pl->ViewAngle.x > 360.0f ) pl->ViewAngle.x = pl->ViewAngle.x - 360.0f;
		if(pl->ViewAngle.x < 0.0f )   pl->ViewAngle.x = pl->ViewAngle.x + 360.0f;

		// Player can't look too high!
		if(pl->ViewAngle.y > CurrentRig.LookUpLimit )  pl->ViewAngle.y = CurrentRig.LookUpLimit;
		if(pl->ViewAngle.y < CurrentRig.LookDownLimit) pl->ViewAngle.y = CurrentRig.LookDownLimit;

		// set player rotation (except when planting mines)
		pl->m_fPlayerRotationTarget = -pl->ViewAngle.x;

		// calculate player vision
		r3dVector FinalViewAngle = pl->ViewAngle + pl->RecoilViewMod + pl->SniperViewMod;
		if(FinalViewAngle.x > 360.0f ) FinalViewAngle.x = FinalViewAngle.x - 360.0f;
		if(FinalViewAngle.x < 0.0f )   FinalViewAngle.x = FinalViewAngle.x + 360.0f;
		// Player can't look too high!
		if(FinalViewAngle.y > CurrentRig.LookUpLimit )  FinalViewAngle.y = CurrentRig.LookUpLimit;
		if(FinalViewAngle.y < CurrentRig.LookDownLimit) FinalViewAngle.y = CurrentRig.LookDownLimit;

		D3DXMATRIX mr;
		D3DXMatrixRotationYawPitchRoll(&mr, R3D_DEG2RAD(-FinalViewAngle.x), R3D_DEG2RAD(-FinalViewAngle.y), 0);
		pl->m_vVision  = r3dVector(mr._31, mr._32, mr._33);
	}

	if (TargetRig.FXFunc) TargetRig.FXFunc(LerpValue);

	pl->UpdateLocalPlayerMovement();
}

//----------------------------------------------------------------
void TPSGameHUD :: Process()
//----------------------------------------------------------------
{
	if( g_cursor_mode->GetInt() )
	{
		imgui_Update();
		imgui2_Update();
	}

	{
		r3dSetAsyncLoading( 1 ) ;
	}

	obj_Player* pl = gClientLogic().localPlayer_;
	if(!pl) return;

#ifndef FINAL_BUILD

	bool allow_specator_mode = true;
	if(Keyboard->WasPressed(kbsF8) && allow_specator_mode)
	{
		d_video_spectator_mode->SetBool(!d_video_spectator_mode->GetBool());
		static float DOF_NS=0, DOF_NE=0, DOF_FS=0, DOF_FE=0;
		static int DOF_N=0, DOF_F=0, DOF_ENABLE=0;
		if(d_video_spectator_mode->GetBool())
		{
			FPS_vViewOrig.Assign(pl->ViewAngle);
			// save
			DOF_NS=DepthOfField_NearStart;
			DOF_NE=DepthOfField_NearEnd;
			DOF_FS=DepthOfField_FarStart;
			DOF_FE=DepthOfField_FarEnd;
			DOF_N=_NEAR_DOF;
			DOF_F=_FAR_DOF;
			DOF_ENABLE=LevelDOF;
		}
		else
		{
			// restore
			DepthOfField_NearStart=DOF_NS;
			DepthOfField_NearEnd=DOF_NE;
			DepthOfField_FarStart=DOF_FS;
			DepthOfField_FarEnd=DOF_FE;
			_NEAR_DOF=DOF_N;
			_FAR_DOF=DOF_F;
			LevelDOF=DOF_ENABLE;
		}
	}

	bool allow_observer_mode = true;
	allow_observer_mode = false;

	if(Keyboard->WasPressed(kbsF9) && allow_observer_mode)
	{
		d_observer_mode->SetBool(!d_observer_mode->GetBool());
		if(d_observer_mode->GetBool())
		{
			FPS_vViewOrig.Assign(pl->ViewAngle);
		}
	}

	if(d_video_spectator_mode->GetBool() || d_observer_mode->GetBool())
	{
		FPS_Acceleration.Assign(0, 0, 0);

		float  glb_MouseSensAdj = g_mouse_sensitivity->GetFloat();	
		// camera view
		if(Gamepad->IsConnected())
		{
			float X, Y;
			Gamepad->GetRightThumb(X, Y);
			FPS_vViewOrig.x += float(-X) * r_gamepad_view_sens->GetFloat();
			FPS_vViewOrig.y += float(Y) * r_gamepad_view_sens->GetFloat() * (g_vertical_look->GetBool()?-1.0f:1.0f);
		}
		else // mouse fallback
		{
			int mMX=Mouse->m_MouseMoveX, mMY=Mouse->m_MouseMoveY;

			FPS_vViewOrig.x += float(-mMX) * glb_MouseSensAdj;
			FPS_vViewOrig.y += float(-mMY) * glb_MouseSensAdj * (g_vertical_look->GetBool()?-1.0f:1.0f);
		}

		if(FPS_vViewOrig.y > 85)  FPS_vViewOrig.y = 85;
		if(FPS_vViewOrig.y < -85) FPS_vViewOrig.y = -85;

		FPS_ViewAngle = FPS_vViewOrig;

		if(FPS_ViewAngle.y > 360 ) FPS_ViewAngle.y = FPS_ViewAngle.y - 360;
		if(FPS_ViewAngle.y < 0 )   FPS_ViewAngle.y = FPS_ViewAngle.y + 360;


		D3DXMATRIX mr;

		D3DXMatrixIdentity(&mr);
		D3DXMatrixRotationYawPitchRoll(&mr, R3D_DEG2RAD(-FPS_ViewAngle.x), R3D_DEG2RAD(-FPS_ViewAngle.y), 0);

		FPS_vVision  = r3dVector(mr._31, mr._32, mr._33);

		D3DXMatrixIdentity(&mr);
		D3DXMatrixRotationYawPitchRoll(&mr, R3D_DEG2RAD(-FPS_ViewAngle.x), 0, 0);
		FPS_vRight = r3dVector(mr._11, mr._12, mr._13);
		FPS_vUp    = r3dVector(0, 1, 0);
		FPS_vForw  = r3dVector(mr._31, mr._32, mr._33);

		FPS_vForw.Normalize();
		FPS_vRight.Normalize();
		FPS_vVision.Normalize();

		// walk
		extern float __EditorWalkSpeed;
		float fSpeed = __EditorWalkSpeed;

		float mult = 1;
		if(Keyboard->IsPressed(kbsLeftShift)) mult = d_spectator_fast_move_mul->GetFloat();
		if(Keyboard->IsPressed(kbsLeftControl)) mult = d_spectator_slow_move_mul->GetFloat();

		if(Keyboard->IsPressed(kbsW)) FPS_Acceleration.Z = fSpeed;
		if(Keyboard->IsPressed(kbsS)) FPS_Acceleration.Z = -fSpeed * 0.7f;
		if(Keyboard->IsPressed(kbsA)) FPS_Acceleration.X = -fSpeed * 0.7f;
		if(Keyboard->IsPressed(kbsD)) FPS_Acceleration.X = fSpeed * 0.7f;
		if(Keyboard->IsPressed(kbsQ)) FPS_Position.Y    += SRV_WORLD_SCALE(1.0f)* r3dGetFrameTime() * mult;
		if(Keyboard->IsPressed(kbsE)) FPS_Position.Y    -= SRV_WORLD_SCALE(1.0f)* r3dGetFrameTime() * mult;

		if(Gamepad->IsConnected())
		{
			float RX, RY, TL, TR;
			Gamepad->GetLeftThumb(RX, RY);
			Gamepad->GetTrigger(TL, TR);

			FPS_Acceleration.Z = -RY*r_gamepad_move_speed->GetFloat();
			FPS_Acceleration.Z = RY*r_gamepad_move_speed->GetFloat();
			FPS_Acceleration.X = -RX*r_gamepad_move_speed->GetFloat();
			FPS_Acceleration.X = RX*r_gamepad_move_speed->GetFloat();
			FPS_Position.Y    += r3dGetFrameTime() * TR * r_gamepad_move_speed->GetFloat();
			FPS_Position.Y    -= r3dGetFrameTime() * TL * r_gamepad_move_speed->GetFloat();
		}

		FPS_Position += FPS_vVision * FPS_Acceleration.Z * r3dGetFrameTime() * mult;
		FPS_Position += FPS_vRight * FPS_Acceleration.X * r3dGetFrameTime() *mult;

		return;
	}
#endif
	ProcessPlayerMovement(pl, false);
}