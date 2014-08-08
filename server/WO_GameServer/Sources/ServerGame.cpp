#include "r3dPCH.h"
#include "r3d.h"
#include "r3dLight.h"
#include "d3dfont.h"

#include "GameObjects/ObjManag.h"

#include "../EclipseStudio/Sources/GameLevel.h"
#include "../EclipseStudio/Sources/rendering/Deffered/RenderDeffered.h"
#include "../../GameEngine/TrueNature/Terrain.h"
#include "../../GameEngine/gameobjects/PhysXWorld.h"
#include "../../GameEngine/ai/AutodeskNav/AutodeskNavMesh.h"
#include "../EclipseStudio/Sources/Editors/CollectionsManager.h"

#include "ServerGameLogic.h"
#include "MasterServerLogic.h"
#include "KeepAliveReporter.h"

// just some crap to get server to link
	CD3DFont* 		Font_Label;
	r3dLightSystem WorldLightSystem;
	r3dCamera gCam ;
	float SunVectorXZProj = 0;
	class r3dSun * Sun;
	int CurHUDID = -1;
	float		LakePlaneY = 101.0;

	r3dScreenBuffer* gBuffer_Depth;
	r3dScreenBuffer* ScreenBuffer;
	r3dScreenBuffer* gBuffer_Color;
	r3dScreenBuffer *gScreenSmall;
	r3dScreenBuffer * DepthBuffer;
	float* ShadowSplitDistances;
	DWORD DriverUpdater(HWND hParentWnd, DWORD VendorId, DWORD v1, DWORD v2, DWORD v3, DWORD v4, DWORD hash) { return hash; }
	void r3dScaleformBeginFrame() {}
	void r3dScaleformEndFrame() {}
	void SetNewSimpleFogParams() {}
	int g_bForceQualitySelectionInEditor = 0;
	void AdvanceLoadingProgress(float) {}
	void SetAdvancedFogParams() {}
	void SetVolumeFogParams() {}
	float ShadowSplitDistancesOpaqueHigh[NumShadowSlices+1] = {0};
	float *ShadowSplitDistancesOpaque = &ShadowSplitDistancesOpaqueHigh[0];
	ShadowMapOptimizationData gShadowMapOptimizationDataOpaque [ NumShadowSlices ];
	ShadowMapOptimizationData gShadowMapOptimizationData [ NumShadowSlices ];
	r3dCameraAccelerometer gCameraAccelerometer;
	#include "MeshPropertyLib.h"
	MeshPropertyLib * g_MeshPropertyLib = NULL;
	MeshPropertyLib::Entry* MeshPropertyLib::GetEntry( const string& key ) { return NULL; }
	void MeshPropertyLib::AddEntry( const string& key ) { r3dError("not implemented"); }
	void MeshPropertyLib::AddEntry( const string& category, const string& meshName ) { r3dError("not implemented"); }
// end of temp variables

	ObjectManager	ServerDummyWorld;

