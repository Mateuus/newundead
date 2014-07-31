#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"
#include "ObjectsCode/Weapons/WeaponArmory.h"
#include "ObjectsCode/Weapons/HeroConfig.h"
#include "Zombies/sobj_ZombieSpawn.h"
#include "sobj_Animals.h"
#include "ObjectsCode/obj_ServerPlayer.h"
#include "ObjectsCode/sobj_DroppedItem.h"
#include "ObjectsCode/obj_ServerBarricade.h"
#include "ServerWeapons/ServerWeapon.h"

#include "../EclipseStudio/Sources/ObjectsCode/Gameplay/AnimalsStates.h"

#include "../../GameEngine/ai/AutodeskNav/AutodeskNavMesh.h"

IMPLEMENT_CLASS(obj_Animals, "obj_Animals", "Object");
AUTOREGISTER_CLASS(obj_Animals);

obj_Animals::obj_Animals() : 
netMover(this, 1.0f / 10.0f, (float)PKT_C2C_MoveSetCell_s::PLAYER_CELL_RADIUS)
{
	navAgent       = NULL;
	animalstate = EAnimalsStates::AState_Idie;
	Health = 100.0f;
	bDead = false;
}

obj_Animals::~obj_Animals()
{
}

BOOL obj_Animals::OnCreate()
{
	//r3d_assert(!navAgent);

	// there is no checks here, they should be done in ZombieSpawn, so pos is navmesh position

	distToCreateSq = 512 * 512;
	distToDeleteSq = 600 * 600;

	r3dPoint3D pos = GetPosition();
	moveTargetPos = GetPosition();
	navAgent = CreateZombieNavAgent(pos);
	animalstate = EAnimalsStates::AState_Idie;
	nextWalkTime = r3dGetTime()+10.0f;


	gServerLogic.NetRegisterObjectToPeers(this);

	r3dOutToLog("obj_Animals %p created at %.2f %.2f %.2f\n",this,GetPosition().x,GetPosition().y,GetPosition().z);
	return parent::OnCreate();
}
BOOL obj_Animals::OnDestroy()
{
	r3dOutToLog("obj_Animals %p destroyed\n", this);

	if (z)
	{
		r3dPoint3D pos;
		r3dPoint3D curpos = GetPosition();
		if (z->GetFreeNavPointIdx(&pos,true,500,&curpos) != -1) // respawn
		{
			obj_Animals* obj = 	(obj_Animals*)srv_CreateGameObject("obj_Animals", "obj_Animals", pos);
			obj->SetNetworkID(gServerLogic.GetFreeNetId());
			obj->NetworkLocal = true;
			obj->z = z;
			obj->OnCreate();
			r3dOutToLog("obj_Animals %p respawn %p\n",this,obj);
		}
	}

	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));
	return parent::OnDestroy();
}
DefaultPacket* obj_Animals::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateAnimals_s n;
	n.spawnID    = toP2pNetId(GetNetworkID());
	n.spawnPos   = GetPosition();

	*out_size = sizeof(n);
	return &n;
}

