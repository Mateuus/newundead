//=========================================================================
//	Module: NavGenerationHelpers.cpp
//	Copyright (C) Online Warmongers Group Inc. 2012.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "NavGenerationHelpers.h"
#include "../TrueNature/ITerrain.h"
#include "../TrueNature/Terrain.h"
#include "../../EclipseStudio/Sources/XMLHelpers.h"
#include "../gameobjects/PhysXWorld.h"

#ifndef WO_SERVER
#include "../../EclipseStudio/Sources/Editors/CollectionsManager.h"
#include "r3dBackgroundTaskDispatcher.h"
#endif

//////////////////////////////////////////////////////////////////////////

extern r3dCamera gCam;
extern float g_NoTerrainLevelBaseHeight;

//////////////////////////////////////////////////////////////////////////

namespace Nav
{
	LONG gNavMeshBuildComplete = 1;

	LevelGeometry *gLevelMesh = 0;
	DrawBuildInfo gDrawBuildInfo;

	void CreateTerrainGeomtry(LevelGeometry &lg);
	void CreateAllGameObjectsAndCollections(LevelGeometry &lg);

//////////////////////////////////////////////////////////////////////////

	void CreateTerrainGeomtry(LevelGeometry &lg)
	{
		const int PATCH_SIZE = 64;

		if (!Terrain)
			return;

		PxHeightFieldGeometry g = Terrain->GetHFShape();
		const float colScale = g.columnScale;
		const float rowScale = g.rowScale;
		const float heightScale = g.heightScale;

		PxHeightField *physHF = g.heightField;

		PxU32 nbCols = physHF->getNbColumns();
		PxU32 nbRows = physHF->getNbRows();

		PxU32 patchCols = static_cast<PxU32>(ceilf(nbCols / static_cast<float>(PATCH_SIZE)));
		PxU32 patchRows = static_cast<PxU32>(ceilf(nbRows / static_cast<float>(PATCH_SIZE)));

		lg.meshes.Resize(patchCols * patchRows);
		lg.meshStartObjectsIdx = lg.meshes.Count();

		for (PxU32 pi = 0; pi < patchCols; ++pi)
		{
			for (PxU32 pj = 0; pj < patchRows; ++pj)
			{
				//	Fill patch vertices first.
				LevelGeometry::Mesh &p = lg.meshes[pi * patchRows + pj];

				PxU32 startCol = pi * PATCH_SIZE;
				PxU32 startRow = pj * PATCH_SIZE;

				PxU32 endCol = (pi + 1) * PATCH_SIZE;
				PxU32 endRow = (pj + 1) * PATCH_SIZE;

				PxU32 w = std::min(endCol - startCol + 1, nbCols - startCol);
				PxU32 h = std::min(endRow - startRow + 1, nbRows - startRow);

				p.vertices.Resize(w * h);

				for(PxU32 i = 0; i < w; ++i)
				{
					for(PxU32 j = 0; j < h; ++j)
					{
						PxReal x = PxReal(i + startCol);
						PxReal z = PxReal(j + startRow);
						PxReal y = physHF->getHeight(x, z) * heightScale;
						PxU32 idx = i * h + j;

						p.vertices[idx] = r3dVector(x * colScale, y, z * rowScale);
					}
				}

				//	Now triangulate patch
				const PxU32 nbFaces = (w - 1) * (h - 1) * 2;

				p.indices.Resize(nbFaces * 3);
				int * indices = &p.indices.GetFirst();
				for (PxU32 i = 0; i < (w - 1); ++i) 
				{
					for (PxU32 j = 0; j < (h - 1); ++j) 
					{
						// first triangle
						uint32_t baseIdx = 6 * (i * (h - 1) + j);

						indices[baseIdx + 0] = (i + 1) * h + j; 
						indices[baseIdx + 1] = i * h + j;
						indices[baseIdx + 2] = i * h + j + 1;
						// second triangle
						indices[baseIdx + 3] = (i + 1) * h + j + 1;
						indices[baseIdx + 4] = (i + 1) * h + j;
						indices[baseIdx + 5] = i * h + j + 1;
					}
				}

				//	Filter outside triangles
  				uint32_t indicesCount = gConvexRegionsManager.FilterOutsideTriangles(&p.vertices.GetFirst(), &p.indices.GetFirst(), p.indices.Count());
  				p.indices.Resize(indicesCount);
			}
		}
	}

//////////////////////////////////////////////////////////////////////////
	
