#pragma once

#include "../../GameCode/UserProfile.h"
#include "r3dProtect.h"

#include "../../../ServerNetPackets/NetPacketsWeaponInfo.h"

// base class for all type of items
class BaseItemConfig
{
public:
	uint32_t m_itemID;
	STORE_CATEGORIES category;

	char* m_Description;
	char* m_StoreIcon;
	char* m_StoreName;
	wchar_t* m_StoreNameW;
	wchar_t* m_DescriptionW;

	float	m_Weight; // in kg

private:	
	// make copy constructor and assignment operator inaccessible
	BaseItemConfig(const BaseItemConfig& rhs);
	BaseItemConfig& operator=(const BaseItemConfig& rhs);

public:
	BaseItemConfig(uint32_t id)
	{
		m_itemID = id;
		category = storecat_INVALID;
		m_StoreIcon = NULL;
		m_StoreName = NULL;
		m_Description = NULL;
		m_StoreNameW = NULL;
		m_DescriptionW = NULL;
		m_Weight = 0.0f;
	}
	virtual ~BaseItemConfig() 
	{
		free(m_StoreIcon);
		free(m_StoreName);
		free(m_Description);
		free(m_StoreNameW);
		free(m_DescriptionW);
	}

	virtual void resetMesh() {}

	virtual bool loadBaseFromXml(pugi::xml_node& xmlItem);
};

// if your item will have model, use this class, otherwise use BaseItemConfig
class ModelItemConfig : public BaseItemConfig
{
protected:
	mutable r3dMesh* m_Model;
public:
	char* m_ModelPath;

	ModelItemConfig(uint32_t id) : BaseItemConfig(id)
	{
		m_Model = NULL;
		m_ModelPath = NULL;
	}
	virtual ~ModelItemConfig() 
	{
		resetMesh();
		free(m_ModelPath);
	}

	const char* getModelPath() { return m_ModelPath ; }

	r3dMesh* getMesh() const;
	int getMeshRefs() const ;

	// called when unloading level
	virtual void resetMesh() { m_Model = 0; }

	virtual bool loadBaseFromXml(pugi::xml_node& xmlItem);
};

class LootBoxConfig : public BaseItemConfig
{
public:
	LootBoxConfig(uint32_t id) : BaseItemConfig(id) {}

	struct LootEntry
	{
		double chance;
		uint32_t itemID;
		int GDMin;
		int GDMax;	
	};
	std::vector<LootEntry> entries;

	virtual bool loadBaseFromXml(pugi::xml_node& xmlItem);
};

// food\water\medical items
class FoodConfig : public ModelItemConfig
{
public:
	// how much of each this item will add\subtract
	float	Health;
	float	Toxicity;
	float	Food;
	float	Water;
	float	Stamina; // from 0 to 1. % of max stamina that it will restore

	int m_ShopStackSize;
public:
	FoodConfig(uint32_t id) : ModelItemConfig(id), m_ShopStackSize(1) {}

	virtual bool loadBaseFromXml(pugi::xml_node& xmlItem);
};


struct ScopeConfig
{
	char name[32];
	bool		hasScopeMode; // instead of just aiming it will show special scope. also will affect camera position and FOV and mouse sensitivity
	bool		lighting;
	bool		hide_player_model;
	r3dTexture* scope_mask; // can be null
	r3dTexture*	scopeBlur_mask; // can be null
	r3dTexture* scope_reticle; // actual scope/reticule whatever else
	r3dTexture* scope_normal;
	r3dTexture* reticule;

	float		scale;

	ScopeConfig(const char* n)
	{
		r3dscpy(name, n);
		scope_mask = 0;
		scopeBlur_mask = 0;
		scope_reticle = 0;
		scope_normal = 0;
		reticule = 0;
		lighting = true;
		hide_player_model = true;
		hasScopeMode = false;
		scale = 1.0f;
	}
	~ScopeConfig()
	{
		if(scope_mask)
			r3dRenderer->DeleteTexture(scope_mask);
		if(scopeBlur_mask)
			r3dRenderer->DeleteTexture(scopeBlur_mask);
		if(scope_reticle)
			r3dRenderer->DeleteTexture(scope_reticle);
		if(scope_normal)
			r3dRenderer->DeleteTexture(scope_normal);
		if(reticule)
			r3dRenderer->DeleteTexture(reticule);
	}
};


struct AbilityConfig
{
	// no abilities yet.
};

enum 
{
	// achievements listed here are offset by one from their id.  Hopefully. 
	// !!! PTumik: this is an INDEX into GameDB.xml <AchievementDB> !!!
	ACHIEVEMENT_FIRST_ACHIEVEMENT = 0,
	NUM_ACHIEVEMENTS,
};

struct AchievementConfig
{
	int id;
	bool enabled;
	char* name; // ID in lang file
	char* desc; // ID in lang file
	char* hud_icon;
	int value;
	AchievementConfig(int n)
	{
		r3d_assert(n>=1);
		id = n;	
		name = 0;
		desc = 0;
		hud_icon = 0;
		value =0;
		enabled = false;
	}
	~AchievementConfig()
	{
		free(name);
		free(desc);
		free(hud_icon);
	}
};
