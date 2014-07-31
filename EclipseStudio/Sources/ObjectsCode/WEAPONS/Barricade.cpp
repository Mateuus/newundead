#include "r3dPCH.h"
#include "r3d.h"

#include "Barricade.h"
#include "WeaponConfig.h"
#include "WeaponArmory.h"
#include "..\ai\AI_Player.H"
#include "..\..\multiplayer\ClientGameLogic.h"


IMPLEMENT_CLASS(obj_Barricade, "obj_Barricade", "Object");
AUTOREGISTER_CLASS(obj_Barricade);

obj_Barricade::obj_Barricade()
{
	m_PrivateModel = NULL;
	m_ItemID = -1;
	m_RotX = 0;
	ObjTypeFlags = OBJTYPE_GameplayItem;
}

obj_Barricade::~obj_Barricade()
{

}

BOOL obj_Barricade::OnCreate()
{
	r3d_assert(m_ItemID > 0);
	if(m_ItemID == WeaponConfig::ITEMID_BarbWireBarricade)
		m_PrivateModel = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Barricade_BarbWire_Built.sco", true, false, true, true );
	else if(m_ItemID == WeaponConfig::ITEMID_WoodShieldBarricade)
		m_PrivateModel = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Barricade_WoodShield_Built.sco", true, false, true, true );
	else if(m_ItemID == WeaponConfig::ITEMID_RiotShieldBarricade)
		m_PrivateModel = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Riot_Shield_01.sco", true, false, true, true );
	else if(m_ItemID == WeaponConfig::ITEMID_SandbagBarricade)
		m_PrivateModel = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\item_barricade_Sandbag_built.sco", true, false, true, true );
	else if(m_ItemID == WeaponConfig::ITEMID_WoodenDoor2M)
		m_PrivateModel = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Block_Door_Wood_2M_01.sco", true, false, true, true );
		else if(m_ItemID == WeaponConfig::ITEMID_PersonalLocker)
		m_PrivateModel = r3dGOBAddMesh("Data\\ObjectsDepot\\Weapons\\Item_Lockbox_01_Crate.sco", true, false, true, true );

	if(m_PrivateModel==NULL)
		return FALSE;

	ReadPhysicsConfig();

	//PhysicsConfig.group = PHYSCOLL_NETWORKPLAYER;
	//PhysicsConfig.isDynamic = 0;

	SetBBoxLocal( GetObjectMesh()->localBBox ) ;

	// raycast and see where the ground is and place dropped box there
	/*PxRaycastHit hit;
	PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
	if(g_pPhysicsWorld->raycastSingle(PxVec3(GetPosition().x, GetPosition().y+1, GetPosition().z), PxVec3(0, -1, 0), 50.0f, PxSceneQueryFlag::eIMPACT, hit, filter))
	{
		SetPosition(r3dPoint3D(hit.impact.x, hit.impact.y, hit.impact.z));
		SetRotationVector(r3dPoint3D(m_RotX, 0, 0));
	}*/

	UpdateTransform();

	return parent::OnCreate();
}

BOOL obj_Barricade::OnDestroy()
{
	m_PrivateModel = NULL;
	return parent::OnDestroy();
}

BOOL obj_Barricade::Update()
{
	return parent::Update();
}


struct BarricadeDeferredRenderable : MeshDeferredRenderable
{
	void Init()
	{
		DrawFunc = Draw;
	}

	static void Draw( Renderable* RThis, const r3dCamera& Cam )
	{
		BarricadeDeferredRenderable* This = static_cast< BarricadeDeferredRenderable* >( RThis );
		r3dApplyPreparedMeshVSConsts(This->Parent->preparedVSConsts);
		if(This->DrawState != rsCreateSM)
			This->Parent->m_PrivateModel->DrawMeshDeferred(r3dColor::white, 0);
		else
			This->Parent->m_PrivateModel->DrawMeshShadows();
	}

	obj_Barricade* Parent;
	eRenderStageID DrawState;
};

void obj_Barricade::AppendShadowRenderables( RenderArray & rarr, const r3dCamera& Cam )
{
	uint32_t prevCount = rarr.Count();
	m_PrivateModel->AppendShadowRenderables( rarr );
	for( uint32_t i = prevCount, e = rarr.Count(); i < e; i ++ )
	{
		BarricadeDeferredRenderable& rend = static_cast<BarricadeDeferredRenderable&>( rarr[ i ] );
		rend.Init();
		rend.Parent = this;
		rend.DrawState = rsCreateSM;
	}
}

void obj_Barricade::AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam )
{
	uint32_t prevCount = render_arrays[ rsFillGBuffer ].Count();
	m_PrivateModel->AppendRenderablesDeferred( render_arrays[ rsFillGBuffer ], r3dColor::white );
	for( uint32_t i = prevCount, e = render_arrays[ rsFillGBuffer ].Count(); i < e; i ++ )
	{
		BarricadeDeferredRenderable& rend = static_cast<BarricadeDeferredRenderable&>( render_arrays[ rsFillGBuffer ][ i ] );
		rend.Init();
		rend.Parent = this;
		rend.DrawState = rsFillGBuffer;
	}
}

/*virtual*/
r3dMesh* obj_Barricade::GetObjectMesh() 
{
	return m_PrivateModel;
}

/*virtual*/
r3dMesh* obj_Barricade::GetObjectLodMesh()
{
	return m_PrivateModel;
}