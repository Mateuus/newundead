#include "r3dPCH.h"
#include "r3d.h"

#include "../GameEngine/TrueNature/Terrain.h"
#include "../sf/RenderBuffer.h"

#include "GameCommon.h"

#include "GrassLib.h"
#include "GrassGen.h"
#include "GrassMap.h"
#include "GrassEditorPlanes.h"

#include "r3dDeviceQueue.h"

extern r3dITerrain* Terrain;
extern GrassPlanesManager* g_GrassPlanesManager;
using namespace r3dTL;

GrassMap* g_pGrassMap = 0;

namespace
{
	const char* GrazCellDataFileName = "grasscells.bin";

	char GrazCellData_SIG101[] = "GRAZCELZ101";
	char GrazCellData_SIG101_2[] = "\3\3GRAZCELZ101";
	char GrazCellData_SIG102[] = "\3\3GRAZCELZ102";
	char GrazCellData_SIG103[] = "\3\3GRAZCELZ103";
	char GrazCellData_SIG104[] = "\3\3GRAZCELZ104";
	char GrazCellData_SIG105[] = "\3\3GRAZCELZ105";

	char GrazCellTexesBlob_SIG[] = "\3\3GRAZTEXZ100";
	

	int P0_PIXEL_SHADER_ID				= -1;

	const D3DFORMAT MASK_TEX_FMT		= D3DFMT_L8;
	const UINT		MASK_TEX_FMT_SIZE	= 1;
	const D3DFORMAT HEIGHT_TEX_FMT		= D3DFMT_L16;
	const UINT		HEIGHT_TEX_FMT_SIZE	= 2;

	clock_t PrevClock = clock();
	float Time = 0;

	typedef TArray< unsigned char > Bytes;

	void ResizeTexture(	uint32_t oldTotalXLen, uint32_t oldTotalZLen, float oldExcessX, float oldExcessZ, const Bytes& oldCompoundTex,
						uint32_t newTotalXLen, uint32_t newTotalZLen, float newExcessX, float newExcessZ, int OLD_TEX_DIM, int NEW_TEX_DIM, Bytes& newCompoundTex );

	void SaveTexture( FILE* fout, r3dTexture* tex, bool isMask );
	void FillTexture( r3dTexture* tex, const Bytes& data, bool isMask );
}

//------------------------------------------------------------------------

GrassCellEntry::GrassCellEntry()
: TypeIdx( -1 )
{

}

//------------------------------------------------------------------------

GrassCell::GrassCell()
: Position( 0.f, 0.f, 0.f )
, YMax( 0.f )
{

}

//------------------------------------------------------------------------

GrassMaskTextureEntry::GrassMaskTextureEntry()
: TypeIdx( -1 )
, MaskTexture( NULL )
{

}

//------------------------------------------------------------------------

GrassTextureCell::GrassTextureCell()
: HeightTexture( NULL )
{

}

//------------------------------------------------------------------------

GrassMap::Settings::Settings()
: XLength( 0.f )
, ZLength( 0.f )

, XOffset( 0.f )
, ZOffset( 0.f )

, YMin( 0 )
, YMax( 0 )
{

}

//------------------------------------------------------------------------

GrassMap::GrassMap()
: mInitialized( false )
, mLoadVersion( 0 )
, mDebugGrassType( -1 ) 
, mCellSize( 0 )
, mCellsPerTextureCell( 1 )
{

}

//------------------------------------------------------------------------

GrassMap::~GrassMap()
{

}

//------------------------------------------------------------------------

void
GrassMap::Init( float xoffset, float zoffset, float xsize, float zsize, int maxTexCellCount )
{
	r3d_assert( !mInitialized );

	mSettings.XOffset	= xoffset;
	mSettings.ZOffset	= zoffset;

	mSettings.XLength	= xsize;
	mSettings.ZLength	= zsize;

	mCellSize			= g_pGrassLib->GetSettings().CellScale;

	int cellsX, cellsZ;

	GetNominalCellCount( mSettings.XLength, mSettings.ZLength, &cellsX, &cellsZ );

	SetupCellArrays( cellsX, cellsZ, maxTexCellCount );

	for( int z = 0, e = mCells.Height(); z < e ; z ++ )
	{
		for( int x = 0, e = mCells.Width(); x < e ; x ++ )
		{
			GrassCell& gc = mCells[ z ][ x ];

			gc.Position.x = x * mCellSize + mSettings.XOffset;
			gc.Position.z = z * mCellSize + mSettings.ZOffset;
		}
	}

	mInitialized = true;
}

//------------------------------------------------------------------------

void
GrassMap::Close()
{
	mSettings.XLength = 0;
	mSettings.ZLength = 0;

	mCellSize = 0;

	for( uint32_t j = 0, e = mTextureCells.Height(); j < e; j ++ )
	{
		for( uint32_t i = 0, e = mTextureCells.Width(); i < e; i ++ )
		{
			// non 0 height texture means it's allocated
			GrassTextureCell& cell = mTextureCells[ j ][ i ];
			if( cell.HeightTexture )
			{
				r3dRenderer->DeleteTexture( cell.HeightTexture );
				cell.HeightTexture = NULL;

				for( uint32_t i = 0, e = cell.MaskTextureEntries.Count() ; i < e; i ++ )
				{
					GrassMaskTextureEntry& en = cell.MaskTextureEntries[ i ];
					if( en.MaskTexture )
					{
						r3dRenderer->DeleteTexture( en.MaskTexture );
					}
				}
			}
		}
	}

	mCells.Resize( 0, 0 );
	mTextureCells.Resize( 0, 0 );

	mInitialized = false;
}

//------------------------------------------------------------------------

bool
GrassMap::Save( const r3dString& levelHomeDir )
{
	SaveSettings( levelHomeDir );

	if( g_pGrassGen->IsDirty() )
	{
		g_pGrassGen->Save() ;
	}

	OptimizeMasks();

	r3dString grazPaz = levelHomeDir + AR_GRAZ_PAZ;

	// try creating just in case
	mkdir( grazPaz.c_str() );

	grazPaz += "\\";

	if( !SaveCellData( grazPaz + GrazCellDataFileName ) )
		return false;

	return true;
}

//------------------------------------------------------------------------

bool
GrassMap::Load( const r3dString& levelHomeDir )
{
	mSettings = Settings();

	LoadSettings( levelHomeDir );

	if( !LoadCellData( levelHomeDir + AR_GRAZ_PAZ "\\" + GrazCellDataFileName ) )
	{
		if( Terrain )
		{
			const r3dTerrainDesc& desc = Terrain->GetDesc() ;
			if( mInitialized )
			{
				Close();
			}

			Init( mSettings.XOffset, mSettings.ZOffset, desc.XSize, desc.ZSize, MAX_TEX_CELL_COUNT );
		}
		return false;
	}

	//	Check for incorrect grass data size
	if( Terrain )
	{
		const r3dTerrainDesc& desc = Terrain->GetDesc() ;

		if (	int( desc.XSize / mCellSize ) >  int( mSettings.XLength / mCellSize ) || 
				int( desc.ZSize / mCellSize ) >  int( mSettings.ZLength / mCellSize ) )
		{
			Close();
			Init( mSettings.XOffset, mSettings.ZOffset, desc.XSize, desc.ZSize, MAX_TEX_CELL_COUNT );
		}
	}

	mSettings.XLength = R3D_MAX( mSettings.XLength, ( mCells.Width() - 0.0625f ) * mCellSize );
	mSettings.ZLength = R3D_MAX( mSettings.ZLength, ( mCells.Height() - 0.0625f ) * mCellSize );

	if( mLoadVersion == 101 )
	{
		r3dError( "GrassMap::Load: grasscells.bin of version 1.01 is no longer supported." );
	}

	UpdateYExtents();

	return true;
}

//------------------------------------------------------------------------

bool
GrassMap::HasGrassCells() const
{
	for( uint32_t j = 0, e = mTextureCells.Height(); j < e; j ++ )
	{
		for( uint32_t i = 0, e = mTextureCells.Width(); i < e; i ++ )
		{
			const GrassTextureCell& cell = mTextureCells[ j ][ i ];
			if( cell.HeightTexture )
			{
				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------

void
GrassMap::OptimizeMasks()
{
	float uniformThreshold = g_pGrassLib->GetSettings().UniformMaskThreshold * 255.f;
	float blankTrheshold = g_pGrassLib->GetSettings().BlankMaskThreshold * 255.f;
	float blankMaxTrheshold = g_pGrassLib->GetSettings().MaxBlankMaskThreshold * 255.f;

	uint32_t optimizedUniform( 0 ), optimizedBlank( 0 ), optimizedHeightmap( 0 ), totalCount( 0 );

	for( uint32_t z = 0, e = mTextureCells.Height(); z < e; z ++ )
	{
		for( uint32_t x = 0, e = mTextureCells.Width(); x < e; x ++ )
		{
			GrassTextureCell& cell = mTextureCells[ z ][ x ];

			for( uint32_t i = 0, e = cell.MaskTextureEntries.Count(); i < e; )
			{
				GrassMaskTextureEntry& gce = cell.MaskTextureEntries[ i ];

				bool do_delete = false;

				if( gce.MaskTexture )
				{
					totalCount ++;

					UINT64 pixelSum( 0 );
					uint32_t maxPixel( 0 );

					IDirect3DTexture9* tex = gce.MaskTexture->AsTex2D();

					D3DLOCKED_RECT lrect;
					D3D_V( tex->LockRect( 0, &lrect, NULL, D3DLOCK_READONLY ) );

					unsigned char* p = (unsigned char*)lrect.pBits;

					for( uint32_t pz = 0, e = CELL_MASK_TEX_DIM; pz < e; pz ++ )
					{
						for( uint32_t px = 0, e = CELL_MASK_TEX_DIM; px < e; px ++ )
						{
							uint32_t val = *p ++;

							maxPixel = R3D_MAX( maxPixel, val );
							pixelSum += val;
						}

						p += lrect.Pitch - CELL_MASK_TEX_DIM;
					}

					tex->UnlockRect( 0 );

					pixelSum *= 10;
					pixelSum /= CELL_MASK_TEX_DIM * CELL_MASK_TEX_DIM;
					
					float avgPixel = pixelSum / 10.f;

					bool do_optimize( false );

					if( avgPixel > uniformThreshold )
					{
						optimizedUniform ++;
						do_optimize = true;
					}

					if( maxPixel < blankMaxTrheshold && avgPixel < blankTrheshold )
					{
						optimizedBlank ++;
						do_optimize = true;
						do_delete = true;
					}

					if( do_optimize )
					{
						r3dRenderer->DeleteTexture( gce.MaskTexture );
						gce.MaskTexture = NULL;
					}
				}

				if( do_delete )
				{
					DeleteMaskEntriesInUnderlyingCells( gce.TypeIdx, x, z );
					cell.MaskTextureEntries.Erase( i );
					e --;

					// deleted all entries, hence height texture is unnecessary
					if( !e )
					{
						r3dRenderer->DeleteTexture( cell.HeightTexture );
						cell.HeightTexture = NULL;

						optimizedHeightmap ++;
					}
				}
				else
				{
					i ++;
				}
			}
		}
	}

	r3dOutToLog( "Grass saving: optimized %d out %d masked tiles ( %d blanks and %d uniforms )\n", 
					optimizedBlank + optimizedUniform,
					totalCount,
					optimizedBlank,
					optimizedUniform );

	r3dOutToLog( "Grass saving: optimized %d heightmaps\n", optimizedHeightmap );

}

//------------------------------------------------------------------------

void
GrassMap::Paint( float x, float z, float radius, float dir, const r3dString& grassType )
{
	x -= mSettings.XOffset;
	z -= mSettings.ZOffset;

	r3d_assert( mInitialized );

	if (mCells.Width() == 0 || mCells.Height() == 0)
		return;

	if( Terrain && !Terrain->GetDesc().OrthoDiffuseTex )
	{
		Terrain->SetOrthoDiffuseTextureDirty() ;
	}

	uint32_t grazTypeIdx = g_pGrassLib->GetEntryIdxByName( grassType );
	r3d_assert( grazTypeIdx != uint32_t(-1) );

	float centreDX, centreDZ;
	float unusedDX, unusedDZ;

	int centreX, centreZ;

	int upperLeftX, upperLeftZ;
	int lowerRightX, lowerRightZ;

	DeriveTextureCellCoords( x, z, centreX, centreZ, centreDX, centreDZ );

	DeriveTextureCellCoords( x - radius, z - radius, upperLeftX, upperLeftZ, unusedDX, unusedDZ );
	DeriveTextureCellCoords( x + radius, z + radius, lowerRightX, lowerRightZ, unusedDX, unusedDZ );

	const float HALF_TEXEL = 0.5f / CELL_MASK_TEX_DIM;

	float relRad = radius / mCellSize / mCellsPerTextureCell;

	upperLeftX	= R3D_MIN( R3D_MAX( upperLeftX, 0 ), (int)mTextureCells.Width() - 1 );
	lowerRightX	= R3D_MIN( R3D_MAX( lowerRightX, 0 ), (int)mTextureCells.Width() - 1 );

	upperLeftZ	= R3D_MIN( R3D_MAX( upperLeftZ, 0 ), (int)mTextureCells.Height() - 1 );
	lowerRightZ	= R3D_MIN( R3D_MAX( lowerRightZ, 0 ), (int)mTextureCells.Height() - 1 );

	for( int iz = upperLeftZ, e = lowerRightZ; iz <= e; iz ++ )
	{
		for( int ix = upperLeftX, e = lowerRightX; ix <= e; ix ++ )
		{
			float dx = centreX + centreDX - ix - HALF_TEXEL;
			float dz = centreZ + centreDZ - iz - HALF_TEXEL;

			GrassTextureCell& cell = mTextureCells[ iz ][ ix ];

			if( !cell.HeightTexture )
			{
				if( dir > 0.f )
					CreateTextureCell( ix, iz );
				else
					continue;
			}

			GrassMaskTextureEntry* pgce ( NULL );

			for( uint32_t i = 0, e = cell.MaskTextureEntries.Count(); i < e; i ++ )
			{
				GrassMaskTextureEntry& gce = cell.MaskTextureEntries[ i ];

				if( gce.TypeIdx == grazTypeIdx )
				{
					pgce = &gce;
				}
			}

			if( !pgce )
			{
				CreateMaskTextureCellEntry( cell, ix, iz, grazTypeIdx, true );
				pgce = &cell.MaskTextureEntries[ cell.MaskTextureEntries.Count() - 1 ];
			}

			if( dir < 0.f && !pgce->MaskTexture )
			{
				pgce->MaskTexture = r3dRenderer->AllocateTexture();
				pgce->MaskTexture->Create( CELL_MASK_TEX_DIM, CELL_MASK_TEX_DIM, MASK_TEX_FMT, 1 );

				Bytes bytes( CELL_MASK_TEX_DIM * CELL_MASK_TEX_DIM * MASK_TEX_FMT_SIZE, 0xff );
				FillTexture( pgce->MaskTexture, bytes, true );
			}

			if( pgce->MaskTexture )
			{
				DoPaint( dx, dz, relRad, dir, pgce->MaskTexture );
			}
		}
	}
}

//------------------------------------------------------------------------

void
GrassMap::UpdateHeight( float x, float z, float radius )
{
	x -= mSettings.XOffset;
	z -= mSettings.ZOffset;

	r3d_assert( mInitialized );

	float unusedDX, unusedDZ;

	int upperLeftX, upperLeftZ;
	int lowerRightX, lowerRightZ;

	DeriveTextureCellCoords( x - radius, z - radius, upperLeftX, upperLeftZ, unusedDX, unusedDZ );
	DeriveTextureCellCoords( x + radius, z + radius, lowerRightX, lowerRightZ, unusedDX, unusedDZ );

	float relRad = radius / mCellSize;

	for( int	iz	= R3D_MAX( upperLeftZ, 0 ), 
				e	= R3D_MIN( lowerRightZ, (int)mTextureCells.Height() - 1 ); 
				iz <= e; iz ++ )
	{
		for( int	ix	= R3D_MAX( upperLeftX, 0 ), 
					e	= R3D_MIN( lowerRightX, (int)mTextureCells.Width() - 1 ); 
					ix <= e; ix ++ )
		{
			GrassTextureCell& cell = mTextureCells[ iz ][ ix ];

			if( cell.HeightTexture )
			{
				UpdateTextureCellHeight( ix, iz, cell );
			}
		}
	}

	UpdateYExtents();
}

//------------------------------------------------------------------------

void
GrassMap::ResetBounds()
{
#ifndef FINAL_BUILD
	g_pGrassMap->Close();

	r3dBoundBox bbox;
	GetGrassPlaneManager()->GetCombinedPlaneBounds( &bbox );

	g_pGrassMap->Init( bbox.Org.x, bbox.Org.z, bbox.Size.x, bbox.Size.z, GrassMap::MAX_TEX_CELL_COUNT );
	g_pGrassMap->ConformWithNewCellSize();	
#endif
}

//------------------------------------------------------------------------

void
GrassMap::UpdateHeight()
{
	for( int z = 0, e = mTextureCells.Height(); z < e; z ++ )
	{
		for( int x = 0, e = mTextureCells.Width(); x < e; x ++ )
		{
			GrassTextureCell& cell = mTextureCells[ z ][ x ];

			if( cell.HeightTexture )
			{
				UpdateTextureCellHeight( x, z, cell );
			}
		}
	}

	UpdateYExtents();
}

//------------------------------------------------------------------------

//------------------------------------------------------------------------

namespace
{
	struct SetRestoreClampAddressMode
	{
		explicit SetRestoreClampAddressMode( DWORD samp )
		{
			Sampler = samp;

			D3D_V( r3dRenderer->pd3ddev->GetSamplerState( Sampler, D3DSAMP_ADDRESSU, &PrevAddressU ) );
			D3D_V( r3dRenderer->pd3ddev->GetSamplerState( Sampler, D3DSAMP_ADDRESSV, &PrevAddressV ) );

			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( Sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP ) );
			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( Sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP ) );
		}

		~SetRestoreClampAddressMode()
		{
			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( Sampler, D3DSAMP_ADDRESSU, PrevAddressU ) );
			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( Sampler, D3DSAMP_ADDRESSV, PrevAddressV ) );
		}

		DWORD PrevAddressU;
		DWORD PrevAddressV;

		DWORD Sampler;

	};
}


