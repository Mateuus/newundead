#include "r3dPCH.h"

#include "r3d.h"

#include "../SF/RenderBuffer.h"

#include "GameObjects/ObjManag.h"
#include "ObjectsCode/world/Lamp.H"

#include "TrueNature/Sun.h"

#include "TrueNature/ITerrain.h"

#include "Editors/ObjectManipulator3d.h"

#include "ProbeMaster.h"

#if R3D_ALLOW_LIGHT_PROBES

IMPLEMENT_CLASS( ProbeProxy, "ProbeProxy", "Object" );
AUTOREGISTER_CLASS( ProbeProxy );

#pragma warning(disable:4996)

extern r3dCamera gCam;

static const float PROBE_DISPLAY_SCALE = 0.33f;

static const int PROBE_RT_DIM = 64;
static const int BOUNCE_RT_DIM = 256;
static const int NORMAL_TO_SH_DIM = 64;

static const D3DFORMAT PROBE_RT_FMT = D3DFMT_R5G6B5;

typedef ProbeMaster::Bytes Bytes;

static const int N_BANDS = 2;
static const int N_COEFS = 4;

#pragma pack(push,1)
struct SHProjectVertex
{
	float4 coefs;
	float3 vec;
	float2 texc;
};
#pragma pack(pop)

extern bool g_bEditMode;

static void SetupCamForFace( r3dCamera* oCam, const r3dPoint3D& pos, int f );
static void CopyPixels( Bytes* oBytes, void* src, int face );
static void CopyPixelsRGB( Bytes* oBytesR, Bytes* oBytesG, Bytes* oBytesB, void* src, int face );
static void ProjectOnSH( float (&oSH)[N_COEFS], float (&fn)( float, float ) );
static void SH_setup_spherical_samples();
static void SH_free_spherical_samples();

static float Integrate( float (&fn)( float, float ), int dirId );

static Bytes* g_SampleSphericalSource;
static float SampleSpherical( float th, float ph );

static D3DXVECTOR3 g_HemisphereNormal;
static float SampleHemisphereLighting( float th, float ph );

template< typename T >
T ConvertProbeRasterSamplesTo( const ProbeRasterSamples& samples, int idir );

static int g_AccumVS_ID = -1;
static int g_AccumPS_ID = -1;
static int g_AccumVPLSH_VS_ID = -1;
static int g_AccumVPLSH_PS_ID = -1;

float DefaultSHBasis[ 4 ][ 4 ] = { 
	{ 0.44514418f, 0.0049388288f, 0.51238781f, 0.014295869f		},
	{ 0.15012246f, 0.0015635064f, 0.17287119f, 0.0055737426f	},
	{ 0.15016453f, 0.0020139161f, 0.17288868f, 0.0047992305f	},
	{ 0.15017986f, 0.0016592280f, 0.17289022f, 0.0053043431f	}
};

struct SHSample
{ 
	float3 sph;
	float3 vec;
	float coeff[ N_COEFS ];

	int faceIdx;
};

static const int SQRT_N_SAMPLES = 128;
static const int N_SAMPLES = SQRT_N_SAMPLES * SQRT_N_SAMPLES;

typedef r3dTL::TArray< SHSample > SHSampleArr;
typedef r3dTL::TArray< float2 > Float2Arr;
typedef r3dTL::TFixedArray< Float2Arr, Probe::NUM_DIRS > DirIntegraArr;
static SHSampleArr g_SHSamples;
static DirIntegraArr g_DirIntegrateSamples;

//------------------------------------------------------------------------

struct DistanceProbeIdxEntry
{
	float distance;
	ProbeIdx probeIdx;
};

static r3dTL::TArray< DistanceProbeIdxEntry > g_ProbesIdxesWithDistances;

struct DistanceProbeEntry
{
	float distance;
	Probe* probe;
};

static r3dTL::TArray< DistanceProbeEntry > g_ProbesWithDistances;

enum
{
	VPL_GATHER_TILE_RANGE = 2
};

//------------------------------------------------------------------------

ProbeProxy::ProbeProxy()
{
	setSkipOcclusionCheck(true);

	Idx.CombinedIdx = -1;

	r3dBoundBox bbox = CreateBB();

	SetBBoxLocal( bbox );
}

//------------------------------------------------------------------------

BOOL ProbeProxy::OnPositionChanged()
{
	GameObject::OnPositionChanged();
	SetPosition( GetPosition() );

	return TRUE;
}

//------------------------------------------------------------------------

void ProbeProxy::SetPosition( const r3dPoint3D& pos )
{
	GameObject::SetPosition( pos );

	if( Probe* pr = g_pProbeMaster->GetProbe( Idx ) )
	{
		Idx = g_pProbeMaster->MoveProbe( Idx, pos ) ;
	}
}

//------------------------------------------------------------------------

BOOL ProbeProxy::GetObjStat ( char * sStr, int iLen )
{
	char sAddStr [1024];
	sAddStr[0] = 0;
	if (GameObject::GetObjStat (sStr, iLen))
		r3dscpy(sAddStr, sStr);

	if( Idx.CombinedIdx != -1 )
	{
		const Probe* probe = g_pProbeMaster->GetProbe( Idx );

		sprintf_s( sStr, iLen, "%sCell:%d %d %d\n", sAddStr, probe->CellX, probe->CellY, probe->CellZ );
	}
	else
	{
		sprintf_s( sStr, iLen, "%sInvalid probe\n" );
	}

	return TRUE;
}

//------------------------------------------------------------------------

void
ProbeProxy::SetIdx( ProbeIdx idx )
{
	Idx = idx;

	if( Probe* pr = g_pProbeMaster->GetProbe( idx ) )
	{
		GameObject::SetPosition( pr->Position );
	}
}

//------------------------------------------------------------------------

r3dBoundBox ProbeProxy::CreateBB()
{
	r3dBoundBox bbox;

	bbox.Org = r3dPoint3D( -PROBE_DISPLAY_SCALE, -PROBE_DISPLAY_SCALE, -PROBE_DISPLAY_SCALE );
	bbox.Size = 2.f * r3dPoint3D( PROBE_DISPLAY_SCALE, PROBE_DISPLAY_SCALE, PROBE_DISPLAY_SCALE );

	return bbox;
}

//------------------------------------------------------------------------

struct ProbeProxyRenderable : Renderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		ProbeProxyRenderable *This = static_cast<ProbeProxyRenderable*>( RThis );

		This->Parent->DoDrawComposite( Cam );
	}

	ProbeProxy*	Parent;	
};

/*virtual*/ void ProbeProxy::AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam ) /*OVERRIDE*/
{
	if( r_hide_icons->GetInt() )
		return;

	ProbeProxyRenderable rend;

	rend.Init();
	rend.Parent		= this;

	rend.SortValue	= 7 * RENDERABLE_USER_SORT_VALUE;

	render_arrays[ rsDrawDebugData ].PushBack( rend );
}

//------------------------------------------------------------------------

void ProbeProxy::DoDrawComposite( const r3dCamera& cam )
{
	if( Idx.CombinedIdx != -1 )
	{
		if( Probe* probe = g_pProbeMaster->GetProbe( Idx ) )
		{
			r3dPoint3D pos0;
			r3dPoint3D pos1;

			g_pProbeMaster->ConvertCellToCoord( probe->CellX + 0, probe->CellY + 0, probe->CellZ + 0, &pos0 );
			g_pProbeMaster->ConvertCellToCoord( probe->CellX + 1, probe->CellY + 1, probe->CellZ + 1, &pos1 );

			r3dBoundBox box;

			box.Org = pos0;
			box.Size = pos1 - pos0;

			r3dDrawUniformBoundBox( box, cam, r3dColor::green );

			int currCellX, currCellY, currCellZ;

			g_pProbeMaster->ConvertCoordToCell( &currCellX, &currCellY, &currCellZ, GetPosition() );

			if( currCellX != probe->CellX 
					||
				currCellY != probe->CellY
					||
				currCellZ != probe->CellZ
				)
			{
				g_pProbeMaster->ConvertCellToCoord( currCellX + 0, currCellY + 0, currCellZ + 0, &pos0 );
				g_pProbeMaster->ConvertCellToCoord( currCellX + 1, currCellY + 1, currCellZ + 1, &pos1 );

				box.Org = pos0;
				box.Size = pos1 - pos0;

				r3dDrawUniformBoundBox( box, cam, r3dColor::red );
			}
		}
	}
}

//------------------------------------------------------------------------

const float Probe::DIR_FOV = float( M_PI / 3.0 );

//------------------------------------------------------------------------

Probe::FixedDirArr Probe::ViewDirs;

//------------------------------------------------------------------------

Probe::FixedDirArr Probe::UpVecs;

//------------------------------------------------------------------------

Probe::Probe()
: Position ( 0, 0, 0 )
, Flags( FLAG_SKY_DIRTY | FLAG_BOUNCE_DIRTY | FLAG_LOCAL_BOUNCE_DIRTY )
, XRadius( 0 )
, YRadius( 0 )
, ZRadius( 0 )
, CellX( 0 )
, CellY( 0 )
, CellZ( 0 )
{
	for( int i = 0 , e  = Probe::NUM_DIRS; i < e; i ++ )
	{
		SH_BounceR [ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
		SH_BounceG [ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
		SH_BounceB [ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
	}

	SkyVisibility[ 0 ] = 0.f;

	for( int i = 1, e = SkyVisibility.COUNT; i < e; i ++ )
	{
		SkyVisibility[ i ] = 1.f;
	}

	DynamicLightsRGB = D3DXVECTOR3( 0, 0, 0 );

	for( int i = 0, e = SH_LocalVPLProbeIndexes.COUNT; i < e; i ++ )
	{
		SH_LocalVPLProbeIndexes[ i ].CombinedIdx = -1;

		SH_LocalVPLsR[ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
		SH_LocalVPLsG[ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
		SH_LocalVPLsB[ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
	}
}

//------------------------------------------------------------------------

int CountProbeLocalVPLs( const Probe* p )
{
	int count = 0;

	for( int i = 0, e = p->SH_LocalVPLProbeIndexes.COUNT; i < e; i ++ )
	{
		if( p->SH_LocalVPLProbeIndexes[ i ].CombinedIdx == -1 )
			break;
		
		count ++;
	}

	return count;
}

//------------------------------------------------------------------------

ProbeMaster::ProbeTile::ProbeTile()
{
	BBox.Org = r3dPoint3D( 0, 0, 0 );
	BBox.Size = r3dPoint3D( 1, 1, 1 );
}

//------------------------------------------------------------------------

ProbeMaster::Settings::Settings()
: ProbeTextureWidth( 64 )
, ProbeTextureHeight( 64 )
, ProbeTextureDepth( 8 )

, ProbeTextureSpanX( 256.f )
, ProbeTextureSpanY( 32.f )
, ProbeTextureSpanZ( 256.f )

, ProbeTextureFmt( D3DFMT_A8R8G8B8 )

, ProbePopulationStepX( 16.f )
, ProbePopulationStepY( 2.f )
, ProbePopulationStepZ( 16.f )

, ProbeElevation( 0.75f )
, MaxVerticalProbes( 4 )

, NominalProbeTileCountX( 32 )
, NominalProbeTileCountZ( 32 )

, ProbeVolumeOffsetY( 0.f )
, DefaultBounceColor_Up( 127, 127, 127 )
, DefaultBounceColor_Down( 127, 127, 127 )

, MaximumProbeRadius( 16 )
, MaximumProbeYRadius( 2 )

, ProbeRadiusExpansion( 0 )
{

}

//------------------------------------------------------------------------

ProbeMaster::Info::Info()
: ProbeMapWorldActualXSize( 1024.f )
, ProbeMapWorldActualYSize( 64.f )
, ProbeMapWorldActualZSize( 1024.f )
, ProbeMapWorldNominalXSize( 1024.f )
, ProbeMapWorldNominalYSize( 64.f )
, ProbeMapWorldNominalZSize( 1024.f )
, ProbeMapWorldXStart( 0.f )
, ProbeMapWorldYStart( 0.f )
, ProbeMapWorldZStart( 0.f )
, TotalProbeCellsCountX( 256 )
, TotalProbeCellsCountY( 16 )
, TotalProbeCellsCountZ( 256 )
, ProbeCellsInTileX( 16 )
, ProbeCellsInTileZ( 16 )
, CellSizeX( 1.f )
, CellSizeY( 1.f )
, CellSizeZ( 1.f )
, ActualProbeTileCountX( 0 )
, ActualProbeTileCountZ( 0 )
, CamCellX( 0 )
, CamCellY( 0 )
, CamCellZ( 0 )
, ProbeVolumeCamCellX( 0 )
, ProbeVolumeCamCellY( 0 )
, ProbeVolumeCamCellZ( 0 )
{

}

//------------------------------------------------------------------------

ProbeMaster::MemStats::MemStats()
: ProbeSize( 0 )
, ProbeVolumeSize( 0 )
{

}

//------------------------------------------------------------------------

R3D_FORCEINLINE void ProbeMaster::GetCellCoords( int * oCellX, int * oCellY, int * oCellZ, const r3dPoint3D& pos )
{
	float offY = m_Settings.ProbeVolumeOffsetY;

	*oCellX = int( ( pos.x - m_Info.ProbeMapWorldXStart			) * m_Info.TotalProbeCellsCountX / m_Info.ProbeMapWorldActualXSize );
	*oCellY = int( ( pos.y - m_Info.ProbeMapWorldYStart + offY	) * m_Info.TotalProbeCellsCountY / m_Info.ProbeMapWorldActualYSize );
	*oCellZ = int( ( pos.z - m_Info.ProbeMapWorldZStart			) * m_Info.TotalProbeCellsCountZ / m_Info.ProbeMapWorldActualZSize );
}

//------------------------------------------------------------------------

R3D_FORCEINLINE ProbeIdx ProbeMaster::GetClosestProbeIdx( int cellX, int cellY, int cellZ )
{
	int tileX = cellX / m_Info.ProbeCellsInTileX;
	int inTileX = cellX % m_Info.ProbeCellsInTileX;

	int tileZ = cellZ / m_Info.ProbeCellsInTileZ;
	int inTileZ = cellZ % m_Info.ProbeCellsInTileZ;

	r3dPoint3D realPose(	( cellX + 0.5f ) * m_Info.CellSizeX,
							( cellY + 0.5f ) * m_Info.CellSizeY,
							( cellZ + 0.5f ) * m_Info.CellSizeZ );

	ProbeTile& tile = m_ProbeMap.At( tileX, tileZ );

	float minDist = FLT_MAX;
	int minDistanceIndex = -1;

	for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
	{
		float dist = ( tile.TheProbes[ i ].Position - realPose ).Length();

		if( dist < minDist )
		{
			minDist = dist;
			minDistanceIndex = i;
		}
	}

	if( minDistanceIndex < 0 )
	{
		ProbeIdx empty;
		empty.CombinedIdx = -1;

		return empty;
	}
	else
	{
		ProbeIdx idx;

		idx.TileX = tileX;
		idx.TileZ = tileZ;
		idx.InTileIdx = minDistanceIndex;

		return idx;
	}
}

//------------------------------------------------------------------------

R3D_FORCEINLINE void ProbeMaster::FillProbeVolumeCellWithProbe(	const D3DLOCKED_BOX (&lboxes)[ Probe::NUM_DIRS ],
																const D3DBOX& box, int cx, int cy, int cz, const Probe& probe )
{
	int acx = m_Info.ProbeVolumeCamCellX + ( cx - m_Info.CamCellX );
	int acy = m_Info.ProbeVolumeCamCellY + ( cy - m_Info.CamCellY );
	int acz = m_Info.ProbeVolumeCamCellZ + ( cz - m_Info.CamCellZ );

	acx %= m_Settings.ProbeTextureWidth;
	acy %= m_Settings.ProbeTextureDepth;
	acz %= m_Settings.ProbeTextureHeight;

	if( acx < 0 ) acx += m_Settings.ProbeTextureWidth;
	if( acy < 0 ) acy += m_Settings.ProbeTextureDepth;
	if( acz < 0 ) acz += m_Settings.ProbeTextureHeight;

	r3d_assert( acx >= (int)box.Left && acx < (int)box.Right );
	r3d_assert( acy >= (int)box.Front && acy < (int)box.Back );
	r3d_assert( acz >= (int)box.Top && acz < (int)box.Bottom );

	acx -= box.Left;
	acy -= box.Front;
	acz -= box.Top;

	for( int i = 0; i < Probe::NUM_DIRS; i ++ )
	{
		const D3DLOCKED_BOX& lbox = lboxes[ i ];
		((UINT32*)((char*)lbox.pBits + lbox.RowPitch * acz + lbox.SlicePitch * acy ))[ acx ] = probe.BasisColors32[ i ];
	}
}

//------------------------------------------------------------------------

R3D_FORCEINLINE void ProbeMaster::RasterizeXSlice( int startTexX, int endTexX, int cellX, int cellY, int cellZ, const Probe& defaultProbe )
{
	int tilesInRad = m_Settings.MaximumProbeRadius / m_Info.ProbeCellsInTileX + 1;

	int xStretch = endTexX - startTexX;

	int texYLen2 = m_Settings.ProbeTextureDepth / 2;
	int texZLen2 = m_Settings.ProbeTextureHeight / 2;

	int xTileStart = cellX / m_Info.ProbeCellsInTileX;
	int xTileEnd = ( cellX + xStretch ) / m_Info.ProbeCellsInTileX;

	int zTileStart = ( cellZ - texZLen2 ) / m_Info.ProbeCellsInTileZ;
	int zTileEnd = ( cellZ + texZLen2 ) / m_Info.ProbeCellsInTileZ;

	xTileStart -= tilesInRad;
	xTileEnd += tilesInRad;

	zTileStart -= tilesInRad;
	zTileEnd += tilesInRad;

	m_ProbeRasterSamplesVolume.Resize( xStretch * m_Settings.ProbeTextureHeight * m_Settings.ProbeTextureDepth );
	ResetRasterSamples();

	xTileStart = R3D_MIN( R3D_MAX( xTileStart, 0 ), (int)m_ProbeMap.Width() - 1 );
	xTileEnd = R3D_MIN( R3D_MAX( xTileEnd, 0 ), (int)m_ProbeMap.Width() - 1 );

	zTileStart = R3D_MIN( R3D_MAX( zTileStart, 0 ), (int)m_ProbeMap.Height() - 1 );
	zTileEnd = R3D_MIN( R3D_MAX( zTileEnd, 0 ), (int)m_ProbeMap.Height() - 1 );

	D3DBOX box;

	box.Left = startTexX;
	box.Right = endTexX;

	box.Top = 0;
	box.Bottom = m_Settings.ProbeTextureHeight;

	box.Front = 0;
	box.Back = m_Settings.ProbeTextureDepth;

	D3DLOCKED_BOX lboxes[ Probe::NUM_DIRS ];

	UINT32 *locked32[ Probe::NUM_DIRS ];

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		r3dTexture* tex = m_DirectionVolumeTextures[ i ];
		IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

		D3D_V( volTex->LockBox( 0, &lboxes[ i ], &box, 0 ) );

		locked32[ i ] = (UINT32*)lboxes[ i ].pBits;
	}

	int lockedSize = lboxes[ 0 ].SlicePitch * ( box.Back - box.Front );

	for( int y = 0, cy = cellY - texYLen2, e = m_Settings.ProbeTextureDepth; y < e; y ++, cy ++ )
	{
		for( int z = 0, e = m_Settings.ProbeTextureHeight, cz = cellZ - texZLen2; z < e; z ++, cz ++ )
		{
			for( int x = 0, cx = cellX, e = xStretch; x < e; x ++, cx ++ )
			{
				FillProbeVolumeCellWithProbe( lboxes, box, cx, cy, cz, defaultProbe );
			}
		}
	}

	for( int tz = zTileStart, tze = zTileEnd; tz <= tze ; tz ++ )
	{
		for( int tx = xTileStart, txe = xTileEnd; tx <= txe ; tx ++ )
		{
			if( tx < 0 || tx >= (int)m_ProbeMap.Width() ||
				tz < 0 || tz >= (int)m_ProbeMap.Height() )
				continue;

			ProbeTile& tile = m_ProbeMap[ tz ][ tx ];

			for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
			{
				Probe& probe = tile.TheProbes[ i ];

				if( probe.CellX + probe.XRadius >= cellX
						&&
					probe.CellX - probe.XRadius < cellX + xStretch
						&&
					probe.CellZ + probe.ZRadius >= cellZ - texZLen2
						&&
					probe.CellZ - probe.ZRadius < cellZ + texZLen2
						&&
					probe.CellY + probe.YRadius >= cellY - texYLen2
						&&
					probe.CellY - probe.YRadius < cellY + texYLen2
						)
				{
					RasterizeProbeIntoVolume( &probe, box, cellX, cellY - texYLen2, cellZ - texZLen2, xStretch, m_Settings.ProbeTextureDepth, m_Settings.ProbeTextureHeight );
				}
			}
		}
	}

	CopySamplesIntoLockedTextures( locked32, lboxes[ 0 ].RowPitch, lboxes[ 0 ].SlicePitch, xStretch, m_Settings.ProbeTextureDepth, m_Settings.ProbeTextureHeight );

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		r3dTexture* tex = m_DirectionVolumeTextures[ i ];
		IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

		D3D_V( volTex->UnlockBox( 0 ) );
	}
}

//------------------------------------------------------------------------

void ProbeMaster::RasterizeYSlice( int startTexY, int endTexY, int cellX, int cellY, int cellZ, const Probe& defaultProbe )
{
	int yStretch = endTexY - startTexY;

	int tilesInRad = m_Settings.MaximumProbeRadius / m_Info.ProbeCellsInTileX + 1;

	int texXLen2 = m_Settings.ProbeTextureWidth / 2;
	int texZLen2 = m_Settings.ProbeTextureHeight / 2;

	int xTileStart = ( cellX - texXLen2 ) / m_Info.ProbeCellsInTileX;
	int xTileEnd = ( cellX + texXLen2 ) / m_Info.ProbeCellsInTileX;

	int zTileStart = ( cellZ - texZLen2 ) / m_Info.ProbeCellsInTileZ;
	int zTileEnd = ( cellZ + texZLen2 ) / m_Info.ProbeCellsInTileZ;

	xTileStart -= tilesInRad;
	xTileEnd += tilesInRad;

	zTileStart -= tilesInRad;
	zTileEnd += tilesInRad;

	m_ProbeRasterSamplesVolume.Resize( yStretch * m_Settings.ProbeTextureWidth * m_Settings.ProbeTextureHeight );
	ResetRasterSamples();

	xTileStart = R3D_MIN( R3D_MAX( xTileStart, 0 ), (int)m_ProbeMap.Width() - 1 );
	xTileEnd = R3D_MIN( R3D_MAX( xTileEnd, 0 ), (int)m_ProbeMap.Width() - 1 );

	zTileStart = R3D_MIN( R3D_MAX( zTileStart, 0 ), (int)m_ProbeMap.Height() - 1 );
	zTileEnd = R3D_MIN( R3D_MAX( zTileEnd, 0 ), (int)m_ProbeMap.Height() - 1 );

	D3DBOX box;

	box.Left = 0;
	box.Right = m_Settings.ProbeTextureWidth;

	box.Top = 0;
	box.Bottom = m_Settings.ProbeTextureHeight;

	box.Front = startTexY;
	box.Back = endTexY;

	//------------------------------------------------------------------------

	D3DLOCKED_BOX lboxes[ Probe::NUM_DIRS ];

	UINT32 *locked32[ Probe::NUM_DIRS ];

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		r3dTexture* tex = m_DirectionVolumeTextures[ i ];
		IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

		D3D_V( volTex->LockBox( 0, &lboxes[ i ], &box, 0 ) );

		locked32[ i ] = (UINT32*)lboxes[ i ].pBits;
	}

	int lockedSize = lboxes[ 0 ].SlicePitch * ( box.Back - box.Front );

	//------------------------------------------------------------------------

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		for( int y = 0, cy = cellY, e = yStretch; y < e; y ++, cy ++ )
		{
			for( int z = 0, e = m_Settings.ProbeTextureHeight, cz = cellZ - texZLen2; z < e; z ++, cz ++ )
			{
				for( int x = 0, cx = cellX - texXLen2, e = m_Settings.ProbeTextureWidth; x < e; x ++, cx ++ )
				{
					FillProbeVolumeCellWithProbe( lboxes, box, cx, cy, cz, defaultProbe );
				}
			}
		}
	}

	//------------------------------------------------------------------------
	for( int tz = zTileStart, tze = zTileEnd; tz <= tze ; tz ++ )
	{
		for( int tx = xTileStart, txe = xTileEnd; tx <= txe ; tx ++ )
		{
			if( tx < 0 || tx >= (int)m_ProbeMap.Width() ||
				tz < 0 || tz >= (int)m_ProbeMap.Height() )
				continue;

			ProbeTile& tile = m_ProbeMap[ tz ][ tx ];

			for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
			{
				Probe& probe = tile.TheProbes[ i ];

				if( probe.CellX + probe.XRadius >= cellX - texXLen2
						&&
					probe.CellX - probe.XRadius < cellX + texXLen2
						&&
					probe.CellZ + probe.ZRadius >= cellZ - texZLen2
						&&
					probe.CellZ - probe.ZRadius < cellZ + texZLen2
						&&
					probe.CellY + probe.YRadius >= cellY
						&&
					probe.CellY - probe.YRadius < cellY + yStretch
						)
				{
					RasterizeProbeIntoVolume( &probe, box, cellX - texXLen2, cellY, cellZ - texZLen2, m_Settings.ProbeTextureWidth, yStretch, m_Settings.ProbeTextureHeight );
				}
			}
		}
	}

	CopySamplesIntoLockedTextures( locked32, lboxes[ 0 ].RowPitch, lboxes[ 0 ].SlicePitch, m_Settings.ProbeTextureWidth, yStretch, m_Settings.ProbeTextureHeight );

	//------------------------------------------------------------------------

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		r3dTexture* tex = m_DirectionVolumeTextures[ i ];
		IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

		D3D_V( volTex->UnlockBox( 0 ) );
	}
}

//------------------------------------------------------------------------

void ProbeMaster::RasterizeZSlice( int startTexZ, int endTexZ, int cellX, int cellY, int cellZ, const Probe& defaultProbe )
{
	int tilesInRad = m_Settings.MaximumProbeRadius / m_Info.ProbeCellsInTileX + 1;

	int zStretch = endTexZ - startTexZ;

	int texXLen2 = m_Settings.ProbeTextureWidth / 2;
	int texYLen2 = m_Settings.ProbeTextureDepth / 2;

	int xTileStart = ( cellX - texXLen2 ) / m_Info.ProbeCellsInTileX;
	int xTileEnd = ( cellX + texXLen2 ) / m_Info.ProbeCellsInTileX;

	int zTileStart = cellZ / m_Info.ProbeCellsInTileZ;
	int zTileEnd = ( cellZ + zStretch ) / m_Info.ProbeCellsInTileZ;

	xTileStart -= tilesInRad;
	xTileEnd += tilesInRad;

	zTileStart -= tilesInRad;
	zTileEnd += tilesInRad;

	m_ProbeRasterSamplesVolume.Resize( m_Settings.ProbeTextureWidth * m_Settings.ProbeTextureDepth * zStretch );
	ResetRasterSamples();

	zTileStart = R3D_MIN( R3D_MAX( zTileStart, 0 ), (int)m_ProbeMap.Height() - 1 );
	zTileEnd = R3D_MIN( R3D_MAX( zTileEnd, 0 ), (int)m_ProbeMap.Height() - 1 );

	xTileStart = R3D_MIN( R3D_MAX( xTileStart, 0 ), (int)m_ProbeMap.Width() - 1 );
	xTileEnd = R3D_MIN( R3D_MAX( xTileEnd, 0 ), (int)m_ProbeMap.Width() - 1 );

	D3DBOX box;

	box.Left = 0;
	box.Right = m_Settings.ProbeTextureWidth;

	box.Top = startTexZ;
	box.Bottom = endTexZ;

	box.Front = 0;
	box.Back = m_Settings.ProbeTextureDepth;

	D3DLOCKED_BOX lboxes[ Probe::NUM_DIRS ];

	UINT32 *locked32[ Probe::NUM_DIRS ];

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		r3dTexture* tex = m_DirectionVolumeTextures[ i ];
		IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

		D3D_V( volTex->LockBox( 0, &lboxes[ i ], &box, 0 ) );

		locked32[ i ] = (UINT32*)lboxes[ i ].pBits;
	}

	int lockedSize = lboxes[ 0 ].SlicePitch * ( box.Back - box.Front );

	for( int y = 0, cy = cellY - texYLen2, e = m_Settings.ProbeTextureDepth; y < e; y ++, cy ++ )
	{
		for( int z = 0, cz = cellZ, e = zStretch; z < e; z ++, cz ++ )
		{
			for( int x = 0, e = m_Settings.ProbeTextureWidth, cx = cellX - texXLen2; x < e; x ++, cx ++ )
			{
				FillProbeVolumeCellWithProbe( lboxes, box, cx, cy, cz, defaultProbe );
			}
		}
	}

	for( int tz = zTileStart, tze = zTileEnd; tz <= tze ; tz ++ )
	{
		for( int tx = xTileStart, txe = xTileEnd; tx <= txe ; tx ++ )
		{
			if( tx < 0 || tx >= (int)m_ProbeMap.Width() ||
				tz < 0 || tz >= (int)m_ProbeMap.Height() )
				continue;

			ProbeTile& tile = m_ProbeMap[ tz ][ tx ];

			for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
			{
				Probe& probe = tile.TheProbes[ i ];

				if( probe.CellX + probe.XRadius >= cellX - texXLen2
						&&
					probe.CellX - probe.XRadius < cellX + texXLen2
						&&
					probe.CellZ + probe.ZRadius >= cellZ
						&&
					probe.CellZ - probe.ZRadius < cellZ + zStretch
						&&
					probe.CellY + probe.YRadius >= cellY - texYLen2
						&&
					probe.CellY - probe.YRadius < cellY + texYLen2
					)
				{
					RasterizeProbeIntoVolume( &probe, box, cellX - texXLen2, cellY - texYLen2, cellZ, m_Settings.ProbeTextureWidth, m_Settings.ProbeTextureDepth, zStretch );
				}
			}
		}
	}

	CopySamplesIntoLockedTextures( locked32, lboxes[ 0 ].RowPitch, lboxes[ 0 ].SlicePitch, m_Settings.ProbeTextureWidth, m_Settings.ProbeTextureDepth, zStretch );

	//------------------------------------------------------------------------

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		r3dTexture* tex = m_DirectionVolumeTextures[ i ];
		IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

		D3D_V( volTex->UnlockBox( 0 ) );
	}
}

