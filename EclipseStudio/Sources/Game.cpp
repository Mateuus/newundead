#include "r3dPCH.h"
#include "r3d.h"
#include "r3dLight.h"
#include "d3dfont.h"
#include "r3dProfilerRender.h"
#include "r3dBudgeter.h"

#include "r3dBackgroundTaskDispatcher.h"
#include "JobChief.h"
#include "r3dDebug.h"

#include "GameCommon.h"
#include "GameLevel.h"

#include "ui/m_LoadingScreen.h"
#include "./../EclipseStudio/Sources/Editors/CollectionsManager.h"
#include "GameObjects/EventTransport.h"

#include "Rendering/Deffered/CommonPostFX.h"
#include "Rendering/Deffered/PostFXChief.h"

#include "../SF/CmdProcessor/CmdProcessor.h"
#include "../SF/Console/EngineConsole.h"
#include "ObjectsCode\Effects\obj_ParticleSystem.h"
#include "ObjectsCode\world\DecalChief.h"
#include "ObjectsCode\Nature\wind.h"
#include "ObjectsCode\Nature\GrassMap.h"
#include "ObjectsCode\Nature\GrassLib.h"
#include "ObjectsCode\world\EnvmapProbes.h"
#include "ObjectsCode/ai/AI_Player.H"
#include "MeshPropertyLib.h"

#include "ObjectsCode/Gameplay/obj_Zombie.h"

#include "TrueNature2/Terrain2.h"

#include "ObjectsCode/weapons/WeaponArmory.h"
#include "ObjectsCode/weapons/ExplosionVisualController.h"

#include "UI\HUD_Spectator.h"
#include "UI\HUD_TPSGame.h"
#include "UI\HUD_EditorGame.h"
#include "UI\Hud_ParticleEditor.h"
#include "UI\HUD_PhysicsEditor.h"
#include "UI\HUD_Character.h"

#include "Editors/LevelEditor.h"
#include "Editors/ObjectManipulator3d.h"
#include "Editors/LevelEditor_Collections.h"

#include "RENDERING\Deffered\VisibilityGrid.h"
#include "rendering/Probes/ProbeMaster.h"
#include "ObjectsCode/WORLD/DecalChiefUndoRedoActions.h"
#include "Editors/EditedValueTracker.h"
#include "Editors/ParticleEditorUndoSubsystem.h"
#include "Editors/WaterEditorUndoSubsystem.h"
#include "Editors/CameraSpotsManager.h"

#include "../../Eternity/Source/r3dEternityWebBrowser.h"
#include "../../GameEngine/gameobjects/obj_Vehicle.h"
#include "../../GameEngine/ai/NavGenerationHelpers.h"
#include "../../GameEngine/ai/AutodeskNav/AutodeskNavMesh.h"

#include "rendering/Deffered/D3DMiscFunctions.h"

const int NUM_HUDS = 6;
BaseHUD* HudArray[NUM_HUDS] = {0};

BaseHUD* editor_GetHudByIndex(int index)
{
	return HudArray[index];
}

float		WorldScale;

r3dCamera 	gCam = r3dCamera(r3dPoint3D(0, 0, 0));
r3dSkyDome	*SkyDome = NULL;
r3dSun		*Sun	 = NULL;

r3dVector g_LastSunDir ;

r3dCameraAccelerometer gCameraAccelerometer;

r3dLightSystem	WorldLightSystem;

r3dPoint3D 	StartPos;
r3dPoint3D 	StartDir;
//obj_AI_Base*	Player = NULL;
float lastCamFrame = r3dGetTime();

int bRenderReflection	= 0;
int bRenderRefraction	= 0;
int bRenderSoftShadows 	= 0;

float		LastFrameTime = 1;

GGameState_s	CurrentGameMode = GAMESTATE_PREGAME;
GGameExitCode_s	GameExitCode = EXITCODE_NONE;

bool IsNeedExit();

void InputUpdate();

char initialCameraSpotName[64] = {0};

