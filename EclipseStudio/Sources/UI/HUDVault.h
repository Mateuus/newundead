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


    void    addClientSurvivor(const wiCharDataFull& slot);
    void    addBackpackItems(const wiCharDataFull& slot);
    void    updateInventoryAndSkillItems();
    void    updateSurvivorTotalWeight(const wiCharDataFull& slot);
    void    reloadBackpackInfo();


    static    unsigned int WINAPI as_BackpackFromInventoryThread(void* in_data);
    void        OnBackpackFromInventorySuccess();
    static    unsigned int WINAPI as_BackpackFromInventorySwapThread(void* in_data);
    void        OnBackpackFromInventorySwapSuccess();
    static    unsigned int WINAPI as_BackpackToInventoryThread(void* in_data);
    void        OnBackpackToInventorySuccess();
    static    unsigned int WINAPI as_BackpackGridSwapThread(void* in_data);
    void        OnBackpackGridSwapSuccess();


    UIItemInventory itemInventory_;
};