//------------------------------------------------------------------------

R3D_FORCEINLINE void ProbeMaster::RasterizeDirtyRect( int startTexX, int endTexX, int startTexZ, int endTexZ, int cellX, int cellY, int cellZ, const Probe& defaultProbe )
{
	int tilesInRad = m_Settings.MaximumProbeRadius / m_Info.ProbeCellsInTileX + 1;

	int xStretch = endTexX - startTexX;
	int zStretch = endTexZ - startTexZ;

	int texYLen2 = m_Settings.ProbeTextureDepth / 2;

	int xTileStart = cellX / m_Info.ProbeCellsInTileX;
	int xTileEnd = ( cellX + xStretch ) / m_Info.ProbeCellsInTileX;

	int zTileStart = cellZ / m_Info.ProbeCellsInTileZ;
	int zTileEnd = ( cellZ + zStretch ) / m_Info.ProbeCellsInTileZ;

	xTileStart -= tilesInRad;
	xTileEnd += tilesInRad;

	zTileStart -= tilesInRad;
	zTileEnd += tilesInRad;

	m_ProbeRasterSamplesVolume.Resize( xStretch * zStretch * m_Settings.ProbeTextureDepth );
	ResetRasterSamples();

	xTileStart = R3D_MIN( R3D_MAX( xTileStart, 0 ), (int)m_ProbeMap.Width() - 1 );
	xTileEnd = R3D_MIN( R3D_MAX( xTileEnd, 0 ), (int)m_ProbeMap.Width() - 1 );

	zTileStart = R3D_MIN( R3D_MAX( zTileStart, 0 ), (int)m_ProbeMap.Height() - 1 );
	zTileEnd = R3D_MIN( R3D_MAX( zTileEnd, 0 ), (int)m_ProbeMap.Height() - 1 );

	D3DBOX box;

	box.Left = startTexX;
	box.Right = endTexX;

	box.Top = startTexZ;
	box.Bottom = endTexZ;

	box.Front = 0;
	box.Back = m_Settings.ProbeTextureDepth;

	D3DLOCKED_BOX lboxes[ Probe::NUM_DIRS ];

	UINT32 *locked32[ Probe::NUM_DIRS ];

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		r3dTexture* tex = m_DirectionVolumeTextures[ i ];
		IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

		D3D_V( volTex->LockBox( 0, &lboxes[ i ], &box, 0 ) );

		locked32[ i ] = (UINT32*)lboxes[ i ].pBits;
	}

	int lockedSize = lboxes[ 0 ].SlicePitch * ( box.Back - box.Front );

	for( int y = 0, cy = cellY - texYLen2, e = m_Settings.ProbeTextureDepth; y < e; y ++, cy ++ )
	{
		for( int z = 0, cz = cellZ, e = zStretch; z < e; z ++, cz ++ )
		{
			for( int x = 0, cx = cellX, e = xStretch; x < e; x ++, cx ++ )
			{
				FillProbeVolumeCellWithProbe( lboxes, box, cx, cy, cz, defaultProbe );
			}
		}
	}

	for( int tz = zTileStart, tze = zTileEnd; tz <= tze ; tz ++ )
	{
		for( int tx = xTileStart, txe = xTileEnd; tx <= txe ; tx ++ )
		{
			if( tx < 0 || tx >= (int)m_ProbeMap.Width() ||
				tz < 0 || tz >= (int)m_ProbeMap.Height() )
				continue;

			ProbeTile& tile = m_ProbeMap[ tz ][ tx ];

			for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
			{
				Probe& probe = tile.TheProbes[ i ];

				if( probe.CellX + probe.XRadius >= cellX
					&&
					probe.CellX - probe.XRadius < cellX + xStretch
					&&
					probe.CellZ + probe.ZRadius >= cellZ
					&&
					probe.CellZ - probe.ZRadius < cellZ + zStretch
					&&
					probe.CellY + probe.YRadius >= cellY - texYLen2
					&&
					probe.CellY - probe.YRadius < cellY + texYLen2
					)
				{
					RasterizeProbeIntoVolume( &probe, box, cellX, cellY - texYLen2, cellZ, xStretch, m_Settings.ProbeTextureDepth, zStretch );
				}
			}
		}
	}

	CopySamplesIntoLockedTextures( locked32, lboxes[ 0 ].RowPitch, lboxes[ 0 ].SlicePitch, xStretch, m_Settings.ProbeTextureDepth, zStretch );

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
	{
		r3dTexture* tex = m_DirectionVolumeTextures[ i ];
		IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

		D3D_V( volTex->UnlockBox( 0 ) );
	}
}

//------------------------------------------------------------------------

R3D_FORCEINLINE void ProbeMaster::AddRasterSample( int idx, const Probe* probe )
{
	ProbeRasterSamples& samples = m_ProbeRasterSamplesVolume[ idx ];
	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		samples.sumR[ i ] += ( probe->CompositeColors32[ i ] >> 16 ) & 0xff;
		samples.sumG[ i ] += ( probe->CompositeColors32[ i ] >> 8 )& 0xff;
		samples.sumB[ i ] += ( probe->CompositeColors32[ i ] >> 0 )& 0xff;
	}

	samples.count ++;
}

//------------------------------------------------------------------------

R3D_FORCEINLINE void ProbeMaster::ResetRasterSamples()
{
	for( int i = 0, e = m_ProbeRasterSamplesVolume.Count(); i < e; i ++ )
	{
		ProbeRasterSamples& samples = m_ProbeRasterSamplesVolume[ i ];

		for( int idir = 0; idir < Probe::NUM_DIRS; idir ++ )
		{
			samples.sumR[ idir ] = 0;
			samples.sumG[ idir ] = 0;
			samples.sumB[ idir ] = 0;
			samples.count = 0;
		}
	}
}

//------------------------------------------------------------------------

R3D_FORCEINLINE void ProbeMaster::AvarageRasterSamples()
{
	for( int i = 0, e = m_ProbeRasterSamplesVolume.Count(); i < e; i ++ )
	{
		ProbeRasterSamples& samples = m_ProbeRasterSamplesVolume[ i ];

		if( samples.count )
		{
			for( int idir = 0; idir < Probe::NUM_DIRS; idir ++ )
			{
				samples.sumR[ idir ] /= samples.count;
				samples.sumG[ idir ] /= samples.count;
				samples.sumB[ idir ] /= samples.count;
			}

			samples.count = 1;
		}
	}
}

//------------------------------------------------------------------------

template <typename T>
R3D_FORCEINLINE void ProbeMaster::CopySamplesIntoLockedTextures( T* locked[ Probe::NUM_DIRS ], int pitchX, int pitchXZ, int lockedSizeX, int lockedSizeY, int lockedSizeZ )
{
	AvarageRasterSamples();

	int lockedSizeXY = lockedSizeX * lockedSizeY;
	int elemPitchX = pitchX / sizeof( T );
	int elemPitchXZ = pitchXZ / sizeof( T );
	int lockedElemSize = elemPitchXZ * lockedSizeY;

	for( int z = 0, e = lockedSizeZ; z < e; z ++ )
	{
		for( int y = 0, e = lockedSizeY; y < e; y ++ )
		{
			for( int x = 0, e = lockedSizeX; x < e; x ++ )
			{
				const ProbeRasterSamples& samples = m_ProbeRasterSamplesVolume[ x + y * lockedSizeX + z * lockedSizeXY ];

				if( samples.count )
				{
					r3d_assert( samples.count == 1 );

					int elemIdx = x + y * elemPitchXZ + z * elemPitchX;
					r3d_assert( elemIdx < lockedElemSize );

					for( int idir = 0, e = Probe::NUM_DIRS; idir < e; idir ++ )
					{
						locked[ idir ][ elemIdx ] = ConvertProbeRasterSamplesTo<T>( samples, idir );
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------

ProbeMaster::ProbeMaster()
: m_TempRT( NULL )
, m_BounceDiffuseRT ( NULL )
, m_BounceNormalRT ( NULL )
, m_BounceDepthRT ( NULL )
, m_BounceAccumSHRRT ( NULL )
, m_BounceAccumSHGRT ( NULL )
, m_BounceAccumSHBRT ( NULL )
, m_NormalToSHTex( NULL )
, m_SysmemTex( NULL )
, m_SavedUseOQ( 0 )
, m_VisMode( VISUALIZE_SKY_VISIBILITY )
, m_BounceSysmemRT_R( NULL )
, m_BounceSysmemRT_G( NULL )
, m_BounceSysmemRT_B( NULL )
, m_EditorMode( 0 )
, m_SkyDomeSH_R( 0, 0, 0, 0 )
, m_SkyDomeSH_G( 0, 0, 0, 0 )
, m_SkyDomeSH_B( 0, 0, 0, 0 )
, m_SunSH_R( 0, 0, 0, 0 )
, m_SunSH_G( 0, 0, 0, 0 )
, m_SunSH_B( 0, 0, 0, 0 )
, m_SkyVisA( 0.f )
, m_SkyVisB( 1.f )
, m_LastInfoFrame( 0.f )
, m_PrevCamCellX( 0 )
, m_PrevCamCellY( 0 )
, m_PrevCamCellZ( 0 )
, m_Created( 0 )
, m_ProbeRadiusesDirty( 0 )
, m_ProbeVolumeDirty( 1 )
, m_DEBUG_EndProbe( -1 )
, m_RasterizedProbeCount( 0 )
, m_SHProjectVertexDecl( 0 )
{
	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		m_SkyDirColors.DirColor[ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
	}
}

//------------------------------------------------------------------------

ProbeMaster::~ProbeMaster()
{

}

//------------------------------------------------------------------------

void ProbeMaster::Init()
{
	// fill probe directions
	{
		int idx = 0;
		float angle = 0;

		float ANGLE_STEP = 360.f / ( Probe::NUM_DIRS - 1 );

		Probe::ViewDirs[ idx ] = r3dPoint3D( 0, -1, 0 );
		Probe::UpVecs[ idx ] = r3dPoint3D( 1, 0, 0 );

		const float VERT_RAD_ANGLE = R3D_DEG2RAD( 30.f );

		for( ++ idx; idx < Probe::NUM_DIRS; idx ++, angle += ANGLE_STEP )
		{
			Probe::ViewDirs[ idx ] = r3dPoint3D(	cos( R3D_DEG2RAD( angle ) ) * cos( VERT_RAD_ANGLE ),
													sin( VERT_RAD_ANGLE ), 
													sin( R3D_DEG2RAD( angle ) ) * cos( VERT_RAD_ANGLE ) );

			Probe::ViewDirs[ idx ].Normalize();

			r3dPoint3D side = Probe::ViewDirs[ idx ].Cross( r3dPoint3D( 0, 1, 0 ) );
			Probe::UpVecs[ idx ] = side.Cross( Probe::ViewDirs[ idx ] );
			Probe::UpVecs[ idx ].Normalize();
		}
	}

	SH_setup_spherical_samples();

	// initialize probe basis transfer SH
	{
		for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
		{
			memcpy( &g_HemisphereNormal, &Probe::ViewDirs[ i ], sizeof g_HemisphereNormal );
			
			float coefs[ 4 ];
			ProjectOnSH( coefs, SampleHemisphereLighting );

			m_BasisSHArray[ i ].x = coefs[ 0 ];
			m_BasisSHArray[ i ].y = coefs[ 1 ];
			m_BasisSHArray[ i ].z = coefs[ 2 ];
			m_BasisSHArray[ i ].w = coefs[ 3 ];
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::InitEditor()
{
	m_EditorMode = 1;

	g_AccumVS_ID = r3dRenderer->GetShaderIdx( "VS_ACCUM_SH" );
	g_AccumPS_ID = r3dRenderer->GetShaderIdx( "PS_ACCUM_SH" );

	g_AccumVPLSH_VS_ID = r3dRenderer->GetShaderIdx( "VS_ACCUM_VPLSH" );
	g_AccumVPLSH_PS_ID = r3dRenderer->GetShaderIdx( "PS_ACCUM_VPLSH" );

	CreateTempRTAndSysMemTex();

	m_BounceDiffuseRT	= r3dScreenBuffer::CreateClass( "ProbeBounceDiffuse", (float)BOUNCE_RT_DIM, (float)BOUNCE_RT_DIM, D3DFMT_A8R8G8B8, r3dScreenBuffer::Z_OWN );
	m_BounceNormalRT	= r3dScreenBuffer::CreateClass( "ProbeBounceNormal",  (float)BOUNCE_RT_DIM, (float)BOUNCE_RT_DIM, D3DFMT_A8R8G8B8, r3dScreenBuffer::Z_NO_Z );
	m_BounceDepthRT		= r3dScreenBuffer::CreateClass( "ProbeBounceDepth",  (float)BOUNCE_RT_DIM, (float)BOUNCE_RT_DIM, D3DFMT_R32F, r3dScreenBuffer::Z_NO_Z );
	m_BounceAccumSHRRT	= r3dScreenBuffer::CreateClass( "ProbeBounceSHR", 1, 1, D3DFMT_A32B32G32R32F, r3dScreenBuffer::Z_NO_Z );
	m_BounceAccumSHGRT	= r3dScreenBuffer::CreateClass( "ProbeBounceSHG", 1, 1, D3DFMT_A32B32G32R32F, r3dScreenBuffer::Z_NO_Z );
	m_BounceAccumSHBRT	= r3dScreenBuffer::CreateClass( "ProbeBounceSHB", 1, 1, D3DFMT_A32B32G32R32F, r3dScreenBuffer::Z_NO_Z );
	
	D3D_V( r3dRenderer->pd3ddev->CreateTexture( 1, 1, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM, &m_BounceSysmemRT_R, NULL ) );
	D3D_V( r3dRenderer->pd3ddev->CreateTexture( 1, 1, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM, &m_BounceSysmemRT_G, NULL ) );
	D3D_V( r3dRenderer->pd3ddev->CreateTexture( 1, 1, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM, &m_BounceSysmemRT_B, NULL ) );	

	SH_setup_spherical_samples();

	//------------------------------------------------------------------------

	D3DVERTEXELEMENT9 VBDecl[] = 
	{
		{0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		{0, 16, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
		D3DDECL_END()
	};

	D3D_V( r3dRenderer->pd3ddev->CreateVertexDeclaration( VBDecl, &m_SHProjectVertexDecl ) );

	//------------------------------------------------------------------------

	int countsPerDir[ Probe::NUM_DIRS ];

	memset( countsPerDir, 0, sizeof countsPerDir );

	for( int i = 0, e = (int)g_SHSamples.Count(); i < e; i ++ )
	{
		const SHSample& s = g_SHSamples[ i ];
		
		countsPerDir[ s.faceIdx ] ++;
	}

	D3DXMATRIX viewMatrices[ Probe::NUM_DIRS ];
	D3DXMATRIX projMatrices[ Probe::NUM_DIRS ];

	for( int d = 0; d < 4; d ++ )
	{
		r3dCamera cam;

		(r3dPoint3D&)cam = r3dPoint3D( 0, 0, 0 );

		r3dPoint3D viewDir = Probe::ViewDirs[ d ];

		cam.vUP = Probe::UpVecs[ d ];
		cam.PointTo( gCam + viewDir );

		D3DXMATRIX viewMatrix;
		r3dRenderer->BuildViewMtx( cam, &viewMatrices[ d ], NULL );

		D3DXMATRIX projMatrix;
		r3dRenderer->BuildPerspectiveMtx( cam, &projMatrices[ d ] );
	}


	for( int d = 0, e = Probe::NUM_DIRS; d < e; d ++ )
	{
		r3dVertexBuffer* vbuf = new r3dVertexBuffer( countsPerDir[ d ], sizeof( SHProjectVertex ) );

		m_SHProjectVertexBuffers[ d ] = vbuf;

		SHProjectVertex* data = static_cast< SHProjectVertex* >( vbuf->Lock() );
		SHProjectVertex* data_start = data;

		for( int i = 0, e = (int)g_SHSamples.Count(); i < e; i ++ )
		{
			const SHSample& s = g_SHSamples[ i ];

			if( s.faceIdx == d )
			{
				memcpy( &data->coefs, s.coeff, sizeof data->coefs );

				D3DXVECTOR4 d3dProjDir( s.vec.x, s.vec.y, s.vec.z, 1.0f );

				D3DXMATRIX viewProjMatrix = viewMatrices[ d ] * projMatrices[ d ];

				D3DXVec4Transform( &d3dProjDir, &d3dProjDir, &viewProjMatrix );

				d3dProjDir*= 1.0f / d3dProjDir.w;

				data->texc.x = d3dProjDir.x * 0.5f + 0.5f;
				data->texc.y = d3dProjDir.y * 0.5f + 0.5f;

				D3DXVECTOR3 d3dVec( s.vec.x, s.vec.y, s.vec.z );

				const r3dPoint3D& viewZDir = Probe::ViewDirs[ d ];
				D3DXVECTOR3 d3dViewZDir( viewZDir.x, viewZDir.y, viewZDir.z );

				float factor = D3DXVec3Dot( &d3dVec, &d3dViewZDir );

				// make Z in view space == 1 for easier conversion in accum ps from
				// depth value and into real world position
				d3dVec /= factor;

				data->vec = float3( d3dVec.x, d3dVec.y, d3dVec.z );

				data ++;

				r3d_assert( data - data_start <= vbuf->GetItemCount() );
			}
		}

		vbuf->Unlock();
	}

	FillNormalToSHTex();

	UndoLightProbeCreateDelete::Register();
}

//------------------------------------------------------------------------

void ProbeMaster::Close()
{
	DestroyTempRTAndSysMemTex();

	if( m_NormalToSHTex )
	{
		r3dRenderer->DeleteTexture( m_NormalToSHTex );
		m_NormalToSHTex = NULL;
	}

	SAFE_DELETE( m_BounceDiffuseRT );
	SAFE_DELETE( m_BounceNormalRT );
	SAFE_DELETE( m_BounceDepthRT );
	SAFE_DELETE( m_BounceAccumSHRRT );
	SAFE_DELETE( m_BounceAccumSHGRT );
	SAFE_DELETE( m_BounceAccumSHBRT );

	SAFE_RELEASE( m_BounceSysmemRT_R );
	SAFE_RELEASE( m_BounceSysmemRT_G );
	SAFE_RELEASE( m_BounceSysmemRT_B );

}

//------------------------------------------------------------------------

bool ProbeMaster::IsCreated() const
{
	return m_Created ? true : false;
}

//------------------------------------------------------------------------

#define R3D_PROBE_DIR "/LightProbes"
#define R3D_PROBE_PATH R3D_PROBE_DIR "/probes.bin"

#define R3D_PROBE_BIN_SIG101 "ProbeZ101"
#define R3D_PROBE_BIN_SIG102 "ProbeZ102"
#define R3D_PROBE_BIN_SIG "ProbeZ105"

void ProbeMaster::Save( const char* levelDir )
{
	SaveXMLSettings( levelDir );

	char fullpath[ MAX_PATH * 2 ];
	strcpy( fullpath, levelDir );
	strcat( fullpath, R3D_PROBE_DIR );
	mkdir( fullpath );

	strcpy( fullpath, levelDir );
	strcat( fullpath, R3D_PROBE_PATH );

	FILE* fout = fopen ( fullpath, "wb" );

	fwrite( R3D_PROBE_BIN_SIG, sizeof R3D_PROBE_BIN_SIG, 1 , fout );

	fwrite( &m_Info.ProbeMapWorldNominalXSize, sizeof m_Info.ProbeMapWorldNominalXSize, 1, fout );
	fwrite( &m_Info.ProbeMapWorldNominalYSize, sizeof m_Info.ProbeMapWorldNominalYSize, 1, fout );
	fwrite( &m_Info.ProbeMapWorldNominalZSize, sizeof m_Info.ProbeMapWorldNominalZSize, 1, fout );

	fwrite( &m_Info.ProbeMapWorldXStart, sizeof m_Info.ProbeMapWorldXStart, 1, fout );
	fwrite( &m_Info.ProbeMapWorldYStart, sizeof m_Info.ProbeMapWorldYStart, 1, fout );
	fwrite( &m_Info.ProbeMapWorldZStart, sizeof m_Info.ProbeMapWorldZStart, 1, fout );

	UINT16 width = m_Settings.NominalProbeTileCountX;
	UINT16 height = m_Settings.NominalProbeTileCountZ;

	fwrite( &width, sizeof width, 1, fout );
	fwrite( &height, sizeof height, 1, fout );

	UINT16 structSize = sizeof( Probe );

	fwrite( &structSize, sizeof structSize, 1, fout );

	Probes probes;

	FetchAllProbes( &probes );

	UINT32 count = probes.Count();

	fwrite( &count, sizeof count, 1, fout );

	for( int i = 0, e = probes.Count(); i < e ; i ++ )
	{
		const Probe& probe = probes[ i ];			

		fwrite( &probe.SH_BounceR, sizeof probe.SH_BounceR, 1, fout );
		fwrite( &probe.SH_BounceG, sizeof probe.SH_BounceG, 1, fout );
		fwrite( &probe.SH_BounceB, sizeof probe.SH_BounceB, 1, fout );

		fwrite( &probe.SkyVisibility, sizeof probe.SkyVisibility, 1, fout );
		fwrite( &probe.Position, sizeof probe.Position, 1, fout );

		fwrite( &probe.Flags, sizeof probe.Flags, 1, fout );

		INT32 localVPLCount = CountProbeLocalVPLs( &probe );

		fwrite( &localVPLCount, sizeof localVPLCount, 1, fout );

		fwrite( &probe.SH_LocalVPLProbeIndexes[ 0 ], sizeof probe.SH_LocalVPLProbeIndexes[ 0 ] * localVPLCount, 1, fout );

		fwrite( &probe.SH_LocalVPLsR[ 0 ], sizeof probe.SH_LocalVPLsR[ 0 ] * localVPLCount, 1, fout );
		fwrite( &probe.SH_LocalVPLsG[ 0 ], sizeof probe.SH_LocalVPLsG[ 0 ] * localVPLCount, 1, fout );
		fwrite( &probe.SH_LocalVPLsB[ 0 ], sizeof probe.SH_LocalVPLsB[ 0 ] * localVPLCount, 1, fout );
	}

	fclose( fout );
}

//------------------------------------------------------------------------

int ProbeMaster::Load( const char* levelDir )
{
	LoadXMLSettings( levelDir );

	m_ProbeVolumeDirty = 1;

	char fullpath[ MAX_PATH * 2 ];

	strcpy( fullpath, levelDir );
	strcat( fullpath, R3D_PROBE_PATH );

	if( r3dFile* fin = r3d_open( fullpath, "rb" ) )
	{
		struct CloseFile
		{
			~CloseFile()
			{
				fclose( f );
			}
			r3dFile* f;
		} closeFile = { fin }; (void)closeFile;

		char sig[ sizeof R3D_PROBE_BIN_SIG ];

		if( !fread( sig, sizeof sig, 1, fin ) )
			return 0;

		int sigCurr = !strcmp( sig, R3D_PROBE_BIN_SIG );

		if( !sigCurr )
		{
			return 0;
		}

		if( !fread( &m_Info.ProbeMapWorldNominalXSize, sizeof m_Info.ProbeMapWorldNominalXSize, 1, fin ) )
			return 0;

		if( !fread( &m_Info.ProbeMapWorldNominalYSize, sizeof m_Info.ProbeMapWorldNominalYSize, 1, fin ) )
			return 0;

		if( !fread( &m_Info.ProbeMapWorldNominalZSize, sizeof m_Info.ProbeMapWorldNominalZSize, 1, fin ) )
			return 0;

		if( !fread( &m_Info.ProbeMapWorldXStart, sizeof m_Info.ProbeMapWorldXStart, 1, fin ) )
			return 0;

		if( !fread( &m_Info.ProbeMapWorldYStart, sizeof m_Info.ProbeMapWorldYStart, 1, fin ) )
			return 0;

		if( !fread( &m_Info.ProbeMapWorldZStart, sizeof m_Info.ProbeMapWorldZStart, 1, fin ) )
			return 0;

		UINT16 width;
		UINT16 height;

		if( !fread( &width, sizeof width, 1, fin ) )
			return 0;

		if( !fread( &height, sizeof height, 1, fin ) )
			return 0;

		m_Settings.NominalProbeTileCountX = width;
		m_Settings.NominalProbeTileCountZ = height;

		Create( m_Info.ProbeMapWorldXStart,
				m_Info.ProbeMapWorldYStart,
				m_Info.ProbeMapWorldZStart,
				m_Info.ProbeMapWorldNominalXSize,
				m_Info.ProbeMapWorldNominalYSize,
				m_Info.ProbeMapWorldNominalZSize );

		UINT16 structSize;

		if( !fread( &structSize, sizeof structSize, 1, fin ) )
			return 0;

		r3d_assert( sizeof( Probe ) >= structSize );

		{
			UINT32 count;

			if( !fread( &count, sizeof count, 1, fin ) )
				return 0;

			for( int i = 0, e = (int)count; i < e; i ++ )
			{
				Probe probe;

				if( !fread( &probe.SH_BounceR, sizeof probe.SH_BounceR, 1, fin ) ) return 0;
				if( !fread( &probe.SH_BounceG, sizeof probe.SH_BounceG, 1, fin ) ) return 0;
				if( !fread( &probe.SH_BounceB, sizeof probe.SH_BounceB, 1, fin ) ) return 0;

				if( !fread( &probe.SkyVisibility, sizeof probe.SkyVisibility, 1, fin ) ) return 0;
				if( !fread( &probe.Position, sizeof probe.Position, 1, fin ) ) return 0;

				if( !fread( &probe.Flags, sizeof probe.Flags, 1, fin ) ) return 0;

				INT32 localVPLCount;

				if( !fread( &localVPLCount, sizeof localVPLCount, 1, fin ) ) return 0;

				r3d_assert( localVPLCount <= Probe::NUM_LOCAL_VPL );

				if( localVPLCount )
				{
					if( !fread( &probe.SH_LocalVPLProbeIndexes[ 0 ], sizeof probe.SH_LocalVPLProbeIndexes[ 0 ] * localVPLCount, 1, fin ) ) return 0;

					if( !fread( &probe.SH_LocalVPLsR[ 0 ], sizeof probe.SH_LocalVPLsR[ 0 ] * localVPLCount, 1, fin ) ) return 0;
					if( !fread( &probe.SH_LocalVPLsG[ 0 ], sizeof probe.SH_LocalVPLsG[ 0 ] * localVPLCount, 1, fin ) ) return 0;
					if( !fread( &probe.SH_LocalVPLsB[ 0 ], sizeof probe.SH_LocalVPLsB[ 0 ] * localVPLCount, 1, fin ) ) return 0;
				}

				AddProbe( probe, false );
			}
		}

		AdjustProbeRadiuses();

		SetupDefaultLightProbe();

		RelightProbe( &m_DefaultProbe );

		return 1;
	}

	return 0;
}

//------------------------------------------------------------------------

void ProbeMaster::SaveXMLSettings( const char* levelDir )
{
	pugi::xml_document xmlFile;
	xmlFile.append_child( pugi::node_comment ).set_value("ProbeMaster Settings File");
	pugi::xml_node xmlRoot = xmlFile.append_child();
	xmlRoot.set_name( "root" );

	pugi::xml_node xmlSettings = xmlRoot.append_child();
	xmlSettings.set_name( "settings" );

	xmlSettings.append_attribute( "maximum_probe_radius" ) = m_Settings.MaximumProbeRadius;
	xmlSettings.append_attribute( "maximum_probe_yradius" ) = m_Settings.MaximumProbeYRadius;
	xmlSettings.append_attribute( "probe_radius_expansion") = m_Settings.ProbeRadiusExpansion;

	r3dString path = r3dString( levelDir ) + R3D_PROBE_DIR "/" + "Settings.xml";

	xmlFile.save_file( path.c_str() );
}

//------------------------------------------------------------------------

void ProbeMaster::LoadXMLSettings( const char* levelDir )
{
	r3dString path = r3dString( levelDir ) + R3D_PROBE_DIR "/" + "Settings.xml";

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
		m_Settings.MaximumProbeRadius = settings.attribute( "maximum_probe_radius" ).as_int();
		m_Settings.MaximumProbeYRadius = settings.attribute( "maximum_probe_yradius" ).as_int();
		m_Settings.ProbeRadiusExpansion = settings.attribute( "probe_radius_expansion" ).as_int();
	}

	delete [] fileBuffer;

	fclose( f );
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateDefaultProbe()
{
	SetupDefaultLightProbe();

	RelightProbe( &m_DefaultProbe );

	m_ProbeVolumeDirty = 1;

	UpdateProbeVolumes();
}

//------------------------------------------------------------------------

int ProbeMaster::UpdateDynamicLights()
{
	int startCellX = m_Info.CamCellX - m_Settings.ProbeTextureWidth / 2;
	int startCellY = m_Info.CamCellY - m_Settings.ProbeTextureDepth / 2;
	int startCellZ = m_Info.CamCellZ - m_Settings.ProbeTextureHeight / 2;

	int endCellX = m_Info.CamCellX + m_Settings.ProbeTextureWidth / 2;
	int endCellY = m_Info.CamCellY + m_Settings.ProbeTextureDepth / 2;
	int endCellZ = m_Info.CamCellZ + m_Settings.ProbeTextureHeight / 2;

	int startTileX = ( startCellX - m_Settings.MaximumProbeRadius ) / m_Info.ProbeCellsInTileX;
	int endTileX = ( endCellX + m_Settings.MaximumProbeRadius ) / m_Info.ProbeCellsInTileX;

	int startTileZ = ( startCellZ - m_Settings.MaximumProbeRadius ) / m_Info.ProbeCellsInTileZ;
	int endTileZ = ( endCellZ + m_Settings.MaximumProbeRadius ) / m_Info.ProbeCellsInTileZ;

	for( int tz = startTileZ, e = endTileZ; tz <= e ; tz ++ )
	{
		for( int tx = startTileX, e = endTileX; tx <= e ; tx ++ )
		{
			if( tx < 0 || tx >= (int)m_ProbeMap.Width() ||
				tz < 0 || tz >= (int)m_ProbeMap.Height() )
				continue;

			Probes& probes = m_ProbeMap[ tz ][ tx ].TheProbes;

			for( int i = 0, e = (int)probes.Count() ; i < e ; i ++ )
			{
				Probe& probe = probes[ i ];

				probe.DynamicLightsRGB.x = 0;
				probe.DynamicLightsRGB.y = 0;
				probe.DynamicLightsRGB.z = 0;
			}
		}
	}

	int hasDirty = 0;

	for (uint32_t i = 0, i_end = WorldLightSystem.Lights.Count(); i != i_end; ++i)
	{
		r3dLight *l = WorldLightSystem.Lights[i];
		if (!l || l->Type != R3D_OMNI_LIGHT ) continue;

		float outerR = l->GetOuterRadius();

		if( l->x + outerR < m_Info.ProbeMapWorldXStart
				||
			l->x - outerR > m_Info.ProbeMapWorldXStart + m_Info.ProbeMapWorldActualXSize
				||
			l->y + outerR < m_Info.ProbeMapWorldYStart
				||
			l->y - outerR > m_Info.ProbeMapWorldYStart + m_Info.ProbeMapWorldNominalYSize
				||
			l->z + outerR < m_Info.ProbeMapWorldZStart
				||
			l->z - outerR > m_Info.ProbeMapWorldZStart + m_Info.ProbeMapWorldActualZSize			
			)
		{
			l->InProbesVolumeIntensity = -1.0f;
			continue;
		}

		hasDirty |= DistributeLightToProbes( l );
	}

	return hasDirty;
}

//------------------------------------------------------------------------

static float g_LastTick = -1.f;

void ProbeMaster::Tick()
{
	if( g_LastTick < 0 )
		g_LastTick = r3dGetTime();

	float newTick = r3dGetTime();

	float deltaTick = newTick - g_LastTick;

	g_LastTick = newTick;

	for( int i = 0, e = m_ProbeVolumeDirtyAnimArr.Count(); i < e; i ++ )
	{
		int& val = m_ProbeVolumeDirtyAnimArr.AtIndex( i );

		val -= R3D_MAX( int(deltaTick * 256), 1 );

		if( val < 0 ) val = 0;
	}
}

//------------------------------------------------------------------------

void ProbeMaster::Test()
{
	ClearProbeMap();

	AddProbe( gCam, true, NULL );
}

//------------------------------------------------------------------------

void ProbeMaster::Create( float startX, float startY, float startZ, float sizeX, float sizeY, float sizeZ )
{
	m_Info.ProbeMapWorldNominalXSize = sizeX;
	m_Info.ProbeMapWorldNominalYSize = sizeY;
	m_Info.ProbeMapWorldNominalZSize = sizeZ;

	m_Info.ProbeMapWorldXStart = startX;
	m_Info.ProbeMapWorldYStart = startY;
	m_Info.ProbeMapWorldZStart = startZ;

	UpdateCellsAndSizeParams();

	m_ProbeMap.Resize( m_Info.ActualProbeTileCountX, m_Info.ActualProbeTileCountZ );

	ResetProbeMapBBoxes();

	// world render resources
	RecreateVolumeTexes();

	m_Created = 1;
}

//------------------------------------------------------------------------

void ProbeMaster::Reset()
{
	DeleteAllProbes();

	m_Created = 0;
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateProbeRadiuses()
{
	if( m_ProbeRadiusesDirty )
	{
		AdjustProbeRadiuses();
		m_ProbeRadiusesDirty = 0;
	}
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateSkyVisibility( int DirtyOnly )
{
	StartUpdatingSkyVisibility();

	int total = DirtyOnly ? GetDirtyProbeCount( Probe::FLAG_SKY_DIRTY ) : GetProbeCount();
	int complete = 0;

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			Probes& probes = m_ProbeMap.At( x, z ).TheProbes;

			for( int i = 0, e = probes.Count(); i < e; i ++ )
			{
				Probe* probe = &probes[ i ];

				if( !DirtyOnly || probe->Flags & Probe::FLAG_SKY_DIRTY )
				{
					UpdateProbeSkyVisibility( probe );
					probe->Flags &= ~Probe::FLAG_SKY_DIRTY;
					complete ++;
				}

				if( r3dGetTime() - m_LastInfoFrame > 0.033f && total > 1 )
				{
					m_LastInfoFrame = r3dGetTime();

					StopUpdatingSkyVisibility();
					OutputBakeProgress( "Sky Visibility", total, complete );
					StartUpdatingSkyVisibility();
				}
			}
		}
	}

	StopUpdatingSkyVisibility();
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateBounce( int DirtyOnly )
{
	int total = DirtyOnly ? GetDirtyProbeCount( Probe::FLAG_BOUNCE_DIRTY ) : GetProbeCount();
	int complete = 0;

	StartUpdatingBounce();

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			Probes& probes = m_ProbeMap.At( x, z ).TheProbes;

			for( int i = 0, e = probes.Count(); i < e; i ++ )
			{
				if( r3dGetTime() - m_LastInfoFrame > 0.033f && total > 1 )
				{
					m_LastInfoFrame = r3dGetTime();

					StopUpdatingBounce();
					OutputBakeProgress( "Bounces", total, complete );
					StartUpdatingBounce();
				}

				Probe* probe = &probes[ i ];

				if( !DirtyOnly || probe->Flags & Probe::FLAG_BOUNCE_DIRTY )
				{
					UpdateProbeBounce( probe );
					complete ++;
				}
			}
		}
	}

	StopUpdatingBounce();

	//------------------------------------------------------------------------

	total = DirtyOnly ? GetDirtyProbeCount( Probe::FLAG_LOCAL_BOUNCE_DIRTY ) : GetProbeCount();

	StartUpdatingLocalVPLBounce();

	complete = 0;

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			Probes& probes = m_ProbeMap.At( x, z ).TheProbes;

			for( int i = 0, e = probes.Count(); i < e; i ++ )
			{
				if( r3dGetTime() - m_LastInfoFrame > 0.033f && total > 1 )
				{
					m_LastInfoFrame = r3dGetTime();

					StopUpdatingLocalVPLBounce();
					OutputBakeProgress( "Local Bounces", total, complete );
					StartUpdatingLocalVPLBounce();
				}

				Probe* probe = &probes[ i ];

				if( !DirtyOnly || probe->Flags & Probe::FLAG_LOCAL_BOUNCE_DIRTY )
				{
					UpdateLocalVPLBounce( probe );
					complete ++;
				}
			}
		}
	}

	StopUpdatingLocalVPLBounce();

	//------------------------------------------------------------------------

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			Probes& probes = m_ProbeMap.At( x, z ).TheProbes;

			for( int i = 0, e = probes.Count(); i < e; i ++ )
			{
				probes[ i ].Flags &= ~( Probe::FLAG_BOUNCE_DIRTY | Probe::FLAG_LOCAL_BOUNCE_DIRTY );
			}
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::ShowProbes( ProbeVisualizationMode mode )
{
	D3DPERF_BeginEvent( 0, L"ProbeMaster::ShowProbes" );

	m_VisibleProbeTileArray.Clear();

	StartProbesVisualization( mode );

	float lenX = m_Info.ProbeMapWorldActualXSize / m_ProbeMap.Width();
	float lenZ = m_Info.ProbeMapWorldActualZSize / m_ProbeMap.Height();

	float cellLen_x2 = 2 * sqrtf( lenX * lenX + lenZ * lenZ );

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			const ProbeTile& en = m_ProbeMap.At( x, z );

			if( ( en.BBox.Org - gCam ).Length() >= r_show_probes_radius->GetFloat() + cellLen_x2 )
				continue;

			if( !r3dRenderer->IsBoxInsideFrustum( en.BBox ) )
				continue;

			if( !en.TheProbes.Count() )
				continue;

			m_VisibleProbeTileArray.PushBack( &en );

			const Probes& probes = en.TheProbes;

			for( int i = 0, e = probes.Count(); i < e; i ++ )
			{
				const Probe* p = &probes[ i ];

				if( ( p->Position - gCam ).Length() < r_show_probes_radius->GetFloat() )
				{
					VisualizeProbe( p );
				}
			}
		}
	}

	StopProbesVisualization();

	D3DPERF_EndEvent();

	bool needProbeArrows = mode == VISUALIZE_SKY_VISIBILITY
										||
									mode == VISUALIZE_SKY_SH_MUL_VISIBILITY
										||
									mode >= VISUALIZE_BOUNCE_DIR0 && mode <= VISUALIZE_BOUNCE_LAST
										||
									mode == VISUALIZE_PROBE_STATIC_COLORS;

	// draw arrows & boxes
	if( needProbeArrows || r_show_probe_boxes->GetInt() )
	{
		D3DPERF_BeginEvent( 0, L"ProbeMaster::ShowProbes.Arrows" );

		StartProbeDirections();

		if( needProbeArrows )
		{
			for( int i = 0, e = m_VisibleProbeTileArray.Count(); i < e; i ++ )
			{
				const ProbeTile* pe = m_VisibleProbeTileArray[ i ];

				for( int i = 0, e = pe->TheProbes.Count(); i < e; i ++ )
				{
					const Probe& p = pe->TheProbes[ i ];

					r3dPoint3D probePos ( p.Position.x, p.Position.y, p.Position.z );

					if( ( gCam - probePos ).Length() > 32.f )
						continue;

					DrawProbeDirections( &p );
				}
			}
		}

		if( r_show_probe_boxes->GetInt() )
		{
			float psConst[ 4 ] = { 0, 1, 0, 1 };
			D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, psConst, 1 ) );

			for( int z = 0, e = (int)m_ProbeMap.Height(); z < e; z ++ )
			{
				for( int x = 0, e = (int)m_ProbeMap.Width(); x < e; x ++ )
				{
					r3dDrawUniformBoundBox( m_ProbeMap[ z ][ x ].BBox, gCam, r3dColor::green );
				}
			}
		}

		StopProbeDirections();

		D3DPERF_EndEvent();
	}

	m_VisibleProbeTileArray.Clear();
}

//------------------------------------------------------------------------

void ProbeMaster::ShowProbeVolumesScheme()
{
	extern int VS_FWD_COLOR_ID;
	extern int PS_FWD_COLOR_ID;

	r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW | R3D_BLEND_PUSH );

	r3dRenderer->SetCullMode( D3DCULL_CCW );

	r3dRenderer->SetVertexShader( VS_FWD_COLOR_ID );
	r3dRenderer->SetPixelShader( PS_FWD_COLOR_ID );

	D3DXMATRIX mtx = r3dRenderer->ViewProjMatrix;
	D3DXMatrixTranspose( &mtx, &mtx );
	D3D_V( r3dRenderer->pd3ddev->SetVertexShaderConstantF( 0, &mtx._11, 4 ) );

	r3dRenderer->SetRenderingMode( R3D_BLEND_ALPHA | R3D_BLEND_NZ );

	float vColTr[ 4 ] = { 0.5f, 0.5f, 0.5f, 0.75f };
	D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, vColTr, 1 ) );

	DrawProbeVolumesFrame();

	r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW );

	float vColOp[ 4 ] = { 0.f, 1.f, 0.f, 1.f };
	D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, vColOp, 1 ) );

	DrawProbeVolumesFrame();
	
	r3dRenderer->SetRenderingMode( R3D_BLEND_POP );

	r3dRenderer->RestoreCullMode();

	r3dRenderer->SetVertexShader();
	r3dRenderer->SetPixelShader();
}

