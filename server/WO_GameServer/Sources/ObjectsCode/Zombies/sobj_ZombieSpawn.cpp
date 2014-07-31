#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "XMLHelpers.h"
#include "ServerGameLogic.h"
#include "../EclipseStudio/Sources/ObjectsCode/weapons/WeaponArmory.h"

#include "sobj_ZombieSpawn.h"
#include "sobj_Zombie.h"
#include "../sobj_Animals.h"

IMPLEMENT_CLASS(obj_ZombieSpawn, "obj_ZombieSpawn", "Object");
AUTOREGISTER_CLASS(obj_ZombieSpawn);

obj_ZombieSpawn::obj_ZombieSpawn()
{
	timeSinceLastSpawn = r3dGetTime() + 10.0f; // give few secs delay before spawning start
	numSpawnedZombies  = 0;

	spawnRadius        = 20;
	maxZombieCount     = 5;
	zombieSpawnDelay   = 15;

	minDetectionRadius = 0;
	maxDetectionRadius = 5;

	// percent in 0-100, they will be convered to 0-1 at ReadSerializedData
	sleepersRate       = 10;
	speedVariation     = 10.0f;
	fastZombieChance   = 50.0f;

	lootBoxID	   = 0;
	lootBoxCfg	   = NULL;
}

obj_ZombieSpawn::~obj_ZombieSpawn()
{
}

BOOL obj_ZombieSpawn::OnCreate()
{
	lootBoxCfg = g_pWeaponArmory->getLootBoxConfig(lootBoxID);

	/*if((GetPosition() - r3dPoint3D(7048, 41, 6931)).Length() > 5)
	{
	r3dOutToLog("obj_ZombieSpawn skipped\n");
	return FALSE;
	}*/

	r3dOutToLog("obj_ZombieSpawn %p, %d zombies in %.0f meters, delay %.0f, r:%.1f-%.1f\n", 
		this,
		maxZombieCount, spawnRadius, zombieSpawnDelay, 
		minDetectionRadius, maxDetectionRadius);
	CLOG_INDENT;

	extern float _zai_MaxSpawnDelay;
	if(zombieSpawnDelay > _zai_MaxSpawnDelay)
		zombieSpawnDelay = _zai_MaxSpawnDelay;

	GenerateNavPoints();

	//for (int i=0;i<2;i++)
	//{
		/*r3dPoint3D curpos = GetPosition();
		r3dPoint3D pos;
		if (GetFreeNavPointIdx(&pos,true,500,&curpos) != -1)
		{
			obj_Animals* obj = 	(obj_Animals*)srv_CreateGameObject("obj_Animals", "obj_Animals", pos);
			obj->SetNetworkID(gServerLogic.GetFreeNetId());
			obj->NetworkLocal = true;
			obj->z = this;
			obj->OnCreate();
		}*/
	//}

	return parent::OnCreate();
}

BOOL obj_ZombieSpawn::OnDestroy()
{
	return parent::OnDestroy();
}

BOOL obj_ZombieSpawn::Update()
{
	if(gServerLogic.net_mapLoaded_LastNetID == 0) // do not spawn zombies until server has fully loaded
		return TRUE;

	parent::Update();

	// check if need to spawn zombie
	if(r3dGetTime() < timeSinceLastSpawn + zombieSpawnDelay)
		return TRUE;
	timeSinceLastSpawn = r3dGetTime();

	// if we have enough zombies	
	if(zombies.size() < maxZombieCount)
	{
		r3dPoint3D pos;
		if(!GetZombieSpawnPosition(&pos))
		{
			return TRUE;
		}

		char name[28];
		sprintf(name, "Zombie_%d_%p", numSpawnedZombies++, this);

		obj_Zombie* z = (obj_Zombie*)srv_CreateGameObject("obj_Zombie", name, pos);
		z->SetNetworkID(gServerLogic.GetFreeNetId());
		z->NetworkLocal = true;
		z->spawnObject  = this;
		z->DetectRadius = u_GetRandom(minDetectionRadius, maxDetectionRadius);
		z->SetRotationVector(r3dVector(u_GetRandom(0, 360), 0, 0));

		zombies.push_back(z);
	}

	return TRUE;
}

