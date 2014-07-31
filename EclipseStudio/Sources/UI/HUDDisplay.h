#ifndef HUDDisplay_h
#define HUDDisplay_h

#include "r3d.h"
#include "APIScaleformGfx.h"
#include "../GameCode/UserProfile.h"
#include "../ObjectsCode/weapons/Weapon.h"
#include "../VoIP/public_definitions.h"
#include "../VoIP/public_errors.h"
#include "../VoIP/clientlib_publicdefinitions.h"
#include "../VoIP/clientlib.h"

#define MAX_HUD_ACHIEVEMENT_QUEUE 8

class obj_Player;
class HUDDisplay
{
protected:
	bool Inited;
	bool chatVisible;
bool isgroups;
	//bool chatVisible;

char * mode1;
	bool chatInputActive;
	float chatVisibleUntilTime;
	float lastChatMessageSent;
	int	currentChatChannel;

	int playersListVisible;

	float bloodAlpha;

	int writeNoteSavedSlotIDFrom;
	float timeoutForNotes; // stupid UI design :(
	float timeoutNoteReadAbuseReportedHideUI;

	bool RangeFinderUIVisible;

	int weaponInfoVisible;
	bool SafeZoneWarningVisible;

public:
	r3dScaleformMovie gfxHUD;
	r3dScaleformMovie gfxBloodStreak;
	r3dScaleformMovie gfxRangeFinder;
//	r3dScaleformMovie gfxCraft;
void removeMissionInfo(int var1);
	void	eventChatMessage(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void HUDDisplay::addplayertogroup(const char* gamertag,bool legend);
	void addMissionInfo(const char* var1);
	void setCarInfo(int var1 , int var2 , int var3 ,int var4, int var5 , bool show);
	void setMissionObjectiveCompleted(int var1 , int var2);
	void addMissionObjective(int var1 , const char* var2 , bool var3 , const char* var4 , bool var5);
	void setMissionObjectiveNumbers(int var1, int var2 , const char* var3);
	void HUDDisplay::showMissionInfo(bool var1);
	void HUDDisplay::removeplayerfromgroup(const char* gamertag,bool legend);
	void	eventNoteWritePost(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventNoteClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventGraveNoteClosed(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
		void	eventSafelockPass(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventNoteReportAbuse(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventShowPlayerListContextMenu(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);
	void	eventPlayerListAction(r3dScaleformMovie* pMovie, const Scaleform::GFx::Value* args, unsigned argCount);

public:
	HUDDisplay();
	~HUDDisplay();


	float updatetime;
	float updatetime2;
	bool currentinvite;
	bool 	Init();
	bool 	Unload();

int	SafeID;

//obj_SafeLock* SafeLock;
bool isT;
anyID clientid;
int status;
char* name123;
	int 	Update();
	int 	Draw();

	void	setBloodAlpha(float alpha);

	// HUD functions
	void	setVisibility(float percent); // [0,1]
	void	setHearing(float percent); // [0,1]
	void	setLifeParams(int food, int water, int health, int toxicity, int stamina);
	void setConditionIcon(const char* var1,int var2);
	void	setWeaponInfo(int ammo, int clips, int firemode);
	void	showWeaponInfo(int state);
	void	setSlotInfo(int slotID, const char* name, int quantity, const char* icon);
	void	updateSlotInfo(int slotID, int quantity);
	void	showSlots(bool state);
	void	setActiveSlot(int slotID);
	void	setActivatedSlot(int slotID);
	void	showMessage(const wchar_t* text);

	void	showChat(bool showChat, bool force=false);
	void	showChatInput();
	void	addChatMessage(int tabIndex, const char* user, const char* text, uint32_t flags);
	void	JoinGroup(const char* gamertag, bool isLegend);
	bool	isChatInputActive() const { return chatInputActive || (r3dGetTime()-lastChatMessageSent)<0.25f || writeNoteSavedSlotIDFrom; }
	bool	isChatVisible() const { return chatVisible; }
	void	setChatTransparency(float alpha); //[0,1]
	void addVoip(const char *name, char* name1);
	void removeVoip(const char* name, char* name1);
	void onTalkStatusChangeEvent2(anyID custom,int status);
	void onTalkStatusChangeEvent1(int clienti,int status);
	void	setChatChannel(int index);
	void	enableClanChannel();
		
	void	enableGroupChannel();
//	void onConnectStatusChangeEvent(int serverConnectionHandlerID, int newStatus, unsigned int errorNumber);

bool IsNoDrop;

float idietime;

float Cooldown;
int currentslot;
	// player list fn
	void	clearPlayersList();
	void	addPlayerToList(int num, const char* name, int Reputation, bool isLegend, bool isDev, bool isPunisher, bool isInvitePending, bool isPremium,bool isMute, bool local);
	void	showPlayersList(int flag);
	int		isPlayersListVisible() const {return playersListVisible;}

	void	groupmenu();

	// notes
	bool	canShowWriteNote() const { return r3dGetTime() > timeoutForNotes; }
	void	showWriteNote(int slotIDFrom);
	void	showReadNote(const char* msg);
	void	showGraveNote(const char* plr,const char* plr2);
	void	showSL(bool var1,bool var2);

	void	showRangeFinderUI(bool set) { RangeFinderUIVisible = set; }

	void	showYouAreDead(const char* killedBy);

	void	showSafeZoneWarning(bool flag);

void addgrouplist(const char* name);

void removegrouplist(const char* name);

	void	addCharTag1(const char* name, int Reputation, bool isSameClan, Scaleform::GFx::Value& result);
	//EXPORTDLL unsigned int ts3client_startConnection(uint64 serverConnectionHandlerID, const char* identity, const char* ip, unsigned int port, const char* nickname,
                                                 //const char** defaultChannelArray, const char* defaultChannelPassword, const char* serverPassword);
	void	moveUserIcon(Scaleform::GFx::Value& icon, const r3dPoint3D& pos, bool alwaysShow, bool force_invisible = false, bool pos_in_screen_space=false); 
	void	setCharTagTextVisible1(Scaleform::GFx::Value& icon, bool isShowName, bool isSameGroup);
	void setThreatValue(int value);
	void setCooldown(int slot,int CoolSecond,int value);
	void	removeUserIcon(Scaleform::GFx::Value& icon);
	void aboutToLeavePlayerFromGroup(const char* var1);
};

#endif