#include "r3dpch.h"
#include "r3d.h"

#include "ChemLight.h"
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

IMPLEMENT_CLASS(obj_ChemLight, "obj_ChemLight", "Object");
AUTOREGISTER_CLASS(obj_ChemLight);

obj_ChemLight::obj_ChemLight()
{
	m_Ammo = NULL;
	m_Weapon = 0;
	m_ParticleTracer = NULL;
}

r3dMesh* obj_ChemLight::GetObjectMesh()
{
	r3d_assert(m_Ammo);
	return getModel();
}

r3dMesh* obj_ChemLight::GetObjectLodMesh()
{
	r3d_assert(m_Ammo);
	return getModel();
}

BOOL obj_ChemLight::OnCreate()
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

	return TRUE;
}

BOOL obj_ChemLight::OnDestroy()
{
	if(m_ParticleTracer)
		m_ParticleTracer->bKillDelayed = 1;

	return parent::OnDestroy();
}

void MatrixGetYawPitchRoll ( const D3DXMATRIX & , float &, float &, float & );
BOOL obj_ChemLight::Update()
{
	parent::Update();

	if(m_CreationTime+m_Weapon->m_AmmoDelay < r3dGetTime())
	{
		return FALSE;
	}

	if(m_ParticleTracer)
		m_ParticleTracer->SetPosition(GetPosition());

	return TRUE;
}
