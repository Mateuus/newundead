#pragma once

#include "GameCommon.h"
#include "SharedUsableItem.h"

class NPCObject : public SharedUsableItem
{	
public:
	NPCObject();
	virtual ~NPCObject();

	virtual	BOOL OnCreate();
	virtual	BOOL OnDestroy();

	virtual void OnPreRender();

	virtual	BOOL Update();

	virtual BOOL Load(const char* filename);

	virtual void OnAction() = 0; // event for when NPC is interacted with

private:
	bool LoadSkeleton(const std::string& meshFilename);

	int AddAnimation(const std::string& animationName);

	void SetIdleAnimation();

	void UpdateAnimation();

private:
	int	physXObstacleIndex_;

	r3dSkeleton skeleton_;
	r3dAnimPool animationPool_;
	r3dAnimation animation_;
	
	bool animated_;
};