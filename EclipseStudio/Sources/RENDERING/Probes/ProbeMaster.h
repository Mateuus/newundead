#pragma once

#include "r3dTypeTuples.h"
#include "..\..\..\GameEngine\gameobjects\GameObj.h"

#ifdef FINAL_BUILD
#define R3D_ALLOW_LIGHT_PROBES 0
#else
#define R3D_ALLOW_LIGHT_PROBES 1
#endif

#if R3D_ALLOW_LIGHT_PROBES

#pragma pack(push)
#pragma pack(1)
union ProbeIdx
{
	struct
	{
		UINT32 TileX : 8;
		UINT32 TileZ : 8;
		UINT32 InTileIdx : 16;
	};

	UINT32 CombinedIdx;
};

#pragma pack(pop)

struct Probe
{
	enum
	{
		FLAG_SKY_DIRTY = 1,
		FLAG_BOUNCE_DIRTY = 2,
		FLAG_LOCAL_BOUNCE_DIRTY = 4
	};

	enum
	{
		NUM_DIRS = 4,
		NUM_LOCAL_VPL = 32,
	};

	typedef r3dTL::TFixedArray< r3dPoint3D, NUM_DIRS > FixedDirArr;
	typedef r3dTL::TFixedArray< D3DXVECTOR4, NUM_DIRS > FixedFloat4Arr;
	typedef r3dTL::TFixedArray< UINT16, NUM_DIRS > FixedUINT16Arr;
	typedef r3dTL::TFixedArray< UINT32, NUM_DIRS > FixedUINT32Arr;
	typedef r3dTL::TFixedArray< float, NUM_DIRS > FixedFloatArr;

	typedef r3dTL::TFixedArray< D3DXVECTOR4, NUM_LOCAL_VPL > FixedLocalVPLSHArr;
	typedef r3dTL::TFixedArray< ProbeIdx, NUM_LOCAL_VPL > FixedLocalProbeIdxArr;

	// for for any given direction in radians
	static const float DIR_FOV;
	static FixedDirArr ViewDirs;
	static FixedDirArr UpVecs;

	FixedFloat4Arr SH_BounceR;
	FixedFloat4Arr SH_BounceG;
	FixedFloat4Arr SH_BounceB;

	FixedLocalVPLSHArr 		SH_LocalVPLsR;
	FixedLocalVPLSHArr 		SH_LocalVPLsG;
	FixedLocalVPLSHArr 		SH_LocalVPLsB;
	FixedLocalProbeIdxArr	SH_LocalVPLProbeIndexes;

	D3DXVECTOR3				DynamicLightsRGB;

	FixedFloatArr SkyVisibility;

	r3dPoint3D Position;

	int CellX;
	int CellY;
	int CellZ;

	FixedUINT32Arr BasisColors32;
	FixedUINT32Arr DynamicColors32;
	FixedUINT32Arr CompositeColors32;

	UINT8 Flags;

	int XRadius;
	int YRadius;
	int ZRadius;

	Probe();
};

int CountProbeLocalVPLs( const Probe* probe );

class ProbeProxy: public GameObject
{
	DECLARE_CLASS(ProbeProxy, GameObject)
public:
	ProbeProxy();

	virtual BOOL		OnPositionChanged();
	virtual	void 		SetPosition(const r3dPoint3D& pos);

	virtual BOOL		GetObjStat ( char * sStr, int iLen ) OVERRIDE;

	void SetIdx( ProbeIdx idx );

	static r3dBoundBox CreateBB();

	virtual void		AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam ) OVERRIDE;

	void DoDrawComposite( const r3dCamera& cam );

	ProbeIdx Idx;
};

struct ProbeRasterSamples
{
	typedef r3dTL::TFixedArray< UINT16, Probe::NUM_DIRS > FixedUINT16Arr;

	FixedUINT16Arr sumR;
	FixedUINT16Arr sumG;
	FixedUINT16Arr sumB;

	int count;
};


class ProbeMaster
{
	friend class UndoLightProbeCreateDelete;

public:

	typedef r3dTL::TArray< UINT8 > Bytes;
	typedef r3dTL::TArray< Probe > Probes;

