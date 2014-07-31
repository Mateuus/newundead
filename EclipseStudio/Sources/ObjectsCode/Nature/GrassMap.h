#pragma once

#define AR_GRAZ_PAZ "\\grass"

//////////////////////////////////////////////////////////////////////////

struct GrassCellEntry
{
	GrassCellEntry();

	int					TypeIdx;
};

typedef r3dTL::TArray< GrassCellEntry > GrassCellEntries;

struct GrassCell
{
	GrassCell();

	GrassCellEntries	Entries;

	r3dPoint3D			Position;

	float				YMax;
};

struct GrassMaskTextureEntry
{
	int					TypeIdx;
	r3dTexture*			MaskTexture;

	GrassMaskTextureEntry();
};

struct GrassTextureCell
{
	typedef r3dTL::TArray< GrassMaskTextureEntry > MaskTextureEntriesArr;

	GrassTextureCell();

	MaskTextureEntriesArr	MaskTextureEntries;
	r3dTexture*				HeightTexture;
};

class GrassMap
{
	friend void UpdateTextureCellHeightInMainThread( void* );
	friend void CreateMaskTextureCellEntryInMainThread( void* param );
	// types
public:

	enum Path
	{
		DEPTH_PATH,
		COLOR_PATH,
		COMBINED_PATH
	};

	typedef r3dTL::T2DArray< GrassCell >		GrassCells;
	typedef r3dTL::T2DArray< GrassTextureCell >	GrassTextureCells;
	typedef r3dTL::TArray< unsigned char >		Bytes;

	static const int CELL_HEIGHT_TEX_DIM	= 64;
	static const int CELL_MASK_TEX_DIM		= 64;

	static const int MAX_TEX_CELL_COUNT		= 32;

	struct Settings
	{
		float XLength;
		float ZLength;

		float XOffset;
		float ZOffset;

		float YMin;
		float YMax;

		Settings();
	};

	// construction/ destruction
public:
	GrassMap();
	~GrassMap();

	// manipulation/ accesss
public:
	void Init( float xoffset, float zoffset, float xsize, float zsize, int maxTexCellCount );
	void Close();

	bool Save( const r3dString& levelHomeDir );
	bool Load( const r3dString& levelHomeDir );

	bool HasGrassCells() const;

	// remove masks were all values are approximately 1
	void OptimizeMasks();

	void Paint( float x, float z, float radius, float dir, const r3dString& grassType );
	void UpdateHeight( float x, float z, float radius );
	void UpdateHeight();

	void ResetBounds();

	void Prepare() ;
	void Draw( const r3dCamera& cam, Path path, bool useDepthEqual, bool drawAux );

	void ConformWithNewCellSize();

	void ClearCells();
	void ClearGrassType( const r3dString& grassType ) ;

	r3dString GetUsedTypes() const ;

	void SetDebugType( const r3dString& typeName ) ;

	static float GetVisRad();
	void GetGridWorldSize(float &x, float &z) const { x = mSettings.XLength; z = mSettings.ZLength; }
	float GetCellSize();
	float GetTextureCellSize();

	const Settings& GetSettings() const;

	// helpers
private:
	void SetupCellArrays( int nominzalCellsX, int nominzalCellsZ, int maxTexCellCount );
	void GetNominalCellCount( float fWidth, float fHeight, int *oCellsX, int* oCellsZ );
	void GetTextureCellsBasedOnNormalCells( int nominzalCellsX, int nominzalCellsZ, int maxTexCellCount, int * cellsInTexCell, int * oCountX, int * oCountZ );
	void DeriveTextureCellCoords( float X, float Z, int& oCellX, int& oCellZ, float& oCellDX, float& oCellDZ );
	void TexCellToWorldCoords( int CellX, int CellZ, float CellDX, float CellDZ, float& oX, float& oZ );
	void CreateTextureCell( int X, int Z );
	void CreateCells( const Bytes& CompoundTex, uint32_t TypeIdx );
	void CreateMaskTextureCellEntry( GrassTextureCell& cell, int X, int Z, uint32_t TypeIdx, bool createTexture );
	void DoCreateMaskTextureCellEntry( GrassTextureCell& cell, int X, int Z, uint32_t TypeIdx, bool createTexture );
	void CreateCellEntry( GrassCell& cell, uint32_t TypeIdx );

	void DoPaint( float CentreDX, float CentreDZ, float Radius, float Dir, r3dTexture* Tex );
	void UpdateTextureCellHeight( int X, int Z, GrassTextureCell& cell );

	void DoUpdateTextureCellHeight( int X, int Z, GrassTextureCell& cell );
	template <typename T>
	void DoUpdateTextureCellHeightT( int X, int Z, GrassTextureCell& cell );
	void DrawCell( const GrassTextureCell& gtc, int TexCellX, int TexCellZ, const GrassCell& cell, const r3dCamera& Cam, int X, int Z, float SSAO );

	void LoadSettings( const r3dString& HomeDir );
	void SaveSettings( const r3dString& HomeDir );

	bool SaveCellData( const r3dString& HomeDir );
	bool LoadCellData( const r3dString& HomeDir );

	bool LoadCellData_101( r3dFile* fin );
	bool LoadCellData_102( r3dFile* fin );
	bool LoadCellData_103( r3dFile* fin );
	bool LoadCellData_104_105( r3dFile* fin, bool is105 );

	void CreateTextureCellHeightTexture( GrassTextureCell& cell );
	void DeleteMaskEntriesInUnderlyingCells( int typeIdx, int textureCellX, int textureCellZ );

	void UpdateYExtents();

	// data
private:
	GrassCells			mCells;
	GrassTextureCells	mTextureCells;

	int				mCellsPerTextureCell;

	float			mCellSize;

	bool			mInitialized;

	int				mLoadVersion;

	int				mDebugGrassType;

	Settings		mSettings;

};
extern GrassMap* g_pGrassMap;

r3dString GetGrassMapTexPath( const r3dString& LevelPath );
r3dString GetGrassMapHeightTexName( int CellX, int CellZ );
r3dString GetGrassMapMaskTexName( const r3dString& Type, int CellX, int CellZ );

void InitGrass();
void LoadGrassLib();
void UnloadGrassLib();
void AnimateGrass();
void PrepareGrass() ;
void DrawGrass( GrassMap::Path path, bool UseDepthEqual, bool drawToAux );
void CloseGrass();
float GetGrassHeight(const r3dPoint3D &pt);