/*static unsigned int WINAPI NetworkThread( void * param )
{
	while(1)
	{
	::Sleep(1);
	gServerLogic.net_->Update();
	}
}*/
void PlayGameServer()
{
  r3d_assert(gServerLogic.ginfo_.IsValid());
  switch(gServerLogic.ginfo_.mapId) 
  {
    default: 
      r3dError("invalid map id\n");
      break;
    case GBGameInfo::MAPID_Editor_Particles: 
      r3dGameLevel::SetHomeDir("WorkInProgress\\Editor_Particles"); 
      break;
    case GBGameInfo::MAPID_ServerTest:
      r3dGameLevel::SetHomeDir("WorkInProgress\\ServerTest");
      break;
    case GBGameInfo::MAPID_WZ_Colorado:
      r3dGameLevel::SetHomeDir("WZ_Colorado");
      break;
    case GBGameInfo::MAPID_WZ_CaliWood:
      r3dGameLevel::SetHomeDir("WZ_CaliWood");
      break;
    case GBGameInfo::MAPID_WZ_Valley:
      r3dGameLevel::SetHomeDir("WZ_Valley");
      break;
    case GBGameInfo::MAPID_WZ_Deserto:
      r3dGameLevel::SetHomeDir("WZ_Deserto");
      break;
  }

  r3dResetFrameTime();
  
  GameWorld_Create();

  u_srand(timeGetTime());
  GameWorld().Init(OBJECTMANAGER_MAXOBJECTS, OBJECTMANAGER_MAXSTATICOBJECTS);
  ServerDummyWorld.Init(OBJECTMANAGER_MAXOBJECTS, OBJECTMANAGER_MAXSTATICOBJECTS);
  
  g_pPhysicsWorld = new PhysXWorld;
  g_pPhysicsWorld->Init();

  r3dTerrain::SetNeedShaders( false );
  LoadLevel_Objects( 1.f );

  gCollectionsManager.Init( 0, 1 );

  r3dOutToLog( "NavMesh.Load...\n" );
  gAutodeskNavMesh.Init();

  r3dResetFrameTime();
  GameWorld().Update();
  ServerDummyWorld.Update();

  r3dOutToLog("Spawning Vehicles... Count:%d\n",GameWorld().spawncar);
  //gServerLogic.SpawnNewCar();

  r3dGameLevel::SetStartGameTime(r3dGetTime());
  
  r3dOutToLog("server main loop started\n");
  r3dResetFrameTime();
  gServerLogic.OnGameStart();
  
  gKeepAliveReporter.SetStarted(true);
  
 // _beginthreadex ( NULL, 0, NetworkThread, NULL, 0, NULL );
  while(1) 
  {
    ::Sleep(20);		// limit to 100 FPS

	char text[64] ={0};
	sprintf(text,"WarZ Game Server [Running..] id:%d curPeer:%d players:%d/%d",gServerLogic.ginfo_.gameServerId,gServerLogic.curPeersConnected,gServerLogic.curPlayers_,gServerLogic.ginfo_.maxPlayers);
	SetConsoleTitle(text);

	gServerLogic.net_->Update();

    r3dEndFrame();
    r3dStartFrame();
    
    //if(GetAsyncKeyState(VK_F1)&0x8000) r3dError("r3dError test");
    //if(GetAsyncKeyState(VK_F2)&0x8000) r3d_assert(false && "r3d_Assert test");
    gKeepAliveReporter.Tick(gServerLogic.curPlayers_);

    gServerLogic.Tick();
	gServerLogic.UpdateNetId();
    gMasterServerLogic.Tick();
    
    if(gMasterServerLogic.IsMasterDisconnected()) {
      r3dOutToLog("Master Server disconnected, exiting\n");
      gKeepAliveReporter.SetStarted(false);
      return;
    }

    GameWorld().StartFrame();
    GameWorld().Update();
    // fire few ticks of temporary world update, just for safety (physics init and stuff)
    static int TempWorldUpdateN = 0;
    if(TempWorldUpdateN < 20) {
      TempWorldUpdateN++;
      ServerDummyWorld.Update();
    }

    // start physics after game world update right now, as gameworld will move some objects around if necessary
    g_pPhysicsWorld->StartSimulation();
    g_pPhysicsWorld->EndSimulation();

#if ENABLE_AUTODESK_NAVIGATION
    const float t1 = r3dGetTime();
    gAutodeskNavMesh.Update();
    const float t2 = r3dGetTime() - t1;

    bool showSpd = (GetAsyncKeyState('Q') & 0x8000) && (GetAsyncKeyState('E') & 0x8000);
    bool showKys = false; //(GetAsyncKeyState('Q') & 0x8000) && (GetAsyncKeyState('R') & 0x8000);

    extern int _zstat_NumZombies;
    extern int _zstat_NavFails;
    extern int _zstat_Disabled;
    if(showKys) {
      gAutodeskNavMesh.perfBridge.Dump();
      r3dOutToLog("NavMeshUpdate %f sec, z:%d, bad:%d, f:%d\n", t2, _zstat_NumZombies, _zstat_Disabled, _zstat_NavFails);
    }
    if(showSpd) {
      r3dOutToLog("NavMeshUpdate %f sec, z:%d, bad:%d, f:%d\n", t2, _zstat_NumZombies, _zstat_Disabled, _zstat_NavFails);
    }
#ifndef _DEBUG    
    if((t2 > (1.0f / 60.0f))) {
      r3dOutToLog("!!!! NavMeshUpdate %f sec, z:%d, bad:%d, f:%d\n", t2, _zstat_NumZombies, _zstat_Disabled, _zstat_NavFails);
      //gAutodeskNavMesh.perfBridge.Dump();
      //r3dOutToLog("!!!! NavMeshUpdate %f sec, z:%d, bad:%d, f:%d, navPend:%d\n", t2, _zstat_NumZombies, _zstat_Disabled, _zstat_NavFails, _zstat_UnderProcess);
    }
#endif
#endif // ENABLE_AUTODESK_NAVIGATION

    GameWorld().EndFrame();
    
    if(gServerLogic.gameFinished_)
      break;
  }

  // set that we're finished
  gKeepAliveReporter.SetStarted(false);

  gMasterServerLogic.FinishGame();
  
  gServerLogic.DumpPacketStatistics();
  
  gCollectionsManager.Destroy();

  GameWorld_Destroy();
  ServerDummyWorld.Destroy();
  
  return;
}