	typedef r3dTL::TArray< ProbeIdx > ProbeIdxArray;

	enum
	{
		PROBE_DIRTY_ARR_CELL_SIZE = 8
	};

	struct ProbeTile
	{
		r3dBox3D			BBox;
		Probes				TheProbes;

		ProbeTile();
	};

	typedef r3dTL::T2DArray< ProbeTile > ProbeMap;
	typedef r3dTL::TArray< const ProbeTile* > ConstProbeTileArray;
	typedef r3dTL::TFixedArray< D3DXVECTOR4, Probe::NUM_DIRS > BasisArray;
	typedef r3dTL::TFixedArray< r3dVertexBuffer*, Probe::NUM_DIRS > SHProjectVertexBuffers;
	typedef r3dTL::TArray< ProbeRasterSamples > ProbeRasterSamplesVolume;
	typedef r3dTL::T2DArray< int > ProbeVolumeTileDirtyArr;

	enum ProbeVisualizationMode
	{
		VISUALIZE_SKY_VISIBILITY,
		VISUALIZE_SKY_SH,
		VISUALIZE_SUN_SH,
		VISUALIZE_SKY_SH_MUL_VISIBILITY,
		VISUALIZE_BOUNCE_DIR0,
		VISUALIZE_BOUNCE_DIR1,
		VISUALIZE_BOUNCE_DIR2,
		VISUALIZE_BOUNCE_DIR3,
		VISUALIZE_BOUNCE_LAST = VISUALIZE_BOUNCE_DIR3,
		VISUALIZE_PROBE_STATIC_COLORS,
		VISUALIZE_PROBE_DYNAMIC_COLORS,
		VISUALIZE_PROBE_COMPOSITE_COLORS,
		VISUALIZE_COUNT
	};

	struct Settings
	{
		int ProbeTextureWidth;
		int ProbeTextureHeight;
		int ProbeTextureDepth;

		float ProbeVolumeOffsetY;

		float ProbeTextureSpanX;
		float ProbeTextureSpanY;
		float ProbeTextureSpanZ;

		D3DFORMAT ProbeTextureFmt;

		float ProbePopulationStepX;
		float ProbePopulationStepY;
		float ProbePopulationStepZ;		

		float ProbeElevation;

		int MaxVerticalProbes;

		int NominalProbeTileCountX;
		int NominalProbeTileCountZ;

		r3dColor DefaultBounceColor_Up;
		r3dColor DefaultBounceColor_Down;

		int MaximumProbeRadius;
		int MaximumProbeYRadius;

		int ProbeRadiusExpansion;

		Settings();
	};

	struct Info
	{
		float ProbeMapWorldActualXSize;
		float ProbeMapWorldActualYSize;
		float ProbeMapWorldActualZSize;

		float ProbeMapWorldNominalXSize;
		float ProbeMapWorldNominalYSize;
		float ProbeMapWorldNominalZSize;

		float ProbeMapWorldXStart;
		float ProbeMapWorldYStart;
		float ProbeMapWorldZStart;

		int TotalProbeCellsCountX;
		int TotalProbeCellsCountY;
		int TotalProbeCellsCountZ;

		int ProbeCellsInTileX;
		int ProbeCellsInTileZ;

		float CellSizeX;
		float CellSizeY;
		float CellSizeZ;

		int ActualProbeTileCountX;
		int ActualProbeTileCountZ;

		int CamCellX;
		int CamCellY;
		int CamCellZ;

		int ProbeVolumeCamCellX;
		int ProbeVolumeCamCellY;
		int ProbeVolumeCamCellZ;

		Info();
	};

	struct MemStats
	{
		MemStats();

		int ProbeSize;
		int ProbeVolumeSize;
	};

	struct SkyDirectColors
	{
		r3dTL::TFixedArray< D3DXVECTOR4, Probe::NUM_DIRS > DirColor;
	};

	typedef r3dTL::TFixedArray< r3dTexture*, Probe::NUM_DIRS > DirectionTextures;
	typedef r3dTL::TArray< ProbeProxy* > ProbeProxies;

public:
	ProbeMaster();
	~ProbeMaster();

public:
	void Init();
	void InitEditor();
	void Close();

