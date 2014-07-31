#include "r3dPCH.h"
#include "r3d.h"

#include "FrontEndShared.h"
#include "../ObjectsCode/weapons/Weapon.h"
#include "APIScaleformGfx.h"

#include "../ObjectsCode/weapons/WeaponArmory.h"
#include "../ObjectsCode/weapons/Weapon.h"
#include "../ObjectsCode/weapons/Ammo.h"
#include "../ObjectsCode/weapons/Gear.h"


// disconnect from master server after this seconds of unactivity
float		_p2p_idleTime  = 10.0f; 

GraphicSettings::GraphicSettings()
{
	mesh_quality = r_mesh_quality->GetInt();
	texture_quality = r_texture_quality->GetInt();
	terrain_quality = r_terrain_quality->GetInt();
	water_quality = r_water_quality->GetInt();
	shadows_quality = r_shadows_quality->GetInt();
	lighting_quality = r_lighting_quality->GetInt();
	particles_quality = r_particles_quality->GetInt();
	decoration_quality = r_decoration_quality->GetInt();
	antialiasing_quality = r_antialiasing_quality->GetInt();
	anisotropy_quality = r_anisotropy_quality->GetInt();
	postprocess_quality = r_postprocess_quality->GetInt();
	ssao_quality = r_ssao_quality->GetInt();
}

DWORD GraphSettingsToVars( const GraphicSettings& settings )
{
	DWORD flags = 0;

	if( r_mesh_quality->GetInt() != settings.mesh_quality )
	{
		flags |= FrontEndShared::SC_MESH_QUALITY;
		r_mesh_quality->SetInt( settings.mesh_quality );
	}

	if( r_texture_quality->GetInt() != settings.texture_quality )
	{
		flags |= FrontEndShared::SC_TEXTURE_QUALITY;
		r_texture_quality->SetInt( settings.texture_quality );
	}

	if( r_terrain_quality->GetInt() != settings.terrain_quality )
	{
		flags |= FrontEndShared::SC_TERRAIN_QUALITY;
		r_terrain_quality->SetInt( settings.terrain_quality );
	}

	if( r_water_quality->GetInt() != settings.water_quality )
	{
		flags |= FrontEndShared::SC_WATER_QUALITY;
		r_water_quality->SetInt( settings.water_quality );
	}

	if( r_shadows_quality->GetInt() != settings.shadows_quality )
	{
		flags |= FrontEndShared::SC_SHADOWS_QUALITY;
		r_shadows_quality->SetInt( settings.shadows_quality );
	}

	if( r_lighting_quality->GetInt() != settings.lighting_quality )
	{
		flags |= FrontEndShared::SC_LIGHTING_QUALITY;
		r_lighting_quality->SetInt( settings.lighting_quality );
	}

	if( r_particles_quality->GetInt() != settings.particles_quality )
	{
		flags |= FrontEndShared::SC_PARTICLES_QUALITY;
		r_particles_quality->SetInt( settings.particles_quality );
	}

	if( r_decoration_quality->GetInt() != settings.decoration_quality )
	{
		flags |= FrontEndShared::SC_DECORATIONS_QUALITY;
		r_decoration_quality->SetInt( settings.decoration_quality );
	}

	if( r_antialiasing_quality->GetInt() != settings.antialiasing_quality )
	{
		flags |= FrontEndShared::SC_ANTIALIASING_QUALITY;
		r_antialiasing_quality->SetInt( settings.antialiasing_quality );
	}

	if( r_anisotropy_quality->GetInt() != settings.anisotropy_quality )
	{
		flags |= FrontEndShared::SC_ANISOTROPY_QUALITY;
		r_anisotropy_quality->SetInt( settings.anisotropy_quality );
	}

	if( r_postprocess_quality->GetInt() != settings.postprocess_quality )
	{
		flags |= FrontEndShared::SC_POSTPROCESS_QUALITY;
		r_postprocess_quality->SetInt( settings.postprocess_quality );
	}

	if( r_ssao_quality->GetInt() != settings.ssao_quality )
	{
		flags |= FrontEndShared::SC_SSAO_QUALITY;
		r_ssao_quality->SetInt( settings.ssao_quality );
	}

	r_decals_proximity_multiplier->SetFloat(1 - float(settings.decoration_quality) / (S_ULTRA + 1));

	return flags;
}

