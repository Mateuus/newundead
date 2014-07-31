
#include "multiplayer/P2PMessages.h"
#include "multiplayer/NetCellMover.h"
#include "UI\UIimEdit.h"
#include "AnimalsStates.h"

class obj_ParticleSystem ;
//class r3dPhysSkeleton;
class r3dSkeleton;
class obj_Animals : public MeshGameObject
{
	DECLARE_CLASS(obj_Animals, MeshGameObject)

public:

	int				m_bAnimated;
	int				m_bGlobalAnimFolder;
	char			m_sAnimName[256];
	r3dSkeleton*	m_BindSkeleton; // //@TODO: make it shared for every same object name
	//r3dSkeleton*	m_BindSkeleton2; // //@TODO: make it shared for every same object name
	int				m_IsSkeletonShared ;
	r3dAnimPool		m_AnimPool;
	r3dAnimation	m_Animation;
	bool bDead;
	CNetCellMover	netMover;
	int animalstate;
	int physid;
	//r3dPhysSkeleton* pxSkeleton;

	obj_ParticleSystem*	m_DestructionParticles ;
	int							m_DestructionSoundID ;

	bool			NeedDrawAnimated(const r3dCamera& Cam);
	void			DrawAnimated(const r3dCamera& Cam, bool shadow_pass);
	void			ChangeAnim(DWORD Flags);

	void			DestroyBuilding();
	void			FixBuilding();
	virtual	BOOL		OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
	void OnNetPacket(const PKT_C2C_MoveRel_s& n);
	void OnNetPacket(const PKT_C2C_MoveSetCell_s& n);
	void OnNetPacket(const PKT_S2C_MoveTeleport_s& n);
	void OnNetPacket(const PKT_S2C_AnimalsMove_s& n);
public:
	obj_Animals();
	virtual	~obj_Animals();

	static void	LoadSkeleton( const char* baseMeshFName, r3dSkeleton** oSkeleton, int * oIsSkeletonShared , const char* len) ;

	virtual	BOOL		OnCreate();
	virtual	BOOL		Load(const char* fname);
	virtual	BOOL		Update();

	virtual GameObject *Clone ();

	virtual BOOL		GetObjStat ( char * sStr, int iLen );

	virtual	void 		SetPosition(const r3dPoint3D& pos);

#ifndef FINAL_BUILD
	virtual	float		DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected ) OVERRIDE;
	virtual void		DrawSelected( const r3dCamera& Cam, eRenderStageID DrawState );
#endif

	virtual void		AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam ) OVERRIDE;
	virtual void		AppendShadowRenderables( RenderArray& rarr, const r3dCamera& Cam ) OVERRIDE;

	virtual	void		ReadSerializedData(pugi::xml_node& node);
	virtual void		WriteSerializedData(pugi::xml_node& node);

	virtual void		UpdateDestructionData();
	virtual int			IsStatic() OVERRIDE;

protected:
	void PostCloneParamsCopy(obj_Animals *pNew);
};