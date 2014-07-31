#ifndef	__R3D_RENDERPIPELINE_H
#define	__R3D_RENDERPIPELINE_H

class r3dRenderPipeline;


class r3dRenderPipeline
{
 public :
	r3dRenderPipeline();
virtual	~r3dRenderPipeline();

virtual	void		Init();
virtual	void		Close();

virtual	void		PreRender();
virtual	void		Render();

virtual void		AppendPostFXes();

virtual	void		PostProcess();

// need this separate to be able to make render targets react to resolution changes
virtual void		CreateResolutionBoundResources() = 0;
virtual void		DestroyResolutionBoundResources() = 0;

virtual void		CreateShadowResources() = 0;
virtual void		DestroyShadowResources() = 0;

virtual void		DestroyAuxResource() = 0;
virtual void		CreateAuxResource() = 0;

virtual void		Finalize() = 0;

virtual void		SetPreColorCorrectionScreenshotTarget( r3dScreenBuffer* target ) = 0;

};

extern r3dRenderPipeline* CurRenderPipeline;

#endif	// __R3D_RENDERPIPELINE_H
