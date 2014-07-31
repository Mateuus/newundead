#pragma once
// define gameplay constants here

// MAKE SURE to increase this value after adding/removing/moving variables
#define GAMEPLAYPARAM_VERSION	0x000006

#pragma pack(push)
#pragma pack(1)
struct CGamePlayParams
{
	//////////////////////////////////////////////////////////////
	/////////// PLAYER SHOOTING CONSTANTS ////////////////////////
	//////////////////////////////////////////////////////////////
	float c_fSpreadRecoilMod_Running;
	float c_fSpreadRecoilMod_Aim;
	float c_fSpreadRecoilMod_Crouch;
	float c_fSpreadRecoilMod_Moving;
	float c_fSpreadTriggerHappyNum; // after ten bullets your spread will increase by
	float c_fSpreadTriggerHappyMod; // 1.0f - means doubles (as we adding, not multiplying)
	float c_fSpreadModShotgun_Aim;
	float c_fSpreadModShotgun_Crouch;
	float c_fGlobalSpreadMod;

	float c_fZeroSpreadDelayBetweenShots;
	float c_fRecoilCooldownSpeed;
	float c_MaxVerticalRecoil;

	float c_fSprintMaxEnergy;
	float c_fSprintRegenRateLowHealth;
	float c_fSprintRegenRateNormal;

	float c_fSniperRifleHoldBreathTime;
	float c_fSniperRifleAfterHoldBounceTime;

	float c_fFlashBangDuration;
	float c_fFlashBangRadius;

	float c_fHitMarkerFadeoutSpeed;

	float c_fBloodToxicLevel1;
	float c_fBloodToxicLevel1_HPDamage;
	float c_fBloodToxicLevel2;
	float c_fBloodToxicLevel2_HPDamage;
	float c_fBloodToxicLevel3;
	float c_fBloodToxicLevel3_HPDamage;
	float c_fHungerLevel1;
	float c_fHungerLevel_HPDamage;
	float c_fThirstLevel1;
	float c_fThirstLevel_HPDamage;
	float c_fBloodToxicIncLevel1;
	float c_fBloodToxicIncLevel1Value;
	float c_fBloodToxicIncLevel2;
	float c_fBloodToxicIncLevel2Value;
	float c_fThirstInc;
	float c_fThirstSprintInc;
	float c_fThirstHighToxicLevel;
	float c_fThirstHighToxicLevelInc;
	float c_fHungerInc;
	float c_fHungerRunInc;
	float c_fHungerSprintInc;
	float c_fHungerHighToxicLevel;
	float c_fHungerHighToxicLevelInc;

	float c_fSpeedMultiplier_LowHealthLevel;
	float c_fSpeedMultiplier_LowHealthValue;
	float c_fSpeedMultiplier_HighThirstLevel;
	float c_fSpeedMultiplier_HighThirstValue;
	float c_fSpeedMultiplier_HighHungerLevel;
	float c_fSpeedMultiplier_HighHungerValue;
	
	float c_fPostBox_EnableRadius;	// radius around postbox when you can use global inventory
	
	int c_iGameTimeCompression;	// coefficiend of game time

	// speeds
	float AI_BASE_MOD_SIDE;
	float AI_BASE_MOD_FORWARD;
	float AI_BASE_MOD_BACKWARD;
	float AI_WALK_SPEED;
	float AI_RUN_SPEED;
	float AI_SPRINT_SPEED;


//  set default values here
	CGamePlayParams()
	{
    //////////////////////////////////////////////////////////////
   	/////////// PLAYER SHOOTING CONSTANTS ////////////////////////
    //////////////////////////////////////////////////////////////
    c_fSpreadRecoilMod_Running = 1.2f;
	c_fSpreadRecoilMod_Aim = 0.4f;
	c_fSpreadRecoilMod_Crouch = 0.85f;
	c_fSpreadRecoilMod_Moving = 1.2f;
	c_fSpreadModShotgun_Aim = 1.0f;
	c_fSpreadModShotgun_Crouch = 0.9f;
	c_fGlobalSpreadMod = 1.0f;

	c_fZeroSpreadDelayBetweenShots = 0.5f;

	c_fRecoilCooldownSpeed = 4.5f;

	c_MaxVerticalRecoil = 8.0f;

	c_fSprintMaxEnergy = 45.0f;
	c_fSprintRegenRateLowHealth = 0.15f; // full regen in 5min
	c_fSprintRegenRateNormal = 0.375f; // full regen in 120seconds (45/120)

	c_fSniperRifleHoldBreathTime = 3.0f;
	c_fSniperRifleAfterHoldBounceTime = 3.0f;

	c_fFlashBangDuration = 5.0f;
	c_fFlashBangRadius = 15.0f;

	c_fHitMarkerFadeoutSpeed = 3.0f;

	// divide damage by 60 to convert it to dmg per minute, otherwise it will be damage per second!
	c_fBloodToxicLevel1 = 50.0f;
	c_fBloodToxicLevel1_HPDamage = 1.0f/60.0f;
	c_fBloodToxicLevel2 = 80.0f;
	c_fBloodToxicLevel2_HPDamage = 3.0f/60.0f;
	c_fBloodToxicLevel3 = 95.0f;
	c_fBloodToxicLevel3_HPDamage = 10.0f/60.0f;
	c_fHungerLevel1 = 75.0f;
	c_fHungerLevel_HPDamage = 1.0f/60.0f;
	c_fThirstLevel1 = 75.0f;
	c_fThirstLevel_HPDamage = 2.0f/60.0f;
	c_fBloodToxicIncLevel1 = 1.0f;
	c_fBloodToxicIncLevel1Value = 1.0f/60.0f;
	c_fBloodToxicIncLevel2 = 21.0f;
	c_fBloodToxicIncLevel2Value = 2.0f/60.0f;
	c_fThirstInc = 1.5f/60.0f;
	c_fThirstSprintInc = 2.5f/60.0f;
	c_fThirstHighToxicLevel = 50.0f;
	c_fThirstHighToxicLevelInc = 2.0f/60.0f;
	c_fHungerInc = 1.5f/60.0f;
	c_fHungerRunInc = 1.5f/60.0f;
	c_fHungerSprintInc = 2.5f/60.0f;
	c_fHungerHighToxicLevel = 50.0f;
	c_fHungerHighToxicLevelInc = 3.0f/60.0f;

	c_fSpeedMultiplier_LowHealthLevel = 40.0f;
	c_fSpeedMultiplier_LowHealthValue = 1.0f;
	c_fSpeedMultiplier_HighThirstLevel = 50.0f;
	c_fSpeedMultiplier_HighThirstValue = 1.0f;
	c_fSpeedMultiplier_HighHungerLevel = 50.0f;
	c_fSpeedMultiplier_HighHungerValue = 1.0f;
	
	c_fPostBox_EnableRadius = 10.0f;
	
	c_iGameTimeCompression  = 6; // every 4 hours of real time is equal to 24 hrs of game time

	// speeds 
	AI_BASE_MOD_SIDE = 0.75f;
	AI_BASE_MOD_FORWARD = 1.0f;
	AI_BASE_MOD_BACKWARD = 0.75f;
	AI_WALK_SPEED   = 1.6f;
	AI_RUN_SPEED    = 2.8f;
	AI_SPRINT_SPEED = 4.2f;

	}
	
	DWORD GetCrc32() const {
		return r3dCRC32((const BYTE*)this, sizeof(*this));
	}
};
#pragma pack(pop)

extern	const CGamePlayParams* GPP;
