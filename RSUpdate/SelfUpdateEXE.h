#pragma once

extern	const char* getShellExecuteVerb();

extern	bool	selfUpd_StartUpdate(const BYTE* data, long size);
extern	bool	selfUpd_TestIfUpdated();

extern	bool	g_IsExeUpdated;