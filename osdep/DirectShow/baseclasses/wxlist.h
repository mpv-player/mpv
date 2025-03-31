//------------------------------------------------------------------------------
// File: WXList.h
//
// Desc: DirectShow base classes - defines a non-MFC generic template list
//       class.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

/* A generic list of pointers to objects.
   No storage management or copying is done on the objects pointed to.
   Objectives: avoid using MFC libraries in ndm kernel mode and
   provide a really useful list type.

   The class is thread safe in that separate threads may add and
   delete items in the list concurrently although the application
   must ensure that constructor and destructor access is suitably
   synchronised. An application can cause deadlock with operations
   which use two lists by simultaneously calling
   list1->Operation(list2) and list2->Operation(list1).  So don't!

   The names must not conflict with MFC classes as an application
   may use both.
   */

#ifndef __WXLIST__
#define __WXLIST__

/* A POSITION represents (in some fashion that's opaque) a cursor
   on the list that can be set to identify any element.  NULL is
   a valid value and several operations regard NULL as the position
   "one step off the end of the list".  (In an n element list there
   are n+1 places to insert and NULL is that "n+1-th" value).
   The POSITION of an element in the list is only invalidated if
   that element is deleted.  Move operations may mean that what
   was a valid POSITION in one list is now a valid POSITION in
   a different list.

   Some operations which at first sight are illegal are allowed as
   harmless no-ops.  For instance RemoveHead is legal on an empty
   list and it returns NULL.  This allows an atomic way to test if
   there is an element there, and if so, get it.  The two operations
   AddTail and RemoveHead thus implement a MONITOR (See Hoare's paper).

   Single element operations return POSITIONs, non-NULL means it worked.
   whole list operations return a BOOL.  TRUE means it all worked.

   This definition is the same as the POSITION type for MFCs, so we must
   avoid defining it twice.
*/
#ifndef __AFX_H__
struct __POSITION
{
    int unused;
};
typedef __POSITION *POSITION;
#endif

const int DEFAULTCACHE = 10; /* Default node object cache size */

/* A class representing one node in a list.
   Each node knows a pointer to it's adjacent nodes and also a pointer
   to the object that it looks after.
   All of these pointers can be retrieved or set through member functions.
*/
class CBaseList
#ifdef DEBUG
    : public CBaseObject
