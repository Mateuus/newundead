#pragma once

#include "GameCommon.h"
#include "BaseItemSpawnPoint.h"

#include "APIScaleformGfx.h"

class obj_ItemSpawnPoint : public BaseItemSpawnPoint
{
	DECLARE_CLASS(obj_ItemSpawnPoint, BaseItemSpawnPoint)
private:
	int		m_SelectedSpawnPoint;
	bool m_bEditorCheckSpawnPointAtStart;

public:
	obj_ItemSpawnPoint();
	virtual ~obj_ItemSpawnPoint();

	virtual	BOOL		Load(const char *name);

	virtual	BOOL		OnCreate();
	virtual	BOOL		OnDestroy();

	virtual	BOOL		Update();
#ifndef FINAL_BUILD
	virtual	float		DrawPropertyEditor(float scrx, float scry, float scrw, float scrh, const AClass* startClass, const GameObjects& selected) OVERRIDE;
#endif

	void				DoDrawComposite( const r3dCamera& Cam );

	virtual void		AppendShadowRenderables( RenderArray & rarr, const r3dCamera& Cam ) OVERRIDE;
	virtual void		AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam ) OVERRIDE;

	virtual	BOOL		OnNetReceive(DWORD EventID, const void* packetData, int packetSize);
};