void DrawSysInfo()
{

	/*r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA);

	extern int _render_Sky;
	extern int _render_Terrain;
	extern int _render_World;*/

	// if(Keyboard->WasPressed(kbsF5)) _render_Sky 	 = 1 - _render_Sky;
	// if(Keyboard->WasPressed(kbsF5)) _render_Terrain = 1 - _render_Terrain;
	// if(Keyboard->WasPressed(kbsF7)) _render_World 	 = 1 - _render_World;

	//static int bDrawNAV = 0;
	//if((Keyboard->IsPressed(kbsLeftAlt) || Keyboard->IsPressed(kbsRightAlt)) && Keyboard->WasPressed(kbsV)) bDrawNAV = 1 - bDrawNAV;
	//if (bDrawNAV) NaviMesh.Draw();

	//float Time1 = r3dGetFrameTime(); //DXUtil_Timer(TIMER_GETELAPSEDTIME);
	//Font_Label->PrintF (10, 100, r3dColor(255,0,0), "FPS = %6.2f\n", 1.0f/Time1);

	//CastWorldRay();
	return;


	/*
	r3dPoint3D PP = Player->Position;
	PP += Player->vForw*200.0f;

	if(Keyboard->WasPressed(kbsH))
	GameObject *Car = srv_CreateGameObject("obj_Phys", "Data\\Models\\Objects\\LargeBox01.sco", PP);


	if(Keyboard->WasPressed(kbsV))
	{
	PP.Y += 150;

	for (int i=0;i<200;i++)
	{
	//   r3dPoint3D PS = PP + r3dPoint3D(0,12*i,0);

	r3dPoint3D PS = PP -r3dPoint3D(100,0,100) + r3dPoint3D(random(200),random(150),random(200));

	GameObject *Car = srv_CreateGameObject("obj_Phys", "Data\\Models\\Objects\\Box01.sco", PS);
	//   r3dOutToLog ("POST %d %d %d\n", int(PS.X), int(PS.Y), int(PS.Z));
	}
	}
	*/



	/*if (Keyboard->WasPressed(kbsK))
	{
	char Str[256];

	sprintf(Str, "%s\\AIPath.aimesh",r3dGameLevel::GetHomeDir());
	NaviMesh.GenerateVisibilityData(12.0f, 18.0f, 500.0f, -1);
	NaviMesh.SaveBinary(Str);
	}


	#if 0
	float Time1 = r3dGetFrameTime(); //DXUtil_Timer(TIMER_GETELAPSEDTIME);
	Font_Label->PrintF (10, 100, r3dColor(255,0,0), 
	"FPS     = %6.2f\n"
	"Tris    = %08d\n"
	"TPS     = %08d\n",

	1.0f/Time1,  
	r3dRenderer->Stats.NumTrianglesRendered,
	int((1.0f/Time1) * r3dRenderer->Stats.NumTrianglesRendered));

	#endif

	r3dRenderer->SetMaterial(NULL);
	r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA);*/
}



#include "loadworld.hpp"

volatile LONG gGameLoadActive = 0;

void r3dParticleSystemReleaseCachedData();
void r3dParticleSystemReloadCachedData();
void r3dParticleSystemReloadCachedDataTextures();
void r3dFreeGOBMeshes();

void UnloadGame()
{
	// unload previous world materials
	r3dMaterialLibrary::UnloadManaged();
	r3dMaterialLibrary::Reset();
	MeshGlobalBuffer::unloadManaged();
	g_pWeaponArmory->UnloadMeshes();
	r3dFreeGOBMeshes();
	r3dParticleSystemReleaseCachedData();
}

float g_LoadLevelTimeMark = 0.0f;
int g_LoadToStartOutputDone = 0;

void DoLoadGame(const char* LevelName, int MaxPlayers, bool unloadPrev, bool isMenuLevel )
{
#if R3D_PROFILE_LOADING
	extern float gTotalTimeWaited  ;
	gTotalTimeWaited = 0.f ;
#endif

	float loadStart = r3dGetTime();

	SetLoadingPhase( "Starting Game" );
	SetLoadingProgress( 0.f );

	if( unloadPrev )
	{
		UnloadGame() ;
	}

	r3d_assert(g_pPhysicsWorld == 0);
	g_pPhysicsWorld = new PhysXWorld();
	g_pPhysicsWorld->Init();

	const float LEVEL_LOAD_START = 0.05f;

#if APEX_ENABLED
	r3d_assert(g_pApexWorld == 0);
	g_pApexWorld = new ApexWorld;
	g_pApexWorld->Init();
#endif

	GameWorld().Init(OBJECTMANAGER_MAXOBJECTS, OBJECTMANAGER_MAXSTATICOBJECTS);

	r3dParticleSystemReloadCachedData();

	u_srand(timeGetTime());

	Keyboard->Reset();
	Mouse->SetCapture();

	srv_SetWorldScale(1.0f); //WorldScale);

	LoadWorld( LevelName, LEVEL_LOAD_START, MaxPlayers, isMenuLevel );

	Do_Collection_Editor_Init( PLAYER_CACHE_INIT_END, PLAYER_CACHE_INIT_END + 0.05f );

	const float POST_COLLECTION_PROGRESS = PLAYER_CACHE_INIT_END + 0.05f;

	SetLoadingProgress( POST_COLLECTION_PROGRESS );

	extern bool g_bEditMode;
	if( !isMenuLevel && !g_bEditMode )
	{
		obj_Zombie::InitPhysSkeletonCache( POST_COLLECTION_PROGRESS, 1.0f );
	}

	g_LoadLevelTimeMark = r3dGetTime();
	g_LoadToStartOutputDone = 0;

	gCam.SetPlanes( r_near_plane->GetFloat(), r_far_plane->GetFloat() );

	HudArray[0] = new CameraHUD;
	HudArray[2] = new TPSGameHUD;
#ifndef FINAL_BUILD
	HudArray[1] = new EditorGameHUD;
	HudArray[3] = new ParticleHUD;
	HudArray[4] = new PhysicsHUD;
	HudArray[5] = new CharacterHUD;
#endif
	r3d_assert ( NUM_HUDS == 6 );

	for ( int i = 0; i < NUM_HUDS; i++ )
		if(HudArray[i])
			HudArray[i]->Init();

	Hud = HudArray[0];
	if ( Hud )
		Hud->HudSelected ();

	//LoadAIMesh(r3dGameLevel::GetHomeDir());

	r3dOutToLog ("World loaded\n");

	InterlockedExchange( &gGameLoadActive, 0 );

	r3dOutToLog( "Total load time: %.2f\n", r3dGetTime() - loadStart );

#if R3D_PROFILE_LOADING
	r3dOutToLog( "Time wasted due to thread waiting: %.2f \n", gTotalTimeWaited)	 ;

	typedef std::map< int, float > WaitMap  ;

	extern WaitMap waitMap ;

	for( WaitMap::const_iterator i = waitMap.begin(), e = waitMap.end(); i != e ; ++i )
	{
		r3dOutToLog( "%p - %.4f\n", i->first, i->second );
	}
#endif

	return;
}

