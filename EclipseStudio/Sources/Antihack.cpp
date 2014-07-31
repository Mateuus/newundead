// ----------------------------------------------------
//	AntiHack
//	by coloserver2009@hotmail.com
// ----------------------------------------------------
#include "AntiHack.h"
#include <windows.h>
#include <string.h>
#include <cstdlib>

int webb (void) 
{    
	system ("start http://antihack.colo-server.com/antihacklogAllrightWarZ/inject.php");    
	return 0;
}


BOOLEAN BlockAPI (HANDLE,CHAR *,CHAR *); 
DWORD g_dwLoadLibraryAJMP;


DWORD WINAPI jumphook( DWORD AddressToPerformJump, DWORD AddressOfMyFunction, DWORD LenghOfTheAreaToPerformTheJump	)
{
	if( LenghOfTheAreaToPerformTheJump < 5 )
		return 0;

DWORD RelativeJump, 
		  NextInstructionAddress,
		  Flag;

if ( ! VirtualProtect((LPVOID)AddressToPerformJump, LenghOfTheAreaToPerformTheJump, PAGE_EXECUTE_READWRITE, &Flag) )
		return 0;

NextInstructionAddress = AddressToPerformJump + LenghOfTheAreaToPerformTheJump;

*(BYTE*)AddressToPerformJump = 0xE9;

for( DWORD i = 5; i < LenghOfTheAreaToPerformTheJump; i++)
		*(BYTE*)(AddressToPerformJump+i) = 0x90;

RelativeJump = AddressOfMyFunction - AddressToPerformJump - 0x5;

*(DWORD*)(AddressToPerformJump + 0x1) = RelativeJump;

VirtualProtect((LPVOID)AddressToPerformJump, LenghOfTheAreaToPerformTheJump, Flag, &Flag);

return NextInstructionAddress; 
}


HMODULE WINAPI hLoadLibraryA( LPCSTR lpLibFileName )
{	
	__asm
	{
		mov eax, dword ptr ss:[esp + 0x18]
		cmp dword ptr ds:[eax-0x12], 0x8B55FF8B
		je erro
	}
	
if( lpLibFileName )
	{
		if( !strcmp( lpLibFileName, "twain_32.dll" ) )
			__asm jmp g_dwLoadLibraryAJMP
	}			

return LoadLibraryExA( lpLibFileName, 0, 0 );

erro:
webb();
ExitProcess(0);

return 0;
}

void ZPerformHooks()
{
	g_dwLoadLibraryAJMP = (DWORD)GetModuleHandleA( "kernel32" ) + 0x6E2A1;              
	jumphook( (DWORD)LoadLibraryA, (DWORD)&hLoadLibraryA, 57 );

}


#ifdef Q_PROTECT
using namespace std;
typedef void (*HackCheck)();
BOOL system()
{
   HackCheck _HackCheck;
   HINSTANCE hInstLibrary = LoadLibrary("Guard/guard.dll");

   if (hInstLibrary)
   {
      _HackCheck = (HackCheck)GetProcAddress(hInstLibrary,"AllRZefrdjkhqdos");
      _HackCheck();
   }
   else
   {
	    MessageBoxA(0,"Antihack file not found !!","[AntiHack System]",MB_OK | MB_ICONSTOP);
	    ExitProcess(-1);
   }
   return true;
}
#endif



void runantihack()
{

 	ZPerformHooks();
	system();

}