//------------------------------------------------------------------------

R3D_FORCEINLINE static void PushDebugBBox( const r3dBoundBox& bbox )
{
	void PushDebugBox(	r3dPoint3D p0, r3dPoint3D p1, r3dPoint3D p2, r3dPoint3D p3,
						r3dPoint3D p4, r3dPoint3D p5, r3dPoint3D p6, r3dPoint3D p7,
						r3dColor color ) ;

	PushDebugBox(	bbox.Org,
					bbox.Org + r3dPoint3D( bbox.Size.x, 0.f, 0.f ),
					bbox.Org + r3dPoint3D( 0.f, bbox.Size.y, 0.f ), 
					bbox.Org + r3dPoint3D( bbox.Size.x, bbox.Size.y, 0.f ), 
					bbox.Org + r3dPoint3D( 0.f, 0.f, bbox.Size.z ), 
					bbox.Org + r3dPoint3D( bbox.Size.x, 0.f, bbox.Size.z ),
					bbox.Org + r3dPoint3D( 0.f, bbox.Size.y, bbox.Size.z ), 
					bbox.Org + r3dPoint3D( bbox.Size.x, bbox.Size.y, bbox.Size.z ),
					r3dColor( 0, 255, 0 )
					) ;
}

void
GrassMap::Prepare()
{
	if( Terrain )
		Terrain->UpdateOrthoDiffuseTexture();
}

void
GrassMap::Draw( const r3dCamera& cam, Path path, bool useDepthEqual, bool drawToAux )
{
	R3DPROFILE_FUNCTION("GrassMap::Draw");

	float GetDefaultSSAOValue();
	float SSAO = GetDefaultSSAOValue();

	// ensure we did initialize

	SetRestoreClampAddressMode setRestoreClamp0( 0 ); (void)setRestoreClamp0;
	SetRestoreClampAddressMode setRestoreClamp1( 1 ); (void)setRestoreClamp1;

	// compute bounds in terms of cell xz

	float grazVizRad = GetVisRad();

	float maxFadeDistanceCoef = 0.f ;

	for( int i = 0, e = g_pGrassLib->GetEntryCount() ; i < e ; i ++ )
	{
		const GrassLibEntry& gle = g_pGrassLib->GetEntry( i ) ;
		maxFadeDistanceCoef = R3D_MAX( maxFadeDistanceCoef, gle.FadeDistance ) ;
	}

	maxFadeDistanceCoef = R3D_MAX( R3D_MIN( maxFadeDistanceCoef, R3D_MAX_GRASS_PATCH_DISTANCE_COEF ), 0.f )	 ;
	grazVizRad *= maxFadeDistanceCoef ;

	float xmin = cam.x - grazVizRad;
	float xmax = cam.x + grazVizRad;

	float zmin = cam.z - grazVizRad;
	float zmax = cam.z + grazVizRad;

	int ZCellMin = int( ( zmin - mSettings.ZOffset ) / mCellSize );
	int ZCellMax = int( ( zmax - mSettings.ZOffset ) / mCellSize );

	int	z	= R3D_MAX( ZCellMin, 0 ),
		ze	= R3D_MIN( ZCellMax, int( mCells.Height() ) - 1 );

	float fz = z * mCellSize - cam.z;

	float grazVizRadSqr = grazVizRad * grazVizRad;

	int width = mCells.Width();

	float maxMeshScale			= g_pGrassLib->GetSettings().MaxMeshScale;
	r3dPoint3D maxMeshScaleVec ( maxMeshScale, maxMeshScale, maxMeshScale );

	r3dPoint3D cellScaleBase (	mCellSize + maxMeshScale * 2, 
								mCellSize + maxMeshScale * 2,
								mCellSize + maxMeshScale * 2 );

	//------------------------------------------------------------------------
	// Setup common render states

	switch( path )
	{
	case COMBINED_PATH:
		{
			GrassPShaderID psid;

			psid.aux			= drawToAux;
			psid.do_clip		= !useDepthEqual;
			psid.output_depth	= 1;

			r3dRenderer->SetPixelShader( gGrassPSIds[ psid.Id ] );
			r3dRenderer->SetRenderingMode(R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW | R3D_BLEND_PUSH );
		}
		break ;

	case DEPTH_PATH:
		r3dRenderer->SetPixelShader( P0_PIXEL_SHADER_ID );
		r3dRenderer->SetRenderingMode(R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW | R3D_BLEND_PUSH );
		break ;

	case COLOR_PATH:
		GrassPShaderID psid;

		psid.aux			= drawToAux;
		psid.do_clip		= !useDepthEqual;
		psid.output_depth	= 0;

		r3dRenderer->SetRenderingMode(R3D_BLEND_ALPHA | R3D_BLEND_ZC | R3D_BLEND_PUSH );
		D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE ) );
		D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE ) );
		break ;
	}

	if( useDepthEqual )
	{
		D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_ZFUNC, D3DCMP_EQUAL ) ) ;
	}

	r3dTexture *tex = __r3dShadeTexture[2];
	if ( Terrain )
	{
		tex = Terrain->GetDesc().OrthoDiffuseTex ;
	}
	r3dRenderer->SetTex( tex, 1 );
	r3dSetFiltering( R3D_BILINEAR, 1 );

	//------------------------------------------------------------------------

	float halfCellSize = mCellSize * 0.5f;

	for( ; z <= ze; z ++, fz += mCellSize )
	{
		float fz_for_x0 = fz;
		float fz_for_x1 = fz + mCellSize;

		// oh no...
		if( ((unsigned&) fz_for_x0  ^ (unsigned&) fz_for_x1 ) & 0x80000000 )
		{
			fz_for_x0 = 0.f;
		}

		float fz_for_x = R3D_MIN( fabsf( fz_for_x0 ), fabsf( fz_for_x1 ) );

		float fx = sqrtf( R3D_MAX( grazVizRadSqr - fz_for_x * fz_for_x, 0.f ) );

		float fx_min = -fx + cam.x ;
		float fx_max = +fx + cam.x;

		int XCellMin = R3D_MAX( int( ( fx_min - mSettings.XOffset ) / mCellSize ), 0 );
		int XCellMax = R3D_MIN( int( ( fx_max - mSettings.XOffset ) / mCellSize ), width - 1 );

		for( int x = XCellMin, xe = XCellMax; x <= xe; x ++ )
		{			
			const GrassCell& cell = mCells[ z ][ x ];

			int texCellX = x / mCellsPerTextureCell;
			int texCellZ = z / mCellsPerTextureCell;

			const GrassTextureCell& gtc = mTextureCells[ texCellZ ][ texCellX ];

			if( gtc.HeightTexture )
			{
				r3dBoundBox bbox;

				bbox.Org	= cell.Position - maxMeshScaleVec;
				bbox.Size	= cellScaleBase;

				bbox.Size.y	= maxMeshScale * 3 + cell.YMax - cell.Position.y;

				if( !r3dRenderer->IsBoxInsideFrustum( bbox ) )
					continue;

				DrawCell( gtc, texCellX, texCellZ, cell, cam, x, z, SSAO );

#ifndef FINAL_BUILD
				if( r_grass_show_boxes->GetInt() )
				{
					for( uint32_t i = 0, e = cell.Entries.Count(); i < e; i ++ )
					{
						if( cell.Entries[ i ].TypeIdx == mDebugGrassType )
						{
							PushDebugBBox( bbox );
							break;
						}
					}				
				}
#endif
			}
		}
	}

	if( path == COLOR_PATH )
	{
		D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA ) );
		D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_COLORWRITEENABLE1, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA ) );
	}

	if( useDepthEqual )
	{
		r3dRenderer->RestoreZFunc();
	}

	r3dRenderer->SetRenderingMode( R3D_BLEND_POP );
}

//------------------------------------------------------------------------