//------------------------------------------------------------------------

r3dScreenBuffer* ProbeMaster::GetBounceRT() const
{
	return m_BounceDiffuseRT;
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateProbeVolumes()
{
#ifndef FINAL_BUILD
#if 0
	struct ShowTime
	{
		ShowTime()
		{
			start = r3dGetTime();
		}

		~ShowTime()
		{
			r3dOutToLog( "ProbeMaster::UpdateProbeVolumesWithRasterization: took %.3f msecs to execute\n", ( r3dGetTime() - start ) * 1000.f );
		}

		float start;

	} showTime; (void)showTime;
#endif
#endif

	if( abs( m_PrevCamCellX - m_Info.CamCellX ) >= m_Settings.ProbeTextureWidth / 2 
			||
		abs( m_PrevCamCellY - m_Info.CamCellY ) >= m_Settings.ProbeTextureDepth / 2 
			||
		abs( m_PrevCamCellZ - m_Info.CamCellZ ) >= m_Settings.ProbeTextureHeight / 2 
			||
		!r_lp_fast_update->GetInt()
		)
	{
		// no reason to optimize updates if we're that far
		m_ProbeVolumeDirty = 1;
	}

	if( m_ProbeVolumeDirty )
	{
		memset(	m_ProbeVolumeDirtyArr.GetDataPtr(), 
				0,
				m_ProbeVolumeDirtyArr.Count() * sizeof( m_ProbeVolumeDirtyArr[0][0] ) );

		if( m_ProbeVolumeDirtyAnimArr.Count() )
		{
			for( int i = 0, e = m_ProbeVolumeDirtyAnimArr.Count(); i < e; i ++ )
			{
				m_ProbeVolumeDirtyAnimArr.AtIndex( i ) = 255;
			}
		}

		m_Info.ProbeVolumeCamCellX = m_Settings.ProbeTextureWidth / 2;
		m_Info.ProbeVolumeCamCellY = m_Settings.ProbeTextureDepth / 2;
		m_Info.ProbeVolumeCamCellZ = m_Settings.ProbeTextureHeight / 2;

		m_PrevCamCellX = m_Info.CamCellX;
		m_PrevCamCellY = m_Info.CamCellY;
		m_PrevCamCellZ = m_Info.CamCellZ;

		D3DLOCKED_BOX lboxes[ Probe::NUM_DIRS ];

		UINT32 *locked32[ Probe::NUM_DIRS ];

		for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
		{
			r3dTexture* tex = m_DirectionVolumeTextures[ i ];
			IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

			D3D_V( volTex->LockBox( 0, &lboxes[ i ], NULL, 0 ) );

			locked32[ i ] = (UINT32*)lboxes[ i ].pBits;
		}

		int lockedSize = lboxes[ 0 ].SlicePitch * m_DirectionVolumeTextures[ 0 ]->GetDepth();

		int rasterizedCount = 0;

		{
			m_ProbeRasterSamplesVolume.Resize( m_Settings.ProbeTextureWidth * m_Settings.ProbeTextureHeight * m_Settings.ProbeTextureDepth );
			ResetRasterSamples();

			int centreCellX = m_Info.CamCellX;
			int centreCellY = m_Info.CamCellY;
			int centreCellZ = m_Info.CamCellZ;

			int startCellX = centreCellX - m_Settings.ProbeTextureWidth / 2;
			int startCellY = centreCellY - m_Settings.ProbeTextureDepth / 2;
			int startCellZ = centreCellZ - m_Settings.ProbeTextureHeight / 2;

			int endCellX = centreCellX + m_Settings.ProbeTextureWidth / 2;
			int endCellY = centreCellY + m_Settings.ProbeTextureDepth / 2;
			int endCellZ = centreCellZ + m_Settings.ProbeTextureHeight / 2;

			int startTileX = ( startCellX - m_Settings.MaximumProbeRadius ) / m_Info.ProbeCellsInTileX;
			int endTileX = ( endCellX + m_Settings.MaximumProbeRadius ) / m_Info.ProbeCellsInTileX;

			int startTileZ = ( startCellZ - m_Settings.MaximumProbeRadius ) / m_Info.ProbeCellsInTileZ;
			int endTileZ = ( endCellZ + m_Settings.MaximumProbeRadius ) / m_Info.ProbeCellsInTileZ;

			int tilesInRad = m_Settings.MaximumProbeRadius / m_Info.ProbeCellsInTileX + 1;

			startTileX -= tilesInRad;
			endTileX += tilesInRad;

			startTileZ -= tilesInRad;
			endTileZ += tilesInRad;

			D3DBOX box;

			box.Left = 0;
			box.Right = m_Settings.ProbeTextureWidth;

			box.Top = 0;
			box.Bottom = m_Settings.ProbeTextureHeight;

			box.Front = 0;
			box.Back = m_Settings.ProbeTextureDepth;

			for( int y = 0, cy = centreCellY - m_Settings.ProbeTextureDepth / 2, e = m_Settings.ProbeTextureDepth; y < e; y ++, cy ++ )
			{
				for( int z = 0, e = m_Settings.ProbeTextureHeight, cz = centreCellZ - m_Settings.ProbeTextureHeight / 2; z < e; z ++, cz ++ )
				{
					for( int x = 0, cx = centreCellX - m_Settings.ProbeTextureWidth / 2, e = m_Settings.ProbeTextureWidth; x < e; x ++, cx ++ )
					{
						FillProbeVolumeCellWithProbe( lboxes, box, cx, cy, cz, m_DefaultProbe );
					}
				}
			}

			for( int tz = startTileZ, te = endTileZ; tz < te; tz ++ )
			{
				if( tz < 0 || tz >= (int)m_ProbeMap.Height() )
					continue;

				for( int tx = startTileX, te = endTileX; tx < te; tx ++ )
				{
					if( tx < 0 || tx >= (int)m_ProbeMap.Width() )
						continue;

					ProbeTile& tile = m_ProbeMap[ tz ][ tx ];

					for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
					{
						Probe& probe = tile.TheProbes[ i ];

						if( probe.CellX + probe.XRadius > startCellX
								&&
							probe.CellX - probe.XRadius <= endCellX
								&&
							probe.CellZ + probe.ZRadius > startCellZ
								&&
							probe.CellZ - probe.ZRadius <= endCellZ
								&&
							probe.CellY + probe.YRadius > startCellY
								&&
							probe.CellY - probe.YRadius <= endCellY
							)
						{
							if( m_DEBUG_EndProbe < 0 || rasterizedCount ++ <= m_DEBUG_EndProbe )
							{
								RasterizeProbeIntoVolume( &probe, box, startCellX, startCellY, startCellZ, m_Settings.ProbeTextureWidth, m_Settings.ProbeTextureDepth, m_Settings.ProbeTextureHeight );
							}
						}
					}
				}
			}
		}

		m_RasterizedProbeCount = rasterizedCount;

		//------------------------------------------------------------------------

		CopySamplesIntoLockedTextures( locked32, lboxes[ 0 ].RowPitch, lboxes[ 0 ].SlicePitch, m_Settings.ProbeTextureWidth, m_Settings.ProbeTextureDepth, m_Settings.ProbeTextureHeight );

		//------------------------------------------------------------------------

		for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e; i ++ )
		{
			r3dTexture* tex = m_DirectionVolumeTextures[ i ];
			IDirect3DVolumeTexture9* volTex = tex->AsTexVolume();

			D3D_V( volTex->UnlockBox( 0 ) );
		}
	}
	else
	{
		int XUpdateStart;
		int XUpdateEnd;

		int YUpdateStart;
		int YUpdateEnd;

		int ZUpdateStart;
		int ZUpdateEnd;

		int hw = m_Settings.ProbeTextureWidth / 2;
		int hh = m_Settings.ProbeTextureHeight / 2;
		int hd = m_Settings.ProbeTextureDepth / 2;

		if( m_Info.CamCellX > m_PrevCamCellX )
		{				
			XUpdateStart = m_PrevCamCellX + hw;
			XUpdateEnd = m_Info.CamCellX + hw;
		}
		else
		{
			XUpdateStart = m_Info.CamCellX - hw;
			XUpdateEnd = m_PrevCamCellX - hw;
		}

		if( m_Info.CamCellY > m_PrevCamCellY )
		{
			YUpdateStart = m_PrevCamCellY + hd;
			YUpdateEnd = m_Info.CamCellY + hd;
		}
		else
		{
			YUpdateStart = m_Info.CamCellY - hd;
			YUpdateEnd = m_PrevCamCellY - hd;
		}

		if( m_Info.CamCellZ > m_PrevCamCellZ )
		{
			ZUpdateStart = m_PrevCamCellZ + hh;
			ZUpdateEnd = m_Info.CamCellZ + hh;
		}
		else
		{
			ZUpdateStart = m_Info.CamCellZ - hh;
			ZUpdateEnd = m_PrevCamCellZ - hh;
		}

		m_Info.ProbeVolumeCamCellX += m_Info.CamCellX - m_PrevCamCellX;
		m_Info.ProbeVolumeCamCellY += m_Info.CamCellY - m_PrevCamCellY;
		m_Info.ProbeVolumeCamCellZ += m_Info.CamCellZ - m_PrevCamCellZ;

		m_Info.ProbeVolumeCamCellX %= m_Settings.ProbeTextureWidth;
		m_Info.ProbeVolumeCamCellY %= m_Settings.ProbeTextureDepth;
		m_Info.ProbeVolumeCamCellZ %= m_Settings.ProbeTextureHeight;

		if( m_Info.ProbeVolumeCamCellX < 0 ) m_Info.ProbeVolumeCamCellX += m_Settings.ProbeTextureWidth;
		if( m_Info.ProbeVolumeCamCellY < 0 ) m_Info.ProbeVolumeCamCellY += m_Settings.ProbeTextureDepth;
		if( m_Info.ProbeVolumeCamCellZ < 0 ) m_Info.ProbeVolumeCamCellZ += m_Settings.ProbeTextureHeight;

		//------------------------------------------------------------------------

		if( XUpdateStart != XUpdateEnd )
		{
			int actualXStart = ( XUpdateStart - m_Info.CamCellX + m_Info.ProbeVolumeCamCellX ) % m_Settings.ProbeTextureWidth;
			int actualXEnd = ( XUpdateEnd - m_Info.CamCellX + m_Info.ProbeVolumeCamCellX ) % m_Settings.ProbeTextureWidth;

			if( actualXStart < 0 ) actualXStart += m_Settings.ProbeTextureWidth;
			if( actualXEnd < 0 ) actualXEnd += m_Settings.ProbeTextureWidth;
			else
				if( !actualXEnd ) actualXEnd = m_Settings.ProbeTextureWidth;

			if( actualXStart > actualXEnd )
			{
				RasterizeXSlice( actualXStart, m_Settings.ProbeTextureWidth, XUpdateStart, m_Info.CamCellY, m_Info.CamCellZ, m_DefaultProbe );
				RasterizeXSlice( 0, actualXEnd, XUpdateStart + m_Settings.ProbeTextureWidth - actualXStart, m_Info.CamCellY, m_Info.CamCellZ, m_DefaultProbe );
			}
			else
			{
				RasterizeXSlice( actualXStart, actualXEnd, XUpdateStart, m_Info.CamCellY, m_Info.CamCellZ, m_DefaultProbe );
			}
		}

		//------------------------------------------------------------------------

		if( YUpdateStart != YUpdateEnd )
		{
			int actualYStart = ( YUpdateStart - m_Info.CamCellY + m_Info.ProbeVolumeCamCellY ) % m_Settings.ProbeTextureDepth;
			int actualYEnd = ( YUpdateEnd - m_Info.CamCellY + m_Info.ProbeVolumeCamCellY ) % m_Settings.ProbeTextureDepth;

			if( actualYStart < 0 ) actualYStart += m_Settings.ProbeTextureDepth;
			if( actualYEnd < 0 ) actualYEnd += m_Settings.ProbeTextureDepth;
			else
				if( !actualYEnd ) actualYEnd = m_Settings.ProbeTextureDepth;

			if( actualYStart > actualYEnd )
			{
				RasterizeYSlice( actualYStart, m_Settings.ProbeTextureDepth, m_Info.CamCellX, YUpdateStart, m_Info.CamCellZ, m_DefaultProbe );
				RasterizeYSlice( 0, actualYEnd, m_Info.CamCellX, YUpdateStart + m_Settings.ProbeTextureDepth - actualYStart, m_Info.CamCellZ, m_DefaultProbe );
			}
			else
			{
				RasterizeYSlice( actualYStart, actualYEnd, m_Info.CamCellX, YUpdateStart, m_Info.CamCellZ, m_DefaultProbe );
			}
		}

		//------------------------------------------------------------------------

		if( ZUpdateStart != ZUpdateEnd )
		{
			int actualZStart = ( ZUpdateStart - m_Info.CamCellZ + m_Info.ProbeVolumeCamCellZ ) % m_Settings.ProbeTextureHeight;
			int actualZEnd = ( ZUpdateEnd - m_Info.CamCellZ + m_Info.ProbeVolumeCamCellZ ) % m_Settings.ProbeTextureHeight;

			if( actualZStart < 0 ) actualZStart += m_Settings.ProbeTextureHeight;

			if( actualZEnd < 0 ) actualZEnd += m_Settings.ProbeTextureHeight;
			else
				if( !actualZEnd ) actualZEnd = m_Settings.ProbeTextureHeight;

			if( actualZStart > actualZEnd )
			{
				RasterizeZSlice( actualZStart, m_Settings.ProbeTextureHeight, m_Info.CamCellX, m_Info.CamCellY, ZUpdateStart, m_DefaultProbe );
				RasterizeZSlice( 0, actualZEnd, m_Info.CamCellX, m_Info.CamCellY, ZUpdateStart + m_Settings.ProbeTextureHeight - actualZStart, m_DefaultProbe );
			}
			else
			{
				RasterizeZSlice( actualZStart, actualZEnd, m_Info.CamCellX, m_Info.CamCellY, ZUpdateStart, m_DefaultProbe );
			}
		}

		//------------------------------------------------------------------------

		for( int tz = 0, e = m_ProbeVolumeDirtyArr.Height(); tz < e; tz ++ )
		{
			for( int tx = 0, e = m_ProbeVolumeDirtyArr.Width(); tx < e; tx ++ )
			{
				int* dirtyFlag = &m_ProbeVolumeDirtyArr[ tz ][ tx ];

				if( *dirtyFlag )
				{
					int tileXUpdateStart = m_Info.CamCellX - hw + tx * PROBE_DIRTY_ARR_CELL_SIZE;
					int tileXUpdateEnd = tileXUpdateStart + PROBE_DIRTY_ARR_CELL_SIZE;

					int tileZUpdateStart = m_Info.CamCellZ - hh + tz * PROBE_DIRTY_ARR_CELL_SIZE;
					int tileZUpdateEnd = tileZUpdateStart + PROBE_DIRTY_ARR_CELL_SIZE;

					int actualXStart = ( tileXUpdateStart - m_Info.CamCellX + m_Info.ProbeVolumeCamCellX ) % m_Settings.ProbeTextureWidth;
					int actualXEnd = ( tileXUpdateEnd - m_Info.CamCellX + m_Info.ProbeVolumeCamCellX ) % m_Settings.ProbeTextureWidth;

					if( actualXStart < 0 ) actualXStart += m_Settings.ProbeTextureWidth;
					if( actualXEnd < 0 ) actualXEnd += m_Settings.ProbeTextureWidth;
					else
						if( !actualXEnd ) actualXEnd = m_Settings.ProbeTextureWidth;

					int actualZStart = ( tileZUpdateStart - m_Info.CamCellZ + m_Info.ProbeVolumeCamCellZ ) % m_Settings.ProbeTextureHeight;
					int actualZEnd = ( tileZUpdateEnd - m_Info.CamCellZ + m_Info.ProbeVolumeCamCellZ ) % m_Settings.ProbeTextureHeight;

					if( actualZStart < 0 ) actualZStart += m_Settings.ProbeTextureHeight;

					if( actualZEnd < 0 ) actualZEnd += m_Settings.ProbeTextureHeight;
					else
						if( !actualZEnd ) actualZEnd = m_Settings.ProbeTextureHeight;

					if( actualZStart > actualZEnd 
							&&
						actualXStart > actualXEnd
						)
					{
						RasterizeDirtyRect( 
							actualXStart, m_Settings.ProbeTextureWidth,
							actualZStart, m_Settings.ProbeTextureHeight,
							tileXUpdateStart, m_Info.CamCellY, tileZUpdateStart,
							m_DefaultProbe );

						RasterizeDirtyRect( 
							0, actualXStart,
							0, actualZStart,
							tileXUpdateStart + m_Settings.ProbeTextureWidth - actualXStart,
							m_Info.CamCellY, 
							tileZUpdateStart + m_Settings.ProbeTextureHeight - actualZStart,
							m_DefaultProbe );

						RasterizeDirtyRect( 
							actualXStart, m_Settings.ProbeTextureWidth,
							0, actualZStart,
							tileXUpdateStart,
							m_Info.CamCellY, 
							tileZUpdateStart + m_Settings.ProbeTextureHeight - actualZStart,
							m_DefaultProbe );

						RasterizeDirtyRect( 
							0, actualXStart,
							actualZStart, m_Settings.ProbeTextureHeight,
							tileXUpdateStart + m_Settings.ProbeTextureWidth - actualXStart,
							m_Info.CamCellY, 
							tileZUpdateStart,
							m_DefaultProbe );
					}
					else
					{
						if( actualZStart > actualZEnd )
						{
							RasterizeDirtyRect(
								actualXStart, actualXEnd,
								actualZStart, m_Settings.ProbeTextureHeight,
								tileXUpdateStart, m_Info.CamCellY, tileZUpdateStart,
								m_DefaultProbe );

							RasterizeDirtyRect(
								actualXStart, actualXEnd,
								0, actualZStart,
								tileXUpdateStart, m_Info.CamCellY, 
								tileZUpdateStart + m_Settings.ProbeTextureHeight - actualZStart,
								m_DefaultProbe );
						}
						else
						{
							if( actualXStart > actualXEnd )
							{
								RasterizeDirtyRect( 
									actualXStart, m_Settings.ProbeTextureWidth,
									actualZStart, actualZEnd,
									tileXUpdateStart, m_Info.CamCellY, tileZUpdateStart,
									m_DefaultProbe );

								RasterizeDirtyRect( 
									0, actualXStart,
									actualZStart, actualZEnd,
									tileXUpdateStart + m_Settings.ProbeTextureWidth - actualXStart,
									m_Info.CamCellY, 
									tileZUpdateStart,
									m_DefaultProbe );
							}
							else
							{
								RasterizeDirtyRect( 
									actualXStart, actualXEnd,
									actualZStart, actualZEnd,
									tileXUpdateStart, m_Info.CamCellY, tileZUpdateStart,
									m_DefaultProbe );
							}
						}
					}

					*dirtyFlag = 0;
				}
			}
		}
	}

	m_PrevCamCellX = m_Info.CamCellX;
	m_PrevCamCellY = m_Info.CamCellY;
	m_PrevCamCellZ = m_Info.CamCellZ;

	m_ProbeVolumeDirty = 0;
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateSkyAndSun()
{
	struct EnterExit
	{
		EnterExit( ProbeMaster* parent )
		: Parent( parent )
		{
			D3DPERF_BeginEvent( 0, L"ProbeMaster::UpdateSky" );

			if( !Parent->m_EditorMode )
				Parent->CreateTempRTAndSysMemTex();

			D3D_V( Parent->m_SysmemTex->GetSurfaceLevel( 0, &sysMemSurf ) );

			r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_NZ | R3D_BLEND_PUSH );

			savedCam = gCam;
		}

		~EnterExit()
		{
			if( !Parent->m_EditorMode )
				Parent->DestroyTempRTAndSysMemTex();

			sysMemSurf->Release();

			r3dRenderer->SetRenderingMode( R3D_BLEND_POP );

			gCam = savedCam;

			D3DPERF_EndEvent();
		}

		r3dCamera savedCam;

		ProbeMaster *Parent;
		IDirect3DSurface9* sysMemSurf;
	} enterExit( this ); (void)enterExit;

	m_TempRT->Activate();

	memset( &m_RTBytesR[ 0 ], 0, m_RTBytesR.Count() );
	memset( &m_RTBytesG[ 0 ], 0, m_RTBytesG.Count() );
	memset( &m_RTBytesB[ 0 ], 0, m_RTBytesB.Count() );

	for( int f = 0; f < 6; f ++ )
	{
		D3D_V( r3dRenderer->pd3ddev->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000, r3dRenderer->GetClearZValue(), 0 ) );

		// let it safely be black
		if( f == D3DCUBEMAP_FACE_NEGATIVE_Y )
			continue;

		SetupCamForFace( &gCam, r3dPoint3D( 0, 0, 0 ), f );

		r3dRenderer->SetCamera( gCam, false );

		void DrawSkyDome( bool drawNormals, bool hemisphere );

		DrawSkyDome( false, true );
		
		D3D_V( r3dRenderer->pd3ddev->GetRenderTargetData( m_TempRT->GetTex2DSurface(), enterExit.sysMemSurf ) );

		D3DLOCKED_RECT lrect;
		m_SysmemTex->LockRect( 0, &lrect, NULL, D3DLOCK_READONLY );

		CopyPixelsRGB( &m_RTBytesR, &m_RTBytesG, &m_RTBytesB, lrect.pBits, f );
		m_SysmemTex->UnlockRect( 0 );
	}

	m_TempRT->Deactivate();

	float SH[ 4 ];

	g_SampleSphericalSource = &m_RTBytesR;
	ProjectOnSH( SH, SampleSpherical );
	memcpy( &m_SkyDomeSH_R, SH, sizeof m_SkyDomeSH_R );

	g_SampleSphericalSource = &m_RTBytesG;
	ProjectOnSH( SH, SampleSpherical );
	memcpy( &m_SkyDomeSH_G, SH, sizeof m_SkyDomeSH_G );

	g_SampleSphericalSource = &m_RTBytesB;
	ProjectOnSH( SH, SampleSpherical );
	memcpy( &m_SkyDomeSH_B, SH, sizeof m_SkyDomeSH_B );

	//------------------------------------------------------------------------
	// The Sun 

	extern r3dSun* Sun;

	D3DXVECTOR3 vSun ( -Sun->SunDir.x, -Sun->SunDir.z, -Sun->SunDir.y );

	float SH_R[ 4 ], SH_G[ 4 ], SH_B[ 4 ];

	D3DXSHEvalDirectionalLight( 2, &vSun,	Sun->SunLight.R / 255.f,
											Sun->SunLight.G / 255.f,
											Sun->SunLight.B / 255.f,
											SH_R, SH_G, SH_B );

	memcpy( &m_SunSH_R.x, SH_R, sizeof m_SunSH_R );
	memcpy( &m_SunSH_G.x, SH_G, sizeof m_SunSH_G );
	memcpy( &m_SunSH_B.x, SH_B, sizeof m_SunSH_B );

	//------------------------------------------------------------------------

	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		float skyDirectR = D3DXVec4Dot( &m_SkyDomeSH_R, &m_BasisSHArray[ i ] );
		float skyDirectG = D3DXVec4Dot( &m_SkyDomeSH_G, &m_BasisSHArray[ i ] );
		float skyDirectB = D3DXVec4Dot( &m_SkyDomeSH_B, &m_BasisSHArray[ i ] );

		m_SkyDirColors.DirColor[ i ] = D3DXVECTOR4( skyDirectR, skyDirectG, skyDirectB, 0.f );
	}
}