int obj_ZombieSpawn::CheckForZombieAtPos(const r3dPoint3D& pos, float distSq)
{
	int num = 0;
	for(size_t i=0, e=zombies.size(); i<e; i++)
	{
		if((zombies[i]->GetPosition() - pos).LengthSq() < distSq)
			num++;
	}

	return num;
}

void obj_ZombieSpawn::GenerateNavPoints()
{
	r3d_assert(navPoints.size() == 0);
	const float NAV_GRID_SIZE = 3.0f; // generate every 3m

	int cells = (int)((spawnRadius + spawnRadius) / NAV_GRID_SIZE) + 1;
	navPoints.reserve(cells * cells);

	float x1 = GetPosition().x - spawnRadius + NAV_GRID_SIZE / 2;
	float x2 = GetPosition().x + spawnRadius;
	float z1 = GetPosition().z - spawnRadius + NAV_GRID_SIZE / 2;
	float z2 = GetPosition().z + spawnRadius;

	r3dPoint3D pos(x1, GetPosition().y, z1);
	for(pos.x = x1; pos.x < x2; pos.x += NAV_GRID_SIZE)
	{
		for(pos.z = z1; pos.z < z2; pos.z += NAV_GRID_SIZE)
		{
			r3dPoint3D npos = pos;
			if(! obj_Zombie::CheckNavPos(npos))
				continue;

			// check that we are not trying to spawn zombie on top of the building (sergey's request)
			if(Terrain)
			{
				float terrHeight = Terrain->GetHeight(npos);
				if((npos.y - terrHeight) > 2.0f) {
					continue;
				}
			}

			navPnt_s npnt;
			npnt.pos    = npos;
			npnt.marked = 0;

			navPoints.push_back(npnt);
		}
	}

	// shuffle positions around
	if(navPoints.size() > 0)
		std::random_shuffle(navPoints.begin(), navPoints.end());

	r3dOutToLog("%d/%d valid navpoints for %d zombies at %f %f %f\n", navPoints.size(), cells * cells, maxZombieCount, GetPosition().x, GetPosition().y, GetPosition().z);
}

int obj_ZombieSpawn::GetFreeNavPointIdx(r3dPoint3D* out_pos, bool mark, float maxDist, const r3dPoint3D* cur_pos)
{
	if(navPoints.size() == 0)
		return -1;

	float maxDistSq = maxDist * maxDist;

	// start from random position in array and scan for first free point
	int found = 0;
	int size  = (int)navPoints.size();
	int start = u_random(size);
	int idx   = start;
	do
	{
		if(maxDist > 0)
		{
			if((navPoints[idx].pos - *cur_pos).LengthSq() > maxDistSq)
			{
				idx = (idx + 1) % size;
				continue;
			}
		}

		if(navPoints[idx].marked == 0)
		{
			if(mark)
			{
				navPoints[idx].marked = mark;
				//r3dOutToLog("SpawnPoint snav%p_%d marked\n", this, idx);
			}
			*out_pos = navPoints[idx].pos;
			return idx;
		}
		else
		{
			found++;
		}

		idx = (idx + 1) % size;

	} while(idx != start);

	//usually that happens whem zombies was attacking someone and went too far from available spawn points
	//r3dOutToLog("SpawnPoint%p had no free navpoints (%d/%d) at %f %f %f with radius %f\n", this, found, navPoints.size(), GetPosition().x, GetPosition().y, GetPosition().z, maxDist);

	return -1;
}

void obj_ZombieSpawn::ReleaseNavPoint(int idx)
{
	r3d_assert(idx < (int)navPoints.size());
	if(navPoints[idx].marked == 0)
		r3dOutToLog("!!! SpawnPoint releasing not used snav%p_%d\n", this, idx);

	//r3dOutToLog("SpawnPoint snav%p_%d released\n", this, idx);
	navPoints[idx].marked = 0;
}