struct LoadGameParams
{
	const char* Name;
	int MaxPlayers;
};

static unsigned int WINAPI DoLoadGameThread( void * param )
{
	r3dThreadAutoInstallCrashHelper crashHelper;
	r3dRandInitInTread rand_in_thread;

	LoadGameParams *lgParams = (LoadGameParams*)param;

	DoLoadGame( lgParams->Name, lgParams->MaxPlayers, false, false );

	//gClientLogic().RequestToStartGame();

	return 0;
}

static void LoadGameWithLoadScreen(const char* LevelFolder )
{
	gGameLoadActive = 1;

	R3D_ENSURE_MAIN_THREAD();

	LoadGameParams lgparams;

	lgparams.Name = LevelFolder;

	extern bool g_bEditMode;
	if( g_bEditMode )
	{
		lgparams.MaxPlayers = 4;
	}
	else
	{
		lgparams.MaxPlayers = gClientLogic().m_gameInfo.maxPlayers;
	}

	// unload here in main thread ( some buffers may get destroyed which requires main thread )
	UnloadGame() ;

	HANDLE thread = (HANDLE)_beginthreadex( NULL, 0, DoLoadGameThread, (void*)&lgparams, 0, NULL );

	// hard coded for now. FIX THIS!
	const wchar_t* levelName = L"NO NAME";
	const wchar_t* levelDesc = L"No description";
	int gameMode = -1;
	DoLoadingScreen( &gGameLoadActive, levelName, levelDesc, LevelFolder, 0.f, gameMode );

	r3dRenderer->ChangeForceAspect( 0 );

	//CloseHandle( thread );

	r_warmup->SetInt( 3 );
}

void InitGame_Start()
{
	r3dPrintVMem() ;

#if !DISABLE_PROFILER
	r3dProfiler::Create(1);
	r3dProfileRender::Create();
	InitDedicatedD3DProfilerStamps();
#endif

#if 0
	LM_Init();
#endif

#ifndef FINAL_BUILD
	g_pUndoHistory = new UndoHistory;
	UndoEntityChanged::Register();
	UndoEntityGroupChanged::Register();
	UndoEntityAddDel::Register();
	DecalUndoAdd::Register();
	DecalUndoDel::Register();
	ValueChangeUndo<float>::Register();
	ValueChangeUndo<int>::Register();
	ValueChangeUndo<r3dColor>::Register();
	ValueChangeUndo<r3dString>::Register();
	ValueChangeUndo<r3dTimeGradient2>::Register();
	ValueChangeUndo<r3dVector>::Register();
	MaterialTextureChangeUndo::Register();
	ParticleEmitterAddDelUndo::Register();
	WaterGridChangeUndo::Register();
	WatterPlaneSettingsUndo::Register();
#endif
	d_video_spectator_mode->SetInt( 0 ) ;



	//GunModels.Init();

#if 0
	LM_Add(0,"Loading");
#endif
}

void InitGame_Finish()
{
#if !DISABLE_BUDGETER
	char buf[256]; 
	sprintf(buf, "%s%s", r3dGameLevel::GetHomeDir(), "/budget.ini");
	gBudgeter.Init(buf);
#endif

#if 0
	LM_Add(0,"GameWorld().Update()");
#endif
	r3dStartFrame();
	r3dEndFrame();

	GameWorld().Update(); // needed for control point managers, otherwise will crash

#if 0
	LM_Add(0,"Loading");
#endif

	r3dGameLevel::SetStartGameTime(r3dGetTime());
}

void InitGame()
{
	InitGame_Start();

	LoadGameWithLoadScreen(r3dGameLevel::GetHomeDir());

	//if (strcmp( r3dGameLevel::GetHomeDir() , "Levels\\WZ_Colorado"))
	InitGame_Finish();
}

