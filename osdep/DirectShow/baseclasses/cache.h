//------------------------------------------------------------------------------
// File: Cache.h
//
// Desc: DirectShow base classes - efines a non-MFC generic cache class.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

/* This class implements a simple cache. A cache object is instantiated
   with the number of items it is to hold. An item is a pointer to an
   object derived from CBaseObject (helps reduce memory leaks). The cache
   can then have objects added to it and removed from it. The cache size
   is fixed at construction time and may therefore run out or be flooded.
   If it runs out it returns a NULL pointer, if it fills up it also returns
   a NULL pointer instead of a pointer to the object just inserted */

/* Making these classes inherit from CBaseObject does nothing for their
   functionality but it allows us to check there are no memory leaks */

/* WARNING Be very careful when using this class, what it lets you do is
   store and retrieve objects so that you can minimise object creation
   which in turns improves efficiency. However the object you store is
   exactly the same as the object you get back which means that it short
   circuits the constructor initialisation phase. This means any class
   variables the object has (eg pointers) are highly likely to be invalid.
   Therefore ensure you reinitialise the object before using it again */

#ifndef __CACHE__
#define __CACHE__

class CCache : CBaseObject
{

    /* Make copy constructor and assignment operator inaccessible */

    CCache(const CCache &refCache);
    CCache &operator=(const CCache &refCache);

  private:
    /* These are initialised in the constructor. The first variable points to
       an array of pointers, each of which points to a CBaseObject derived
       object. The m_iCacheSize is the static fixed size for the cache and the
       m_iUsed defines the number of places filled with objects at any time.
       We fill the array of pointers from the start (ie m_ppObjects[0] first)
       and then only add and remove objects from the end position, so in this
       respect the array of object pointers should be treated as a stack */

    CBaseObject **m_ppObjects;
    const INT m_iCacheSize;
    INT m_iUsed;

  public:
    CCache(__in_opt LPCTSTR pName, INT iItems);
    virtual ~CCache();

    /* Add an item to the cache */
    CBaseObject *AddToCache(__in CBaseObject *pObject);

    /* Remove an item from the cache */
    CBaseObject *RemoveFromCache();

    /* Delete all the objects held in the cache */
    void RemoveAll(void);

    /* Return the cache size which is set during construction */
    INT GetCacheSize(void) const { return m_iCacheSize; };
};

#endif /* __CACHE__ */
