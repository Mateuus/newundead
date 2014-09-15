#pragma once

#include "APIScaleformGfx.h"
#include "UIItemInventory.h"

#include "FrontEndShared.h"
#include "../GameCode/UserProfile.h"

#include "UIAsync.h"

class HUDSafeLock
{
	bool	isActive_;
	bool	isInit_;

public: 
	HUDSafeLock();
	~HUDSafeLock();
	int SafeLockID2;
	float SafeWeight;
	char dateforexpire[512];
	char Pass[512];
	int CustomerID;
	bool Init();
	bool Unload();

	bool IsInited() const { return isInit_; }

	void Update();
	void Draw();

	bool isActive() const { return isActive_; }
	bool SafeLockisFull(int InventoryID);
	void AddDate(const char* Date);
	void EndSesion();
	bool BackPackFULL(int InventoryID);
	bool isverybig(int Quantity, int Item, int grid);
	void Activate();
	void Deactivate();
	void AddSafeLock(int Safe, const char* Password);
	void addCategories();
	void addTabTypes();
	void addItems();

	void eventChangeKeyCode(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBackpackFromInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBackpackToInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventPickupLockbox(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

private:
	UIAsync<HUDSafeLock> async_;
	r3dScaleformMovie gfxMovie;
	r3dScaleformMovie* prevKeyboardCaptureMovie;

	__int64	m_inventoryID;
	int		m_gridTo;
	int		m_gridFrom;
	int		m_Amount;
	int		m_Amount2;
	int SwapGridFrom;
	int SwapGridTo;
	char Password[512];

	uint32_t SafelockID;
	uint32_t ItemID;
	uint32_t ExpireTime;
	uint32_t MapID;
	uint32_t Quantity;
	int Var1;
	int Var2;
	float BackPackToInv;
	bool OnBackpackToInventorySuccess;

	float BackPackFromInv;
	bool OnBackpackFromInventorySuccess;

	float BackPackGridSwap;
	bool OnBackpackGridSwapSuccess;

	void	addClientSurvivor(const wiCharDataFull& slot);
	void	addBackpackItems(const wiCharDataFull& slot);
	void	updateInventoryAndSkillItems();
	void	updateSurvivorTotalWeight();
	void	reloadBackpackInfo();

	//void		OnBackpackToInventorySuccess();

	//static	unsigned int WINAPI as_BackpackFromInventoryThread(void* in_data);
	//void		OnBackpackFromInventorySuccess();
	//static	unsigned int WINAPI as_BackpackFromInventorySwapThread(void* in_data);
	//void		OnBackpackFromInventorySwapSuccess();
	//static	unsigned int WINAPI as_BackpackToInventoryThread(void* in_data);
	//
	//static	unsigned int WINAPI as_BackpackGridSwapThread(void* in_data);
	//void		OnBackpackGridSwapSuccess();

	UIItemInventory itemInventory_;
};