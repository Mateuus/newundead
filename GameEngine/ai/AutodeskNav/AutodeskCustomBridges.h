//=========================================================================
//	Module: AutodeskCustomBridges.h
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#pragma once

#if ENABLE_AUTODESK_NAVIGATION

#include "gwnavgeneration/input/generatorinputproducer.h"
#include "gwnavruntime/basesystem/iperfmarkerinterface.h"
#include "gwnavruntime/visualsystem/ivisualgeometry.h"
#include "gwnavgeneration/generator/iparallelforinterface.h"

//////////////////////////////////////////////////////////////////////////

class AutodeskGeneratorInputProducer: public Kaim::GeneratorInputProducer
{
public:
	AutodeskGeneratorInputProducer();
	r3dTL::TArray<r3dVector> seedPoints;
	virtual KyResult Produce(const Kaim::GeneratorSector& sector, Kaim::ClientInputConsumer& inputConsumer);
};

//////////////////////////////////////////////////////////////////////////

class AutodeskPerfCounter: public Kaim::IPerfMarkerInterface
{
	char strBuf[0xffff];
	uint32_t newStrOffset;
	r3dTL::TArray<char*> eventsStack;
public:
	AutodeskPerfCounter();
	virtual ~AutodeskPerfCounter();
	virtual void Begin(const char* name);
	virtual void End();
};

// text based fast version
class AutodeskPerfCounter2 : public Kaim::IPerfMarkerInterface
{
public:
	typedef std::tr1::unordered_map<const char*, float> EventType;
	EventType	counters;
	EventType	events;
	
	float		lastTime;
	EventType::iterator lastEvent;
	
	void		Reset();
	void		Dump();

public:
	AutodeskPerfCounter2();
	virtual ~AutodeskPerfCounter2();
	virtual void Begin(const char* name);
	virtual void End();
};

//////////////////////////////////////////////////////////////////////////

class AutodeskNavMeshDrawer: public Kaim::IVisualGeometry
{
	r3dD3DVertexBufferTunnel vertices;

	virtual void DoBeginTriangles();
	virtual void DoPushTriangle(const Kaim::VisualTriangle& triangle);
	virtual void DoEndTriangles();

	uint32_t vtxOffset;
	//	Capacity in vertices
	uint32_t capacity;
	byte *lockPtr;

public:
	AutodeskNavMeshDrawer();
	virtual ~AutodeskNavMeshDrawer();
	void Render();
	bool IsFilled() const { return capacity > 0; }
	void Clear() { capacity = 0; }

	void D3DCreateResource();
	void D3DReleaseResource();
};

//////////////////////////////////////////////////////////////////////////

class AutodeskParallelBuilder: public Kaim::IParallelForInterface
{
public:
	virtual KyResult Init ();
	virtual KyResult ParallelFor (void **elements, KyUInt32 elementsCount, Kaim::IParallelElementFunctor *functor);
	virtual KyResult Terminate();
};

#endif