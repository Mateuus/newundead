#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"

#include "ServerGameLogic.h"
#include "../EclipseStudio/Sources/ObjectsCode/weapons/WeaponArmory.h"

#include "ObjectsCode/obj_ServerItemSpawnPoint.h"
#include "ObjectsCode/sobj_SpawnedItem.h"

IMPLEMENT_CLASS(obj_ServerItemSpawnPoint, "obj_ItemSpawnPoint", "Object");
AUTOREGISTER_CLASS(obj_ServerItemSpawnPoint);

obj_ServerItemSpawnPoint::obj_ServerItemSpawnPoint()
{
	m_spawnAllItemsAtTime = 0;
	m_nextUpdateTime = 0;
	m_LootBoxConfig = NULL;
}

obj_ServerItemSpawnPoint::~obj_ServerItemSpawnPoint()
{
}

BOOL obj_ServerItemSpawnPoint::Load(const char *fname)
{
	// skip mesh loading
	if(!GameObject::Load(fname)) 
		return FALSE;

	return TRUE;
}

BOOL obj_ServerItemSpawnPoint::OnCreate()
{
	// item spawn point can be empty. i don't see any reason for that, but anyway...
	if(m_LootBoxID == 0)
		return FALSE;

	if(!m_OneItemSpawn)
	{
		m_LootBoxConfig = const_cast<LootBoxConfig*>(g_pWeaponArmory->getLootBoxConfig(m_LootBoxID));
		if(m_LootBoxConfig == NULL)
		{
			r3dOutToLog("LootBox: !!!! Failed to get lootbox with ID: %d. Server/Client desync??\n", m_LootBoxID);
			return FALSE;
		}

		if(m_LootBoxConfig->entries.size() == 0)
		{
			r3dOutToLog("LootBox: !!!! m_LootBoxID %d does NOT have items inside\n", m_LootBoxID);
			return FALSE;
		}

		r3dOutToLog("LootBox: %p with ItemID %d, %d items, tick: %.0f\n", this, m_LootBoxID, m_LootBoxConfig->entries.size(), m_TickPeriod);
	}
	else
	{
		const BaseItemConfig* bic = g_pWeaponArmory->getConfig(m_LootBoxID);
		if(bic == NULL)
		{
			r3dOutToLog("LootBox: !!!! Failed to get item with ID: %d. Server/Client desync??\n", m_LootBoxID);
			return FALSE;
		}

		r3dOutToLog("LootBox: %p with ItemID %d, tick: %.0f\n", this, m_LootBoxID, m_TickPeriod);
	}

	m_nextUpdateTime = r3dGetTime() + m_TickPeriod;

	m_spawnAllItemsAtTime = r3dGetTime() + 5.0f;

	return parent::OnCreate();
}

BOOL obj_ServerItemSpawnPoint::Update()
{
	if(gServerLogic.net_mapLoaded_LastNetID > 0) // do not spawn items until server has fully loaded
	{
		if(r3dGetTime() > m_spawnAllItemsAtTime && m_spawnAllItemsAtTime>0)
		{
			m_spawnAllItemsAtTime = 0;
			// sergey's request. spawn all items at server start up
			for(size_t i=0; i<m_SpawnPointsV.size(); ++i)
			{
				if(m_SpawnPointsV[i].itemID == 0)
					SpawnItem((int)i);
			}
		}

		if(r3dGetTime() > m_nextUpdateTime)
		{
			m_nextUpdateTime = r3dGetTime() + m_TickPeriod;

			if(!m_OneItemSpawn)
				if(m_LootBoxConfig->entries.size() == 0)
					return parent::Update();

			for(size_t i=0; i<m_SpawnPointsV.size(); ++i)
			{
				ItemSpawn& spawn = m_SpawnPointsV[i];
				if(spawn.itemID == 0 && spawn.cooldown < r3dGetTime()) 
				{
					SpawnItem((int)i);
					break; // only one item spawn per tickPeriod
				}
			}
		}
	}

	return parent::Update();
}