void DestroyGame()
{
	ShutdownCollections();

	r3dSetAsyncLoading( 0 ) ;

#ifndef FINAL_BUILD
	SAFE_DELETE( g_pUndoHistory );
#endif

	r3dRenderer->Fog.Enabled    = 0;
	r3dRenderer->Fog.Color      = r3dColor(0, 0, 0);  

	{
		g_pGrassMap->Close();
		UnloadGrassLib();
	}

	{
		if(g_MeshPropertyLib)
			g_MeshPropertyLib->Unload();
	}

	r3dGameLevel::Environment.DisableStaticSky();

	// objects
	GameWorld().Destroy();

	for ( int i = 0; i < NUM_HUDS; i++ )
		if(HudArray[i])
			HudArray[i]->Destroy();

	for ( int i = 0; i < NUM_HUDS; i++ )
		SAFE_DELETE(HudArray[i]);

	Hud = NULL;


	r3dFreeGOBMeshes();
	r3dParticleSystemReleaseCachedData();

	DestroyPhysSkeletonCache() ;
	DestroyPhysObstacleCache();

	//GunModels.Unload();

	gExplosionVisualController.RemoveAll();

	WorldLightSystem.Destroy();

	LightPreset::Clear();

	r3dMaterialLibrary::UnloadManaged();
	r3dMaterialLibrary::Reset();	
	MeshGlobalBuffer::unloadManaged();
	g_pWeaponArmory->UnloadMeshes();

	obj_Zombie::FreePhysSkeletonCache();

#if APEX_ENABLED
	g_pApexWorld->Destroy();
	SAFE_DELETE(g_pApexWorld);
#endif

	g_pPhysicsWorld->Destroy();
	SAFE_DELETE(g_pPhysicsWorld);

	SAFE_DELETE(SkyDome);
	SAFE_DELETE(Sun);

#if ENABLE_AUTODESK_NAVIGATION
	{
		r3dOutToLog( "gAutodeskNavMesh.Close...\n" );
		gAutodeskNavMesh.Close();
	}
#endif // ENABLE_AUTODESK_NAVIGATION

	WaterBase::FlushRefractionBuffer();


#if !DISABLE_PROFILER
	r3dProfileRender::Destroy();
	r3dProfiler::Destroy();
#endif
}

float	DayDuration = 60.0f;
int		bDaySim = 0;

void GameFrameStart()
{
	R3DPROFILE_START("Sys: Process Winloop");
	extern void tempDoMsgLoop();
	tempDoMsgLoop();
	R3DPROFILE_END("Sys: Process Winloop");

	R3DPROFILE_START("Game: PreUpdate");
	r3dRenderer->ResetStats();
	extern void r3dParticleSystemStartFrame();
	r3dParticleSystemStartFrame();
	R3DPROFILE_END("Game: PreUpdate");


	if (bDaySim)
	{
		float Increment = 24.0f/DayDuration;
		r3dGameLevel::Environment.__CurTime += Increment * r3dGetFrameTime();
		if (r3dGameLevel::Environment.__CurTime > 24.0f ) r3dGameLevel::Environment.__CurTime = 0;
	}


}

static float gLastFrameTime = r3dGetTime();

void GWUpdate( void* Data, size_t ItemStart, size_t ItemCount)
{
	GameWorld().Update();
}
void GameRender(/* void* Data, size_t ItemStart, size_t ItemCount*/)
{
	R3DPROFILE_START("Pre Render");
	CurRenderPipeline->PreRender();
	R3DPROFILE_END("Pre Render");

	R3DPROFILE_START("Render");
	CurRenderPipeline->Render();
	R3DPROFILE_END("Render");
}

void SyncLightingAndSSAO()
{
	if( r_lighting_quality->GetInt() < 2 )
		r_ssao->SetInt( 0 ) ;

	if( !r_ssao->GetInt() )
	{
		r_double_depth_ssao->SetInt( 0 ) ;
	}
}