#endif
{
    /* Making these classes inherit from CBaseObject does nothing
       functionally but it allows us to check there are no memory
       leaks in debug builds.
    */

  public:
#ifdef DEBUG
    class CNode : public CBaseObject
    {
#else
    class CNode
    {
#endif

        CNode *m_pPrev;  /* Previous node in the list */
        CNode *m_pNext;  /* Next node in the list */
        void *m_pObject; /* Pointer to the object */

      public:
        /* Constructor - initialise the object's pointers */
        CNode()
#ifdef DEBUG
            : CBaseObject(NAME("List node"))
#endif
                  {};

        /* Return the previous node before this one */
        __out CNode *Prev() const { return m_pPrev; };

        /* Return the next node after this one */
        __out CNode *Next() const { return m_pNext; };

        /* Set the previous node before this one */
        void SetPrev(__in_opt CNode *p) { m_pPrev = p; };

        /* Set the next node after this one */
        void SetNext(__in_opt CNode *p) { m_pNext = p; };

        /* Get the pointer to the object for this node */
        __out void *GetData() const { return m_pObject; };

        /* Set the pointer to the object for this node */
        void SetData(__in void *p) { m_pObject = p; };
    };

    class CNodeCache
    {
      public:
        CNodeCache(INT iCacheSize)
            : m_iCacheSize(iCacheSize)
            , m_pHead(NULL)
            , m_iUsed(0){};
        ~CNodeCache()
        {
            CNode *pNode = m_pHead;
            while (pNode)
            {
                CNode *pCurrent = pNode;
                pNode = pNode->Next();
                delete pCurrent;
            }
        };
        void AddToCache(__inout CNode *pNode)
        {
            if (m_iUsed < m_iCacheSize)
            {
                pNode->SetNext(m_pHead);
                m_pHead = pNode;
                m_iUsed++;
            }
            else
            {
                delete pNode;
            }
        };
        CNode *RemoveFromCache()
        {
            CNode *pNode = m_pHead;
            if (pNode != NULL)
            {
                m_pHead = pNode->Next();
                m_iUsed--;
                ASSERT(m_iUsed >= 0);
            }
            else
            {
                ASSERT(m_iUsed == 0);
            }
            return pNode;
        };

      private:
        INT m_iCacheSize;
        INT m_iUsed;
        CNode *m_pHead;
    };

  protected:
    CNode *m_pFirst; /* Pointer to first node in the list */
    CNode *m_pLast;  /* Pointer to the last node in the list */
    LONG m_Count;    /* Number of nodes currently in the list */

  private:
    CNodeCache m_Cache; /* Cache of unused node pointers */

  private:
    /* These override the default copy constructor and assignment
       operator for all list classes. They are in the private class
       declaration section so that anybody trying to pass a list
       object by value will generate a compile time error of
       "cannot access the private member function". If these were
       not here then the compiler will create default constructors
       and assignment operators which when executed first take a
       copy of all member variables and then during destruction
       delete them all. This must not be done for any heap
       allocated data.
    */
    CBaseList(const CBaseList &refList);
    CBaseList &operator=(const CBaseList &refList);

  public:
    CBaseList(__in_opt LPCTSTR pName, INT iItems);

    CBaseList(__in_opt LPCTSTR pName);
#ifdef UNICODE
    CBaseList(__in_opt LPCSTR pName, INT iItems);

    CBaseList(__in_opt LPCSTR pName);
#endif
    ~CBaseList();

    /* Remove all the nodes from *this i.e. make the list empty */
    void RemoveAll();

    /* Return a cursor which identifies the first element of *this */
    __out_opt POSITION GetHeadPositionI() const;

    /* Return a cursor which identifies the last element of *this */
    __out_opt POSITION GetTailPositionI() const;

    /* Return the number of objects in *this */
    int GetCountI() const;

  protected:
    /* Return the pointer to the object at rp,
       Update rp to the next node in *this
       but make it NULL if it was at the end of *this.
       This is a wart retained for backwards compatibility.
       GetPrev is not implemented.
       Use Next, Prev and Get separately.
    */
    __out void *GetNextI(__inout POSITION &rp) const;

    /* Return a pointer to the object at p
       Asking for the object at NULL will return NULL harmlessly.
    */
    __out_opt void *GetI(__in_opt POSITION p) const;
    __out void *GetValidI(__in POSITION p) const;

  public:
    /* return the next / prev position in *this
       return NULL when going past the end/start.
       Next(NULL) is same as GetHeadPosition()
       Prev(NULL) is same as GetTailPosition()
       An n element list therefore behaves like a n+1 element
       cycle with NULL at the start/end.

       !!WARNING!! - This handling of NULL is DIFFERENT from GetNext.

       Some reasons are:
       1. For a list of n items there are n+1 positions to insert
          These are conveniently encoded as the n POSITIONs and NULL.
       2. If you are keeping a list sorted (fairly common) and you
          search forward for an element to insert before and don't
          find it you finish up with NULL as the element before which
          to insert.  You then want that NULL to be a valid POSITION
          so that you can insert before it and you want that insertion
          point to mean the (n+1)-th one that doesn't have a POSITION.
          (symmetrically if you are working backwards through the list).
       3. It simplifies the algebra which the methods generate.
          e.g. AddBefore(p,x) is identical to AddAfter(Prev(p),x)
          in ALL cases.  All the other arguments probably are reflections
          of the algebraic point.
    */
    __out_opt POSITION Next(__in_opt POSITION pos) const
    {
        if (pos == NULL)
        {
            return (POSITION)m_pFirst;
        }
        CNode *pn = (CNode *)pos;
        return (POSITION)pn->Next();
    } // Next

    // See Next
    __out_opt POSITION Prev(__in_opt POSITION pos) const
    {
        if (pos == NULL)
        {
            return (POSITION)m_pLast;
        }
        CNode *pn = (CNode *)pos;
        return (POSITION)pn->Prev();
    } // Prev

    /* Return the first position in *this which holds the given
       pointer.  Return NULL if the pointer was not not found.
    */
  protected:
    __out_opt POSITION FindI(__in void *pObj) const;

    // ??? Should there be (or even should there be only)
    // ??? POSITION FindNextAfter(void * pObj, POSITION p)
    // ??? And of course FindPrevBefore too.
    // ??? List.Find(&Obj) then becomes List.FindNextAfter(&Obj, NULL)

    /* Remove the first node in *this (deletes the pointer to its
       object from the list, does not free the object itself).
       Return the pointer to its object.
       If *this was already empty it will harmlessly return NULL.
    */
    __out_opt void *RemoveHeadI();

    /* Remove the last node in *this (deletes the pointer to its
       object from the list, does not free the object itself).
       Return the pointer to its object.
       If *this was already empty it will harmlessly return NULL.
    */
    __out_opt void *RemoveTailI();

    /* Remove the node identified by p from the list (deletes the pointer
       to its object from the list, does not free the object itself).
       Asking to Remove the object at NULL will harmlessly return NULL.
       Return the pointer to the object removed.
    */
    __out_opt void *RemoveI(__in_opt POSITION p);

    /* Add single object *pObj to become a new last element of the list.
       Return the new tail position, NULL if it fails.
       If you are adding a COM objects, you might want AddRef it first.
       Other existing POSITIONs in *this are still valid
    */
    __out_opt POSITION AddTailI(__in void *pObj);

  public:
    /* Add all the elements in *pList to the tail of *this.
       This duplicates all the nodes in *pList (i.e. duplicates
       all its pointers to objects).  It does not duplicate the objects.
       If you are adding a list of pointers to a COM object into the list
       it's a good idea to AddRef them all  it when you AddTail it.
       Return TRUE if it all worked, FALSE if it didn't.
       If it fails some elements may have been added.
       Existing POSITIONs in *this are still valid

       If you actually want to MOVE the elements, use MoveToTail instead.
    */
    BOOL AddTail(__in CBaseList *pList);

    /* Mirror images of AddHead: */

    /* Add single object to become a new first element of the list.
       Return the new head position, NULL if it fails.
       Existing POSITIONs in *this are still valid
    */
  protected:
    __out_opt POSITION AddHeadI(__in void *pObj);

  public:
    /* Add all the elements in *pList to the head of *this.
       Same warnings apply as for AddTail.
       Return TRUE if it all worked, FALSE if it didn't.
       If it fails some of the objects may have been added.

       If you actually want to MOVE the elements, use MoveToHead instead.
    */
    BOOL AddHead(__in CBaseList *pList);

    /* Add the object *pObj to *this after position p in *this.
       AddAfter(NULL,x) adds x to the start - equivalent to AddHead
       Return the position of the object added, NULL if it failed.
       Existing POSITIONs in *this are undisturbed, including p.
    */
  protected:
    __out_opt POSITION AddAfterI(__in_opt POSITION p, __in void *pObj);

  public:
    /* Add the list *pList to *this after position p in *this
       AddAfter(NULL,x) adds x to the start - equivalent to AddHead
       Return TRUE if it all worked, FALSE if it didn't.
       If it fails, some of the objects may be added
       Existing POSITIONs in *this are undisturbed, including p.
    */
    BOOL AddAfter(__in_opt POSITION p, __in CBaseList *pList);

    /* Mirror images:
       Add the object *pObj to this-List after position p in *this.
       AddBefore(NULL,x) adds x to the end - equivalent to AddTail
       Return the position of the new object, NULL if it fails
       Existing POSITIONs in *this are undisturbed, including p.
    */
  protected:
    __out_opt POSITION AddBeforeI(__in_opt POSITION p, __in void *pObj);

  public:
    /* Add the list *pList to *this before position p in *this
       AddAfter(NULL,x) adds x to the start - equivalent to AddHead
       Return TRUE if it all worked, FALSE if it didn't.
       If it fails, some of the objects may be added
       Existing POSITIONs in *this are undisturbed, including p.
    */
    BOOL AddBefore(__in_opt POSITION p, __in CBaseList *pList);

    /* Note that AddAfter(p,x) is equivalent to AddBefore(Next(p),x)
       even in cases where p is NULL or Next(p) is NULL.
       Similarly for mirror images etc.
       This may make it easier to argue about programs.
    */

    /* The following operations do not copy any elements.
       They move existing blocks of elements around by switching pointers.
       They are fairly efficient for long lists as for short lists.
       (Alas, the Count slows things down).

       They split the list into two parts.
       One part remains as the original list, the other part
       is appended to the second list.  There are eight possible
       variations:
       Split the list {after/before} a given element
       keep the {head/tail} portion in the original list
       append the rest to the {head/tail} of the new list.

       Since After is strictly equivalent to Before Next
       we are not in serious need of the Before/After variants.
       That leaves only four.

       If you are processing a list left to right and dumping
       the bits that you have processed into another list as
       you go, the Tail/Tail variant gives the most natural result.
       If you are processing in reverse order, Head/Head is best.

       By using NULL positions and empty lists judiciously either
       of the other two can be built up in two operations.

       The definition of NULL (see Next/Prev etc) means that
       degenerate cases include
          "move all elements to new list"
          "Split a list into two lists"
          "Concatenate two lists"
          (and quite a few no-ops)

       !!WARNING!! The type checking won't buy you much if you get list
       positions muddled up - e.g. use a POSITION that's in a different
       list and see what a mess you get!
    */

    /* Split *this after position p in *this
       Retain as *this the tail portion of the original *this
       Add the head portion to the tail end of *pList
       Return TRUE if it all worked, FALSE if it didn't.

       e.g.
          foo->MoveToTail(foo->GetHeadPosition(), bar);
              moves one element from the head of foo to the tail of bar
          foo->MoveToTail(NULL, bar);
              is a no-op, returns NULL
          foo->MoveToTail(foo->GetTailPosition, bar);
              concatenates foo onto the end of bar and empties foo.

       A better, except excessively long name might be
           MoveElementsFromHeadThroughPositionToOtherTail
    */
    BOOL MoveToTail(__in_opt POSITION pos, __in CBaseList *pList);

    /* Mirror image:
       Split *this before position p in *this.
       Retain in *this the head portion of the original *this
       Add the tail portion to the start (i.e. head) of *pList

       e.g.
          foo->MoveToHead(foo->GetTailPosition(), bar);
              moves one element from the tail of foo to the head of bar
          foo->MoveToHead(NULL, bar);
              is a no-op, returns NULL
          foo->MoveToHead(foo->GetHeadPosition, bar);
              concatenates foo onto the start of bar and empties foo.
    */
    BOOL MoveToHead(__in_opt POSITION pos, __in CBaseList *pList);

    /* Reverse the order of the [pointers to] objects in *this
     */
    void Reverse();

/* set cursor to the position of each element of list in turn  */
#define TRAVERSELIST(list, cursor) for (cursor = (list).GetHeadPosition(); cursor != NULL; cursor = (list).Next(cursor))

/* set cursor to the position of each element of list in turn
   in reverse order
*/
#define REVERSETRAVERSELIST(list, cursor) \
    for (cursor = (list).GetTailPosition(); cursor != NULL; cursor = (list).Prev(cursor))

}; // end of class declaration

template <class OBJECT> class CGenericList : public CBaseList
{
  public:
    CGenericList(__in_opt LPCTSTR pName, INT iItems, BOOL bLock = TRUE, BOOL bAlert = FALSE)
        : CBaseList(pName, iItems)
    {
        UNREFERENCED_PARAMETER(bAlert);
        UNREFERENCED_PARAMETER(bLock);
    };
    CGenericList(__in_opt LPCTSTR pName)
        : CBaseList(pName){};

    __out_opt POSITION GetHeadPosition() const { return (POSITION)m_pFirst; }
    __out_opt POSITION GetTailPosition() const { return (POSITION)m_pLast; }
    int GetCount() const { return m_Count; }

    __out OBJECT *GetNext(__inout POSITION &rp) const { return (OBJECT *)GetNextI(rp); }

    __out_opt OBJECT *Get(__in_opt POSITION p) const { return (OBJECT *)GetI(p); }
    __out OBJECT *GetValid(__in POSITION p) const { return (OBJECT *)GetValidI(p); }
    __out_opt OBJECT *GetHead() const { return Get(GetHeadPosition()); }

    __out_opt OBJECT *RemoveHead() { return (OBJECT *)RemoveHeadI(); }

    __out_opt OBJECT *RemoveTail() { return (OBJECT *)RemoveTailI(); }

    __out_opt OBJECT *Remove(__in_opt POSITION p) { return (OBJECT *)RemoveI(p); }
    __out_opt POSITION AddBefore(__in_opt POSITION p, __in OBJECT *pObj) { return AddBeforeI(p, pObj); }
    __out_opt POSITION AddAfter(__in_opt POSITION p, __in OBJECT *pObj) { return AddAfterI(p, pObj); }
    __out_opt POSITION AddHead(__in OBJECT *pObj) { return AddHeadI(pObj); }
    __out_opt POSITION AddTail(__in OBJECT *pObj) { return AddTailI(pObj); }
    BOOL AddTail(__in CGenericList<OBJECT> *pList) { return CBaseList::AddTail((CBaseList *)pList); }
    BOOL AddHead(__in CGenericList<OBJECT> *pList) { return CBaseList::AddHead((CBaseList *)pList); }
    BOOL AddAfter(__in_opt POSITION p, __in CGenericList<OBJECT> *pList)
    {
        return CBaseList::AddAfter(p, (CBaseList *)pList);
    };
    BOOL AddBefore(__in_opt POSITION p, __in CGenericList<OBJECT> *pList)
    {
        return CBaseList::AddBefore(p, (CBaseList *)pList);
    };
    __out_opt POSITION Find(__in OBJECT *pObj) const { return FindI(pObj); }
}; // end of class declaration

/* These define the standard list types */

typedef CGenericList<CBaseObject> CBaseObjectList;
typedef CGenericList<IUnknown> CBaseInterfaceList;

#endif /* __WXLIST__ */
