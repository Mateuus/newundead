#pragma region VAULT_IS_WORKING
#include "r3dPCH.h"
#include "r3d.h"


#include "r3dDebug.h"


#include "HUDVault.h"


#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/ai/AI_Player.h"


#include "../multiplayer/MasterServerLogic.h"
#include "../multiplayer/ClientGameLogic.h"


extern const char* getReputationString(int Reputation);


HUDVault::HUDVault() :
isActive_(false),
isInit_(false),
prevKeyboardCaptureMovie(NULL)
{
}


HUDVault::~HUDVault()
{
}


void HUDVault::addClientSurvivor(const wiCharDataFull& slot)
{
    Scaleform::GFx::Value var[22];
    char tmpGamertag[128];
    if(slot.ClanID != 0)
        sprintf(tmpGamertag, "[%s] %s", slot.ClanTag, slot.Gamertag);
    else
        r3dscpy(tmpGamertag, slot.Gamertag);
    var[0].SetString(tmpGamertag);
    var[1].SetNumber(slot.Health);
    var[2].SetNumber(slot.Stats.XP);
    var[3].SetNumber(slot.Stats.TimePlayed);
    var[4].SetNumber(slot.Hardcore);
    var[5].SetNumber(slot.HeroItemID);
    var[6].SetNumber(slot.HeadIdx);
    var[7].SetNumber(slot.BodyIdx);
    var[8].SetNumber(slot.LegsIdx);
    var[9].SetNumber(slot.Alive);
    var[10].SetNumber(slot.Hunger);
    var[11].SetNumber(slot.Thirst);
    var[12].SetNumber(slot.Toxic);
    var[13].SetNumber(slot.BackpackID);
    var[14].SetNumber(slot.BackpackSize);
    var[15].SetNumber(0);        // weight
    var[16].SetNumber(slot.Stats.KilledZombies);        // zombies Killed
    var[17].SetNumber(slot.Stats.KilledBandits);        // bandits killed
    var[18].SetNumber(slot.Stats.KilledSurvivors);        // civilians killed
    var[19].SetString(getReputationString(slot.Stats.Reputation));    // alignment
    var[20].SetString("COLORADO");    // last Map
    var[21].SetBoolean(slot.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox);


    gfxMovie.Invoke("_root.api.addClientSurvivor", var, 22);


    gfxMovie.Invoke("_root.api.clearBackpack", NULL, 0);


    addBackpackItems(slot);


    
}
void HUDVault::addBackpackItems(const wiCharDataFull& slot)
{
    Scaleform::GFx::Value var[7];
    r3dOutToLog("AddBackPackItems\n");
    for (int a = 0; a < slot.BackpackSize; a++)
    {
        if (slot.Items[a].InventoryID != 0)
        {
            var[0].SetInt(a);
            var[1].SetUInt(uint32_t(slot.Items[a].InventoryID));
            var[2].SetUInt(slot.Items[a].itemID);
            var[3].SetInt(slot.Items[a].quantity);
            var[4].SetInt(slot.Items[a].Var1);
            var[5].SetInt(slot.Items[a].Var2);
            char tmpStr[128] = {0};
			getAdditionalDescForItem(slot.Items[a].itemID, slot.Items[a].Var1, slot.Items[a].Var2, tmpStr);
            var[6].SetString(tmpStr);
            gfxMovie.Invoke("_root.api.addBackpackItem", var, 7);
        }
    }
}
void HUDVault::updateInventoryAndSkillItems()
{
    Scaleform::GFx::Value var[7];
    // clear inventory DB
    gfxMovie.Invoke("_root.api.clearInventory", NULL, 0);
    
    // add all items
    for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
    {
        var[0].SetUInt(uint32_t(gUserProfile.ProfileData.Inventory[i].InventoryID));
        var[1].SetUInt(gUserProfile.ProfileData.Inventory[i].itemID);
        var[2].SetNumber(gUserProfile.ProfileData.Inventory[i].quantity);
        var[3].SetNumber(gUserProfile.ProfileData.Inventory[i].Var1);
        var[4].SetNumber(gUserProfile.ProfileData.Inventory[i].Var2);
        bool isConsumable = false;
        {
            const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(gUserProfile.ProfileData.Inventory[i].itemID);
            if(wc && wc->category == storecat_UsableItem && wc->m_isConsumable)
                isConsumable = true;
        }
        var[5].SetBoolean(isConsumable);
        char tmpStr[128] = {0};
		getAdditionalDescForItem(gUserProfile.ProfileData.Inventory[i].itemID, gUserProfile.ProfileData.Inventory[i].Var1, gUserProfile.ProfileData.Inventory[i].Var2, tmpStr);
        var[6].SetString(tmpStr);
        gfxMovie.Invoke("_root.api.addInventoryItem", var, 7);
    }
}