BOOL obj_Animals::Update()
{
	// destroy this animals if he dead 15 mins. for clear lag problem.(packet flood)
	if (r3dGetTime() > dietime+900 && bDead)
	{
		setActiveFlag(0);
		return true;
	}

	SetPosition(navAgent->GetPosition());
	if(navAgent->m_status == AutodeskNavAgent::Moving)
	{
		Kaim::Vec3f rot  = navAgent->m_velocity;
		if(rot.GetSquareLength2d() > 0.001f)
		{
			r3dPoint3D v = r3dPoint3D(rot[0], rot[2], rot[1]);
			float angle = atan2f(-v.z, v.x);
			angle = R3D_RAD2DEG(angle);
			SetRotationVector(r3dVector(angle - 90, 0, 0));
		}
	}

	if (navAgent->m_status == navAgent->PathNotFound)
		animalstate = EAnimalsStates::AState_Idie;

	switch (animalstate)
	{

	case EAnimalsStates::AState_Idie:
		{
			if(!navAgent) break;
			ObjectManager& GW = GameWorld();

			if (r3dGetTime() > nextWalkTime)
			{
				r3dPoint3D pos;
				if (GetFreePoint(&pos,30))
				{
					animalstate = EAnimalsStates::AState_Walk;
					navAgent->StartMove(pos);
					moveTargetPos = pos;
				}
			}

			break;
		}
	case EAnimalsStates::AState_JumpSprint:
		{
			if(!navAgent) break;
			navAgent->SetTargetSpeed(9);
			if (r3dGetTime() < lastHit+10.0f && navAgent->m_status == navAgent->Arrived)
			{
				r3dPoint3D pos;
				if (GetFreePoint(&pos,70))
					navAgent->StartMove(pos);
			}
			else if (r3dGetTime() > lastHit+10.0f) // end of survive time. get idie
			{
				if (navAgent->m_status != navAgent->Arrived)
					navAgent->StopMove();

				// keep idie time for 5 secs.
				nextWalkTime = r3dGetTime()+5;
				animalstate = EAnimalsStates::AState_Idie;
			}
			break;
		}
	case EAnimalsStates::AState_Walk:
		{
			if(!navAgent) break;
			navAgent->SetTargetSpeed(1.5f);
			GameObject* trg = GameWorld().GetObject(hardObjLock);
			//moveTargetPos = trg->GetPosition();
			//navAgent->StartMove(moveTargetPos);
			if((GetPosition() - moveTargetPos).LengthSq() < 0.05f || navAgent->m_status == navAgent->Arrived)
			{
				//navAgent->StopMove();
				nextWalkTime = r3dGetTime()+20; // keep idie times
				animalstate = EAnimalsStates::AState_Idie;
			}
			break;
		}
	}

	//navAgent->StartMove(r3dPoint3D(3500,Terrain->GetHeight(3500,5500),5500),1000);
	PKT_S2C_AnimalsMove_s n1;
	n1.angle = GetRotationVector();
	n1.pos = GetPosition();
	n1.state = animalstate;
	gServerLogic.p2pBroadcastToActive(this, &n1, sizeof(n1));
	return parent::Update();
}
bool obj_Animals::GetFreePoint(r3dPoint3D* out_pos , float length)
{
	ObjectManager& GW = GameWorld();
	for (GameObject *targetObj = GW.GetFirstObject(); targetObj; targetObj = GW.GetNextObject(targetObj))
	{
		if (targetObj->Class->Name == "obj_ZombieSpawn")
		{
			obj_ZombieSpawn* spawn = (obj_ZombieSpawn*)targetObj;
			r3dPoint3D pos;
			r3dPoint3D curpos = GetPosition();
			if (spawn->GetFreeNavPointIdx(&pos,true,length,&curpos) != -1)
			{
				*out_pos = pos;
				return true;
			}
		}
	}
	return false;
}
void obj_Animals::ApplyDamage(float damage , GameObject* fromObj)
{
	if (bDead) return;

	// Health Logic.
	damage *= 0.15f; // decreased 85% of damage. hard to kill
	Health -= damage;
	lastHit = r3dGetTime();

	// start survive from player at now!
	if (animalstate != EAnimalsStates::AState_JumpSprint)
	{
		r3dPoint3D pos;
		if (GetFreePoint(&pos,70))
		{
			animalstate = EAnimalsStates::AState_JumpSprint;
			navAgent->StartMove(pos);
		}
	}
	r3dOutToLog("Animals %p Damaged! damage %.2f , health %.2f\n",this,damage,Health);

	if (Health <= 0 && fromObj)
	{
		DoDeath(fromObj);
		return;
	}

	if (Health <= 0)
		DoDeath(NULL);
}

void obj_Animals::DoDeath(GameObject* fromObj)
{
	if (bDead) return;

	r3dOutToLog("DoDeath Animals %p\n",this);

	navAgent->StopMove();
	bDead = true;
	Health = 0;
	dietime = r3dGetTime();

	animalstate = EAnimalsStates::AState_Dead;
	if (fromObj && IsServerPlayer(fromObj))
	{
		obj_ServerPlayer* plr = (obj_ServerPlayer*)fromObj;
		if (plr)
		{
			// add 150 xp
			int xp = 250;
			if (plr->profile_.ProfileData.isPremium && gServerLogic.ginfo_.ispre)
			{
				plr->loadout_->Stats.XP += 500;
				xp = 500;
			}
			else
				plr->loadout_->Stats.XP += 250;

			//plr->profile_.ProfileData.GameDollars += 1200;
			PKT_S2C_AddScore_s n;
			n.ID = 0;
			n.XP = R3D_CLAMP(xp, -30000, 30000);
			n.GD = 1500;
			gServerLogic.p2pSendToPeer(plr->peerId_, plr, &n, sizeof(n));
		}
	}

	// create drop objects.

	// create random position around zombie
	wiInventoryItem wi;
	wi.itemID = 101410; // ready-to-eat meat.
	wi.quantity = 1;
	r3dPoint3D pos = GetPosition();
	pos.y += 0.4f;
	pos.x += u_GetRandom(-1, 1);
	pos.z += u_GetRandom(-1, 1);

	// create network object
	obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", pos);
	obj->SetNetworkID(gServerLogic.GetFreeNetId());
	obj->NetworkLocal = true;
	// vars
	obj->m_Item       = wi;

	// create random position around zombie
	{
		wiInventoryItem wi;
		wi.itemID = 'GOLD'; // GOLD
		wi.quantity = 3000;
		r3dPoint3D pos = GetPosition();
		pos.y += 0.4f;
		pos.x += u_GetRandom(-1, 1);
		pos.z += u_GetRandom(-1, 1);

		// create network object
		obj_DroppedItem* obj = (obj_DroppedItem*)srv_CreateGameObject("obj_DroppedItem", "obj_DroppedItem", pos);
		obj->SetNetworkID(gServerLogic.GetFreeNetId());
		obj->NetworkLocal = true;
		// vars
		obj->m_Item       = wi;
	}
}