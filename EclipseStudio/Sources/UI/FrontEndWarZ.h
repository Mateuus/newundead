#pragma once

#include "UIMenu.h"
#include "../../ServerNetPackets/NetPacketsGameBrowser.h"

#include "CkHttp.h"

#include "FrontEndShared.h"
#include "GameCode\UserProfile.h"
#include "GameCode\UserClans.h"

#include "UIAsync.h"
#include "UIMarket.h"

class FrontendWarZ : public UIMenu, public r3dIResource
{
public:
	virtual bool 	Unload();
private:
	Scaleform::Render::D3D9::Texture* RTScaleformTexture;
	bool		needReInitScaleformTexture;
	int			frontendStage; // 0 - login, 1-frontend

	EGameResult	prevGameResult;

	void drawPlayer();

//	void	eventFriendPopupClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	
public:
	FrontendWarZ(const char * movieName);
	virtual ~FrontendWarZ();

	virtual bool Initialize();

	virtual int Update();
	void addPlayerInfo(const char* name , int alive , int rep , int xp);
void LoadKillInject();
void InitTs3();
void InitButtons();
bool isInitTs3;
bool isHack;
	void postLoginStepInit(EGameResult gameResult);
	void initLoginStep(const wchar_t* loginErrorMsg);

	//	r3dIResource, callbacks
	//	Need to rebind texture to scaleform after device lost
	virtual	void D3DCreateResource();
	virtual	void D3DReleaseResource(){};

	void bindRTsToScaleForm();

	void SetLoadedThings(obj_Player* plr)
	{
		r3d_assert(m_Player == NULL);
		m_Player = plr;
	}

	int CurrentBrowse;

	// login part of frontend
private:
	friend void FrontendWarZ_LoginProcessThread(void* in_data);
	static unsigned int WINAPI LoginProcessThread(void* in_data);
	static unsigned int WINAPI LoginAuthThread(void* in_data);
	static unsigned int WINAPI ClanProcessThread(void* in_data);

	HANDLE	loginThread;
	enum {
		ANS_Unactive,
		ANS_Processing,
		ANS_Timeout,
		ANS_Error,