//------------------------------------------------------------------------

void ProbeMaster::RelightProbes()
{
	m_SkyVisA = 0.0f;
	m_SkyVisB = 1.0f;

	if( r_sky_vis_affects_bounces->GetInt() )
	{
		m_SkyVisA = 1.0f;
		m_SkyVisB = 0.0f;
	}

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			Probes& probes = m_ProbeMap.At( x, z ).TheProbes;

			for( int i = 0, e = probes.Count(); i < e; i ++ )
			{
				RelightProbe( &probes[ i ] );
			}
		}
	}

	RelightProbe( &m_DefaultProbe );

	m_ProbeVolumeDirty = 1;
}

//------------------------------------------------------------------------

void ProbeMaster::RelightDynamicProbes()
{
	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			Probes& probes = m_ProbeMap.At( x, z ).TheProbes;

			for( int i = 0, e = probes.Count(); i < e; i ++ )
			{
				RelightDynamicProbe( &probes[ i ] );
			}
		}
	}
}

//------------------------------------------------------------------------

r3dTexture* ProbeMaster::GetProbeTexture( int direction ) const
{
	return m_DirectionVolumeTextures[ direction ];
}

//------------------------------------------------------------------------

const ProbeMaster::Settings& ProbeMaster::GetSettings() const
{
	return m_Settings;
}

//------------------------------------------------------------------------

const ProbeMaster::Info& ProbeMaster::GetInfo() const
{
	return m_Info ;
}

//------------------------------------------------------------------------

ProbeMaster::MemStats ProbeMaster::GetMemStats() const
{
	MemStats stats;

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			const ProbeTile& tile = m_ProbeMap[ z ][ x ];

			stats.ProbeSize += tile.TheProbes.Count() * sizeof( Probe );
		}
	}

	for( int i = 0, e = m_DirectionVolumeTextures.COUNT; i < e ; i ++ )
	{
		stats.ProbeVolumeSize += m_DirectionVolumeTextures[ i ]->GetTextureSizeInVideoMemory();
	}

	return stats;
}

//------------------------------------------------------------------------

