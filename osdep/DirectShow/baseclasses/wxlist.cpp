//------------------------------------------------------------------------------
// File: WXList.cpp
//
// Desc: DirectShow base classes - implements a non-MFC based generic list
//       template class.
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

/* A generic list of pointers to objects.
   Objectives: avoid using MFC libraries in ndm kernel mode and
   provide a really useful list type.

   The class is thread safe in that separate threads may add and
   delete items in the list concurrently although the application
   must ensure that constructor and destructor access is suitably
   synchronised.

   The list name must not conflict with MFC classes as an
   application may use both

   The nodes form a doubly linked, NULL terminated chain with an anchor
   block (the list object per se) holding pointers to the first and last
   nodes and a count of the nodes.
   There is a node cache to reduce the allocation and freeing overhead.
   It optionally (determined at construction time) has an Event which is
   set whenever the list becomes non-empty and reset whenever it becomes
   empty.
   It optionally (determined at construction time) has a Critical Section
   which is entered during the important part of each operation.  (About
   all you can do outside it is some parameter checking).

   The node cache is a repository of nodes that are NOT in the list to speed
   up storage allocation.  Each list has its own cache to reduce locking and
   serialising.  The list accesses are serialised anyway for a given list - a
   common cache would mean that we would have to separately serialise access
   of all lists within the cache.  Because the cache only stores nodes that are
   not in the list, releasing the cache does not release any list nodes.  This
   means that list nodes can be copied or rechained from one list to another
   without danger of creating a dangling reference if the original cache goes
   away.

   Questionable design decisions:
   1. Retaining the warts for compatibility
   2. Keeping an element count -i.e. counting whenever we do anything
      instead of only when we want the count.
   3. Making the chain pointers NULL terminated.  If the list object
      itself looks just like a node and the list is kept as a ring then
      it reduces the number of special cases.  All inserts look the same.
*/

#include <streams.h>

/* set cursor to the position of each element of list in turn  */
#define INTERNALTRAVERSELIST(list, cursor) \
    for (cursor = (list).GetHeadPositionI(); cursor != NULL; cursor = (list).Next(cursor))

/* set cursor to the position of each element of list in turn
   in reverse order
*/
#define INTERNALREVERSETRAVERSELIST(list, cursor) \
    for (cursor = (list).GetTailPositionI(); cursor != NULL; cursor = (list).Prev(cursor))

/* Constructor calls a separate initialisation function that
   creates a node cache, optionally creates a lock object
   and optionally creates a signaling object.

   By default we create a locking object, a DEFAULTCACHE sized
   cache but no event object so the list cannot be used in calls
   to WaitForSingleObject
*/
CBaseList::CBaseList(__in_opt LPCTSTR pName, // Descriptive list name
                     INT iItems)
    : // Node cache size
#ifdef DEBUG
    CBaseObject(pName)
    ,
#endif
    m_pFirst(NULL)
    , m_pLast(NULL)
    , m_Count(0)
    , m_Cache(iItems)
{
} // constructor

CBaseList::CBaseList(__in_opt LPCTSTR pName)
    : // Descriptive list name
#ifdef DEBUG
    CBaseObject(pName)
    ,
#endif
    m_pFirst(NULL)
    , m_pLast(NULL)
    , m_Count(0)
    , m_Cache(DEFAULTCACHE)
{
} // constructor

#ifdef UNICODE
CBaseList::CBaseList(__in_opt LPCSTR pName, // Descriptive list name
                     INT iItems)
    : // Node cache size
#ifdef DEBUG
    CBaseObject(pName)
    ,
#endif
    m_pFirst(NULL)
    , m_pLast(NULL)
    , m_Count(0)
    , m_Cache(iItems)
{
} // constructor

CBaseList::CBaseList(__in_opt LPCSTR pName)
    : // Descriptive list name
#ifdef DEBUG
    CBaseObject(pName)
    ,
#endif
    m_pFirst(NULL)
    , m_pLast(NULL)
    , m_Count(0)
    , m_Cache(DEFAULTCACHE)
{
} // constructor

