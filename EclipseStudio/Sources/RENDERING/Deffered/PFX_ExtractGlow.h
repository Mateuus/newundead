#pragma once

#include "PostFX.h"

class PFX_ExtractGlow : public PostFX
{
public:
	struct Settings
	{
		r3dColor	TintColor;
		float		Multiplier;
	};

	// construction/ destruction
public:
	PFX_ExtractGlow();
	~PFX_ExtractGlow();

	// manipulation/ access
public:
	const Settings& GetSettings() const;
	void			SetSettings( const Settings& settings );

	// polymorphism
private:
	virtual void InitImpl()								OVERRIDE;
	virtual	void CloseImpl()							OVERRIDE;
	virtual void PrepareImpl(	r3dScreenBuffer* dest,
								r3dScreenBuffer* src )	OVERRIDE;

	virtual void FinishImpl()							OVERRIDE;

	// data
private:
	Settings mSettings;

};