void FillDefaultSettings( GraphicSettings& settings, r3dDevStrength strength )
{
	switch( strength )
	{
	case S_WEAK:
		settings.mesh_quality			= 2;
		settings.texture_quality		= 2;
		settings.terrain_quality		= 1;
		settings.water_quality			= 1;
		settings.shadows_quality		= 1;
		settings.lighting_quality		= 1;
		settings.particles_quality		= 1;
		settings.decoration_quality		= 1;
		//settings.antialiasing_quality	= 1;
		settings.anisotropy_quality		= 2;
		settings.postprocess_quality	= 1;
		settings.ssao_quality			= 1;
		break;

	case S_MEDIUM:
		settings.mesh_quality			= 2;
		settings.texture_quality		= 2;
		settings.terrain_quality		= 2;
		settings.water_quality			= 2;
		settings.shadows_quality		= 2;
		settings.lighting_quality		= 2;
		settings.particles_quality		= 2;
		settings.decoration_quality		= 2;
		//settings.antialiasing_quality	= 1;
		settings.anisotropy_quality		= 2;
		settings.postprocess_quality	= 1;
		settings.ssao_quality			= 2;
		break;

	case S_STRONG:
		settings.mesh_quality			= 3;
		settings.texture_quality		= 3;
		settings.terrain_quality		= 3;
		settings.water_quality			= 3;
		settings.shadows_quality		= 3;
		settings.lighting_quality		= 3;
		settings.particles_quality		= 3;
		settings.decoration_quality		= 3;
		//settings.antialiasing_quality	= 1;
		settings.anisotropy_quality		= 3;
		settings.postprocess_quality	= 2;
		settings.ssao_quality			= 3;
		break;

	case S_ULTRA:
		settings.mesh_quality			= 3;
		settings.texture_quality		= 3;
		settings.terrain_quality		= 3;
		settings.water_quality			= 3;
		settings.shadows_quality		= 4;
		settings.lighting_quality		= 3;
		settings.particles_quality		= 4;
		settings.decoration_quality		= 3;
		//settings.antialiasing_quality	= 1;
		settings.anisotropy_quality		= 4;
		settings.postprocess_quality	= 3;
		settings.ssao_quality			= 4;
		break;

	default:
		r3dError( "SetDefaultSettings: unknown strength..." );

	};
}

DWORD SetDefaultSettings( r3dDevStrength strength )
{
	GraphicSettings settings;
	FillDefaultSettings( settings, strength );

	r3d_assert( strength <=  S_ULTRA );

	r_overall_quality->SetInt( strength + 1 );
	r_apex_enabled->SetBool(r_overall_quality->GetInt() == 4);

	return GraphSettingsToVars( settings );
}

DWORD SetCustomSettings( const GraphicSettings& settings )
{
	r_apex_enabled->SetBool( false );

	r_overall_quality->SetInt( 5 );

	return GraphSettingsToVars( settings );
}

bool CreateFullIniPath( char* dest, bool old );

#define CUSTOM_INI_FILE "CustomSettings.ini"

void FillIniPath( char* target )
{
	bool useLocal = true;

	if( CreateConfigPath( target ) )
	{
		strcat( target, CUSTOM_INI_FILE );
		useLocal = false;
	}

	if( useLocal )
	{
		strcpy( target, CUSTOM_INI_FILE );
	}
}

void SaveCustomSettings( const GraphicSettings& settings )
{
	char fullPath[ MAX_PATH * 2 ];

	FillIniPath( fullPath );

	r3dOutToLog( "SaveCustomSettings: using file %s\n", fullPath );

	FILE* fout = fopen_for_write( fullPath, "wt");

	fprintf( fout, "%s %d\n", r_mesh_quality->GetName(), settings.mesh_quality );
	fprintf( fout, "%s %d\n", r_texture_quality->GetName(), settings.texture_quality );
	fprintf( fout, "%s %d\n", r_terrain_quality->GetName(), settings.terrain_quality );
	fprintf( fout, "%s %d\n", r_water_quality->GetName(), settings.water_quality );
	fprintf( fout, "%s %d\n", r_shadows_quality->GetName(), settings.shadows_quality );
	fprintf( fout, "%s %d\n", r_lighting_quality->GetName(), settings.lighting_quality );
	fprintf( fout, "%s %d\n", r_particles_quality->GetName(), settings.particles_quality );
	fprintf( fout, "%s %d\n", r_decoration_quality->GetName(), settings.decoration_quality );
	fprintf( fout, "%s %d\n", r_antialiasing_quality->GetName(), settings.antialiasing_quality );
	fprintf( fout, "%s %d\n", r_anisotropy_quality->GetName(), settings.anisotropy_quality );
	fprintf( fout, "%s %d\n", r_postprocess_quality->GetName(), settings.postprocess_quality );
	fprintf( fout, "%s %d\n", r_ssao_quality->GetName(), settings.ssao_quality );

	fclose( fout );
}