#endif

/* The destructor enumerates all the node objects in the list and
   in the cache deleting each in turn. We do not do any processing
   on the objects that the list holds (i.e. points to) so if they
   represent interfaces for example the creator of the list should
   ensure that each of them is released before deleting us
*/
CBaseList::~CBaseList()
{
    /* Delete all our list nodes */

    RemoveAll();

} // destructor

/* Remove all the nodes from the list but don't do anything
   with the objects that each node looks after (this is the
   responsibility of the creator).
   Aa a last act we reset the signalling event
   (if available) to indicate to clients that the list
   does not have any entries in it.
*/
void CBaseList::RemoveAll()
{
    /* Free up all the CNode objects NOTE we don't bother putting the
       deleted nodes into the cache as this method is only really called
       in serious times of change such as when we are being deleted at
       which point the cache will be deleted anway */

    CNode *pn = m_pFirst;
    while (pn)
    {
        CNode *op = pn;
        pn = pn->Next();
        delete op;
    }

    /* Reset the object count and the list pointers */

    m_Count = 0;
    m_pFirst = m_pLast = NULL;

} // RemoveAll

/* Return a position enumerator for the entire list.
   A position enumerator is a pointer to a node object cast to a
   transparent type so all we do is return the head/tail node
   pointer in the list.
   WARNING because the position is a pointer to a node there is
   an implicit assumption for users a the list class that after
   deleting an object from the list that any other position
   enumerators that you have may be invalid (since the node
   may be gone).
*/
__out_opt POSITION CBaseList::GetHeadPositionI() const
{
    return (POSITION)m_pFirst;
} // GetHeadPosition

__out_opt POSITION CBaseList::GetTailPositionI() const
{
    return (POSITION)m_pLast;
} // GetTailPosition

/* Get the number of objects in the list,
   Get the lock before accessing the count.
   Locking may not be entirely necessary but it has the side effect
   of making sure that all operations are complete before we get it.
   So for example if a list is being added to this list then that
   will have completed in full before we continue rather than seeing
   an intermediate albeit valid state
*/
int CBaseList::GetCountI() const
{
    return m_Count;
} // GetCount

/* Return the object at rp, update rp to the next object from
   the list or NULL if you have moved over the last object.
   You may still call this function once we return NULL but
   we will continue to return a NULL position value
*/
__out void *CBaseList::GetNextI(__inout POSITION &rp) const
{
    /* have we reached the end of the list */

    if (rp == NULL)
    {
        return NULL;
    }

    /* Lock the object before continuing */

    void *pObject;

    /* Copy the original position then step on */

    CNode *pn = (CNode *)rp;
    ASSERT(pn != NULL);
    rp = (POSITION)pn->Next();

    /* Get the object at the original position from the list */

    pObject = pn->GetData();
    // ASSERT(pObject != NULL);    // NULL pointers in the list are allowed.
    return pObject;
} // GetNext

/* Return the object at p.
   Asking for the object at NULL ASSERTs then returns NULL
   The object is NOT locked.  The list is not being changed
   in any way.  If another thread is busy deleting the object
   then locking would only result in a change from one bad
   behaviour to another.
*/
__out_opt void *CBaseList::GetI(__in_opt POSITION p) const
{
    if (p == NULL)
    {
        return NULL;
    }

    CNode *pn = (CNode *)p;
    void *pObject = pn->GetData();
    // ASSERT(pObject != NULL);    // NULL pointers in the list are allowed.
    return pObject;
} // Get

__out void *CBaseList::GetValidI(__in POSITION p) const
{
    CNode *pn = (CNode *)p;
    void *pObject = pn->GetData();
    // ASSERT(pObject != NULL);    // NULL pointers in the list are allowed.
    return pObject;
} // Get

/* Return the first position in the list which holds the given pointer.
   Return NULL if it's not found.
*/
__out_opt POSITION CBaseList::FindI(__in void *pObj) const
{
    POSITION pn;
    INTERNALTRAVERSELIST(*this, pn)
    {
        if (GetI(pn) == pObj)
        {
            return pn;
        }
    }
    return NULL;
} // Find