	// for given level
	bool IsCreated() const;

	void Save( const char* levelDir );
	int Load( const char* levelDir );

	void SaveXMLSettings( const char* levelDir );
	void LoadXMLSettings( const char* levelDir );

	void UpdateDefaultProbe();
	int UpdateDynamicLights();

	void Tick();

public:
	void Test();

	void Create( float startX, float startY, float startZ, float sizeX, float sizeY, float sizeZ );
	void Reset();

	void UpdateProbeRadiuses();

	void UpdateSkyVisibility( int DirtyOnly );
	void UpdateBounce( int DirtyOnly );

	void ShowProbes( ProbeVisualizationMode mode );
	void ShowProbeVolumesScheme();

	r3dScreenBuffer* GetBounceRT() const;

	void UpdateProbeVolumes();

	void UpdateSkyAndSun();

	void RelightProbes();

	void RelightDynamicProbes();

	r3dTexture* GetProbeTexture( int direction ) const;

	const Settings& GetSettings() const;
	const Info&		GetInfo() const;
	MemStats		GetMemStats() const;
	void			SetSettings( const Settings& settings );

	void ClearProbes();
	void PopulateProbes( int tileX, int tileZ );

	int GetProbeCount( int tileX, int tileZ );
	int GetProbeCount() const;
	int GetDirtyProbeCount( int flag );

	void SwitchVolumeFormat();

	r3dPoint3D GetProbeTexturePosition();
	r3dPoint3D GetProbeTextureWrapPoint();

	void UpdateCamera( const r3dPoint3D& cam );
	int UpdateCameraCell( const r3dPoint3D& cam );

	int3 GetCameraProbeCell() const;

	const SkyDirectColors& GetSkyDirectColors() const;

	const Probe& GetDefaultProbe();

	void SelectProbes( float x0, float y0, float x1, float y1 );
	void DeleteProxies();
	void DeleteProxyFor( ProbeIdx idx );

	Probe* GetProbe( ProbeIdx idx );
	ProbeIdx MoveProbe( ProbeIdx idx, const r3dPoint3D& newPos );
	Probe* GetClosestProbe( const r3dPoint3D& pos );

	ProbeIdx AddProbe( const r3dPoint3D& pos, bool markTileDirty, const Probe* savedProbe );
	ProbeIdx AddProbe( const Probe& probe, bool markTileDirty );

	bool IsOccupied( const r3dPoint3D& pos );

	void DeleteAllProbes();

	void DeleteProbesInCylinder( const r3dPoint3D& base, float radius, float height );
	void DeleteProbes( const r3dTL::TArray< GameObject* > & objects );
	void DeleteProbes( const ProbeIdxArray& probeIdxArray, bool deleteProxies );
	void DeleteProbe( ProbeIdx idx, bool deleteProxy );

	void AdjustProbeRadiuses();
	void AdjustProbeRadiuses( int tileX, int tileZ );

	const r3dBoundBox* GetTileBBox( int tileX, int tileZ );

	void UpdateDirtyTilesRadiuses();

	int GetVisibleProbesNum();

	int DistributeLightToProbes( r3dLight* light );

	const ProbeProxies& GetSelectedProbes() const;

	void DEBUG_SetEndProbe( int endProbe );

	void ConvertCellToCoord( int cellX, int cellY, int cellZ, r3dPoint3D* oPos );
	void ConvertCoordToCell( int * oCellX, int * oCellY, int * oCellZ, const r3dPoint3D& pos );

	const ProbeVolumeTileDirtyArr& GetAnimatedDirtiness() const;

private:
	void RelightProbe( Probe* probe );
	void RelightDynamicProbe( Probe* probe );

	int GatherClosestProbesForLightDistribution( const r3dPoint3D& lightPos, struct DistanceProbeEntry* probeEntries, int maxCount );

	void StartUpdatingBounce();
	void UpdateProbeBounce( Probe* probe );
	void StopUpdatingBounce();

