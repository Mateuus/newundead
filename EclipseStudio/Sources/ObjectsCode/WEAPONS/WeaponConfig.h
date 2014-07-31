#pragma once

#include "BaseItemConfig.h"

enum WeaponAnimTypeEnum
{
	WPN_ANIM_ASSAULT,
	WPN_ANIM_PISTOL,
	WPN_ANIM_GRENADE,
	WPN_ANIM_RPG,
	WPN_ANIM_SMG,
	WPN_ANIM_MINE,
	WPN_ANIM_MELEE,

	WPN_ANIM_COUNT
};

enum WeaponFiremodeEnum
{
	WPN_FRM_SINGLE=1<<0,
	WPN_FRM_TRIPLE=1<<1,
	WPN_FRM_AUTO=1<<2,
};

#include "../../../ServerNetPackets/NetPacketsWeaponInfo.h"

extern const char* WeaponAttachmentBoneNames[WPN_ATTM_MAX];

class WeaponAttachmentConfig : public ModelItemConfig
{
private:
	mutable r3dMesh* m_Model_AIM; // second model for when you are aiming. enabled only for scopes right now
public:
	// config
	WeaponAttachmentTypeEnum m_type;
	int m_specID;

	char* m_MuzzleParticle;

	char* m_ScopeAnimPath;

	// mods
	float	m_Damage;
	float	m_Range;
	int		m_Firerate;
	float	m_Recoil;
	float	m_Spread;
	int		m_Clipsize;
	const struct ScopeConfig* m_scopeConfig;
	const struct ScopeConfig* m_scopeConfigTPS; // spec.config for when you are switching to TPS mode
	float	m_scopeZoom; // 0..1; 0 - no zoom. 1 - maximum zoom

	// new weapon sounds IDs
	int		m_sndFireID_single;
	int		m_sndFireID_auto;
	int		m_sndFireID_single_player; // for local player
	int		m_sndFireID_auto_player; // for local player

public:
	WeaponAttachmentConfig(uint32_t id) : ModelItemConfig(id)
	{
		category = storecat_FPSAttachment;
		m_type = WPN_ATTM_INVALID;
		m_specID = 0;
		m_Model_AIM = NULL;
		m_MuzzleParticle = NULL;

		m_ScopeAnimPath = NULL;

		m_Damage = 0.0f;
		m_Range = 0.0f;
		m_Firerate = 0;
		m_Recoil = 0.0f;
		m_Spread = 0.0f;
		m_Clipsize = 0;
		m_scopeConfig = NULL;
		m_scopeConfigTPS = NULL;
		m_scopeZoom= 1.0f; 

		m_sndFireID_single = -1;
		m_sndFireID_auto   = -1;
		m_sndFireID_single_player = -1;
		m_sndFireID_auto_player = -1;
	}
	virtual ~WeaponAttachmentConfig() 
	{
		free(m_MuzzleParticle);
		free(m_ScopeAnimPath);
	}
	virtual bool loadBaseFromXml(pugi::xml_node& xmlAttachment);

	r3dMesh* getMesh( bool allow_async_loading, bool aim_model ) const;

	int getAimMeshRefs() const ;

	// called when unloading level
	virtual void resetMesh() { ModelItemConfig::resetMesh(); m_Model_AIM = 0; }
};

class WeaponConfig : public ModelItemConfig
{
private:
	mutable r3dMesh* m_Model_FPS;
	mutable int	m_ModelRefCount ;

public:

	mutable r3dSkeleton* m_Model_FPS_Skeleton;
	mutable r3dAnimPool*  m_AnimPool_FPS;
	
	// PTUMIK: if adding new skill based items, make sure to add them to DB FN_VALIDATE_LOADOUT proc and also to CUserProfile::isValidInventoryItem()
	enum EUsableItemIDs
	{
		ITEMID_Antibiotics  = 101256, 
		ITEMID_Bandages  = 101261,
		ITEMID_Bandages2 = 101262,
		ITEMID_Painkillers = 101300,
		ITEMID_ZombieRepellent = 101301,
		ITEMID_C01Vaccine = 101302,
		ITEMID_C04Vaccine = 101303,
		ITEMID_Medkit = 101304,
		ITEMID_PieceOfPaper = 101305,	// used to create world notes
		ITEMID_Flashlight = 101306, // ultraporing - flashlight
		ITEMID_AirHorn = 101323,
		ITEMID_Binoculars = 101315,
		ITEMID_RangeFinder = 101319,