void HUDVault::updateSurvivorTotalWeight(const wiCharDataFull& slot)
{
    float totalWeight = slot.getTotalWeight();


    Scaleform::GFx::Value var[2];
    var[0].SetString(slot.Gamertag);
    var[1].SetNumber(totalWeight);
    gfxMovie.Invoke("_root.api.updateClientSurvivorWeight", var, 2);
}


void HUDVault::eventReturnToGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
    Deactivate();
}


void HUDVault::eventBackpackFromInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
    const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
    if(!(slot.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox))
        return;


    m_inventoryID = args[0].GetUInt();
    m_gridTo = args[1].GetInt();
    m_Amount = args[2].GetInt();


    Scaleform::GFx::Value var[2];
    var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
    var[1].SetBoolean(false);
    gfxMovie.Invoke("_root.api.showInfoMsg", var, 2);


    uint32_t itemID = 0;
    for(uint32_t i=0; i<gUserProfile.ProfileData.NumItems; ++i)
    {
        if(gUserProfile.ProfileData.Inventory[i].InventoryID == m_inventoryID)
        {
            itemID = gUserProfile.ProfileData.Inventory[i].itemID;
            break;
        }
    }


    // check to see if there is anything in backpack, and if there is, then we need to firstly move that item to inventory
    if(slot.Items[m_gridTo].itemID != 0 && slot.Items[m_gridTo].itemID!=itemID)
    {
        m_gridFrom = m_gridTo;
        m_Amount2 = slot.Items[m_gridTo].quantity;


        // check weight
        float totalWeight = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].getTotalWeight();


    


        const BaseItemConfig* bic = g_pWeaponArmory->getConfig(slot.Items[m_gridTo].itemID);
        if(bic)
            totalWeight -= bic->m_Weight*slot.Items[m_gridTo].quantity;
    
        bic = g_pWeaponArmory->getConfig(itemID);
        if(bic)
            totalWeight += bic->m_Weight*m_Amount;
        // Skillsystem
        
        if(slot.Stats.skillid2 == 1)
            totalWeight *= 0.9f;
        


        const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].BackpackID);
        r3d_assert(bc);
        if(totalWeight > bc->m_maxWeight)
        {
            Scaleform::GFx::Value var[2];
            var[0].SetStringW(gLangMngr.getString("FR_PAUSE_TOO_MUCH_WEIGHT"));
            var[1].SetBoolean(true);
            var[2].SetString("");
            gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
            return;
        }


        async_.StartAsyncOperation(this, &HUDVault::as_BackpackFromInventorySwapThread, &HUDVault::OnBackpackFromInventorySuccess);
    }
    else
    {
        // check weight
        float totalWeight = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].getTotalWeight();


        const BaseItemConfig* bic = g_pWeaponArmory->getConfig(itemID);
        if(bic)
            totalWeight += bic->m_Weight*m_Amount;
        // Skillsystem
        r3dOutToLog("Player %s TotalWeight is: %f \n", slot.Gamertag, totalWeight);
        if(slot.Stats.skillid2 == 1)
            totalWeight *= 0.9f;
        r3dOutToLog("Player %s adjusted Totalweight is: %f \n", slot.Gamertag, totalWeight);


        const BackpackConfig* bc = g_pWeaponArmory->getBackpackConfig(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID].BackpackID);
        r3d_assert(bc);
        if(totalWeight > bc->m_maxWeight)
        {
            Scaleform::GFx::Value var[2];
            var[0].SetStringW(gLangMngr.getString("FR_PAUSE_TOO_MUCH_WEIGHT"));
            var[1].SetBoolean(true);
            var[2].SetString("");
            gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);
            return;
        }


        async_.StartAsyncOperation(this, &HUDVault::as_BackpackFromInventoryThread, &HUDVault::OnBackpackFromInventorySuccess);
    }
}


