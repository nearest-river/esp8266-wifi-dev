#ifndef OS_LIST_H
#define OS_LIST_H
#include "config.h"
#include "../prelude.h"



#define configLIST_VOLATILE


struct LinkedList;
typedef struct LinkedListNode {
  // The value being listed.  In most cases this is used to sort the list in descending order.
	configLIST_VOLATILE u32 xItemValue;
	struct LinkedListNode* configLIST_VOLATILE pxNext;
	struct LinkedListNode* configLIST_VOLATILE pxPrevious;
  // Pointer to the object (normally a TCB) that contains the list item.  There is therefore a two way link between the object containing the list item and the list item itself.
	void* pvOwner;
  // Pointer to the list in which this list item is placed (if any).
	struct LinkedList* configLIST_VOLATILE pxContainer;
} LinkedListNode;



typedef struct MiniLinkedListNode {
	configLIST_VOLATILE u32 xItemValue;
	LinkedListNode* configLIST_VOLATILE pxNext;
	LinkedListNode* configLIST_VOLATILE pxPrevious;
} MiniLinkedListNode;



typedef struct LinkedList {
	volatile usize uxNumberOfItems;
  // Used to walk through the list.  Points to the last item returned by a call to listGET_OWNER_OF_NEXT_ENTRY ().
	LinkedListNode* configLIST_VOLATILE pxIndex;
  // List item that contains the maximum possible item value meaning it is always at the end of the list and is therefore used as a marker.
	MiniLinkedListNode xListEnd;
} LinkedList;





/*
 * Access macro to set the owner of a list item.  The owner of a list item
 * is the object (usually a TCB) that contains the list item.
 *
 * \page listSET_LIST_ITEM_OWNER listSET_LIST_ITEM_OWNER
 * \ingroup LinkedList
 */
#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )		( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )

/*
 * Access macro to get the owner of a list item.  The owner of a list item
 * is the object (usually a TCB) that contains the list item.
 *
 * \page listSET_LIST_ITEM_OWNER listSET_LIST_ITEM_OWNER
 * \ingroup LinkedList
 */
#define listGET_LIST_ITEM_OWNER( pxListItem )	( ( pxListItem )->pvOwner )

/*
 * Access macro to set the value of the list item.  In most cases the value is
 * used to sort the list in descending order.
 *
 * \page listSET_LIST_ITEM_VALUE listSET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )	( ( pxListItem )->xItemValue = ( xValue ) )

/*
 * Access macro to retrieve the value of the list item.  The value can
 * represent anything - for example the priority of a task, or the time at
 * which a task should be unblocked.
 *
 * \page listGET_LIST_ITEM_VALUE listGET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listGET_LIST_ITEM_VALUE( pxListItem )	( ( pxListItem )->xItemValue )

/*
 * Access macro to retrieve the value of the list item at the head of a given
 * list.
 *
 * \page listGET_LIST_ITEM_VALUE listGET_LIST_ITEM_VALUE
 * \ingroup LinkedList
 */
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext->xItemValue )

/*
 * Return the list item at the head of the list.
 *
 * \page listGET_HEAD_ENTRY listGET_HEAD_ENTRY
 * \ingroup LinkedList
 */
#define listGET_HEAD_ENTRY( pxList )	( ( ( pxList )->xListEnd ).pxNext )

/*
 * Return the list item at the head of the list.
 *
 * \page listGET_NEXT listGET_NEXT
 * \ingroup LinkedList
 */
#define listGET_NEXT( pxListItem )	( ( pxListItem )->pxNext )

/*
 * Return the list item that marks the end of the list
 *
 * \page listGET_END_MARKER listGET_END_MARKER
 * \ingroup LinkedList
 */
#define listGET_END_MARKER( pxList )	( ( LinkedListNode const * ) ( &( ( pxList )->xListEnd ) ) )

/*
 * Access macro to determine if a list contains any items.  The macro will
 * only have the value true if the list is empty.
 *
 * \page listLIST_IS_EMPTY listLIST_IS_EMPTY
 * \ingroup LinkedList
 */
#define listLIST_IS_EMPTY( pxList )	( ( ( pxList )->uxNumberOfItems == ( usize ) 0 ) ? true : false )

/*
 * Access macro to return the number of items in the list.
 */
#define listCURRENT_LIST_LENGTH( pxList )	( ( pxList )->uxNumberOfItems )

/*
 * Access function to obtain the owner of the next entry in a list.
 *
 * The list member pxIndex is used to walk through a list.  Calling
 * listGET_OWNER_OF_NEXT_ENTRY increments pxIndex to the next item in the list
 * and returns that entry's pxOwner parameter.  Using multiple calls to this
 * function it is therefore possible to move through every item contained in
 * a list.
 *
 * The pxOwner parameter of a list item is a pointer to the object that owns
 * the list item.  In the scheduler this is normally a task control block.
 * The pxOwner parameter effectively creates a two way link between the list
 * item and its owner.
 *
 * @param pxTCB pxTCB is set to the address of the owner of the next list item.
 * @param pxList The list from which the next item owner is to be returned.
 *
 * \page listGET_OWNER_OF_NEXT_ENTRY listGET_OWNER_OF_NEXT_ENTRY
 * \ingroup LinkedList
 */