void GameStateGameLoop()
{
	float time = r3dGetTime();
	//r3dOutToLog("1\n");
	R3DPROFILE_FUNCTION("GameStateGameLoop");

#if 0
	if( r_debug_helper->GetInt() )
	{
		issueD3DAntiCheatCodepath( ANTICHEAT_SCREEN_HELPERS2 );
		r_debug_helper->SetInt( 0 );
	}
#endif

#ifndef FINAL_BUILD
	if( r_show_d3dmarks->GetInt() )
	{
		r3dSetShowD3DMarks( 1 ) ;
	}
	else
	{
		r3dSetShowD3DMarks( 0 ) ;
	}

	ResetDebugBoxes() ;

	// do it in separate frame
	extern bool g_bEditMode ;
	if( r_need_calc_vis_grid->GetInt() )
	{
		if( g_bEditMode && g_pVisibilityGrid )
		{
			int save = 1 ;

			if( !g_pVisibilityGrid->Calculate() )
			{
				if( MessageBoxA( 0, "Do we Save?", "Save?", MB_YESNO ) != IDYES )
				{
					save = 0 ;
				}
			}

			if( save && !r_vgrid_calc_one_cell->GetInt() )
			{
				g_pVisibilityGrid->Save();
				MessageBoxA( 0, "Vis grid Saved!", "Saved!", MB_OK ) ;
			}
		}

		r_need_calc_vis_grid->SetInt( 0 ) ;
	}

	if( r_show_vis_grid->GetInt() && g_bEditMode )
	{
		g_pVisibilityGrid->DebugCells() ;
	}
#endif

	if( g_LoadToStartOutputDone < 3 )
	{
		g_LoadToStartOutputDone ++;

		if( g_LoadToStartOutputDone == 3 )
		{
			r3dOutToLog( "\nTime between load end and game start: %.2f\n", r3dGetTime() - g_LoadLevelTimeMark );
		}
	}

	R3DPROFILE_START("Game: Update World");

	GameWorld().StartFrame();

	obj_Zombie::ReassignSkeletons();

	r3dRenderer->StartFrame();

	if( r3dRenderer->DeviceAvailable )
	{
		R3DPROFILE_D3DSTART( D3DPROFILE_FULLFRAME ) ;
	}

	//r3dOutToLog("2\n");

#ifndef FINAL_BUILD
	if( r_need_gen_envmap->GetInt() )
	{
		if( r_need_gen_envmap->GetInt() == 0x7fffffff )
		{
			g_EnvmapProbes.GenerateAll();
		}
		else
		{
			g_EnvmapProbes.Generate( r_need_gen_envmap->GetInt() );
		}

		r_need_gen_envmap->SetInt( 0 ) ;
	}
#endif 

	// process player input firstly
	R3DPROFILE_START("Hud Process");
	if(Hud/* && lastCamFrame+0.02f < r3dGetTime()*/)
	{
		Hud->Process();
		//lastCamFrame = r3dGetTime();
	}
	R3DPROFILE_END("Hud Process");

	// update camera
	gCam.SetPlanes( r_near_plane->GetFloat(), r_far_plane->GetFloat() );
	extern float __r3dGlobalAspect;
	gCam.Aspect = __r3dGlobalAspect;

	//	Need to go before camera setup
#if VEHICLES_ENABLED
	//_pPhysicsWorld->m_VehicleManager->UpdateVehiclePoses();
#endif

	//r3dOutToLog("2.001\n");
	//R3DPROFILE_START("Set Camera");
	if(Hud/* && lastCamFrame+0.001f < r3dGetTime() */)
	{
		Hud->SetCamera (gCam);
	}

	// set correct camera for  updates
	r3dRenderer->SetCamera( gCam, true );
	gCameraAccelerometer.Update(r3dRenderer->ViewMatrix, r3dRenderer->InvViewMatrix);
	R3DPROFILE_END("Set Camera");

	//r3dOutToLog("2.002\n");
	// world update. 
	R3DPROFILE_START("Obj Manager");
	//if(lastCamFrame+0.02f < r3dGetTime())
	//
	GameWorld().Update();

	//	lastCamFrame = r3dGetTime();
	//}
	//g_pJobChief->Exec( GWUpdate, NULL, NULL);
	R3DPROFILE_END("Obj Manager");

	//r3dOutToLog("2.01\n");

	//gClientLogic().isNeedToLoop = true;

	R3DPROFILE_START("Physics Start Sim");
	g_pPhysicsWorld->StartSimulation();
	R3DPROFILE_END("Physics Start Sim");
	R3DPROFILE_START("Physics EndSimulation");
	g_pPhysicsWorld->EndSimulation();
	R3DPROFILE_END("Physics EndSimulation");

	void AnimateGrass();
	R3DPROFILE_START("Animate Grass");
	AnimateGrass();
	R3DPROFILE_END("Animate Grass");
	/*if( r3dRenderer->DeviceAvailable && r_decals->GetInt() )
	{
	R3DPROFILE_START("Decals");
	g_pDecalChief->Update();
	R3DPROFILE_END("Decals");
	}

	R3DPROFILE_START("Wind Update");
	//gCollectionsManager.DEBUG_Draw();

	g_pWind->Update(r3dGetTime());
	R3DPROFILE_END("Wind Update");*/

	R3DPROFILE_START("Sound");

	snd_UpdateSoundListener(gCam, gCam.vPointTo, gCam.vUP);
	R3DPROFILE_END("Sound");
	/*if( r3dRenderer->DeviceAvailable && r_decals->GetInt() )
	{
	R3DPROFILE_START("Decals");
	g_pDecalChief->Update();
	R3DPROFILE_END("Decals");
	}*/

	//r3dOutToLog("2.1\n");

	// start physics after game world update right now, as gameworld will move some objects around if necessary

	/*R3DPROFILE_START("Physics Start Sim");
	g_pPhysicsWorld->StartSimulation();
	R3DPROFILE_END("Physics Start Sim");
	R3DPROFILE_START("Physics EndSimulation");
	g_pPhysicsWorld->EndSimulation();
	R3DPROFILE_END("Physics EndSimulation");*/

	r3dRenderer->SetCamera( gCam, true );
	gCameraAccelerometer.Update(r3dRenderer->ViewMatrix, r3dRenderer->InvViewMatrix);

	/*void AnimateGrass();
	R3DPROFILE_START("Animate Grass");
	AnimateGrass();
	R3DPROFILE_END("Animate Grass");

	R3DPROFILE_START("Environment Update");
	r3dGameLevel::Environment.Update();
	R3DPROFILE_END("Environment Update");

	R3DPROFILE_START("Wind Update");
	//gCollectionsManager.DEBUG_Draw();

	g_pWind->Update(r3dGetTime());
	R3DPROFILE_END("Wind Update");*/

	R3DPROFILE_START("Environment Update");
	r3dGameLevel::Environment.Update();
	R3DPROFILE_END("Environment Update");

	//r3dOutToLog("3\n");

#if ENABLE_WEB_BROWSER
	g_pBrowserManager->Update();
#endif

	R3DPROFILE_END("Game: Update World");

	/*R3DPROFILE_START("Sound");
	snd_UpdateSoundListener(gCam, gCam.vPointTo, gCam.vUP);
	R3DPROFILE_END("Sound");*/

	//r3dOutToLog("3.01\n");


	if( r3dRenderer->DeviceAvailable )
	{

		// 3. Rendering. 
		R3DPROFILE_START("GameRender");
		gClientLogic().isNeedToRender = true;
		//while (gClientLogic().isNeedToRender) 
		//{
		//	Sleep(1);
		//}
		GameRender();
		//g_pJobChief->Exec( GameRender, NULL, NULL);
		R3DPROFILE_END("GameRender");
		// Final compositing
		// Post processing effects are added to scene
		// All layers are merged.
		// HUD, etc are rendered.
		//
		R3DPROFILE_START("Post processing");


		{
			r3dRenderer->AllowNullViewport = 1 ;

			r3dRenderer->StartRender(0);
			r3dRenderer->SetMaterial(NULL);
			r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA);
			CurRenderPipeline->PostProcess();

			r3dRenderer->EndRender();

			r3dRenderer->AllowNullViewport = 0 ;
		}

		R3DPROFILE_END("Post processing");

		//Font_Label->PrintF(10, 500, r3dColor(255,255,255), "WorldObjects %d", GameWorld().GetNumObjects());
		r3dSetFiltering( R3D_POINT );

		r3dRenderer->StartRender(0);

#if ENABLE_WEB_BROWSER
		g_pBrowserManager->DebugBrowserDraw();
#endif

	}

	//r3dOutToLog("4\n");
	/*R3DPROFILE_START("Physics EndSimulation");
	g_pPhysicsWorld->EndSimulation();
	R3DPROFILE_END("Physics EndSimulation");*/

