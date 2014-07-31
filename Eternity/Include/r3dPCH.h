#ifndef	__ETERNITY_R3DPCH_H
#define	__ETERNITY_R3DPCH_H

#pragma warning(disable: 4201) // warning C4201: nonstandard extension used : nameless struct/union

// stdio
#define _CRT_SECURE_NO_WARNINGS	1
#define _CRT_NONSTDC_NO_WARNINGS 1
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_NONSTDC_NO_DEPRECATE 1

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include <assert.h>
#include <memory>

// RJH Network2: added the following preprocessor defines for the network, 
// since the game project is compiled for precompiled headers and this pch
// is everywhere, ensures that the winsock header part of windows.h
// is included.
#define _WIN32_WINNT 0x0500
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#ifndef STRICT
#define STRICT 1
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#define NOMINMAX
#include <windows.h>
#include <process.h> /* _beginthread, _endthread */
#include <mmsystem.h>
#include <winsock2.h>
#include <errno.h>
#include <shlobj.h>

// stl
#include <set>
#include <vector>
#include <queue>
#include <list>
#include <map>
#include <string>
#include <sstream>
#include <hash_map>
#include <unordered_map>
#include <unordered_set>

// starsafe
#define STRSAFE_NO_DEPRECATE 1
#define STRSAFE_NO_CCH_FUNCTIONS 1
#include <tchar.h>
#include <strsafe.h>

// for assert
#include <crtdefs.h>

// D3D9
#define DIRECTINPUT_VERSION	0x0800
#include <dinput.h>
#ifdef _DEBUG
#define D3D_DEBUG_INFO 1
#endif
#include <d3d9.h>
#include <d3dx9core.h>
#include <d3dx9.h>

#include "r3dCompileAssert.h"

// disable PIX events in final build
#ifdef FINAL_BUILD
#undef D3DPERF_BeginEvent
#undef D3DPERF_EndEvent
#undef D3DPERF_SetMarker
#undef D3DPERF_SetRegion
#undef D3DPERF_QueryRepeatFrame

#define D3DPERF_BeginEvent(col, wszName)
#define D3DPERF_EndEvent()
#define D3DPERF_SetMarker(col, wszName)
#define D3DPERF_SetRegion(col, wszName)
#define D3DPERF_QueryRepeatFrame()
#endif

#ifdef _MSC_VER
#define OVERRIDE override
#define R3D_FORCEINLINE __forceinline
#else
#define OVERRIDE
#define R3D_FORCEINLINE inline 
#endif

#ifdef FINAL_BUILD
#define DISABLE_PROFILER 1
#define DISABLE_CONSOLE 1
#define DISABLE_BUDGETER 1
#else
#define DISABLE_PROFILER 0
#define DISABLE_CONSOLE 0
#define DISABLE_BUDGETER 0
#endif


#define APEX_ENABLED 0

#ifdef FINAL_BUILD
#define VEHICLES_ENABLED 1
//#define TPGLIB_ENABLED 1
#define HS_ENABLED 1
#else 
#ifndef WO_SERVER
#define VEHICLES_ENABLED 1 // temp disabled due to new PhysX API changes
#else
#define VEHICLES_ENABLED 0
#endif
#endif

#ifndef FINAL_BUILD
#define ENABLE_AUTODESK_NAVIGATION 1
#else
#define ENABLE_AUTODESK_NAVIGATION 0
#endif

#ifdef FINAL_BUILD
#define R3D_ALLOW_TEMPORAL_SSAO 0
#else
#define R3D_ALLOW_TEMPORAL_SSAO 0
#endif

#ifndef DISABLE_PHYSX  // DISABLE_PHYSX defined for server
#include "PxPhysicsAPI.h"
#include "CharacterKinematic/PxControllerManager.h"
using namespace physx;
#endif

#define ENABLE_RAGDOLL 1

#ifndef WO_SERVER 
#ifndef FINAL_BUILD
#define ENABLE_WEB_BROWSER 1
#endif
#endif

// memory debug, disable define to turn off memory tracking
#ifndef ENABLE_MEMORY_DEBUG
#define ENABLE_MEMORY_DEBUG 1
#endif

#ifdef _DEBUG
#if ENABLE_MEMORY_DEBUG
#include <crtdbg.h>
#define _CRTDBG_MAP_ALLOC
#define DEBUG_NEW new( _CLIENT_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif //ENABLE_MEMORY_DEBUG
#endif

// string copy functions
R3D_FORCEINLINE char* r3dscpy_s(char* a, int len, const char* b)
{
	StringCbCopyA(a, len, b);
	return a;
}

// buffer character copy template
template<int CCH> R3D_FORCEINLINE char* r3dscpy(char (&a)[CCH], const char* b)
{
	const int len = CCH;
	r3dscpy_s(a, len, b);
	return a;
}

// genetic template
template<typename T> R3D_FORCEINLINE T r3dscpy(T a, const char *b);
// normal char* specialization
template<> R3D_FORCEINLINE char* r3dscpy(char* a, const char *b)
{
	return strcpy(a, b);
}

// WIDE CHAR
R3D_FORCEINLINE wchar_t* r3dscpy_s(wchar_t* a, int size_in_bytes, const wchar_t* b)
{
	StringCbCopyW(a, size_in_bytes, b);
	return a;
}

// buffer character copy template
template<int CCH> R3D_FORCEINLINE wchar_t* r3dscpy(wchar_t (&a)[CCH], const wchar_t* b)
{
	const int len = CCH * 2; // need to be in bytes
	r3dscpy_s(a, len, b);
	return a;
}

// genetic template
template<typename T> R3D_FORCEINLINE T r3dscpy(T a, const wchar_t *b);
// normal char* specialization
template<> R3D_FORCEINLINE wchar_t* r3dscpy(wchar_t* a, const wchar_t *b)
{
	return wcscpy(a, b);
}


#endif	//__ETERNITY_R3DPCH_H
