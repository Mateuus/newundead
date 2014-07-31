#include "r3dPCH.h"
#include "r3d.h"

#include "PostFXChief.h"

#include "RenderDeffered.h"

#include "PFX_ComposeMultibloom.h"

//------------------------------------------------------------------------
PFX_ComposeMultibloom::Settings::Settings()
{
	for (int i = 0; i < bloomParts.COUNT; ++i)
	{
		bloomParts[i] = PostFXChief::RTT_COUNT;
	}
}

//------------------------------------------------------------------------
PFX_ComposeMultibloom::PFX_ComposeMultibloom(int numImages)
: Parent( this )
, mColorWriteMask(D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED)
, mNumImages(numImages)
{

}

//------------------------------------------------------------------------
PFX_ComposeMultibloom::~PFX_ComposeMultibloom()
{

}

//------------------------------------------------------------------------
/*virtual*/
void
PFX_ComposeMultibloom::InitImpl() /*OVERRIDE*/
{
	char psName[] = "PS_COMPOSE_MULTIBLOOM1";
	psName[strlen(psName) - 1] += mNumImages;
	mData.PixelShaderID = r3dRenderer->GetPixelShaderIdx( psName );
}

//------------------------------------------------------------------------
/*virtual*/
void
PFX_ComposeMultibloom::CloseImpl() /*OVERRIDE*/
{

}

//------------------------------------------------------------------------
/*virtual*/
void
PFX_ComposeMultibloom::PrepareImpl( r3dScreenBuffer* dest, r3dScreenBuffer* src )	/*OVERRIDE*/
{
	const Settings& sts = mSettingsArr[ 0 ];

	r3dRenderer->SetRenderingMode( R3D_BLEND_ADD | R3D_BLEND_PUSH );

	if( mColorWriteMask != PostFXChief::DEFAULT_COLOR_WRITE_MASK )
	{		
		D3D_V( r3dRenderer->pd3ddev->SetRenderState( D3DRS_COLORWRITEENABLE, mColorWriteMask ) );
	}

	r3dSetFiltering( R3D_BILINEAR, 0 );
	r3dSetFiltering( R3D_BILINEAR, 1 );
	r3dSetFiltering( R3D_BILINEAR, 2 );
	r3dSetFiltering( R3D_BILINEAR, 3 );

	for (int i = 0; i < mNumImages; ++i)
	{
		r3dRenderer->SetTex(g_pPostFXChief->GetBuffer(sts.bloomParts[i])->Tex, i + 1);
	}
}

//------------------------------------------------------------------------
/*virtual*/

void
PFX_ComposeMultibloom::FinishImpl() /*OVERRIDE*/
{
	r3dRenderer->SetRenderingMode( R3D_BLEND_POP );

	const Settings& sts = mSettingsArr[ 0 ];

	if (g_pPostFXChief->GetZeroTexStageFilter())
		r3dSetFiltering( R3D_BILINEAR, 0 );
	r3dSetFiltering( R3D_POINT, 1 );
	r3dSetFiltering( R3D_POINT, 2 );
	r3dSetFiltering( R3D_POINT, 3 );

	if( mColorWriteMask != PostFXChief::DEFAULT_COLOR_WRITE_MASK )
	{
		g_pPostFXChief->SetDefaultColorWriteMask();
	}

	mSettingsArr.Erase( 0 );
}

//------------------------------------------------------------------------
/*virtual*/

void
PFX_ComposeMultibloom::PushDefaultSettingsImpl() /*OVERRIDE*/
{
	mSettingsArr.PushBack( Settings() ) ;
}

//------------------------------------------------------------------------

void
PFX_ComposeMultibloom::PushSettings( const Settings& settings ) 
{
	mSettingsArr.PushBack( settings );

	mSettingsPushed = 1;
}