		ITEMID_BarbWireBarricade = 101316,
		ITEMID_WoodShieldBarricade = 101317,
		ITEMID_RiotShieldBarricade = 101318,
		ITEMID_SandbagBarricade = 101324,
		ITEMID_WoodenDoor2M = 101352,
			ITEMID_PersonalLocker = 101348,
	};

	char* m_MuzzleParticle;

	char* FNAME;

	int	  m_isConsumable; // for usableitems
	int	  m_ShopStackSize;

	char* m_ModelPath_1st;
	char* m_AnimPrefix; // used for FPS mode, for grips


	class Ammo*	m_PrimaryAmmo;

	r3dSec_type<float, 0xA2204A5C> m_AmmoMass;
	r3dSec_type<float, 0xB5404B5C> m_AmmoSpeed;
	float m_AmmoDamage;
	float m_AmmoDecay;
	float m_AmmoArea;
	float m_AmmoDelay;
	float m_AmmoTimeout;
	bool  m_AmmoImmediate;

	float m_reloadActiveTick; // when active reload becomes available. Duration is not longer than 10% of reload time or 0.4sec maximum; for grenades used as a time when grenade should be launched from a hand
	r3dSec_type<float, 0xA2464A6C> m_spread;  // spread set as diameter at 50meter range

	WeaponAnimTypeEnum m_AnimType;
	r3dSec_type<float, 0xA2A6FAC3> m_recoil;
	uint32_t m_fireModeAvailable; // flags
	r3dSec_type<float, 0x1F27A2F9> m_fireDelay;
	const struct ScopeConfig* m_scopeConfig;
	float						m_scopeZoom; // 0..1; 0 - no zoom. 1 - maximum zoom
	r3dSec_type<float, 0xAD56D4C1> m_reloadTime;

	mutable r3dPoint3D	muzzlerOffset;
	mutable bool		muzzleOffsetDetected;

	r3dPoint3D adjMuzzlerOffset; // used privately, do not use it
	mutable r3dPoint3D shellOffset; // where shells are coming out

	int		m_sndReloadID;

	int		m_sndFireID_single;
	int		m_sndFireID_auto;
	int		m_sndFireID_single_player; // for local player
	int		m_sndFireID_auto_player; // for local player
	
	int*		m_animationIds;
	int*		m_animationIds_FPS;
	
	// fps item attachments
	int		IsFPS;
	int		FPSSpecID[WPN_ATTM_MAX];	// m_specID in WeaponAttachmentConfig for each attachment slot
	int		FPSDefaultID[WPN_ATTM_MAX];	// default attachment item ids in each slot

  private:	
	// make copy constructor and assignment operator inaccessible
	WeaponConfig(const WeaponConfig& rhs);
	WeaponConfig& operator=(const WeaponConfig& rhs);

  public:
	WeaponConfig(uint32_t id) : ModelItemConfig(id)
	{
		m_ModelRefCount = 0 ;

		muzzleOffsetDetected = false ;

		m_Model_FPS = NULL;
		m_Model_FPS_Skeleton = NULL;
		m_AnimPool_FPS = NULL;
		m_MuzzleParticle = NULL;
		FNAME = NULL;
		m_ModelPath_1st = NULL;
		m_PrimaryAmmo = NULL;
		m_scopeConfig = NULL;

		m_AnimPrefix = NULL;

		m_isConsumable = 0;
		m_ShopStackSize = 1;

		m_AmmoMass			= 0.1f;
		m_AmmoSpeed			= 100.f;
		m_AmmoDamage		= 1.0f;
		m_AmmoDecay			= 0.1f;
		m_AmmoArea			= 0.1f;
		m_AmmoDelay			= 0.f;
		m_AmmoTimeout		= 1.0f;
		m_AmmoImmediate		= true;

		m_reloadTime		= 1.0f;
		m_reloadActiveTick	= 0.f; // when active reload becomes available. Duration is not longer than 10% of reload time or 0.4sec maximum; for grenades used as a time when grenade should be launched from a hand
		m_fireDelay			= 0.5f;
		m_spread			= 0.01f; 
		m_recoil			= 0.1f;
		m_AnimType			= WPN_ANIM_ASSAULT;
		m_fireModeAvailable	= WPN_FRM_SINGLE; // flags
		m_scopeZoom			= 0; // 0..1; 0 - no zoom. 1 - maximum zoom

		muzzlerOffset		= r3dPoint3D( 0.25f, 0.f, 0.f );
		adjMuzzlerOffset	= muzzlerOffset ; // used privately, do not use it
		shellOffset			= r3dPoint3D( 0, 0, 0 ); // where shells are coming out

		m_sndReloadID		= -1 ;

		m_sndFireID_single = -1;
		m_sndFireID_auto   = -1;
		m_sndFireID_single_player = -1;
		m_sndFireID_auto_player = -1;
		
		m_animationIds          = NULL;
		m_animationIds_FPS		= NULL;
		
		IsFPS = 0;
		memset(FPSSpecID, 0, sizeof(FPSSpecID));
		memset(FPSDefaultID, 0, sizeof(FPSDefaultID));
	}
	virtual ~WeaponConfig() 
	{
		free(m_MuzzleParticle);
		free(FNAME);
		free(m_ModelPath_1st);
		free(m_AnimPrefix);
		SAFE_DELETE_ARRAY(m_animationIds);
		SAFE_DELETE_ARRAY(m_animationIds_FPS);
	}
	virtual bool loadBaseFromXml(pugi::xml_node& xmlWeapon);