/* Remove the first node in the list (deletes the pointer to its object
   from the list, does not free the object itself).
   Return the pointer to its object or NULL if empty
*/
__out_opt void *CBaseList::RemoveHeadI()
{
    /* All we do is get the head position and ask for that to be deleted.
       We could special case this since some of the code path checking
       in Remove() is redundant as we know there is no previous
       node for example but it seems to gain little over the
       added complexity
    */

    return RemoveI((POSITION)m_pFirst);
} // RemoveHead

/* Remove the last node in the list (deletes the pointer to its object
   from the list, does not free the object itself).
   Return the pointer to its object or NULL if empty
*/
__out_opt void *CBaseList::RemoveTailI()
{
    /* All we do is get the tail position and ask for that to be deleted.
       We could special case this since some of the code path checking
       in Remove() is redundant as we know there is no previous
       node for example but it seems to gain little over the
       added complexity
    */

    return RemoveI((POSITION)m_pLast);
} // RemoveTail

/* Remove the pointer to the object in this position from the list.
   Deal with all the chain pointers
   Return a pointer to the object removed from the list.
   The node object that is freed as a result
   of this operation is added to the node cache where
   it can be used again.
   Remove(NULL) is a harmless no-op - but probably is a wart.
*/
__out_opt void *CBaseList::RemoveI(__in_opt POSITION pos)
{
    /* Lock the critical section before continuing */

    // ASSERT (pos!=NULL);     // Removing NULL is to be harmless!
    if (pos == NULL)
        return NULL;

    CNode *pCurrent = (CNode *)pos;
    ASSERT(pCurrent != NULL);

    /* Update the previous node */

    CNode *pNode = pCurrent->Prev();
    if (pNode == NULL)
    {
        m_pFirst = pCurrent->Next();
    }
    else
    {
        pNode->SetNext(pCurrent->Next());
    }

    /* Update the following node */

    pNode = pCurrent->Next();
    if (pNode == NULL)
    {
        m_pLast = pCurrent->Prev();
    }
    else
    {
        pNode->SetPrev(pCurrent->Prev());
    }

    /* Get the object this node was looking after */

    void *pObject = pCurrent->GetData();

    // ASSERT(pObject != NULL);    // NULL pointers in the list are allowed.

    /* Try and add the node object to the cache -
       a NULL return code from the cache means we ran out of room.
       The cache size is fixed by a constructor argument when the
       list is created and defaults to DEFAULTCACHE.
       This means that the cache will have room for this many
       node objects. So if you have a list of media samples
       and you know there will never be more than five active at
       any given time of them for example then override the default
       constructor
    */

    m_Cache.AddToCache(pCurrent);

    /* If the list is empty then reset the list event */

    --m_Count;
    ASSERT(m_Count >= 0);
    return pObject;
} // Remove

/* Add this object to the tail end of our list
   Return the new tail position.
*/

__out_opt POSITION CBaseList::AddTailI(__in void *pObject)
{
    /* Lock the critical section before continuing */

    CNode *pNode;
    // ASSERT(pObject);   // NULL pointers in the list are allowed.

    /* If there is a node objects in the cache then use
       that otherwise we will have to create a new one */

    pNode = (CNode *)m_Cache.RemoveFromCache();
    if (pNode == NULL)
    {
        pNode = new CNode;
    }

    /* Check we have a valid object */

    if (pNode == NULL)
    {
        return NULL;
    }

    /* Initialise all the CNode object
       just in case it came from the cache
    */

    pNode->SetData(pObject);
    pNode->SetNext(NULL);
    pNode->SetPrev(m_pLast);

    if (m_pLast == NULL)
    {
        m_pFirst = pNode;
    }
    else
    {
        m_pLast->SetNext(pNode);
    }

    /* Set the new last node pointer and also increment the number
       of list entries, the critical section is unlocked when we
       exit the function
    */

    m_pLast = pNode;
    ++m_Count;

    return (POSITION)pNode;
} // AddTail(object)