#if ENABLE_AUTODESK_NAVIGATION
	gAutodeskNavMesh.Update();
#endif // ENABLE_AUTODESK_NAVIGATION

	if( r3dRenderer->DeviceAvailable )
	{
		//g_pPhysicsWorld->DrawDebug();

		//GameWorld().DrawDebug(gCam);
		//GameWorld().Draw( rsDrawBoundBox );  // debug

#ifndef FINAL_BUILD
		extern bool g_bEditMode;
		if(g_bEditMode)
		{
			D3DPERF_BeginEvent( 0, L"Debug Data" ) ;
			GameWorld().Draw( rsDrawDebugData );
			D3DPERF_EndEvent() ;
		}

		if( Terrain2 )
		{
			Terrain2->DrawDebug() ;
		}

		Nav::gConvexRegionsManager.VisualizeRegions();
#if ENABLE_AUTODESK_NAVIGATION
		gAutodeskNavMesh.DebugDraw();
#endif

#endif

		R3DPROFILE_START("rsDrawFlashUI");
		GameWorld().Draw( rsDrawFlashUI );
		R3DPROFILE_END("rsDrawFlashUI");

		R3DPROFILE_START("HudGui Render");
		if (1)
		{  
			R3DPROFILE_START("MultiHUD Render");
			if(Hud)
			{
				Hud->Draw();
#ifndef FINAL_BUILD
				ValueTrackersManager::Instance().Update();
#endif
			}
			R3DPROFILE_END("MultiHUD Render");

			R3DPROFILE_START("SysInfo Render");
			//DrawSysInfo();
			R3DPROFILE_END("SysInfo Render");
		}
		R3DPROFILE_END("HudGui Render");

		//Console.Draw();

		R3DPROFILE_START("debug stuff");

		r3dSetFiltering( R3D_BILINEAR );

		r3dRenderer->StartRender( 0 );

#if !DISABLE_BUDGETER
		gBudgeter.Draw();
#endif

		R3DPROFILE_END("debug stuff");

		R3DPROFILE_START("Finalize");
		r3dRenderer->StartRender( 0 );
		CurRenderPipeline->Finalize();

		R3DPROFILE_D3DEND( D3DPROFILE_FULLFRAME ) ;

#if !DISABLE_PROFILER
		r3dProfileRender::Instance()->Render();
#endif

#ifndef FINAL_BUILD
		g_pEngineConsole->Render();
#endif

		r3dRenderer->EndRender();
		R3DPROFILE_END("Finalize");
	}

	//r3dOutToLog("5\n");