	void ResetCachedLevelGeometry()
	{
#ifndef WO_SERVER
		if (gLevelMesh)
		{
			delete gLevelMesh;
			gLevelMesh = 0;
		}
#endif
	}

//////////////////////////////////////////////////////////////////////////

	void RemoveAllGameObjectsAndCollections(LevelGeometry &lg)
	{
		if (lg.meshStartObjectsIdx < lg.meshes.Count())
		{
			lg.meshes.Resize(lg.meshStartObjectsIdx);
		}
	}

//////////////////////////////////////////////////////////////////////////

	bool CreateGeometryFromPhysxGeometry(PxTriangleMeshGeometry &g, const PxTransform &globalPose, LevelGeometry::Mesh &mesh)
	{
		PxTriangleMesh *tm = g.triangleMesh;

		const PxU32 nbVerts = tm->getNbVertices();
		const PxVec3* verts = tm->getVertices();
		PxU32 nbTris = tm->getNbTriangles();
		const void* typelessTris = tm->getTriangles();

		PxU32 numIndices = nbTris * 3;
		mesh.indices.Resize(numIndices);

		if (tm->has16BitTriangleIndices())
		{
			//	Convert indices to 32 bit
			const short *i16 = reinterpret_cast<const short*>(typelessTris);

			for (PxU32 i = 0; i < numIndices; ++i)
			{
				mesh.indices[i] = i16[i];
			}
		}
		else
		{
			int size = numIndices * sizeof(mesh.indices[0]);
			memcpy_s(&mesh.indices[0], size, typelessTris, size);
		}

		//	Transform vertices to world space
		mesh.vertices.Resize(nbVerts);
		for (PxU32 i = 0; i < nbVerts; ++i)
		{
			PxVec3 pos = globalPose.transform(verts[i]);
			mesh.vertices[i].Assign(pos.x, pos.y, pos.z);
		}

		//	Reject out-of-regions triangles
		numIndices = gConvexRegionsManager.FilterOutsideTriangles(&mesh.vertices[0], &mesh.indices[0], numIndices);
		r3d_assert(numIndices % 3 == 0);

		mesh.indices.Resize(numIndices);

		return true;
	}

//////////////////////////////////////////////////////////////////////////

	bool CreateGeometryFromPhysxActor(PxActor *a, LevelGeometry::Mesh &mesh)
	{
		bool rv = false;

		if (!a)
			return rv;

		PxRigidActor *ra = a->isRigidActor();
		if (!ra)
			return rv;

		GameObject* gameObj = NULL;
		PhysicsCallbackObject* userData = static_cast<PhysicsCallbackObject*>(ra->userData);
		if(userData) gameObj = userData->isGameObject();

		r3dCSHolder block(g_pPhysicsWorld->GetConcurrencyGuard());

		PxU32 nbShapes = ra->getNbShapes();
		for (PxU32 i = 0; i < nbShapes; ++i)
		{
			PxShape *s = 0;
			ra->getShapes(&s, 1, i);
			PxGeometryType::Enum gt = s->getGeometryType();
			switch(gt)
			{
			case PxGeometryType::eTRIANGLEMESH:
				{
					PxTriangleMeshGeometry g;
					s->getTriangleMeshGeometry(g);
					PxTransform t = s->getLocalPose().transform(ra->getGlobalPose());
					rv = CreateGeometryFromPhysxGeometry(g, t, mesh);
					break;
				}
			default:
				r3dArtBug("buildNavigation: Unsupported physx mesh type %d, obj: %s\n", gt, gameObj ? gameObj->Name.c_str() : "<unknown>");
			}
		}
		return rv;
	}

//////////////////////////////////////////////////////////////////////////

