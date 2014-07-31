#pragma once

#include "GameCommon.h"
#include "GameCode/UserProfile.h"
#include "SharedUsableItem.h"

class obj_DroppedItem : public SharedUsableItem
{
	DECLARE_CLASS(obj_DroppedItem, SharedUsableItem)
public:
	wiInventoryItem	m_Item;
	
public:
	obj_DroppedItem();
	virtual ~obj_DroppedItem();

	void				SetHighlight( bool highlight );
	bool				GetHighlight() const;

	void				UpdateObjectPositionAfterCreation();

	virtual	BOOL		Load(const char *name);

	virtual	BOOL		OnCreate();
	virtual	BOOL		OnDestroy();

	virtual	BOOL		Update();

	virtual void		AppendRenderables( RenderArray ( & render_arrays  )[ rsCount ], const r3dCamera& Cam ) OVERRIDE;
};
