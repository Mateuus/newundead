#pragma once

#include "APIScaleformGfx.h"

class HUDAttachments
{
	bool	isActive_;
	bool	isInit;

private:
	r3dScaleformMovie gfxMovie;

public:
	HUDAttachments();
	~HUDAttachments();

	bool 	Init();
	bool 	Unload();

	bool	IsInited() const { return isInit; }

	void 	Update();
	void 	Draw();

	bool	isActive() const { return isActive_; }
	void	Activate();
	void	Deactivate();

	int ItemIdAdded[72];

	void AddItemIdAdded(int ItemId);

	bool isItemAdded(int ItemId);

	void ResetItemId();

	void	eventSelectAttachment(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
};