	void CreateAllGameObjectsAndCollections(LevelGeometry &lg)
	{
		//	Submit level objects triangles
		GameObject *obj = 0;
		int k = 0;

		lg.meshes.Reserve(lg.meshes.Count() + GameWorld().GetNumObjects() + GameWorld().GetStaticObjectCount() + gCollectionsManager.GetElementsCount());

		for( ObjectIterator iter = GameWorld().GetFirstOfAllObjects(); iter.current ; iter = GameWorld().GetNextOfAllObjects( iter ) )
		{
			GameObject* obj = iter.current;

			if (!obj->isActive() || obj->ObjFlags & OBJFLAG_SkipCastRay)
			{
				continue;
			}

			// if it's a mesh objects
			if (!obj->isObjType(OBJTYPE_Mesh) || !obj->PhysicsObject)
			{
				continue;
			}

			//	If it is inside or intersect of tile bb
			const r3dBoundBox &bb = obj->GetBBoxWorld();
			if (!gConvexRegionsManager.IsIntersectSomeRegion(bb))
				continue;

			lg.meshes.Resize(lg.meshes.Count() + 1);
			CreateGeometryFromPhysxActor(obj->PhysicsObject->getPhysicsActor(), lg.meshes.GetLast());
		}

		//	Submit collection elements
		for (uint32_t i = 0; i < gCollectionsManager.GetElementsCount(); ++i)
		{
			CollectionElement *ce = gCollectionsManager.GetElement(i);
			if (!ce || !ce->physObj)
				continue;

			//	If it is inside or intersect of tile bb
			const r3dBoundBox &bb = ce->GetWorldAABB();
			if (!gConvexRegionsManager.IsIntersectSomeRegion(bb))
				continue;

			lg.meshes.Resize(lg.meshes.Count() + 1);
			CreateGeometryFromPhysxActor(ce->physObj->getPhysicsActor(), lg.meshes.GetLast());
		}
	}

//////////////////////////////////////////////////////////////////////////

	void ExportGeometryToObjFormat(LevelGeometry &lg, const r3dString &outFilePath)
	{
		FILE *f = fopen(outFilePath.c_str(), "w");

		char buf[256] = {0};

		uint32_t startIdx = 0;

		for (uint32_t i = 0; i < lg.meshes.Count(); ++i)
		{
			Nav::LevelGeometry::Mesh &p = gLevelMesh->meshes[i];

			for (uint32_t j = 0; j < p.vertices.Count(); ++j)
			{
				const r3dPoint3D &v = p.vertices[j];
				sprintf_s(buf, _countof(buf), "v %2.4f %2.4f %2.4f\n", v.x, v.y, v.z);
				fwrite(buf, strlen(buf), 1, f);
			}

			for (uint32_t k = 0; k < p.indices.Count(); k += 3)
			{
				int i0 = p.indices[k + 0] + 1 + startIdx;
				int i1 = p.indices[k + 1] + 1 + startIdx;
				int i2 = p.indices[k + 2] + 1 + startIdx;
				sprintf_s(buf, _countof(buf), "f %u %u %u\n", i1, i0, i2);
				fwrite(buf, strlen(buf), 1, f);
			}
			startIdx += p.vertices.Count();
		}

		fclose(f);
	}

//////////////////////////////////////////////////////////////////////////

	void PopulateLevelMesh(LevelGeometry *&lg)
	{
		if (!lg)
		{
			lg = new Nav::LevelGeometry;
			Nav::CreateTerrainGeomtry(*lg);
		}
		RemoveAllGameObjectsAndCollections(*lg);
		Nav::CreateAllGameObjectsAndCollections(*lg);
	}

//-------------------------------------------------------------------------
//	DrawBuildInfo class
//-------------------------------------------------------------------------

	DrawBuildInfo::DrawBuildInfo()
	{
		InitializeCriticalSection(&buildInfoStringGuard);
		ZeroMemory(&buildInfoString, _countof(buildInfoString));
	}

//////////////////////////////////////////////////////////////////////////

	DrawBuildInfo::~DrawBuildInfo()
	{
		DeleteCriticalSection(&buildInfoStringGuard);
	}

//////////////////////////////////////////////////////////////////////////

	void DrawBuildInfo::Draw()
	{
		if (gNavMeshBuildComplete != 0)
			return;

		r3dCSHolder block(buildInfoStringGuard);
		Font_Label->PrintF(20, r3dRenderer->ScreenH - 25, r3dColor::white, "Nav mesh builder: %s", buildInfoString);
	}
}