#define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )										\
{																							\
LinkedList * const pxConstList = ( pxList );													\
	/* Increment the index to the next item and return the item, ensuring */				\
	/* we don't return the marker used at the end of the list.  */							\
	( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;							\
	if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) )	\
	{																						\
		( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;						\
	}																						\
	( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;											\
}


/*
 * Access function to obtain the owner of the first entry in a list.  Lists
 * are normally sorted in ascending item value order.
 *
 * This function returns the pxOwner member of the first item in the list.
 * The pxOwner parameter of a list item is a pointer to the object that owns
 * the list item.  In the scheduler this is normally a task control block.
 * The pxOwner parameter effectively creates a two way link between the list
 * item and its owner.
 *
 * @param pxList The list from which the owner of the head item is to be
 * returned.
 *
 * \page listGET_OWNER_OF_HEAD_ENTRY listGET_OWNER_OF_HEAD_ENTRY
 * \ingroup LinkedList
 */
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )  ( (&( ( pxList )->xListEnd ))->pxNext->pvOwner )

/*
 * Check to see if a list item is within a list.  The list item maintains a
 * "container" pointer that points to the list it is in.  All this macro does
 * is check to see if the container and the list match.
 *
 * @param pxList The list we want to know if the list item is within.
 * @param pxListItem The list item we want to know if is in the list.
 * @return true if the list item is in the list, otherwise false.
 */
#define listIS_CONTAINED_WITHIN( pxList, pxListItem ) ( ( ( pxListItem )->pxContainer == ( pxList ) ) ? ( true ) : ( false ) )

/*
 * Return the list a list item is contained within (referenced from).
 *
 * @param pxListItem The list item being queried.
 * @return A pointer to the LinkedList object that references the pxListItem
 */
#define listLIST_ITEM_CONTAINER( pxListItem ) ( ( pxListItem )->pxContainer )

/*
 * This provides a crude means of knowing if a list has been initialised, as
 * pxList->xListEnd.xItemValue is set to portMAX_DELAY by the vListInitialise()
 * function.
 */
#define listLIST_IS_INITIALISED( pxList ) ( ( pxList )->xListEnd.xItemValue == portMAX_DELAY )





/*
 * Must be called before a list is used!  This initialises all the members
 * of the list structure and inserts the xListEnd item into the list as a
 * marker to the back of the list.
 *
 * @param pxList Pointer to the list being initialised.
 *
 * \page vListInitialise vListInitialise
 * \ingroup LinkedList
 */
void vListInitialise( LinkedList * const pxList ) PRIVILEGED_FUNCTION;

/*
 * Must be called before a list item is used.  This sets the list container to
 * null so the item does not think that it is already contained in a list.
 *
 * @param pxItem Pointer to the list item being initialised.
 *
 * \page vListInitialiseItem vListInitialiseItem
 * \ingroup LinkedList
 */
void vListInitialiseItem( LinkedListNode * const pxItem ) PRIVILEGED_FUNCTION;

/*
 * Insert a list item into a list.  The item will be inserted into the list in
 * a position determined by its item value (descending item value order).
 *
 * @param pxList The list into which the item is to be inserted.
 *
 * @param pxNewListItem The item that is to be placed in the list.
 *
 * \page vListInsert vListInsert
 * \ingroup LinkedList
 */
void vListInsert( LinkedList * const pxList, LinkedListNode * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * Insert a list item into a list.  The item will be inserted in a position
 * such that it will be the last item within the list returned by multiple
 * calls to listGET_OWNER_OF_NEXT_ENTRY.
 *
 * The list member pxIndex is used to walk through a list.  Calling
 * listGET_OWNER_OF_NEXT_ENTRY increments pxIndex to the next item in the list.
 * Placing an item in a list using vListInsertEnd effectively places the item
 * in the list position pointed to by pxIndex.  This means that every other
 * item within the list will be returned by listGET_OWNER_OF_NEXT_ENTRY before
 * the pxIndex parameter again points to the item being inserted.
 *
 * @param pxList The list into which the item is to be inserted.
 *
 * @param pxNewListItem The list item to be inserted into the list.
 *
 * \page vListInsertEnd vListInsertEnd
 * \ingroup LinkedList
 */
void vListInsertEnd( LinkedList * const pxList, LinkedListNode * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * Remove an item from a list.  The list item has a pointer to the list that
 * it is in, so only the list item need be passed into the function.
 *
 * @param uxListRemove The item to be removed.  The item will remove itself from
 * the list pointed to by it's pxContainer parameter.
 *
 * @return The number of items that remain in the list after the list item has
 * been removed.
 *
 * \page uxListRemove uxListRemove
 * \ingroup LinkedList
 */
usize uxListRemove( LinkedListNode * const pxItemToRemove ) PRIVILEGED_FUNCTION;




#endif // !OS_LIST_H
