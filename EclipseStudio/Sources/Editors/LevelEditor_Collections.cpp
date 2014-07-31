#include "r3dPCH.h"
#include "LevelEditor_Collections.h"
#include "../SF/RenderBuffer.h"

#include "ObjectsCode\Nature\wind.h"
#include "CollectionElementProxyObject.h"
#include "../../../GameEngine/gameobjects/GenericQuadTree.h"
#include "CollectionsManager.h"

#define R3D_ENABLE_TREE_WIND 1

float BrushRadius = 10;
int CollectionPaintMode = 1;
float g_fCollectionsRandomColor = 0.3f;

int gCollectionsCastShadows = 1;
float gCollectionsCastShadowDistance = 1000.0f;

r3dPoint3D gLastMousePos;

extern r3dPoint3D UI_TargetPos;
extern r3dPoint3D UI_TerraTargetPos;
void r3dDrawCircle3DT(const r3dPoint3D& P1, const float Radius, const r3dCamera &Cam, float Width, const r3dColor24& clr, r3dTexture *Tex = NULL);

static float SliderX = -1;
static float SliderY = 45;
bool imgui_Button_Bill(float sx, float sy, float sw, float sh, const char* name, int selected);

//////////////////////////////////////////////////////////////////////////

void Update_Random_Color()
{
	for (uint32_t i = 0; i < gCollectionsManager.GetElementsCount(); ++i)
	{
		CollectionElement *ce = gCollectionsManager.GetElement(i);
		if (ce)
		{
			ce->GenerateRandomColor();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void DrawPossibleElementsChoose(bool flag);

void Draw_Collection_Toolbar()
{
#ifndef FINAL_BUILD
	float SliderX = r3dRenderer->ScreenW-375;
	float SliderY = 50;

	const float DEFAULT_CONTROLS_WIDTH = 360.0f;
	const float DEFAULT_CONTROLS_HEIGHT = 22.0f;

	g_pDesktopManager->Begin( "ed_collections" );

	// This is toolbar control in case you need it 
	SliderY += imgui_Value_Slider(SliderX, SliderY, "Brush Radius",		&BrushRadius, 1,1000,"%-02.2f",1);
	SliderY += imgui_Checkbox ( SliderX, SliderY, "Cast Shadows",		&gCollectionsCastShadows, 1 );
	SliderY += imgui_Value_Slider(SliderX, SliderY, "Cast Shadow Dist",	&gCollectionsCastShadowDistance, 1,2000,"%-02.2f",1);

	int showCells = r_show_collection_grid->GetInt();
	int showCells2D = showCells & 1;
	int showCells3D = (showCells & 2) >> 1;
	SliderY += imgui_Checkbox( SliderX, SliderY, (int)360.f, (int)22.f, "Show Cells 2D", &showCells2D, 1 ) ;
	SliderY += imgui_Checkbox( SliderX, SliderY, (int)360.f, (int)22.f, "Show Cells 3D", &showCells3D, 1 ) ;

	r_show_collection_grid->SetInt( showCells2D | (showCells3D << 1) ) ;

	float fLastRandomClr = g_fCollectionsRandomColor;

	SliderY += imgui_Value_Slider(SliderX, SliderY, "Random Color",	&g_fCollectionsRandomColor, 0,1,"%-0.2f",1);

	if ( fabsf ( g_fCollectionsRandomColor - fLastRandomClr ) > 0.01f )
	{
		Update_Random_Color ();
	}

	// Editing mode - what we do - erase trees or add them
	const static char* listpaint[] = { "ERASER", "BRUSH","ERASE ALL" };
	SliderY += imgui_Toolbar(SliderX, SliderY, 360, 45, &CollectionPaintMode, 0, listpaint, _countof(listpaint));

	SliderY = LevelEditor.DrawPaintBounds( SliderX, SliderY );

 	void Fix_Height();
 	if(imgui_Button(SliderX, SliderY, 300, 25, "REPOSITION ON TERRAIN", 0)) {
 		Fix_Height();
 	}

	SliderY += 30;

	if (imgui_Button(SliderX, SliderY, 147, 25, "Create proxy objects", 0))
	{
		CreateProxyObjectsForAllCollections();
	}

	if (imgui_Button(SliderX + 153, SliderY, 147, 25, "Delete proxy objects", 0))
	{
		DeleteProxyObjectsForCollections();
	}

	SliderY += 30;

// 	if(imgui_Button(SliderX, SliderY, 90, 25, "Add Object", 0)) 
// 	{
// 		Collections.DataDirectory.push_back(gInstanceInfoData[0].mMeshName);
// 		Collections.NumObjects++;
// 	}
// 	SliderY += 30;

	static int choose = 0;
	SliderY += imgui_Checkbox(SliderX, SliderY, "Available Decorators", &choose, 1);
	DrawPossibleElementsChoose(choose!=0);

	const char* toolbar_list[] =	{ "Edit Layers",	"Mesh Params"	};
	enum							{ CE_EDITLAYERS,	CE_MESHPARAMS	};

	static int EditMode = 0;

	SliderY += imgui_Toolbar( SliderX, SliderY, 360.f, 26.f, &EditMode, 0, toolbar_list, sizeof toolbar_list / sizeof toolbar_list[ 0 ] );

	switch( EditMode )
	{
	case CE_EDITLAYERS:
		{
			for (uint32_t i = 0; i < gCollectionsManager.GetTypesCount(); i++)
			{
				int typeIndex = static_cast<int>(i);
				CollectionType *t = gCollectionsManager.GetType(i);
				if (!t || !t->meshLOD[0])
					continue;

				CollectionType::GroupInfo &currentGroup = t->groupInfo;

				// Checkbox if you need it  
				static int CheckBoxValue = 0;
				char string[200];
				sprintf(string, "%c", currentGroup.ExpandedProperties ? '>' : 'V');

				if (imgui_Button(SliderX-20, SliderY, 20, 25, string, 0))
				{
					t->groupInfo.ExpandedProperties ^= 1;
				}

 				if(imgui_Button(SliderX+330, SliderY, 20, 25, "X", 0)) 
				{
					gCollectionsManager.DeleteType(i);
 					continue;
 				}

				//imgui_Checkbox_Small(SliderX - 20, SliderY,20,string,&gInstanceGroupVisible[gCurrentLayer][t], 1);
				sprintf(string,"Use %s", t->meshLOD[0]->Name);
				SliderY += imgui_Checkbox(SliderX, SliderY, 330, 25, string, &currentGroup.Active, 1);

				if(currentGroup.ExpandedProperties)
				{
					SliderY += imgui_Value_Slider(SliderX + 10, SliderY, "Rotate Var",	&currentGroup.Rotate, 0,180,"%-02.0f",1);
					//SliderY += imgui_Value_Slider(SliderX + 10, SliderY, "Scale Var",	&gInstanceGroupScale[gCurrentLayer][t], 0,100,"%-02.0f",1);
					SliderY += imgui_Value_Slider(SliderX + 10, SliderY, "Spacing",		&currentGroup.Spacing, 0.5f,300,"%-02.1f",1);
					int offset = 10;
					int wid = 65;
					// imgui_Checkbox_Small(SliderX+offset, SliderY,wid,"Slope",&gInstanceGroupSlope[gCurrentLayer][t],1); 
					// offset+=wid;
					// SliderY+=imgui_Checkbox_Small(SliderX+offset, SliderY,wid-10,"Tint",&gDoInstanceTint[t],1); 
 					SliderY+=imgui_Value_Slider(SliderX+offset, SliderY, "LOD1 Dist",		&t->lod1Dist, 0.0,9999,"%-02.0f",1); 
					SliderY+=imgui_Value_Slider(SliderX+offset, SliderY, "LOD2 Dist",		&t->lod2Dist, 0.0,9999,"%-02.0f",1); 
					SliderY+=imgui_Value_Slider(SliderX+offset, SliderY, "Render Dist",		&t->renderDist, 100,9999,"%-02.0f",1); 

					bool oldPhys = t->physicsEnable;
					int iHasAnim = t->hasAnimation ? 1 : 0;
					int iPhysEnabled = t->physicsEnable ? 1 : 0;
					SliderY += imgui_Checkbox(SliderX+10, SliderY, "Has Animation", &iHasAnim, 1 );
					SliderY += imgui_Checkbox(SliderX+10, SliderY, "Phys", &iPhysEnabled, 1 );

					t->hasAnimation = !!iHasAnim;
					t->physicsEnable = !!iPhysEnabled;

					if (oldPhys != t->physicsEnable)
					{
						for (uint32_t i = 0; i < gCollectionsManager.GetElementsCount(); ++i)
						{
							CollectionElement *ce = gCollectionsManager.GetElement(i);
							if (ce && ce->typeIndex == typeIndex)
							{
								t->physicsEnable ? ce->InitPhysicsData() : ce->ClearPhysicsData();
							}
						}
					}
				}
			}
		}
		break;

	case CE_MESHPARAMS:
		{
			SliderY += imgui_Static( SliderX, SliderY, "Level Wind Params" ) ;

			r3dWind::Settings sts = g_pWind->GetSettings() ;

			SliderY += imgui_Value_Slider( SliderX, SliderY, "Direction.X", &sts.DirX, -1.f, 1.f, "%.2f" ) ;
			SliderY += imgui_Value_Slider( SliderX, SliderY, "Direction.Z", &sts.DirZ, -1.f, 1.f, "%.2f" ) ;

			SliderY += imgui_Value_Slider( SliderX, SliderY, "Pattern Scale", &sts.WindPatternScale, 2.f, 2048.f, "%.0f" ) ;
			SliderY += imgui_Value_Slider( SliderX, SliderY, "Pattern Speed", &sts.WindPatternSpeed, 1.f, 2048.f, "%.1f" ) ;

			SliderY += imgui_Checkbox( SliderX, SliderY, "Force Wind", &sts.ForceWind, 1 ) ;
			SliderY += imgui_Value_Slider( SliderX, SliderY, "Force Val", &sts.ForceWindVal, 0.f, 1.f, "%.2f" ) ;

			int show_wind = r_show_wind->GetInt() ;

			SliderY += imgui_Checkbox( SliderX, SliderY, 360, 22, "Show Wind", &show_wind, 1 ) ;

			r_show_wind->SetInt( show_wind ) ;

			g_pWind->SetSettings( sts ) ;

			SliderY += 24.f;

			for (uint32_t i = 0; i < gCollectionsManager.GetTypesCount(); i++)
			{
				char string[256];

				CollectionType *t = gCollectionsManager.GetType(i);
				if (!t || !t->meshLOD[0])
					continue;

				sprintf(string,"%c", t->UIVisible ? '>' : 'V');
				
				if (imgui_Button(SliderX-20, SliderY, 20, 25, string, 0))
				{
					t->UIVisible^=1;
				}

				sprintf(string, "%s", t->meshLOD[0]->Name);
				SliderY += imgui_Static(SliderX, SliderY,string );

				if(t->UIVisible)
				{
					for( int i = 0 ; i < CollectionType::MAX_ANIM_LAYERS ; i ++ )
					{
						CollectionType::AnimLayer& layer = t->AnimLayers[ i ] ; 

						char buf[ 64 ] ;
						sprintf( buf, "Anim Layer %d", i ) ;

						SliderY += imgui_Static( SliderX, SliderY, buf, 360 ) ;
 
						SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Scale",		&layer.Scale,		0,	i ? 1.0f : 10.0f,	"%-02.3f",	1 ) ;

						if( i )
						{
							SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Freq",		&layer.Freq,		0,	4,		"%-02.2f",	1 ) ;
							SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Speed",		&layer.Speed,		0,	20,		"%-02.2f",	1 ) ;

							if( i == 2 )
							{
								SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Motion randomness",	&t->LeafMotionRandomness,		0,	1,		"%-02.2f",	1 ) ;
							}
						}
						else
						{
							SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Dissipation",		&t->Dissipation,				0.0f,		2.0f,	"%-02.2f",	1 ) ;
							SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Stiffness",			&t->Stiffness,					0.5f,		128.0f,	"%-02.2f",	1 ) ;
							SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Mass",				&t->Mass,						0.125f,		1.0f,	"%-02.2f",	1 ) ;
							SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Bend Pow",			&t->BendPow,					1.0f/8.0f,	8.0f,	"%-02.2f",	1 ) ;

#if 0
							SliderY += imgui_Static( SliderX + 10, SliderY, "Border smooth" ) ;
							SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Value",				&currentParams.BorderDissipation,			0.0f,		0.5f,	"%-02.2f",	1 ) ;
							SliderY += imgui_Static( SliderX + 10, SliderY, "Border smooth strength" ) ;
							SliderY += imgui_Value_Slider( SliderX + 10, SliderY, "Value",				&currentParams.BorderDissipationStrength,	0.125f,		64.0f,	"%-02.2f",	1 ) ;
#endif
						}
					}
				}
			}

			SliderY += imgui_Static( SliderX, SliderY, " " ) ;
		}
		break;
	}

	g_pDesktopManager->End();
#endif
}

//////////////////////////////////////////////////////////////////////////

float GetMinTerrainHeight(const r3dBoundBox &bb)
{
	if (Terrain)
	{
		float min, max;
		Terrain->GetHeightRange(&min, &max, r3dPoint2D(bb.Org.x, bb.Org.z), r3dPoint2D(bb.Org.x + bb.Size.x, bb.Org.z + bb.Size.z));
		return min;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

r3dTL::TArray<int> gUtilityObjIdArr;

bool CreateCollectionElementInBrushRadius()
{
	int activeType = GetRandomActiveCollectionType();
	if (activeType == -1)
		return false;

	CollectionType *ct = gCollectionsManager.GetType(activeType);
	if (!ct)
		return false;

	//	Get objects in brush radius
	r3dPoint2D pos(UI_TargetPos.x, UI_TargetPos.z);
	gCollectionsManager.GetElementsInRadius(pos, std::max(ct->groupInfo.Spacing, BrushRadius), gUtilityObjIdArr);

	r3dPoint3D newPt(0, 0, 0);
	bool found = false;
	for (int i = 0; i < 5; ++i)
	{
		//	Pick random spot within brush radius
		newPt = GetRandomPointInRadius(BrushRadius, UI_TargetPos);
		bool haveCloseProximity = false;
		//	Check it for close proximity with other collection elements of same type
		for (uint32_t i = 0; i < gUtilityObjIdArr.Count(); ++i)
		{
			CollectionElement *ce = gCollectionsManager.GetElement(gUtilityObjIdArr[i]);
			if (ce && ce->typeIndex == activeType)
			{
				r3dBoundBox bb = ce->GetWorldAABB();
				r3dPoint3D center = bb.Center();
				float dist = (center - newPt).LengthSq();
				if (dist < ct->groupInfo.Spacing * ct->groupInfo.Spacing)
				{
					haveCloseProximity = true;
					break;
				}
			}
		}
		if (!haveCloseProximity)
		{
			found = true;
			break;
		}
	}

	if (!found)
		return false;

	int idx = gCollectionsManager.CreateNewElement();
	CollectionElement *el = gCollectionsManager.GetElement(idx);
	el->pos = newPt;
	el->typeIndex = activeType;
	float scaleRange = 0.25f * ct->groupInfo.Scale;
	el->scale = u_GetRandom(1.0f - scaleRange, 1.0f + scaleRange);
	el->angle = u_GetRandom(0.0f, ct->groupInfo.Rotate);
	el->GenerateRandomColor();
	if (ct->physicsEnable)
	{
		el->InitPhysicsData();
	}
	el->pos.y = GetMinTerrainHeight(el->GetWorldAABB());

	bool rv = gCollectionsManager.UpdateElementQuadTreePlacement(idx);
	if (!rv)
	{
		gCollectionsManager.DeleteElement(idx);
	}
	return rv;
}

//////////////////////////////////////////////////////////////////////////

void EraseCollectionElementsInBrushRadius(bool all)
{
	int activeType = GetRandomActiveCollectionType();

	//	Get objects in brush radius
	r3dPoint2D pos(UI_TargetPos.x, UI_TargetPos.z);
	gCollectionsManager.GetElementsInRadius(pos, BrushRadius - BrushRadius * 0.1f, gUtilityObjIdArr);

	for (uint32_t i = 0; i < gUtilityObjIdArr.Count(); ++i)
	{
		int idx = gUtilityObjIdArr[i];
		CollectionElement *ce = gCollectionsManager.GetElement(idx);
		if (ce)
		{
			if (all || ce->typeIndex == activeType)
			{
				gCollectionsManager.DeleteElement(idx);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

static int iEditMode = 0;

void Handle_Brush_Draw()
{
	int moved = false;
	static int mDown = false;
	if(!imgui_lbp){
		mDown = false;
	}else if(!mDown){ // wasnt down before, so first click
		mDown = true;
		if(iEditMode){
			moved = true;
			gLastMousePos = UI_TargetPos;
		}
	}
	if (iEditMode && imgui_lbp)
	{

		if(fabs(UI_TargetPos.x - gLastMousePos.x) > 1.0 || fabs(UI_TargetPos.z - gLastMousePos.z) > 1.0)
		{
			moved = true;
			gLastMousePos = UI_TargetPos;
		}

		int done = false;
		int inc = 0;
		while (!done)
		{
			done = true;

			// this means that I am erasing!!!
			if(CollectionPaintMode != 1 && CollectionPaintMode != 3)
			{
				EraseCollectionElementsInBrushRadius(CollectionPaintMode == 2);
			}
			else
			{
				while (CreateCollectionElementInBrushRadius())
				{
					if(CollectionPaintMode != 3)
					{
						break;
					}
					else
					{
						done = false;
					}
				}
			}
		}
	}
}

void Do_Collection_Editor_Init( float levelProgressStart, float levelProgressEnd )
{
#ifndef FINAL_BUILD
	r3dOutToLog( "Do_Collection_Editor_Init...\n" );
	float timeStart = r3dGetTime();
#endif
	gCollectionsManager.Init( levelProgressStart, levelProgressEnd );
#ifndef FINAL_BUILD
	r3dOutToLog( "Do_Collection_Editor_Init ok. (%.2f seconds)\n", r3dGetTime() - timeStart );
#endif
}

void ShutdownCollections()
{
	gCollectionsManager.Destroy();
}

void Editor_Level :: ProcessCollections()
{
	SliderX = r3dRenderer->ScreenW-365;
	SliderY = 45;
	Draw_Collection_Toolbar();
	if (Keyboard->IsPressed(kbsLeftControl)){
		iEditMode = 1;
	}else{
		iEditMode = 0;
	}
	if (Keyboard->IsPressed(kbsLeftAlt)) {
		gCam.SetPosition(UI_TargetPos);
	}
	if(iEditMode) {
		DrawCollectionBrush(); 
	}

	if( Terrain )
	{
		Handle_Brush_Draw();
	}
}

void DEBUG_Draw_Instance_Wind()
{
#ifndef	FINAL_BUILD
	if( r_show_wind->GetInt() )
	{
		for (uint32_t i = 0; i < gCollectionsManager.GetElementsCount(); ++i)
		{
			CollectionElement *ce = gCollectionsManager.GetElement(i);
			if (!ce)
				continue;

			CollectionType *ct = gCollectionsManager.GetType(ce->typeIndex);
			if (!ct || !ct->hasAnimation)
				continue;

			r3dPoint3D pos = ce->pos;

			float force = g_pWind->GetWindAt( pos ) / 255.f ;

			r3dMesh* m = ct->meshLOD[0];
			if (!m)
				continue;

			pos.y += m->localBBox.Org.y + m->localBBox.Size.y * 0.75f ;

			r3dPoint3D coord ;

			if( r3dProjectToScreen(  pos, &coord ) )
			{
				r3dDrawBox2D( coord.x - 100, coord.y + 0, 200 * 1,		11,	r3dColor24::white ) ;
				r3dDrawBox2D( coord.x -  99, coord.y + 1, 198 * 1,		9,	r3dColor24::black ) ;
				r3dDrawBox2D( coord.x -  99, coord.y + 1, 198 * force,	9,	r3dColor24::green ) ;
			}
		}
	}
#endif
}

r3dMaterial* Get_Material_By_Ray(const r3dPoint3D& vStart, const r3dPoint3D& vRay, float& dist)
{
	R3DPROFILE_FUNCTION( "Get_Material_By_Ray" ) ;

	const float renderdist = 15000.0f;

	r3dPoint3D dir = vRay * dist;
	gCollectionsManager.GetElementsAlongRay(vStart, dir, gUtilityObjIdArr);

	D3DXMATRIX rotation, scaling;
	r3dMaterial* resMaterial = 0;

	for (uint32_t i = 0; i < gUtilityObjIdArr.Count(); ++i)
	{
		CollectionElement *ce = gCollectionsManager.GetElement(gUtilityObjIdArr[i]);
		if (!ce)
			continue;

		CollectionType *ct = gCollectionsManager.GetType(ce->typeIndex);
		if (!ct)
			continue;

		r3dMesh *pMesh = ct->meshLOD[0];
		if (!pMesh)
			continue;

		float dst = 999999;
		float len = 25000.0f;

		D3DXMatrixRotationY(&rotation, R3D_DEG2RAD(ce->angle));
		D3DXMatrixScaling(&scaling, ce->scale, ce->scale, ce->scale);
		D3DXMatrixMultiply(&rotation, &scaling, &rotation);

		int OutMinFace = -1;
		r3dMaterial* material = 0;
		if (!pMesh->ContainsRay(vStart, vRay, len, &dst, &material, ce->pos, rotation, &OutMinFace)) 
			continue;

		if(dst < dist)
		{
			dist = dst;
			resMaterial = material;;
		}
	}
	return resMaterial;
}

//////////////////////////////////////////////////////////////////////////

void Fix_Height()
{
	for (uint32_t i = 0; i < gCollectionsManager.GetElementsCount(); ++i)
	{
		CollectionElement *ce = gCollectionsManager.GetElement(i);
		if (!ce)
			continue;

		r3dBoundBox bb = ce->GetWorldAABB();
		ce->pos.y = GetMinTerrainHeight(bb);
	}
}

//////////////////////////////////////////////////////////////////////////

void DrawCollectionBrush()
{
#ifndef FINAL_BUILD
	// draw circle and line height of human

	if( Terrain )
	{
		r3dDrawCircle3DT(UI_TerraTargetPos, BrushRadius , gCam, 0.5f, r3dColor(100,255,100));
		r3dDrawLine3D(UI_TerraTargetPos, UI_TerraTargetPos+r3dPoint3D(0,2,0), gCam, 0.2f, r3dColor(255,155,0));
	}
	else
	{
		imgui_Static( r3dRenderer->ScreenW / 2 - 180, r3dRenderer->ScreenH / 2, "There are no collections without terrain", 360, false, 22, true ) ;
	}


	// dont really need now that the grid does not do anything!!
#if 0 
	Draw_Grid_Section();
#endif

#endif
}

//////////////////////////////////////////////////////////////////////////

int GetRandomActiveCollectionType()
{
	uint32_t totalTypes = gCollectionsManager.GetTypesCount();
	int totalActive = 0;
	for (uint32_t i = 0; i < totalTypes; ++i)
	{
		CollectionType *t = gCollectionsManager.GetType(i);
		if (t && t->groupInfo.Active)
			++totalActive;
	}

	if (totalActive == 0)
		return INVALID_COLLECTION_INDEX;

	int randomActive = random(totalActive);
	int curActive = 0;
	for (uint32_t i = 0; i < totalTypes; ++i)
	{
		CollectionType *t = gCollectionsManager.GetType(i);
		if (t)
		{
			if (t->groupInfo.Active)
			{
				if (curActive == randomActive)
				{
					return i;
				}
				else
				{
					++curActive;
				}
			}
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////

r3dPoint3D GetRandomPointInRadius(float r, const r3dPoint3D &center)
{
	r3dPoint2D tmp;
	tmp.x = u_GetRandom(-1.0f, 1.0f);
	tmp.y = u_GetRandom(-1.0f, 1.0f);
	tmp.Normalize();
	tmp *= u_GetRandom(0, r);
	r3dPoint3D rv(tmp.x + center.x, center.y, tmp.y + center.z);
	rv.y = terra_GetH(rv);

	return rv;
}

//////////////////////////////////////////////////////////////////////////

typedef std::vector<std::string> stringlist_t;

extern int				ObjCatInit;
extern int				NumCategories;

typedef std::vector<CategoryStruct>  catlist;
extern catlist  ObjectCategories;
extern stringlist_t 	CatNames;
extern float			CatOffset;

void InitObjCategories();

//////////////////////////////////////////////////////////////////////////

void DrawPossibleElementsChoose(bool flag)
{
#ifndef FINAL_BUILD
	if(!flag)
		return;

	const float DEFAULT_CONTROLS_WIDTH = 360.0f;
	const float DEFAULT_CONTROLS_HEIGHT = 22.0f;


	float SliderX = r3dRenderer->ScreenW-375-375;
	float SliderY = 50;

	//
	static char CategoryName[64] = "";
	static char ClassName[64] = "";
	static char FileName[64] = "";

	char Str[256];

	InitObjCategories();

	SliderY += imgui_Static(SliderX, SliderY, CategoryName, 200);
	static int idx = -1;
	if (imgui_DrawList(SliderX, SliderY, 360, 200, CatNames, &CatOffset, &idx))
	{
		sprintf(CategoryName, "%s", CatNames.at(idx).c_str());

		sprintf(ClassName,"");
		sprintf(FileName, "");
		if(ObjectCategories.at(idx).ObjectsClasses.size() > 0)
		{
			sprintf(ClassName, "%s", ObjectCategories.at(idx).ObjectsClasses.at(0).c_str());
			sprintf(FileName, "%s", ObjectCategories.at(idx).ObjectsNames.at(0).c_str());
		}
	}

	SliderY += 210;

	SliderY += imgui_Static(SliderX, SliderY, FileName);

	static int idx1 = 0;
	if (idx != -1)
	{
		if (imgui_DrawList(SliderX, SliderY, 360, 200, ObjectCategories.at(idx).ObjectsNames , &ObjectCategories.at(idx).Offset, &idx1))
		{
			sprintf (ClassName,"%s", ObjectCategories.at(idx).ObjectsClasses.at(idx1).c_str());
			sprintf (FileName,"%s", ObjectCategories.at(idx).ObjectsNames.at(idx1).c_str());
		}
		SliderY += 205;

		if(ClassName[0] == 0 || FileName[0] == 0)
			return;

		sprintf (Str,"Data\\ObjectsDepot\\%s\\%s", CategoryName, FileName);
		std::string fileName = Str;

		sprintf (Str,"%s\\%s", CategoryName, FileName);
		std::string uiName = Str;

		if(imgui_Button(SliderX, SliderY, DEFAULT_CONTROLS_WIDTH, DEFAULT_CONTROLS_HEIGHT, "Add Decorator"))
		{
			int idx = gCollectionsManager.CreateNewType();
			CollectionType *t = gCollectionsManager.GetType(idx);
			char buf[MAX_PATH];
			sprintf(buf, "data\\objectsdepot\\%s", Str);
			t->InitMeshes(buf);
			t->lod1Dist = 30.0f;
			t->lod2Dist = 100.0f;
			t->renderDist = 2000.0f;
		}
		SliderY += DEFAULT_CONTROLS_HEIGHT;
	}
#endif
}