void
GrassMap::ConformWithNewCellSize()
{
	uint32_t totalXLen = CELL_MASK_TEX_DIM * mTextureCells.Width();
	uint32_t totalZLen = CELL_MASK_TEX_DIM * mTextureCells.Height();

	if( totalXLen * totalZLen > ( 256 + 1 ) * ( 256 + 1 ) * 32 * 32 )
	{
		r3dArtBug( "GrassMap::ConformWithNewCellSize: current map is too detailed, can't resize - resetting to new size\n" );
		ClearCells();
		return;
	}

	typedef TArray< Bytes > CompoundTextures;
		
	CompoundTextures compoundTexes ( g_pGrassLib->GetEntryCount() );

	for( uint32_t i = 0, e = compoundTexes.Count(); i < e; i ++ )
	{
		compoundTexes[ i ].Resize( totalXLen * totalZLen );
	}

	for( uint32_t typeIdx = 0, e = g_pGrassLib->GetEntryCount(); typeIdx < e; typeIdx ++ )
	{
		unsigned char* compoundTex = &compoundTexes[ typeIdx ][ 0 ];

		for( uint32_t z = 0, e = mTextureCells.Height(); z < e; z ++ )
		{
			for( uint32_t x = 0, e = mTextureCells.Width(); x < e; x ++ )
			{
				const GrassTextureCell& cell = mTextureCells[ z ][ x ];

				for( uint32_t i = 0, e = cell.MaskTextureEntries.Count(); i < e; i ++ )
				{
					const GrassMaskTextureEntry& gce = cell.MaskTextureEntries[ i ];
					if( gce.TypeIdx == typeIdx )
					{
						char* dest = (char*)compoundTex + z * CELL_MASK_TEX_DIM * totalXLen + x * CELL_MASK_TEX_DIM;

						if( gce.MaskTexture )
						{
							IDirect3DTexture9* tex = gce.MaskTexture->AsTex2D();

							D3DLOCKED_RECT lrect;
							D3D_V( tex->LockRect( 0, &lrect, NULL, D3DLOCK_READONLY ) );

							char* data = (char*)lrect.pBits;

							for( uint32_t tz = 0, tze = CELL_MASK_TEX_DIM; tz < tze; tz ++ )
							{
								for( uint32_t tx = 0, txe = CELL_MASK_TEX_DIM; tx < txe; tx ++ )
								{
									*dest++ = *data++;
								}

								dest += totalXLen - CELL_MASK_TEX_DIM;
								data += lrect.Pitch - CELL_MASK_TEX_DIM;
							}

							D3D_V( tex->UnlockRect( 0 ) );
						}
						else
						{

							for( uint32_t tz = 0, tze = CELL_MASK_TEX_DIM; tz < tze; tz ++ )
							{
								for( uint32_t tx = 0, txe = CELL_MASK_TEX_DIM; tx < txe; tx ++ )
								{
									*dest++ = (char)255;
								}

								dest += totalXLen - CELL_MASK_TEX_DIM;
							}							
						}
					}
				}
			}
		}
	}

	float oldCellSize	= mCellSize * mCellsPerTextureCell;
	float oldExcessX	= mTextureCells.Width() - mSettings.XLength / mCellSize / mCellsPerTextureCell;
	float oldExcessZ	= mTextureCells.Height() - mSettings.ZLength / mCellSize / mCellsPerTextureCell;

	ClearCells();

	SetupCellArrays( mCells.Width(), mCells.Height(), MAX_TEX_CELL_COUNT );

	uint32_t newTotalXLen = CELL_MASK_TEX_DIM * mTextureCells.Width();
	uint32_t newTotalZLen = CELL_MASK_TEX_DIM * mTextureCells.Height();

	float newExcessX	= mTextureCells.Width() - mSettings.XLength / mCellSize / mCellsPerTextureCell;
	float newExcessZ	= mTextureCells.Height() - mSettings.ZLength / mCellSize / mCellsPerTextureCell;

	for( uint32_t i = 0, e = compoundTexes.Count() ; i < e ; i ++ )
	{
		Bytes newCompoundTex ( newTotalXLen * newTotalZLen );

		ResizeTexture(	totalXLen, totalZLen, oldExcessX, oldExcessZ, compoundTexes[ i ],
						newTotalXLen, newTotalZLen, newExcessX, newExcessZ, CELL_MASK_TEX_DIM, CELL_MASK_TEX_DIM, newCompoundTex );

		CreateCells( newCompoundTex, i );
	}
}

//------------------------------------------------------------------------

void
GrassMap::ClearCells()
{
	float xo = mSettings.XOffset;
	float zo = mSettings.ZOffset;
	float xl = mSettings.XLength;
	float zl = mSettings.ZLength;
	Close();
	Init(xo, zo, xl, zl, MAX_TEX_CELL_COUNT);
}

//------------------------------------------------------------------------

void
GrassMap::ClearGrassType( const r3dString& grassType )
{
	uint32_t grazTypeIdx = g_pGrassLib->GetEntryIdxByName( grassType );

	for( uint32_t z = 0, e = mCells.Height(); z < e; z ++ )
	{
		for( uint32_t x = 0, e = mCells.Width(); x < e; x ++ )
		{
			GrassCell& cell = mCells[ z ][ x ];

			for( uint32_t i = 0, e = cell.Entries.Count(); i < e; )
			{
				GrassCellEntry& gce = cell.Entries[ i ];
				if( gce.TypeIdx == grazTypeIdx )
				{
					e -- ;
					cell.Entries.Erase( i ) ;
				}
				else
					i ++ ;
			}
		}
	}

}

//------------------------------------------------------------------------

r3dString
GrassMap::GetUsedTypes() const
{
	typedef std::set< int > UsedIndexes ;

	UsedIndexes usedIndexes ;

	for( uint32_t z = 0, e = mCells.Height(); z < e; z ++ )
	{
		for( uint32_t x = 0, e = mCells.Width(); x < e; x ++ )
		{
			const GrassCell& cell = mCells[ z ][ x ];

			for( uint32_t i = 0, e = cell.Entries.Count(); i < e; i ++ )
			{
				const GrassCellEntry& gce = cell.Entries[ i ];
				usedIndexes.insert( gce.TypeIdx ) ;
			}
		}
	}

	r3dString result ;

	for( UsedIndexes::const_iterator i = usedIndexes.begin(), e = usedIndexes.end() ; i != e ; ++ i )
	{
		result += g_pGrassLib->GetEntryNameByIdx( *i ) ;
		result += "\n" ;
	}

	return result ;
}

//------------------------------------------------------------------------

void GrassMap::SetDebugType( const r3dString& typeName )
{
	mDebugGrassType = g_pGrassLib->GetEntryIdxByName( typeName ) ;
}

//------------------------------------------------------------------------
/*static*/

float
GrassMap::GetVisRad()
{
	return r_grass_view_coef->GetFloat() * r_grass_view_dist->GetFloat() * r_grass_zoom_coef->GetFloat();
}

//------------------------------------------------------------------------

void GrassMap::SetupCellArrays( int nominzalCellsX, int nominzalCellsZ, int maxTexCellCount )
{
	int texCellsX, texCellsZ;

	GetTextureCellsBasedOnNormalCells( nominzalCellsX, nominzalCellsZ, maxTexCellCount, &mCellsPerTextureCell, &texCellsX, &texCellsZ );

	mTextureCells.Resize( texCellsX, texCellsZ );

	mCells.Resize(	mTextureCells.Width() * mCellsPerTextureCell, 
					mTextureCells.Height() * mCellsPerTextureCell );
}

//------------------------------------------------------------------------

void GrassMap::GetNominalCellCount( float fWidth, float fHeight, int *oCellsX, int* oCellsZ )
{
	*oCellsX = int( fWidth / mCellSize ) + 1;
	*oCellsZ = int( fHeight / mCellSize ) + 1;
}

//------------------------------------------------------------------------

void GrassMap::GetTextureCellsBasedOnNormalCells( int nominzalCellsX, int nominzalCellsZ, int maxTexCellCount, int * cellsInTexCell, int * oCountX, int * oCountZ )
{
	int texCellsX = R3D_MIN( nominzalCellsX, maxTexCellCount );
	int texCellsZ = R3D_MIN( nominzalCellsZ, maxTexCellCount );

	*cellsInTexCell = nominzalCellsX / texCellsX;
	*cellsInTexCell = R3D_MAX( int( nominzalCellsZ / texCellsZ ), *cellsInTexCell );

	for( ; ; )
	{
		texCellsX = nominzalCellsX / *cellsInTexCell;
		texCellsZ = nominzalCellsZ / *cellsInTexCell;

		texCellsX = R3D_MAX( texCellsX, 1 );
		texCellsZ = R3D_MAX( texCellsZ, 1 );

		if( nominzalCellsX % *cellsInTexCell )
			texCellsX ++;

		if( nominzalCellsZ % *cellsInTexCell )
			texCellsZ ++;

		if( texCellsX <= maxTexCellCount && texCellsZ <= maxTexCellCount )
			break;

		++ *cellsInTexCell ;
	}

	*oCountX = texCellsX;
	*oCountZ = texCellsZ;
}

//------------------------------------------------------------------------

void
GrassMap::DeriveTextureCellCoords( float X, float Z, int& oCellX, int& oCellZ, float& oCellDX, float& oCellDZ )
{
	float relX = X / mCellSize / mCellsPerTextureCell;
	float relZ = Z / mCellSize / mCellsPerTextureCell;

	oCellX = int( relX );
	oCellZ = int( relZ );

	oCellDX = relX - oCellX;
	oCellDZ = relZ - oCellZ;
}

//------------------------------------------------------------------------

void
GrassMap::TexCellToWorldCoords( int CellX, int CellZ, float CellDX, float CellDZ, float& oX, float& oZ )
{
	oX = ( CellX + CellDX ) * mCellSize * mCellsPerTextureCell;
	oZ = ( CellZ + CellDZ ) * mCellSize * mCellsPerTextureCell;
}

//------------------------------------------------------------------------

void GrassMap::CreateTextureCell( int X, int Z )
{
	CreateTextureCellHeightTexture( mTextureCells[ Z ][ X ] );
	UpdateTextureCellHeight( X, Z, mTextureCells[ Z ][ X ] );
}

//------------------------------------------------------------------------

void
GrassMap::CreateCells( const Bytes& CompoundTex, uint32_t TypeIdx )
{
	uint32_t totalDim = mCells.Width() * CELL_MASK_TEX_DIM;

	for( uint32_t z = 0, e = mCells.Height(); z < e; z ++ )
	{
		for( uint32_t x = 0, e = mCells.Width(); x < e; x ++ )
		{
			uint32_t x0 = x * CELL_MASK_TEX_DIM;
			uint32_t x1 = x0 + CELL_MASK_TEX_DIM;

			uint32_t z0 = z * CELL_MASK_TEX_DIM;
			uint32_t z1 = z0 + CELL_MASK_TEX_DIM;

			bool found = false;

			// see if we have any non zero mask points
			{
				for( uint32_t z = z0, e = z1; z < e && !found ; z ++ )
				{
					for( uint32_t x = x0, e = x1; x < e && !found ; x ++ )
					{
						if( CompoundTex[ x + z * totalDim ] )
						{
							found = true;
						}
					}
				}
			}

			if( found )
			{
				CreateTextureCell( x, z );

				GrassTextureCell& cell = mTextureCells[ z ][ x ];

				CreateMaskTextureCellEntry( cell, x, z, TypeIdx, true );

				GrassMaskTextureEntry& gce = cell.MaskTextureEntries[ cell.MaskTextureEntries.Count() - 1 ];

				IDirect3DTexture9* tex = gce.MaskTexture->AsTex2D();

				D3DLOCKED_RECT lrect;
				D3D_V( tex->LockRect( 0, &lrect, NULL, 0 ) );

				unsigned char* p = (unsigned char*)lrect.pBits;

				for( uint32_t z = 0, zz = z0, e = CELL_MASK_TEX_DIM; z < e; z ++, zz++ )
				{
					for( uint32_t x = 0, xx = x0, e = CELL_MASK_TEX_DIM; x < e; x ++, xx++ )
					{
						*p ++ = CompoundTex[ xx + zz * totalDim ];
					}

					p += lrect.Pitch - CELL_MASK_TEX_DIM;
				}

				tex->UnlockRect( 0 );
			}
		}
	}
}

//------------------------------------------------------------------------

struct CreatMskTextureCellEntryParams
{
	GrassTextureCell* cell; 
	int X;
	int Z;
	uint32_t TypeIdx;
	bool CreateTexture;

	GrassMap* gm;
};

void CreateMaskTextureCellEntryInMainThread( void* param )
{
	CreatMskTextureCellEntryParams* params = (CreatMskTextureCellEntryParams*)param;

	params->gm->DoCreateMaskTextureCellEntry( *params->cell, params->X, params->Z, params->TypeIdx, params->CreateTexture );	
}

void GrassMap::CreateMaskTextureCellEntry( GrassTextureCell& cell, int X, int Z, uint32_t TypeIdx, bool createTexture )
{
	if( R3D_IS_MAIN_THREAD() || !createTexture )
	{
		DoCreateMaskTextureCellEntry( cell, X, Z, TypeIdx, createTexture );
	}
	else
	{
		CreatMskTextureCellEntryParams params;

		params.cell = &cell; 
		params.X = X;
		params.Z = Z;
		params.TypeIdx = TypeIdx;
		params.CreateTexture = createTexture;

		params.gm = this;

		ProcessCustomDeviceQueueItem( CreateMaskTextureCellEntryInMainThread, &params );
	}
}

