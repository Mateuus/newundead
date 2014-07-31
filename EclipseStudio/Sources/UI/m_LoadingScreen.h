#ifndef LOADINGSCREEN_H
#define LOADINGSCREEN_H

#include "UIMenu.h"

#include "multiplayer/ClientGameLogic.h"

class LoadingScreen : public UIMenu
{
private:
	r3dTexture*	m_pBackgroundTex;
	bool m_RenderingDisabled;

	Scaleform::GFx::Value m_GFX_Main;

	wchar_t* m_MapName;
	wchar_t* m_MapDesc;
	wchar_t* m_TipOfTheDay;

	void	updateUIDataText(const wchar_t* name, const wchar_t* desc, const wchar_t* tip);
public:

	LoadingScreen( const char * movieName );
	virtual ~LoadingScreen();

	void	eventRegisterUI(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

	virtual bool Initialize() OVERRIDE;
	virtual int Update() OVERRIDE;

	void SetLoadingTexture(const char* ImagePath);
	void SetData( const char* ImagePath, const wchar_t* Name, const wchar_t* Message, const wchar_t* tip_of_the_day );
	void SetProgress( float progress );
	void SetRenderingDisabled( bool disabled ) { m_RenderingDisabled = disabled; }

};

void StartLoadingScreen( );
void DisableLoadingRendering( );
void StopLoadingScreen( );
void SetLoadingTexture(const char* ImagePath);
void SetLoadingProgress( float progress );
void AdvanceLoadingProgress( float add );
float GetLoadingProgress();
void SetLoadingPhase( const char* Phase );
int DoLoadingScreen( volatile LONG* loading, const wchar_t* LevelName, const wchar_t* LevelDescription, const char* LevelFolder, float TimeOut, int gameMode );

template <typename T>
int DoConnectScreen( T* Logic, bool (T::*CheckFunc)(), const wchar_t* Message, float TimeOut );

int DoConnectScreen( volatile LONG* loading, const wchar_t* Message, float TimeOut );

#define PROGRESS_LOAD_LEVEL_START 0.033f
#define PROGRESS_LOAD_LEVEL_END 0.8f
#define PLAYER_CACHE_INIT_END 0.85f

#endif  //LOADINGSCREEN_H