unsigned int WINAPI HUDVault::as_BackpackFromInventorySwapThread(void* in_data)
{
    r3dThreadAutoInstallCrashHelper crashHelper;
    HUDVault* This = (HUDVault*)in_data;


    This->async_.DelayServerRequest();


    // move item in backpack to inventory
    int apiCode = gUserProfile.ApiBackpackToInventory(This->m_gridFrom, This->m_Amount2);
    r3dOutToLog("GridFrom: %d, Amount: %d\n", This->m_gridFrom, This->m_Amount);
    if(apiCode != 0)
    {
        if(apiCode==7)
            This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
        else
            This->async_.SetAsyncError(apiCode, gLangMngr.getString("BackpackToInventoryFail"));
        return 0;
    }


    apiCode = gUserProfile.ApiBackpackFromInventory(This->m_inventoryID, This->m_gridTo, This->m_Amount);
    r3dOutToLog("InventoryID: %d, GridTo: %d, Amount: %d\n", This->m_inventoryID, This->m_gridTo, This->m_Amount);
    if(apiCode != 0)
    {
        if(apiCode==7)
            This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
        else
            This->async_.SetAsyncError(apiCode, gLangMngr.getString("FailedToFindBackpack"));
        return 0;
    }


    return 1;
}


void HUDVault::OnBackpackFromInventorySwapSuccess()
{
    obj_Player* plr = gClientLogic().localPlayer_;


    plr->CurLoadout = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];


    PKT_C2S_InventoryOp_s n;
    n.op = n.OP_TOINV;
    n.Slot = m_gridFrom;
    n.Quantity = m_Amount2;
    p2pSendToHost(plr, &n, sizeof(n));


    n.op = n.OP_FROMINV;
    n.InventoryID = m_inventoryID;
    n.Slot = m_gridTo;
    n.Quantity = m_Amount;
    p2pSendToHost(plr, &n, sizeof(n));


    plr->OnBackpackChanged(m_gridTo);


    Scaleform::GFx::Value var[8];
    gfxMovie.Invoke("_root.api.clearBackpack", NULL, 0);
    


        addBackpackItems(plr->CurLoadout);
    


    updateInventoryAndSkillItems();


    updateSurvivorTotalWeight(plr->CurLoadout);


    gfxMovie.Invoke("_root.api.hideInfoMsg", NULL, 0);
    gfxMovie.Invoke("_root.api.backpackFromInventorySuccess", NULL, 0);
    return;
}


unsigned int WINAPI HUDVault::as_BackpackFromInventoryThread(void* in_data)
{
    r3dThreadAutoInstallCrashHelper crashHelper;
    HUDVault* This = (HUDVault*)in_data;


    This->async_.DelayServerRequest();
    
    int apiCode = gUserProfile.ApiBackpackFromInventory(This->m_inventoryID, This->m_gridTo, This->m_Amount);
    if(apiCode != 0)
    {
        if(apiCode==7)
            This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
        else
            This->async_.SetAsyncError(apiCode, gLangMngr.getString("BackpackFromInventoryFail"));
        return 0;
    }


    return 1;
}


void HUDVault::OnBackpackFromInventorySuccess()
{
    obj_Player* plr = gClientLogic().localPlayer_;


    plr->CurLoadout = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];


    PKT_C2S_InventoryOp_s n;
    n.op = n.OP_FROMINV;
    n.InventoryID = m_inventoryID;
    n.Slot = m_gridTo;
    n.Quantity = m_Amount;
    p2pSendToHost(plr, &n, sizeof(n));


    plr->OnBackpackChanged(m_gridTo);


    Scaleform::GFx::Value var[8];
    gfxMovie.Invoke("_root.api.clearBackpack", NULL, 0);




        addBackpackItems(plr->CurLoadout);
    


    updateInventoryAndSkillItems();


    updateSurvivorTotalWeight(plr->CurLoadout);


    gfxMovie.Invoke("_root.api.hideInfoMsg", NULL, 0);
    gfxMovie.Invoke("_root.api.backpackFromInventorySuccess", NULL, 0);
    return;
}


void HUDVault::eventBackpackToInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
    const wiCharDataFull& slot = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];
    if(!(slot.GameFlags & wiCharDataFull::GAMEFLAG_NearPostBox))
        return;


    m_gridFrom = args[0].GetInt();
    m_Amount = args[1].GetInt();
    r3dOutToLog("GridFrom: %d, Amount: %d\n", m_gridFrom, m_Amount);
    Scaleform::GFx::Value var[3];
    var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
    var[1].SetBoolean(false);
    var[2].SetString("");
    gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);


    async_.StartAsyncOperation(this, &HUDVault::as_BackpackToInventoryThread, &HUDVault::OnBackpackToInventorySuccess);
}


