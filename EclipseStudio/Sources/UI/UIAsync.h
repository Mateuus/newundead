#pragma once

class r3dScaleformMovie;

template <typename T>
struct TAsyncFunctionPointer
{
	typedef void(T::*Type)();
};

template <typename T>
class UIAsync
{
public:
	typedef unsigned int (WINAPI *fn_thread)(void*);

	UIAsync() :
	lastServerReqTime_(-1),
	asyncThread_(NULL)
	{
		asyncErr_[0] = 0;
	}

	~UIAsync()
	{
		r3d_assert(asyncThread_ == NULL);
	}

	template<typename T>
	void StartAsyncOperation(T* object, fn_thread threadFn, typename TAsyncFunctionPointer<T>::Type finishFn = NULL)
	{
		r3d_assert(asyncThread_ == NULL);

		asyncFinish_ = finishFn;
		asyncErr_[0] = 0;
		asyncThread_ = (HANDLE)_beginthreadex(NULL, 0, threadFn, object, 0, NULL);
		if(asyncThread_ == NULL)
			r3dError("Failed to begin thread");
	}

	template<typename T>
	void ProcessAsyncOperation(T* object, r3dScaleformMovie& gfxMovie)
	{
		if(asyncThread_ == NULL)
			return;

		DWORD w0 = WaitForSingleObject(asyncThread_, 0);
		if(w0 == WAIT_TIMEOUT) 
			return;

		CloseHandle(asyncThread_);
		asyncThread_ = NULL;
		
		if(gMasterServerLogic.badClientVersion_)
		{
			Scaleform::GFx::Value args[2];
			args[0].SetStringW(gLangMngr.getString("ClientMustBeUpdated"));
			args[1].SetBoolean(true);
			gfxMovie.Invoke("_root.api.showInfoMsg", args, 2);		
			//@TODO: on infoMsg closing, exit app.
			return;
		}

		if(asyncErr_[0]) 
		{
			Scaleform::GFx::Value args[2];
			args[0].SetStringW(asyncErr_);
			args[1].SetBoolean(true);
			gfxMovie.Invoke("_root.api.showInfoMsg", args, 2);		
			return;
		}
		
		if(asyncFinish_)
			(object->*asyncFinish_)();
	}

	void DelayServerRequest()
	{
		// allow only one server request per second
		if(r3dGetTime() < lastServerReqTime_ + 1.0f)
		{
			::Sleep(1000);
		}
		lastServerReqTime_ = r3dGetTime();
	}

	void SetAsyncError(int apiCode, const wchar_t* msg)
	{
		if(gMasterServerLogic.shuttingDown_)
		{
			swprintf(asyncErr_, sizeof(asyncErr_), L"%s", gLangMngr.getString("MSShutdown1"));
			return;
		}

		if(apiCode == 0)
		{
			swprintf(asyncErr_, sizeof(asyncErr_), L"%s", msg);
		}
		else
		{
			swprintf(asyncErr_, sizeof(asyncErr_), L"%s, code:%d", msg, apiCode);
		}
	}

	bool Processing()
	{
		return asyncThread_ != NULL ? true : false;
	}

private:
	typename TAsyncFunctionPointer<T>::Type asyncFinish_;
	HANDLE asyncThread_;
	wchar_t	asyncErr_[512];

	float lastServerReqTime_;
};