void ProbeMaster::SetSettings( const Settings& settings )
{
	bool needUpdateProbeMap = false;
	bool needUpdateProbesTextures = false;
	bool needUpdateProbeRadiuses = false;

	if( m_Settings.ProbeTextureSpanX != settings.ProbeTextureSpanX
			||
		m_Settings.ProbeTextureSpanY != settings.ProbeTextureSpanY
			||
		m_Settings.ProbeTextureSpanZ != settings.ProbeTextureSpanZ )
	{
		needUpdateProbeMap = true;
	}

	if( m_Settings.ProbeTextureWidth != settings.ProbeTextureWidth 
			||
		m_Settings.ProbeTextureHeight != settings.ProbeTextureHeight
			||
		m_Settings.ProbeTextureDepth != settings.ProbeTextureDepth
		)
	{
		r3d_assert( ! ( settings.ProbeTextureWidth & 1 ) );
		r3d_assert( ! ( settings.ProbeTextureHeight & 1 ) );
		r3d_assert( ! ( settings.ProbeTextureDepth & 1 ) );

		needUpdateProbeMap = true;
		needUpdateProbesTextures = true ;
	}

	if( m_Settings.MaximumProbeRadius != settings.MaximumProbeRadius
			||
		m_Settings.MaximumProbeYRadius !=settings.MaximumProbeYRadius )
	{
		needUpdateProbeRadiuses = true;
		needUpdateProbesTextures = true;
	}

	m_Settings = settings;

	if( m_Created )
	{
		if( needUpdateProbeMap )
		{
			m_ProbeVolumeDirty = 1;

			Probes probes;

			FetchAllProbes( &probes );

			UpdateCellsAndSizeParams();

			m_ProbeMap.Resize( m_Info.ActualProbeTileCountX, m_Info.ActualProbeTileCountZ );

			ClearProbeMap();
			ResetProbeMapBBoxes();

			for( int i = 0, e = (int)probes.Count(); i < e ; i ++ )
			{
				AddProbe( probes[ i ], false );
			}

			if( !needUpdateProbesTextures )
				UpdateProbeVolumes();
		}

		if( needUpdateProbeRadiuses )
		{
			m_ProbeRadiusesDirty = true;
			UpdateProbeRadiuses();
		}

		if( needUpdateProbesTextures )
		{
			m_ProbeVolumeDirty = 1;
			RecreateVolumeTexes();

			r_need_update_probes->SetInt( 1 );
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::ClearProbes()
{
	for( int tz = 0, e = m_ProbeMap.Height(); tz < e; tz ++ )
	{
		for( int tx = 0, e = m_ProbeMap.Width(); tx < e; tx ++ )
		{
			ClearProbeMapTile( tx, tz );
			ResetProbeMapBBox( tx, tz );
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::PopulateProbes( int tileX, int tileZ )
{
	ClearProbeMapTile( tileX, tileZ );
	ResetProbeMapBBox( tileX, tileZ );

	typedef r3dTL::TArray< r3dPoint3D > Point3DArr;

	Point3DArr verticalArr;

	typedef r3dTL::TArray< Point3DArr > Point3DArrArr;

	Point3DArrArr verticalDivisions;

	verticalDivisions.Resize( m_Settings.MaxVerticalProbes );

	const float CAST_DELTA = 10.f;
	
	float rayCastYStart = m_Info.ProbeMapWorldYStart + m_Info.ProbeMapWorldActualYSize + CAST_DELTA;
	float rayCastYLength = m_Info.ProbeMapWorldActualYSize + CAST_DELTA * 2.0f;

	float tileSizeX = m_Info.CellSizeX * m_Info.ProbeCellsInTileX;
	float tileSizeZ = m_Info.CellSizeZ * m_Info.ProbeCellsInTileZ;

	int countX = int( tileSizeX / m_Settings.ProbePopulationStepX ) + 1;
	int countZ = int( tileSizeZ / m_Settings.ProbePopulationStepZ ) + 1;

	float startX = tileSizeX * tileX + m_Info.ProbeMapWorldXStart;
	float startZ = tileSizeZ * tileZ + m_Info.ProbeMapWorldZStart;

	for( int z = 0, e = countZ; z < e; z ++ )
	{
		for( int x = 0, e = countX; x < e; x ++ )
		{
			float XPoke = ( x + 0.5f ) * m_Settings.ProbePopulationStepX + startX;
			float ZPoke = ( z + 0.5f ) * m_Settings.ProbePopulationStepZ + startZ;

			int chkX, chkZ;

			ConvertToTileCoords( XPoke, ZPoke, &chkX, &chkZ );

			if( chkX != tileX || chkZ != tileZ )
				continue;

			CollisionInfo cinfo;

			r3dPoint3D probePos;

			probePos.x = XPoke;
			probePos.z = ZPoke;

			float yHit = rayCastYStart;

			float hitBottom;

			if( Terrain )
				hitBottom = Terrain->GetHeight( r3dPoint3D( XPoke, 0.f, ZPoke ) );
			else
				hitBottom = rayCastYStart - rayCastYLength;

			verticalArr.Clear();

			for(; yHit >= hitBottom + m_Settings.ProbeElevation; )
			{
				r3dPoint3D castStart = r3dPoint3D( XPoke, yHit - CAST_DELTA * 0.5f, ZPoke );

				GameObject* gobj = NULL;

				if( gobj = GameWorld().CastRay( castStart, r3dPoint3D( 0, -1, 0 ), rayCastYLength, &cinfo ) )
				{
					yHit = R3D_MAX( cinfo.NewPosition.y, hitBottom );
				}
				else
				{
					// just put it in the centre
					yHit = hitBottom;
				}

				probePos.y = yHit + m_Settings.ProbeElevation;

				if( gobj && gobj->ObjFlags & OBJFLAG_PlayerCollisionOnly )
					continue;

				if( !Terrain && !gobj )
					continue;

				verticalArr.PushBack( probePos );
			}

			// remove excessive probes
			if( (int)verticalArr.Count() > m_Settings.MaxVerticalProbes )
			{
				for( int i = 0, e = verticalDivisions.Count(); i < e; i ++ )
				{
					verticalDivisions[ i ].Clear();
				}

				float probeStart = verticalArr[ verticalArr.Count() - 1 ].y;
				float probeSpan = verticalArr[ 0 ].y - probeStart;

				float divisionSpan = probeSpan / m_Settings.MaxVerticalProbes;

				// divide probes into divisions of equal length
				for( int i = 0, e = verticalArr.Count(); i < e; i ++ )
				{
					r3dPoint3D* p = &verticalArr[ i ];
					int divIdx = int( ( p->y - probeStart ) / divisionSpan );

					divIdx = R3D_MAX( R3D_MIN( divIdx, m_Settings.MaxVerticalProbes - 1 ), 0 );

					verticalDivisions[ divIdx ].PushBack( *p );
				}

				int totalCount = 0x7fffffff;

				// remove probes from most populated divisions until
				// we meet max vertical probes requirement
				for(; totalCount > m_Settings.MaxVerticalProbes; )
				{
					// find most populated division.
					int mostPopulated = 0;
					int mostPopulatedIdx = -1;

					// find most populated division
					for( int i = 0, e = verticalDivisions.Count(); i < e; i ++ )
					{
						if( (int)verticalDivisions[ i ].Count() > mostPopulated )
						{
							mostPopulated = verticalDivisions[ i ].Count();
							mostPopulatedIdx = i;
						}
					}

					if( mostPopulatedIdx < 0 || mostPopulated <= 0 )
						break;

					Point3DArr& mostPopulatedArr = verticalDivisions[ mostPopulatedIdx ];

					float furthestFromCentre = -1.f;
					int furthestFromCentreIdx = -1;

					// find probe in the division which is the furthest from the
					// division center
					for( int i = 0, e = mostPopulated; i < e; i ++ )
					{
						float dist = fabs( ( probeStart + ( mostPopulatedIdx + 0.5f ) * divisionSpan ) - mostPopulatedArr[ i ].y );

						if( dist > furthestFromCentre )
						{
							furthestFromCentreIdx = i;
							furthestFromCentre = dist;
						}
					}

					if( furthestFromCentreIdx < 0 )
						break;

					mostPopulatedArr.Erase( furthestFromCentreIdx );

					totalCount = 0;

					for( int i = 0, e = verticalDivisions.Count(); i < e; i ++ )
					{
						totalCount += verticalDivisions[ i ].Count();
					}
				}

				verticalArr.Clear();

				for( int i = 0, e = verticalDivisions.Count(); i < e; i ++ )
				{
					Point3DArr& probes = verticalDivisions[ i ];

					for( int i = 0, e = probes.Count(); i < e; i ++ )
					{
						verticalArr.PushBack( probes[ i ] );
					}
				}
			}

			for( int i = 0, e = verticalArr.Count(); i < e; i ++ )
			{
				AddProbe( verticalArr[ i ], false, NULL );
			}
		}
	}
}

//------------------------------------------------------------------------

int ProbeMaster::GetProbeCount( int tileX, int tileZ )
{
	if( tileX < 0 || tileX >= m_Info.ActualProbeTileCountX 
			||
		tileZ < 0 || tileZ >= m_Info.ActualProbeTileCountZ
		)
	{
		return 0;
	}

	return m_ProbeMap[ tileZ ][ tileX ].TheProbes.Count();
}

//------------------------------------------------------------------------

int ProbeMaster::GetProbeCount() const
{
	int count = 0;

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			const Probes& probes = m_ProbeMap[ z ][ x ].TheProbes;

			count += probes.Count();
		}
	}

	return count;
}

//------------------------------------------------------------------------

int ProbeMaster::GetDirtyProbeCount( int flag )
{
	int count = 0;

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			Probes& probes = m_ProbeMap[ z ][ x ].TheProbes;

			for( int i = 0, e = (int)probes.Count(); i < e ; i ++ )
			{
				Probe& p = probes[ i ];

				if( p.Flags & flag )
					count ++;
			}
		}
	}

	return count;
}

//------------------------------------------------------------------------

void ProbeMaster::SwitchVolumeFormat()
{
	if( m_Settings.ProbeTextureFmt == D3DFMT_R5G6B5 )
		m_Settings.ProbeTextureFmt = D3DFMT_A8R8G8B8;
	else
		m_Settings.ProbeTextureFmt = D3DFMT_R5G6B5;

	RecreateVolumeTexes();
	UpdateProbeVolumes();
}

//------------------------------------------------------------------------

r3dPoint3D ProbeMaster::GetProbeTexturePosition()
{
	return r3dPoint3D(	( m_Info.CamCellX + 0.5f ) / m_Info.TotalProbeCellsCountX * m_Info.ProbeMapWorldActualXSize + m_Info.ProbeMapWorldXStart,
						( m_Info.CamCellY + 0.5f ) / m_Info.TotalProbeCellsCountY * m_Info.ProbeMapWorldActualYSize + m_Info.ProbeMapWorldYStart,
						( m_Info.CamCellZ + 0.5f ) / m_Info.TotalProbeCellsCountZ * m_Info.ProbeMapWorldActualZSize + m_Info.ProbeMapWorldZStart
							);
}

//------------------------------------------------------------------------

r3dPoint3D ProbeMaster::GetProbeTextureWrapPoint()
{
	return r3dPoint3D(	float(m_Info.ProbeVolumeCamCellX) / m_Settings.ProbeTextureWidth - 0.5f,
						float(m_Info.ProbeVolumeCamCellY) / m_Settings.ProbeTextureDepth - 0.5f,
						float(m_Info.ProbeVolumeCamCellZ) / m_Settings.ProbeTextureHeight - 0.5f );
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateCamera( const r3dPoint3D& cam )
{

	if( UpdateCameraCell( cam ) )
	{
		UpdateProbeVolumes();
	}
}

//-----------------------------------------------------------------------

int ProbeMaster::UpdateCameraCell( const r3dPoint3D& cam )
{
	if( r_debug_helper->GetInt() )
	{
		r_need_update_probes->SetInt( 1 );
		m_ProbeVolumeDirty = 1;
		r_debug_helper->SetInt( 0 );
	}

	int newCamX, newCamY, newCamZ;

	GetCellCoords( &newCamX, &newCamY, &newCamZ, cam );

	if( newCamX != m_Info.CamCellX 
		||
		newCamY != m_Info.CamCellY
		||
		newCamZ != m_Info.CamCellZ
		)
	{
		m_Info.CamCellX = newCamX;
		m_Info.CamCellY = newCamY;
		m_Info.CamCellZ = newCamZ;

		return 1;
	}

	return 0;
}

//------------------------------------------------------------------------

int3 ProbeMaster::GetCameraProbeCell() const
{
	int3 res;
	res.x = m_Info.CamCellX;
	res.y = m_Info.CamCellY;
	res.z = m_Info.CamCellZ;

	return res;
}

//------------------------------------------------------------------------

const ProbeMaster::SkyDirectColors& ProbeMaster::GetSkyDirectColors() const
{
	return m_SkyDirColors;
}

//------------------------------------------------------------------------

const Probe& ProbeMaster::GetDefaultProbe()
{
	return m_DefaultProbe;
}

//------------------------------------------------------------------------

static int IsSphereInside( r3dPoint3D centre, float radius, float x0, float y0, float x1, float y1 )
{
	D3DXVECTOR4 pos( centre.x, centre.y, centre.z, 1.f );

	D3DXVec4Transform( &pos, &pos, &r3dRenderer->ViewMatrix );

	if( pos.z + radius < r3dRenderer->NearClip )
		return 0;

	if( pos.z < r3dRenderer->NearClip )
		pos.z = r3dRenderer->NearClip;

	pos.w = 1;
	D3DXVec4Transform( &pos, &pos, &r3dRenderer->ProjMatrix );

	pos.x /= pos.w;
	pos.y /= pos.w;

	radius /= R3D_MAX( pos.w - radius, r3dRenderer->NearClip );

	if( pos.x + radius < x0 || pos.x - radius > x1 )
		return 0;

	if( pos.y + radius < y0 || pos.y - radius > y1 )
		return 0;

	return 1;
}

void ProbeMaster::SelectProbes( float x0, float y0, float x1, float y1 )
{
	DeleteProxies();

	g_Manipulator3d.PickerResetPicked();

	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			const ProbeTile& tile = m_ProbeMap[ z ][ x ];

			if( !tile.TheProbes.Count() )
				continue;

			const r3dPoint3D& centre = tile.BBox.Center();
			const float radius = tile.BBox.Size.Length() * 0.5f;

			if( !IsSphereInside( centre, radius, x0, y0, x1, y1 ) )
				continue;

			for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
			{
				const Probe& p = tile.TheProbes[ i ];

				if( !IsSphereInside( p.Position, PROBE_DISPLAY_SCALE, x0, y0, x1, y1 ) )
					continue;

				ProbeIdx idx;
				idx.TileX = x;
				idx.TileZ = z;
				idx.InTileIdx = i;

				int alreadySelected = 0;

				for( int i = 0, e = m_ProbeProxies.Count(); i < e; i ++ )
				{
					if( m_ProbeProxies[ i ]->Idx.CombinedIdx == idx.CombinedIdx )
					{
						alreadySelected = 1;
						break;
					}
				}

				if( alreadySelected )
					continue;

				SelectProbe( idx, p );
			}
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::DeleteProxies()
{
	for( int i = 0, e = m_ProbeProxies.Count(); i < e; i ++ )
	{
		GameWorld().DeleteObject( m_ProbeProxies[ i ] );
	}

	m_ProbeProxies.Clear();
}

//------------------------------------------------------------------------

void ProbeMaster::DeleteProxyFor( ProbeIdx idx )
{
	int found = 0;

	for( int i = 0, e = (int)m_ProbeProxies.Count(); i < e;  )
	{
		ProbeProxy* pp = m_ProbeProxies[ i ];

		if( pp->Idx.CombinedIdx == idx.CombinedIdx )
		{
			GameWorld().DeleteObject( pp );
			m_ProbeProxies.Erase( i );
			found ++;
			e --;
		}
		else
		{
			i ++;
		}
	}

	r3d_assert( found == 1 );
}

//------------------------------------------------------------------------

Probe* ProbeMaster::GetProbe( ProbeIdx idx )
{
	if( idx.CombinedIdx == 0xFFFFFFFF )
		return 0;

	ProbeTile& pt = m_ProbeMap[ idx.TileZ ][ idx.TileX ];

	if( pt.TheProbes.Count() <= idx.InTileIdx )
		return 0;

	return &pt.TheProbes[ idx.InTileIdx ];
}

//------------------------------------------------------------------------

ProbeIdx ProbeMaster::MoveProbe( ProbeIdx idx, const r3dPoint3D& newPos )
{
	m_ProbeRadiusesDirty = 1;
	Probe* pProbe = GetProbe( idx );

	r3d_assert( pProbe );

	int newCellX, newCellY, newCellZ;
	
	GetCellCoords( &newCellX, &newCellY, &newCellZ, newPos );

	int cellChanged = 0;

	if( newCellX != pProbe->CellX
			||
		newCellY != pProbe->CellY
			||
		newCellZ != pProbe->CellZ
		)
	{
		if( IsOccupied( newPos ) )
			return idx;

		cellChanged = 1;
	}

	//------------------------------------------------------------------------

	if( cellChanged )
	{
		int tileX = pProbe->CellX / m_Info.ProbeCellsInTileX;
		int tileZ = pProbe->CellZ / m_Info.ProbeCellsInTileZ;

		for( int z = tileZ - VPL_GATHER_TILE_RANGE, e = tileZ + VPL_GATHER_TILE_RANGE ; z <= e ; z ++ )
		{
			for( int x = tileX - VPL_GATHER_TILE_RANGE, e = tileX + VPL_GATHER_TILE_RANGE ; x <= e ; x ++ )
			{
				if( x < 0 || z < 0 || x >= (int)m_ProbeMap.Width() || z >= (int)m_ProbeMap.Height() )
					continue;

				ProbeTile& ptile = m_ProbeMap[ z ][ x ];

				for( int i = 0, e = ptile.TheProbes.Count(); i < e; i ++ )
				{
					Probe* probe = &ptile.TheProbes[ i ];

					for( int i = 0, e = Probe::NUM_LOCAL_VPL; i < e; )
					{
						ProbeIdx lidx = probe->SH_LocalVPLProbeIndexes[ i ];
						if(	lidx.TileX == idx.TileX
								&&
							lidx.TileZ == idx.TileZ
								&&
							// remove consequent once too, because they are going
							// to be shifted after deletion
							lidx.InTileIdx >= idx.InTileIdx
							)
						{
							for( int j = i; j < e - 1; j ++ )
							{
								probe->Flags |= Probe::FLAG_LOCAL_BOUNCE_DIRTY;

								probe->SH_LocalVPLProbeIndexes[ j ] = probe->SH_LocalVPLProbeIndexes[ j + 1 ];
								probe->SH_LocalVPLsR[ j ] = probe->SH_LocalVPLsR[ j + 1 ];
								probe->SH_LocalVPLsG[ j ] = probe->SH_LocalVPLsG[ j + 1 ];
								probe->SH_LocalVPLsB[ j ] = probe->SH_LocalVPLsB[ j + 1 ];
							}

							ProbeIdx invalid;
							invalid.CombinedIdx = -1;

							probe->SH_LocalVPLProbeIndexes[ e - 1 ] = invalid;
							probe->SH_LocalVPLsR[ e - 1 ] = D3DXVECTOR4( 0, 0, 0, 0 );
							probe->SH_LocalVPLsG[ e - 1 ] = D3DXVECTOR4( 0, 0, 0, 0 );
							probe->SH_LocalVPLsB[ e - 1 ] = D3DXVECTOR4( 0, 0, 0, 0 );
						}
						else
						{
							i ++;
						}
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------

	Probe probe = *pProbe;

	DeleteProbe( idx, false );

	probe.Position = newPos;
	probe.Flags |= Probe::FLAG_SKY_DIRTY | Probe::FLAG_BOUNCE_DIRTY | Probe::FLAG_LOCAL_BOUNCE_DIRTY;

	ProbeIdx newIdx = AddProbe( newPos, true, &probe );

	AddDirtyRadiusTile( newIdx );

	return newIdx;
}

//------------------------------------------------------------------------

Probe* ProbeMaster::GetClosestProbe( const r3dPoint3D& pos )
{
	float x = pos.x;
	float z = pos.z;

	int tileX = int( ( x - m_Info.ProbeMapWorldXStart ) / m_Info.ProbeMapWorldActualXSize * m_ProbeMap.Width() );
	int tileZ = int( ( z - m_Info.ProbeMapWorldZStart ) / m_Info.ProbeMapWorldActualZSize * m_ProbeMap.Height() );

	r3dTL::TFixedArray< Probe*, 9 > ps;

	int pi = 0;

	ps[ pi ++  ] = GetClosestProbe( tileX + 0, tileZ + 0, pos );
	
	ps[ pi ++  ] = GetClosestProbe( tileX - 1, tileZ + 0, pos );
	ps[ pi ++  ] = GetClosestProbe( tileX + 1, tileZ + 0, pos );
	ps[ pi ++  ] = GetClosestProbe( tileX + 0, tileZ - 1, pos );
	ps[ pi ++  ] = GetClosestProbe( tileX + 0, tileZ + 1, pos );

	ps[ pi ++  ] = GetClosestProbe( tileX + 1, tileZ + 1, pos );
	ps[ pi ++  ] = GetClosestProbe( tileX + 1, tileZ - 1, pos );
	ps[ pi ++  ] = GetClosestProbe( tileX - 1, tileZ + 1, pos );
	ps[ pi ++  ] = GetClosestProbe( tileX - 1, tileZ - 1, pos );

	Probe* closest = NULL;
	float minDist = FLT_MAX;

	for( int i = 0, e = pi; i < e; i ++ )
	{
		if( Probe* p = ps[ i ] )
		{
			float len = ( p->Position - pos ).Length();

			if( len < minDist )
			{
				minDist = len;
				closest = p;
			}
		}
	}

	return closest;
}

//------------------------------------------------------------------------

ProbeIdx ProbeMaster::AddProbe( const r3dPoint3D& pos, bool markTileDirty, const Probe* savedProbe )
{
	int mapXIdx = int( ( pos.x - m_Info.ProbeMapWorldXStart )  * m_ProbeMap.Width() / m_Info.ProbeMapWorldActualXSize );
	int mapZIdx = int( ( pos.z - m_Info.ProbeMapWorldZStart ) * m_ProbeMap.Height() / m_Info.ProbeMapWorldActualZSize );

	mapXIdx = R3D_MIN( R3D_MAX( mapXIdx, 0 ), (int)m_ProbeMap.Width() - 1 );
	mapZIdx = R3D_MIN( R3D_MAX( mapZIdx, 0 ), (int)m_ProbeMap.Height() - 1 );

	Probe probe;

	if( savedProbe )
	{
		probe = *savedProbe;
	}

	probe.Position = pos;

	GetCellCoords( &probe.CellX, &probe.CellY, &probe.CellZ, pos );

	ProbeIdx res;

	res.TileX = mapXIdx;
	res.TileZ = mapZIdx;

	int inTileIdx = AddProbe( mapXIdx, mapZIdx, probe );

	if( inTileIdx != -1 )
	{
		res.InTileIdx = inTileIdx;

		if( markTileDirty )
		{
			AddDirtyRadiusTile( res );
		}
	}
	else
	{
		res.CombinedIdx = -1;
	}

	return res;
}

//------------------------------------------------------------------------

ProbeIdx ProbeMaster::AddProbe( const Probe& probe, bool markTileDirty )
{
	return AddProbe( probe.Position, markTileDirty, &probe );
}

//------------------------------------------------------------------------

bool ProbeMaster::IsOccupied( const r3dPoint3D& pos )
{
	int cellX, cellY, cellZ;

	GetCellCoords( &cellX, &cellY, &cellZ, pos );

	int tileX = cellX / m_Info.ProbeCellsInTileX;
	int tileZ = cellZ / m_Info.ProbeCellsInTileZ;

	if( tileX < 0 || tileX >= (int)m_ProbeMap.Width()
			||
		tileZ < 0 || tileZ >= (int)m_ProbeMap.Height() )
	{
		return false;
	}

	ProbeTile& tile = m_ProbeMap[ tileZ ][ tileX ];

	for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
	{
		const Probe& probe = tile.TheProbes[ i ];

		if( probe.CellX == cellX && probe.CellY == cellY && probe.CellZ == cellZ )
			return true;
	}

	return false;
}

//------------------------------------------------------------------------

void ProbeMaster::DeleteAllProbes()
{
	ClearProbeMap();
	ResetProbeMapBBoxes();
}

//------------------------------------------------------------------------

void ProbeMaster::DeleteProbesInCylinder( const r3dPoint3D& base, float radius, float height )
{
	int top, left, bottom, right;
	ConvertToTileCoords( base.x - radius, base.z - radius, &left, &top );
	ConvertToTileCoords( base.x + radius, base.z + radius, &right, &bottom );

	left --;
	top --;
	right ++;
	bottom ++;

	left = R3D_MIN( R3D_MAX( left, 0 ), m_Info.ActualProbeTileCountX - 1 );
	right = R3D_MIN( R3D_MAX( right, 0 ), m_Info.ActualProbeTileCountX - 1 );
	top = R3D_MIN( R3D_MAX( top, 0 ), m_Info.ActualProbeTileCountZ - 1 );
	bottom = R3D_MIN( R3D_MAX( bottom, 0 ), m_Info.ActualProbeTileCountZ - 1 );

	float rsq = radius * radius;

	ProbeIdxArray arr;

	for( int z = top; z <= bottom; z ++ )
	{
		for( int x = left; x <= right; x ++ )
		{
			ProbeTile& tile = m_ProbeMap[ z ][ x ];

			for( int i = 0, e = tile.TheProbes.Count(); i < e; i++ )
			{
				Probe& p = tile.TheProbes[ i ];

				float dx = p.Position.x - base.x;
				float dz = p.Position.Z - base.z;

				if( dx * dx + dz * dz < rsq && p.Position.y > base.y && p.Position.y < base.y + height )
				{
					ProbeIdx idx;

					idx.TileX = x;
					idx.TileZ = z;
					idx.InTileIdx = i;

					arr.PushBack( idx );
				}
			}
		}
	}

	DeleteProbes( arr, false );
}

//------------------------------------------------------------------------

void ProbeMaster::DeleteProbes( const r3dTL::TArray< GameObject* > & objects )
{
	ProbeIdxArray parray;

	parray.Reserve( objects.Count() );

	for( int i = 0, e = objects.Count(); i < e; i ++ )
	{
		GameObject* obj = objects[ i ];

		int found = 0;

		for( int i = 0, e = m_ProbeProxies.Count(); i < e; i ++ )
		{
			ProbeProxy* pp = m_ProbeProxies[ i ];

			if( pp == obj )
			{
				found ++;
				parray.PushBack( pp->Idx );
			}
		}

		if( found > 1 )
		{
			r3dOutToLog( "ProbeMaster::DeleteProbes: Probe Found Twice!" );
			MessageBox( r3dRenderer->HLibWin, "ProbeMaster::DeleteProbes: Probe Found Twice!", "Error", MB_ICONERROR );
		}

		if( !found )
		{
			r3dOutToLog( "ProbeMaster::DeleteProbes: Alien object to delete!" );
			MessageBox( r3dRenderer->HLibWin, "ProbeMaster::DeleteProbes: Alien object to delete!", "Error", MB_ICONERROR );
		}
	}

	DeleteProbes( parray, true );
}

//------------------------------------------------------------------------

void ProbeMaster::DeleteProbes( const ProbeIdxArray& probeIdxArray, bool deleteProxies )
{
	if( !probeIdxArray.Count() )
		return;

	// in order to delete correctly, have to sort based on 'in tile idx', 
	// so that probes are always deleted starting from the "furthest one",
	// avoiding making previous indexes invalid.

	ProbeIdxArray sorted = probeIdxArray;

	struct SortF
	{
		bool operator() ( const ProbeIdx& idx0, const ProbeIdx& idx1 )
		{
			return idx0.InTileIdx > idx1.InTileIdx;
		}
	};

	std::sort( &sorted[ 0 ], &sorted[ 0 ] + sorted.Count(), SortF() );

	for( int i = 0, e = (int)sorted.Count(); i < e; i ++ )
	{
		DeleteProbe( sorted[ i ], deleteProxies );
	}
}

//------------------------------------------------------------------------

void ProbeMaster::DeleteProbe( ProbeIdx idx, bool deleteProxy )
{
	if( idx.CombinedIdx != 0xFFFFFFFF )
	{
		m_ProbeRadiusesDirty = 1;

		int tileX = idx.TileX;
		int tileZ = idx.TileZ;

		r3d_assert( tileX >= 0 && tileX < (int)m_ProbeMap.Width() );
		r3d_assert( tileZ >= 0 && tileZ < (int)m_ProbeMap.Height() );

		AddDirtyRadiusTile( idx );

		ProbeTile& tile = m_ProbeMap[ tileZ ][ tileX ];

		r3d_assert( tile.TheProbes.Count() > idx.InTileIdx );

		if( deleteProxy )
		{
			DeleteProxyFor( idx );
		}

		for( int i = 0, e = m_ProbeProxies.Count() ; i < e ; i ++ )
		{
			ProbeProxy* pp = m_ProbeProxies[ i ];

			if( pp->Idx.TileX == idx.TileX && pp->Idx.TileZ == idx.TileZ && pp->Idx.InTileIdx > idx.InTileIdx )
			{
				pp->Idx.InTileIdx --;
			}
		}

		tile.TheProbes.Erase( idx.InTileIdx );

		RecalcTileBBox( tileX, tileZ );
	}
}

//------------------------------------------------------------------------

void ProbeMaster::AdjustProbeRadiuses()
{
	float start = r3dGetTime();

	r3dOutToLog( "ProbeMaster::AdjustProbeRadiuses..." );

	for( int tz = 0, tze = m_ProbeMap.Height(); tz < tze ; tz ++ )
	{
		for( int tx = 0, txe = m_ProbeMap.Width(); tx < txe ; tx ++ )
		{
			AdjustProbeRadiuses( tx, tz );
		}
	}

	r3dOutToLog( "done in %.2f seconds\n", r3dGetTime() - start );
}

//------------------------------------------------------------------------

void ProbeMaster::AdjustProbeRadiuses( int tileX, int tileZ )
{
	ProbeTile& tile = m_ProbeMap[ tileZ ][ tileX ];

	for( int i = 0, e = tile.TheProbes.Count(); i < e; i ++ )
	{
		Probe& probe = tile.TheProbes[ i ];

		AdjustProbeRadius( &probe, tileX, tileZ );
	}
}

//------------------------------------------------------------------------

const r3dBoundBox* ProbeMaster::GetTileBBox( int tileX, int tileZ )
{
	if( tileX < 0 || tileX >= m_Info.ActualProbeTileCountX 
			||
		tileZ < 0 || tileZ >= m_Info.ActualProbeTileCountZ
		)
	{
		return NULL;
	}

	return &m_ProbeMap[ tileZ ][ tileX ].BBox;
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateDirtyTilesRadiuses()
{
	for( int i = 0, e = m_DirtyRadiusesTiles.Count(); i < e ; i ++ )
	{
		ProbeIdx idx = m_DirtyRadiusesTiles[ i ];
		AdjustProbeRadiuses( idx.TileX, idx.TileZ );
	}

	m_DirtyRadiusesTiles.Clear();

	m_ProbeVolumeDirty = 1;
}

//------------------------------------------------------------------------

int ProbeMaster::GetVisibleProbesNum()
{
	return m_RasterizedProbeCount;
}

//------------------------------------------------------------------------

int ProbeMaster::DistributeLightToProbes( r3dLight* light )
{
	int needMarkDirty = 0;

	if( fabs( light->InProbesVolumeIntensity - light->Intensity ) > 0.1f 
		||
		fabs( ( light->InProbesVolumePosition - *light ).LengthSq() ) > 0.0625f
		||
		fabs( light->InProbesVolumeRadius - light->GetOuterRadius() ) > 0.25f
		)
	{
		light->InProbesVolumeIntensity = light->Intensity;
		light->InProbesVolumePosition = *light;
		light->InProbesVolumeRadius = light->GetOuterRadius();

		needMarkDirty = 1;
	}

	int hasDirty = 0;

	float outerR = light->InProbesVolumeRadius;
	float outerRSQR = outerR * outerR;

	r3dPoint3D topLeft = *light - r3dPoint3D( outerR, 0, outerR );
	r3dPoint3D bottomRight = *light + r3dPoint3D( outerR, 0, outerR );

	int csx, csy, csz;
	int cex, cey, cez;

	GetCellCoords( &csx, &csy, &csz, topLeft );
	GetCellCoords( &cex, &cey, &cez, bottomRight );

	if( needMarkDirty )
	{
		int probeTexStartX = m_Info.CamCellX - m_Settings.ProbeTextureWidth / 2;
		int probeTexStartZ = m_Info.CamCellZ - m_Settings.ProbeTextureHeight / 2;

		int dsx = ( csx - probeTexStartX ) / PROBE_DIRTY_ARR_CELL_SIZE;
		int dsz = ( csz - probeTexStartZ ) / PROBE_DIRTY_ARR_CELL_SIZE;

		int dex = ( cex - probeTexStartX ) / PROBE_DIRTY_ARR_CELL_SIZE;
		int dez = ( cez - probeTexStartZ ) / PROBE_DIRTY_ARR_CELL_SIZE;

		for( int dz = dsz; dz <= dez; dz ++ )
		{
			for( int dx = dsx; dx <= dex; dx ++ )
			{
				if( dx >= 0 && dx < (int)m_ProbeVolumeDirtyArr.Width()
					&&
					dz >= 0 && dz < (int)m_ProbeVolumeDirtyArr.Height()
					)
				{
					m_ProbeVolumeDirtyArr[ dz ][ dx ] = 1;

					if( g_bEditMode )
					{
						m_ProbeVolumeDirtyAnimArr[ dz ][ dx ] = 255;
					}

					hasDirty = 1;
				}
			}
		}
	}

	enum
	{
		MAX_CLOSEST_PROBES = 8
	};

	DistanceProbeEntry closestProbeEntries[ MAX_CLOSEST_PROBES ];

	int numClosestProbes = GatherClosestProbesForLightDistribution( *light, closestProbeEntries, MAX_CLOSEST_PROBES );

	float wsum = 0.0f;

	for( int i = 0; i < numClosestProbes; i ++ )
	{
		DistanceProbeEntry& entry = closestProbeEntries[ i ];

		entry.distance = 1.0f / R3D_MAX( entry.distance * entry.distance, 16.F * FLT_MIN );

		// this distance is now weight
		wsum += entry.distance;
	}

	for( int i = 0; i < numClosestProbes; i ++ )
	{
		DistanceProbeEntry& entry = closestProbeEntries[ i ];

		entry.distance /= wsum;		
	}

	float intensity = light->InProbesVolumeIntensity;

	float lightR = light->R / 255.0f * intensity;
	float lightG = light->G / 255.0f * intensity;
	float lightB = light->B / 255.0f * intensity;

	for( int i = 0, e = numClosestProbes; i < e; i ++ )
	{
		DistanceProbeEntry& entry = closestProbeEntries[ i ];

		entry.probe->DynamicLightsRGB.x += lightR * entry.distance;
		entry.probe->DynamicLightsRGB.y += lightG * entry.distance;
		entry.probe->DynamicLightsRGB.z += lightB * entry.distance;
	}

	return hasDirty;
}

//------------------------------------------------------------------------

const ProbeMaster::ProbeProxies&
ProbeMaster::GetSelectedProbes() const
{
	return m_ProbeProxies;
}

//------------------------------------------------------------------------

void ProbeMaster::DEBUG_SetEndProbe( int endProbe )
{
	if( m_DEBUG_EndProbe != endProbe )
	{
		m_ProbeVolumeDirty = 1;
		r_need_update_probes->SetInt( 1 );
	}

	m_DEBUG_EndProbe = endProbe;
}

//------------------------------------------------------------------------

void ProbeMaster::ConvertCellToCoord( int cellX, int cellY, int cellZ, r3dPoint3D* oPos )
{
	float offY = m_Settings.ProbeVolumeOffsetY;

	oPos->x = cellX * m_Info.ProbeMapWorldActualXSize / m_Info.TotalProbeCellsCountX + m_Info.ProbeMapWorldXStart;
	oPos->y = cellY * m_Info.ProbeMapWorldActualYSize / m_Info.TotalProbeCellsCountY + m_Info.ProbeMapWorldYStart - offY;
	oPos->z = cellZ * m_Info.ProbeMapWorldActualZSize / m_Info.TotalProbeCellsCountZ + m_Info.ProbeMapWorldZStart;
}

//------------------------------------------------------------------------

void ProbeMaster::ConvertCoordToCell( int * oCellX, int * oCellY, int * oCellZ, const r3dPoint3D& pos )
{
	GetCellCoords( oCellX, oCellY, oCellZ, pos );
}

//------------------------------------------------------------------------

const ProbeMaster::ProbeVolumeTileDirtyArr&
ProbeMaster::GetAnimatedDirtiness() const
{
	return m_ProbeVolumeDirtyAnimArr;
}

//------------------------------------------------------------------------

void ProbeMaster::RelightProbe( Probe* probe )
{
	float skyDirCoef = r_lp_sky_direct->GetFloat();
	float skyBounceCoef = r_lp_sky_bounce->GetFloat();
	float sunBounceCoef = r_lp_sun_bounce->GetFloat();

	float skyVisOnBounces = 0.f;

	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		skyVisOnBounces += probe->SkyVisibility[ i ];
	}

	// -1 because dir down is perma black
	skyVisOnBounces /= ( Probe::NUM_DIRS - 1 );

	skyVisOnBounces = skyVisOnBounces * m_SkyVisA + m_SkyVisB;
	
	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		float skyVisCoef = probe->SkyVisibility[ i ];
		float skyVis = skyVisCoef * skyDirCoef;

		const D3DXVECTOR4 skyDirColor = m_SkyDirColors.DirColor[ i ];

		float skyDirectR = skyDirColor.x * skyVis;
		float skyDirectG = skyDirColor.y * skyVis;
		float skyDirectB = skyDirColor.z * skyVis;

		float finalSkyBounceCoef = skyBounceCoef * skyVisOnBounces;

		float skyBounceR = D3DXVec4Dot( &m_SkyDomeSH_R, &probe->SH_BounceR[ i ] ) * finalSkyBounceCoef;
		float skyBounceG = D3DXVec4Dot( &m_SkyDomeSH_G, &probe->SH_BounceG[ i ] ) * finalSkyBounceCoef;
		float skyBounceB = D3DXVec4Dot( &m_SkyDomeSH_B, &probe->SH_BounceB[ i ] ) * finalSkyBounceCoef;

		float finalSunBounceCoef = sunBounceCoef * skyVisOnBounces;

		float sunBounceR = D3DXVec4Dot( &m_SunSH_R, &probe->SH_BounceR[ i ] ) * finalSunBounceCoef;
		float sunBounceG = D3DXVec4Dot( &m_SunSH_G, &probe->SH_BounceG[ i ] ) * finalSunBounceCoef;
		float sunBounceB = D3DXVec4Dot( &m_SunSH_B, &probe->SH_BounceB[ i ] ) * finalSunBounceCoef;

		float finalR = skyDirectR + skyBounceR + sunBounceR;
		float finalG = skyDirectG + skyBounceG + sunBounceG;
		float finalB = skyDirectB + skyBounceB + sunBounceB;

#if 0
		union
		{
			struct
			{
				UINT16 b : 5;
				UINT16 g : 6;
				UINT16 r : 5;
			} * rgbVal;

			UINT16* uint16val;
		};

		uint16val = &probe->BasisColors[ i ];

		float the16bit_amp = r_lp_16bit_amplify->GetFloat();

		rgbVal->r = R3D_MAX( R3D_MIN( int( finalR * 31 * the16bit_amp ), 31 ), 0 );
		rgbVal->g = R3D_MAX( R3D_MIN( int( finalG * 63 * the16bit_amp ), 63 ), 0 );
		rgbVal->b = R3D_MAX( R3D_MIN( int( finalB * 31 * the16bit_amp ), 31 ), 0 );
#endif


		union
		{
			struct
			{
				UINT32 b : 8;
				UINT32 g : 8;
				UINT32 r : 8;
				UINT32 a : 8;
			} * rgbaVal;

			UINT32* uint32val;
		};

		uint32val = &probe->BasisColors32[ i ];

		int ibr, ibg, ibb;

		rgbaVal->r = ibr = R3D_MAX( R3D_MIN( int( finalR * 255 ), 255 ), 0 );
		rgbaVal->g = ibg = R3D_MAX( R3D_MIN( int( finalG * 255 ), 255 ), 0 );
		rgbaVal->b = ibb = R3D_MAX( R3D_MIN( int( finalB * 255 ), 255 ), 0 );
		rgbaVal->a = 0;

		int idr = ( probe->DynamicColors32[ i ] >> 16 ) & 0xff;
		int idg = ( probe->DynamicColors32[ i ] >> 8 ) & 0xff;
		int idb = ( probe->DynamicColors32[ i ] >> 0 ) & 0xff;

		uint32val = &probe->CompositeColors32[ i ];

		rgbaVal->r = R3D_MIN( idr + ibr, 255 );
		rgbaVal->g = R3D_MIN( idg + ibg, 255 );
		rgbaVal->b = R3D_MIN( idb + ibb, 255 );
		rgbaVal->a = 0;
	}
}

//------------------------------------------------------------------------

void ProbeMaster::RelightDynamicProbe( Probe* probe )
{
	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		float dynaR = 0.f;
		float dynaG = 0.f;
		float dynaB = 0.f;

		for( int l = 0; l < Probe::NUM_LOCAL_VPL; l ++ )
		{
			ProbeIdx srcProbeIdx = probe->SH_LocalVPLProbeIndexes[ l ];

			if( srcProbeIdx.CombinedIdx == -1 )
				break;

			Probe* srcProbe = GetProbe( srcProbeIdx );

			dynaR += D3DXVec4Dot( &m_BasisSHArray[ i ], &probe->SH_LocalVPLsR[ l ] ) * srcProbe->DynamicLightsRGB.x;
			dynaG += D3DXVec4Dot( &m_BasisSHArray[ i ], &probe->SH_LocalVPLsG[ l ] ) * srcProbe->DynamicLightsRGB.y;
			dynaB += D3DXVec4Dot( &m_BasisSHArray[ i ], &probe->SH_LocalVPLsB[ l ] ) * srcProbe->DynamicLightsRGB.z;
		}

		float dyna_coef = r_lp_dyna_coef->GetFloat();

		float finalR = R3D_MIN( R3D_MAX( dynaR * dyna_coef, 0.f ), 1.f );
		float finalG = R3D_MIN( R3D_MAX( dynaG * dyna_coef, 0.f ), 1.f );
		float finalB = R3D_MIN( R3D_MAX( dynaB * dyna_coef, 0.f ), 1.f );

		union
		{
			struct
			{
				UINT32 b : 8;
				UINT32 g : 8;
				UINT32 r : 8;
				UINT32 a : 8;
			} * rgbaVal;

			UINT32* uint32val;
		};

		uint32val = &probe->DynamicColors32[ i ];

		int idr = int( finalR * 255 );
		int idg = int( finalG * 255 );
		int idb = int( finalB * 255 );

		rgbaVal->r = R3D_MAX( R3D_MIN( idr, 255 ), 0 );
		rgbaVal->g = R3D_MAX( R3D_MIN( idg, 255 ), 0 );
		rgbaVal->b = R3D_MAX( R3D_MIN( idb, 255 ), 0 );
		rgbaVal->a = 0;

		uint32val = &probe->CompositeColors32[ i ];

		int ibr = ( probe->BasisColors32[ i ] >> 16 ) & 0xff;
		int ibg = ( probe->BasisColors32[ i ] >> 8 ) & 0xff;
		int ibb = ( probe->BasisColors32[ i ] >> 0 ) & 0xff;

		rgbaVal->r = R3D_MIN( idr + ibr, 255 );
		rgbaVal->g = R3D_MIN( idg + ibg, 255 );
		rgbaVal->b = R3D_MIN( idb + ibb, 255 );
		rgbaVal->a = 0;
	}
}

//------------------------------------------------------------------------

int ProbeMaster::GatherClosestProbesForLightDistribution( const r3dPoint3D& lightPos, DistanceProbeEntry* probeEntries, int maxCount )
{
	g_ProbesWithDistances.Clear();

	int cellX, cellY, cellZ;

	GetCellCoords( &cellX, &cellY, &cellZ, lightPos );

	int tileX = cellX / m_Info.ProbeCellsInTileX;
	int tileZ = cellZ / m_Info.ProbeCellsInTileZ;

	for( int z = tileZ - 1, e = tileZ + 1 ; z <= e ; z ++ )
	{
		for( int x = tileX - 1, e = tileX + 1 ; x <= e ; x ++ )
		{
			if( x < 0 || x >= (int)m_ProbeMap.Width()
					||
				z < 0 || z >= (int)m_ProbeMap.Height()
				)
				continue;

			ProbeTile& pt = m_ProbeMap[ z ][ x ];

			for( int i = 0, e = pt.TheProbes.Count(); i < e; i ++ )
			{
				Probe* probe = &pt.TheProbes[ i ];

				DistanceProbeEntry dpe;

				dpe.distance = ( probe->Position - lightPos ).Length();
				dpe.probe = probe;

				g_ProbesWithDistances.PushBack( dpe );
			}
		}
	}

	struct sortfunc
	{
		bool operator()( const DistanceProbeEntry& e0, const DistanceProbeEntry& e1 )
		{
			return e0.distance < e1.distance;
		}
	};

	if( g_ProbesWithDistances.Count() )
	{
		std::sort( &g_ProbesWithDistances[ 0 ], &g_ProbesWithDistances[ 0 ] + g_ProbesWithDistances.Count(), sortfunc() );
	}

	int targetCount = R3D_MIN( (int)g_ProbesWithDistances.Count(), maxCount );

	for( int i = 0, e = targetCount; i < e; i ++ )
	{
		probeEntries[ i ] = g_ProbesWithDistances[ i ];
	}

	for( int i = targetCount, e = maxCount; i < e; i ++ )
	{
		probeEntries[ i ].probe = NULL;
	}

	return targetCount;
}

//------------------------------------------------------------------------

void ProbeMaster::StartUpdatingBounce()
{
	m_SavedCam = gCam;
	m_SavedUseOQ = r_use_oq->GetInt();

	gCam.FOV = 120;

	r_use_oq->SetInt( 0 );	
}

//------------------------------------------------------------------------

static void ReadBounceTextureSH( D3DXVECTOR4* targ, IDirect3DSurface9* srcSurf )
{
	D3DLOCKED_RECT lrect;
	D3D_V( srcSurf->LockRect( &lrect, NULL, D3DLOCK_READONLY ) );

	memcpy( targ, (float*)lrect.pBits, 16 );

	float invCoef = 1.0f / BOUNCE_RT_DIM / BOUNCE_RT_DIM;

	targ->x *= invCoef;
	targ->y *= invCoef;
	targ->z *= invCoef;
	targ->w *= invCoef;

	D3D_V( srcSurf->UnlockRect() );
}

static void ReadPVLTextureSH( D3DXVECTOR4* targ, IDirect3DSurface9* srcSurf )
{
	D3DLOCKED_RECT lrect;
	D3D_V( srcSurf->LockRect( &lrect, NULL, D3DLOCK_READONLY ) );

	memcpy( targ, (float*)lrect.pBits, 16 );

	const float weight = 4.0 * float( M_PI );
	float factor = weight / N_SAMPLES;

	targ->x *= factor;
	targ->y *= factor;
	targ->z *= factor;
	targ->w *= factor;

	D3D_V( srcSurf->UnlockRect() );
}

void ProbeMaster::UpdateProbeBounce( Probe* probe )
{
	struct EnableDisableDistanceCull
	{
		EnableDisableDistanceCull()
		{
			oldValue = r_allow_distance_cull->GetInt();
			r_allow_distance_cull->SetInt( 0 );
		}

		~EnableDisableDistanceCull()
		{
			r_allow_distance_cull->SetInt( oldValue );
		}

		int oldValue;

	} enableDisableDistanceCull; (void)enableDisableDistanceCull;

	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		wchar_t evname[ 512 ];
		swprintf( evname, L"ProbeMaster::UpdateProbeBounce: %d", i );

		D3DPERF_BeginEvent( 0, evname );
		{
			gCam.x = probe->Position.x;
			gCam.y = probe->Position.y;
			gCam.z = probe->Position.z;

			gCam.NearClip = 0.1f;
			gCam.FarClip = 4096.0f;

			r3dPoint3D viewDir = Probe::ViewDirs[ i ];

			gCam.vUP = Probe::UpVecs[ i ];
			gCam.PointTo( gCam + viewDir );

			void UpdateTerrain2Atlas();
			UpdateTerrain2Atlas();

			m_BounceDiffuseRT->Activate( 0 );
			m_BounceNormalRT->Activate( 1 );

			r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW );

			D3D_V( r3dRenderer->pd3ddev->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000, r3dRenderer->GetClearZValue(), 0 ) );

			r3dRenderer->SetCamera( gCam, false );

			void DrawTerrain();
			DrawTerrain();

			r3dRenderer->SetCamera( gCam, false );

			D3DXMATRIX invViewNoT = r3dRenderer->InvViewMatrix;

			invViewNoT._41 = 0.f;
			invViewNoT._42 = 0.f;
			invViewNoT._43 = 0.f;

			D3DXMATRIX invViewNoTProj = r3dRenderer->InvProjMatrix * invViewNoT;

			D3DXVECTOR4 CamVec = D3DXVECTOR4(gCam.x, gCam.y, gCam.z, 1);
			r3dRenderer->pd3ddev->SetPixelShaderConstantF(MC_CAMVEC, (float*)&CamVec, 1);

			float defSSAO[ 4 ] = { r_ssao_clear_val->GetFloat(), 0.f, 0.f, 0.f };
			r3dRenderer->pd3ddev->SetPixelShaderConstantF(MC_DEF_SSAO, defSSAO, 1);

			r3dRenderer->SetMipMapBias( 0.f );

			for( int ii = 0; ii < 6; ii ++ )
			{
				r3dRenderer->pd3ddev->SetSamplerState( ii, D3DSAMP_ADDRESSU,   D3DTADDRESS_WRAP );
				r3dRenderer->pd3ddev->SetSamplerState( ii, D3DSAMP_ADDRESSV,   D3DTADDRESS_WRAP );

				r3dSetFiltering( R3D_BILINEAR, ii );
			}

			GameWorld().Prepare( gCam );
			GameWorld().Draw( rsFillGBuffer );

			m_BounceDiffuseRT->Deactivate();
			m_BounceNormalRT->Deactivate();

			r3dSetRestoreFSQuadVDecl setRestoreVDecl; (void) setRestoreVDecl;

			r3dRenderer->SetVertexShader( g_AccumVS_ID );
			r3dRenderer->SetPixelShader( g_AccumPS_ID );

			m_BounceAccumSHRRT->Activate( 0 );
			m_BounceAccumSHGRT->Activate( 1 );
			m_BounceAccumSHBRT->Activate( 2 );

			r3dRenderer->SetRenderingMode( R3D_BLEND_ADD | R3D_BLEND_NZ );

			D3D_V( r3dRenderer->pd3ddev->Clear( 0, NULL, D3DCLEAR_TARGET, 0, r3dRenderer->GetClearZValue(), 0 ) );

			r3dRenderer->SetTex( m_BounceDiffuseRT->Tex, 0 );
			r3dRenderer->SetTex( m_BounceNormalRT->Tex, 1 );
			r3dRenderer->SetTex( m_NormalToSHTex, 2 );

			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( 0, D3DSAMP_ADDRESSU,   D3DTADDRESS_CLAMP ) );
			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( 0, D3DSAMP_ADDRESSV,   D3DTADDRESS_CLAMP ) );
			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( 1, D3DSAMP_ADDRESSU,   D3DTADDRESS_CLAMP ) );
			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( 1, D3DSAMP_ADDRESSV,   D3DTADDRESS_CLAMP ) );
			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( 2, D3DSAMP_ADDRESSU,   D3DTADDRESS_WRAP ) );
			D3D_V( r3dRenderer->pd3ddev->SetSamplerState( 2, D3DSAMP_ADDRESSV,   D3DTADDRESS_WRAP ) );

			r3dSetFiltering( R3D_POINT, 0 );
			r3dSetFiltering( R3D_POINT, 1 );
			r3dSetFiltering( R3D_POINT, 2 );

			
			D3DXVECTOR3 viewU = D3DXVECTOR3( r3dRenderer->ViewMatrix._11, r3dRenderer->ViewMatrix._21, r3dRenderer->ViewMatrix._31 );
			D3DXVECTOR3 viewV = D3DXVECTOR3( r3dRenderer->ViewMatrix._12, r3dRenderer->ViewMatrix._22, r3dRenderer->ViewMatrix._32 );

			float n = gCam.NearClip;

			D3DXVECTOR4 vec00( -n, -n, 0, n );
			D3DXVec4Transform( &vec00, &vec00, &invViewNoTProj );
			D3DXVECTOR4 vec10( +n, -n, 0, n );
			D3DXVec4Transform( &vec10, &vec10, &invViewNoTProj );
			D3DXVECTOR4 vec01( -n, +n, 0, n );
			D3DXVec4Transform( &vec01, &vec01, &invViewNoTProj );

			D3DXVECTOR4 udiff = vec10 - vec00;
			D3DXVECTOR4 vdiff = vec01 - vec00;
			// because .w is zeroed after substraction
			float ulen = D3DXVec4Length( &udiff );
			float vlen = D3DXVec4Length( &vdiff );

			viewU *= ulen / BOUNCE_RT_DIM;
			viewV *= vlen / BOUNCE_RT_DIM;

			float psConsts[ 4 ][ 4 ] =  {	
											// float4 texStep        : register( c0 );
											{ 1.0f / BOUNCE_RT_DIM, 1.0f / BOUNCE_RT_DIM, 0.f, 0.f },
											// float3 viewStepU      : register( c1 );
											{ viewU.x, viewU.y, viewU.z, 0.f },
											// float3 viewStepV      : register( c2 );
											{ viewV.x, viewV.y, viewV.z, 0.f },
											// float3 targetNorm     : register( c3 );
											{ viewDir.x, viewDir.y, viewDir.z, 0 } 
			
			};

			D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, psConsts[0], sizeof psConsts / sizeof( float[4] ) ) );

			// see AccumSH_ps.hls
			const int SAMPLE_COUNT = 8;
			int patch_count = BOUNCE_RT_DIM / SAMPLE_COUNT;

			for( int y = 0; y < patch_count; y ++ )
			{
				for( int x = 0; x < patch_count; x ++ )
				{
					float sc = float( SAMPLE_COUNT ) / BOUNCE_RT_DIM;

					float u = x * sc;
					float v = 1.0f - y * sc;

					D3DXVECTOR4 ppVec( n* ( u * 2 - 1 ), - n * ( v * 2 - 1 ), 0, n );

					D3DXVec4Transform( &ppVec, &ppVec, &invViewNoTProj );

					float psConsts[ 2 ][ 4 ] = {
						// float4 texCoord       : register( c4 );
						{ u, v, 0.f, 0.f },
						// float4 viewVec        : register( c5 );
						{ ppVec.x, ppVec.y, ppVec.z, 0.f }
					};

					D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 4, psConsts[0], sizeof psConsts / sizeof( float[4] ) ) );

					r3dDrawFullScreenQuad( false );
				}
			}
			
			m_BounceAccumSHRRT->Deactivate();
			m_BounceAccumSHGRT->Deactivate();
			m_BounceAccumSHBRT->Deactivate();

			IDirect3DSurface9 *redSurf, *greenSurf, *blueSurf;

			D3D_V( m_BounceSysmemRT_R->GetSurfaceLevel( 0, &redSurf ) );
			D3D_V( m_BounceSysmemRT_G->GetSurfaceLevel( 0, &greenSurf ) );
			D3D_V( m_BounceSysmemRT_B->GetSurfaceLevel( 0, &blueSurf ) );

			D3D_V( r3dRenderer->pd3ddev->GetRenderTargetData( m_BounceAccumSHRRT->GetTex2DSurface(), redSurf  ) );
			D3D_V( r3dRenderer->pd3ddev->GetRenderTargetData( m_BounceAccumSHGRT->GetTex2DSurface(), greenSurf  ) );
			D3D_V( r3dRenderer->pd3ddev->GetRenderTargetData( m_BounceAccumSHBRT->GetTex2DSurface(), blueSurf  ) );

			ReadBounceTextureSH( &probe->SH_BounceR[ i ], redSurf );
			ReadBounceTextureSH( &probe->SH_BounceG[ i ], greenSurf );
			ReadBounceTextureSH( &probe->SH_BounceB[ i ], blueSurf );

			SAFE_RELEASE( redSurf );
			SAFE_RELEASE( greenSurf ); 
			SAFE_RELEASE( blueSurf );
		}
		D3DPERF_EndEvent();
	}
}

