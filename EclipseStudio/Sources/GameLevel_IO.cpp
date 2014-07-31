#include "r3dPCH.h"
#include "r3d.h"

#include "GameObjects/ObjManag.h"
#include "ObjectsCode/WORLD/obj_Group.h"

#include "GameLevel.h"

#ifdef WO_SERVER

// temporary server object
class obj_ServerDummyObject : public GameObject
{
public:
	DECLARE_CLASS(obj_ServerDummyObject, GameObject)
	// keep alive, do not destroy
	// virtual BOOL Update() { return FALSE; }
};
IMPLEMENT_CLASS(obj_ServerDummyObject, "obj_ServerDummyObject", "Object");
AUTOREGISTER_CLASS(obj_ServerDummyObject);

#endif

GameObject * LoadLevelObject ( pugi::xml_node & curNode )
{                                                                    
	pugi::xml_node posNode = curNode.child("position");
	const char* class_name = curNode.attribute("className").value();
	const char* load_name = curNode.attribute("fileName").value();
	r3dPoint3D pos(0, 0, 0);
	pos.x = posNode.attribute("x").as_float();
	pos.y = posNode.attribute("y").as_float();
	pos.z = posNode.attribute("z").as_float();

	GameObject* obj = NULL;

#ifdef WO_SERVER
	int class_id = AObjectTable_GetClassID(class_name, "Object");
	if(class_id == -1)
	{
		r3dOutToLog("skipped not defined server object %s\n", class_name);
		class_name = "obj_ServerDummyObject";
	}
#endif

#ifdef FINAL_BUILD
	//r3dOutToLog("HomeDir %s"
	if (strcmp( r3dGameLevel::GetHomeDir() , "Levels\\WZ_FrontEndLighting") && g_FastLoad->GetBool())
	{
	if(!strcmp( class_name, "obj_StoreNPC" ) || !strcmp( class_name, "obj_VaultNPC" ) || !strcmp( class_name, "obj_Terrain" ) || !strcmp( class_name, "obj_PermanentNote" ) || !strcmp( class_name, "obj_WaterPlane" ) || !strcmp( class_name, "obj_Lake" ) || !strcmp( class_name, "obj_Road" ) || !strcmp( class_name, "obj_ParticleSystem" ) || !strcmp( class_name, "obj_LightMesh" ) || !strcmp( class_name, "obj_LightHelper" ) || !strcmp( class_name, "obj_AmbientSound" )) /*|| (GameWorld().objplrLoad - pos).Length() < 1000 /*Load only object near player around 1000m) // we need to force load terrain , water*/
	{
		//r3dOutToLog("Terrain Found. , Loading..\n");
		obj = srv_CreateGameObject(class_name, load_name, pos);
		if(!obj)
		{
			r3dOutToLog("!!!Failed to create object! class: %s, name: %s\n", class_name, load_name);
			return NULL;
		}
		r3d_assert ( obj );
		obj->ReadSerializedData(curNode);
		return obj;
	}
	}
	else
	{
      	obj = srv_CreateGameObject(class_name, load_name, pos);
	if(!obj)
	{
		r3dOutToLog("!!!Failed to create object! class: %s, name: %s\n", class_name, load_name);
		return NULL;
	}
	r3d_assert ( obj );
	obj->ReadSerializedData(curNode);

	return obj;
	}
	// fill building data.
	/*if ( !strcmp( class_name, "obj_Building" ))
	{
		//ObjectManager& GW = GameWorld();
		ObjectManager::BuildingListData_s& build = GameWorld().BuildingListData[GameWorld().numbuilding++];
		build.pos = pos;
		//GW.BuildingListData[num].Angle =;
		sprintf(build.fname,load_name);
		//GW.BuildingListData[num].Node = curNode;
		build.isVisible = false;
		build.obj = NULL;
		return NULL;
	}*/
#endif
#ifndef FINAL_BUILD
	obj = srv_CreateGameObject(class_name, load_name, pos);
	if(!obj)
	{
		r3dOutToLog("!!!Failed to create object! class: %s, name: %s\n", class_name, load_name);
		return NULL;
	}
	r3d_assert ( obj );
	obj->ReadSerializedData(curNode);
	return obj;
#endif
#ifdef WO_SERVER
	obj = srv_CreateGameObject(class_name, load_name, pos);
	if(!obj)
	{
		r3dOutToLog("!!!Failed to create object! class: %s, name: %s\n", class_name, load_name);
		return NULL;
	}
	r3d_assert ( obj );
	obj->ReadSerializedData(curNode);
	return obj;
#endif
	return NULL;
}

void LoadLevelObjectsGroups ( pugi::xml_node & curNode, r3dTL::TArray < GameObject * > & dObjects )
{
	dObjects.Clear ();
	pugi::xml_node xmlObject = curNode.child("object");
	while(!xmlObject.empty())
	{
		GameObject* obj = LoadLevelObject ( xmlObject );

		dObjects.PushBack(obj);

		xmlObject = xmlObject.next_sibling();
	}
}