unsigned int WINAPI HUDVault::as_BackpackToInventoryThread(void* in_data)
{
    r3dThreadAutoInstallCrashHelper crashHelper;
    HUDVault* This = (HUDVault*)in_data;


    This->async_.DelayServerRequest();
    r3dOutToLog("Inventory from: %d, Amount: %d\n", This->m_gridFrom, This->m_Amount);
    int apiCode = gUserProfile.ApiBackpackToInventory(This->m_gridFrom, This->m_Amount);
    if(apiCode != 0)
    {
        if(apiCode==7)
            This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
        else
            This->async_.SetAsyncError(apiCode, gLangMngr.getString("BackpackToInventoryFail"));
        return 0;
    }


    return 1;
}


void HUDVault::OnBackpackToInventorySuccess()
{
    obj_Player* plr = gClientLogic().localPlayer_;


    plr->CurLoadout = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];


    PKT_C2S_InventoryOp_s n;
    n.op = n.OP_TOINV;
    n.Slot = m_gridFrom;
    n.Quantity = m_Amount;
    p2pSendToHost(plr, &n, sizeof(n));


    plr->OnBackpackChanged(m_gridFrom);


    Scaleform::GFx::Value var[8];
    gfxMovie.Invoke("_root.api.clearBackpack", NULL, 0);


    
        addBackpackItems(plr->CurLoadout);
    


    updateInventoryAndSkillItems ();


    updateSurvivorTotalWeight(plr->CurLoadout);


    gfxMovie.Invoke("_root.api.hideInfoMsg", NULL, 0);
    gfxMovie.Invoke("_root.api.backpackToInventorySuccess", NULL, 0);


    return;
}


void HUDVault::eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount)
{
    m_gridFrom = args[0].GetInt();
    m_gridTo = args[1].GetInt();


    Scaleform::GFx::Value var[3];
    var[0].SetStringW(gLangMngr.getString("OneMomentPlease"));
    var[1].SetBoolean(false);
    var[2].SetString("");
    gfxMovie.Invoke("_root.api.showInfoMsg", var, 3);


    async_.StartAsyncOperation(this, &HUDVault::as_BackpackGridSwapThread, &HUDVault::OnBackpackGridSwapSuccess);
}


unsigned int WINAPI HUDVault::as_BackpackGridSwapThread(void* in_data)
{
    r3dThreadAutoInstallCrashHelper crashHelper;
    HUDVault* This = (HUDVault*)in_data;


    This->async_.DelayServerRequest();
    
    int apiCode = gUserProfile.ApiBackpackGridSwap(This->m_gridFrom, This->m_gridTo);
    if(apiCode != 0)
    {
        if(apiCode==7)
            This->async_.SetAsyncError(0, gLangMngr.getString("GameSessionHasNotClosedYet"));
        else
            This->async_.SetAsyncError(apiCode, gLangMngr.getString("SwitchBackpackSameBackpacks"));
        return 0;
    }


    return 1;
}


void HUDVault::OnBackpackGridSwapSuccess()
{
    obj_Player* plr = gClientLogic().localPlayer_;


    plr->CurLoadout = gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID];


    PKT_C2S_BackpackSwap_s n;
    n.SlotFrom = m_gridFrom;
    n.SlotTo   = m_gridTo;
    p2pSendToHost(plr, &n, sizeof(n));


    plr->OnBackpackChanged(m_gridFrom);
    plr->OnBackpackChanged(m_gridTo);


    Scaleform::GFx::Value var[8];
    gfxMovie.Invoke("_root.api.clearBackpack", NULL, 0);


    
        addBackpackItems(plr->CurLoadout);
    


    updateSurvivorTotalWeight(plr->CurLoadout);


    gfxMovie.Invoke("_root.api.hideInfoMsg", NULL, 0);
    gfxMovie.Invoke("_root.api.backpackGridSwapSuccess", NULL, 0);
    return;
}