/* Add this object to the head end of our list
   Return the new head position.
*/
__out_opt POSITION CBaseList::AddHeadI(__in void *pObject)
{
    CNode *pNode;
    // ASSERT(pObject);  // NULL pointers in the list are allowed.

    /* If there is a node objects in the cache then use
       that otherwise we will have to create a new one */

    pNode = (CNode *)m_Cache.RemoveFromCache();
    if (pNode == NULL)
    {
        pNode = new CNode;
    }

    /* Check we have a valid object */

    if (pNode == NULL)
    {
        return NULL;
    }

    /* Initialise all the CNode object
       just in case it came from the cache
    */

    pNode->SetData(pObject);

    /* chain it in (set four pointers) */
    pNode->SetPrev(NULL);
    pNode->SetNext(m_pFirst);

    if (m_pFirst == NULL)
    {
        m_pLast = pNode;
    }
    else
    {
        m_pFirst->SetPrev(pNode);
    }
    m_pFirst = pNode;

    ++m_Count;

    return (POSITION)pNode;
} // AddHead(object)

/* Add all the elements in *pList to the tail of this list.
   Return TRUE if it all worked, FALSE if it didn't.
   If it fails some elements may have been added.
*/
BOOL CBaseList::AddTail(__in CBaseList *pList)
{
    /* lock the object before starting then enumerate
       each entry in the source list and add them one by one to
       our list (while still holding the object lock)
       Lock the other list too.
    */
    POSITION pos = pList->GetHeadPositionI();

    while (pos)
    {
        if (NULL == AddTailI(pList->GetNextI(pos)))
        {
            return FALSE;
        }
    }
    return TRUE;
} // AddTail(list)

/* Add all the elements in *pList to the head of this list.
   Return TRUE if it all worked, FALSE if it didn't.
   If it fails some elements may have been added.
*/
BOOL CBaseList::AddHead(__in CBaseList *pList)
{
    /* lock the object before starting then enumerate
       each entry in the source list and add them one by one to
       our list (while still holding the object lock)
       Lock the other list too.

       To avoid reversing the list, traverse it backwards.
    */

    POSITION pos;

    INTERNALREVERSETRAVERSELIST(*pList, pos)
    {
        if (NULL == AddHeadI(pList->GetValidI(pos)))
        {
            return FALSE;
        }
    }
    return TRUE;
} // AddHead(list)

/* Add the object after position p
   p is still valid after the operation.
   AddAfter(NULL,x) adds x to the start - same as AddHead
   Return the position of the new object, NULL if it failed
*/
__out_opt POSITION CBaseList::AddAfterI(__in_opt POSITION pos, __in void *pObj)
{
    if (pos == NULL)
        return AddHeadI(pObj);

    /* As someone else might be furkling with the list -
       Lock the critical section before continuing
    */
    CNode *pAfter = (CNode *)pos;
    ASSERT(pAfter != NULL);
    if (pAfter == m_pLast)
        return AddTailI(pObj);

    /* set pnode to point to a new node, preferably from the cache */

    CNode *pNode = (CNode *)m_Cache.RemoveFromCache();
    if (pNode == NULL)
    {
        pNode = new CNode;
    }

    /* Check we have a valid object */

    if (pNode == NULL)
    {
        return NULL;
    }

    /* Initialise all the CNode object
       just in case it came from the cache
    */

    pNode->SetData(pObj);

    /* It is to be added to the middle of the list - there is a before
       and after node.  Chain it after pAfter, before pBefore.
    */
    CNode *pBefore = pAfter->Next();
    ASSERT(pBefore != NULL);

    /* chain it in (set four pointers) */
    pNode->SetPrev(pAfter);
    pNode->SetNext(pBefore);
    pBefore->SetPrev(pNode);
    pAfter->SetNext(pNode);

    ++m_Count;

    return (POSITION)pNode;

} // AddAfter(object)

