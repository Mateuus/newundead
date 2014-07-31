#include "r3dPCH.h"
#include "r3d.h"

#include "ZombieNavAgent.h"
#include "../../GameEngine/ai/AutodeskNav/AutodeskNavMesh.h"
#include "../../GameEngine/gameobjects/PhysXWorld.h"
#include "../../GameEngine/gameobjects/PhysObj.h"

ZombieNavAgent::ZombieNavAgent()
{
}

ZombieNavAgent::~ZombieNavAgent()
{
}

ZombieNavAgent* CreateZombieNavAgent(const r3dPoint3D& pos)
{
	r3d_assert(gAutodeskNavMesh.GetWorld());

	ZombieNavAgent* a = new ZombieNavAgent();
	a->Init(gAutodeskNavMesh.GetWorld(), pos);
	gAutodeskNavMesh.AddNavAgent(a);

	return a;
}

void DeleteZombieNavAgent(ZombieNavAgent* a)
{
	gAutodeskNavMesh.DeleteNavAgent(a);
}