		ANS_Logged,
		ANS_BadPassword,
		ANS_Frozen,
	};
	volatile DWORD loginAnswerCode;
	bool	DecodeAuthParams();
	void	LoginCheckAnswerCode();
	float	loginProcessStartTime;
	bool	loginMsgBoxOK_Exit;

private:
	void eventReviveCharMoney(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventPlayGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventCancelQuickGameSearch(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventQuitGame(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventCreateCharacter(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventDeleteChar(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventReviveChar(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBuyItem(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBackpackFromInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBackpackToInventory(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBackpackGridSwap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventSetSelectedChar(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventOptionsReset(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventOptionsApply(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventOptionsVoipApply(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventOptionsLanguageSelection(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventOptionsControlsRequestKeyRemap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventOptionsControlsReset(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventOptionsControlsApply(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventChangeOutfit(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventSetCurrentBrowseChannel(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventCreateChangeCharacter(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventCreateCancel(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRequestPlayerRender(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventMsgBoxCallback(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventOpenBackpackSelector(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventChangeBackpack(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

	// - Skill System Thanks InvasionMMO
	void eventLearnSkill(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
void OnLearnSkillSuccess();
void OnConvertGDSuccess();
int hardcore;
int type;
int pageType;
void OnLeaderBoardDataSuccess();
void Draw();
void reloadBackpack();
static unsigned int WINAPI DrawThread(void* in_data);
	static unsigned int WINAPI as_ReadSkilLThread(void* in_data);
	static unsigned int WINAPI as_LeaderBoardThread(void* in_data);
	static unsigned int WINAPI as_LearnSkilLThread(void* in_data);
	static unsigned int WINAPI as_ConvertGDThread(void* in_data);
	static unsigned int WINAPI as_RentTheServerThread(void* in_data);
	static unsigned int WINAPI as_BuyPremiumThread(void* in_data);
	static unsigned int WINAPI RenderThread(void* in_data);
	static unsigned int WINAPI as_TransactionsThread(void* in_data);
	void OnTransactionsDataSuccess();
   // Skill

	void OnBuyPremiumSuccess();
	void eventBrowseGamesRequestFilterStatus(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBrowseGamesSetFilter(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
void eventTrialUpgradeAccount(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
void eventBrowseGamesJoin(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBrowseGamesOnAddToFavorites(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBrowseGamesRequestList(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

	void eventMyServerUpdateSettings(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRequestMyClanInfo(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRequestClanList(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventCreateClan(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanAdminDonateGC(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanAdminAction(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanLeaveClan(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanDonateGCToClan(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRequestClanApplications(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanApplicationAction(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanInviteToClan(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanRespondToInvite(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanBuySlots(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventClanApplyToJoin(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRenameCharacter(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
    void eventRequestMyServerInfo(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRequestLeaderboardData(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventMyServerJoinServer(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRequestMyServerList(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRequestGCTransactionData(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRentServerUpdatePrice(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventRentServer(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

	void OnClanListSuccess();
	int ClanSort;
	CUserClans* clansmem;
	const char* FindClanTagAndColorById(int id ,char* str,CUserClans* clan);
	void eventMyServerKickPlayer(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void eventBuyPremiumAccount(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void FrontendWarZ::eventShowSurvivorsMap(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
void FrontendWarZ::eventStorePurchaseGDCallback(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
void FrontendWarZ::eventMarketplaceActive(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
void FrontendWarZ::eventStorePurchaseGD(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void checkForInviteFromClan();
	
	void setClanInfo();
	void showInputPassword();
	int convertvalue;
	int currentvalue;
	int valor;

	CUserClans::CreateParams_s clanCreateParams;
	std::string					   clanCreateParam_Desc;
	static unsigned int WINAPI as_CreateClanThread(void* in_data);
	void		OnCreateClanSuccess();
	void		refreshClanUIMemberList();

	static unsigned int WINAPI as_BrowseGamesThread(void* in_data);
	void		OnGameListReceived();
	void		processNewGameList();

	int		superPings_[2048]; // big enough to hold all possiblesupervisors id
	void		ProcessSupervisorPings();
	int		 GetSupervisorPing(DWORD ip);
	int		GetGamePing(DWORD superId);

	void OnUpdatePlayerOnlineSuccess();
	bool IsRentServer;
	void initFrontend();
	void initInventoryItems();
	void updateInventoryAndSkillItems();
    void OnMyServerListReceived();
	void addClientSurvivor(const wiCharDataFull& slot, int slotIndex);
	void addBackpackItems(const wiCharDataFull& slot, int slotIndex);

	bool		exitRequested_;
	bool		needExitByGameJoin_;
	bool		needReturnFromQuickJoin;

	//
	// Async Function Calls
	//
	UIAsync<FrontendWarZ> async_;

	float		masterConnectTime_;
	bool		ConnectToMasterServer();
	bool		ParseGameJoinAnswer();
	volatile bool CancelQuickJoinRequest;

	static unsigned int WINAPI as_CreateCharThread(void* in_data);
	void		OnCreateCharSuccess();
	static unsigned int WINAPI as_DeleteCharThread(void* in_data);
	void		OnDeleteCharSuccess();
	static	unsigned int WINAPI as_BackpackFromInventoryThread(void* in_data);
	static	unsigned int WINAPI as_BackpackFromInventorySwapThread(void* in_data);
	void		OnBackpackFromInventorySuccess();
	static	unsigned int WINAPI as_ReviveCharThread(void* in_data);
	void		OnReviveCharSuccess();
	static unsigned int WINAPI as_PlayerListThread(void* in_data);
	static	unsigned int WINAPI as_BackpackToInventoryThread(void* in_data);
	void		OnBackpackToInventorySuccess();
	void OnPlayerListSuccess();
	static	unsigned int WINAPI as_BackpackGridSwapThread(void* in_data);
	void		OnBackpackGridSwapSuccess();
	static	unsigned int WINAPI as_BackpackChangeThread(void* in_data);
	void		OnBackpackChangeSuccess();

	void OnBanPlayerSuccess();
	static unsigned int WINAPI FrontendWarZ::as_BanPlayerThread(void* in_data);
	void OnRentSetSettingsSuccess();
	static unsigned int WINAPI FrontendWarZ::as_RentGameChangePwdThread(void* in_data);
	static unsigned int WINAPI as_PlayGameThread(void* in_data);
	static unsigned int WINAPI as_JoinGameThread(void* in_data);
	static	unsigned int WINAPI as_RentGamesThread(void* in_data);
	static	unsigned int WINAPI as_RentGamesStrThread(void* in_data);
	static unsigned int WINAPI FrontendWarZ::as_RentGameKickThread(void* in_data);


	void	SyncGraphicsUI();
	void	AddSettingsChangeFlag( DWORD flag );
	void	SetNeedUpdateSettings();
	void	UpdateSettings();
	void	updateSurvivorTotalWeight(int survivor);

// char create
	uint32_t		m_itemID;

	__int64	m_inventoryID;
	int		m_gridTo;
	int		m_gridFrom;
	int		m_Amount;
	int		m_Amount2;
	__int64	mChangeBackpack_inventoryID;

	wiCharDataFull	PlayerCreateLoadout; // save loadout when creating player, so that user can see rendered player
	uint32_t		m_CreateHeroID;
	int		m_CreateBodyIdx;
	int		m_CreateHeadIdx;
	int		m_CreateLegsIdx;
	char	m_CreateGamerTag[64];
	int		m_CreateGameMode;
	void OnRentKickSuccess();
	DWORD		m_joinGameServerId;
	DWORD		m_ispassword;
	char		m_joinGamePwd[32];
	char		m_RentGameName[32];
	char		m_RentGamePwd[32];
	bool m_RentGameNeedInput;
	int		m_RentGamePeriod;
	int		m_RentGameSlot;
	int		m_RentGameMapid;
	char passstring[512];
    void		OnRentSuccess();
	void OnRentReqSuccess();
	bool	m_ReloadProfile;
	float	m_ReloadTimer;

	DWORD	settingsChangeFlags_;
	bool	needUpdateSettings_;
	int		m_waitingForKeyRemap;

	class obj_Player* m_Player;
	int m_needPlayerRenderingRequest;

	int m_browseGamesMode; // 0 - browse, 1-recent, 2-favorites
    uint32_t skillid;
	int	CharID;

	int m_leaderboardPage;
	int m_leaderboardPageCount;

	UIMarket market_;
};