BOOL CBaseList::AddAfter(__in_opt POSITION p, __in CBaseList *pList)
{
    POSITION pos;
    INTERNALTRAVERSELIST(*pList, pos)
    {
        /* p follows along the elements being added */
        p = AddAfterI(p, pList->GetValidI(pos));
        if (p == NULL)
            return FALSE;
    }
    return TRUE;
} // AddAfter(list)

/* Mirror images:
   Add the element or list after position p.
   p is still valid after the operation.
   AddBefore(NULL,x) adds x to the end - same as AddTail
*/
__out_opt POSITION CBaseList::AddBeforeI(__in_opt POSITION pos, __in void *pObj)
{
    if (pos == NULL)
        return AddTailI(pObj);

    /* set pnode to point to a new node, preferably from the cache */

    CNode *pBefore = (CNode *)pos;
    ASSERT(pBefore != NULL);
    if (pBefore == m_pFirst)
        return AddHeadI(pObj);

    CNode *pNode = (CNode *)m_Cache.RemoveFromCache();
    if (pNode == NULL)
    {
        pNode = new CNode;
    }

    /* Check we have a valid object */

    if (pNode == NULL)
    {
        return NULL;
    }

    /* Initialise all the CNode object
       just in case it came from the cache
    */

    pNode->SetData(pObj);

    /* It is to be added to the middle of the list - there is a before
       and after node.  Chain it after pAfter, before pBefore.
    */

    CNode *pAfter = pBefore->Prev();
    ASSERT(pAfter != NULL);

    /* chain it in (set four pointers) */
    pNode->SetPrev(pAfter);
    pNode->SetNext(pBefore);
    pBefore->SetPrev(pNode);
    pAfter->SetNext(pNode);

    ++m_Count;

    return (POSITION)pNode;

} // Addbefore(object)

BOOL CBaseList::AddBefore(__in_opt POSITION p, __in CBaseList *pList)
{
    POSITION pos;
    INTERNALREVERSETRAVERSELIST(*pList, pos)
    {
        /* p follows along the elements being added */
        p = AddBeforeI(p, pList->GetValidI(pos));
        if (p == NULL)
            return FALSE;
    }
    return TRUE;
} // AddBefore(list)

/* Split *this after position p in *this
   Retain as *this the tail portion of the original *this
   Add the head portion to the tail end of *pList
   Return TRUE if it all worked, FALSE if it didn't.

   e.g.
      foo->MoveToTail(foo->GetHeadPosition(), bar);
          moves one element from the head of foo to the tail of bar
      foo->MoveToTail(NULL, bar);
          is a no-op
      foo->MoveToTail(foo->GetTailPosition, bar);
          concatenates foo onto the end of bar and empties foo.

   A better, except excessively long name might be
       MoveElementsFromHeadThroughPositionToOtherTail
*/
BOOL CBaseList::MoveToTail(__in_opt POSITION pos, __in CBaseList *pList)
{
    /* Algorithm:
       Note that the elements (including their order) in the concatenation
       of *pList to the head of *this is invariant.
       1. Count elements to be moved
       2. Join *pList onto the head of this to make one long chain
       3. Set first/Last pointers in *this and *pList
       4. Break the chain at the new place
       5. Adjust counts
       6. Set/Reset any events
    */

    if (pos == NULL)
        return TRUE; // no-op.  Eliminates special cases later.

    /* Make cMove the number of nodes to move */
    CNode *p = (CNode *)pos;
    int cMove = 0; // number of nodes to move
    while (p != NULL)
    {
        p = p->Prev();
        ++cMove;
    }

    /* Join the two chains together */
    if (pList->m_pLast != NULL)
        pList->m_pLast->SetNext(m_pFirst);
    if (m_pFirst != NULL)
        m_pFirst->SetPrev(pList->m_pLast);

    /* set first and last pointers */
    p = (CNode *)pos;

    if (pList->m_pFirst == NULL)
        pList->m_pFirst = m_pFirst;
    m_pFirst = p->Next();
    if (m_pFirst == NULL)
        m_pLast = NULL;
    pList->m_pLast = p;

    /* Break the chain after p to create the new pieces */
    if (m_pFirst != NULL)
        m_pFirst->SetPrev(NULL);
    p->SetNext(NULL);

    /* Adjust the counts */
    m_Count -= cMove;
    pList->m_Count += cMove;

    return TRUE;

} // MoveToTail