//------------------------------------------------------------------------

void GrassMap::DoCreateMaskTextureCellEntry( GrassTextureCell& cell, int X, int Z, uint32_t TypeIdx, bool createTexture )
{
	for( int i = 0, e = (int)cell.MaskTextureEntries.Count() ; i < e ; i ++ )
	{
		r3d_assert( cell.MaskTextureEntries[ i ].TypeIdx != TypeIdx );
	}

	GrassMaskTextureEntry entry;

	entry.TypeIdx		= TypeIdx;

	if( createTexture )
	{
		entry.MaskTexture	= r3dRenderer->AllocateTexture();
		entry.MaskTexture->Create( CELL_MASK_TEX_DIM, CELL_MASK_TEX_DIM, MASK_TEX_FMT, 1 );

		D3DLOCKED_RECT lockedRect;

		IDirect3DTexture9* tex = entry.MaskTexture->AsTex2D();

		D3D_V( tex->LockRect( 0, &lockedRect, NULL, 0 ) );

		unsigned char* data = (unsigned char* )lockedRect.pBits;

		memset( data, 0, CELL_MASK_TEX_DIM * lockedRect.Pitch );

		D3D_V( tex->UnlockRect( 0 ) );
	}

	for( int cz = Z * mCellsPerTextureCell, e = (Z + 1) * mCellsPerTextureCell; cz < e ; cz ++ )
	{
		for( int cx = X * mCellsPerTextureCell, e = (X + 1) * mCellsPerTextureCell; cx < e ; cx ++ )
		{
			CreateCellEntry( mCells[ cz ][ cx ], TypeIdx );
		}
	}

	cell.MaskTextureEntries.PushBack( entry );
}

//------------------------------------------------------------------------

void
GrassMap::CreateCellEntry( GrassCell& cell, uint32_t TypeIdx )
{
	for( int i = 0, e = cell.Entries.Count(); i < e; i ++ )
	{
		if( cell.Entries[ i ].TypeIdx == TypeIdx )
			return ;
	}

	GrassCellEntry entry;

	entry.TypeIdx		= TypeIdx;

	cell.Entries.PushBack( entry );
}

//------------------------------------------------------------------------

void
GrassMap::DoPaint( float CentreDX, float CentreDZ, float Radius, float Dir, r3dTexture* Tex )
{
	int sx = R3D_MIN( R3D_MAX( int(( CentreDX - Radius ) * CELL_MASK_TEX_DIM ), 0 ), CELL_MASK_TEX_DIM - 1 );
	int sz = R3D_MIN( R3D_MAX( int(( CentreDZ - Radius ) * CELL_MASK_TEX_DIM ), 0 ), CELL_MASK_TEX_DIM - 1 );
	int ex = R3D_MIN( R3D_MAX( int(( CentreDX + Radius ) * CELL_MASK_TEX_DIM ), 0 ), CELL_MASK_TEX_DIM - 1 );
	int ez = R3D_MIN( R3D_MAX( int(( CentreDZ + Radius ) * CELL_MASK_TEX_DIM ), 0 ), CELL_MASK_TEX_DIM - 1 );

	IDirect3DTexture9* tex = static_cast<IDirect3DTexture9*> ( Tex->GetD3DTexture() );

	D3DLOCKED_RECT lockedRect;
	D3D_V( tex->LockRect( 0, &lockedRect, NULL, 0 ) );

	unsigned char* weights = (unsigned char*)lockedRect.pBits;

	float Scale = 255.f;

	bool goDown = Dir > 0 ? false : true;

	float A, B;

	if( goDown )
	{
		A = 0.0f;
		B = 1.0f;
	}
	else
	{
		A = 1.0f;
		B = -1.0f;
	}

	for( int j = sz; j <= ez; j ++ )
	{
		for( int i = sx; i <= ex; i ++ )
		{
			unsigned char* dest = weights + j * lockedRect.Pitch + i;

			float dx = CentreDX - float( i ) / CELL_MASK_TEX_DIM;
			float dz = CentreDZ - float( j ) / CELL_MASK_TEX_DIM;

			float dist = sqrtf( dx*dx + dz*dz );

			float f = dist / Radius;

			int toReplace = int( Scale * R3D_MAX( A + B * f * f * f * f, 0.f ) );

			if( goDown )
				*dest = R3D_MAX( R3D_MIN( (int)*dest, toReplace ), 0 );
			else
				*dest = R3D_MIN( R3D_MAX( (int)*dest, toReplace ), 255 );
		}
	}

	D3D_V( tex->UnlockRect( 0 ) );
}

//------------------------------------------------------------------------

struct UpdateTextureCellHeightParams
{
	int X;
	int Z;
	GrassTextureCell * cell;
	GrassMap * gm;
};

void UpdateTextureCellHeightInMainThread( void* param )
{
	UpdateTextureCellHeightParams* params = ( UpdateTextureCellHeightParams* )param;

	params->gm->DoUpdateTextureCellHeight( params->X, params->Z, *params->cell );
}

void
GrassMap::UpdateTextureCellHeight( int X, int Z, GrassTextureCell& cell )
{
	if( R3D_IS_MAIN_THREAD() )
	{
		DoUpdateTextureCellHeight( X, Z, cell );
	}
	else
	{
		UpdateTextureCellHeightParams params;

		params.X 	= X;
		params.Z 	= Z;
		params.cell	= &cell;
		params.gm	= this;

		ProcessCustomDeviceQueueItem( UpdateTextureCellHeightInMainThread, &params );
	}
}

//------------------------------------------------------------------------

void GrassMap::DoUpdateTextureCellHeight( int X, int Z, GrassTextureCell& cell )
{
	if( HEIGHT_TEX_FMT_SIZE == 2 )
		DoUpdateTextureCellHeightT<unsigned short> ( X, Z, cell );
	else
		DoUpdateTextureCellHeightT<unsigned char> ( X, Z, cell );
}

//------------------------------------------------------------------------

template <typename T>
void GrassMap::DoUpdateTextureCellHeightT( int X, int Z, GrassTextureCell& cell )
{
	int PROJ_DIM = CELL_HEIGHT_TEX_DIM - 1;

	float ymin( FLT_MAX ), ymax( -FLT_MAX );
	for( int z = 0, e = CELL_HEIGHT_TEX_DIM; z < e; z ++ )	
	{		
		for( int x = 0, e = CELL_HEIGHT_TEX_DIM; x < e; x ++ )
		{
			r3dPoint3D wpoint( 0, 0, 0 );

			TexCellToWorldCoords( X, Z, float( x ) / PROJ_DIM, float( z ) / PROJ_DIM, wpoint.x, wpoint.z );

			wpoint.x += mSettings.XOffset;
			wpoint.z += mSettings.ZOffset;

			float height = GetGrassHeight ( wpoint );

			ymin = R3D_MIN( ymin, height );
			ymax = R3D_MAX( ymax, height );
		}
	}

	float ySize = ymax - ymin;

	D3DLOCKED_RECT lockedRect;

	IDirect3DTexture9* tex = cell.HeightTexture->AsTex2D();

	D3D_V( tex->LockRect( 0, &lockedRect, NULL, 0 ) );

	T* data = (T* )lockedRect.pBits;

	for( int z = 0, e = CELL_HEIGHT_TEX_DIM; z < e; z ++ )	
	{		
		for( int x = 0, e = CELL_HEIGHT_TEX_DIM; x < e; x ++ )
		{
			r3dPoint3D wpoint( 0, 0, 0 );

			TexCellToWorldCoords( X, Z, float( x ) / PROJ_DIM, float( z ) / PROJ_DIM, wpoint.x, wpoint.z );

			wpoint.x += mSettings.XOffset;
			wpoint.z += mSettings.ZOffset;

			float height = GetGrassHeight ( wpoint );

			float normHeight = ( height - ymin ) / ySize;

			if( sizeof( T ) == 2 )
				*data++ = R3D_MIN( R3D_MAX( int ( normHeight * 65535.f ), 0 ), 65535 );
			else
				*data++ = R3D_MIN( R3D_MAX( int ( normHeight * 255.f ), 0 ), 255 );
		}

		data += lockedRect.Pitch - sizeof( data[ 0 ] ) * CELL_HEIGHT_TEX_DIM;
	}

	D3D_V( tex->UnlockRect( 0 ) );

	// update cell height data
	for( int cz = Z * mCellsPerTextureCell, e = (Z + 1) * mCellsPerTextureCell ; cz < e ; cz ++ )
	{
		for( int cx = X * mCellsPerTextureCell, e = (X + 1) * mCellsPerTextureCell ; cx < e ; cx ++ )
		{
			if( cx < 0 || cx >= (int)mCells.Width() 
				||
				cz < 0 || cz >= (int)mCells.Height()
				)
				continue;

			GrassCell& gtc = mCells[ cz ][ cx ];

			gtc.Position.y		= ymin;
			gtc.YMax			= ymax;
		}
	}
}

//------------------------------------------------------------------------

