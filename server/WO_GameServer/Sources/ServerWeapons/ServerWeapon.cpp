#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "ServerWeapon.h"
#include "../ObjectsCode/obj_ServerPlayer.h"
#include "ServerGameLogic.h"
#include "../EclipseStudio/Sources/ObjectsCode/weapons/WeaponArmory.h"

ServerWeapon::ServerWeapon(const WeaponConfig* conf, obj_ServerPlayer* owner, int backpackIdx, const wiWeaponAttachment& attm)
{
	m_pConfig     = conf;
	m_Owner       = owner;
	m_BackpackIdx = backpackIdx;

	memset(m_Attachments, 0, sizeof(m_Attachments));
	memset(m_WeaponAttachmentStats, 0, sizeof(m_WeaponAttachmentStats));
	
	// init default attachments
	for(int i=0; i<WPN_ATTM_MAX; i++)
	{
		uint32_t id = attm.attachments[i] ? attm.attachments[i] : m_pConfig->FPSDefaultID[i];
		const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(id);
		
		// verify attachment is legit
		if(!m_pConfig->isAttachmentValid(wac))
			wac = NULL;
		
		m_Attachments[i] = wac;
	}
	recalcAttachmentsStats();

	wiInventoryItem& wi = getPlayerItem();
	const WeaponAttachmentConfig* clipCfg = getClipConfig();
	// check if we need to modify starting ammo. (SERVER CODE SYNC POINT)
	if(wi.Var1 < 0) 
	{
		if(clipCfg)
			wi.Var1 = clipCfg->m_Clipsize;
		else
			wi.Var1 = 0;
	}
	// set itemid of clip (SERVER CODE SYNC POINT)
	if(wi.Var2 < 0 && clipCfg)
	{
		wi.Var2 = clipCfg->m_itemID;
	}
}

ServerWeapon::~ServerWeapon()
{
}

const WeaponAttachmentConfig* ServerWeapon::getClipConfig()
{
	// (SERVER CODE SYNC POINT)
	return m_Attachments[WPN_ATTM_CLIP];
}

wiInventoryItem& ServerWeapon::getPlayerItem()
{

	wiInventoryItem& wi = m_Owner->loadout_->Items[m_BackpackIdx];
	r3d_assert(wi.itemID == m_pConfig->m_itemID);	// make sure it wasn't changed
	return wi;
}

void ServerWeapon::recalcAttachmentsStats()
{
	memset(m_WeaponAttachmentStats, 0, sizeof(m_WeaponAttachmentStats));

	// recalculate stats
	for(int i=0; i<WPN_ATTM_MAX; ++i)
	{
		if(m_Attachments[i]!=NULL)
		{
			m_WeaponAttachmentStats[WPN_ATTM_STAT_DAMAGE]   += m_Attachments[i]->m_Damage;
			m_WeaponAttachmentStats[WPN_ATTM_STAT_RANGE]    += m_Attachments[i]->m_Range;
			m_WeaponAttachmentStats[WPN_ATTM_STAT_FIRERATE] += m_Attachments[i]->m_Firerate;
			m_WeaponAttachmentStats[WPN_ATTM_STAT_RECOIL]   += m_Attachments[i]->m_Recoil;
			m_WeaponAttachmentStats[WPN_ATTM_STAT_SPREAD]   += m_Attachments[i]->m_Spread;
		}
	}

	// convert stats to percents (from -100..100 range to -1..1)
	m_WeaponAttachmentStats[WPN_ATTM_STAT_DAMAGE] *= 0.01f; 
	m_WeaponAttachmentStats[WPN_ATTM_STAT_RANGE] *= 0.01f; 
	m_WeaponAttachmentStats[WPN_ATTM_STAT_FIRERATE] *= 0.01f; 
	m_WeaponAttachmentStats[WPN_ATTM_STAT_RECOIL] *= 0.01f; 
	m_WeaponAttachmentStats[WPN_ATTM_STAT_SPREAD] *= 0.01f;
}

// dist = distance to target
float ServerWeapon::calcDamage(float dist) const
{
	const obj_ServerPlayer* plr = (obj_ServerPlayer*)m_Owner;
	if(plr == NULL) 
	{
		r3dOutToLog("!!! weapon owner is null\n");
		return 0;
	}

	float range = m_pConfig->m_AmmoDecay;
	if(range <= 0)
	{
		gServerLogic.LogInfo(plr->peerId_, "!!! m_AmmoDecay is <= 0", "weapon ID: %d", m_pConfig->m_itemID);
		range = 1;
	}
	range = range * (1.0f+m_WeaponAttachmentStats[WPN_ATTM_STAT_RANGE]);

	float ammoDmg = m_pConfig->m_AmmoDamage;
	if(m_pConfig->category == storecat_MELEE)
		return ammoDmg;

	ammoDmg = ammoDmg * (1.0f + m_WeaponAttachmentStats[WPN_ATTM_STAT_DAMAGE]);

	// new formula
	// if < 100% range = damage 100%
	// if > range - damage 50%
	if(dist < range)
		return ammoDmg;
	else
		return ammoDmg*0.5f;
}
