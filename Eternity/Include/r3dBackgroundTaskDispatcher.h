//=========================================================================
//	Module: r3dBackgroundTaskDispatcher.h
//	Copyright (C) 2011.
//=========================================================================

#pragma once

//////////////////////////////////////////////////////////////////////////

/**
 * This class is used to execute tasks in background thread.
 * Asset loading is example of such task.
 */

struct r3dTaskParams
{
	volatile LONG Taken ;

	r3dTaskParams() ;
};

//------------------------------------------------------------------------

R3D_FORCEINLINE
r3dTaskParams::r3dTaskParams()
: Taken( 0 )
{

}

//------------------------------------------------------------------------

struct r3dTaskPtrParams : r3dTaskParams
{
	r3dTaskPtrParams() ;

	void * Ptr ;
};

//------------------------------------------------------------------------

R3D_FORCEINLINE
r3dTaskPtrParams::r3dTaskPtrParams()
: Ptr( 0 )
{

}

//------------------------------------------------------------------------

template < typename T >
class r3dTaskParramsArray
{
public:
	typedef r3dTL::TArray< T > Elems ;

	r3dTaskParramsArray() ;
	~r3dTaskParramsArray() ;

public:
	void Init( int count ) ;
	T* Alloc() ;
	uint32_t MaxElemsCount() const { return m_Elems.Count(); }

private:
	Elems m_Elems ;
};

//------------------------------------------------------------------------

template < typename T >
r3dTaskParramsArray< T >::r3dTaskParramsArray()
{

}

//------------------------------------------------------------------------

template < typename T >
r3dTaskParramsArray< T >::~r3dTaskParramsArray()
{

}

//------------------------------------------------------------------------

template < typename T >
void
r3dTaskParramsArray< T >::Init( int count )
{
	m_Elems.Resize( count ) ;
}

//------------------------------------------------------------------------

template < typename T >
T* r3dTaskParramsArray< T >::Alloc()
{
	for( ; ; )
	{
		for( int i = 0, e = m_Elems.Count() ; i < e; i ++ )
		{
			// to give meaningful error if users doesn't inherit
			r3dTaskParams* params = static_cast<r3dTaskParams*>( &m_Elems[ i ] ) ;

			if( !params->Taken )
			{
				InterlockedExchange( &params->Taken, 1 ) ;
				return static_cast<T*>( params ) ;
			}
		}

		void r3dTaskParamTimeSlice() ;
		r3dTaskParamTimeSlice() ;
	}
}

//------------------------------------------------------------------------

class r3dBackgroundTaskDispatcher
{
public:
	typedef void (*ExecFunc)(r3dTaskParams* parameters);

	/**	Task descriptor structure. */
	struct TaskDescriptor
	{
		/**	Function to execute. */
		ExecFunc Fn;

		/**	Pointer to user-defined parameters. */
		r3dTaskParams *Params;

		/**	Pointer to int flag, that background thread set to 1 when task is done. */
		volatile LONG *CompletionFlag;

		TaskDescriptor(): Fn(0), Params(0), CompletionFlag(0) {}
	};

	r3dBackgroundTaskDispatcher();
	~r3dBackgroundTaskDispatcher();

	void Init() ;
	void Close() ;

	/**	Add background thread task. */
	void AddTask(const TaskDescriptor &td);
	
	r3dTaskPtrParams* AllocPtrTaskParam() ;

	int GetTaskCount() const ;

private:

	/**	Background thread handle. */
	HANDLE m_ThreadHandle;

	/**	Critical section for task scheduler synchronization purposes. */
	CRITICAL_SECTION m_TaskCS;

	/**	Handle to worker thread start event. */
	HANDLE m_StartEvent;

	volatile LONG m_TaskCount ;

	/**	Array with pending tasks. */
	r3dTL::TArray<TaskDescriptor> m_Tasks;
	r3dTaskParramsArray<r3dTaskPtrParams > m_TaskPtrParams ;
};

extern r3dBackgroundTaskDispatcher* g_pBackgroundTaskDispatcher;

//------------------------------------------------------------------------

void r3dFinishBackGroundTasks() ;
void r3dSetAsyncLoading( int onOff ) ;