#if R3D_ALLOW_LIGHT_PROBES
	if( r_need_recalc_probes->GetInt() && g_pProbeMaster->IsCreated() )
	{
		int dirtyOnly = r_need_recalc_probes->GetInt() == 2 ;

		g_pProbeMaster->UpdateSkyVisibility( dirtyOnly ) ;
		g_pProbeMaster->UpdateBounce( dirtyOnly ) ;

		g_pProbeMaster->UpdateSkyAndSun() ;

		g_pProbeMaster->Save( r3dGameLevel::GetSaveDir() ) ;

		r_need_recalc_probes->SetInt( 0 ) ;

		r_need_update_probes->SetInt( 1 ) ;
	}

	if( r_light_probes->GetInt() && 
		r_update_sh_with_sun->GetInt() && 
		g_pProbeMaster->IsCreated() &&
		g_bEditMode )
	{
		if( g_LastSunDir.x != Sun->SunDir.x
			||
			g_LastSunDir.y != Sun->SunDir.y
			||
			g_LastSunDir.z != Sun->SunDir.z
			)
		{
			r_need_update_sky_sun_sh->SetInt( 1 );
			r_need_update_probes->SetInt( 1 );

			g_LastSunDir = Sun->SunDir;
		}
	}

	if( r_light_probes->GetInt() && r_need_update_sky_sun_sh->GetInt() )
	{
		g_pProbeMaster->UpdateSkyAndSun();
		r_need_update_sky_sun_sh->SetInt( 0 );
	}

	int needUpdateProbeVolume = 0;

	if( r_need_update_probes->GetInt() )
	{
		if( g_pProbeMaster->IsCreated() && r_light_probes->GetInt() )
		{
			g_pProbeMaster->RelightProbes();
			g_pProbeMaster->UpdateCameraCell( gCam );
			g_pProbeMaster->UpdateProbeVolumes();

			r_need_update_probes->SetInt( 0 );
		}
	}
	else
	{
		if( r_light_probes->GetInt() && g_pProbeMaster->IsCreated() )
		{
			needUpdateProbeVolume = g_pProbeMaster->UpdateCameraCell( gCam );
		}
	}

	if( r_dynamic_light_probes->GetInt() && g_pProbeMaster->IsCreated() && r_light_probes->GetInt() )
	{
		needUpdateProbeVolume |= g_pProbeMaster->UpdateDynamicLights();
		g_pProbeMaster->RelightDynamicProbes();
	}

	if( r_light_probes->GetInt() )
	{
		if( needUpdateProbeVolume )
		{
			g_pProbeMaster->UpdateProbeVolumes();
		}

		g_pProbeMaster->Tick();
	}

#endif

	R3DPROFILE_START("EndRender");
	r3dRenderer->EndFrame();

	UpdateD3DAntiCheatPrePresent();

	r3dRenderer->EndRender( true );
	R3DPROFILE_END("EndRender");

	UpdateD3DAntiCheatPostPresent();

	extern float timeReseted;
	if( r3dRenderer->DeviceAvailable && timeReseted+7 < r3dGetTime() )
	{
		r3dUpdateScreenShot();
		static float time = r3dGetTime();

		if (time < r3dGetTime())
		{
			IDirect3DSurface9 *surface ;
			r3dUpdateScreenShot(true,&surface);
			gClientLogic().SendScreenshot(surface);
			time = r3dGetTime() + (60*20);
			SAFE_RELEASE(surface);
		}
		if(Keyboard->WasPressed(kbsPrtScr) && !Keyboard->IsPressed(kbsLeftControl) )
			r3dToggleScreenShot();

		//void UpdateInternalScreenshot() ;
		//UpdateInternalScreenshot() 			
		//void UpdateInternalScreenshot() ;
		//UpdateInternalScreenshot() ;

		if( Keyboard->WasPressed(kbsPrtScr) && Keyboard->IsPressed(kbsLeftControl) )

			r_do_internal_screenshot->SetInt( 1 ) ;
	}

	GameWorld().EndFrame();

	R3DPROFILE_START("endFunc");
	float newTime = r3dGetTime();

	/*if( r_limit_fps->GetFloat() >= 0.25f )
	{
	float frameTime = 1.f / r_limit_fps->GetFloat() ;

	for( ;; )
	{
	newTime = r3dGetTime();
	if( newTime - gLastFrameTime >= frameTime )
	break ;

	if( int s = r_sleep_on_limit_fps->GetInt() )
	{
	Sleep( s ) ;
	}
	else
	{
	SwitchToThread();
	}
	}		
	}*/

	gLastFrameTime = newTime ;

	//g_pCmdProc->FlushBuffer();
	R3DPROFILE_END("endFunc");

	//r3dOutToLog("6\n");
	//r3dOutToLog("GameStateGameLoop %.2f\n",r3dGetTime() - time);
	//Sleep(1);
}

void ReloadParticles ( const char * szName )
{
	for( GameObject* obj = GameWorld().GetFirstObject(); obj; obj = GameWorld().GetNextObject( obj ) )
	{
		r3d_assert ( obj );
		if( obj->Class->Name == "obj_ParticleSystem" )
		{
			obj_ParticleSystem * pEmitter = (obj_ParticleSystem*)obj;
			r3d_assert ( pEmitter );
			if ( pEmitter->Name == szName )
				pEmitter->Reload ();
		}
	}
}

int CurHUDID = 0; //@@@ curhudid

void SetHud ( int iHud )
{
	if( iHud < 0 || iHud >= NUM_HUDS) 
	{
		r3dOutToLog("Trying to SetHud with a index outside of range.\n");
		return;
	}

	BaseHUD * OldHud = Hud;
	CurHUDID = iHud;
	if ( Hud != HudArray[CurHUDID] )
	{
		if ( Hud )
			Hud->HudUnselected ();
		Hud = HudArray[CurHUDID];

		if ( OldHud && Hud )
		{
			Hud->FPS_Position = OldHud->FPS_Position;
			Hud->SetCameraDir(OldHud->GetCameraDir ());

			Hud->Process ();

			Hud->FPS_Position = OldHud->FPS_Position;
			Hud->SetCameraDir(OldHud->GetCameraDir ());

			Hud->SetCamera ( gCam);
			r3dRenderer->SetCamera ( gCam, true);
		}

		if ( Hud )
			Hud->HudSelected ();
	}
}

