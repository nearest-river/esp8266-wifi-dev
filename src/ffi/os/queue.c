#include "../prelude.h"
#include "queue.h"
#include "port.h"
#include "task.h"
#include "portable.h"



/*
 * FreeRTOS Kernel V10.2.0
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

#include <stdlib.h>
#include <string.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

// TODO(nate)
#if ( configUSE_CO_ROUTINES == 1 )
	#include "croutine.h"
#endif

/* Lint e9021, e961 and e750 are suppressed as a MISRA exception justified
because the MPU ports require MPU_WRAPPERS_INCLUDED_FROM_API_FILE to be defined
for the header files above, but not in this file, in order to generate the
correct privileged Vs unprivileged linkage and placement. */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750 !e9021. */

/* When the Queue structure is used to represent a base queue its pcHead and
pcTail members are used as pointers into the queue storage area.  When the
Queue structure is used to represent a mutex pcHead and pcTail pointers are
not necessary, and the pcHead pointer is set to NULL to indicate that the
structure instead holds a pointer to the mutex holder (if any).  Map alternative
names to the pcHead and structure member to ensure the readability of the code
is maintained.  The QueuePointers_t and SemaphoreData types are used to form
a union as their usage is mutually exclusive dependent on what the queue is
being used for. */
#define uxQueueType						pcHead
#define queueQUEUE_IS_MUTEX				NULL


/* The old xQUEUE name is maintained above then typedefed to the new Queue
name below to enable the use of older kernel aware debuggers. */

/*-----------------------------------------------------------*/

/*
 * The queue registry is just a means for kernel aware debuggers to locate
 * queue structures.  It has no other purpose so is an optional component.
 */
#if ( configQUEUE_REGISTRY_SIZE > 0 )

	/* The type stored within the queue registry array.  This allows a name
	to be assigned to each queue making kernel aware debugging a little
	more user friendly. */
	typedef struct QUEUE_REGISTRY_ITEM
	{
		const char *pcQueueName; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
		Queue* xHandle;
	} xQueueRegistryItem;

	/* The old xQueueRegistryItem name is maintained above then typedefed to the
	new xQueueRegistryItem name below to enable the use of older kernel aware
	debuggers. */
	typedef xQueueRegistryItem QueueRegistryItem_t;

	/* The queue registry is simply an array of QueueRegistryItem_t structures.
	The pcQueueName member of a structure being NULL is indicative of the
	array position being vacant. */
	PRIVILEGED_DATA QueueRegistryItem_t xQueueRegistry[ configQUEUE_REGISTRY_SIZE ];

#endif /* configQUEUE_REGISTRY_SIZE */

/*
 * Unlocks a queue locked by a call to prvLockQueue.  Locking a queue does not
 * prevent an ISR from adding or removing items to the queue, but does prevent
 * an ISR from removing tasks from the queue event lists.  If an ISR finds a
 * queue is locked it will instead increment the appropriate queue lock count
 * to indicate that a task may require unblocking.  When the queue in unlocked
 * these lock counts are inspected, and the appropriate action taken.
 */
static void prvUnlockQueue( Queue * const pxQueue ) PRIVILEGED_FUNCTION;

/*
 * Uses a critical section to determine if there is any data in a queue.
 *
 * @return true if the queue contains no items, otherwise false.
 */
static isize prvIsQueueEmpty( const Queue *pxQueue ) PRIVILEGED_FUNCTION;

/*
 * Uses a critical section to determine if there is any space in a queue.
 *
 * @return true if there is no space, otherwise false;
 */
static isize prvIsQueueFull( const Queue *pxQueue ) PRIVILEGED_FUNCTION;

/*
 * Copies an item into the queue, either at the front of the queue or the
 * back of the queue.
 */
static isize prvCopyDataToQueue( Queue * const pxQueue, const void *pvItemToQueue, const isize xPosition ) PRIVILEGED_FUNCTION;

/*
 * Copies an item out of a queue.
 */
static void prvCopyDataFromQueue( Queue * const pxQueue, void * const pvBuffer ) PRIVILEGED_FUNCTION;

#if ( configUSE_QUEUE_SETS == 1 )
	/*
	 * Checks to see if a queue is a member of a queue set, and if so, notifies
	 * the queue set that the queue contains data.
	 */
	static isize prvNotifyQueueSetContainer( const Queue * const pxQueue, const isize xCopyPosition ) PRIVILEGED_FUNCTION;
#endif

/*
 * Called after a Queue structure has been allocated either statically or
 * dynamically to fill in the structure's members.
 */
static void prvInitialiseNewQueue( const usize uxQueueLength, const usize uxItemSize, u8* pucQueueStorage, const u8 ucQueueType, Queue *pxNewQueue ) PRIVILEGED_FUNCTION;

/*
 * Mutexes are a special type of queue.  When a mutex is created, first the
 * queue is created, then prvInitialiseMutex() is called to configure the queue
 * as a mutex.
 */
#if( configUSE_MUTEXES == 1 )
	static void prvInitialiseMutex( Queue *pxNewQueue ) PRIVILEGED_FUNCTION;
#endif

#if( configUSE_MUTEXES == 1 )
	/*
	 * If a task waiting for a mutex causes the mutex holder to inherit a
	 * priority, but the waiting task times out, then the holder should
	 * disinherit the priority - but only down to the highest priority of any
	 * other tasks that are waiting for the same mutex.  This function returns
	 * that priority.
	 */
	static usize prvGetDisinheritPriorityAfterTimeout( const Queue * const pxQueue ) PRIVILEGED_FUNCTION;
#endif
/*-----------------------------------------------------------*/

/*
 * Macro to mark a queue as locked.  Locking a queue prevents an ISR from
 * accessing the queue event lists.
 */
#define prvLockQueue( pxQueue )								\
	vPortEnterCritical();									\
	{														\
		if( ( pxQueue )->cRxLock == queueUNLOCKED )			\
		{													\
			( pxQueue )->cRxLock = queueLOCKED_UNMODIFIED;	\
		}													\
		if( ( pxQueue )->cTxLock == queueUNLOCKED )			\
		{													\
			( pxQueue )->cTxLock = queueLOCKED_UNMODIFIED;	\
		}													\
	}														\
	vPortExitCritical()
/*-----------------------------------------------------------*/