//------------------------------------------------------------------------

void ProbeMaster::StopUpdatingBounce()
{
	r_use_oq->SetInt( m_SavedUseOQ );
	gCam = m_SavedCam;

	r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW );

	r3dRenderer->SetVertexShader();
	r3dRenderer->SetPixelShader();
}

//------------------------------------------------------------------------

void ProbeMaster::StartUpdatingLocalVPLBounce()
{
	m_SavedCam = gCam;
	m_SavedUseOQ = r_use_oq->GetInt();

	gCam.FOV = 120;

	r_use_oq->SetInt( 0 );

	D3DPERF_BeginEvent( 0, L"UpdatingLocalVPLBounces" );
}

//------------------------------------------------------------------------

void ProbeMaster::GatherClosestProbesForVPL( Probe* target, ProbeIdx (&probes)[ Probe::NUM_LOCAL_VPL ] )
{
	g_ProbesIdxesWithDistances.Clear();

	int tileX = target->CellX / m_Info.ProbeCellsInTileX;
	int tileZ = target->CellZ / m_Info.ProbeCellsInTileZ;

	for( int z = tileZ - VPL_GATHER_TILE_RANGE, e = tileZ + VPL_GATHER_TILE_RANGE ; z <= e ; z ++ )
	{
		for( int x = tileX - VPL_GATHER_TILE_RANGE, e = tileX + VPL_GATHER_TILE_RANGE ; x <= e ; x ++ )
		{
			if( x < 0 || x >= (int)m_ProbeMap.Width()
				||
				z < 0 || z >= (int)m_ProbeMap.Height()
				)
				continue;

			ProbeTile& pt = m_ProbeMap[ z ][ x ];

			for( int i = 0, e = pt.TheProbes.Count(); i < e; i ++ )
			{
				Probe* gegenProbe = &pt.TheProbes[ i ];

				if( gegenProbe == target )
					continue;

				DistanceProbeIdxEntry dpe;

				dpe.distance = ( gegenProbe->Position - target->Position ).Length();

				dpe.probeIdx.TileX = x;
				dpe.probeIdx.TileZ = z;
				dpe.probeIdx.InTileIdx = i;

				g_ProbesIdxesWithDistances.PushBack( dpe );
			}
		}
	}

	struct sortfunc
	{
		bool operator()( const DistanceProbeIdxEntry& e0, const DistanceProbeIdxEntry& e1 )
		{
			return e0.distance < e1.distance;
		}
	};

	if( g_ProbesIdxesWithDistances.Count() )
	{
		std::sort( &g_ProbesIdxesWithDistances[ 0 ], &g_ProbesIdxesWithDistances[ 0 ] + g_ProbesIdxesWithDistances.Count(), sortfunc() );
	}

	int targetCount = R3D_MIN( (int)g_ProbesIdxesWithDistances.Count(), (int)Probe::NUM_LOCAL_VPL );

	for( int i = 0, e = targetCount; i < e; i ++ )
	{
		probes[ i ] = g_ProbesIdxesWithDistances[ i ].probeIdx;
	}

	for( int i = targetCount, e = Probe::NUM_LOCAL_VPL; i < e; i ++ )
	{
		probes[ i ].CombinedIdx = -1;
	}
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateLocalVPLBounce( Probe* probe )
{
	struct EnableDisableDistanceCull
	{
		EnableDisableDistanceCull()
		{
			oldValue = r_allow_distance_cull->GetInt();
			r_allow_distance_cull->SetInt( 0 );
		}

		~EnableDisableDistanceCull()
		{
			r_allow_distance_cull->SetInt( oldValue );
		}

		int oldValue;

	} enableDisableDistanceCull; (void)enableDisableDistanceCull;

	ProbeIdx closestProbes[ Probe::NUM_LOCAL_VPL ];

	GatherClosestProbesForVPL( probe, closestProbes );

	for( int i = 0, e = Probe::NUM_LOCAL_VPL; i < e; i ++ )
	{
		probe->SH_LocalVPLsR[ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
		probe->SH_LocalVPLsG[ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
		probe->SH_LocalVPLsB[ i ] = D3DXVECTOR4( 0, 0, 0, 0 );
	}

	for( int d = 0, e = Probe::NUM_DIRS; d < e; d ++ )
	{
		wchar_t evname[ 512 ];
		swprintf( evname, L"ProbeMaster::UpdateLocalVPLBounce: %d", d );

		D3DPERF_BeginEvent( 0, evname );
		{
			gCam.x = probe->Position.x;
			gCam.y = probe->Position.y;
			gCam.z = probe->Position.z;

			gCam.NearClip = 0.1f;
			gCam.FarClip = 4096.0f;

			r3dPoint3D viewDir = Probe::ViewDirs[ d ];

			gCam.vUP = Probe::UpVecs[ d ];
			gCam.PointTo( gCam + viewDir );

			void UpdateTerrain2Atlas();
			UpdateTerrain2Atlas();

			m_BounceDiffuseRT->Activate( 0 );
			m_BounceNormalRT->Activate( 1 );
			m_BounceDepthRT->Activate( 2 );

			r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW );

			D3D_V( r3dRenderer->pd3ddev->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000, r3dRenderer->GetClearZValue(), 0 ) );

			r3dRenderer->SetCamera( gCam, false );

			void DrawTerrain();
			DrawTerrain();

			r3dRenderer->SetCamera( gCam, false );

			D3DXVECTOR4 CamVec = D3DXVECTOR4(gCam.x, gCam.y, gCam.z, 1);
			r3dRenderer->pd3ddev->SetPixelShaderConstantF(MC_CAMVEC, (float*)&CamVec, 1);

			float defSSAO[ 4 ] = { r_ssao_clear_val->GetFloat(), 0.f, 0.f, 0.f };
			r3dRenderer->pd3ddev->SetPixelShaderConstantF(MC_DEF_SSAO, defSSAO, 1);

			r3dRenderer->SetMipMapBias( 0.f );

			for( int ii = 0; ii < 6; ii ++ )
			{
				r3dRenderer->pd3ddev->SetSamplerState( ii, D3DSAMP_ADDRESSU,   D3DTADDRESS_WRAP );
				r3dRenderer->pd3ddev->SetSamplerState( ii, D3DSAMP_ADDRESSV,   D3DTADDRESS_WRAP );

				r3dSetFiltering( R3D_BILINEAR, ii );
			}

			GameWorld().Prepare( gCam );
			GameWorld().Draw( rsFillGBuffer );

			m_BounceDiffuseRT->Deactivate();
			m_BounceNormalRT->Deactivate();
			m_BounceDepthRT->Deactivate();

			r3dRenderer->SetRenderingMode( R3D_BLEND_ADD | R3D_BLEND_NZ );

			r3dRenderer->SetTex( m_BounceDiffuseRT->Tex, 0 );
			r3dRenderer->SetTex( m_BounceNormalRT->Tex, 1 );
			r3dRenderer->SetTex( m_BounceDepthRT->Tex, 2 );

			{
				D3DPERF_BeginEvent( 0, L"Project On SH" );

				for( int i = 0; i < 3; i ++ )
				{
					D3D_V( r3dRenderer->pd3ddev->SetSamplerState( i, D3DSAMP_ADDRESSU,   D3DTADDRESS_CLAMP ) );
					D3D_V( r3dRenderer->pd3ddev->SetSamplerState( i, D3DSAMP_ADDRESSV,   D3DTADDRESS_CLAMP ) );
				}

				for( int i = 0; i < 3; i ++ )
				{
					r3dSetFiltering( R3D_POINT, i );
				}

				//------------------------------------------------------------------------

				{
					float psConsts[ 1 ][ 4 ] = {
						// float3 g_CamPos                 : register( c0 );
						gCam.x, gCam.y, gCam.z, 0.f
					};

					D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, psConsts[0], sizeof psConsts / sizeof( float[4] ) ) );
				}

				r3dRenderer->SetVertexShader( g_AccumVPLSH_VS_ID );
				r3dRenderer->SetPixelShader( g_AccumVPLSH_PS_ID );

				r3dRenderer->SetVertexDecl( m_SHProjectVertexDecl );

				r3dVertexBuffer* targetBuff = m_SHProjectVertexBuffers[ d ];
				targetBuff->Set( 0 );

				for( int i = 0, e = Probe::NUM_LOCAL_VPL; i < e; i ++ )
				{
					if( closestProbes[ i ].CombinedIdx == -1 )
						continue;

					ProbeIdx pidx = closestProbes[ i ];
					const Probe* probeForVPL = &m_ProbeMap[ pidx.TileZ ][ pidx.TileX ].TheProbes[ pidx.InTileIdx ];

					float psConsts[ 5 ][ 4 ] = {
						// float4 g_LightPos   : register( c1 );
						probeForVPL->Position.x, probeForVPL->Position.y, probeForVPL->Position.z, 0.f
						// float4x4 g_VPMtx    : register( c2 );
					};

					D3DXMatrixTranspose( (D3DXMATRIX*)&psConsts[ 1 ][ 0 ], &r3dRenderer->ViewProjMatrix );

					D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 1, psConsts[0], 1 ) );

					m_BounceAccumSHRRT->Activate( 0 );
					m_BounceAccumSHGRT->Activate( 1 );
					m_BounceAccumSHBRT->Activate( 2 );

					D3D_V( r3dRenderer->pd3ddev->Clear( 0, NULL, D3DCLEAR_TARGET, 0, r3dRenderer->GetClearZValue(), 0 ) );

					r3dRenderer->Draw( D3DPT_POINTLIST, 0, targetBuff->GetItemCount() );

					m_BounceAccumSHRRT->Deactivate();
					m_BounceAccumSHGRT->Deactivate();
					m_BounceAccumSHBRT->Deactivate();

					IDirect3DSurface9 *redSurf, *greenSurf, *blueSurf;

					D3D_V( m_BounceSysmemRT_R->GetSurfaceLevel( 0, &redSurf ) );
					D3D_V( m_BounceSysmemRT_G->GetSurfaceLevel( 0, &greenSurf ) );
					D3D_V( m_BounceSysmemRT_B->GetSurfaceLevel( 0, &blueSurf ) );

					D3D_V( r3dRenderer->pd3ddev->GetRenderTargetData( m_BounceAccumSHRRT->GetTex2DSurface(), redSurf  ) );
					D3D_V( r3dRenderer->pd3ddev->GetRenderTargetData( m_BounceAccumSHGRT->GetTex2DSurface(), greenSurf  ) );
					D3D_V( r3dRenderer->pd3ddev->GetRenderTargetData( m_BounceAccumSHBRT->GetTex2DSurface(), blueSurf  ) );

					D3DXVECTOR4 localVPL_R, localVPL_G, localVPL_B;

					ReadPVLTextureSH( &localVPL_R, redSurf );
					ReadPVLTextureSH( &localVPL_G, greenSurf );
					ReadPVLTextureSH( &localVPL_B, blueSurf );

					probe->SH_LocalVPLsR[ i ] += localVPL_R;
					probe->SH_LocalVPLsG[ i ] += localVPL_G;
					probe->SH_LocalVPLsB[ i ] += localVPL_B;

					probe->SH_LocalVPLProbeIndexes[ i ] = pidx;

					SAFE_RELEASE( redSurf );
					SAFE_RELEASE( greenSurf );
					SAFE_RELEASE( blueSurf );
				}

				D3DPERF_EndEvent();
			}
		}
		D3DPERF_EndEvent();
	}
}

//------------------------------------------------------------------------

void ProbeMaster::StopUpdatingLocalVPLBounce()
{
	r_use_oq->SetInt( m_SavedUseOQ );
	gCam = m_SavedCam;

	r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW );

	r3dRenderer->SetVertexShader();
	r3dRenderer->SetPixelShader();

	D3DPERF_EndEvent();
}

//------------------------------------------------------------------------

void ProbeMaster::StartUpdatingSkyVisibility()
{
	m_SavedUseOQ = r_use_oq->GetInt();
	r_use_oq->SetInt( 0 );
	r_depth_mode->SetInt( 1 );

	m_SavedCam = gCam;
	m_TempRT->Activate( 0 );

	r3dRenderer->SetVertexShader( "VS_FWD_COLOR" );
	r3dRenderer->SetPixelShader( "PS_FWD_COLOR" );

	float psConst[ 4 ] = { 0.F, 0.F, 0.F, 0.F };
	D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, psConst, 1 ) );

	r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW );
}

//------------------------------------------------------------------------

static r3dPoint3D vFaceCamZDir[ r3dScreenBuffer::FACE_COUNT ] = 
{
	r3dPoint3D( +1.f, +0.f, +0.f ),
	r3dPoint3D( -1.f, +0.f, +0.f ),
	r3dPoint3D( +0.f, +1.f, +0.f ),
	r3dPoint3D( +0.f, -1.f, +0.f ),
	r3dPoint3D( +0.f, +0.f, +1.f ),
	r3dPoint3D( +0.f, +0.f, -1.f )
};

static r3dPoint3D vFaceCamYDir[ r3dScreenBuffer::FACE_COUNT ] =
{
	r3dPoint3D( +0.f, +1.f, +0.f ),
	r3dPoint3D( +0.f, +1.f, +0.f ),
	r3dPoint3D( +0.f, +0.f, -1.f ),
	r3dPoint3D( +0.f, +0.f, +1.f ),
	r3dPoint3D( +0.f, +1.f, +0.f ),
	r3dPoint3D( +0.f, +1.f, +0.f )
};

static void SetupCamForFace( r3dCamera* oCam, const r3dPoint3D& pos, int f )
{
	float fov = 90.f;

	oCam->x = pos.x;
	oCam->y = pos.y;
	oCam->z = pos.z;

	oCam->NearClip	= 0.05f;
	oCam->FarClip	= 4096.f;

	oCam->vPointTo	= vFaceCamZDir[ f ];
	oCam->vUP		= vFaceCamYDir[ f ];

	// perspective projection matrix 
	oCam->FOV		= fov;

	oCam->ProjectionType	= r3dCamera::PROJTYPE_PRESPECTIVE;
	oCam->Width				= 0.f;
	oCam->Height			= 0.f;
	oCam->Aspect			= 1.0f;
}

void ProbeMaster::UpdateProbeSkyVisibility( Probe* probe )
{
	D3DPERF_BeginEvent( 0, L"ProbeMaster::UpdateProbeSkyVisibility" );

	struct Surf
	{
		Surf( IDirect3DTexture9* a_sysmemTex )
		{
			D3D_V( a_sysmemTex->GetSurfaceLevel( 0, &ptr ) );
		}

		~Surf( )
		{
			ptr->Release();
		}

		IDirect3DSurface9* ptr;
	} sysmemSurf( m_SysmemTex );

	for( int f = 0; f < 6; f ++ )
	{
#define TEXTIZE_CASE(val) case val: msg = L#val; break;

		const wchar_t * msg = L"Unknown!";

		switch( f )
		{			
			TEXTIZE_CASE(D3DCUBEMAP_FACE_NEGATIVE_X)
			TEXTIZE_CASE(D3DCUBEMAP_FACE_POSITIVE_X)
			TEXTIZE_CASE(D3DCUBEMAP_FACE_NEGATIVE_Y)
			TEXTIZE_CASE(D3DCUBEMAP_FACE_POSITIVE_Y)
			TEXTIZE_CASE(D3DCUBEMAP_FACE_NEGATIVE_Z)
			TEXTIZE_CASE(D3DCUBEMAP_FACE_POSITIVE_Z)
		}
#undef TEXTIZE_CASE

		D3DPERF_BeginEvent( 0, msg );

		D3D_V( r3dRenderer->pd3ddev->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xffffffff, r3dRenderer->GetClearZValue(), 0 ) );

		SetupCamForFace( &gCam, probe->Position, f );

		r3dRenderer->SetCamera( gCam, false );

		void RenderDepthScene();
		RenderDepthScene();

		D3D_V( r3dRenderer->pd3ddev->GetRenderTargetData( m_TempRT->GetTex2DSurface(), sysmemSurf.ptr ) );

		D3DLOCKED_RECT lrect;
		m_SysmemTex->LockRect( 0, &lrect, NULL, D3DLOCK_READONLY );

		CopyPixels( &m_RTBytesR, lrect.pBits, f );

		m_SysmemTex->UnlockRect( 0 );

		D3DPERF_EndEvent();
	}

	g_SampleSphericalSource = &m_RTBytesR;

	for( int i = 0; i < Probe::NUM_DIRS; i ++ )
	{
		probe->SkyVisibility[ i ] = Integrate( SampleSpherical, i );
	}

	D3DPERF_EndEvent();
}

//------------------------------------------------------------------------

void ProbeMaster::StopUpdatingSkyVisibility()
{
	r_use_oq->SetInt( m_SavedUseOQ );
	r_depth_mode->SetInt( 0 );

	r3dRenderer->SetVertexShader();
	r3dRenderer->SetPixelShader();

	m_TempRT->Deactivate();
	gCam = m_SavedCam;
}

//------------------------------------------------------------------------

void ProbeMaster::StartProbesVisualization( ProbeVisualizationMode mode )
{
	r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_ZW | R3D_BLEND_PUSH );

	r3dRenderer->SetCullMode( D3DCULL_CCW );

	r3dDrawGeoSpheresStart();

	m_VisMode = mode;

	r3dRenderer->SetVertexShader( "VS_SPHERICAL_VISUALIZE" );

	switch( mode )
	{
	case VISUALIZE_SKY_VISIBILITY:
		r3dRenderer->SetPixelShader( "PS_SPHERICAL_VISUALIZE_GREY" );
		break;
	case VISUALIZE_SKY_SH_MUL_VISIBILITY:
		r3dRenderer->SetPixelShader( "PS_SPHERICAL_VISUALIZE_RGB_SKY" );
		break;
	case VISUALIZE_PROBE_STATIC_COLORS:
	case VISUALIZE_PROBE_DYNAMIC_COLORS:
	case VISUALIZE_PROBE_COMPOSITE_COLORS:
		r3dRenderer->SetPixelShader( "PS_SPHERICAL_VISUALIZE_LITPROBE" );		
		break;
	default:
		r3dRenderer->SetPixelShader( "PS_SPHERICAL_VISUALIZE_RGB" );
		break;
	}

	D3DXMATRIX mtx;
	D3DXMatrixTranspose( &mtx, &r3dRenderer->ViewProjMatrix );

	// float4x4 matVP : register( c3 );
	D3D_V( r3dRenderer->pd3ddev->SetVertexShaderConstantF( 3, &mtx._11, 4 ) );

}