/* Mirror image of MoveToTail:
   Split *this before position p in *this.
   Retain in *this the head portion of the original *this
   Add the tail portion to the start (i.e. head) of *pList
   Return TRUE if it all worked, FALSE if it didn't.

   e.g.
      foo->MoveToHead(foo->GetTailPosition(), bar);
          moves one element from the tail of foo to the head of bar
      foo->MoveToHead(NULL, bar);
          is a no-op
      foo->MoveToHead(foo->GetHeadPosition, bar);
          concatenates foo onto the start of bar and empties foo.
*/
BOOL CBaseList::MoveToHead(__in_opt POSITION pos, __in CBaseList *pList)
{

    /* See the comments on the algorithm in MoveToTail */

    if (pos == NULL)
        return TRUE; // no-op.  Eliminates special cases later.

    /* Make cMove the number of nodes to move */
    CNode *p = (CNode *)pos;
    int cMove = 0; // number of nodes to move
    while (p != NULL)
    {
        p = p->Next();
        ++cMove;
    }

    /* Join the two chains together */
    if (pList->m_pFirst != NULL)
        pList->m_pFirst->SetPrev(m_pLast);
    if (m_pLast != NULL)
        m_pLast->SetNext(pList->m_pFirst);

    /* set first and last pointers */
    p = (CNode *)pos;

    if (pList->m_pLast == NULL)
        pList->m_pLast = m_pLast;

    m_pLast = p->Prev();
    if (m_pLast == NULL)
        m_pFirst = NULL;
    pList->m_pFirst = p;

    /* Break the chain after p to create the new pieces */
    if (m_pLast != NULL)
        m_pLast->SetNext(NULL);
    p->SetPrev(NULL);

    /* Adjust the counts */
    m_Count -= cMove;
    pList->m_Count += cMove;

    return TRUE;

} // MoveToHead

/* Reverse the order of the [pointers to] objects in *this
 */
void CBaseList::Reverse()
{
    /* algorithm:
       The obvious booby trap is that you flip pointers around and lose
       addressability to the node that you are going to process next.
       The easy way to avoid this is do do one chain at a time.

       Run along the forward chain,
       For each node, set the reverse pointer to the one ahead of us.
       The reverse chain is now a copy of the old forward chain, including
       the NULL termination.

       Run along the reverse chain (i.e. old forward chain again)
       For each node set the forward pointer of the node ahead to point back
       to the one we're standing on.
       The first node needs special treatment,
       it's new forward pointer is NULL.
       Finally set the First/Last pointers

    */
    CNode *p;

    // Yes we COULD use a traverse, but it would look funny!
    p = m_pFirst;
    while (p != NULL)
    {
        CNode *q;
        q = p->Next();
        p->SetNext(p->Prev());
        p->SetPrev(q);
        p = q;
    }

    p = m_pFirst;
    m_pFirst = m_pLast;
    m_pLast = p;

#if 0 // old version

    if (m_pFirst==NULL) return;          // empty list
    if (m_pFirst->Next()==NULL) return;  // single node list


    /* run along forward chain */
    for ( p = m_pFirst
        ; p!=NULL
        ; p = p->Next()
        ){
        p->SetPrev(p->Next());
    }


    /* special case first element */
    m_pFirst->SetNext(NULL);     // fix the old first element


    /* run along new reverse chain i.e. old forward chain again */
    for ( p = m_pFirst           // start at the old first element
        ; p->Prev()!=NULL        // while there's a node still to be set
        ; p = p->Prev()          // work in the same direction as before
        ){
        p->Prev()->SetNext(p);
    }


    /* fix forward and reverse pointers
       - the triple XOR swap would work but all the casts look hideous */
    p = m_pFirst;
    m_pFirst = m_pLast;
    m_pLast = p;
#endif

} // Reverse
