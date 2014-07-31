#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "NPCObject.h"
#include "../AI/AI_Player.H"

NPCObject::NPCObject() :
animated_(false),
physXObstacleIndex_(-1)
{
	ObjTypeFlags |= OBJTYPE_NPC;
}

NPCObject::~NPCObject()
{
}

BOOL NPCObject::OnCreate()
{
	m_spawnPos = GetPosition();

	r3d_assert(physXObstacleIndex_ == -1);
	physXObstacleIndex_ = AcquirePlayerObstacle(GetPosition());

	animation_.Init(&skeleton_, &animationPool_);
	SetIdleAnimation();

	return parent::OnCreate();
}

BOOL NPCObject::OnDestroy()
{
	ReleasePlayerObstacle(&physXObstacleIndex_);
	return parent::OnDestroy();
}

void NPCObject::OnPreRender()
{
	animation_.GetCurrentSkeleton()->SetShaderConstants();
}

void NPCObject::SetIdleAnimation()
{
	int aid = -1;
	std::string filename = FileName.c_str();
	if (filename.find_first_of("Civ_") != std::string::npos)
	{
		if (filename.find_first_of("_01") != std::string::npos)
		{
			aid = AddAnimation("Civ_Idle_01");
		}
		else if (filename.find_first_of("_02") != std::string::npos)
		{
			aid = AddAnimation("Civ_Idle_02");
		}
	}

	if (aid != -1)
	{
		// start with randomized frame
		const int tr = animation_.StartAnimation(aid, ANIMFLAG_Looped, 0.0f, 0.0f, 0.0f);
		r3dAnimation::r3dAnimInfo* ai = animation_.GetTrack(tr);
		ai->fCurFrame = u_GetRandom(0, (float)ai->pAnim->NumFrames - 1);

		animated_ = true;
	}
}

void NPCObject::UpdateAnimation()
{
	if (animated_)
	{
		const float timePassed = r3dGetFrameTime();
		D3DXMATRIX mr;
		D3DXMatrixIdentity(&mr);
		animation_.Update(timePassed, r3dPoint3D(0,0,0), mr);
		animation_.Recalc();
	}
}

BOOL NPCObject::Update()
{
	UpdateAnimation();

	return parent::Update();
}

bool NPCObject::LoadSkeleton(const std::string& meshFilename)
{
	std::string skeletonFilename = meshFilename.substr(0, meshFilename.length()-3) + "skl";
	if (r3d_access(skeletonFilename.c_str(), 0) == 0)
	{
		skeleton_.LoadBinary(skeletonFilename.c_str());
	}
	return skeleton_.bLoaded ? true : false;
}

int NPCObject::AddAnimation(const std::string& animationName)
{
	static const char* animationDir = GLOBAL_ANIM_FOLDER;

	std::stringstream ss;
	ss << animationDir << "/" << animationName << ".anm";

	int aid = animationPool_.Add(animationName.c_str(), ss.str().c_str());
	if(aid == -1)
	{
		r3dError("can't add %s anim", animationName);
	}
	return aid;
}

BOOL NPCObject::Load(const char* filename)
{
	if(!MeshGameObject::Load(filename))
	{
		return FALSE;
	}

	if (!LoadSkeleton(filename))
	{
		return FALSE;
	}

	return TRUE;
}