bool obj_ZombieSpawn::GetZombieSpawnPosition(r3dPoint3D* out_pos)
{
	if(!GetFreeNavPointIdx(out_pos, false))
		return false;

	int MAX_SPAWN_TRIES = 5;
	for(int i=0; i<MAX_SPAWN_TRIES; i++)
	{
		// try to not have players around us
		extern float _zai_PlayerSpawnProtect;
		if(gServerLogic.CheckForPlayersAround(*out_pos, _zai_PlayerSpawnProtect))
			continue;

		// try not to have zombies around us
		if(CheckForZombieAtPos(*out_pos, 1.0f * 1.0f) > 1)
			continue;

		// all ok
		return true;
	}

	return false;
}

bool obj_ZombieSpawn::GetZombieRandomNavMeshPosition(r3dPoint3D* out_pos)
{
	r3dPoint3D pos = GetPosition();

	// try few times to spawn zombie
	int MAX_SPAWN_NAVMESH_TRIES = 20;
	for(int i=0; i<MAX_SPAWN_NAVMESH_TRIES; i++)
	{
		// Generate random pos within spawn radius
		float rX = rand() / static_cast<float>(RAND_MAX);
		float rZ = rand() / static_cast<float>(RAND_MAX);

		r3dPoint3D offset(rX, 0, rZ);
		offset = (offset - 0.5f) * 2;
		offset.y = 0;
		offset.Normalize();
		offset *= rand() / static_cast<float>(RAND_MAX) * spawnRadius;
		pos += offset;

		// check that it's valid in navmesh
		if(obj_Zombie::CheckNavPos(pos))
		{
			*out_pos = pos;
			return true;
		}
	}
#ifdef _DEBUG
	r3dOutToLog("!!! obj_ZombieSpawn_NavFail at %f %f %f.\n", GetPosition().x, GetPosition().y, GetPosition().z);
#endif

	return false;
}

// copy from client version
void obj_ZombieSpawn::ReadSerializedData(pugi::xml_node& node)
{
	parent::ReadSerializedData(node);
	pugi::xml_node zombieSpawnNode = node.child("spawn_parameters");
	GetXMLVal("spawn_radius", zombieSpawnNode, &spawnRadius);
	int mzc = maxZombieCount;
	GetXMLVal("max_zombies", zombieSpawnNode, &mzc);
	maxZombieCount = mzc;
	GetXMLVal("sleepers_rate", zombieSpawnNode, &sleepersRate);
	GetXMLVal("zombies_spawn_delay", zombieSpawnNode, &zombieSpawnDelay);
	GetXMLVal("min_detection_radius", zombieSpawnNode, &minDetectionRadius);
	GetXMLVal("max_detection_radius", zombieSpawnNode, &maxDetectionRadius);
	GetXMLVal("lootBoxID", zombieSpawnNode, &lootBoxID);
	GetXMLVal("fastZombieChance", zombieSpawnNode, &fastZombieChance);
	GetXMLVal("speedVariation", zombieSpawnNode, &speedVariation);

	uint32_t numZombies = zombieSpawnNode.attribute("numZombieSelected").as_uint();
	for(uint32_t i=0; i<numZombies; ++i)
	{
		char tempStr[32];
		sprintf(tempStr, "z%d", i);
		pugi::xml_node zNode = zombieSpawnNode.child(tempStr);
		r3d_assert(!zNode.empty());
		ZombieSpawnSelection.push_back(zNode.attribute("id").as_uint());
	}


	// client have it in 0-100 range
	sleepersRate     /= 100.0f;
	speedVariation   /= 100.0f;
	fastZombieChance /= 100.0f;
}

void obj_ZombieSpawn::OnZombieKilled(const obj_Zombie* z)
{
	for(std::vector<obj_Zombie*>::iterator it = zombies.begin(); it != zombies.end(); ++it)
	{
		if(*it == z)
		{
			zombies.erase(it);
			return;
		}
	}

	r3dError("zombie %p isn't found on spawn", z);
}
