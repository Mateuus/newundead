//=========================================================================
//	Module: CollectionElement.cpp
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "CollectionsManager.h"
#include "CollectionType.h"

//////////////////////////////////////////////////////////////////////////

#ifndef WO_SERVER
extern float g_fCollectionsRandomColor;
#endif

//-------------------------------------------------------------------------
//	CollectionElementSerializableData class
//-------------------------------------------------------------------------

CollectionElementSerializableData::CollectionElementSerializableData()
: pos(0, 0, 0)
, scale(1.0f)
, angle(0)
, tint(0)
, typeIndex(INVALID_COLLECTION_INDEX)
{

}

//////////////////////////////////////////////////////////////////////////

void CollectionElementSerializableData::LoadFromMemory(const char *buf)
{
	size_t size = sizeof(CollectionElementSerializableData);
	memcpy_s(this, size, buf, size);
}

//////////////////////////////////////////////////////////////////////////

void CollectionElementSerializableData::SaveToMemory(char *buf) const
{
	size_t size = sizeof(CollectionElementSerializableData);
	memcpy_s(buf, size, this, size);
}

//-------------------------------------------------------------------------
//	CollectionElement class
//-------------------------------------------------------------------------

CollectionElement::CollectionElement()
: physObj(0)
, physCallbackObj(0)
, shadowExDataDirty(true)
, wasVisible(false)
, bendVal(0.5f)
, bendSpeed(0)
, windPower(0)
, randomColor(0)
, curLod(0)
{

}

//////////////////////////////////////////////////////////////////////////

CollectionElement::~CollectionElement()
{
}

//////////////////////////////////////////////////////////////////////////

void CollectionElement::InitPhysicsData()
{
	CollectionType *ct = gCollectionsManager.GetType(typeIndex);

#ifndef WO_SERVER
	bool hasMesh = ct->meshLOD[0]!=NULL;
#else
	bool hasMesh = ct->serverMeshName[0]!=0;
#endif

	if (ct && hasMesh && ct->physicsEnable && !physObj)
	{
		PhysicsObjectConfig physicsConfig;
#ifndef WO_SERVER
		GameObject::LoadPhysicsConfig(ct->meshLOD[0]->FileName.c_str(), physicsConfig);
#else
		GameObject::LoadPhysicsConfig(ct->serverMeshName, physicsConfig);
#endif

		D3DXMATRIX rotation;
		D3DXMatrixRotationY(&rotation, R3D_DEG2RAD(angle));
		r3dVector size(5, 5, 5);

		physCallbackObj = new CollectionPhysicsCallbackObject();
#ifndef WO_SERVER
		physCallbackObj->mesh = ct->meshLOD[0];
#else
		physCallbackObj->mesh = NULL; // it is okay to pass NULL into createStaticObject for PhysX
#endif
		physObj = BasePhysicsObject::CreateStaticObject(physicsConfig, physCallbackObj, &pos, &size, physCallbackObj->mesh, &rotation);
		if (physObj)
		{
			physObj->SetScale(r3dPoint3D(scale, scale, scale));
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CollectionElement::ClearPhysicsData()
{
	SAFE_DELETE(physObj);
	SAFE_DELETE(physCallbackObj);
}

//////////////////////////////////////////////////////////////////////////
#ifndef WO_SERVER
r3dBoundBox CollectionElement::GetWorldAABB() const
{
	r3dBoundBox bb;
	bb.Org  = r3dPoint3D(0, 0, 0);
	bb.Size = r3dPoint3D(0, 0, 0);

	CollectionType * ct = gCollectionsManager.GetType(typeIndex);
	if (ct && ct->meshLOD[0])
	{
		bb = ct->meshLOD[0]->localBBox;

		D3DXMATRIX rotation, scaling;
		D3DXMatrixRotationY(&rotation, R3D_DEG2RAD(angle));
		D3DXMatrixScaling(&scaling, scale, scale, scale);
		D3DXMatrixMultiply(&rotation, &scaling, &rotation);

		bb.Transform(reinterpret_cast<r3dMatrix*>(&rotation));
		bb.Org += pos;
	}
	return bb;
}

//////////////////////////////////////////////////////////////////////////


void CollectionElement::GenerateRandomColor()
{
	randomColor = u_GetRandom(1.0f - g_fCollectionsRandomColor, 1.0f);
}
#endif
//////////////////////////////////////////////////////////////////////////
#ifndef WO_SERVER
r3dBoundBox CollectionElement_GetAABB(QuadTreeObjectID id)
{
	r3dBoundBox bb;
	bb.Org  = r3dPoint3D(0, 0, 0);
	bb.Size = r3dPoint3D(0, 0, 0);

	CollectionElement *e = gCollectionsManager.GetElement(id);
	if (!e)
		return bb;

	return e->GetWorldAABB();
}
#endif