wiInventoryItem RollItem(const LootBoxConfig* lootCfg, int depth)
{
	wiInventoryItem wi;
	if(depth > 4)
	{
		r3dOutToLog("!!! obj_ServerItemSpawnPoint: arrived to lootbox %d with big depth\n", lootCfg->m_itemID);
		return wi;
	}

	r3d_assert(lootCfg);
	double roll = (double)u_GetRandom();
	r3dOutToLog("Rolling %d %s, %d entries, roll:%f\n", lootCfg->m_itemID, lootCfg->m_StoreName, lootCfg->entries.size(), roll); CLOG_INDENT;

	for(size_t i=0; i<lootCfg->entries.size(); i++)
	{
		const LootBoxConfig::LootEntry& le = lootCfg->entries[i];
		{
			const BaseItemConfig* cfg2 = g_pWeaponArmory->getConfig(le.itemID);
			double	chance = le.chance;
			if (cfg2 && gServerLogic.ginfo_.ispre && cfg2->category != storecat_LootBox)
			{
				STORE_CATEGORIES cat = cfg2->category;
				//if (cat != storecat_ASR && cat != storecat_SNP && cat != storecat_SHTG && cat != storecat_MG && cat != storecat_SMG && cat != storecat_SHTG) // if not weapons decrease 30% chance drop.
				//{
				//// decrease chance for other item (medic , mag , all of item) 20-30% (users request)
    //             chance *= u_GetRandom(0.40f,0.50f);
				//}
				//else 
				if (cat == storecat_SNP) // for sniper rifles. increase 5-15% chance.
				{
				 chance *= u_GetRandom(1.05f,1.15f);
				}
				else if (cat != storecat_HG) // for other weapons. increase 10-20% chance.
				{
				 chance *= u_GetRandom(1.10f,1.20f);
				}
			}

			if (cfg2 && cfg2->category == storecat_SNP && !gServerLogic.ginfo_.ispre && !gServerLogic.ginfo_.ispass)
			{
				chance *= 0.80f;
			}
	
			if (cfg2 && cfg2->category == storecat_SNP && gServerLogic.ginfo_.ispass)
			chance *= 1.05f;

			// Other Server Except PremiumServer not Chance drop Scar
			if(cfg2 && cfg2->m_itemID == 101210 && !gServerLogic.ginfo_.ispre)
				chance = 0;

		//if(gServerLogic.ginfo_.ispre)
        //roll *= 0.80; // increase 20% drop chance for PREMIUM servers.
		//else if (!gServerLogic.ginfo_.ispass)
        //roll *= 1.10; // decrease 10% drop chance for NORMAL servers

		if(roll > chance)
			continue;
		if(le.itemID == 0)
		{
			wi.itemID   = 'GOLD'; // special item for GameDollars
			wi.quantity = (int)u_GetRandom((float)le.GDMin, (float)le.GDMax);
			break;
		}

		wi.itemID   = le.itemID;
		wi.quantity = 1;

		// -1 is special "no roll" code
		if(wi.itemID == -1) {
			wi.itemID = 0;
			break;
		}

		// check if this is nested lootbox
		//const BaseItemConfig* cfg2 = g_pWeaponArmory->getConfig(wi.itemID);
		if(!cfg2) {
			r3dOutToLog("!! lootbox %d contain not existing item %d\n", lootCfg->m_itemID, wi.itemID);
			wi.itemID = 0;
			break;
		}
		r3dOutToLog("won %d %s\n", cfg2->m_itemID, cfg2->m_StoreName);

		if (cfg2->category == storecat_SNP && !gServerLogic.ginfo_.ispre)
		{
		r3dOutToLog("LootBox Can Rolling a Sniper. random is dropping...\n");
		bool drop = u_GetRandom() >= 0.90f ? true : false;
        r3dOutToLog("Rolling result %d\n",(int)drop);

		if (!drop)
		{
			wi.itemID = 0;
			return wi;
		}
		}

		if(cfg2->category == storecat_LootBox)
			return RollItem((const LootBoxConfig*)cfg2, ++depth);
		break;
		}
	}
	
	return wi;
}

void obj_ServerItemSpawnPoint::SpawnItem(int spawnIndex)
{
	r3dOutToLog("obj_ServerItemSpawnPoint %p rolling on %d\n", this, spawnIndex); CLOG_INDENT;
	ItemSpawn& spawn = m_SpawnPointsV[spawnIndex];
	
	// roll item and mark it inside spawn
	wiInventoryItem wi;
	if(!m_OneItemSpawn)
	{
		//if (!gServerLogic.ginfo_.ispre)
		wi = RollItem(m_LootBoxConfig, 0);
		//else
		//{
			
		//}
	}
	else
	{
		wi.itemID = m_LootBoxID;
		wi.quantity = 1;
	}
	spawn.itemID = wi.itemID;

	// check if somehow roll wasn't successful
	if(wi.itemID == 0)
	{
		spawn.cooldown = m_Cooldown + r3dGetTime(); // mark that spawn point not to spawn anything until next cooldown
		return;
	}

	// create network object
	obj_SpawnedItem* obj = (obj_SpawnedItem*)srv_CreateGameObject("obj_SpawnedItem", "obj_SpawnedItem", spawn.pos);
	obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	// vars
	if(m_DestroyItemTimer > 0.0f)
		obj->m_DestroyIn = r3dGetTime() + m_DestroyItemTimer;
	obj->m_SpawnObj   = GetSafeID();
	obj->m_SpawnIdx   = spawnIndex;
	obj->m_Item       = wi;
	
	return;
}


void obj_ServerItemSpawnPoint::PickUpObject(int spawnIndex)
{
	ItemSpawn& spawn = m_SpawnPointsV[spawnIndex];
	spawn.itemID   = 0;
	spawn.cooldown = m_Cooldown + r3dGetTime();
}