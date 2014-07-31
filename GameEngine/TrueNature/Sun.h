#ifndef __R3DSUN_H
#define __R3DSUN_H


#include "TimeGradient.h"

class r3dSun
{
 public: 

	r3dVector		SunDir;
	int				bLoaded;
	
	r3dLight		SunLight;
	
	float			DawnTime;
	float			DuskTime;
	float			Time;			

	float			SunDirAngle;
	float			SunElevAngle;

	float			AngleRange;

	r3dColor		SunColor;

	r3dSun() { bLoaded = 0; }
	~r3dSun() { Unload(); }


	void	Init();
	void	Unload();
	
	void		SetLocation(float Angle1, float Angle2);
	r3dPoint3D	GetSunVecAtNormalizedTime( float CurTime );
	r3dPoint3D	GetCurrentSunVec();
	float		TimeToValD( float Time );
	void		SetParams(float Hour, float NewDawnTime, float NewDuskTime, float NewSunDirAngle, float NewSunElevAngle, r3dColor NewSunColor, float NewAngleRange );
	float		GetAngleAtNormalizedTime( float CurTime );

	void	DrawSun(const r3dCamera &Cam, int bReplicate = 1);
};

#endif // R3DSUN