static void CheckOption( const char* line, const CmdVar* var, int* target )
{
	int val;
	char scanfline[ 512 ];

	sprintf( scanfline, "%s %%d", var->GetName() );
	
	if( sscanf( line, scanfline, &val ) == 1 ) 
		*target = val;
}

GraphicSettings GetCustomSettings()
{
	GraphicSettings settings;

	char fullPath[ MAX_PATH * 2 ];

	FillIniPath( fullPath );

	r3dOutToLog( "GetCustomSettings: using file %s\n", fullPath );

	if( FILE* fin = fopen( fullPath, "rt") )
	{
		for( ; !feof( fin ) ; )
		{
			char line[ 1024 ] = { 0 };

			fgets( line, sizeof line - 1, fin );

			if( strlen( line ) )
			{
				CheckOption( line, r_mesh_quality, &settings.mesh_quality );
				CheckOption( line, r_texture_quality, &settings.texture_quality );
				CheckOption( line, r_terrain_quality, &settings.terrain_quality );
				CheckOption( line, r_water_quality, &settings.water_quality );
				CheckOption( line, r_shadows_quality, &settings.shadows_quality );
				CheckOption( line, r_lighting_quality, &settings.lighting_quality );
				CheckOption( line, r_particles_quality, &settings.particles_quality );
				CheckOption( line, r_decoration_quality, &settings.decoration_quality );
				CheckOption( line, r_antialiasing_quality, &settings.antialiasing_quality );
				CheckOption( line, r_anisotropy_quality, &settings.anisotropy_quality );
				CheckOption( line, r_postprocess_quality, &settings.postprocess_quality );
				CheckOption( line, r_ssao_quality, &settings.ssao_quality );
			}
		}

		fclose( fin );
	}

	return settings;
}

void FillSettingsFromVars ( GraphicSettings& settings )
{
	settings.mesh_quality			= r_mesh_quality		->GetInt();
	settings.texture_quality		= r_texture_quality		->GetInt();
	settings.terrain_quality		= r_terrain_quality		->GetInt();
	settings.water_quality			= r_water_quality		->GetInt();
	settings.shadows_quality		= r_shadows_quality		->GetInt();
	settings.lighting_quality		= r_lighting_quality	->GetInt();
	settings.particles_quality		= r_particles_quality	->GetInt();
	settings.decoration_quality		= r_decoration_quality	->GetInt();
	settings.antialiasing_quality	= r_antialiasing_quality->GetInt();
	settings.anisotropy_quality		= r_anisotropy_quality	->GetInt();
	settings.postprocess_quality	= r_postprocess_quality	->GetInt();
	settings.ssao_quality			= r_ssao_quality		->GetInt();
}


void GetInterfaceSize(int& width, int& height, int& y_shift, const r3dScaleformMovie &m)
{
	int x, y;
	m.GetViewport(&x, &y, &width, &height);
	y_shift = (r_height->GetInt() - height) / 2;
}

float GetOptimalDist(const r3dPoint3D& boxSize, float halfFovInDegrees)
{
	float halfFOV = R3D_DEG2RAD(halfFovInDegrees);
	float halfH = boxSize.y * 0.5f;
	float treeR = R3D_MAX(boxSize.x, boxSize.z);

	float t = tanf( halfFOV );
	float distance = (halfH / t) + treeR;

	return distance;
}