//------------------------------------------------------------------------

void ProbeMaster::VisualizeProbe( const Probe* probe )
{
	D3DXMATRIX mtx, scl;

	const float S = PROBE_DISPLAY_SCALE;

	D3DXMatrixScaling( &scl, S, S, S );
	D3DXMatrixTranslation( &mtx, probe->Position.x, probe->Position.y, probe->Position.z );
	D3DXMatrixMultiply( &mtx, &scl, &mtx );
	D3DXMatrixTranspose( &mtx, &mtx );

	// float4x3 matW : register( c0 );
	D3D_V( r3dRenderer->pd3ddev->SetVertexShaderConstantF( 0, &mtx._11, 3 ) );

	float psConsts[ 11 ][ 4 ];

	D3DXVECTOR4 src0, src1, src2;

	switch( m_VisMode )
	{
	case VISUALIZE_SUN_SH:
		src0 = m_SunSH_R;
		src1 = m_SunSH_G;
		src2 = m_SunSH_B;
		break;

	case VISUALIZE_SKY_SH:
	case VISUALIZE_SKY_SH_MUL_VISIBILITY:
		src0 = m_SkyDomeSH_R;
		src1 = m_SkyDomeSH_G;
		src2 = m_SkyDomeSH_B;
		break;
	default:
		{
			int dirIdx = 0;

			if( m_VisMode >= VISUALIZE_BOUNCE_DIR0 && m_VisMode <= VISUALIZE_BOUNCE_LAST )
			{
				dirIdx = m_VisMode - VISUALIZE_BOUNCE_DIR0;
			}

			src0 = probe->SH_BounceR[ dirIdx ];
			src1 = probe->SH_BounceG[ dirIdx ];
			src2 = probe->SH_BounceB[ dirIdx ];
		}
		break;
	}

	// float4 gSphericalCoefsR : register( c0 );
	memcpy( psConsts[ 0 ], &src0, 16 );
	// float4 gSphericalCoefsG : register( c1 );
	memcpy( psConsts[ 1 ], &src1, 16 );
	// float4 gSphericalCoefsB : register( c2 );
	memcpy( psConsts[ 2 ], &src2, 16 );

	r3dPoint3D basisDir;
	// float3 gBasisDir0 : register( c3 );
	// float3 gBasisDir1 : register( c4 );
	// float3 gBasisDir2 : register( c5 );
	// float3 gBasisDir3 : register( c6 );	

	for( int i = 0; i < 4; i ++ )
	{
		basisDir =  Probe::ViewDirs[ i ];

		if( m_VisMode == VISUALIZE_SKY_VISIBILITY
				||
			m_VisMode == VISUALIZE_SKY_SH_MUL_VISIBILITY )
		{
			basisDir *= probe->SkyVisibility[ i ];
		}

		memcpy( psConsts[ 3 + i ], &basisDir, 12 );
		psConsts[ 3 + i ][ 3 ] = 0.f;
	}

	for( int i = 0; i < 4; i ++ )
	{
		float colors[ 4 ] = { 0, 0, 0, 1 };

		union
		{
			struct
			{
				UINT16 B : 5;
				UINT16 G : 6;
				UINT16 R : 5;
			};
			UINT16 col;
		};

		union
		{
			struct
			{
				UINT32 B8 : 8;
				UINT32 G8 : 8;
				UINT32 R8 : 8;
				UINT32 A8 : 8;
			};
			UINT32 col32;
		};

#if 0
		col = probe->BasisColors[ i ];
#endif

		switch( m_VisMode )
		{
		case VISUALIZE_PROBE_STATIC_COLORS:
			col32 = probe->BasisColors32[ i ];
			break;

		case VISUALIZE_PROBE_DYNAMIC_COLORS:
			col32 = probe->DynamicColors32[ i ];
			break;

		default:
			col32 = probe->CompositeColors32[ i ];
			break;
		}

#if 0
		if( m_Settings.ProbeTextureFmt == D3DFMT_R5G6B5 )
		{
			float invamp = 1.0f / r_lp_16bit_amplify->GetFloat();

			colors[ 0 ] = R / 31.f * invamp;
			colors[ 1 ] = G / 63.f * invamp;
			colors[ 2 ] = B / 31.f * invamp;
		}
		else
#endif
		{
			colors[ 0 ] = R8 / 255.0f;
			colors[ 1 ] = G8 / 255.0f;
			colors[ 2 ] = B8 / 255.0f;
		}


		memcpy( psConsts[ 7 + i ], colors, sizeof ( float[4] ) );
	}

	D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, psConsts[0], sizeof psConsts / sizeof( float[4] ) ) );

	r3dDrawGeoSphere();
}

//------------------------------------------------------------------------

void ProbeMaster::StopProbesVisualization()
{
	r3dRenderer->RestoreCullMode();
	r3dRenderer->SetRenderingMode( R3D_BLEND_POP );

	r3dDrawGeoSpheresEnd();

	r3dRenderer->SetVertexShader();
	r3dRenderer->SetPixelShader();
}

//------------------------------------------------------------------------

void ProbeMaster::StartProbeDirections()
{
	r3dRenderer->SetCullMode( D3DCULL_NONE );
	r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_ZC | R3D_BLEND_PUSH );

	r3dRenderer->SetVertexShader( "VS_FWD_COLOR" );
	r3dRenderer->SetPixelShader( "PS_FWD_COLOR" );

	float psConst[ 4 ] = { 0.f, 1.f, 0.f, 1.f };
	D3D_V( r3dRenderer->pd3ddev->SetPixelShaderConstantF( 0, psConst, 1 ) );

	D3DXMATRIX wvp;

	D3DXMatrixTranspose( &wvp, &r3dRenderer->ViewProjMatrix );

	D3D_V( r3dRenderer->pd3ddev->SetVertexShaderConstantF( 0, &wvp._11, 4 ) );
}

//------------------------------------------------------------------------

void ProbeMaster::DrawProbeDirection( const Probe* probe, int dirIdx )
{
	r3d_assert( dirIdx >= 0 && dirIdx < Probe::NUM_DIRS );

	r3dPoint3D src ( probe->Position.x, probe->Position.y, probe->Position.z );

	src += Probe::ViewDirs[ dirIdx ];

	r3dPoint3D targ = src + Probe::ViewDirs[ dirIdx ];

	r3dDrawUniformLine3D( src, targ , gCam, r3dColor::green );

	r3dCone cone;

	cone.fFOV = R3D_DEG2RAD( 30.f );
	cone.vCenter = targ;
	cone.vDir = -Probe::ViewDirs[ dirIdx ];

	r3dDrawConeSolid( cone, gCam, r3dColor::green, 0.125f );
}

//------------------------------------------------------------------------