	bool isAttachmentValid(const WeaponAttachmentConfig* wac) const;
	
	bool isSameParameters(const WeaponConfig& rhs) const
	{
		#define DO(XX) if(XX != rhs.XX) return false;
		DO(m_AmmoMass);
		DO(m_AmmoSpeed);
		DO(m_AmmoDamage);
		DO(m_AmmoDecay);
		DO(m_AmmoArea);
		DO(m_AmmoDelay);
		DO(m_AmmoTimeout);

		DO(m_reloadTime);
		DO(m_reloadActiveTick);
		DO(m_fireDelay);
		DO(m_spread); 
		DO(m_recoil);
		#undef DO
		return true;
	}
	void copyParametersTo(GBWeaponInfo& wi) const
	{
		#define DO(XX) wi.XX = XX
		DO(m_AmmoMass);
		DO(m_AmmoSpeed);
		DO(m_AmmoDamage);
		DO(m_AmmoDecay);
		DO(m_AmmoArea);
		DO(m_AmmoDelay);
		DO(m_AmmoTimeout);

		DO(m_reloadTime);
		DO(m_reloadActiveTick);
		DO(m_fireDelay);
		DO(m_spread); 
		DO(m_recoil);
		#undef DO
	}
	void copyParametersFrom(const GBWeaponInfo& wi)
	{
		#define DO(XX) XX = wi.XX
		DO(m_AmmoMass);
		DO(m_AmmoSpeed);
		DO(m_AmmoDamage);
		DO(m_AmmoDecay);
		DO(m_AmmoArea);
		DO(m_AmmoDelay);
		DO(m_AmmoTimeout);

		DO(m_reloadTime);
		DO(m_reloadActiveTick);
		DO(m_fireDelay);
		DO(m_spread); 
		DO(m_recoil);
		#undef DO
	}

	DWORD GetClientParametersHash() const
	{
		VMPROTECT_BeginVirtualization("GetClientParametersHash");
		// hold copy of variables to hash, work with r3dSecType
#pragma pack(push,1)
		struct hash_s 
		{
			float m_spread;
			float m_fireDelay;
			float m_recoil;
			float m_AmmoSpeed;
			float m_reloadTime;
		};
#pragma pack(pop)

		hash_s h;
		h.m_reloadTime = m_reloadTime;
		h.m_fireDelay  = m_fireDelay;
		h.m_spread     = m_spread;
		h.m_recoil     = m_recoil;
		h.m_AmmoSpeed  = m_AmmoSpeed;
		DWORD crc32 = r3dCRC32((BYTE*)&h, sizeof(h));
		VMPROTECT_End();
		return crc32;
	}

	r3dMesh* getMesh( bool allow_async_loading, bool first_person ) const;

	int getConfigMeshRefs() const ;

	// called when unloading level
	virtual void resetMesh() { 
		ModelItemConfig::resetMesh();
		m_Model_FPS = 0; 
		SAFE_DELETE(m_Model_FPS_Skeleton); 
		SAFE_DELETE(m_AnimPool_FPS); 
	}
	void detectMuzzleOffset(bool first_person) const;

	// because mesh can be delay-loaded
	void updateMuzzleOffset(bool first_person) const ;

	r3dSkeleton* getSkeleton() const { return m_Model_FPS_Skeleton; }

	void aquireMesh( bool allow_async_loading ) const ;
	void releaseMesh() const ;

	R3D_FORCEINLINE bool hasFPSModel() const
	{
		return m_Model_FPS ? true : false ;
	}

	R3D_FORCEINLINE bool isFPSModelSkeletal() const
	{
		return m_Model_FPS->IsSkeletal() ? true : false ;
	}
};
