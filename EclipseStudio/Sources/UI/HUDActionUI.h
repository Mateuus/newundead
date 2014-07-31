#pragma once

#include "APIScaleformGfx.h"

class HUDActionUI
{
	bool	isActive_;
	bool	isInit;

private:
	r3dScaleformMovie gfxMovie;
public:
	HUDActionUI();
	~HUDActionUI();

	bool 	Init();
	bool 	Unload();

	bool	IsInited() const { return isInit; }

	void 	Update();
	void 	Draw();
	void	setScreenPos(int x, int y);

	bool	isActive() const { return isActive_; }
	void	Activate();
	void	Deactivate();
 void showEBlock(bool var, bool show);
void setCarInfo(int var1 , int var2 , int var3 ,int var4, int var5 , bool show);
	void	setText(const wchar_t* title, const wchar_t* msg, const char* letter);
	void	enableRegularBlock();
	void	enableProgressBlock();
	void	setProgress(int value); // [0,100]
};
