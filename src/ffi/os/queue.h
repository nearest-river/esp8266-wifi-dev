#ifndef QUEUE_H
#define QUEUE_H
#include "../prelude.h"
#include "list.h"
#include "task.h"
#include "config.h"
#include "port.h"




/* Constants used with the cRxLock and cTxLock structure members. */
#define queueUNLOCKED	((i8)-1)
#define queueLOCKED_UNMODIFIED ((i8)0)
#define queueQUEUE_TYPE_MUTEX ((u8)1U)
#define queueSEND_TO_BACK ((u32)0)
#define queueOVERWRITE ((i32)2)
#define errQUEUE_FULL ((i32)0)
#define errQUEUE_EMPTY ((i32)0)
#define queueQUEUE_TYPE_RECURSIVE_MUTEX ((u8)4U)

/* When the Queue_t structure is used to represent a base queue its pcHead and
pcTail members are used as pointers into the queue storage area.  When the
Queue_t structure is used to represent a mutex pcHead and pcTail pointers are
not necessary, and the pcHead pointer is set to NULL to indicate that the
structure instead holds a pointer to the mutex holder (if any).  Map alternative
names to the pcHead and structure member to ensure the readability of the code
is maintained.  The QueuePointers_t and SemaphoreData_t types are used to form
a union as their usage is mutually exclusive dependent on what the queue is
being used for. */
#define uxQueueType	pcHead
#define queueQUEUE_IS_MUTEX	NULL



typedef struct {
  // Points to the byte at the end of the queue storage area.  Once more byte is allocated than necessary to store the queue items, this is used as a marker.
	i8* pcTail;
  // Points to the last place that a queued item was read from when the structure is used as a queue.
	i8* pcReadFrom;
} QueuePointers;

typedef struct {
  // The handle of the task that holds the mutex.
	TaskHandle xMutexHolder;
  // Maintains a count of the number of times a recursive mutex has been recursively 'taken' when the structure is used as a mutex.
	usize uxRecursiveCallCount;
} SemaphoreData;


// Semaphores do not actually store or copy data, so have an item size of zero.
#define queueSEMAPHORE_QUEUE_ITEM_LENGTH ((usize)0U)
#define queueMUTEX_GIVE_BLOCK_TIME ((TickType)0U)


#if !configUSE_PREEMPTION
	/* If the cooperative scheduler is being used then a yield should not be
	performed just because a higher priority task has been woken. */
	#define queueYIELD_IF_USING_PREEMPTION()
#else
	#define queueYIELD_IF_USING_PREEMPTION() portYIELD()
#endif





typedef struct Queue {
  // Points to the beginning of the queue storage area.
	i8* pcHead;
  // Points to the free next place in the storage area
	i8* pcWriteTo;

	union {
    // Data required exclusively when this structure is used as a queue.
		QueuePointers xQueue;
    // Data required exclusively when this structure is used as a semaphore. */
		SemaphoreData xSemaphore;
  } u;
  // List of tasks that are blocked waiting to post onto this queue.  Stored in priority order.
	LinkedList xTasksWaitingToSend;
  // List of tasks that are blocked waiting to read from this queue.  Stored in priority order.
	LinkedList xTasksWaitingToReceive;

  // The number of items currently in the queue.
	volatile usize uxMessagesWaiting;
  // The length of the queue defined as the number of items it will hold, not the number of bytes.
	usize uxLength;
  // The size of each items that the queue will hold.
	usize uxItemSize;

  // Stores the number of items received from the queue (removed from the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked.
	volatile i8 cRxLock;
  // Stores the number of items transmitted to the queue (added to the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked.
	volatile i8 cTxLock;

	#if configSUPPORT_STATIC_ALLOCATION && configSUPPORT_DYNAMIC_ALLOCATION
    // Set to pdTRUE if the memory used by the queue was statically allocated to ensure no attempt is made to free the memory.
		u8 ucStaticallyAllocated;
	#endif

	#if configUSE_QUEUE_SETS
		struct Queue* pxQueueSetContainer;
	#endif

	#if configUSE_TRACE_FACILITY
		usize uxQueueNumber;
		u8 ucQueueType;
	#endif

} Queue;




Queue* xQueueCreateMutex(const u8 queue_type);
Queue* xQueueGenericCreate(const usize len,const usize item_size,const u8 queue_type);
isize xQueueGenericReset(Queue* xQueue,isize xNewQueue);
isize xQueueGenericSend(Queue* xQueue,const void* const pvItemToQueue,TickType xTicksToWait,const isize xCopyPosition);
isize xQueueReceive(Queue* xQueue,void* const pvBuffer,TickType xTicksToWait);
isize xQueueSemaphoreTake(Queue* xQueue,TickType xTicksToWait);









#endif // !QUEUE_H