void LoadLevelObjects ( pugi::xml_node & curNode, float range )
{
	int count = 0;

	pugi::xml_node xmlObject = curNode.child("object");

	int hasTerrain = 0 ;

	while(!xmlObject.empty())
	{
		count ++;

		if( !hasTerrain )
		{
			const char *className = xmlObject.attribute( "className" ).value() ;
			if( !stricmp( className, "obj_Terrain" ) )
			{
				hasTerrain = 1 ;
			}
		}

		xmlObject = xmlObject.next_sibling();
	}

	if( hasTerrain )
	{
		range -= 0.1f ;
	}

	float delta = range / count;


	xmlObject = curNode.child("object");
	while(!xmlObject.empty())
	{		
		GameObject* obj = LoadLevelObject ( xmlObject );

		void AdvanceLoadingProgress( float );
		AdvanceLoadingProgress( delta );

		xmlObject = xmlObject.next_sibling();
	}
}


int LoadLevel_Objects( float BarRange )
{
	char fname[MAX_PATH];
	sprintf(fname, "%s\\LevelData.xml", r3dGameLevel::GetHomeDir());
	r3dFile* f = r3d_open(fname, "rb");
	if ( ! f )
		return 0;

	char* fileBuffer = new char[f->size + 1];
	fread(fileBuffer, f->size, 1, f);
	fileBuffer[f->size] = 0;
	pugi::xml_document xmlLevelFile;
	pugi::xml_parse_result parseResult = xmlLevelFile.load_buffer_inplace(fileBuffer, f->size);
	fclose(f);
	if(!parseResult)
		r3dError("Failed to parse XML, error: %s", parseResult.description());
	pugi::xml_node xmlLevel = xmlLevelFile.child("level");

	g_leveldata_xml_ver->SetInt( 0 );
	if( !xmlLevel.attribute("version").empty() )
	{
		g_leveldata_xml_ver->SetInt( xmlLevel.attribute("version").as_int() );
	}


	if( g_level_settings_ver->GetInt() < 2 || g_level_settings_ver->GetInt() >=3 )
	{
		GameWorld().m_MinimapOrigin.x = xmlLevel.attribute("minimapOrigin.x").as_float();
		GameWorld().m_MinimapOrigin.z = xmlLevel.attribute("minimapOrigin.z").as_float();
		GameWorld().m_MinimapSize.x = xmlLevel.attribute("minimapSize.x").as_float();  
		GameWorld().m_MinimapSize.z = xmlLevel.attribute("minimapSize.z").as_float();

		if(g_level_settings_ver->GetInt() < 2 )
		{
			r_shadow_extrusion_limit->SetFloat(xmlLevel.attribute("shadowLimitHeight").as_float());
			if(!xmlLevel.attribute("near_plane").empty())
			{
				r_near_plane->SetFloat(xmlLevel.attribute("near_plane").as_float()); 
				r_far_plane->SetFloat(xmlLevel.attribute("far_plane").as_float());
			}
		}
	}

	if(GameWorld().m_MinimapSize.x == 0 || GameWorld().m_MinimapSize.z == 0)
	{
		GameWorld().m_MinimapSize.x = 100;
		GameWorld().m_MinimapSize.z = 100;
	}
	GameWorld().numbuilding = 0;
#ifdef WO_SERVER
	GameWorld().spawncar = xmlLevel.attribute("spawncar").as_int();
#endif
	int i = 0;
	char name[512] = {0};
	sprintf(name,"%s\\LevelData\\ObjectData_%d.bin", r3dGameLevel::GetHomeDir(),i++);
	int i2 = 0;
	char name2[512] = {0};
	sprintf(name2,"%s\\LevelData\\SoundData_%d.bin", r3dGameLevel::GetHomeDir(),i2++);
	int i3 = 0;
	char name3[512] = {0};
	sprintf(name3,"%s\\LevelData\\LightData_%d.bin", r3dGameLevel::GetHomeDir(),i3++);

	if (r3d_access(name,0))
		LoadLevelObjects ( xmlLevel, BarRange );

	while (!r3d_access(name,0))
	{
		r3dOutToLog("%s\n",name);
		r3dFile * hFile;
	    hFile = r3d_open( name, "rb");
		r3dLevelData data;
		ZeroMemory(&data,sizeof(r3dLevelData));
		fread(&data, sizeof(r3dLevelData), 1, hFile );
		//r3dOutToLog("%s %s\n",data.filename,data.text);
		//LoadLevelObjects ( dataxmlObject, BarRange );
	    pugi::xml_document xmlLevelFile;
	    pugi::xml_parse_result parseResult = xmlLevelFile.load_buffer_inplace(data.text, strlen(data.text));
	    if(!parseResult)
		{
		r3dError("Failed to parse XML, error: %s", parseResult.description(),data.text);
		}

		//LoadLevelObjects ( xmlLevelFile, BarRange );

		pugi::xml_node xmlObjectData = xmlLevelFile.child("ObjectData");
		pugi::xml_node xmlData = xmlObjectData.child("object");
		LoadLevelObject(xmlData);
		//GameObject* obj = srv_CreateGameObject(data.class_name, data.filename, data.pos);
	    //obj->ReadSerializedData(xmlLevelFile);

		fclose(hFile);
		sprintf(name,"%s\\LevelData\\ObjectData_%d.bin", r3dGameLevel::GetHomeDir(),i++);
	}

	while (!r3d_access(name2,0))
	{
		r3dOutToLog("%s\n",name2);
		r3dFile * hFile;
	    hFile = r3d_open( name2, "rb");
		r3dLevelData data;
		ZeroMemory(&data,sizeof(r3dLevelData));
		fread(&data, sizeof(r3dLevelData), 1, hFile );
		//r3dOutToLog("%s %s\n",data.filename,data.text);
		//LoadLevelObjects ( dataxmlObject, BarRange );
	    pugi::xml_document xmlLevelFile;
	    pugi::xml_parse_result parseResult = xmlLevelFile.load_buffer_inplace(data.text, strlen(data.text));
	    if(!parseResult)
		{
		r3dError("Failed to parse XML, error: %s", parseResult.description(),data.text);
		}

		//LoadLevelObjects ( xmlLevelFile, BarRange );

		pugi::xml_node xmlObjectData = xmlLevelFile.child("ObjectData");
		pugi::xml_node xmlData = xmlObjectData.child("object");
		LoadLevelObject(xmlData);
		//GameObject* obj = srv_CreateGameObject(data.class_name, data.filename, data.pos);
	    //obj->ReadSerializedData(xmlLevelFile);

		fclose(hFile);
		sprintf(name2,"%s\\LevelData\\SoundData_%d.bin", r3dGameLevel::GetHomeDir(),i2++);
	}

	while (!r3d_access(name3,0))
	{
		r3dOutToLog("%s\n",name3);
		r3dFile * hFile;
	    hFile = r3d_open( name3, "rb");
		r3dLevelData data;
		ZeroMemory(&data,sizeof(r3dLevelData));
		fread(&data, sizeof(r3dLevelData), 1, hFile );
		//r3dOutToLog("%s %s\n",data.filename,data.text);
		//LoadLevelObjects ( dataxmlObject, BarRange );
	    pugi::xml_document xmlLevelFile;
	    pugi::xml_parse_result parseResult = xmlLevelFile.load_buffer_inplace(data.text, strlen(data.text));
	    if(!parseResult)
		{
		r3dError("Failed to parse XML, error: %s", parseResult.description(),data.text);
		}

		//LoadLevelObjects ( xmlLevelFile, BarRange );

		pugi::xml_node xmlObjectData = xmlLevelFile.child("ObjectData");
		pugi::xml_node xmlData = xmlObjectData.child("object");
		LoadLevelObject(xmlData);
		//GameObject* obj = srv_CreateGameObject(data.class_name, data.filename, data.pos);
	    //obj->ReadSerializedData(xmlLevelFile);

		fclose(hFile);
		sprintf(name3,"%s\\LevelData\\LightData_%d.bin", r3dGameLevel::GetHomeDir(),i3++);
	}

	// delete only after we are done parsing xml!
	delete [] fileBuffer;

	return 1;
}

#ifndef FINAL_BUILD
#ifndef WO_SERVER
int LoadLevel_Groups ()
{
	char fname[MAX_PATH];
	sprintf(fname, "Data\\ObjectsDepot\\LevelGroups.xml", r3dGameLevel::GetHomeDir());
	obj_Group::LoadFromFile(fname);
	return 1;
}
#endif	
#endif

int LoadLevel_MatLibs()
{
	if(r3dMaterialLibrary::IsDynamic) {
		// skip loading level materials if we're in editing mode
		return 1;
	}

	char fname[MAX_PATH];
	sprintf(fname, "%s\\room.mat", r3dGameLevel::GetHomeDir());

	r3dFile* f = r3d_open(fname, "rt");
	if(!f) {
		r3dArtBug("LoadLevel: can't find %s - switching to dynamic matlib\n", fname);
		r3dMaterialLibrary::IsDynamic = true;
		return 1;
	}

	char Str2[256], Str3[256];
	sprintf(Str2,"%s\\room.mat", r3dGameLevel::GetHomeDir());
	sprintf(Str3,"%s\\Textures\\", r3dGameLevel::GetHomeDir());

	r3dMaterialLibrary::LoadLibrary(Str2, Str3);
	return 1;
}