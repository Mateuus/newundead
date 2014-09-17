#pragma once


#include "APIScaleformGfx.h"
#include "UIItemInventory.h"


#include "FrontEndShared.h"
#include "../GameCode/UserProfile.h"


#include "UIAsync.h"


class HUDVault
{
    bool    isActive_;
    bool    isInit_;


public: 
    HUDVault();
    ~HUDVault();


    bool Init();
    bool Unload();


    bool IsInited() const { return isInit_; }


    void Update();
    void Draw();


    bool isActive() const { return isActive_; }
    void Activate();
    void Deactivate();


    void addCategories();
    void addTabTypes();
    void addItems();


    void eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
    void eventBackpackFromInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
    void eventBackpackToInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
    void eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);


private:
    UIAsync<HUDVault> async_;
    r3dScaleformMovie gfxMovie;
    r3dScaleformMovie* prevKeyboardCaptureMovie;


    __int64    m_inventoryID;
    int        m_gridTo;
    int        m_gridFrom;
    int        m_Amount;
    int        m_Amount2;
	int		   ItemID;
	int		   Var1;
	int		   Var2;
	int IDInventory[2048];
	int ItemsOnInventory[2048];
	int Var_1[2048];
	int Var_2[2048];
	int Quantity[2048];
	//int Durability[2048];
	DWORD Session;
	int LastInventory;

	int ItemsData[2048];
	int ItemsItem[2048];
	int ItemsVar1[2048];
	int ItemsVar2[2048];
	int ItemsQuantity[2048];

	wiInventoryItem* wi1;

	int SwapGridFrom;
	int SwapGridTo;

	float BackPackToInv;
	bool OnBackpackToInventorySuccess;

	float BackPackFromInv;
	bool OnBackpackFromInventorySuccess;

	float BackPackGridSwap;
	bool OnBackpackGridSwapSuccess;

	bool	BackPackFULL(int ItemID, int Var1);
	bool	isverybig(int Quantity, int Item, int grid);
    void    addClientSurvivor(const wiCharDataFull& slot);
    void    addBackpackItems(const wiCharDataFull& slot);
    void    updateInventoryAndSkillItems();
    void    updateSurvivorTotalWeight();
    void    reloadBackpackInfo();

    UIItemInventory itemInventory_;
};