	void StartUpdatingLocalVPLBounce();
	void GatherClosestProbesForVPL( Probe* target, ProbeIdx (&probes)[ Probe::NUM_LOCAL_VPL ] );
	void UpdateLocalVPLBounce( Probe* probe );
	void StopUpdatingLocalVPLBounce();

	void StartUpdatingSkyVisibility();
	void UpdateProbeSkyVisibility( Probe* probe );
	void StopUpdatingSkyVisibility();

	void StartProbesVisualization( ProbeVisualizationMode mode );
	void VisualizeProbe( const Probe* probe );
	void StopProbesVisualization();

	void StartProbeDirections();
	void DrawProbeDirection( const Probe* probe, int dirIdx );
	void DrawProbeDirections( const Probe* probe );
	void StopProbeDirections();

	void RasterizeProbeIntoVolume( Probe* probe, const D3DBOX& box, int lockedStartX, int lockedStartY, int lockedStartZ, int lockedSizeX, int lockedSizeY, int lockedSizeZ );

	void FillNormalToSHTex();

	void CreateTempRTAndSysMemTex();
	void DestroyTempRTAndSysMemTex();

	void ClearProbeMap();
	void ClearProbeMapTile( int tileX, int tileZ );
	void UpdateBBox( int x, int z, const r3dPoint3D& pos );
	int AddProbe( int x, int z, const Probe& probe );

	void ResetProbeMapBBoxes();
	void ResetProbeMapBBox( int tileX, int tileZ );

	void RecreateVolumeTexes();

	void FindClosestProbe( int * oIdx, float* oDist, int tileX, int tileZ, Probe* exclude );

	void GetProbeTileIndexes( int * oX, int* oZ, float x, float z ) const;

	Probe* GetClosestProbe( int cellx, int celly, int cellz );

	Probe* GetClosestProbe( int tileX, int tileZ, const r3dPoint3D& pos );

	int OutputBakeProgress( const char* operationName, int total, int complete );

	void SelectProbe( ProbeIdx idx, const Probe& p );

	void RecalcTileBBox( int tileX, int tileZ );

	void DrawProbeVolumesFrame();

	void UpdateCellsAndSizeParams();

	void ConvertToTileCoords( float x, float z, int* oTileX, int* oTileZ );

	void FetchAllProbes( Probes* oProbes );

	void AddDirtyRadiusTile( ProbeIdx pidx );

	void SetupDefaultLightProbe();

	void AdjustProbeRadius( Probe* probe, int tileX, int tileZ );

	R3D_FORCEINLINE void GetCellCoords( int * oCellX, int * oCellY, int * oCellZ, const r3dPoint3D& pos );

	R3D_FORCEINLINE ProbeIdx GetClosestProbeIdx( int cellX, int cellY, int cellZ );
	R3D_FORCEINLINE void FillProbeVolumeCell( const D3DLOCKED_BOX& lbox, const D3DBOX& box, int cx, int cy, int cz, const Probe& blackProbe, int texIdx );
	
	R3D_FORCEINLINE void FillProbeVolumeCellWithProbe( const D3DLOCKED_BOX (&lboxes)[ Probe::NUM_DIRS ], const D3DBOX& box, int cx, int cy, int cz, const Probe& probe );
	
	R3D_FORCEINLINE void RasterizeXSlice( int startTexX, int endTexX, int cellX, int cellY, int cellZ, const Probe& defaultProbe );
	R3D_FORCEINLINE void RasterizeYSlice( int startTexY, int endTexY, int cellX, int cellY, int cellZ, const Probe& defaultProbe );
	R3D_FORCEINLINE void RasterizeZSlice( int startTexZ, int endTexZ, int cellX, int cellY, int cellZ, const Probe& defaultProbe );

	R3D_FORCEINLINE void RasterizeDirtyRect( int startTexX, int endTexX, int startTexZ, int endTexZ, int cellX, int cellY, int cellZ, const Probe& defaultProbe );

	R3D_FORCEINLINE void AddRasterSample( int idx, const Probe* probe );
	R3D_FORCEINLINE void ResetRasterSamples();
	R3D_FORCEINLINE void AvarageRasterSamples();