void GrassMap::DrawCell( const GrassTextureCell& gtc, int TexX, int TexZ, const GrassCell& cell, const r3dCamera& Cam, int X, int Z, float SSAO )
{
	D3DPERF_BeginEvent ( 0x0, L"DrawGrass" );

	float UVScale_Height = 0.f;
	float UOffset_Height = 0.f;
	float VOffset_Height = 0.f;

	float UVScale_Mask = 0.f;
	float UOffset_Mask = 0.f;
	float VOffset_Mask = 0.f;

	struct
	{
		void operator() ( GrassMap* m, int X, int Z, float* oUVScale, float* oUOffset, float* oVOffset, int DIM )
		{
			int subCellX = X % m->mCellsPerTextureCell;
			int subCellZ = Z % m->mCellsPerTextureCell;

			float k = float( DIM - 1 ) / DIM;

			*oUOffset = k * ( subCellX + 0.5f ) / m->mCellsPerTextureCell + 0.5f / DIM;
			*oVOffset = k * ( subCellZ + 0.5f ) / m->mCellsPerTextureCell + 0.5f / DIM;

			r3dPoint3D halfChunkScale = GetGrassChunkScale() * 0.5f;
			float halfCellScale = m->mCellSize * 0.5f;

			*oUVScale = 0.5f * halfChunkScale.x / halfCellScale / m->mCellsPerTextureCell * k;
		}
	} fillUVXFormsForDim;

	fillUVXFormsForDim( this, X, Z, &UVScale_Height, &UOffset_Height, &VOffset_Height, CELL_HEIGHT_TEX_DIM );
	fillUVXFormsForDim( this, X, Z, &UVScale_Mask, &UOffset_Mask, &VOffset_Mask, CELL_MASK_TEX_DIM );

	float halfCellScale = mCellSize * 0.5f;

	D3DXMATRIX scaleMtx;
	D3DXMATRIX animScaleMtx;

	r3dPoint3D halfChunkScale = GetGrassChunkScale() * 0.5f;

	D3DXMatrixScaling( &scaleMtx, halfChunkScale.x, halfChunkScale.y, halfChunkScale.z );

	float twoPi = 2 * R3D_PI;

	float animScaleX = twoPi / halfChunkScale.x;
	float animScaleZ = twoPi / halfChunkScale.z;

	float visRad = GetVisRad() ;

	D3DXMatrixScaling( &animScaleMtx, animScaleX, twoPi / halfChunkScale.y, animScaleZ );

	float heightScale = (cell.YMax - cell.Position.y) / halfChunkScale.y;

	D3DXVECTOR4 vDistConstant = D3DXVECTOR4( Cam.x * animScaleX, Cam.z * animScaleZ, 0.f, 1.f ) ;

	float invTerraWidth		=  1.f / mSettings.XLength ;
	float invTerraHeight	=  1.f / mSettings.ZLength ;

	if( Terrain )
	{
		const r3dTerrainDesc& desc = Terrain->GetDesc() ;

		invTerraWidth = 1.f / desc.XSize ;
		invTerraHeight = 1.f / desc.ZSize ;
	}

	{
		r3dRenderer->SetCullMode( D3DCULL_NONE );

		r3dSetFiltering( R3D_BILINEAR, D3DVERTEXTEXTURESAMPLER0 );
		r3dRenderer->SetTex( gtc.HeightTexture, D3DVERTEXTEXTURESAMPLER0 );

		r3dRenderer->pd3ddev->SetSamplerState( D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
		r3dRenderer->pd3ddev->SetSamplerState( D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

		d3dc._SetDecl( GrassChunk::VDecl );
	}

	for( uint32_t i = 0, e = cell.Entries.Count(); i < e; i ++ )
	{
		const GrassCellEntry& gce = cell.Entries[ i ];

		const GrassLibEntry& gle = g_pGrassLib->GetEntry( gce.TypeIdx );

		r3dPoint3D pos = cell.Position + r3dPoint3D( halfCellScale, 0.f, halfCellScale );

		if( ( pos - gCam ).Length() > gle.FadeDistance * visRad + mCellSize )
			continue ;

		if( !r_grass_show_debug->GetBool() )
		{
			float visRadScaledSqr = visRad * animScaleX * gle.FadeDistance ;
			visRadScaledSqr *= visRadScaledSqr;

			vDistConstant.z = -5.f / visRadScaledSqr ;
			vDistConstant.w = 5.f ;
		}
		else
		{
			vDistConstant.z = 0.f ;
			vDistConstant.w = 1.f ;
		}

		bool hasNormalTexture = !!Terrain2;

		// vertex shader constants ( common for all chunks )
		{
			D3DXMATRIX world;

			D3DXMatrixTranslation( &world, pos.x, pos.y, pos.z );

			D3DXMatrixMultiply( &world, &scaleMtx, &world );

			D3DXMATRIX ShaderMat =  world * r3dRenderer->ViewProjMatrix ;
			D3DXMatrixTranspose( &ShaderMat, &ShaderMat );

			D3DXVECTOR4 vConsts[ 12 ];
			// float4x4    mWVP                 : register( c0 );
			memcpy( vConsts, &ShaderMat, sizeof ShaderMat );

			D3DXMatrixMultiply( &world, &world, &animScaleMtx );

			// float4      vDescaleX            : register( c4 );
			vConsts[4] = D3DXVECTOR4( world.m[0][0], world.m[1][0], world.m[2][0], world.m[3][0] );

			// float4      vDescaleZ            : register( c5 );
			vConsts[5] = D3DXVECTOR4( world.m[0][2], world.m[1][2], world.m[2][2], world.m[3][2] );

			// float4      vHeightScale_Time    : register( c6 );
			vConsts[6] = D3DXVECTOR4( heightScale, r_grass_anim_amp->GetFloat(), Time, 0 );

			// float4      vTexcoordTransform_H : register( c7 );
			vConsts[7] = D3DXVECTOR4( UVScale_Height, UVScale_Height, UOffset_Height, VOffset_Height );

			// float4      vTexcoordTransform_M : register( c8 );
			vConsts[8] = D3DXVECTOR4( UVScale_Mask, UVScale_Mask, UOffset_Mask, VOffset_Mask );

			// float4      vDistFade            : register( c9 );
			vConsts[9] = vDistConstant;

			// float4      vWorldTexcTransform  : register( c10 );
			vConsts[10] = D3DXVECTOR4( halfChunkScale.x, halfChunkScale.z, pos.x, pos.z );

			vConsts[10].x *= invTerraWidth;
			vConsts[10].y *= invTerraHeight;
			vConsts[10].z *= invTerraWidth;
			vConsts[10].w *= invTerraHeight;

			vConsts[10].y = -vConsts[10].y;
			vConsts[10].w = 1.0f - vConsts[10].w;

			D3DXVECTOR3 vCamDir = -D3DXVECTOR3(	r3dRenderer->ViewMatrix._13, 
												r3dRenderer->ViewMatrix._23,
												r3dRenderer->ViewMatrix._33 );

			D3DXVECTOR3 vNormal = vCamDir;

			vNormal.y = 0;

			if( !hasNormalTexture )
				vNormal += D3DXVECTOR3( 0.0f, 1.0f, 0.0f );

			D3DXVec3Normalize( &vNormal, &vNormal );

			if( !hasNormalTexture )
				vNormal = vNormal * 0.5f + D3DXVECTOR3( 0.5f, 0.5f, 0.5f );
			else
				vNormal *= 0.4f;

			// float3      vGrassNormal         : register( c11 );
			vConsts[11] = D3DXVECTOR4( vNormal.x, vNormal.y, vNormal.z, 0.f );

			D3D_V( r3dRenderer->pd3ddev->SetVertexShaderConstantF( 0, (float*)vConsts, sizeof vConsts / sizeof vConsts[ 0 ] ) );
		}

		{
			const GrassMaskTextureEntry& gmte = gtc.MaskTextureEntries[ i ];

			GrassVShaderID vsid;

			if( gmte.MaskTexture )
			{
				r3dSetFiltering( R3D_BILINEAR, D3DVERTEXTEXTURESAMPLER1 );
				r3dRenderer->SetTex( gmte.MaskTexture, D3DVERTEXTEXTURESAMPLER1 );

				r3dRenderer->pd3ddev->SetSamplerState( D3DVERTEXTEXTURESAMPLER1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
				r3dRenderer->pd3ddev->SetSamplerState( D3DVERTEXTEXTURESAMPLER1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

				vsid.masked = 1;
			}
			else
			{
				vsid.masked = 0;
			}

			if( hasNormalTexture )
			{
				r3dSetFiltering( R3D_BILINEAR, D3DVERTEXTEXTURESAMPLER2 );
				r3dRenderer->SetTex( Terrain2->GetNormalTexture(), D3DVERTEXTEXTURESAMPLER2 );

				r3dRenderer->pd3ddev->SetSamplerState( D3DVERTEXTEXTURESAMPLER2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
				r3dRenderer->pd3ddev->SetSamplerState( D3DVERTEXTEXTURESAMPLER2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

				vsid.has_normals = 1;
			}
			else
			{
				vsid.has_normals = 0;
			}

			r3dRenderer->SetVertexShader( gGrassVSIds[ vsid.Id ] );
		}

		for( uint32_t i = 0, e = gle.Chunks.Count(); i < e; i ++ )
		{
			const GrassChunk& chunk =	gle.Chunks[ i ];

			int varIdx =  ( X ^ Z ) % chunk.NumVariations;

			chunk.VertexBuffs[ varIdx ]->Set( 0 );
			chunk.IndexBuffer->Set();

			// float3 vAlphaRef_TintStrength_SSAO   : register( c0 );
			float vPSConsts[ 4 ] = { chunk.AlphaRef, chunk.TintStrength, SSAO, 0 };

			D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, vPSConsts, 1 ) );

			r3dSetFiltering( R3D_ANISOTROPIC, 0 );
			r3dRenderer->SetTex( chunk.Texture );

			UINT triCount = chunk.IndexBuffer->GetItemCount() / 3;

			r3dRenderer->DrawIndexed( D3DPT_TRIANGLELIST, 0, 0, chunk.VertexBuffs[ varIdx ]->GetItemCount(), 0, triCount  );
		}
	}

	r3dRenderer->RestoreCullMode();

	D3DPERF_EndEvent ();
	
}

//------------------------------------------------------------------------

void
GrassMap::LoadSettings( const r3dString& HomeDir )
{
	r3dString path = HomeDir + AR_GRAZ_PAZ "\\" + "GrassSettings.xml";

	r3dFile* f = r3d_open( path.c_str(), "rb" );
	if( !f )
	{
		return;
	}

	char* fileBuffer = new char[f->size + 1];
	fread(fileBuffer, f->size, 1, f);
	fileBuffer[f->size] = 0;

	pugi::xml_document xmlFile;
	pugi::xml_parse_result parseResult = xmlFile.load_buffer_inplace(fileBuffer, f->size);
	if(!parseResult)
		r3dError( "Failed to parse XML %s, error: %s", path, parseResult.description() );

	pugi::xml_node root = xmlFile.child( "root" );
	if( !root.empty() )
	{
		pugi::xml_node settings = root.child( "settings" );
		mSettings.XOffset = settings.attribute( "x_offset" ).as_float();
		mSettings.ZOffset = settings.attribute( "z_offset" ).as_float();
	}

	delete [] fileBuffer;

	fclose( f );
}

//------------------------------------------------------------------------

void
GrassMap::SaveSettings( const r3dString& HomeDir )
{
	pugi::xml_document xmlFile;
	xmlFile.append_child( pugi::node_comment ).set_value("Grass Settings File");
	pugi::xml_node xmlRoot = xmlFile.append_child();
	xmlRoot.set_name( "root" );

	pugi::xml_node xmlSettings = xmlRoot.append_child();
	xmlSettings.set_name( "settings" );

	xmlSettings.append_attribute( "x_offset" ) = mSettings.XOffset;
	xmlSettings.append_attribute( "z_offset" ) = mSettings.ZOffset;

	r3dString path = HomeDir + AR_GRAZ_PAZ "\\" + "GrassSettings.xml";

	xmlFile.save_file( path.c_str() );
}

//------------------------------------------------------------------------

bool
GrassMap::SaveCellData( const r3dString& FilePath )
{
	r3d_assert( g_pGrassLib->GetEntryCount() ) ;

	FILE* fout = fopen( FilePath.c_str(), "wb" );

	if( !fout )
	{
		r3dOutToLog( "GrassMap::SaveCellData: Couldn't open grass cells file %s for writing!", FilePath.c_str() );
		return false;
	}

	struct AutoClose
	{
		~AutoClose()
		{
			fclose( file );
		}

		FILE *file;
	} autoClose = { fout }; (void)autoClose;

	fwrite( GrazCellData_SIG105, sizeof GrazCellData_SIG105, 1, fout );

	// write type map
	uint32_t typeCount = g_pGrassLib->GetEntryCount();
	fwrite_be( typeCount, fout );

	for( uint32_t i = 0, e = typeCount; i < e; i ++ )
	{
		const char* str = g_pGrassLib->GetEntryNameByIdx( i ).c_str();
		fwrite( str, strlen( str ) + 1, 1, fout );
	}

	// write texture dimmensions
	fwrite_be( CELL_HEIGHT_TEX_DIM, fout );
	fwrite_be( CELL_MASK_TEX_DIM, fout );

	fwrite_be( HEIGHT_TEX_FMT, fout );
	fwrite_be( MASK_TEX_FMT, fout );

	fwrite_be( mCellSize, fout );

	fwrite_be( mSettings.XLength, fout );
	fwrite_be( mSettings.ZLength, fout );

	uint32_t val = mCells.Width();
	fwrite_be( val, fout );

	val = mCells.Height();
	fwrite_be( val, fout );

	val = MAX_TEX_CELL_COUNT;
	fwrite_be( val, fout );

	val = mTextureCells.Width();
	fwrite_be( val, fout );

	val = mTextureCells.Height();
	fwrite_be( val, fout );

	for( uint32_t z = 0, e = mTextureCells.Height(); z < e; z ++ )
	{
		for( uint32_t x = 0, e = mTextureCells.Width(); x < e; x ++ )
		{
			const GrassTextureCell& cell = mTextureCells[ z ][ x ];

			char haveHeightTexture = !!cell.HeightTexture;
			fwrite_be( haveHeightTexture, fout );

			if( haveHeightTexture )
			{
				const GrassCell& gc = mCells[ z * mCellsPerTextureCell ][ x * mCellsPerTextureCell ];

				fwrite_be( gc.Position.y, fout );
				fwrite_be( gc.YMax, fout );

				r3d_assert( cell.MaskTextureEntries.Count() < 256 );
				unsigned char count = (unsigned char)cell.MaskTextureEntries.Count();

				fwrite_be( count, fout );

				for( uint32_t i = 0, e = count; i < e; i ++ )
				{
					unsigned char idx = (unsigned char)cell.MaskTextureEntries[ i ].TypeIdx;
					fwrite_be( idx, fout );

					if( r3dTexture* tex = cell.MaskTextureEntries[i].MaskTexture )
					{
						char val = 1;
						fwrite_be( val, fout );
						SaveTexture( fout, tex, true );
					}
					else
					{
						char val = 0;
						fwrite_be( val, fout );
					}
				}
			}
			else
			{
				r3d_assert( !cell.MaskTextureEntries.Count() );
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------

bool ReadString( r3dString& oString, r3dFile* fin );

//------------------------------------------------------------------------

bool
GrassMap::LoadCellData( const r3dString& FilePath )
{
	r3dFile * fin = r3d_open( FilePath.c_str(), "rb" );

	if( !fin )
	{
		//r3dArtBug( "GrassMap::LoadCellData: Couldn't open grass cells file %s for reading!", FilePath.c_str() );
		return false;
	}

	struct AutoClose
	{
		~AutoClose()
		{
			fclose( file );
		}

		r3dFile *file;
	} autoClose = { fin }; (void)autoClose;

	char compSig[ sizeof GrazCellData_SIG101_2 ];

	if( fread( compSig, sizeof compSig, 1, fin ) != 1 )
		return false;

	if( !strcmp( GrazCellData_SIG105, compSig ) )
	{
		return LoadCellData_104_105( fin, true );
	}
	else
	if( !strcmp( GrazCellData_SIG104, compSig ) )
	{
		return LoadCellData_104_105( fin, false );
	}
	else
	if( !strcmp( GrazCellData_SIG103, compSig ) )
	{
		return LoadCellData_103( fin );
	}
	else
	if( !strcmp( GrazCellData_SIG102, compSig ) )
	{
		return LoadCellData_102( fin );
	}
	else
	{
		if( strcmp( GrazCellData_SIG101, compSig )!=0 )
		{
			if( strcmp( GrazCellData_SIG101_2, compSig )!=0 )
			{
				r3dArtBug( "GrassMap::LoadCellData: file %s has unsuppotrted version( current is %s but got %s )!\n", FilePath.c_str(), GrazCellData_SIG101, compSig );
				return false;
			}
		}
		else
		{
			fseek( fin, (int)sizeof GrazCellData_SIG101 - (int) sizeof GrazCellData_SIG101_2, SEEK_CUR );
		}

		return LoadCellData_101( fin );
	}

	return false;
}

//------------------------------------------------------------------------

bool
GrassMap::LoadCellData_101( r3dFile* fin )
{
	R3D_ENSURE_MAIN_THREAD();

	uint32_t count;

	if( fread_be( count, fin ) != 1 )
		return false;

	TArray< r3dString > cellTypes;

	for( uint32_t i = 0, e = count; i < e; i ++ )
	{
		r3dString type;

		if( !ReadString( type, fin ) )
			return false;

		cellTypes.PushBack( type );
	}

	uint32_t width, height;

	if( fread_be( width, fin ) != 1 )
		return false;

	if( fread_be( height, fin ) != 1 )
		return false;

	if (width < 1) width = 1;
	if (height < 1) height = 1;

	float fWidth = (width - 1) * g_pGrassLib->GetSettings().CellScale;
	float fHeight = (height - 1) * g_pGrassLib->GetSettings().CellScale;
	Init(mSettings.XOffset, mSettings.ZOffset, fWidth, fHeight, MAX_TEX_CELL_COUNT);

	for( uint32_t z = 0, e = mCells.Height(); z < e; z ++ )
	{
		for( uint32_t x = 0, e = mCells.Width(); x < e; x ++ )
		{
			GrassCell& cell = mCells[ z ][ x ];

			cell.Position.x = x * mCellSize + mSettings.XOffset;
			cell.Position.z = z * mCellSize + mSettings.ZOffset;

			if( fread_be( cell.Position.y, fin ) != 1 )
				return false;

			if( fread_be( cell.YMax, fin ) != 1 )
				return false;

			unsigned char count;

			if( fread_be( count, fin ) != 1 )
				return false;

			for( uint32_t i = 0, e = count; i < e; i ++ )
			{
				unsigned char typeIdx;
				if( fread_be( typeIdx, fin ) != 1 )
					return false;

				r3d_assert ( typeIdx < cellTypes.Count() );

				uint32_t realIdx;
				realIdx = g_pGrassLib->GetEntryIdxByName( cellTypes[ typeIdx ] );

				if( realIdx == uint32_t(-1) )
				{
					// this grass type is no longer here... conitnue
					continue;
				}

				GrassCellEntry gce;
				gce.TypeIdx = realIdx;

				cell.Entries.PushBack( gce );
			}
		}
	}

	mLoadVersion = 101;

	return true;	

}

//------------------------------------------------------------------------

bool
GrassMap::LoadCellData_102( r3dFile* fin )
{
	uint32_t count;

	if( fread_be( count, fin ) != 1 )
		return false;

	TArray< r3dString > cellTypes;

	for( uint32_t i = 0, e = count; i < e; i ++ )
	{
		r3dString type;

		if( !ReadString( type, fin ) )
			return false;

		cellTypes.PushBack( type );
	}

	int maskTexDim, heightTexDim;

	if( fread_be( heightTexDim, fin ) != 1 )
		return false;

	if( fread_be( maskTexDim, fin ) != 1 )
		return false;

	uint32_t width, height;

	if( fread_be( width, fin ) != 1 )
		return false;

	if( fread_be( height, fin ) != 1 )
		return false;

	if (width < 1) width = 1;
	if (height < 1) height = 1;

	float fWidth = (width - 1) * g_pGrassLib->GetSettings().CellScale;
	float fHeight = (height - 1) * g_pGrassLib->GetSettings().CellScale;
	Init(mSettings.XOffset, mSettings.ZOffset, fWidth, fHeight, MAX_TEX_CELL_COUNT);

	struct CompoundMaskEntry
	{
		int TypeIdx;
		Bytes Data;
	};

	typedef r3dTL::TArray< CompoundMaskEntry > CompoundMaskArr;

	CompoundMaskArr compoundMasks;

	compoundMasks.Reserve( g_pGrassLib->GetEntryCount() );

	int compoundWidth = width * maskTexDim;
	int compoundHeight = height * maskTexDim;

	bool hasActiveCells = false;

	for( uint32_t z = 0, e = height; z < e; z ++ )
	{
		for( uint32_t x = 0, e = width; x < e; x ++ )
		{
			char haveHeightTex;
			if( fread_be( haveHeightTex, fin ) != 1 )
				return false;

			if( haveHeightTex )
			{
				int texCellX = x / mCellsPerTextureCell;
				int texCellZ = z / mCellsPerTextureCell;

				hasActiveCells = true;

				Bytes heightTexData( heightTexDim * heightTexDim * HEIGHT_TEX_FMT_SIZE );

				if( fread( &heightTexData[0], heightTexDim * heightTexDim * HEIGHT_TEX_FMT_SIZE, 1, fin ) != 1 )
					return false;

				float ypos, ymax ;

				if( fread_be( ypos, fin ) != 1 )
					return false;

				if( fread_be( ymax, fin ) != 1 )
					return false;

				unsigned char count;

				if( fread_be( count, fin ) != 1 )
					return false;

				for( uint32_t i = 0, e = count; i < e; i ++ )
				{
					unsigned char typeIdx;
					if( fread_be( typeIdx, fin ) != 1 )
						return false;

					r3d_assert ( typeIdx < cellTypes.Count() );

					uint32_t realIdx;
					realIdx = g_pGrassLib->GetEntryIdxByName( cellTypes[ typeIdx ] );

					char haveMaskTex;

					if( fread_be( haveMaskTex, fin ) != 1 )
						return false;

					if( realIdx == uint32_t(-1) )
					{
						if( haveMaskTex )
						{
							fseek( fin, maskTexDim * maskTexDim * MASK_TEX_FMT_SIZE, SEEK_CUR );
						}

						// this grass type is no longer here... conitnue
						continue;
					}
					else
					{
						Bytes* compMask = NULL;

						for( int i = 0, e = compoundMasks.Count(); i < e; i ++ )
						{
							if( compoundMasks[ i ].TypeIdx == realIdx )
							{
								compMask = &compoundMasks[ i ].Data;
								break;
							}
						}

						if( !compMask )
						{
							compoundMasks.Resize( compoundMasks.Count() + 1 );

							CompoundMaskEntry& cme = compoundMasks[ compoundMasks.Count() - 1 ];

							cme.Data.Resize( compoundHeight * compoundWidth, 0 );
							cme.TypeIdx = realIdx;

							compMask = &cme.Data;
						}

						if( haveMaskTex )
						{
							Bytes maskTexData( maskTexDim * maskTexDim * MASK_TEX_FMT_SIZE );

							if( fread( &maskTexData[0], maskTexData.Count(), 1, fin ) != 1 )
								return false;

							int tz_offset = z * maskTexDim;
							int tx_offset = x * maskTexDim;

							for( int tz = 0, e = maskTexDim ; tz < e ; tz ++ )
							{
								memcpy( &(*compMask)[ ( tz + tz_offset ) * compoundWidth + tx_offset ], &maskTexData[ tz * maskTexDim ], maskTexDim );
							}
						}
						else
						{
							int tz_offset = z * maskTexDim;
							int tx_offset = x * maskTexDim;

							for( int tz = 0, e = maskTexDim ; tz < e ; tz ++ )
							{
								memset( &(*compMask)[ ( tz + tz_offset ) * compoundWidth + tx_offset ], 0xff, maskTexDim );
							}
						}
					}
				}
			}
		}
	}

	float oldExcessX	= width - mSettings.XLength / mCellSize;
	float oldExcessZ	= height - mSettings.ZLength / mCellSize;

	int newTotalX = mTextureCells.Width() * CELL_MASK_TEX_DIM;
	int newTotalZ = mTextureCells.Height() * CELL_MASK_TEX_DIM;

	float newExcessX	=  mTextureCells.Width() - mSettings.XLength / mCellSize / mCellsPerTextureCell;
	float newExcessZ	=  mTextureCells.Height() - mSettings.ZLength / mCellSize / mCellsPerTextureCell;

	Bytes resizedData( newTotalX * newTotalZ );
	
	for( int i = 0, e = compoundMasks.Count(); i < e; i ++ )
	{
		ResizeTexture(	compoundWidth, compoundHeight, oldExcessX, oldExcessZ, compoundMasks[ i ].Data,
						newTotalX, newTotalZ, newExcessX, newExcessZ, 32, CELL_MASK_TEX_DIM, resizedData );

		compoundMasks[ i ].Data = resizedData;
	}

	Bytes tempTexData( CELL_MASK_TEX_DIM * CELL_MASK_TEX_DIM * MASK_TEX_FMT_SIZE, 0x0 );

	for( int z = 0, e = mTextureCells.Height(); z < e ; z ++ )
	{
		for( int x = 0, e = mTextureCells.Width(); x < e ; x ++ )
		{
			GrassTextureCell& gtc = mTextureCells[ z ][ x ];

			for( int i = 0, e = compoundMasks.Count(); i < e ; i ++ )
			{
				CompoundMaskEntry& cme = compoundMasks[ i ];

				int hasShadesOfGrey = 0;

				for( int tz = z * CELL_MASK_TEX_DIM, e = ( z + 1 ) * CELL_MASK_TEX_DIM ; tz < e && !hasShadesOfGrey ; tz ++ )
				{
					for( int tx = x * CELL_MASK_TEX_DIM, e = ( x + 1 ) * CELL_MASK_TEX_DIM ; tx < e && !hasShadesOfGrey ; tx ++ )
					{
						if( cme.Data[ tx + tz * newTotalX ] )
						{
							hasShadesOfGrey = 1;
						}
					}
				}

				if( hasShadesOfGrey )
				{
					if( !gtc.HeightTexture )
					{
						CreateTextureCellHeightTexture( gtc );
						UpdateTextureCellHeight( x, z, gtc );
					}

					CreateMaskTextureCellEntry( gtc, x, z, cme.TypeIdx, false );

					GrassMaskTextureEntry& gmte = gtc.MaskTextureEntries.GetLast();

					gmte.MaskTexture = r3dRenderer->AllocateTexture();
					gmte.MaskTexture->Create( CELL_MASK_TEX_DIM, CELL_MASK_TEX_DIM, MASK_TEX_FMT, 1 );

					for( int tz = z * CELL_MASK_TEX_DIM, zz = 0, e = ( z + 1 ) * CELL_MASK_TEX_DIM ; tz < e ; tz ++, zz ++ )
					{
						for( int tx = x * CELL_MASK_TEX_DIM, xx = 0, e = ( x + 1 ) * CELL_MASK_TEX_DIM ; tx < e ; tx ++, xx ++ )
						{
							tempTexData[ xx + zz * CELL_MASK_TEX_DIM ] = cme.Data[ tx + tz * newTotalX ];
						}
					}

					FillTexture( gmte.MaskTexture, tempTexData, true );
				}
			}
		}
	}

	if( hasActiveCells && Terrain )
	{
		Terrain->SetOrthoDiffuseTextureDirty() ;
	}

	mLoadVersion = 102;

	return true;
}

//------------------------------------------------------------------------

struct imtparms
{
	int compWidth;
	int compHeigh;

	int idx;

	const char* prefix;

	Bytes* data;
};

void InMainThread( void* p )
{
	imtparms* prms = (imtparms*) p;

	IDirect3DTexture9* tex;
	D3D_V( r3dRenderer->pd3ddev->CreateTexture( prms->compWidth, prms->compHeigh, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &tex, NULL ) );

	D3DLOCKED_RECT lrect;
	D3D_V( tex->LockRect( 0, &lrect, NULL, 0 ) );

	for ( int z = 0, e = prms->compHeigh; z < e ; z ++ )
	{
		for( int x = 0, e = prms->compWidth; x < e ; x ++ )
		{
			((DWORD*)((char*)lrect.pBits + lrect.Pitch * z))[x] = (*prms->data)[ x + z * prms->compWidth ] | 0xff000000;
		}
	}

	D3D_V( tex->UnlockRect( 0 ) );

	char fname[ 512 ];
	sprintf( fname, "__%s%d.bmp", prms->prefix, prms->idx );

	D3D_V( D3DXSaveTextureToFile( fname, D3DXIFF_BMP, tex, NULL ) );

	tex->Release();

}

bool GrassMap::LoadCellData_103( r3dFile* fin )
{
	uint32_t count;

	if( fread_be( count, fin ) != 1 )
		return false;

	TArray< r3dString > cellTypes;

	for( uint32_t i = 0, e = count; i < e; i ++ )
	{
		r3dString type;

		if( !ReadString( type, fin ) )
			return false;

		cellTypes.PushBack( type );
	}

	int maskTexDim, heightTexDim;

	if( fread_be( heightTexDim, fin ) != 1 )
		return false;

	if( fread_be( maskTexDim, fin ) != 1 )
		return false;

	uint32_t width, height;

	if( fread_be( width, fin ) != 1 )
		return false;

	if( fread_be( height, fin ) != 1 )
		return false;

	if (width < 1) width = 1;
	if (height < 1) height = 1;

	float fWidth = (width - 1) * g_pGrassLib->GetSettings().CellScale;
	float fHeight = (height - 1) * g_pGrassLib->GetSettings().CellScale;

	Init(mSettings.XOffset, mSettings.ZOffset, fWidth, fHeight, MAX_TEX_CELL_COUNT);

	bool hasActiveCells = false;

	int cellsInTexCell, fileTexCountX, fileTexCountZ;

	GetTextureCellsBasedOnNormalCells( width, height, 48, &cellsInTexCell, &fileTexCountX, &fileTexCountZ );

	struct CompoundMaskEntry
	{
		int TypeIdx;
		Bytes Data;
	};

	typedef r3dTL::TArray< CompoundMaskEntry > CompoundMaskArr;

	CompoundMaskArr compoundMasks;

	compoundMasks.Reserve( g_pGrassLib->GetEntryCount() );

	int compoundWidth = fileTexCountZ * maskTexDim;
	int compoundHeight = fileTexCountZ * maskTexDim;

	Bytes heightTexData( heightTexDim * heightTexDim * HEIGHT_TEX_FMT_SIZE );

	for( uint32_t z = 0, e = fileTexCountZ; z < e; z ++ )
	{
		for( uint32_t x = 0, e = fileTexCountX; x < e; x ++ )
		{
			char haveHeightTex;
			if( fread_be( haveHeightTex, fin ) != 1 )
				return false;

			if( haveHeightTex )
			{
				if( fread( &heightTexData[0], heightTexDim * heightTexDim * HEIGHT_TEX_FMT_SIZE, 1, fin ) != 1 )
					return false;

				float ypos, ymax;

				if( fread_be( ypos, fin ) != 1 )
					return false;

				if( fread_be( ymax, fin ) != 1 )
					return false;

				unsigned char count;

				if( fread_be( count, fin ) != 1 )
					return false;

				for( uint32_t i = 0, e = count; i < e; i ++ )
				{
					unsigned char typeIdx;
					if( fread_be( typeIdx, fin ) != 1 )
						return false;

					r3d_assert ( typeIdx < cellTypes.Count() );

					uint32_t realIdx;
					realIdx = g_pGrassLib->GetEntryIdxByName( cellTypes[ typeIdx ] );

					char haveMaskTex;

					if( fread_be( haveMaskTex, fin ) != 1 )
						return false;

					if( realIdx == uint32_t(-1) )
					{
						if( haveMaskTex )
						{
							fseek( fin, maskTexDim * maskTexDim * MASK_TEX_FMT_SIZE, SEEK_CUR );
						}

						// this grass type is no longer here... conitnue
						continue;
					}
					else
					{
						Bytes* compMask = NULL;

						for( int i = 0, e = compoundMasks.Count(); i < e; i ++ )
						{
							if( compoundMasks[ i ].TypeIdx == realIdx )
							{
								compMask = &compoundMasks[ i ].Data;
								break;
							}
						}

						if( !compMask )
						{
							compoundMasks.Resize( compoundMasks.Count() + 1 );

							CompoundMaskEntry& cme = compoundMasks[ compoundMasks.Count() - 1 ];

							cme.Data.Resize( compoundHeight * compoundWidth, 0 );
							cme.TypeIdx = realIdx;

							compMask = &cme.Data;
						}

						if( haveMaskTex )
						{
							Bytes maskTexData( maskTexDim * maskTexDim * MASK_TEX_FMT_SIZE );

							if( fread( &maskTexData[0], maskTexData.Count(), 1, fin ) != 1 )
								return false;

							int tz_offset = z * maskTexDim;
							int tx_offset = x * maskTexDim;

							for( int tz = 0, e = maskTexDim ; tz < e ; tz ++ )
							{
								memcpy( &(*compMask)[ ( tz + tz_offset ) * compoundWidth + tx_offset ], &maskTexData[ tz * maskTexDim ], maskTexDim );
							}
						}
						else
						{
							int tz_offset = z * maskTexDim;
							int tx_offset = x * maskTexDim;

							for( int tz = 0, e = maskTexDim ; tz < e ; tz ++ )
							{
								memset( &(*compMask)[ ( tz + tz_offset ) * compoundWidth + tx_offset ], 0xff, maskTexDim );
							}
						}
					}					
				}
			}
		}
	}

	float oldExcessX	= fileTexCountX - mSettings.XLength / mCellSize / cellsInTexCell;
	float oldExcessZ	= fileTexCountZ - mSettings.ZLength / mCellSize / cellsInTexCell;

	int newTotalX = mTextureCells.Width() * CELL_MASK_TEX_DIM;
	int newTotalZ = mTextureCells.Height() * CELL_MASK_TEX_DIM;

	float newExcessX	=  mTextureCells.Width() - mSettings.XLength / mCellSize / mCellsPerTextureCell;
	float newExcessZ	=  mTextureCells.Height() - mSettings.ZLength / mCellSize / mCellsPerTextureCell;

	Bytes resizedData( newTotalX * newTotalZ );

	for( int i = 0, e = compoundMasks.Count(); i < e; i ++ )
	{
#if 0
		imtparms prms;

		prms.compHeigh = compoundHeight;
		prms.compWidth = compoundWidth;
		prms.data = &compoundMasks[ i ].Data;
		prms.prefix = "pre";
		prms.idx = i;

		ProcessCustomDeviceQueueItem( InMainThread, &prms );
#endif

		ResizeTexture(	compoundWidth, compoundHeight, oldExcessX, oldExcessZ, compoundMasks[ i ].Data,
						newTotalX, newTotalZ, newExcessX, newExcessZ, 32, CELL_MASK_TEX_DIM, resizedData );

#if 0
		prms.compHeigh = newTotalZ;
		prms.compWidth = newTotalX;
		prms.data = &resizedData;
		prms.prefix = "post";
		prms.idx = i;

		ProcessCustomDeviceQueueItem( InMainThread, &prms );
#endif

		compoundMasks[ i ].Data = resizedData;
	}

	Bytes tempTexData( CELL_MASK_TEX_DIM * CELL_MASK_TEX_DIM * MASK_TEX_FMT_SIZE, 0x0 );

	for( int z = 0, e = mTextureCells.Height(); z < e ; z ++ )
	{
		for( int x = 0, e = mTextureCells.Width(); x < e ; x ++ )
		{
			GrassTextureCell& gtc = mTextureCells[ z ][ x ];

			for( int i = 0, e = compoundMasks.Count(); i < e ; i ++ )
			{
				CompoundMaskEntry& cme = compoundMasks[ i ];

				int hasShadesOfGrey = 0;

				for( int tz = z * CELL_MASK_TEX_DIM, e = ( z + 1 ) * CELL_MASK_TEX_DIM ; tz < e && !hasShadesOfGrey ; tz ++ )
				{
					for( int tx = x * CELL_MASK_TEX_DIM, e = ( x + 1 ) * CELL_MASK_TEX_DIM ; tx < e && !hasShadesOfGrey ; tx ++ )
					{
						if( cme.Data[ tx + tz * newTotalX ] )
						{
							hasShadesOfGrey = 1;
						}
					}
				}

				if( hasShadesOfGrey )
				{
					if( !gtc.HeightTexture )
					{
						CreateTextureCellHeightTexture( gtc );
						UpdateTextureCellHeight( x, z, gtc );
					}

					CreateMaskTextureCellEntry( gtc, x, z, cme.TypeIdx, false );

					GrassMaskTextureEntry& gmte = gtc.MaskTextureEntries.GetLast();

					gmte.MaskTexture = r3dRenderer->AllocateTexture();
					gmte.MaskTexture->Create( CELL_MASK_TEX_DIM, CELL_MASK_TEX_DIM, MASK_TEX_FMT, 1 );

					for( int tz = z * CELL_MASK_TEX_DIM, zz = 0, e = ( z + 1 ) * CELL_MASK_TEX_DIM ; tz < e ; tz ++, zz ++ )
					{
						for( int tx = x * CELL_MASK_TEX_DIM, xx = 0, e = ( x + 1 ) * CELL_MASK_TEX_DIM ; tx < e ; tx ++, xx ++ )
						{
							tempTexData[ xx + zz * CELL_MASK_TEX_DIM ] = cme.Data[ tx + tz * newTotalX ];
						}
					}

					FillTexture( gmte.MaskTexture, tempTexData, true );
				}
			}
		}
	}	

	if( hasActiveCells && Terrain )
	{
		Terrain->SetOrthoDiffuseTextureDirty();
	}

	mLoadVersion = 103;

	return true;
}

//------------------------------------------------------------------------

bool GrassMap::LoadCellData_104_105( r3dFile* fin, bool is105 )
{
	uint32_t count;

	if( fread_be( count, fin ) != 1 )
		return false;

	TArray< r3dString > cellTypes;

	for( uint32_t i = 0, e = count; i < e; i ++ )
	{
		r3dString type;

		if( !ReadString( type, fin ) )
			return false;

		cellTypes.PushBack( type );
	}

	int maskTexDim, heightTexDim;

	if( fread_be( heightTexDim, fin ) != 1 )
		return false;

	if( fread_be( maskTexDim, fin ) != 1 )
		return false;

	D3DFORMAT heightFmt = D3DFMT_L8, maskFmt = D3DFMT_L8;

	if( is105 )
	{
		if( fread_be( heightFmt, fin ) != 1 )
			return false;

		if( fread_be( maskFmt, fin ) != 1 )
			return false;
	}

	float fCellSize;
	float fWidth;
	float fHeight;

	if( fread_be( fCellSize, fin ) != 1 )
		return false;

	r3d_assert( fCellSize == g_pGrassLib->GetSettings().CellScale );

	if( fread_be( fWidth, fin ) != 1 )
		return false;

	if( fread_be( fHeight, fin ) != 1 )
		return false;

	uint32_t width, height;

	if( fread_be( width, fin ) != 1 )
		return false;

	if( fread_be( height, fin ) != 1 )
		return false;

	if (width < 1) width = 1;
	if (height < 1) height = 1;

	int maxTexCellCount;

	if( fread_be( maxTexCellCount, fin ) != 1 )
		return false;

	int texCellsX, texCellsZ;

	if( fread_be( texCellsX, fin ) != 1 )
		return false;

	if( fread_be( texCellsZ, fin ) != 1 )
		return false;

	Init(mSettings.XOffset, mSettings.ZOffset, fWidth, fHeight, MAX_TEX_CELL_COUNT);

	// if this fails need to append resize code
	r3d_assert( texCellsX == mTextureCells.Width() 
					&&
				texCellsZ == mTextureCells.Height() );

	bool hasActiveCells = false;

	Bytes heightTexData;
	
	if( !is105 )
		heightTexData.Resize( heightTexDim * heightTexDim * 1 );

	for( uint32_t z = 0, e = mTextureCells.Height(); z < e; z ++ )
	{
		for( uint32_t x = 0, e = mTextureCells.Width(); x < e; x ++ )
		{
			GrassTextureCell& cell = mTextureCells[ z ][ x ];

			char haveHeightTex;
			if( fread_be( haveHeightTex, fin ) != 1 )
				return false;

			if( haveHeightTex )
			{
				CreateTextureCellHeightTexture( cell );

				hasActiveCells = true;

				if( !is105 )
				{
					if( fread( &heightTexData[0], heightTexDim * heightTexDim * 1, 1, fin ) != 1 )
						return false;
				}

				if( is105 || heightTexDim != CELL_HEIGHT_TEX_DIM || heightFmt != HEIGHT_TEX_FMT )
				{
					UpdateTextureCellHeight( x, z, cell );
				}
				else
				{
					FillTexture( cell.HeightTexture, heightTexData, false );
				}

				float ypos, ymax;

				if( fread_be( ypos, fin ) != 1 )
					return false;

				if( fread_be( ymax, fin ) != 1 )
					return false;

				for( int cz = z * mCellsPerTextureCell, e = ( z + 1 ) * mCellsPerTextureCell; cz < e; cz ++ )
				{
					for( int cx = x * mCellsPerTextureCell, e = ( x + 1 ) * mCellsPerTextureCell; cx < e; cx ++ )
					{
						mCells[ cz ][ cx ].Position.y = ypos;
						mCells[ cz ][ cx ].YMax = ymax;
					}
				}

				unsigned char count;

				if( fread_be( count, fin ) != 1 )
					return false;

				for( uint32_t i = 0, e = count; i < e; i ++ )
				{
					unsigned char typeIdx;
					if( fread_be( typeIdx, fin ) != 1 )
						return false;

					r3d_assert ( typeIdx < cellTypes.Count() );

					uint32_t realIdx;
					realIdx = g_pGrassLib->GetEntryIdxByName( cellTypes[ typeIdx ] );

					char haveMaskTex;

					if( fread_be( haveMaskTex, fin ) != 1 )
						return false;

					if( realIdx == uint32_t(-1) )
					{
						if( haveMaskTex )
						{
							fseek( fin, maskTexDim * maskTexDim * MASK_TEX_FMT_SIZE, SEEK_CUR );
						}

						// this grass type is no longer here... conitnue
						continue;
					}
					else
					{
						CreateMaskTextureCellEntry( cell, x, z, realIdx, false );

						GrassMaskTextureEntry& gmte = cell.MaskTextureEntries.GetLast();

						if( haveMaskTex )
						{
							Bytes maskTexData( maskTexDim * maskTexDim * MASK_TEX_FMT_SIZE );

							if( fread( &maskTexData[0], maskTexData.Count(), 1, fin ) != 1 )
								return false;
							if( maskTexDim != CELL_MASK_TEX_DIM )
							{
								Bytes newData( CELL_MASK_TEX_DIM * CELL_MASK_TEX_DIM * MASK_TEX_FMT_SIZE );
								ResizeTexture( maskTexDim, maskTexDim, 0.f, 0.f, maskTexData, CELL_MASK_TEX_DIM, CELL_MASK_TEX_DIM, 0.f, 0.f, CELL_MASK_TEX_DIM, CELL_MASK_TEX_DIM, newData );

								maskTexData.Swap( newData );
							}

							gmte.MaskTexture = r3dRenderer->AllocateTexture();
							gmte.MaskTexture->Create( CELL_MASK_TEX_DIM, CELL_MASK_TEX_DIM, MASK_TEX_FMT, 1 );
							FillTexture( gmte.MaskTexture, maskTexData, true );
						}
					}					
				}
			}
		}
	}

	if( hasActiveCells && Terrain )
	{
		Terrain->SetOrthoDiffuseTextureDirty();
	}

	mLoadVersion = is105 ? 105 : 104;

	return true;
}

//------------------------------------------------------------------------

void GrassMap::CreateTextureCellHeightTexture( GrassTextureCell& cell )
{
	cell.HeightTexture = r3dRenderer->AllocateTexture();
	cell.HeightTexture->Create( CELL_HEIGHT_TEX_DIM, CELL_HEIGHT_TEX_DIM, HEIGHT_TEX_FMT, 1 );
}

//------------------------------------------------------------------------

void GrassMap::DeleteMaskEntriesInUnderlyingCells( int typeIdx, int textureCellX, int textureCellZ )
{
	for( int z = textureCellZ * mCellsPerTextureCell, e = (textureCellZ + 1) * mCellsPerTextureCell ; z < e ; z ++ )
	{
		for( int x = textureCellX * mCellsPerTextureCell, e = (textureCellX + 1) * mCellsPerTextureCell ; x < e ; x ++ )
		{
			if( x < 0 || x >= (int)mCells.Width() 
				||
				z < 0 || z >= (int)mCells.Height()
				)
				continue;

			GrassCell& gc = mCells[ z ][ x ];

			for( int i = 0, e = gc.Entries.Count(); i < e ; )
			{
				GrassCellEntry& gce = gc.Entries[ i ];

				if( typeIdx == gce.TypeIdx )
				{
					gc.Entries.Erase( i );
					e --;
				}

				i ++;
			}
		}
	}
}

//------------------------------------------------------------------------

void GrassMap::UpdateYExtents()
{
	mSettings.YMax = -FLT_MAX;
	mSettings.YMin = FLT_MAX;

	float maxMeshScale = g_pGrassLib->GetSettings().MaxMeshScale * 0.25f;

	for( int z = 0, ze = mCells.Height(); z < ze; z ++ )
	{
		for( int x = 0, xe = mCells.Width(); x < xe; x ++ )
		{
			const GrassCell& cell = mCells[ z ][ x ];

			if( !cell.Entries.Count() )
				continue;

			mSettings.YMax = R3D_MAX( cell.YMax + maxMeshScale, mSettings.YMax );
			mSettings.YMin = R3D_MIN( cell.Position.y, mSettings.YMin );
		}
	}

	if( mSettings.YMin > mSettings.YMax )
	{
		mSettings.YMin = 0;
		mSettings.YMax = 0;

		if( Terrain )
		{
			mSettings.YMin = Terrain->GetDesc().MinHeight;
			mSettings.YMax = mSettings.YMin;
		}
		else
		{
#ifndef FINAL_BUILD
			r3dBoundBox bbox;
			GetGrassPlaneManager()->GetCombinedPlaneBounds( &bbox );

			mSettings.YMin = bbox.Org.y;
			mSettings.YMax = bbox.Org.y + bbox.Size.y;
#endif
		}
	}
}

//------------------------------------------------------------------------

float GrassMap::GetCellSize()
{
	if (mCellSize == 0)
		mCellSize = g_pGrassLib->GetSettings().CellScale;
	return mCellSize;
}

//------------------------------------------------------------------------

float GrassMap::GetTextureCellSize()
{
	return GetCellSize() * mCellsPerTextureCell;
}

//------------------------------------------------------------------------

const GrassMap::Settings&
GrassMap::GetSettings() const
{
	return mSettings;
}

//------------------------------------------------------------------------

GrassMap gGrassMap;

//------------------------------------------------------------------------

r3dString GetGrassMapTexPath( const r3dString& LevelPath )
{
	r3dString res = LevelPath;
	res += AR_GRAZ_PAZ "\\";

	return res;
}

//------------------------------------------------------------------------

r3dString GetGrassMapHeightTexName( int CellX, int CellZ )
{
	char Name[ 128 ];
	sprintf( Name, "height_%d_%d.dds", CellX, CellZ );

	return Name;
}

//------------------------------------------------------------------------

r3dString GetGrassMapMaskTexName( const r3dString& Type, int CellX, int CellZ )
{
	char drive[ 8 ], folder[ 256 ], fname[ 64 ], ext[ 32 ];
	_splitpath( Type.c_str(), drive, folder, fname, ext );

	char Name[ 128 ];
	sprintf( Name, "mask_%s_%d_%d.dds", fname, CellX, CellZ );

	return Name;
}

//------------------------------------------------------------------------

void InitGrass()
{
	r3d_assert(g_pGrassMap == 0);
	g_pGrassMap = new GrassMap();
	r3d_assert(g_pGrassLib == 0);
	g_pGrassLib = new GrassLib();
	r3d_assert(g_pGrassGen == 0);
	g_pGrassGen = new GrassGen();

	D3DVERTEXELEMENT9 DeclElems[] = 
	{
		{0,  0, D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0,  8, D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};

	( r3dDeviceTunnel::CreateVertexDeclaration( DeclElems, &GrassChunk::VDecl ) );

	P0_PIXEL_SHADER_ID		= r3dRenderer->GetPixelShaderIdx( "PS_GRASS_P0" );

	g_pGrassGen->Load();

	if( g_pGrassGen->IsNewerThan( g_pGrassLib->GetStamp() ) )
	{
		r3dOutToLog( "Generating grass.. \n" );

		float timeStart = r3dGetTime();

		g_pGrassGen->GenerateAll();
		g_pGrassLib->Save();

		r3dOutToLog( "Generated grass for %f\n", r3dGetTime() - timeStart );

		g_pGrassLib->Unload() ;
	}
}

//------------------------------------------------------------------------

void LoadGrassLib()
{
	r3dOutToLog( "LoadGrassLib()...\n" );
	if( !g_pGrassLib->Load() )
	{
#ifndef FINAL_BUILD
		r3dOutToLog( "LoadGrassLib(): couldn't load grass lib. Regenerating." ) ;
		g_pGrassGen->GenerateAll() ;
		g_pGrassLib->Save() ;
#endif
	}
}

//------------------------------------------------------------------------

void UnloadGrassLib()
{
	g_pGrassLib->Unload();
}

//------------------------------------------------------------------------

void AnimateGrass()
{
	clock_t newClock = clock();

	clock_t delta = newClock - PrevClock;

	PrevClock = newClock;

	Time += 3.f * delta / CLOCKS_PER_SEC;

	Time = fmodf( Time, R3D_PI * 2 );
}

//------------------------------------------------------------------------

void PrepareGrass()
{
	if( r_grass_draw->GetBool() && r3dRenderer->SupportsVertexTextureFetch )
	{
		g_pGrassMap->Prepare() ;
	}
}

//------------------------------------------------------------------------

void DrawGrass( GrassMap::Path path, bool UseDepthEqual, bool drawToAux )
{
	if( r_grass_draw->GetBool() && r3dRenderer->SupportsVertexTextureFetch )
	{
		g_pGrassMap->Draw( gCam, path, UseDepthEqual, drawToAux );
	}
}

//------------------------------------------------------------------------

void CloseGrass()
{
	SAFE_DELETE(g_pGrassGen);
	SAFE_DELETE(g_pGrassLib);
	SAFE_DELETE(g_pGrassMap);
}

//------------------------------------------------------------------------

float GetGrassHeight(const r3dPoint3D &pt)
{
	float th = -FLT_MAX;
	float ph = -FLT_MAX;
	if (Terrain)
		th = Terrain->GetHeight ( pt );
#ifndef FINAL_BUILD
	if (g_GrassPlanesManager)
		ph = g_GrassPlanesManager->GetHeight(pt.x, pt.z);
#endif
	return std::max(th, ph);
}

//------------------------------------------------------------------------

namespace
{
	float lerp( float a, float b, float t )
	{
		return a + ( b - a ) * t;
	}

	float GetBiliniear( const Bytes& bytes, uint32_t totalXLen, uint32_t totalZLen, float x, float z )
	{

		x = R3D_MIN( R3D_MAX( x, 0.f ), (float) totalXLen - 1.f );
		z = R3D_MIN( R3D_MAX( z, 0.f ), (float) totalZLen - 1.f );

		int x0 = (int)x;
		int z0 = (int)z;

		int x1 = R3D_MIN( x0 + 1, (int)totalXLen - 1 );
		int z1 = R3D_MIN( z0 + 1, (int)totalZLen - 1 );

		float s00 = (float)bytes[ x0 + z0 * totalXLen ];
		float s10 = (float)bytes[ x1 + z0 * totalXLen ];
		float s01 = (float)bytes[ x0 + z1 * totalXLen ];
		float s11 = (float)bytes[ x1 + z1 * totalXLen ];

		float tx = x - (float)x0;
		float tz = z - (float)z0;

		return 		lerp( 
						lerp( s00, s10, tx ),
							lerp( s01, s11, tx ),
								tz
									);
	}

	void ResizeTexture(	uint32_t oldTotalXLen, uint32_t oldTotalZLen, float oldExcessX, float oldExcessZ, const Bytes& oldCompoundTex,
						uint32_t newTotalXLen, uint32_t newTotalZLen, float newExcessX, float newExcessZ, int OLD_TEX_DIM, int NEW_TEX_DIM, Bytes& newCompoundTex )
	{
		uint32_t oldEffectiveXLen = oldTotalXLen - uint32_t( oldExcessX * OLD_TEX_DIM );
		uint32_t oldEffectiveZLen = oldTotalZLen - uint32_t( oldExcessZ * OLD_TEX_DIM );

		uint32_t newEffectiveXLen = newTotalXLen - uint32_t( newExcessX * NEW_TEX_DIM );
		uint32_t newEffectiveZLen = newTotalZLen - uint32_t( newExcessZ * NEW_TEX_DIM );

		float kx = (float)oldEffectiveXLen / newEffectiveXLen;
		float kz = (float)oldEffectiveZLen / newEffectiveZLen;

		for( uint32_t x = 0, e = newEffectiveXLen; x < e; x ++ )
		{
			for( uint32_t z = 0, e = newEffectiveZLen; z < e; z ++ )
			{
				float ox0 = x * kx;
				float oz0 = z * kz;

				float ox1 = ( x + 1 ) * kx;
				float oz1 = ( z + 1 ) * kz;

				float sum = 0;

				float count = 0;

				for( float fx = ox0; fx < ox1; fx += 0.5f )
				{
					for( float fz = oz0; fz < oz1; fz += 0.5f )
					{
						sum		+= GetBiliniear( oldCompoundTex, oldTotalXLen, oldTotalZLen, fx, fz );
						count	+= 1.0f;
					}
				}

				sum /= count;
				newCompoundTex[ x + z * newTotalXLen ] = R3D_MIN( R3D_MAX( (int)sum, 0 ), (int)255 );
			}
		}
	}

	void SaveTexture( FILE* fout, r3dTexture* tex, bool isMask )
	{
		UINT fmtSize = isMask ? MASK_TEX_FMT_SIZE : HEIGHT_TEX_FMT_SIZE;

		D3DLOCKED_RECT lrect;

		D3D_V( tex->AsTex2D()->LockRect( 0, &lrect, NULL, D3DLOCK_READONLY ) );

		char* p = (char*)lrect.pBits;

		for( UINT i = 0, e = tex->GetHeight(); i < e; i ++ )
		{
			fwrite( p, tex->GetWidth() * fmtSize, 1, fout );
			p += lrect.Pitch;
		}

		D3D_V( tex->AsTex2D()->UnlockRect( 0 ) );
	}

	void FillTexture( r3dTexture* tex, const Bytes& data, bool isMask )
	{
		r3d_assert( data.Count() == tex->GetWidth() * tex->GetHeight() );

		r3dD3DTextureTunnel d3dtex = tex->GetD3DTunnel();

		D3DLOCKED_RECT lrect;
		d3dtex.LockRect( 0, &lrect, NULL, 0 );

		unsigned char* p = (unsigned char*)lrect.pBits;

		int idx = 0;

		int fmtSize = isMask ? MASK_TEX_FMT_SIZE : HEIGHT_TEX_FMT_SIZE;

		for( uint32_t z = 0, e = tex->GetHeight(); z < e; z ++ )
		{
			for( uint32_t x = 0, e = tex->GetWidth() * fmtSize; x < e; x ++ )
			{
				*p++ = data[ idx ++ ];
			}

			p += lrect.Pitch - tex->GetWidth() * fmtSize;
		}

		d3dtex.UnlockRect( 0 );

	}
}

//------------------------------------------------------------------------