isize xQueueGenericReset( Queue* xQueue, isize xNewQueue )
{
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );

  vPortEnterCritical();
	{
		pxQueue->u.xQueue.pcTail = pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize ); /*lint !e9016 Pointer arithmetic allowed on char types, especially when it assists conveying intent. */
		pxQueue->uxMessagesWaiting = ( usize ) 0U;
		pxQueue->pcWriteTo = pxQueue->pcHead;
		pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead + ( ( pxQueue->uxLength - 1U ) * pxQueue->uxItemSize ); /*lint !e9016 Pointer arithmetic allowed on char types, especially when it assists conveying intent. */
		pxQueue->cRxLock = queueUNLOCKED;
		pxQueue->cTxLock = queueUNLOCKED;

		if( xNewQueue == false )
		{
			/* If there are tasks blocked waiting to read from the queue, then
			the tasks will remain blocked as after this function exits the queue
			will still be empty.  If there are tasks blocked waiting to write to
			the queue, then one should be unblocked as after this function exits
			it will be possible to write to it. */
			if(!listLIST_IS_EMPTY(&pxQueue->xTasksWaitingToSend))
			{
				if(xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != false )
				{
					queueYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			/* Ensure the event queues start in the correct state. */
			vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
			vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );
		}
	}
	vPortExitCritical();

	/* A value is returned for calling semantic consistency with previous
	versions. */
	return true;
}
/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

	Queue* xQueueGenericCreateStatic( const usize uxQueueLength, const usize uxItemSize, u8* pucQueueStorage, StaticQueue *pxStaticQueue, const u8 ucQueueType )
	{
	Queue *pxNewQueue;

		configASSERT( uxQueueLength > ( usize ) 0 );

		/* The StaticQueue structure and the queue storage area must be
		supplied. */
		configASSERT( pxStaticQueue != NULL );

		/* A queue storage area should be provided if the item size is not 0, and
		should not be provided if the item size is 0. */
		configASSERT( !( ( pucQueueStorage != NULL ) && ( uxItemSize == 0 ) ) );
		configASSERT( !( ( pucQueueStorage == NULL ) && ( uxItemSize != 0 ) ) );

		#if( configASSERT_DEFINED == 1 )
		{
			/* Sanity check that the size of the structure used to declare a
			variable of type StaticQueue or StaticSemaphore_t equals the size of
			the real queue and semaphore structures. */
			volatile usize xSize = sizeof( StaticQueue );
			configASSERT( xSize == sizeof( Queue ) );
			( void ) xSize; /* Keeps lint quiet when configASSERT() is not defined. */
		}
		#endif /* configASSERT_DEFINED */

		/* The address of a statically allocated queue was passed in, use it.
		The address of a statically allocated storage area was also passed in
		but is already set. */
		pxNewQueue = ( Queue * ) pxStaticQueue; /*lint !e740 !e9087 Unusual cast is ok as the structures are designed to have the same alignment, and the size is checked by an assert. */

		if( pxNewQueue != NULL )
		{
			#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
			{
				/* Queues can be allocated wither statically or dynamically, so
				note this queue was allocated statically in case the queue is
				later deleted. */
				pxNewQueue->ucStaticallyAllocated = true;
			}
			#endif /* configSUPPORT_DYNAMIC_ALLOCATION */

			prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
		}
		else
		{
			traceQUEUE_CREATE_FAILED( ucQueueType );
			mtCOVERAGE_TEST_MARKER();
		}

		return pxNewQueue;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	Queue* xQueueGenericCreate( const usize uxQueueLength, const usize uxItemSize, const u8 ucQueueType )
	{
	Queue *pxNewQueue;
	usize xQueueSizeInBytes;
	u8* pucQueueStorage;

		configASSERT( uxQueueLength > ( usize ) 0 );

		if( uxItemSize == ( usize ) 0 )
		{
			/* There is not going to be a queue storage area. */
			xQueueSizeInBytes = ( usize ) 0;
		}
		else
		{
			/* Allocate enough space to hold the maximum number of items that
			can be in the queue at any time. */
			xQueueSizeInBytes = ( usize ) ( uxQueueLength * uxItemSize ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
		}

		/* Allocate the queue and storage area.  Justification for MISRA
		deviation as follows:  pvPortMalloc() always ensures returned memory
		blocks are aligned per the requirements of the MCU stack.  In this case
		pvPortMalloc() must return a pointer that is guaranteed to meet the
		alignment requirements of the Queue structure - which in this case
		is an i8* .  Therefore, whenever the stack alignment requirements
		are greater than or equal to the pointer to char requirements the cast
		is safe.  In other cases alignment requirements are not strict (one or
		two bytes). */
		pxNewQueue = (Queue*)pvPortMalloc(sizeof(Queue) + xQueueSizeInBytes ); /*lint !e9087 !e9079 see comment above. */

		if( pxNewQueue != NULL )
		{
			/* Jump past the queue structure to find the location of the queue
			storage area. */
			pucQueueStorage = ( u8*  ) pxNewQueue;
			pucQueueStorage += sizeof( Queue ); /*lint !e9016 Pointer arithmetic allowed on char types, especially when it assists conveying intent. */

			#if( configSUPPORT_STATIC_ALLOCATION == 1 )
			{
				/* Queues can be created either statically or dynamically, so
				note this task was created dynamically in case it is later
				deleted. */
				pxNewQueue->ucStaticallyAllocated = false;
			}
			#endif /* configSUPPORT_STATIC_ALLOCATION */

			prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
		}
		else
		{
			// traceQUEUE_CREATE_FAILED( ucQueueType );
			mtCOVERAGE_TEST_MARKER();
		}

		return pxNewQueue;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

static void prvInitialiseNewQueue( const usize uxQueueLength, const usize uxItemSize, u8* pucQueueStorage, const u8 ucQueueType, Queue *pxNewQueue )
{
	/* Remove compiler warnings about unused parameters should
	configUSE_TRACE_FACILITY not be set to 1. */
	( void ) ucQueueType;

	if( uxItemSize == ( usize ) 0 )
	{
		/* No RAM was allocated for the queue storage area, but PC head cannot
		be set to NULL because NULL is used as a key to say the queue is used as
		a mutex.  Therefore just set pcHead to point to the queue as a benign
		value that is known to be within the memory map. */
		pxNewQueue->pcHead = ( i8*  ) pxNewQueue;
	}
	else
	{
		/* Set the head to the start of the queue storage area. */
		pxNewQueue->pcHead = ( i8*  ) pucQueueStorage;
	}

	/* Initialise the queue members as described where the queue type is
	defined. */
	pxNewQueue->uxLength = uxQueueLength;
	pxNewQueue->uxItemSize = uxItemSize;
	( void ) xQueueGenericReset( pxNewQueue, true );

	#if ( configUSE_TRACE_FACILITY == 1 )
	{
		pxNewQueue->ucQueueType = ucQueueType;
	}
	#endif /* configUSE_TRACE_FACILITY */

	#if( configUSE_QUEUE_SETS == 1 )
	{
		pxNewQueue->pxQueueSetContainer = NULL;
	}
	#endif /* configUSE_QUEUE_SETS */

	// traceQUEUE_CREATE( pxNewQueue );
}
/*-----------------------------------------------------------*/

#if( configUSE_MUTEXES == 1 )

	static void prvInitialiseMutex( Queue *pxNewQueue )
	{
		if( pxNewQueue != NULL )
		{
			/* The queue create function will set all the queue structure members
			correctly for a generic queue, but this function is creating a
			mutex.  Overwrite those members that need to be set differently -
			in particular the information required for priority inheritance. */
			pxNewQueue->u.xSemaphore.xMutexHolder = NULL;
			pxNewQueue->uxQueueType = queueQUEUE_IS_MUTEX;

			/* In case this is a recursive mutex. */
			pxNewQueue->u.xSemaphore.uxRecursiveCallCount = 0;

			// traceCREATE_MUTEX( pxNewQueue );

			/* Start with the semaphore in the expected state. */
			( void ) xQueueGenericSend( pxNewQueue, NULL, ( TickType ) 0U, queueSEND_TO_BACK );
		}
		else
		{
			// traceCREATE_MUTEX_FAILED();
		}
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	Queue* xQueueCreateMutex( const u8 ucQueueType )
	{
	Queue* xNewQueue;
	const usize uxMutexLength = ( usize ) 1, uxMutexSize = ( usize ) 0;

		xNewQueue = xQueueGenericCreate( uxMutexLength, uxMutexSize, ucQueueType );
		prvInitialiseMutex( ( Queue * ) xNewQueue );

		return xNewQueue;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

	Queue* xQueueCreateMutexStatic( const u8 ucQueueType, StaticQueue *pxStaticQueue )
	{
	Queue* xNewQueue;
	const usize uxMutexLength = ( usize ) 1, uxMutexSize = ( usize ) 0;

		/* Prevent compiler warnings about unused parameters if
		configUSE_TRACE_FACILITY does not equal 1. */
		( void ) ucQueueType;

		xNewQueue = xQueueGenericCreateStatic( uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType );
		prvInitialiseMutex( ( Queue * ) xNewQueue );

		return xNewQueue;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

	TaskHandle xQueueGetMutexHolder( Queue* xSemaphore )
	{
	TaskHandle pxReturn;
	Queue * const pxSemaphore = ( Queue * ) xSemaphore;

		/* This function is called by xSemaphoreGetMutexHolder(), and should not
		be called directly.  Note:  This is a good way of determining if the
		calling task is the mutex holder, but not a good way of determining the
		identity of the mutex holder, as the holder may change between the
		following critical section exiting and the function returning. */
		vPortEnterCritical();
		{
			if( pxSemaphore->uxQueueType == queueQUEUE_IS_MUTEX )
			{
				pxReturn = pxSemaphore->u.xSemaphore.xMutexHolder;
			}
			else
			{
				pxReturn = NULL;
			}
		}
		vPortExitCritical();

		return pxReturn;
	} /*lint !e818 xSemaphore cannot be a pointer to const because it is a typedef. */

#endif
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

	TaskHandle xQueueGetMutexHolderFromISR( Queue* xSemaphore )
	{
	TaskHandle pxReturn;

		configASSERT( xSemaphore );

		/* Mutexes cannot be used in interrupt service routines, so the mutex
		holder should not change in an ISR, and therefore a critical section is
		not required here. */
		if( ( ( Queue * ) xSemaphore )->uxQueueType == queueQUEUE_IS_MUTEX )
		{
			pxReturn = ( ( Queue * ) xSemaphore )->u.xSemaphore.xMutexHolder;
		}
		else
		{
			pxReturn = NULL;
		}

		return pxReturn;
	} /*lint !e818 xSemaphore cannot be a pointer to const because it is a typedef. */

#endif
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

	isize xQueueGiveMutexRecursive( Queue* xMutex )
	{
	isize xReturn;
	Queue * const pxMutex = ( Queue * ) xMutex;

		configASSERT( pxMutex );

		/* If this is the task that holds the mutex then xMutexHolder will not
		change outside of this task.  If this task does not hold the mutex then
		pxMutexHolder can never coincidentally equal the tasks handle, and as
		this is the only condition we are interested in it does not matter if
		pxMutexHolder is accessed simultaneously by another task.  Therefore no
		mutual exclusion is required to test the pxMutexHolder variable. */
		if( pxMutex->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle() )
		{
			// traceGIVE_MUTEX_RECURSIVE( pxMutex );

			/* uxRecursiveCallCount cannot be zero if xMutexHolder is equal to
			the task handle, therefore no underflow check is required.  Also,
			uxRecursiveCallCount is only modified by the mutex holder, and as
			there can only be one, no mutual exclusion is required to modify the
			uxRecursiveCallCount member. */
			( pxMutex->u.xSemaphore.uxRecursiveCallCount )--;

			/* Has the recursive call count unwound to 0? */
			if( pxMutex->u.xSemaphore.uxRecursiveCallCount == ( usize ) 0 )
			{
				/* Return the mutex.  This will automatically unblock any other
				task that might be waiting to access the mutex. */
				( void ) xQueueGenericSend( pxMutex, NULL, queueMUTEX_GIVE_BLOCK_TIME, queueSEND_TO_BACK );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			xReturn = true;
		}
		else
		{
			/* The mutex cannot be given because the calling task is not the
			holder. */
			xReturn = false;

			// traceGIVE_MUTEX_RECURSIVE_FAILED( pxMutex );
		}

		return xReturn;
	}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

	isize xQueueTakeMutexRecursive( Queue* xMutex, TickType xTicksToWait )
	{
	isize xReturn;
	Queue * const pxMutex = ( Queue * ) xMutex;

		configASSERT( pxMutex );

		/* Comments regarding mutual exclusion as per those within
		xQueueGiveMutexRecursive(). */

		// traceTAKE_MUTEX_RECURSIVE( pxMutex );

		if( pxMutex->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle() )
		{
			( pxMutex->u.xSemaphore.uxRecursiveCallCount )++;
			xReturn = true;
		}
		else
		{
			xReturn = xQueueSemaphoreTake( pxMutex, xTicksToWait );

			/* true will only be returned if the mutex was successfully
			obtained.  The calling task may have entered the Blocked state
			before reaching here. */
			if( xReturn != false )
			{
				( pxMutex->u.xSemaphore.uxRecursiveCallCount )++;
			}
			else
			{
				// traceTAKE_MUTEX_RECURSIVE_FAILED( pxMutex );
			}
		}

		return xReturn;
	}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

	Queue* xQueueCreateCountingSemaphoreStatic( const usize uxMaxCount, const usize uxInitialCount, StaticQueue *pxStaticQueue )
	{
	Queue* xHandle;

		configASSERT( uxMaxCount != 0 );
		configASSERT( uxInitialCount <= uxMaxCount );

		xHandle = xQueueGenericCreateStatic( uxMaxCount, queueSEMAPHORE_QUEUE_ITEM_LENGTH, NULL, pxStaticQueue, queueQUEUEYPE_COUNTING_SEMAPHORE );

		if( xHandle != NULL )
		{
			( ( Queue * ) xHandle )->uxMessagesWaiting = uxInitialCount;

			traceCREATE_COUNTING_SEMAPHORE();
		}
		else
		{
			traceCREATE_COUNTING_SEMAPHORE_FAILED();
		}

		return xHandle;
	}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	Queue* xQueueCreateCountingSemaphore( const usize uxMaxCount, const usize uxInitialCount )
	{
	Queue* xHandle;

		configASSERT( uxMaxCount != 0 );
		configASSERT( uxInitialCount <= uxMaxCount );

		xHandle = xQueueGenericCreate( uxMaxCount, queueSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUEYPE_COUNTING_SEMAPHORE );

		if( xHandle != NULL )
		{
			( ( Queue * ) xHandle )->uxMessagesWaiting = uxInitialCount;

			traceCREATE_COUNTING_SEMAPHORE();
		}
		else
		{
			traceCREATE_COUNTING_SEMAPHORE_FAILED();
		}

		return xHandle;
	}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

isize xQueueGenericSend( Queue* xQueue, const void * const pvItemToQueue, TickType xTicksToWait, const isize xCopyPosition )
{
isize xEntryTimeSet = false, xYieldRequired;
TimeOut xTimeOut;
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( usize ) 0U ) ) );
	configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif


	/*lint -save -e904 This function relaxes the coding standard somewhat to
	allow return statements within the function itself.  This is done in the
	interest of execution time efficiency. */
	for( ;; )
	{
		vPortEnterCritical();
		{
			/* Is there room on the queue now?  The running task must be the
			highest priority task wanting to access the queue.  If the head item
			in the queue is to be overwritten then it does not matter if the
			queue is full. */
			if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
			{
				// traceQUEUE_SEND( pxQueue );

				#if ( configUSE_QUEUE_SETS == 1 )
				{
				usize uxPreviousMessagesWaiting = pxQueue->uxMessagesWaiting;

					xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

					if( pxQueue->pxQueueSetContainer != NULL )
					{
						if( ( xCopyPosition == queueOVERWRITE ) && ( uxPreviousMessagesWaiting != ( usize ) 0 ) )
						{
							/* Do not notify the queue set as an existing item
							was overwritten in the queue so the number of items
							in the queue has not changed. */
							mtCOVERAGE_TEST_MARKER();
						}
						else if( prvNotifyQueueSetContainer( pxQueue, xCopyPosition ) != false )
						{
							/* The queue is a member of a queue set, and posting
							to the queue set caused a higher priority task to
							unblock. A context switch is required. */
							queueYIELD_IF_USING_PREEMPTION();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						/* If there was a task waiting for data to arrive on the
						queue then unblock it now. */
						if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
						{
							if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
							{
								/* The unblocked task has a priority higher than
								our own so yield immediately.  Yes it is ok to
								do this from within the critical section - the
								kernel takes care of that. */
								queueYIELD_IF_USING_PREEMPTION();
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
						}
						else if( xYieldRequired != false )
						{
							/* This path is a special case that will only get
							executed if the task was holding multiple mutexes
							and the mutexes were given back in an order that is
							different to that in which they were taken. */
							queueYIELD_IF_USING_PREEMPTION();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
				}
				#else /* configUSE_QUEUE_SETS */
				{
					xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

					/* If there was a task waiting for data to arrive on the
					queue then unblock it now. */
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
						{
							/* The unblocked task has a priority higher than
							our own so yield immediately.  Yes it is ok to do
							this from within the critical section - the kernel
							takes care of that. */
							queueYIELD_IF_USING_PREEMPTION();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else if( xYieldRequired != false )
					{
						/* This path is a special case that will only get
						executed if the task was holding multiple mutexes and
						the mutexes were given back in an order that is
						different to that in which they were taken. */
						queueYIELD_IF_USING_PREEMPTION();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif /* configUSE_QUEUE_SETS */

				vPortExitCritical();
				return true;
			}
			else
			{
				if( xTicksToWait == ( TickType ) 0 )
				{
					/* The queue was full and no block time is specified (or
					the block time has expired) so leave now. */
					vPortExitCritical();

					/* Return to the original privilege level before exiting
					the function. */
					// traceQUEUE_SEND_FAILED( pxQueue );
					return errQUEUE_FULL;
				}
				else if( xEntryTimeSet == false )
				{
					/* The queue was full and a block time was specified so
					configure the timeout structure. */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = true;
				}
				else
				{
					/* Entry time was already set. */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		vPortExitCritical();

		/* Interrupts and other tasks can send to and receive from the queue
		now the critical section has been exited. */

		vTaskSuspendAll();
		prvLockQueue( pxQueue );

		/* Update the timeout state to see if it has expired yet. */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == false )
		{
			if( prvIsQueueFull( pxQueue ) != false )
			{
				// traceBLOCKING_ON_QUEUE_SEND( pxQueue );
				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

				/* Unlocking the queue means queue events can effect the
				event list.  It is possible that interrupts occurring now
				remove this task from the event list again - but as the
				scheduler is suspended the task will go onto the pending
				ready last instead of the actual ready list. */
				prvUnlockQueue( pxQueue );

				/* Resuming the scheduler will move tasks from the pending
				ready list into the ready list - so it is feasible that this
				task is already in a ready list before it yields - in which
				case the yield will not cause a context switch unless there
				is also a higher priority task in the pending ready list. */
				if( xTaskResumeAll() == false )
				{
					portYIELD_WITHIN_API();
				}
			}
			else
			{
				/* Try again. */
				prvUnlockQueue( pxQueue );
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			/* The timeout has expired. */
			prvUnlockQueue( pxQueue );
			( void ) xTaskResumeAll();

			// traceQUEUE_SEND_FAILED( pxQueue );
			return errQUEUE_FULL;
		}
	} /*lint -restore */
}
/*-----------------------------------------------------------*/

isize xQueueGenericSendFromISR( Queue* xQueue, const void * const pvItemToQueue, isize * const pxHigherPriorityTaskWoken, const isize xCopyPosition )
{
isize xReturn;
usize uxSavedInterruptStatus;
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( usize ) 0U ) ) );
	configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );

	/* RTOS ports that support interrupt nesting have the concept of a maximum
	system call (or maximum API call) interrupt priority.  Interrupts that are
	above the maximum system call priority are kept permanently enabled, even
	when the RTOS kernel is in a critical section, but cannot make any calls to
	FreeRTOS API functions.  If configASSERT() is defined in FreeRTOSConfig.h
	then portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
	failure if a FreeRTOS API function is called from an interrupt that has been
	assigned a priority above the configured maximum system call priority.
	Only FreeRTOS functions that end in FromISR can be called from interrupts
	that have been assigned a priority at or (logically) below the maximum
	system call	interrupt priority.  FreeRTOS maintains a separate interrupt
	safe API to ensure interrupt entry is as fast and as simple as possible.
	More information (albeit Cortex-M specific) is provided on the following
	link: http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	/* Similar to xQueueGenericSend, except without blocking if there is no room
	in the queue.  Also don't directly wake a task that was blocked on a queue
	read, instead return a flag to say whether a context switch is required or
	not (i.e. has a task with a higher priority than us been woken by this
	post). */
	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
		{
			const i8 cTxLock = pxQueue->cTxLock;

			// traceQUEUE_SEND_FROM_ISR( pxQueue );

			/* Semaphores use xQueueGiveFromISR(), so pxQueue will not be a
			semaphore or mutex.  That means prvCopyDataToQueue() cannot result
			in a task disinheriting a priority and prvCopyDataToQueue() can be
			called here even though the disinherit function does not check if
			the scheduler is suspended before accessing the ready lists. */
			( void ) prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

			/* The event list is not altered if the queue is locked.  This will
			be done when the queue is unlocked later. */
			if( cTxLock == queueUNLOCKED )
			{
				#if ( configUSE_QUEUE_SETS == 1 )
				{
					if( pxQueue->pxQueueSetContainer != NULL )
					{
						if( prvNotifyQueueSetContainer( pxQueue, xCopyPosition ) != false )
						{
							/* The queue is a member of a queue set, and posting
							to the queue set caused a higher priority task to
							unblock.  A context switch is required. */
							if( pxHigherPriorityTaskWoken != NULL )
							{
								*pxHigherPriorityTaskWoken = true;
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
						{
							if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
							{
								/* The task waiting has a higher priority so
								record that a context switch is required. */
								if( pxHigherPriorityTaskWoken != NULL )
								{
									*pxHigherPriorityTaskWoken = true;
								}
								else
								{
									mtCOVERAGE_TEST_MARKER();
								}
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
				}
				#else /* configUSE_QUEUE_SETS */
				{
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
						{
							/* The task waiting has a higher priority so record that a
							context	switch is required. */
							if( pxHigherPriorityTaskWoken != NULL )
							{
								*pxHigherPriorityTaskWoken = true;
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif /* configUSE_QUEUE_SETS */
			}
			else
			{
				/* Increment the lock count so the task that unlocks the queue
				knows that data was posted while it was locked. */
				pxQueue->cTxLock = ( i8 ) ( cTxLock + 1 );
			}

			xReturn = true;
		}
		else
		{
			// traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
			xReturn = errQUEUE_FULL;
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

isize xQueueGiveFromISR( Queue* xQueue, isize * const pxHigherPriorityTaskWoken )
{
isize xReturn;
usize uxSavedInterruptStatus;
Queue * const pxQueue = xQueue;

	/* Similar to xQueueGenericSendFromISR() but used with semaphores where the
	item size is 0.  Don't directly wake a task that was blocked on a queue
	read, instead return a flag to say whether a context switch is required or
	not (i.e. has a task with a higher priority than us been woken by this
	post). */

	configASSERT( pxQueue );

	/* xQueueGenericSendFromISR() should be used instead of xQueueGiveFromISR()
	if the item size is not 0. */
	configASSERT( pxQueue->uxItemSize == 0 );

	/* Normally a mutex would not be given from an interrupt, especially if
	there is a mutex holder, as priority inheritance makes no sense for an
	interrupts, only tasks. */
	configASSERT( !( ( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX ) && ( pxQueue->u.xSemaphore.xMutexHolder != NULL ) ) );

	/* RTOS ports that support interrupt nesting have the concept of a maximum
	system call (or maximum API call) interrupt priority.  Interrupts that are
	above the maximum system call priority are kept permanently enabled, even
	when the RTOS kernel is in a critical section, but cannot make any calls to
	FreeRTOS API functions.  If configASSERT() is defined in FreeRTOSConfig.h
	then portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
	failure if a FreeRTOS API function is called from an interrupt that has been
	assigned a priority above the configured maximum system call priority.
	Only FreeRTOS functions that end in FromISR can be called from interrupts
	that have been assigned a priority at or (logically) below the maximum
	system call	interrupt priority.  FreeRTOS maintains a separate interrupt
	safe API to ensure interrupt entry is as fast and as simple as possible.
	More information (albeit Cortex-M specific) is provided on the following
	link: http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		const usize uxMessagesWaiting = pxQueue->uxMessagesWaiting;

		/* When the queue is used to implement a semaphore no data is ever
		moved through the queue but it is still valid to see if the queue 'has
		space'. */
		if( uxMessagesWaiting < pxQueue->uxLength )
		{
			const i8 cTxLock = pxQueue->cTxLock;

			// traceQUEUE_SEND_FROM_ISR( pxQueue );

			/* A task can only have an inherited priority if it is a mutex
			holder - and if there is a mutex holder then the mutex cannot be
			given from an ISR.  As this is the ISR version of the function it
			can be assumed there is no mutex holder and no need to determine if
			priority disinheritance is needed.  Simply increase the count of
			messages (semaphores) available. */
			pxQueue->uxMessagesWaiting = uxMessagesWaiting + ( usize ) 1;

			/* The event list is not altered if the queue is locked.  This will
			be done when the queue is unlocked later. */
			if( cTxLock == queueUNLOCKED )
			{
				#if ( configUSE_QUEUE_SETS == 1 )
				{
					if( pxQueue->pxQueueSetContainer != NULL )
					{
						if( prvNotifyQueueSetContainer( pxQueue, queueSEND_TO_BACK ) != false )
						{
							/* The semaphore is a member of a queue set, and
							posting	to the queue set caused a higher priority
							task to	unblock.  A context switch is required. */
							if( pxHigherPriorityTaskWoken != NULL )
							{
								*pxHigherPriorityTaskWoken = true;
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
						{
							if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
							{
								/* The task waiting has a higher priority so
								record that a context switch is required. */
								if( pxHigherPriorityTaskWoken != NULL )
								{
									*pxHigherPriorityTaskWoken = true;
								}
								else
								{
									mtCOVERAGE_TEST_MARKER();
								}
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
				}
				#else /* configUSE_QUEUE_SETS */
				{
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
						{
							/* The task waiting has a higher priority so record that a
							context	switch is required. */
							if( pxHigherPriorityTaskWoken != NULL )
							{
								*pxHigherPriorityTaskWoken = true;
							}
							else
							{
								mtCOVERAGE_TEST_MARKER();
							}
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif /* configUSE_QUEUE_SETS */
			}
			else
			{
				/* Increment the lock count so the task that unlocks the queue
				knows that data was posted while it was locked. */
				pxQueue->cTxLock = ( i8 ) ( cTxLock + 1 );
			}

			xReturn = true;
		}
		else
		{
			// traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
			xReturn = errQUEUE_FULL;
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

isize xQueueReceive( Queue* xQueue, void * const pvBuffer, TickType xTicksToWait )
{
isize xEntryTimeSet = false;
TimeOut xTimeOut;
Queue * const pxQueue = xQueue;

	/* Check the pointer is not NULL. */
	configASSERT( ( pxQueue ) );

	/* The buffer into which data is received can only be NULL if the data size
	is zero (so no data is copied into the buffer. */
	configASSERT( !( ( ( pvBuffer ) == NULL ) && ( ( pxQueue )->uxItemSize != ( usize ) 0U ) ) );

	/* Cannot block if the scheduler is suspended. */
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif


	/*lint -save -e904  This function relaxes the coding standard somewhat to
	allow return statements within the function itself.  This is done in the
	interest of execution time efficiency. */
	for( ;; )
	{
		vPortEnterCritical();
		{
			const usize uxMessagesWaiting = pxQueue->uxMessagesWaiting;

			/* Is there data in the queue now?  To be running the calling task
			must be the highest priority task wanting to access the queue. */
			if( uxMessagesWaiting > ( usize ) 0 )
			{
				/* Data available, remove one item. */
				prvCopyDataFromQueue( pxQueue, pvBuffer );
				// traceQUEUE_RECEIVE( pxQueue );
				pxQueue->uxMessagesWaiting = uxMessagesWaiting - ( usize ) 1;

				/* There is now space in the queue, were any tasks waiting to
				post to the queue?  If so, unblock the highest priority waiting
				task. */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == false )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != false )
					{
						queueYIELD_IF_USING_PREEMPTION();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				vPortExitCritical();
				return true;
			}
			else
			{
				if( xTicksToWait == ( TickType ) 0 )
				{
					/* The queue was empty and no block time is specified (or
					the block time has expired) so leave now. */
					vPortExitCritical();
					// traceQUEUE_RECEIVE_FAILED( pxQueue );
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == false )
				{
					/* The queue was empty and a block time was specified so
					configure the timeout structure. */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = true;
				}
				else
				{
					/* Entry time was already set. */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		vPortExitCritical();

		/* Interrupts and other tasks can send to and receive from the queue
		now the critical section has been exited. */

		vTaskSuspendAll();
		prvLockQueue( pxQueue );

		/* Update the timeout state to see if it has expired yet. */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == false )
		{
			/* The timeout has not expired.  If the queue is still empty place
			the task on the list of tasks waiting to receive from the queue. */
			if( prvIsQueueEmpty( pxQueue ) != false )
			{
				// traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );
				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
				prvUnlockQueue( pxQueue );
				if( xTaskResumeAll() == false )
				{
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* The queue contains data again.  Loop back to try and read the
				data. */
				prvUnlockQueue( pxQueue );
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			/* Timed out.  If there is no data in the queue exit, otherwise loop
			back and attempt to read the data. */
			prvUnlockQueue( pxQueue );
			( void ) xTaskResumeAll();

			if( prvIsQueueEmpty( pxQueue ) != false )
			{
				// traceQUEUE_RECEIVE_FAILED( pxQueue );
				return errQUEUE_EMPTY;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	} /*lint -restore */
}
/*-----------------------------------------------------------*/

isize xQueueSemaphoreTake( Queue* xQueue, TickType xTicksToWait )
{
isize xEntryTimeSet = false;
TimeOut xTimeOut;
Queue * const pxQueue = xQueue;

#if( configUSE_MUTEXES == 1 )
	isize xInheritanceOccurred = false;
#endif

	/* Check the queue pointer is not NULL. */
	configASSERT( ( pxQueue ) );

	/* Check this really is a semaphore, in which case the item size will be
	0. */
	configASSERT( pxQueue->uxItemSize == 0 );

	/* Cannot block if the scheduler is suspended. */
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif


	/*lint -save -e904 This function relaxes the coding standard somewhat to allow return
	statements within the function itself.  This is done in the interest
	of execution time efficiency. */
	for( ;; )
	{
		vPortEnterCritical();
		{
			/* Semaphores are queues with an item size of 0, and where the
			number of messages in the queue is the semaphore's count value. */
			const usize uxSemaphoreCount = pxQueue->uxMessagesWaiting;

			/* Is there data in the queue now?  To be running the calling task
			must be the highest priority task wanting to access the queue. */
			if( uxSemaphoreCount > ( usize ) 0 )
			{
				// traceQUEUE_RECEIVE( pxQueue );

				/* Semaphores are queues with a data size of zero and where the
				messages waiting is the semaphore's count.  Reduce the count. */
				pxQueue->uxMessagesWaiting = uxSemaphoreCount - ( usize ) 1;

				#if ( configUSE_MUTEXES == 1 )
				{
					if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
					{
						/* Record the information required to implement
						priority inheritance should it become necessary. */
						pxQueue->u.xSemaphore.xMutexHolder = pvTaskIncrementMutexHeldCount();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif /* configUSE_MUTEXES */

				/* Check to see if other tasks are blocked waiting to give the
				semaphore, and if so, unblock the highest priority such task. */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == false )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != false )
					{
						queueYIELD_IF_USING_PREEMPTION();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				vPortExitCritical();
				return true;
			}
			else
			{
				if( xTicksToWait == ( TickType ) 0 )
				{
					/* For inheritance to have occurred there must have been an
					initial timeout, and an adjusted timeout cannot become 0, as
					if it were 0 the function would have exited. */
					#if( configUSE_MUTEXES == 1 )
					{
						configASSERT( xInheritanceOccurred == false );
					}
					#endif /* configUSE_MUTEXES */

					/* The semaphore count was 0 and no block time is specified
					(or the block time has expired) so exit now. */
					vPortExitCritical();
					// traceQUEUE_RECEIVE_FAILED( pxQueue );
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == false )
				{
					/* The semaphore count was 0 and a block time was specified
					so configure the timeout structure ready to block. */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = true;
				}
				else
				{
					/* Entry time was already set. */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		vPortExitCritical();

		/* Interrupts and other tasks can give to and take from the semaphore
		now the critical section has been exited. */

		vTaskSuspendAll();
		prvLockQueue( pxQueue );

		/* Update the timeout state to see if it has expired yet. */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == false )
		{
			/* A block time is specified and not expired.  If the semaphore
			count is 0 then enter the Blocked state to wait for a semaphore to
			become available.  As semaphores are implemented with queues the
			queue being empty is equivalent to the semaphore count being 0. */
			if( prvIsQueueEmpty( pxQueue ) != false )
			{
				// traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );

				#if ( configUSE_MUTEXES == 1 )
				{
					if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
					{
						vPortEnterCritical();
						{
							xInheritanceOccurred = xTaskPriorityInherit( pxQueue->u.xSemaphore.xMutexHolder );
						}
						vPortExitCritical();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				#endif

				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
				prvUnlockQueue( pxQueue );
				if( xTaskResumeAll() == false )
				{
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* There was no timeout and the semaphore count was not 0, so
				attempt to take the semaphore again. */
				prvUnlockQueue( pxQueue );
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			/* Timed out. */
			prvUnlockQueue( pxQueue );
			( void ) xTaskResumeAll();

			/* If the semaphore count is 0 exit now as the timeout has
			expired.  Otherwise return to attempt to take the semaphore that is
			known to be available.  As semaphores are implemented by queues the
			queue being empty is equivalent to the semaphore count being 0. */
			if( prvIsQueueEmpty( pxQueue ) != false )
			{
				#if ( configUSE_MUTEXES == 1 )
				{
					/* xInheritanceOccurred could only have be set if
					pxQueue->uxQueueType == queueQUEUE_IS_MUTEX so no need to
					test the mutex type again to check it is actually a mutex. */
					if( xInheritanceOccurred != false )
					{
						vPortEnterCritical();
						{
							usize uxHighestWaitingPriority;

							/* This task blocking on the mutex caused another
							task to inherit this task's priority.  Now this task
							has timed out the priority should be disinherited
							again, but only as low as the next highest priority
							task that is waiting for the same mutex. */
							uxHighestWaitingPriority = prvGetDisinheritPriorityAfterTimeout( pxQueue );
							vTaskPriorityDisinheritAfterTimeout( pxQueue->u.xSemaphore.xMutexHolder, uxHighestWaitingPriority );
						}
						vPortExitCritical();
					}
				}
				#endif /* configUSE_MUTEXES */

				// traceQUEUE_RECEIVE_FAILED( pxQueue );
				return errQUEUE_EMPTY;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	} /*lint -restore */
}
/*-----------------------------------------------------------*/

isize xQueuePeek( Queue* xQueue, void * const pvBuffer, TickType xTicksToWait )
{
isize xEntryTimeSet = false;
TimeOut xTimeOut;
i8* pcOriginalReadPosition;
Queue * const pxQueue = xQueue;

	/* Check the pointer is not NULL. */
	configASSERT( ( pxQueue ) );

	/* The buffer into which data is received can only be NULL if the data size
	is zero (so no data is copied into the buffer. */
	configASSERT( !( ( ( pvBuffer ) == NULL ) && ( ( pxQueue )->uxItemSize != ( usize ) 0U ) ) );

	/* Cannot block if the scheduler is suspended. */
	#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
	{
		configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
	}
	#endif


	/*lint -save -e904  This function relaxes the coding standard somewhat to
	allow return statements within the function itself.  This is done in the
	interest of execution time efficiency. */
	for( ;; )
	{
		vPortEnterCritical();
		{
			const usize uxMessagesWaiting = pxQueue->uxMessagesWaiting;

			/* Is there data in the queue now?  To be running the calling task
			must be the highest priority task wanting to access the queue. */
			if( uxMessagesWaiting > ( usize ) 0 )
			{
				/* Remember the read position so it can be reset after the data
				is read from the queue as this function is only peeking the
				data, not removing it. */
				pcOriginalReadPosition = pxQueue->u.xQueue.pcReadFrom;

				prvCopyDataFromQueue( pxQueue, pvBuffer );
				// traceQUEUE_PEEK( pxQueue );

				/* The data is not being removed, so reset the read pointer. */
				pxQueue->u.xQueue.pcReadFrom = pcOriginalReadPosition;

				/* The data is being left in the queue, so see if there are
				any other tasks waiting for the data. */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
					{
						/* The task waiting has a higher priority than this task. */
						queueYIELD_IF_USING_PREEMPTION();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				vPortExitCritical();
				return true;
			}
			else
			{
				if( xTicksToWait == ( TickType ) 0 )
				{
					/* The queue was empty and no block time is specified (or
					the block time has expired) so leave now. */
					vPortExitCritical();
					// traceQUEUE_PEEK_FAILED( pxQueue );
					return errQUEUE_EMPTY;
				}
				else if( xEntryTimeSet == false )
				{
					/* The queue was empty and a block time was specified so
					configure the timeout structure ready to enter the blocked
					state. */
					vTaskInternalSetTimeOutState( &xTimeOut );
					xEntryTimeSet = true;
				}
				else
				{
					/* Entry time was already set. */
					mtCOVERAGE_TEST_MARKER();
				}
			}
		}
		vPortExitCritical();

		/* Interrupts and other tasks can send to and receive from the queue
		now the critical section has been exited. */

		vTaskSuspendAll();
		prvLockQueue( pxQueue );

		/* Update the timeout state to see if it has expired yet. */
		if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == false )
		{
			/* Timeout has not expired yet, check to see if there is data in the
			queue now, and if not enter the Blocked state to wait for data. */
			if( prvIsQueueEmpty( pxQueue ) != false )
			{
				// traceBLOCKING_ON_QUEUE_PEEK( pxQueue );
				vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
				prvUnlockQueue( pxQueue );
				if( xTaskResumeAll() == false )
				{
					portYIELD_WITHIN_API();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* There is data in the queue now, so don't enter the blocked
				state, instead return to try and obtain the data. */
				prvUnlockQueue( pxQueue );
				( void ) xTaskResumeAll();
			}
		}
		else
		{
			/* The timeout has expired.  If there is still no data in the queue
			exit, otherwise go back and try to read the data again. */
			prvUnlockQueue( pxQueue );
			( void ) xTaskResumeAll();

			if( prvIsQueueEmpty( pxQueue ) != false )
			{
				// traceQUEUE_PEEK_FAILED( pxQueue );
				return errQUEUE_EMPTY;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	} /*lint -restore */
}
/*-----------------------------------------------------------*/

isize xQueueReceiveFromISR( Queue* xQueue, void * const pvBuffer, isize * const pxHigherPriorityTaskWoken )
{
isize xReturn;
usize uxSavedInterruptStatus;
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( usize ) 0U ) ) );

	/* RTOS ports that support interrupt nesting have the concept of a maximum
	system call (or maximum API call) interrupt priority.  Interrupts that are
	above the maximum system call priority are kept permanently enabled, even
	when the RTOS kernel is in a critical section, but cannot make any calls to
	FreeRTOS API functions.  If configASSERT() is defined in FreeRTOSConfig.h
	then portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
	failure if a FreeRTOS API function is called from an interrupt that has been
	assigned a priority above the configured maximum system call priority.
	Only FreeRTOS functions that end in FromISR can be called from interrupts
	that have been assigned a priority at or (logically) below the maximum
	system call	interrupt priority.  FreeRTOS maintains a separate interrupt
	safe API to ensure interrupt entry is as fast and as simple as possible.
	More information (albeit Cortex-M specific) is provided on the following
	link: http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		const usize uxMessagesWaiting = pxQueue->uxMessagesWaiting;

		/* Cannot block in an ISR, so check there is data available. */
		if( uxMessagesWaiting > ( usize ) 0 )
		{
			const i8 cRxLock = pxQueue->cRxLock;

			// traceQUEUE_RECEIVE_FROM_ISR( pxQueue );

			prvCopyDataFromQueue( pxQueue, pvBuffer );
			pxQueue->uxMessagesWaiting = uxMessagesWaiting - ( usize ) 1;

			/* If the queue is locked the event list will not be modified.
			Instead update the lock count so the task that unlocks the queue
			will know that an ISR has removed data while the queue was
			locked. */
			if( cRxLock == queueUNLOCKED )
			{
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == false )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != false )
					{
						/* The task waiting has a higher priority than us so
						force a context switch. */
						if( pxHigherPriorityTaskWoken != NULL )
						{
							*pxHigherPriorityTaskWoken = true;
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* Increment the lock count so the task that unlocks the queue
				knows that data was removed while it was locked. */
				pxQueue->cRxLock = ( i8 ) ( cRxLock + 1 );
			}

			xReturn = true;
		}
		else
		{
			xReturn = false;
			// traceQUEUE_RECEIVE_FROM_ISR_FAILED( pxQueue );
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

isize xQueuePeekFromISR( Queue* xQueue,  void * const pvBuffer )
{
isize xReturn;
usize uxSavedInterruptStatus;
i8* pcOriginalReadPosition;
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );
	configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( usize ) 0U ) ) );
	configASSERT( pxQueue->uxItemSize != 0 ); /* Can't peek a semaphore. */

	/* RTOS ports that support interrupt nesting have the concept of a maximum
	system call (or maximum API call) interrupt priority.  Interrupts that are
	above the maximum system call priority are kept permanently enabled, even
	when the RTOS kernel is in a critical section, but cannot make any calls to
	FreeRTOS API functions.  If configASSERT() is defined in FreeRTOSConfig.h
	then portASSERT_IF_INTERRUPT_PRIORITY_INVALID() will result in an assertion
	failure if a FreeRTOS API function is called from an interrupt that has been
	assigned a priority above the configured maximum system call priority.
	Only FreeRTOS functions that end in FromISR can be called from interrupts
	that have been assigned a priority at or (logically) below the maximum
	system call	interrupt priority.  FreeRTOS maintains a separate interrupt
	safe API to ensure interrupt entry is as fast and as simple as possible.
	More information (albeit Cortex-M specific) is provided on the following
	link: http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	{
		/* Cannot block in an ISR, so check there is data available. */
		if( pxQueue->uxMessagesWaiting > ( usize ) 0 )
		{
			// traceQUEUE_PEEK_FROM_ISR( pxQueue );

			/* Remember the read position so it can be reset as nothing is
			actually being removed from the queue. */
			pcOriginalReadPosition = pxQueue->u.xQueue.pcReadFrom;
			prvCopyDataFromQueue( pxQueue, pvBuffer );
			pxQueue->u.xQueue.pcReadFrom = pcOriginalReadPosition;

			xReturn = true;
		}
		else
		{
			xReturn = false;
			// traceQUEUE_PEEK_FROM_ISR_FAILED( pxQueue );
		}
	}
	portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

	return xReturn;
}
/*-----------------------------------------------------------*/

usize uxQueueMessagesWaiting( const Queue* xQueue )
{
usize uxReturn;

	configASSERT( xQueue );

	vPortEnterCritical();
	{
		uxReturn = ( ( Queue * ) xQueue )->uxMessagesWaiting;
	}
	vPortExitCritical();

	return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

usize uxQueueSpacesAvailable( const Queue* xQueue )
{
usize uxReturn;
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );

	vPortEnterCritical();
	{
		uxReturn = pxQueue->uxLength - pxQueue->uxMessagesWaiting;
	}
	vPortExitCritical();

	return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

usize uxQueueMessagesWaitingFromISR( const Queue* xQueue )
{
usize uxReturn;
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );
	uxReturn = pxQueue->uxMessagesWaiting;

	return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

void vQueueDelete( Queue* xQueue )
{
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );
	// traceQUEUE_DELETE( pxQueue );

	#if ( configQUEUE_REGISTRY_SIZE > 0 )
	{
		vQueueUnregisterQueue( pxQueue );
	}
	#endif

	#if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
	{
		/* The queue can only have been allocated dynamically - free it
		again. */
		vPortFree( pxQueue );
	}
	#elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
	{
		/* The queue could have been allocated statically or dynamically, so
		check before attempting to free the memory. */
		if( pxQueue->ucStaticallyAllocated == ( u8 ) false )
		{
			vPortFree( pxQueue );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#else
	{
		/* The queue must have been statically allocated, so is not going to be
		deleted.  Avoid compiler warnings about the unused parameter. */
		( void ) pxQueue;
	}
	#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	usize uxQueueGetQueueNumber( Queue* xQueue )
	{
		return ( ( Queue * ) xQueue )->uxQueueNumber;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	void vQueueSetQueueNumber( Queue* xQueue, usize uxQueueNumber )
	{
		( ( Queue * ) xQueue )->uxQueueNumber = uxQueueNumber;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

	u8 ucQueueGetQueueType( Queue* xQueue )
	{
		return ( ( Queue * ) xQueue )->ucQueueType;
	}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if( configUSE_MUTEXES == 1 )

	static usize prvGetDisinheritPriorityAfterTimeout( const Queue * const pxQueue )
	{
	usize uxHighestPriorityOfWaitingTasks;

		/* If a task waiting for a mutex causes the mutex holder to inherit a
		priority, but the waiting task times out, then the holder should
		disinherit the priority - but only down to the highest priority of any
		other tasks that are waiting for the same mutex.  For this purpose,
		return the priority of the highest priority task that is waiting for the
		mutex. */
		if( listCURRENT_LIST_LENGTH( &( pxQueue->xTasksWaitingToReceive ) ) > 0U )
		{
			uxHighestPriorityOfWaitingTasks = ( usize ) configMAX_PRIORITIES - ( usize ) listGET_ITEM_VALUE_OF_HEAD_ENTRY( &( pxQueue->xTasksWaitingToReceive ) );
		}
		else
		{
			uxHighestPriorityOfWaitingTasks = tskIDLE_PRIORITY;
		}

		return uxHighestPriorityOfWaitingTasks;
	}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

static isize prvCopyDataToQueue( Queue * const pxQueue, const void *pvItemToQueue, const isize xPosition )
{
isize xReturn = false;
usize uxMessagesWaiting;

	/* This function is called from a critical section. */

	uxMessagesWaiting = pxQueue->uxMessagesWaiting;

	if( pxQueue->uxItemSize == ( usize ) 0 )
	{
		#if ( configUSE_MUTEXES == 1 )
		{
			if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
			{
				/* The mutex is no longer being held. */
				xReturn = xTaskPriorityDisinherit( pxQueue->u.xSemaphore.xMutexHolder );
				pxQueue->u.xSemaphore.xMutexHolder = NULL;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		#endif /* configUSE_MUTEXES */
	}
	else if( xPosition == queueSEND_TO_BACK )
	{
		( void ) memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, ( usize ) pxQueue->uxItemSize ); /*lint !e961 !e418 !e9087 MISRA exception as the casts are only redundant for some ports, plus previous logic ensures a null pointer can only be passed to memcpy() if the copy size is 0.  Cast to void required by function signature and safe as no alignment requirement and copy length specified in bytes. */
		pxQueue->pcWriteTo += pxQueue->uxItemSize; /*lint !e9016 Pointer arithmetic on char types ok, especially in this use case where it is the clearest way of conveying intent. */
		if( pxQueue->pcWriteTo >= pxQueue->u.xQueue.pcTail ) /*lint !e946 MISRA exception justified as comparison of pointers is the cleanest solution. */
		{
			pxQueue->pcWriteTo = pxQueue->pcHead;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	else
	{
		( void ) memcpy( ( void * ) pxQueue->u.xQueue.pcReadFrom, pvItemToQueue, ( usize ) pxQueue->uxItemSize ); /*lint !e961 !e9087 !e418 MISRA exception as the casts are only redundant for some ports.  Cast to void required by function signature and safe as no alignment requirement and copy length specified in bytes.  Assert checks null pointer only used when length is 0. */
		pxQueue->u.xQueue.pcReadFrom -= pxQueue->uxItemSize;
		if( pxQueue->u.xQueue.pcReadFrom < pxQueue->pcHead ) /*lint !e946 MISRA exception justified as comparison of pointers is the cleanest solution. */
		{
			pxQueue->u.xQueue.pcReadFrom = ( pxQueue->u.xQueue.pcTail - pxQueue->uxItemSize );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		if( xPosition == queueOVERWRITE )
		{
			if( uxMessagesWaiting > ( usize ) 0 )
			{
				/* An item is not being added but overwritten, so subtract
				one from the recorded number of items in the queue so when
				one is added again below the number of recorded items remains
				correct. */
				--uxMessagesWaiting;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

	pxQueue->uxMessagesWaiting = uxMessagesWaiting + ( usize ) 1;

	return xReturn;
}
/*-----------------------------------------------------------*/

static void prvCopyDataFromQueue( Queue * const pxQueue, void * const pvBuffer )
{
	if( pxQueue->uxItemSize != ( usize ) 0 )
	{
		pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize; /*lint !e9016 Pointer arithmetic on char types ok, especially in this use case where it is the clearest way of conveying intent. */
		if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail ) /*lint !e946 MISRA exception justified as use of the relational operator is the cleanest solutions. */
		{
			pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
		( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( usize ) pxQueue->uxItemSize ); /*lint !e961 !e418 !e9087 MISRA exception as the casts are only redundant for some ports.  Also previous logic ensures a null pointer can only be passed to memcpy() when the count is 0.  Cast to void required by function signature and safe as no alignment requirement and copy length specified in bytes. */
	}
}
/*-----------------------------------------------------------*/

static void prvUnlockQueue( Queue * const pxQueue )
{
	/* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED. */

	/* The lock counts contains the number of extra data items placed or
	removed from the queue while the queue was locked.  When a queue is
	locked items can be added or removed, but the event lists cannot be
	updated. */
	vPortEnterCritical();
	{
		i8 cTxLock = pxQueue->cTxLock;

		/* See if data was added to the queue while it was locked. */
		while( cTxLock > queueLOCKED_UNMODIFIED )
		{
			/* Data was posted while the queue was locked.  Are any tasks
			blocked waiting for data to become available? */
			#if ( configUSE_QUEUE_SETS == 1 )
			{
				if( pxQueue->pxQueueSetContainer != NULL )
				{
					if( prvNotifyQueueSetContainer( pxQueue, queueSEND_TO_BACK ) != false )
					{
						/* The queue is a member of a queue set, and posting to
						the queue set caused a higher priority task to unblock.
						A context switch is required. */
						vTaskMissedYield();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					/* Tasks that are removed from the event list will get
					added to the pending ready list as the scheduler is still
					suspended. */
					if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
					{
						if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
						{
							/* The task waiting has a higher priority so record that a
							context	switch is required. */
							vTaskMissedYield();
						}
						else
						{
							mtCOVERAGE_TEST_MARKER();
						}
					}
					else
					{
						break;
					}
				}
			}
			#else /* configUSE_QUEUE_SETS */
			{
				/* Tasks that are removed from the event list will get added to
				the pending ready list as the scheduler is still suspended. */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
				{
					if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
					{
						/* The task waiting has a higher priority so record that
						a context switch is required. */
						vTaskMissedYield();
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					break;
				}
			}
			#endif /* configUSE_QUEUE_SETS */

			--cTxLock;
		}

		pxQueue->cTxLock = queueUNLOCKED;
	}
	vPortExitCritical();

	/* Do the same for the Rx lock. */
	vPortEnterCritical();
	{
		i8 cRxLock = pxQueue->cRxLock;

		while( cRxLock > queueLOCKED_UNMODIFIED )
		{
			if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == false )
			{
				if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != false )
				{
					vTaskMissedYield();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}

				--cRxLock;
			}
			else
			{
				break;
			}
		}

		pxQueue->cRxLock = queueUNLOCKED;
	}
	vPortExitCritical();
}
/*-----------------------------------------------------------*/

static isize prvIsQueueEmpty( const Queue *pxQueue )
{
isize xReturn;

	vPortEnterCritical();
	{
		if( pxQueue->uxMessagesWaiting == ( usize )  0 )
		{
			xReturn = true;
		}
		else
		{
			xReturn = false;
		}
	}
	vPortExitCritical();

	return xReturn;
}
/*-----------------------------------------------------------*/

isize xQueueIsQueueEmptyFromISR( const Queue* xQueue )
{
isize xReturn;
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );
	if( pxQueue->uxMessagesWaiting == ( usize ) 0 )
	{
		xReturn = true;
	}
	else
	{
		xReturn = false;
	}

	return xReturn;
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */
/*-----------------------------------------------------------*/

static isize prvIsQueueFull( const Queue *pxQueue )
{
isize xReturn;

	vPortEnterCritical();
	{
		if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
		{
			xReturn = true;
		}
		else
		{
			xReturn = false;
		}
	}
	vPortExitCritical();

	return xReturn;
}
/*-----------------------------------------------------------*/

isize xQueueIsQueueFullFromISR( const Queue* xQueue )
{
isize xReturn;
Queue * const pxQueue = xQueue;

	configASSERT( pxQueue );
	if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
	{
		xReturn = true;
	}
	else
	{
		xReturn = false;
	}

	return xReturn;
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

	isize xQueueCRSend( Queue* xQueue, const void *pvItemToQueue, TickType xTicksToWait )
	{
	isize xReturn;
	Queue * const pxQueue = xQueue;

		/* If the queue is already full we may have to block.  A critical section
		is required to prevent an interrupt removing something from the queue
		between the check to see if the queue is full and blocking on the queue. */
		portDISABLE_INTERRUPTS();
		{
			if( prvIsQueueFull( pxQueue ) != false )
			{
				/* The queue is full - do we want to block or just leave without
				posting? */
				if( xTicksToWait > ( TickType ) 0 )
				{
					/* As this is called from a coroutine we cannot block directly, but
					return indicating that we need to block. */
					vCoRoutineAddToDelayedList( xTicksToWait, &( pxQueue->xTasksWaitingToSend ) );
					portENABLE_INTERRUPTS();
					return errQUEUE_BLOCKED;
				}
				else
				{
					portENABLE_INTERRUPTS();
					return errQUEUE_FULL;
				}
			}
		}
		portENABLE_INTERRUPTS();

		portDISABLE_INTERRUPTS();
		{
			if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
			{
				/* There is room in the queue, copy the data into the queue. */
				prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );
				xReturn = true;

				/* Were any co-routines waiting for data to become available? */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
				{
					/* In this instance the co-routine could be placed directly
					into the ready list as we are within a critical section.
					Instead the same pending ready list mechanism is used as if
					the event were caused from within an interrupt. */
					if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
					{
						/* The co-routine waiting has a higher priority so record
						that a yield might be appropriate. */
						xReturn = errQUEUE_YIELD;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				xReturn = errQUEUE_FULL;
			}
		}
		portENABLE_INTERRUPTS();

		return xReturn;
	}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

	isize xQueueCRReceive( Queue* xQueue, void *pvBuffer, TickType xTicksToWait )
	{
	isize xReturn;
	Queue * const pxQueue = xQueue;

		/* If the queue is already empty we may have to block.  A critical section
		is required to prevent an interrupt adding something to the queue
		between the check to see if the queue is empty and blocking on the queue. */
		portDISABLE_INTERRUPTS();
		{
			if( pxQueue->uxMessagesWaiting == ( usize ) 0 )
			{
				/* There are no messages in the queue, do we want to block or just
				leave with nothing? */
				if( xTicksToWait > ( TickType ) 0 )
				{
					/* As this is a co-routine we cannot block directly, but return
					indicating that we need to block. */
					vCoRoutineAddToDelayedList( xTicksToWait, &( pxQueue->xTasksWaitingToReceive ) );
					portENABLE_INTERRUPTS();
					return errQUEUE_BLOCKED;
				}
				else
				{
					portENABLE_INTERRUPTS();
					return errQUEUE_FULL;
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		portENABLE_INTERRUPTS();

		portDISABLE_INTERRUPTS();
		{
			if( pxQueue->uxMessagesWaiting > ( usize ) 0 )
			{
				/* Data is available from the queue. */
				pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize;
				if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail )
				{
					pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
				--( pxQueue->uxMessagesWaiting );
				( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

				xReturn = true;

				/* Were any co-routines waiting for space to become available? */
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == false )
				{
					/* In this instance the co-routine could be placed directly
					into the ready list as we are within a critical section.
					Instead the same pending ready list mechanism is used as if
					the event were caused from within an interrupt. */
					if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != false )
					{
						xReturn = errQUEUE_YIELD;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				xReturn = false;
			}
		}
		portENABLE_INTERRUPTS();

		return xReturn;
	}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

	isize xQueueCRSendFromISR( Queue* xQueue, const void *pvItemToQueue, isize xCoRoutinePreviouslyWoken )
	{
	Queue * const pxQueue = xQueue;

		/* Cannot block within an ISR so if there is no space on the queue then
		exit without doing anything. */
		if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
		{
			prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );

			/* We only want to wake one co-routine per ISR, so check that a
			co-routine has not already been woken. */
			if( xCoRoutinePreviouslyWoken == false )
			{
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == false )
				{
					if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != false )
					{
						return true;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return xCoRoutinePreviouslyWoken;
	}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

	isize xQueueCRReceiveFromISR( Queue* xQueue, void *pvBuffer, isize *pxCoRoutineWoken )
	{
	isize xReturn;
	Queue * const pxQueue = xQueue;

		/* We cannot block from an ISR, so check there is data available. If
		not then just leave without doing anything. */
		if( pxQueue->uxMessagesWaiting > ( usize ) 0 )
		{
			/* Copy the data from the queue. */
			pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize;
			if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail )
			{
				pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
			--( pxQueue->uxMessagesWaiting );
			( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

			if( ( *pxCoRoutineWoken ) == false )
			{
				if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == false )
				{
					if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != false )
					{
						*pxCoRoutineWoken = true;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			xReturn = true;
		}
		else
		{
			xReturn = false;
		}

		return xReturn;
	}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

	void vQueueAddToRegistry( Queue* xQueue, const char *pcQueueName ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	{
	usize ux;

		/* See if there is an empty space in the registry.  A NULL name denotes
		a free slot. */
		for( ux = ( usize ) 0U; ux < ( usize ) configQUEUE_REGISTRY_SIZE; ux++ )
		{
			if( xQueueRegistry[ ux ].pcQueueName == NULL )
			{
				/* Store the information on this queue. */
				xQueueRegistry[ ux ].pcQueueName = pcQueueName;
				xQueueRegistry[ ux ].xHandle = xQueue;

				traceQUEUE_REGISTRY_ADD( xQueue, pcQueueName );
				break;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
	}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

	const char *pcQueueGetName( Queue* xQueue ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	{
	usize ux;
	const char *pcReturn = NULL; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

		/* Note there is nothing here to protect against another task adding or
		removing entries from the registry while it is being searched. */
		for( ux = ( usize ) 0U; ux < ( usize ) configQUEUE_REGISTRY_SIZE; ux++ )
		{
			if( xQueueRegistry[ ux ].xHandle == xQueue )
			{
				pcReturn = xQueueRegistry[ ux ].pcQueueName;
				break;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

		return pcReturn;
	} /*lint !e818 xQueue cannot be a pointer to const because it is a typedef. */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

	void vQueueUnregisterQueue( Queue* xQueue )
	{
	usize ux;

		/* See if the handle of the queue being unregistered in actually in the
		registry. */
		for( ux = ( usize ) 0U; ux < ( usize ) configQUEUE_REGISTRY_SIZE; ux++ )
		{
			if( xQueueRegistry[ ux ].xHandle == xQueue )
			{
				/* Set the name to NULL to show that this slot if free again. */
				xQueueRegistry[ ux ].pcQueueName = NULL;

				/* Set the handle to NULL to ensure the same queue handle cannot
				appear in the registry twice if it is added, removed, then
				added again. */
				xQueueRegistry[ ux ].xHandle = ( Queue* ) 0;
				break;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}

	} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configUSE_TIMERS == 1 )

	void vQueueWaitForMessageRestricted( Queue* xQueue, TickType xTicksToWait, const isize xWaitIndefinitely )
	{
	Queue * const pxQueue = xQueue;

		/* This function should not be called by application code hence the
		'Restricted' in its name.  It is not part of the public API.  It is
		designed for use by kernel code, and has special calling requirements.
		It can result in vListInsert() being called on a list that can only
		possibly ever have one item in it, so the list will be fast, but even
		so it should be called with the scheduler locked and not from a critical
		section. */

		/* Only do anything if there are no messages in the queue.  This function
		will not actually cause the task to block, just place it on a blocked
		list.  It will not block until the scheduler is unlocked - at which
		time a yield will be performed.  If an item is added to the queue while
		the queue is locked, and the calling task blocks on the queue, then the
		calling task will be immediately unblocked when the queue is unlocked. */
		prvLockQueue( pxQueue );
		if( pxQueue->uxMessagesWaiting == ( usize ) 0U )
		{
			/* There is nothing in the queue, block for the specified period. */
			vTaskPlaceOnEventListRestricted( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait, xWaitIndefinitely );
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
		prvUnlockQueue( pxQueue );
	}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

#if( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

	QueueSetHandle_t xQueueCreateSet( const usize uxEventQueueLength )
	{
	QueueSetHandle_t pxQueue;

		pxQueue = xQueueGenericCreate( uxEventQueueLength, ( usize ) sizeof( Queue * ), queueQUEUEYPE_SET );

		return pxQueue;
	}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	isize xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet )
	{
	isize xReturn;

		vPortEnterCritical();
		{
			if( ( ( Queue * ) xQueueOrSemaphore )->pxQueueSetContainer != NULL )
			{
				/* Cannot add a queue/semaphore to more than one queue set. */
				xReturn = false;
			}
			else if( ( ( Queue * ) xQueueOrSemaphore )->uxMessagesWaiting != ( usize ) 0 )
			{
				/* Cannot add a queue/semaphore to a queue set if there are already
				items in the queue/semaphore. */
				xReturn = false;
			}
			else
			{
				( ( Queue * ) xQueueOrSemaphore )->pxQueueSetContainer = xQueueSet;
				xReturn = true;
			}
		}
		vPortExitCritical();

		return xReturn;
	}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	isize xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet )
	{
	isize xReturn;
	Queue * const pxQueueOrSemaphore = ( Queue * ) xQueueOrSemaphore;

		if( pxQueueOrSemaphore->pxQueueSetContainer != xQueueSet )
		{
			/* The queue was not a member of the set. */
			xReturn = false;
		}
		else if( pxQueueOrSemaphore->uxMessagesWaiting != ( usize ) 0 )
		{
			/* It is dangerous to remove a queue from a set when the queue is
			not empty because the queue set will still hold pending events for
			the queue. */
			xReturn = false;
		}
		else
		{
			vPortEnterCritical();
			{
				/* The queue is no longer contained in the set. */
				pxQueueOrSemaphore->pxQueueSetContainer = NULL;
			}
			vPortExitCritical();
			xReturn = true;
		}

		return xReturn;
	} /*lint !e818 xQueueSet could not be declared as pointing to const as it is a typedef. */

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet, TickType const xTicksToWait )
	{
	QueueSetMemberHandle_t xReturn = NULL;

		( void ) xQueueReceive( ( Queue* ) xQueueSet, &xReturn, xTicksToWait ); /*lint !e961 Casting from one typedef to another is not redundant. */
		return xReturn;
	}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet )
	{
	QueueSetMemberHandle_t xReturn = NULL;

		( void ) xQueueReceiveFromISR( ( Queue* ) xQueueSet, &xReturn, NULL ); /*lint !e961 Casting from one typedef to another is not redundant. */
		return xReturn;
	}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	static isize prvNotifyQueueSetContainer( const Queue * const pxQueue, const isize xCopyPosition )
	{
	Queue *pxQueueSetContainer = pxQueue->pxQueueSetContainer;
	isize xReturn = false;

		/* This function must be called form a critical section. */

		configASSERT( pxQueueSetContainer );
		configASSERT( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength );

		if( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength )
		{
			const i8 cTxLock = pxQueueSetContainer->cTxLock;

			traceQUEUE_SEND( pxQueueSetContainer );

			/* The data copied is the handle of the queue that contains data. */
			xReturn = prvCopyDataToQueue( pxQueueSetContainer, &pxQueue, xCopyPosition );

			if( cTxLock == queueUNLOCKED )
			{
				if( listLIST_IS_EMPTY( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) == false )
				{
					if( xTaskRemoveFromEventList( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) != false )
					{
						/* The task waiting has a higher priority. */
						xReturn = true;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				pxQueueSetContainer->cTxLock = ( i8 ) ( cTxLock + 1 );
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		return xReturn;
	}

#endif /* configUSE_QUEUE_SETS */












