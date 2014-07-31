#include "r3dpch.h"
#include "r3d.h"

#include "Flare.h"
#include "ObjectsCode\Effects\obj_ParticleSystem.h"
#include "..\world\DecalChief.h"
#include "..\world\MaterialTypes.h"
#include "ExplosionVisualController.h"
#include "FlashbangVisualController.h"
#include "Gameplay_Params.h"

#include "multiplayer/P2PMessages.h"

#include "..\..\multiplayer\ClientGameLogic.h"
#include "..\ai\AI_Player.H"
#include "WeaponConfig.h"
#include "Weapon.h"

IMPLEMENT_CLASS(obj_Flare, "obj_Flare", "Object");
AUTOREGISTER_CLASS(obj_Flare);

obj_Flare::obj_Flare()
{
	m_Ammo = NULL;
	m_Weapon = 0;
	m_ParticleTracer = NULL;
	m_sndThrow = 0;
	m_sndBurn = 0;
}

r3dMesh* obj_Flare::GetObjectMesh()
{
	r3d_assert(m_Ammo);
	return getModel();
}

r3dMesh* obj_Flare::GetObjectLodMesh()
{
	r3d_assert(m_Ammo);
	return getModel();
}

BOOL obj_Flare::OnCreate()
{
	const GameObject* owner = GameWorld().GetObject(ownerID);
	if(!owner)
		return FALSE;

	m_isSerializable = false;

	ReadPhysicsConfig();
	PhysicsConfig.group = PHYSCOLL_PROJECTILES;
	PhysicsConfig.isFastMoving = true;

	r3d_assert(m_Ammo);
	r3d_assert(m_Weapon);

	parent::OnCreate();

	m_CreationTime = r3dGetTime() - m_AddedDelay;
	m_CreationPos = GetPosition();

	if(m_Ammo->getParticleTracer())
		m_ParticleTracer = (obj_ParticleSystem*)srv_CreateGameObject("obj_ParticleSystem", m_Ammo->getParticleTracer(), GetPosition() );

	SetBBoxLocal( GetObjectMesh()->localBBox ) ;
	UpdateTransform();

	m_FireDirection.y += 0.1f; // to make grenade fly where you point
	m_AppliedVelocity = m_FireDirection*m_Weapon->m_AmmoSpeed;
	SetVelocity(m_AppliedVelocity);

	m_sndThrow = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_THROWFLARE"), GetPosition());
	m_sndBurn = SoundSys.Play(SoundSys.GetEventIDByPath("Sounds/WarZ/PlayerSounds/PLAYER_FLARELOOP"), GetPosition());

	return TRUE;
}

BOOL obj_Flare::OnDestroy()
{
	if(m_ParticleTracer)
		m_ParticleTracer->bKillDelayed = 1;

	if(m_sndThrow)
	{
		SoundSys.Release(m_sndThrow); 
		m_sndThrow = NULL;
	}
	if(m_sndBurn)
	{
		SoundSys.Release(m_sndBurn); 
		m_sndBurn = NULL;
	}

	return parent::OnDestroy();
}

void MatrixGetYawPitchRoll ( const D3DXMATRIX & , float &, float &, float & );
BOOL obj_Flare::Update()
{
	parent::Update();

	if(m_sndBurn)
	{
		r3dPoint3D pos = GetPosition();
		SoundSys.SetSoundPos(m_sndBurn, pos);
	}

	if(m_CreationTime+m_Weapon->m_AmmoDelay < r3dGetTime())
	{
		return FALSE;
	}

	if(m_ParticleTracer)
	{
		D3DXMATRIX rot = GetRotationMatrix();
		r3dPoint3D offset = r3dPoint3D(rot._21, rot._22, rot._23);
		m_ParticleTracer->SetPosition(GetPosition() + offset*0.25f);
	}

	return TRUE;
}