void ProbeMaster::DrawProbeDirections( const Probe* probe )
{
	if( m_VisMode >= VISUALIZE_BOUNCE_DIR0 && m_VisMode <= VISUALIZE_BOUNCE_LAST )
	{
		int dirIdx = m_VisMode - VISUALIZE_BOUNCE_DIR0;
		DrawProbeDirection( probe, dirIdx );
	}
	else
	{
		for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
		{
			DrawProbeDirection( probe, i );
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::StopProbeDirections()
{
	r3dRenderer->Flush();
	r3dRenderer->RestoreCullMode();
	r3dRenderer->SetRenderingMode( R3D_BLEND_POP );
	r3dRenderer->SetVertexShader();
	r3dRenderer->SetPixelShader();
}

//------------------------------------------------------------------------

void ProbeMaster::RasterizeProbeIntoVolume( Probe* probe, const D3DBOX& box, int lockedStartX, int lockedStartY, int lockedStartZ, int lockedSizeX, int lockedSizeY, int lockedSizeZ )
{
	int px = probe->CellX;
	int py = probe->CellY;
	int pz = probe->CellZ;

	int lockedSizeXY = lockedSizeX * lockedSizeY;

	if( probe->XRadius < 1 && probe->YRadius < 1 && probe->ZRadius < 1 )
	{
		int acx = m_Info.ProbeVolumeCamCellX + ( px - m_Info.CamCellX );
		int acy = m_Info.ProbeVolumeCamCellY + ( py - m_Info.CamCellY );
		int acz = m_Info.ProbeVolumeCamCellZ + ( pz - m_Info.CamCellZ );

		acx %= m_Settings.ProbeTextureWidth;
		acy %= m_Settings.ProbeTextureDepth;
		acz %= m_Settings.ProbeTextureHeight;

		if( acx < 0 ) acx += m_Settings.ProbeTextureWidth;
		if( acy < 0 ) acy += m_Settings.ProbeTextureDepth;
		if( acz < 0 ) acz += m_Settings.ProbeTextureHeight;

		r3d_assert( acx >= (int)box.Left && acx < (int)box.Right );
		r3d_assert( acy >= (int)box.Front && acy < (int)box.Back );
		r3d_assert( acz >= (int)box.Top && acz < (int)box.Bottom );

		acx -= box.Left;
		acy -= box.Front;
		acz -= box.Top;

		int idx = acx + acy * lockedSizeX + acz * lockedSizeXY;

		AddRasterSample( idx, probe );
	}
	else
	{
		for( int z = pz - probe->ZRadius, e = pz + probe->ZRadius; z <= e; z ++ )
		{
			for( int y = py - probe->YRadius, e = py + probe->YRadius; y <= e; y ++ )
			{
				for( int x = px - probe->XRadius, e = px + probe->XRadius; x <= e; x ++ )
				{
					if( x < lockedStartX || 
						y < lockedStartY || 
						z < lockedStartZ ||
						x >= lockedStartX + lockedSizeX || 
						y >= lockedStartY + lockedSizeY || 
						z >= lockedStartZ + lockedSizeZ )
						continue;

					int acx = m_Info.ProbeVolumeCamCellX + ( x - m_Info.CamCellX );
					int acy = m_Info.ProbeVolumeCamCellY + ( y - m_Info.CamCellY );
					int acz = m_Info.ProbeVolumeCamCellZ + ( z - m_Info.CamCellZ );

					acx %= m_Settings.ProbeTextureWidth;
					acy %= m_Settings.ProbeTextureDepth;
					acz %= m_Settings.ProbeTextureHeight;

					if( acx < 0 ) acx += m_Settings.ProbeTextureWidth;
					if( acy < 0 ) acy += m_Settings.ProbeTextureDepth;
					if( acz < 0 ) acz += m_Settings.ProbeTextureHeight;

					r3d_assert( acx >= (int)box.Left && acx < (int)box.Right );
					r3d_assert( acy >= (int)box.Front && acy < (int)box.Back );
					r3d_assert( acz >= (int)box.Top && acz < (int)box.Bottom );

					acx -= box.Left;
					acy -= box.Front;
					acz -= box.Top;

					int idx = acx + acy * lockedSizeX + acz * lockedSizeXY;

					AddRasterSample( idx, probe );
				}
			}
		}
	}
}

//------------------------------------------------------------------------

static float SampleHemisphereLighting( float th, float ph )
{
	D3DXVECTOR3 vec( cos( ph ) * sin( th ), cos( th ), sin( ph ) * sin( th ) );

	return R3D_MAX( D3DXVec3Dot( &vec, &g_HemisphereNormal ), 0.f );
}

static const char* SH_TEXTURE_PATH = "Data/Tests/normal_to_sh.dds";

void ProbeMaster::FillNormalToSHTex()
{
	if( m_NormalToSHTex )
	{
		r3dRenderer->DeleteTexture( m_NormalToSHTex );
		m_NormalToSHTex = NULL;
	}

	if( r3dFileExists( SH_TEXTURE_PATH ) )
	{
		m_NormalToSHTex = r3dRenderer->LoadTexture( SH_TEXTURE_PATH );
		return;
	}

	float genStart = r3dGetTime();

	m_NormalToSHTex = r3dRenderer->AllocateTexture();

	m_NormalToSHTex->CreateCubemap( NORMAL_TO_SH_DIM, D3DFMT_A32B32G32R32F, 1 );

	IDirect3DCubeTexture9* cube = m_NormalToSHTex->AsTexCUBE();
	
	for( int f = 0, e = 6; f < e; f ++ )
	{
		r3dOutToLog( "ProbeMaster::FillNormalToSHTex: face %d", f );

		D3DLOCKED_RECT lrect;

		D3D_V( cube->LockRect( D3DCUBEMAP_FACES( f ), 0, &lrect, NULL, 0 ) );

		for( int y = 0, e = NORMAL_TO_SH_DIM; y < e; y ++ )
		{
			for( int x = 0, e = NORMAL_TO_SH_DIM; x < e; x ++ )
			{
				switch( f )
				{
				case D3DCUBEMAP_FACE_POSITIVE_X:
					g_HemisphereNormal.x = +1.0f;
					g_HemisphereNormal.y = float( y ) / NORMAL_TO_SH_DIM - 0.5f;
					g_HemisphereNormal.z = 0.5f - float( x ) / NORMAL_TO_SH_DIM;
					break;
				case D3DCUBEMAP_FACE_NEGATIVE_X:
					g_HemisphereNormal.x = -1.0f;
					g_HemisphereNormal.y = float( y ) / NORMAL_TO_SH_DIM - 0.5f;
					g_HemisphereNormal.z = float( x ) / NORMAL_TO_SH_DIM - 0.5f;
					break;
				case D3DCUBEMAP_FACE_POSITIVE_Y:
					g_HemisphereNormal.x = float( x ) / NORMAL_TO_SH_DIM - 0.5f;
					g_HemisphereNormal.y = +1.0f;
					g_HemisphereNormal.z = 0.5f - float( y ) / NORMAL_TO_SH_DIM;
					break;
				case D3DCUBEMAP_FACE_NEGATIVE_Y:
					g_HemisphereNormal.x = float( x ) / NORMAL_TO_SH_DIM - 0.5f;
					g_HemisphereNormal.y = -1.0f;
					g_HemisphereNormal.z = float( y ) / NORMAL_TO_SH_DIM - 0.5f;
					break;
				case D3DCUBEMAP_FACE_POSITIVE_Z:
					g_HemisphereNormal.x = float( x ) / NORMAL_TO_SH_DIM - 0.5f;
					g_HemisphereNormal.y = float( y ) / NORMAL_TO_SH_DIM - 0.5f;
					g_HemisphereNormal.z = 1.0f;
					break;
				case D3DCUBEMAP_FACE_NEGATIVE_Z:
					g_HemisphereNormal.x = 0.5f - float( x ) / NORMAL_TO_SH_DIM;
					g_HemisphereNormal.y = float( y ) / NORMAL_TO_SH_DIM - 0.5f;
					g_HemisphereNormal.z = -1.0f;
					break;
				}

				D3DXVec3Normalize( &g_HemisphereNormal, &g_HemisphereNormal );

				float* ptr = (float*)( (char*)lrect.pBits + ( NORMAL_TO_SH_DIM - 1 - y ) * lrect.Pitch ) + x * 4;

				ProjectOnSH( *(float(*)[4])ptr, SampleHemisphereLighting );
			}

			r3dOutToLog( "." );
		}

		r3dOutToLog( "\n" );

		D3D_V( cube->UnlockRect( D3DCUBEMAP_FACES( f ), 0 ) );
	}

	D3D_V( D3DXSaveTextureToFile( SH_TEXTURE_PATH, D3DXIFF_DDS, m_NormalToSHTex->AsTexCUBE(), NULL ) );

	r3dOutToLog( "ProbeMaster::FillNormalToSHTex: generated texture in %.2f seconds\n", r3dGetTime() - genStart );
}

//------------------------------------------------------------------------

void ProbeMaster::CreateTempRTAndSysMemTex()
{
	m_TempRT = r3dScreenBuffer::CreateClass( "ProbeMaster", (float)PROBE_RT_DIM, (float)PROBE_RT_DIM, PROBE_RT_FMT, r3dScreenBuffer::Z_OWN );
	D3D_V( r3dRenderer->pd3ddev->CreateTexture( PROBE_RT_DIM, PROBE_RT_DIM, 1, 0, PROBE_RT_FMT, D3DPOOL_SYSTEMMEM, &m_SysmemTex, NULL ) );

	m_RTBytesR.Resize( PROBE_RT_DIM * PROBE_RT_DIM * 6 );
	m_RTBytesG.Resize( PROBE_RT_DIM * PROBE_RT_DIM * 6 );
	m_RTBytesB.Resize( PROBE_RT_DIM * PROBE_RT_DIM * 6 );
}

//------------------------------------------------------------------------

void ProbeMaster::DestroyTempRTAndSysMemTex()
{
	SAFE_DELETE( m_TempRT );
	SAFE_RELEASE( m_SysmemTex );	

	Bytes().Swap( m_RTBytesR );
	Bytes().Swap( m_RTBytesG );
	Bytes().Swap( m_RTBytesB );
}

//------------------------------------------------------------------------

void ProbeMaster::ClearProbeMap()
{
	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e; x ++ )
		{
			ProbeTile& entry = m_ProbeMap.At( x, z );
			entry.TheProbes.Clear();
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::ClearProbeMapTile( int tileX, int tileZ )
{
	if( tileX >= 0 && tileX < m_Info.ActualProbeTileCountX
			&&
		tileZ >= 0 && tileZ < m_Info.ActualProbeTileCountZ
		)
	{
		m_ProbeMap.At( tileX, tileZ ).TheProbes.Clear();
	}
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateBBox( int x, int z, const r3dPoint3D& pos )
{
	ProbeTile& ptile = m_ProbeMap.At( x, z );
	float minY = R3D_MIN( ptile.BBox.Org.y, pos.y - PROBE_DISPLAY_SCALE );
	float maxY = R3D_MAX( ptile.BBox.Org.y + ptile.BBox.Size.y, pos.y + PROBE_DISPLAY_SCALE );

	float minX = R3D_MIN( ptile.BBox.Org.x, pos.x - PROBE_DISPLAY_SCALE );
	float maxX = R3D_MAX( ptile.BBox.Org.x + ptile.BBox.Size.x, pos.x + PROBE_DISPLAY_SCALE );

	float minZ = R3D_MIN( ptile.BBox.Org.z, pos.z - PROBE_DISPLAY_SCALE );
	float maxZ = R3D_MAX( ptile.BBox.Org.z + ptile.BBox.Size.z, pos.z + PROBE_DISPLAY_SCALE );

	ptile.BBox.Org = r3dPoint3D( minX, minY, minZ );
	ptile.BBox.Size = r3dPoint3D( maxX - minX, maxY - minY, maxZ - minZ );

}

//------------------------------------------------------------------------

int ProbeMaster::AddProbe( int x, int z, const Probe& probe )
{
	if( IsOccupied( probe.Position ) )
		return -1;

	m_ProbeRadiusesDirty = 1;

	UpdateBBox( x, z, probe.Position );

	ProbeTile& ptile = m_ProbeMap.At( x, z );

	ptile.TheProbes.PushBack( probe );

	return ptile.TheProbes.Count() - 1;
}

//------------------------------------------------------------------------

void ProbeMaster::ResetProbeMapBBox( int tileX, int tileZ )
{
	if( tileX >= 0 && tileX < m_Info.ActualProbeTileCountX
		&&
		tileZ >= 0 && tileZ < m_Info.ActualProbeTileCountZ
		)
	{
		float tileSizeX = m_Info.ProbeMapWorldActualXSize / m_ProbeMap.Width();
		float tileSizeZ = m_Info.ProbeMapWorldActualZSize / m_ProbeMap.Height();

		ProbeTile& pt = m_ProbeMap[ tileZ ][ tileX ];

		pt.BBox.Org = r3dPoint3D( m_Info.ProbeMapWorldXStart + tileX * tileSizeX, FLT_MAX, m_Info.ProbeMapWorldZStart + tileZ * tileSizeZ );
		pt.BBox.Size = r3dPoint3D( tileSizeX, -FLT_MAX, tileSizeZ );
	}
}

//------------------------------------------------------------------------

void ProbeMaster::ResetProbeMapBBoxes()
{
	for( int z = 0, e = m_ProbeMap.Height(); z < e; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Height(); x < e; x ++ )
		{
			ResetProbeMapBBox( x, z );
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::RecreateVolumeTexes()
{
	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		if( r3dTexture * tex = m_DirectionVolumeTextures[ i ] )
			r3dRenderer->DeleteTexture( tex );

		m_DirectionVolumeTextures[ i ] = NULL;
	}

	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		r3dTexture* volumeTex( NULL );
		volumeTex = r3dRenderer->AllocateTexture();
		volumeTex->CreateVolume( m_Settings.ProbeTextureWidth, m_Settings.ProbeTextureHeight, m_Settings.ProbeTextureDepth, m_Settings.ProbeTextureFmt, 1 );

		m_DirectionVolumeTextures[ i ] = volumeTex;
	}

	m_ProbeVolumeDirtyArr.Resize( m_Settings.ProbeTextureWidth / PROBE_DIRTY_ARR_CELL_SIZE, m_Settings.ProbeTextureHeight / PROBE_DIRTY_ARR_CELL_SIZE, 0 );
	if( g_bEditMode )
	{
		m_ProbeVolumeDirtyAnimArr.Resize( m_ProbeVolumeDirtyArr.Width(), m_ProbeVolumeDirtyArr.Height() );
	}

}

//------------------------------------------------------------------------

void ProbeMaster::FindClosestProbe( int * oIdx, float* oDist, int tileX, int tileZ, Probe* exclude )
{
	float posX, posY, posZ;

	posX = exclude->Position.x;
	posY = exclude->Position.y;
	posZ = exclude->Position.z;

	const ProbeTile& ptile = m_ProbeMap[ tileZ ][ tileX ];

	float minDist = FLT_MAX;
	int minDistIdx = -1;

	r3dPoint3D centre( posX, posY, posZ );

	for( int i = 0, e = (int) ptile.TheProbes.Count(); i < e; i ++ )
	{
		const Probe& p = ptile.TheProbes[ i ];

		if( &p == exclude )
			continue;

		float dist = ( p.Position - centre ).Length();

		if( dist < minDist )
		{
			minDist = dist;
			minDistIdx = i;
		}
	}

	*oIdx = minDistIdx;
	*oDist = minDist;

}

//------------------------------------------------------------------------

void ProbeMaster::GetProbeTileIndexes( int * oX, int* oZ, float x, float z ) const
{
	int ix = int( ( x - m_Info.ProbeMapWorldXStart ) * m_ProbeMap.Width() / m_Info.ProbeMapWorldActualXSize );
	int iz = int( ( z - m_Info.ProbeMapWorldZStart ) * m_ProbeMap.Height() / m_Info.ProbeMapWorldActualZSize );

	ix = R3D_MIN( R3D_MAX( ix, 0 ), (int)m_ProbeMap.Width() - 1 );
	iz = R3D_MIN( R3D_MAX( iz, 0 ), (int)m_ProbeMap.Height() - 1 );

	*oX = ix;
	*oZ = iz;
}

//------------------------------------------------------------------------

Probe* ProbeMaster::GetClosestProbe( int cellx, int celly, int cellz )
{
	cellx = R3D_MAX( R3D_MIN( cellx, m_Info.TotalProbeCellsCountX - 1 ), 0 );
	celly = R3D_MAX( R3D_MIN( celly, m_Info.TotalProbeCellsCountY - 1 ), 0 );
	cellz = R3D_MAX( R3D_MIN( cellz, m_Info.TotalProbeCellsCountZ - 1 ), 0 );

	ProbeIdx idx =	GetClosestProbeIdx( cellx, celly, cellz );

	if( idx.InTileIdx == 0xffff )
		return NULL;
	else
		return &m_ProbeMap[ idx.TileZ ][ idx.TileX ].TheProbes[ idx.InTileIdx ];
}

//------------------------------------------------------------------------

Probe* ProbeMaster::GetClosestProbe( int tileX, int tileZ, const r3dPoint3D& pos )
{
	float minDist = FLT_MAX;
	Probe* closest ( NULL );

	tileZ = R3D_MIN( R3D_MAX( tileZ, 0 ), (int)m_ProbeMap.Height() - 1 );
	tileX = R3D_MIN( R3D_MAX( tileX, 0 ), (int)m_ProbeMap.Width() - 1 );

	Probes& pbs = m_ProbeMap[ tileZ ][ tileX ].TheProbes;

	for( int i = 0, e = pbs.Count(); i < e; i ++ )
	{
		float len = ( pbs[ i ].Position - pos ).Length ();

		if( len < minDist )
		{
			minDist = len;
			closest = &pbs[ i ];
		}
	}

	return closest;
}

//------------------------------------------------------------------------


int ProbeMaster::OutputBakeProgress( const char* operationName, int total, int complete )
{
	m_LastInfoFrame = r3dGetTime();

	r3dRenderer->pd3ddev->Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), r3dRenderer->GetClearZValue(), 0 );

	r3dRenderer->SetRenderingMode( R3D_BLEND_NOALPHA | R3D_BLEND_NZ );

	extern	CD3DFont*	MenuFont_Editor;

	char buf[ 512 ];

	r3dRenderer->SetVertexShader();
	r3dRenderer->SetPixelShader();

	sprintf( buf, "%s: %d of %d complete", operationName, total, complete );

	MenuFont_Editor->DrawScaledText( 128.f, 64.f, 2, 2, 1, r3dColor::white, buf, 0 );

	r3dProcessWindowMessages();

	void InputUpdate();
	InputUpdate();
	if( Keyboard->IsPressed( kbsEsc ) )
	{
		return 0;
	}

	r3dRenderer->EndFrame();
	r3dRenderer->EndRender( true );

	r3dRenderer->StartFrame();
	r3dRenderer->StartRender();

	return 1;
}

//------------------------------------------------------------------------

void ProbeMaster::SelectProbe( ProbeIdx idx, const Probe& p )
{
	for( int i = 0, e = m_ProbeProxies.Count(); i <  e; i ++ )
	{
		ProbeProxy* pp = m_ProbeProxies[ i ];

		if( pp->Idx.CombinedIdx == idx.CombinedIdx )
		{
			r3dTL::TArray< GameObject* > objs;

			g_Manipulator3d.GetPickedObjects( &objs );

			for( int i = 0, e = (int)objs.Count(); i < e; i ++ )
			{
				if( objs[ i ] == pp )
					return;
			}

			g_Manipulator3d.PickerAddToPicked( pp );
			return;
		}
	}

	ProbeProxy* obj = static_cast<ProbeProxy*>( srv_CreateGameObject( "ProbeProxy", "proxy", p.Position ) );

	obj->SetIdx( idx );

	m_ProbeProxies.PushBack( obj );

	g_Manipulator3d.PickerAddToPicked( obj );
}

//------------------------------------------------------------------------

void ProbeMaster::RecalcTileBBox( int tileX, int tileZ )
{
	ProbeTile& tile = m_ProbeMap[ tileZ ][ tileX ];

	float tileSizeX = m_Info.ProbeMapWorldActualXSize / m_ProbeMap.Width();
	float tileSizeZ = m_Info.ProbeMapWorldActualZSize / m_ProbeMap.Height();

	tile.BBox.Org = r3dPoint3D( m_Info.ProbeMapWorldXStart + tileX * tileSizeX, FLT_MAX, m_Info.ProbeMapWorldZStart + tileZ * tileSizeZ );
	tile.BBox.Size = r3dPoint3D( tileSizeX, -FLT_MAX, tileSizeZ );

	for( int i = 0, e = (int)tile.TheProbes.Count() ; i < e ; i ++ )
	{
		UpdateBBox( tileX, tileZ, tile.TheProbes[ i ].Position );
	}
}

//------------------------------------------------------------------------

void ProbeMaster::DrawProbeVolumesFrame()
{
	float cellSizeX = m_Info.CellSizeX;
	float cellSizeY = m_Info.CellSizeY;
	float cellSizeZ = m_Info.CellSizeZ;

	float sx = m_Settings.ProbeTextureWidth * cellSizeX;
	float sy = m_Settings.ProbeTextureDepth * cellSizeY;
	float sz = m_Settings.ProbeTextureHeight * cellSizeZ;

	float hsx = sx / 2.0f;
	float hsy = sy / 2.0f;
	float hsz = sz / 2.0f;

	r3dPoint3D centre;

	centre.x = cellSizeX * m_Info.CamCellX + m_Info.ProbeMapWorldXStart;
	centre.y = cellSizeY * m_Info.CamCellY + m_Info.ProbeMapWorldYStart;
	centre.z = cellSizeZ * m_Info.CamCellZ + m_Info.ProbeMapWorldZStart;

	r3dBoundBox bbox;

	bbox.Org = centre - 0.5f * r3dPoint3D( sx, sy, sz );
	bbox.Size = r3dPoint3D( sx, sy, sz );

	r3dDrawUniformBoundBox( bbox, gCam, r3dColor::white );

	for( int x = 1, e = m_Settings.ProbeTextureWidth ; x < e ; x ++ )
	{
		float fx = float( x ) * bbox.Size.x / float( m_Settings.ProbeTextureWidth );

		r3dDrawUniformLine3D(	r3dPoint3D( centre.x - hsx + fx, centre.y - hsy, centre.z - hsz ),
								r3dPoint3D( centre.x - hsx + fx, centre.y - hsy, centre.z + hsz ), gCam, r3dColor::white );
	}

	for( int z = 1, e = m_Settings.ProbeTextureHeight ; z < e ; z ++ )
	{
		float fz = float( z ) * bbox.Size.z / float( m_Settings.ProbeTextureHeight );

		r3dDrawUniformLine3D(	r3dPoint3D( centre.x - hsx, centre.y - hsy, centre.z - hsz + fz ),
								r3dPoint3D( centre.x + hsx, centre.y - hsy, centre.z - hsz + fz ), gCam, r3dColor::white );
	}

	for( int x = 1, e = m_Settings.ProbeTextureWidth ; x < e ; x ++ )
	{
		float fx = float( x ) * bbox.Size.x / float( m_Settings.ProbeTextureWidth );

		r3dDrawUniformLine3D(	r3dPoint3D( centre.x - hsx + fx, centre.y + hsy, centre.z - hsz ),
								r3dPoint3D( centre.x - hsx + fx, centre.y + hsy, centre.z + hsz ), gCam, r3dColor::white );
	}

	for( int z = 1, e = m_Settings.ProbeTextureHeight ; z < e ; z ++ )
	{
		float fz = float( z ) * bbox.Size.z / float( m_Settings.ProbeTextureHeight );

		r3dDrawUniformLine3D(	r3dPoint3D( centre.x - hsx, centre.y + hsy, centre.z - hsz + fz ),
								r3dPoint3D( centre.x + hsx, centre.y + hsy, centre.z - hsz + fz ), gCam, r3dColor::white );
	}

	for( int y = 1, e = m_Settings.ProbeTextureDepth ; y < e ; y ++ )
	{
		float fy = float( y ) * bbox.Size.y / float( m_Settings.ProbeTextureDepth );

		r3dDrawUniformLine3D(	r3dPoint3D( centre.x - hsx, centre.y - hsy + fy, centre.z - hsz ),
								r3dPoint3D( centre.x + hsx, centre.y - hsy + fy, centre.z - hsz ), gCam, r3dColor::white );

		r3dDrawUniformLine3D(	r3dPoint3D( centre.x - hsx, centre.y - hsy + fy, centre.z + hsz ),
								r3dPoint3D( centre.x + hsx, centre.y - hsy + fy, centre.z + hsz ), gCam, r3dColor::white );

		r3dDrawUniformLine3D(	r3dPoint3D( centre.x - hsx, centre.y - hsy + fy, centre.z - hsz ),
								r3dPoint3D( centre.x - hsx, centre.y - hsy + fy, centre.z + hsz ), gCam, r3dColor::white );

		r3dDrawUniformLine3D(	r3dPoint3D( centre.x + hsx, centre.y - hsy + fy, centre.z - hsz ),
								r3dPoint3D( centre.x + hsx, centre.y - hsy + fy, centre.z + hsz ), gCam, r3dColor::white );

	}


	r3dRenderer->Flush();
}

//------------------------------------------------------------------------

void ProbeMaster::UpdateCellsAndSizeParams()
{
	m_Info.CellSizeX = m_Settings.ProbeTextureSpanX / m_Settings.ProbeTextureWidth;
	m_Info.CellSizeY = m_Settings.ProbeTextureSpanY / m_Settings.ProbeTextureDepth;
	m_Info.CellSizeZ = m_Settings.ProbeTextureSpanZ / m_Settings.ProbeTextureHeight;

	int numProxyCellsX = int( m_Info.ProbeMapWorldNominalXSize / m_Info.CellSizeX + 0.5f );
	int numProxyCellsY = int( m_Info.ProbeMapWorldNominalYSize / m_Info.CellSizeY + 0.5f );
	int numProxyCellsZ = int( m_Info.ProbeMapWorldNominalZSize / m_Info.CellSizeZ + 0.5f );

	int numProxyCellsInTileX = numProxyCellsX / m_Settings.NominalProbeTileCountX;
	int numProxyCellsInTileZ = numProxyCellsZ / m_Settings.NominalProbeTileCountZ;

	numProxyCellsInTileX ++;	
	numProxyCellsInTileZ ++;

	m_Info.ProbeCellsInTileX = numProxyCellsInTileX;
	m_Info.ProbeCellsInTileZ = numProxyCellsInTileZ;

	m_Info.ActualProbeTileCountX = numProxyCellsX / numProxyCellsInTileX + 1;
	m_Info.ActualProbeTileCountZ = numProxyCellsZ / numProxyCellsInTileZ + 1;

	numProxyCellsX = numProxyCellsInTileX * m_Info.ActualProbeTileCountX;
	numProxyCellsZ = numProxyCellsInTileZ * m_Info.ActualProbeTileCountZ;

	m_Info.ProbeMapWorldActualXSize = numProxyCellsX * m_Info.CellSizeX;
	m_Info.ProbeMapWorldActualYSize = numProxyCellsY * m_Info.CellSizeY;
	m_Info.ProbeMapWorldActualZSize = numProxyCellsZ * m_Info.CellSizeZ;

	m_Info.TotalProbeCellsCountX = numProxyCellsX;
	m_Info.TotalProbeCellsCountY = numProxyCellsY;
	m_Info.TotalProbeCellsCountZ = numProxyCellsZ;
}

//------------------------------------------------------------------------

void ProbeMaster::ConvertToTileCoords( float x, float z, int* oTileX, int* oTileZ )
{
	int tileX = int( ( x - m_Info.ProbeMapWorldXStart ) / m_Info.ProbeMapWorldActualXSize * m_ProbeMap.Width() );
	int tileZ = int( ( z - m_Info.ProbeMapWorldZStart ) / m_Info.ProbeMapWorldActualZSize * m_ProbeMap.Height() );

	*oTileX = tileX;
	*oTileZ = tileZ;
}

//------------------------------------------------------------------------

void ProbeMaster::FetchAllProbes( Probes* oProbes )
{
	oProbes->Clear();

	for( int z = 0, e = m_ProbeMap.Height(); z < e ; z ++ )
	{
		for( int x = 0, e = m_ProbeMap.Width(); x < e ; x ++ )
		{
			const Probes& probes = m_ProbeMap[ z ][ x ].TheProbes ;

			for( int i = 0, e = (int)probes.Count() ; i < e ; i ++ )
			{
				oProbes->PushBack( probes[ i ] );
			}
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::AddDirtyRadiusTile( ProbeIdx pidx )
{
	for( int z = (int)pidx.TileZ - 1, e = pidx.TileZ + 1 ; z <= e ; z ++ )
	{
		for( int x = (int)pidx.TileX - 1, e = pidx.TileX + 1 ; x <= e ; x ++ )
		{
			if( x < 0 || (int)x >= m_Info.ActualProbeTileCountX
				||
				z < 0 || (int)z >= m_Info.ActualProbeTileCountZ
				)
				continue;

			int found = 0;

			for( int i = 0, e = m_DirtyRadiusesTiles.Count() ; i < e ; i ++ )
			{
				const ProbeIdx& comp = m_DirtyRadiusesTiles[ i ];
				if( x == comp.TileX
						&&
					z == comp.TileZ )
				{
					found = 1;
					break;
				}
			}

			if( found )
				continue;

			ProbeIdx aidx;

			aidx.TileX = x;
			aidx.TileZ = z;

			m_DirtyRadiusesTiles.PushBack( aidx );
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::SetupDefaultLightProbe()
{
	float RCoef_Up = m_Settings.DefaultBounceColor_Up.R / 255.f;
	float GCoef_Up = m_Settings.DefaultBounceColor_Up.G / 255.f;
	float BCoef_Up = m_Settings.DefaultBounceColor_Up.B / 255.f;

	for( int j = 0, e = 4 ; j < e ; j ++ )
	{
		m_DefaultProbe.SH_BounceR[ 0 ][ j ] = DefaultSHBasis[ 0 ][ j ] * RCoef_Up;
		m_DefaultProbe.SH_BounceG[ 0 ][ j ] = DefaultSHBasis[ 0 ][ j ] * GCoef_Up;
		m_DefaultProbe.SH_BounceB[ 0 ][ j ] = DefaultSHBasis[ 0 ][ j ] * BCoef_Up;
	}

	float RCoef_Down = m_Settings.DefaultBounceColor_Down.R / 255.f;
	float GCoef_Down = m_Settings.DefaultBounceColor_Down.G / 255.f;
	float BCoef_Down = m_Settings.DefaultBounceColor_Down.B / 255.f;

	for( int i = 1, e = Probe::NUM_DIRS ; i < e ; i ++ )
	{
		for( int j = 0, e = 4 ; j < e ; j ++ )
		{
			m_DefaultProbe.SH_BounceR[ i ][ j ] = DefaultSHBasis[ i ][ j ] * RCoef_Down;
			m_DefaultProbe.SH_BounceG[ i ][ j ] = DefaultSHBasis[ i ][ j ] * GCoef_Down;
			m_DefaultProbe.SH_BounceB[ i ][ j ] = DefaultSHBasis[ i ][ j ] * BCoef_Down;
		}
	}
}

//------------------------------------------------------------------------

void ProbeMaster::AdjustProbeRadius( Probe* probe, int tileX, int tileZ )
{
	int minXRad = m_Settings.MaximumProbeRadius;
	int minZRad = m_Settings.MaximumProbeRadius;
	int minYRad = m_Settings.MaximumProbeYRadius;

	int mapXIdx = tileX;
	int mapZIdx = tileZ;

	for( int tz = mapZIdx - 1, e = mapZIdx + 1; tz <= e; tz ++ )
	{
		for( int tx = mapXIdx - 1, e = mapXIdx + 1; tx <= e; tx ++ )
		{
			if( tx < 0 || tz < 0 || 
				tx >= (int)m_ProbeMap.Width() || tz >= (int)m_ProbeMap.Height() )
				continue;

			const ProbeTile& pt = m_ProbeMap[ tz ][ tx ];

			for( int i = 0, e = pt.TheProbes.Count(); i < e; i ++ )
			{
				const Probe& adjProbe = pt.TheProbes[ i ];

				if( &adjProbe == probe )
					continue;

				int adx = abs( adjProbe.CellX - probe->CellX );
				int ady = abs( adjProbe.CellY - probe->CellY );
				int adz = abs( adjProbe.CellZ - probe->CellZ );

				if( adx >= adz )
					minXRad = R3D_MIN( adx, minXRad );
				
				if( adz >= adx )
					minZRad = R3D_MIN( adz, minZRad );

				if( ady >= adx && ady >= adz )
				{
					minYRad = R3D_MIN( ady, minYRad );
				}
			}
		}
	}

	int expand = m_Settings.ProbeRadiusExpansion;

	probe->XRadius = minXRad + expand;
	probe->YRadius = minYRad + expand;
	probe->ZRadius = minZRad + expand;
}

//------------------------------------------------------------------------

static void CopyPixels( Bytes* oBytes, void* src, int face )
{
	struct RGB16S
	{
		UINT16 r : 5;
		UINT16 g : 6;
		UINT16 b : 5;
	} * p16b = (RGB16S*)src;

	int count = PROBE_RT_DIM * PROBE_RT_DIM;
	int offset = face * count;

	for( int i = 0, e = count; i < e; i ++ )
	{
		(*oBytes)[ offset ] = ( p16b->r + p16b->g + p16b->b ) * 255 / ( 31 + 63 + 31 );

		offset ++;
		p16b ++;
	}
}

//------------------------------------------------------------------------

static void CopyPixelsRGB( Bytes* oBytesR, Bytes* oBytesG, Bytes* oBytesB, void* src, int face )
{
	struct RGB16S
	{
		UINT16 b : 5;
		UINT16 g : 6;
		UINT16 r : 5;
	} * p16b = (RGB16S*)src;

	int count = PROBE_RT_DIM * PROBE_RT_DIM;
	int offset = face * count;

	for( int i = 0, e = count; i < e; i ++ )
	{
		(*oBytesR)[ offset ] = p16b->r * 255 / 31;
		(*oBytesG)[ offset ] = p16b->g * 255 / 63;
		(*oBytesB)[ offset ] = p16b->b * 255 / 31;

		offset ++;
		p16b ++;
	}

}

//------------------------------------------------------------------------

R3D_FORCEINLINE float rnd()
{
	return float( rand() ) / RAND_MAX;
}

//------------------------------------------------------------------------

static float P( int l, int m, float x ) 
{ 
	// evaluate an Associated Legendre Polynomial P(l,m,x) at x 
	
	float pmm = 1.0f; 

	if( m > 0 )
	{ 
		float somx2 = sqrt((1.0f-x)*(1.0f+x)); 
		
		float fact = 1.0f; 
		for( int i = 1; i <= m; i ++ )
		{ 
			pmm *= (-fact) * somx2; 
			fact += 2.0f; 
		} 
	} 
	if( l == m ) 
		return pmm;

	float pmmp1 = x * ( 2.0f*m + 1.0f ) * pmm; 

	if( l == m + 1 ) return pmmp1; 

	float pll = 0.0f; 
	for(int ll=m+2; ll<=l; ++ll)
	{
		pll = ( (2.0f*ll-1.0f)*x*pmmp1-(ll+m-1.0f)*pmm ) / (ll-m); 
		pmm = pmmp1; 
		pmmp1 = pll; 
	} 

	return pll; 
}

static int fact_values[] = 
{
	1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800, 39916800, 479001600
};

static int factorial( int val )
{
	r3d_assert( val < R3D_ARRAYSIZE( fact_values ) );
	return fact_values[ val ];
}

static float K(int l, int m) 
{ 
	// renormalisation constant for SH function 
	float temp = ((2.0f*l+1.0f)*factorial(l-m)) / (4.0f*float(M_PI)*factorial(l+m)); 
	return sqrt(temp); 
}

float SH(int l, int m, float theta, float phi) 
{ 
	// return a point sample of a Spherical Harmonic basis function 
	// l is the band, range [0..N] 
	// m in the range [-l..l] 
	// theta in the range [0..Pi] 
	// phi in the range [0..2*Pi] 
	const float sqrt2 = sqrt(2.0f); 
	if(m==0) return K(l,0)*P(l,m,cos(theta)); 
	else if(m>0) return sqrt2*K(l,m)*cos(m*phi)*P(l,m,cos(theta)); 
	else return sqrt2*K(l,-m)*sin(-m*phi)*P(l,-m,cos(theta)); 
}

//------------------------------------------------------------------------

void SH_setup_spherical_samples() 
{ 
	if( g_SHSamples.Count() )
		return;

	g_SHSamples.Resize( N_SAMPLES );

	const int sqrt_n_samples = SQRT_N_SAMPLES;

	// fill an N*N*2 array with uniformly distributed 
	// samples across the sphere using jittered stratification 
	int i=0; // array index 
	float oneoverN = 1.0f/sqrt_n_samples; 
	for(int a=0; a<sqrt_n_samples; a++) { 
		for(int b=0; b<sqrt_n_samples; b++) { 
			// generate unbiased distribution of spherical coords 
			float x = (a + rnd()) * oneoverN; // do not reuse results 
			float y = (b + rnd()) * oneoverN; // each sample must be random 
			float theta = 2.0f * acos(sqrt(1.0f - x)); 
			float phi = 2.0f * float( M_PI ) * y; 
			g_SHSamples[i].sph = float3(theta,phi,1.0f); 
			// convert spherical coords to unit vector 
			float3 vec(sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi)); 
			g_SHSamples[i].vec = vec;

			SHSample& sample = g_SHSamples[ i ];
			sample.faceIdx = -1;

			float maxDot = -1.f;
			int maxIdx = -1;

			for( int d = 0, e = Probe::NUM_DIRS; d < e; d ++ )
			{
				D3DXVECTOR3 faceViewDir = Probe::ViewDirs[ d ];

				D3DXVECTOR3 d3dvec( vec.x, vec.y, vec.z );

				float dot = D3DXVec3Dot( &d3dvec, &faceViewDir );

				if( dot >= maxDot )
				{
					maxIdx = d;
					maxDot = dot;
				}
			}

			sample.faceIdx = maxIdx;

			// precompute all SH coefficients for this sample 
			for(int l=0; l<N_BANDS; ++l) { 
				for(int m=-l; m<=l; ++m) { 
					int index = l*(l+1)+m; 
					g_SHSamples[i].coeff[index] = SH(l,m,theta,phi); 
				} 
			} 
			++i; 
		} 
	}

	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		const r3dPoint3D& srcUp = Probe::UpVecs[ i ];
		const r3dPoint3D& srcDir = Probe::ViewDirs[ i ];

		D3DXVECTOR3 up ( srcUp.x, srcUp.y, srcUp.z );
		D3DXVECTOR3 view( srcDir.x, srcDir.y, srcDir.z );
		D3DXVECTOR3 side;

		D3DXVec3Cross( &side, &view, &up );

		D3DXMATRIX dir_xfm;

		dir_xfm._11 = side.x;	dir_xfm._12 = side.y;	dir_xfm._13 = side.z;	dir_xfm._14 = 0.f;
		dir_xfm._21 = view.x;	dir_xfm._22 = view.y;	dir_xfm._23 = view.z;	dir_xfm._24 = 0.f;
		dir_xfm._31 = up.x;		dir_xfm._32 = up.y;		dir_xfm._33 = up.z;		dir_xfm._34 = 0.f;
		dir_xfm._41 = 0.f;		dir_xfm._42 = 0.f;		dir_xfm._43 = 0.f;		dir_xfm._44 = 1.0f;

		Float2Arr& targArr = g_DirIntegrateSamples[ i ];
		targArr.Resize( N_SAMPLES );

		for( int i = 0, e = N_SAMPLES; i < e; i ++ )
		{
			float theta = 2.0f * Probe::DIR_FOV * ( float( rand() ) / RAND_MAX - 0.5f );
			float phi = float( 2.0 * M_PI ) * rand() / RAND_MAX;

			float sin_theta = sinf( theta );

			D3DXVECTOR3 dir( cosf( phi ) * sin_theta, cosf( theta ), sin( phi ) * sin_theta );

			D3DXVec3TransformNormal( &dir, &dir, &dir_xfm );

			D3DXVECTOR3 checkmevec( 0, 1, 0 );

			D3DXVec3TransformNormal( &checkmevec, &checkmevec, &dir_xfm );

			// now retrieve 'world space' theta and phy

			theta = acos( dir.y );

			sin_theta	= sqrtf( 1.0f - dir.y * dir.y );
			phi			= fabs( sin_theta ) > 0.0001 ? acos( R3D_MAX( R3D_MIN( dir.x / sin_theta, 1.f ), -1.0f ) ) : 0;

			if( dir.z < 0 )
			{
				phi = float( 2 * M_PI ) - phi;
			}

			targArr[ i ].x = theta;
			targArr[ i ].y = phi;

#ifdef _DEBUG
			D3DXVECTOR3 check_me( cosf( phi ) * sinf( theta ), cosf( theta ), sinf( phi ) * sinf( theta )  );

			float check_val = D3DXVec3Dot( &check_me, &view );
			float check_against = cosf( Probe::DIR_FOV );

			r3d_assert( check_val >= check_against - 0.001f );
#endif
		}
	}
}

void SH_free_spherical_samples() 
{
	SHSampleArr().Swap( g_SHSamples );

	for( int i = 0, e = Probe::NUM_DIRS; i < e; i ++ )
	{
		Float2Arr().Swap( g_DirIntegrateSamples[ i ] );
	}
}

void ProjectOnSH( float (&oSH)[N_COEFS], float (&fn)( float, float ) )
{
	for( int i = 0; i < N_COEFS; i ++ )
	{
		oSH[ i ] = 0;
	}

	const float weight = 4.0 * float( M_PI );
	// for each sample 
	for(int i=0; i < N_SAMPLES; ++i) 
	{ 
		float theta = g_SHSamples[i].sph.x;
		float phi = g_SHSamples[i].sph.y;

		float fnVal = fn( theta, phi );

		for( int n=0; n < N_COEFS; ++ n ) { 
			oSH[n] += fnVal * g_SHSamples[i].coeff[n]; 
		}
	}

	// divide the result by weight and number of samples 
	float factor = weight / N_SAMPLES; 
	for(int i = 0; i < N_COEFS; ++ i )
	{ 
		oSH[i] = oSH[i] * factor; 
	} 
}

static float Integrate( float (&fn)( float, float ), int dirId )
{
	float sum = 0;

	Float2Arr& samples = g_DirIntegrateSamples[ dirId ];
	
	// for each sample 
	for(int i=0; i < N_SAMPLES; ++i) 
	{
		const float2& sample = samples[ i ];
		sum += fn( sample.x, sample.y );
	}

	return sum / float ( N_SAMPLES );
}

static float SampleSpherical( float th, float ph )
{
	float sin_th = sin( th );
	float cos_th = cos( th );
	float sin_ph = sin( ph );
	float cos_ph = cos( ph );

	float tan_th = sin_th / cos_th;

	float3 vec( sin_th * cos_ph, cos_th, sin_th * sin_ph ); 

	float nx, ny, nz;

	int u, v;
	int face;

	const float CLOSE_TO_ZERO = 0.00001f;

	if( fabs( vec.z ) > CLOSE_TO_ZERO )
	{
		nx = vec.x / vec.z;
		ny = vec.y / vec.z;

		if( fabs( nx ) <= 1.0f )
		{
			// +Z or -Z plane
			if( fabs( ny ) <= 1.0f )
			{
				u = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nx * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );
				v = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( ny * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );

				if( vec.z >= 0 )
				{
					face = 4;
				}
				else
				{
					face = 5;
				}
			}
			else
			{
				// +Y or -Y plane
				nx = vec.x / vec.y;
				nz = vec.z / vec.y;

				r3d_assert( fabs( nx ) <= 1.0f && fabs( nz ) <= 1.0f );

				u = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nx * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );
				v = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nz * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );

				if( vec.y >= 0 )
				{
					face = 2;
				}
				else
				{
					face = 3;
				}
			}
		}
		else
		{
			ny = vec.y / vec.x;
			nz = vec.z / vec.x;

			if( fabs( ny ) <= 1.f )
			{
				r3d_assert( fabs( nz ) <= 1.f );

				u = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nz * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );
				v = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( ny * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );

				if( vec.x >= 0 )
				{
					face = 0;
				}
				else
				{
					face = 1;
				}
			}
			else
			{
				nx = vec.x / vec.y;
				nz = vec.z / vec.y;

				r3d_assert( fabs( nx ) <= 1.0f && fabs( nz ) <= 1.0f );

				u = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nx * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );
				v = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nz * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );

				if( vec.y >= 0 )
				{
					face = 2;
				}
				else
				{
					face = 3;
				}
			}
		}
	}
	else
	{
		// z is very close to zero, so we can discard Z + and Z - planes
		if( fabs( vec.x ) > CLOSE_TO_ZERO )
		{
			float ny = vec.y / vec.x;
			float nz = vec.z / vec.x;

			if( fabs( ny ) <= 1.0f )
			{
				// +X OR -X
				r3d_assert( fabs( nz ) <= 1.f );

				u = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nz * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );
				v = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( ny * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );

				if( vec.x >= 0 )
				{
					face = 0;
				}
				else
				{
					face = 1;
				}
			}
			else
			{
				nx = vec.x / vec.y;
				nz = vec.z / vec.y;

				r3d_assert( fabs( nx ) <= 1.0f && fabs( nz ) <= 1.0f );

				u = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nx * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );
				v = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nz * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );

				if( vec.y >= 0 )
				{
					face = 2;
				}
				else
				{
					face = 3;
				}
			}
		}
		else
		{
			// x is very close to 0 as well, so this is either +Y or -Y

			float nx = vec.x / vec.y;
			float nz = vec.z / vec.y;

			r3d_assert( fabs( nx ) <= 1.0f && fabs( nz ) <= 1.0f );

			u = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nx * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );
			v = R3D_MAX( R3D_MIN( int( ( PROBE_RT_DIM - 1 ) * ( nz * 0.5f + 0.5f ) ), PROBE_RT_DIM - 1 ), 0 );

			if( vec.y >= 0 )
				face = 2;
			else
				face = 3;
		}
	}

	if( face & 1 )
	{
		u = ( PROBE_RT_DIM - 1 ) - u;
	}
	else
	{
		v = ( PROBE_RT_DIM - 1 ) - v;
	}

	float val = (*g_SampleSphericalSource)[ v * PROBE_RT_DIM + u + face * PROBE_RT_DIM * PROBE_RT_DIM ] / 255.f;

	return val;
}

//------------------------------------------------------------------------

ProbeMaster* g_pProbeMaster;

//------------------------------------------------------------------------

UndoLightProbeCreateDelete::UndoLightProbeCreateDelete()
: m_IsDelete( false )
{
	
}

//------------------------------------------------------------------------

void UndoLightProbeCreateDelete::Release()
{
	PURE_DELETE( this ); 
}

//------------------------------------------------------------------------

UndoAction_e UndoLightProbeCreateDelete::GetActionID()
{
	return ms_eActionID;
}


//------------------------------------------------------------------------

void UndoLightProbeCreateDelete::Undo()
{
	if( m_IsDelete )
		AddProbes( true );
	else
		DeleteProbes();
}

//------------------------------------------------------------------------

void UndoLightProbeCreateDelete::Redo()
{
	if( m_IsDelete )
		DeleteProbes();
	else
		AddProbes( false );
}

//------------------------------------------------------------------------

const FixedString& UndoLightProbeCreateDelete::GetTitle()
{
	return m_IsDelete ? ms_DelName : ms_CreateName;
}

//------------------------------------------------------------------------

void UndoLightProbeCreateDelete::SetIsDelete	( bool isDeleete )
{
	m_IsDelete = isDeleete;
}

//------------------------------------------------------------------------

void UndoLightProbeCreateDelete::AddItem( ProbeIdx idx, const Probe& probe )
{
	ProbeUndoItem item;

	item.idx = idx;
	item.probe = probe;

	m_UndoArr.PushBack( item );
}

//------------------------------------------------------------------------
IUndoItem * UndoLightProbeCreateDelete::CreateUndoItem	()
{
	return new UndoLightProbeCreateDelete;
}


//------------------------------------------------------------------------
void UndoLightProbeCreateDelete::Register()
{
	UndoAction_t action;
	action.nActionID = ms_eActionID;
	action.pCreateUndoItem = CreateUndoItem;
	g_pUndoHistory->RegisterUndoAction( action );
}

//------------------------------------------------------------------------

void UndoLightProbeCreateDelete::AddProbes( bool select )
{
	if( select )
	{
		g_Manipulator3d.PickerResetPicked();
		g_pProbeMaster->DeleteProxies();
	}

	for( int i = 0, e = (int) m_UndoArr.Count(); i < e; i ++ )
	{
		ProbeUndoItem& ui = m_UndoArr[ i ];

		ui.idx.InTileIdx = g_pProbeMaster->AddProbe( ui.idx.TileX, ui.idx.TileZ, ui.probe );

		if( select )
		{
			g_pProbeMaster->SelectProbe( ui.idx, ui.probe );
		}
	}	
}

//------------------------------------------------------------------------

void UndoLightProbeCreateDelete::DeleteProbes()
{
	ProbeMaster::ProbeIdxArray parray;

	parray.Reserve( m_UndoArr.Count() );

	for( int i = 0, e = (int) m_UndoArr.Count(); i < e; i ++ )
	{
		const ProbeUndoItem& ui = m_UndoArr[ i ];

		parray.PushBack( ui.idx );
	}

	g_pProbeMaster->DeleteProbes( parray, true );
}

//------------------------------------------------------------------------

FixedString UndoLightProbeCreateDelete::ms_CreateName = "Add Light Probes";
FixedString UndoLightProbeCreateDelete::ms_DelName = "Del. Light Probes";

template<> UINT16 ConvertProbeRasterSamplesTo<UINT16>( const ProbeRasterSamples& samples, int idir )
{
	return	( samples.sumB[ idir ] )
				|
			( samples.sumG[ idir ] << 5 )
				|
			( samples.sumR[ idir ] << 11 );
}

template<> UINT32 ConvertProbeRasterSamplesTo<UINT32>( const ProbeRasterSamples& samples, int idir )
{
	return	( samples.sumR[ idir ] << 16 )
				|
			( samples.sumG[ idir ] << 8 )
				|
			( samples.sumB[ idir ] << 0 );
}


#endif