	template <typename T>
	R3D_FORCEINLINE void CopySamplesIntoLockedTextures( T* locked[ Probe::NUM_DIRS ], int pitchX, int pitchXZ, int lockedSizeX, int lockedSizeY, int lockedSizeZ );

private:
	Bytes					m_RTBytesR;
	Bytes					m_RTBytesG;
	Bytes					m_RTBytesB;

	r3dScreenBuffer*		m_TempRT;
	IDirect3DTexture9*		m_SysmemTex;

	r3dScreenBuffer*		m_BounceDiffuseRT;
	r3dScreenBuffer*		m_BounceNormalRT;
	r3dScreenBuffer*		m_BounceDepthRT;
	r3dScreenBuffer*		m_BounceAccumSHRRT;
	r3dScreenBuffer*		m_BounceAccumSHGRT;
	r3dScreenBuffer*		m_BounceAccumSHBRT;

	IDirect3DVertexDeclaration9*	m_SHProjectVertexDecl;
	SHProjectVertexBuffers			m_SHProjectVertexBuffers;

	r3dTexture*				m_NormalToSHTex;

	IDirect3DTexture9*		m_BounceSysmemRT_R;
	IDirect3DTexture9*		m_BounceSysmemRT_G;
	IDirect3DTexture9*		m_BounceSysmemRT_B;

	r3dCamera				m_SavedCam;
	int						m_SavedUseOQ;

	ProbeVisualizationMode	m_VisMode;

	ProbeMap				m_ProbeMap;

	int						m_EditorMode;

	Settings				m_Settings;
	Info					m_Info;

	DirectionTextures		m_DirectionVolumeTextures;

	D3DXVECTOR4				m_SkyDomeSH_R;
	D3DXVECTOR4				m_SkyDomeSH_G;
	D3DXVECTOR4				m_SkyDomeSH_B;

	D3DXVECTOR4				m_SunSH_R;
	D3DXVECTOR4				m_SunSH_G;
	D3DXVECTOR4				m_SunSH_B;

	BasisArray				m_BasisSHArray;

	float					m_SkyVisA;
	float					m_SkyVisB;

	float					m_LastInfoFrame;

	ConstProbeTileArray		m_VisibleProbeTileArray;

	int						m_PrevCamCellX;
	int						m_PrevCamCellY;
	int						m_PrevCamCellZ;

	int						m_ProbeVolumeDirty;

	SkyDirectColors			m_SkyDirColors;

	ProbeProxies			m_ProbeProxies;

	int						m_Created;
	int						m_ProbeRadiusesDirty;

	int						m_DEBUG_EndProbe;

	int						m_RasterizedProbeCount;

	ProbeVolumeTileDirtyArr	m_ProbeVolumeDirtyArr;
	// for debug visualization
	ProbeVolumeTileDirtyArr m_ProbeVolumeDirtyAnimArr;

	ProbeIdxArray			m_DirtyRadiusesTiles;

	Probe					m_DefaultProbe;

	ProbeRasterSamplesVolume m_ProbeRasterSamplesVolume;

} extern * g_pProbeMaster;

//------------------------------------------------------------------------

struct ProbeUndoItem
{
	ProbeIdx idx;
	Probe probe;
};

class UndoLightProbeCreateDelete : public IUndoItem
{
public:
	typedef r3dTL::TArray< ProbeUndoItem > ProbeUndoArray;
	static const UndoAction_e ms_eActionID = UA_LIGHTPROBES_CREATE_DELETE;

public:
	UndoLightProbeCreateDelete();

	virtual void				Release			() OVERRIDE;
	virtual UndoAction_e		GetActionID		() OVERRIDE;

	virtual void				Undo			() OVERRIDE;
	virtual void				Redo			() OVERRIDE;

	virtual const FixedString&	GetTitle		() OVERRIDE;

	void				SetIsDelete		( bool isDeleete );
	void				AddItem			( ProbeIdx idx, const Probe& probe );

	static IUndoItem * CreateUndoItem	();
	static void Register();

private:
	void AddProbes( bool select );
	void DeleteProbes();

private:
	ProbeUndoArray	m_UndoArr;
	bool			m_IsDelete;

	static FixedString ms_CreateName;
	static FixedString ms_DelName;
};


#endif