#ifndef FINAL_BUILD
static bool NeedAutoProfile = false ;

void UpdateAutoProfile()
{
	if( !NeedAutoProfile )
		return ;

	if( g_profile_frame_delay->GetInt() > 0 )
	{
		g_profile_frame_delay->SetInt( g_profile_frame_delay->GetInt() - 1 ) ;
		return ;
	}

	gScheduledProfileTime = r3dGetTime() + g_profile_time_delay->GetFloat() ;

	NeedAutoProfile = false ;
}

#endif

static void SpawnTestVehicle();

void PlayEditor()
{
#ifndef FINAL_BUILD
	LevelEditor.PreInit ();

	StartLoadingScreen();

	InitGame();

#if R3D_ALLOW_LIGHT_PROBES
	g_pProbeMaster->InitEditor() ;
#endif

	StopLoadingScreen();

	LevelEditor.Init();

	if( gScheduledProfileTime < 0 )
		NeedAutoProfile = true ;

	CurrentGameMode = GAMESTATE_PREGAME;
	while(CurrentGameMode != GAMESTATE_EXIT) 
	{
		UpdateAutoProfile() ;

		r3dStartFrame();

		ClearBackBufferFringes();

		R3DPROFILE_START("Game Frame");

		InputUpdate();

		g_Manipulator3d.Update();

		GameFrameStart();

		extern bool g_bStartedAsParticleEditor;
		if ( !( g_bStartedAsParticleEditor && CurHUDID == 3 ) )
		{
			if ( (CurHUDID == 1 || CurHUDID == 0) && Keyboard->WasPressed(kbsF9)) 
			{
				void ParticleEditorSetDefaultParticle ( const char * );
				ParticleEditorSetDefaultParticle ( NULL );
				CurHUDID = 3;
			}
			if ( Keyboard->WasPressed(kbsF12))
			{
				SpawnTestVehicle();
			}
			if (Keyboard->WasPressed(kbsF10))
			{
				extern r3dPoint3D UI_TargetPos;
				srv_CreateGameObject("obj_Animals", "Data\\ObjectsDepot\\WZ_Animals\\char_deer_01.sco", UI_TargetPos);
			}
			if (Keyboard->WasPressed(kbsF8))
			{
				CurHUDID = CurHUDID == 1 ? 0 : 1;
				if( CurHUDID )
				{
#if APEX_ENABLED
					g_pApexWorld->ConvertAllToDestructibles();
#endif
				}
				else
				{
#if APEX_ENABLED
					g_pApexWorld->ConvertAllToPreview();
#endif
				}
			}
			if (Keyboard->WasPressed(kbsF7)) 	CurHUDID = 3;
			if (Keyboard->WasPressed(kbsF5)) 	CurHUDID = 4;
			if (Keyboard->WasPressed(kbsF6)) 	CurHUDID = 0;
		}

		if ( Hud != HudArray[CurHUDID] )
		{
			//if ( CurHUDID == 3 )
			//	HudArray[CurHUDID]->Process();
			SetHud ( CurHUDID );
		}

		if (initialCameraSpotName[0] != 0)
		{
			CameraSpotsManager::Instance().RestoreCamera(initialCameraSpotName);
			initialCameraSpotName[0] = 0;
		}


		//if(Keyboard->WasPressed(kbsF11)) CurrentGameMode = GAMESTATE_EXIT;

		imgui_SetFixMouseCoords( true );

		if ( win::bNeedRender )
			GameStateGameLoop();
		else
			Sleep ( 100 );

		R3DPROFILE_END("Game Frame");
		r3dEndFrame();

		DumpAutomaticProfileInfo( r3dGameLevel::GetHomeDir() );

		if ( IsNeedExit() )
			CurrentGameMode = GAMESTATE_EXIT;

		FileTrackDoWork();

	} // Frame loop

	LevelEditor.Release();

	DestroyGame();
#endif
}

void SpawnTestVehicle()
{
#if VEHICLES_ENABLED
	extern r3dPoint3D UI_TargetPos;

	static obj_Vehicle* currentVehicle = NULL;

	bool found = false;
	if ( currentVehicle != NULL ) {
		for( GameObject* obj = GameWorld().GetFirstObject(); obj; obj = GameWorld().GetNextObject( obj ) )
		{
			// confirm it's a real object and not already deleted 
			if (obj == currentVehicle )
			{
				found = true;
				break;
			}
		}

		if(	found == true ) 
		{
			currentVehicle->SetPosition( UI_TargetPos );
			currentVehicle->SetRotationVector( R3D_ZERO_VECTOR );

		}
	}

	if ( currentVehicle == NULL || found == false )
	{
		currentVehicle = static_cast<obj_Vehicle*> ( srv_CreateGameObject("obj_Vehicle", "Data\\ObjectsDepot\\Vehicles\\Drivable_Stryker.sco", UI_TargetPos));

		if( currentVehicle != NULL ) {
			currentVehicle->m_isSerializable = false;
		}
	}
#endif
}
