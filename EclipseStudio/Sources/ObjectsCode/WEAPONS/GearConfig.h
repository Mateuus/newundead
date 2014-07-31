#pragma once

#include "BaseItemConfig.h"

class GearConfig : public ModelItemConfig
{
private:
	mutable r3dMesh* m_FirstPersonModel;
	mutable int m_ModelRefCount ;
public:
	char* m_ModelPath_1st; // 1st person model

	float m_damagePerc;
	float m_damageMax;
	float m_bulkiness;
	float m_inaccuracy;
	float m_stealth;
	int   m_ProtectionLevel;

public:
	GearConfig(uint32_t id) : ModelItemConfig(id)
	{
		m_ModelRefCount = 0 ;

		m_FirstPersonModel = NULL;
		m_ModelPath_1st = NULL;
	}
	virtual ~GearConfig() 
	{
		free(m_ModelPath_1st);
	}

	void aquireMesh() const ;
	void releaseMesh() const ;

	virtual bool loadBaseFromXml(pugi::xml_node& xmlGear);

	bool isSameParameters(const GearConfig& rhs) const
	{
#define DO(XX) if(XX != rhs.XX) return false;
		DO(m_damagePerc);
		DO(m_damageMax);
		DO(m_bulkiness);
		DO(m_inaccuracy);
		DO(m_stealth);
		DO(m_ProtectionLevel);
#undef DO
		return true;
	}
	void copyParametersTo(GBGearInfo& wi) const
	{
#define DO(XX) wi.XX = XX
		DO(m_damagePerc);
		DO(m_damageMax);
		DO(m_bulkiness);
		DO(m_inaccuracy);
		DO(m_stealth);
		DO(m_ProtectionLevel);
#undef DO
	}
	void copyParametersFrom(const GBGearInfo& wi)
	{
#define DO(XX) XX = wi.XX
		DO(m_damagePerc);
		DO(m_damageMax);
		DO(m_bulkiness);
		DO(m_inaccuracy);
		DO(m_stealth);
		DO(m_ProtectionLevel);
#undef DO
	}

	const char* getFirstPersonModelPath() const { return m_ModelPath_1st; }

	r3dMesh* getFirstPersonMesh() const;

	int getConfigMeshRefs() const ;

	// called when unloading level
	virtual void resetMesh() { ModelItemConfig::resetMesh(); m_FirstPersonModel = 0; }
};

class BackpackConfig : public ModelItemConfig
{
public:
	int m_maxSlots;
	float m_maxWeight;

public:
	BackpackConfig(uint32_t id) : ModelItemConfig(id)
	{
	}
	virtual ~BackpackConfig() 
	{
	}

	virtual bool loadBaseFromXml(pugi::xml_node& xmlItem);
};