// any changes please duplicate to getWeaponStatMinMaxForUI
void getWeaponParamForUI(const WeaponConfig* wc, int* damage, int* spread, int* firerate, int *recoil)
{
	// min-max values to map.
	int	d1  = 15;
	int	d2  = 65;
	int	fr1 = 100;
	int	fr2 = 1000;
	float	sp1 = 0.5f;
	float	sp2 = 5.0f;
	float	re1 = 1.0f;
	float   re2 = 15.0f;

	switch(wc->category)
	{
	case storecat_SNP:
		d1  = 30;  d2  = 250;  
		fr1 = 10;  fr2 = 200;
		re1 = 1.0f;  re2 = 13.0f;
		break;
	default:
		break;
	}

	if(damage)
	{
		*damage = (int)wc->m_AmmoDamage;
		*damage = ((*damage - d1) * 100 )/ (d2 - d1);
		*damage = R3D_CLAMP(*damage, 0, 100);
	}

	// firedelay convert into fire rate per minute
	if(firerate)
	{
		*firerate = int(60.0f / wc->m_fireDelay);
		*firerate = (*firerate - fr1) * 100 / (fr2 - fr1);
		*firerate = R3D_CLAMP(*firerate, 0, 100);
	}

	if(spread)
	{
		*spread = int((wc->m_spread - sp1) * 100.0f / (sp2 - sp1));
		*spread = R3D_CLAMP(*spread, 0, 100);
	}

	if(recoil)
	{
		*recoil = int((wc->m_recoil - re1) * 100.0f / (re2 - re1));
		*recoil = R3D_CLAMP(*recoil, 0, 100);
	}
}

// any changes please duplicate to getWeaponParamForUI
void getWeaponStatMinMaxForUI(const WeaponConfig* wc, int* mindamage, int* maxdamage, int* minfirerate, int* maxfirerate, 
							int* minclip, int* maxclip, int* minrange, int* maxrange)
{
	// min-max values to map.
	int	d1  = 15;
	int	d2  = 65;
	int	fr1 = 100;
	int	fr2 = 1000;
	int c1 = 0;
	int c2 = 200;
	int r1 = 0;
	int r2 = 200;

	switch(wc->category)
	{
	case storecat_SNP:
		d1  = 30;  d2  = 250;  
		fr1 = 10;  fr2 = 200;
		c1 = 1; c2 = 50;
		r1 = 100; r2 = 1000;
		break;
	default:
		break;
	}

	if(mindamage)
		*mindamage = d1;
	if(maxdamage)
		*maxdamage = d2;
	
	if(minfirerate)
		*minfirerate = fr1;
	if(maxfirerate)
		*maxfirerate = fr2;

	if(minclip)
		*minclip = c1;
	if(maxclip)
		*maxclip = c2;

	if(minrange)
		*minrange = r1;
	if(maxrange)
		*maxrange = r2;
}

void getAdditionalDescForItem(uint32_t itemID, int Var1, int Var2, char* res)
{
	const WeaponConfig* wc = g_pWeaponArmory->getWeaponConfig(itemID);
	const WeaponAttachmentConfig* wac = g_pWeaponArmory->getAttachmentConfig(itemID);
	if(wc && wc->category != storecat_UsableItem && wc->category != storecat_GRENADE && wc->category!=storecat_MELEE) // weapon
	{
		const WeaponAttachmentConfig* clip = g_pWeaponArmory->getAttachmentConfig(Var2<0?(wc->FPSDefaultID[WPN_ATTM_CLIP]):(Var2));
		if(clip)
		{
			int ammoLeft = Var1<0?clip->m_Clipsize:Var1;
			if(ammoLeft > 0)
			{
				if(ammoLeft == 1)
					sprintf(res, "%s: %d bullet left", clip->m_StoreName, ammoLeft);
				else
					sprintf(res, "%s: %d bullets left", clip->m_StoreName, ammoLeft);
			}
			else
				sprintf(res, "%s", "NO AMMO");
		}
	}
	else if(wac)
	{
		int ammoLeft = Var1<0?wac->m_Clipsize:Var1;
		if(ammoLeft > 0)
		{
			if(ammoLeft == 1)
				sprintf(res, "%d bullet left", ammoLeft);
			else
				sprintf(res, "%d bullets left", ammoLeft);
		}
		else
			sprintf(res, "%s", "EMPTY CLIP");
	}
}