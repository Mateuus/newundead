#include "r3dPCH.h"
#include "r3d.h"

#include "sun.h"

#include "../SF/Console/Config.h"


void r3dSun :: Init()
{
 if (bLoaded) Unload();

 bLoaded = 1;

 SetLocation(0, 45);

 DawnTime = 4;
 DuskTime = 21;

 SunLight.SetType(R3D_DIRECT_LIGHT);
 //SunLight.SetColor(SunColor);
 SunLight.bAffectBump = 1;
 SunLight.bCastShadows = 0;

 SunDirAngle	= 0.f;
 SunElevAngle	= 0.f;

 AngleRange = 200.f;

 SunColor = r3dColor::white;
}




void r3dSun :: Unload()
{
 if ( !bLoaded ) return;

 bLoaded = 0;
}



void r3dSun :: SetLocation(float Angle1, float Angle2)
{
 if (!bLoaded) return;

 r3dVector V = r3dVector(0,0,1);

 V.RotateAroundX(-Angle2);
 V.RotateAroundY(Angle1);
 V.Normalize();
 SunDir = V;

 //SunLight.SetColor(SunColor);
 SunLight.Direction = -SunDir;
}

r3dPoint3D r3dSun :: GetSunVecAtNormalizedTime( float CurTime )
{
	float Angle = GetAngleAtNormalizedTime( CurTime );

	D3DXMATRIX rotY, rotZ, rotY2;

	D3DXMatrixRotationY( &rotY, R3D_DEG2RAD( SunDirAngle ) );
	D3DXMatrixRotationZ( &rotZ, R3D_DEG2RAD( SunElevAngle ) - float(M_PI)*0.5f );
	D3DXMatrixRotationY( &rotY2, R3D_DEG2RAD( Angle ) - float(M_PI)*0.5f );

	D3DXMATRIX compoundRot = rotY * rotZ * rotY2;

	D3DXMatrixInverse( &compoundRot, NULL, &compoundRot );

	D3DXVECTOR3 DXSunVec( 1.0f, 0, 0 );

	D3DXVec3TransformNormal( &DXSunVec, &DXSunVec, &compoundRot );

	r3dPoint3D SunVec( DXSunVec.x, DXSunVec.y, DXSunVec.z );

	SunVec.Normalize();

	return SunVec;
}

r3dPoint3D	r3dSun :: GetCurrentSunVec()
{
	return GetSunVecAtNormalizedTime( TimeToValD( Time ) );
}

float r3dSun :: TimeToValD( float Time )
{
	float ValD = (Time-DawnTime) / (DuskTime-DawnTime);
	if (ValD <0 ) ValD = 0;
	if (ValD >1 ) ValD = 1;

	return ValD;
}

void r3dSun :: SetParams(float Hour, float NewDawnTime, float NewDuskTime, float NewSunDirAngle, float NewSunElevAngle, r3dColor NewSunColor, float NewAngleRange)
{
 if (!bLoaded) return;

 Time = Hour;
 DawnTime = NewDawnTime;
 DuskTime = NewDuskTime;

 SunDirAngle = NewSunDirAngle;
 SunElevAngle = NewSunElevAngle;

 AngleRange = NewAngleRange;

 SunColor = NewSunColor;

 if (Time < 0 ) Time = 0;
 if (Time > 24 ) Time = 0;

 float ValD = TimeToValD( Time );

 //V.RotateAroundX(-Angle2);
 //V.RotateAroundY(Angle1);
 //V.Normalize();

 r3dVector SunVec = GetSunVecAtNormalizedTime( ValD );


// 	if ( d_sun_rotate->GetBool() )
// 	{
// 		float fPhase = timeGetTime() * 0.001f;
// 		SunVec.x = cosf( fPhase );
// 		SunVec.y = 0.5f;
// 		SunVec.z = sinf( fPhase );
// 		SunVec.Normalize();
// 	}


 float Mult = -1.0f;

 SunDir = SunVec*Mult;
 SunLight.Direction = SunVec*Mult;

 SunLight.SetColor(SunColor);
// r3dRenderer->AmbientColor = AmbientColorG.GetColorValue(ValD);

  #if 1
  // FOR TEST: setup external particle system sun color/direction
  extern r3dColor  gPartShadeColor;
  extern r3dVector gPartShadeDir;
  gPartShadeDir = SunLight.Direction;
  gPartShadeDir.Y = 0; gPartShadeDir.Normalize();	// in 2D

  gPartShadeColor = SunColor;
  //gPartShadeColor = r3dColor(255, 255, 0);
  #endif
}

float r3dSun :: GetAngleAtNormalizedTime( float CurTime )
{
	return (CurTime-0.5f)*AngleRange+90.f;
}


void r3dSun :: DrawSun(const r3dCamera &Cam, int bReplicate)
{
}