bool HUDVault::Init()
{
     if(!gfxMovie.Load("Data\\Menu\\WarZ_HUD_PostOffice.swf", false)) 
    {
         return false;
    }


#define MAKE_CALLBACK(FUNC) new r3dScaleformMovie::TGFxEICallback<HUDVault>(this, &HUDVault::FUNC)
     gfxMovie.RegisterEventHandler("eventBackpackFromInventory", MAKE_CALLBACK(eventBackpackFromInventory));
    gfxMovie.RegisterEventHandler("eventBackpackToInventory", MAKE_CALLBACK(eventBackpackToInventory));
    gfxMovie.RegisterEventHandler("eventBackpackGridSwap", MAKE_CALLBACK(eventBackpackGridSwap));
    gfxMovie.RegisterEventHandler("eventReturnToGame", MAKE_CALLBACK(eventReturnToGame));


    gfxMovie.SetCurentRTViewport(Scaleform::GFx::Movie::SM_ExactFit);
    itemInventory_.initialize(&gfxMovie);
    isActive_ = false;
    isInit_ = true;
    return true;
}


bool HUDVault::Unload()
{
     gfxMovie.Unload();
    isActive_ = false;
    isInit_ = false;
    return true;
}


void HUDVault::Update()
{
    async_.ProcessAsyncOperation(this, gfxMovie);
}


void HUDVault::Draw()
{
     gfxMovie.UpdateAndDraw();
}


void HUDVault::Deactivate()
{
    if (async_.Processing())
    {
        return;
    }


    Scaleform::GFx::Value var[1];
    var[0].SetString("menu_close");
    gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);


    if(prevKeyboardCaptureMovie)
    {
        prevKeyboardCaptureMovie->SetKeyboardCapture();
        prevKeyboardCaptureMovie = NULL;
    }


    if(!g_cursor_mode->GetInt())
    {
        r3dMouse::Hide();
    }


    isActive_ = false;
}


void HUDVault::Activate()
{
    prevKeyboardCaptureMovie = gfxMovie.SetKeyboardCapture(); // for mouse scroll events
    
    r3d_assert(!isActive_);
    r3dMouse::Show();
    isActive_ = true;
    addClientSurvivor(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);
    updateInventoryAndSkillItems();
    updateSurvivorTotalWeight(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);


    reloadBackpackInfo();
    //updateSurvivorTotalWeight(gUserProfile.ProfileData.ArmorySlots[gUserProfile.SelectedCharID]);
    
    gfxMovie.Invoke("_root.api.showInventoryScreen", NULL, 0);


    Scaleform::GFx::Value var[1];
    var[0].SetString("menu_open");
    gfxMovie.OnCommandCallback("eventSoundPlay", var, 1);
}
void HUDVault::reloadBackpackInfo()
{
    // reset backpack
    {
        gfxMovie.Invoke("_root.api.clearBackpack", "");
        gfxMovie.Invoke("_root.api.clearBackpacks", "");
    }


    std::vector<uint32_t> uniqueBackpacks; // to filter identical backpack
    int backpackSlotIDInc = 0;
    // add backpack content info
    {
        obj_Player* plr = gClientLogic().localPlayer_;
        r3d_assert(plr);
        wiCharDataFull& slot = plr->CurLoadout;


        Scaleform::GFx::Value var[7];
        for (int a = 0; a < slot.BackpackSize; a++)
        {
            if (slot.Items[a].itemID != 0)
            {
                var[0].SetInt(a);
                var[1].SetUInt(0); // not used for game
                var[2].SetUInt(slot.Items[a].itemID);
                var[3].SetInt(slot.Items[a].quantity);
                var[4].SetInt(slot.Items[a].Var1);
                var[5].SetInt(slot.Items[a].Var2);
                char tmpStr[128] = {0};
                getAdditionalDescForItem(slot.Items[a].itemID, slot.Items[a].Var1, slot.Items[a].Var2, tmpStr);
                var[6].SetString(tmpStr);
                gfxMovie.Invoke("_root.api.addBackpackItem", var, 7);


                const BackpackConfig* bpc = g_pWeaponArmory->getBackpackConfig(slot.Items[a].itemID);
                if(bpc)
                {
                    if(std::find<std::vector<uint32_t>::iterator, uint32_t>(uniqueBackpacks.begin(), uniqueBackpacks.end(), slot.Items[a].itemID) != uniqueBackpacks.end())
                        continue;


                    // add backpack info
                    var[0].SetInt(backpackSlotIDInc++);
                    var[1].SetUInt(slot.Items[a].itemID);
                    gfxMovie.Invoke("_root.api.addBackpack", var, 2);


                    uniqueBackpacks.push_back(slot.Items[a].itemID);
                }
            }
        }
    }


    gfxMovie.Invoke("_root.api.Main.Inventory.showBackpack", "");
}


#pragma endregion VAULT_IS_WORKING