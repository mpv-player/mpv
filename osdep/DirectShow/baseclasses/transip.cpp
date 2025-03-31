//------------------------------------------------------------------------------
// File: TransIP.cpp
//
// Desc: DirectShow base classes - implements class for simple Transform-
//       In-Place filters such as audio.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

// How allocators are decided.
//
// An in-place transform tries to do its work in someone else's buffers.
// It tries to persuade the filters on either side to use the same allocator
// (and for that matter the same media type).  In desperation, if the downstream
// filter refuses to supply an allocator and the upstream filter offers only
// a read-only one then it will provide an allocator.
// if the upstream filter insists on a read-only allocator then the transform
// filter will (reluctantly) copy the data before transforming it.
//
// In order to pass an allocator through it needs to remember the one it got
// from the first connection to pass it on to the second one.
//
// It is good if we can avoid insisting on a particular order of connection
// (There is a precedent for insisting on the input
// being connected first.  Insisting on the output being connected first is
// not allowed.  That would break RenderFile.)
//
// The base pin classes (CBaseOutputPin and CBaseInputPin) both have a
// m_pAllocator member which is used in places like
// CBaseOutputPin::GetDeliveryBuffer and CBaseInputPin::Inactive.
// To avoid lots of extra overriding, we should keep these happy
// by using these pointers.
//
// When each pin is connected, it will set the corresponding m_pAllocator
// and will have a single ref-count on that allocator.
//
// Refcounts are acquired by GetAllocator calls which return AddReffed
// allocators and are released in one of:
//     CBaseInputPin::Disconnect
//     CBaseOutputPin::BreakConect
// In each case m_pAllocator is set to NULL after the release, so this
// is the last chance to ever release it.  If there should ever be
// multiple refcounts associated with the same pointer, this had better
// be cleared up before that happens.  To avoid such problems, we'll
// stick with one per pointer.

// RECONNECTING and STATE CHANGES
//
// Each pin could be disconnected, connected with a read-only allocator,
// connected with an upstream read/write allocator, connected with an
// allocator from downstream or connected with its own allocator.
// Five states for each pin gives a data space of 25 states.
//
// Notation:
//
// R/W == read/write
// R-O == read-only
//
// <input pin state> <output pin state> <comments>
//
// 00 means an unconnected pin.
// <- means using a R/W allocator from the upstream filter
// <= means using a R-O allocator from an upstream filter
// || means using our own (R/W) allocator.
// -> means using a R/W allocator from a downstream filter
//    (a R-O allocator from downstream is nonsense, it can't ever work).
//
//
// That makes 25 possible states.  Some states are nonsense (two different
// allocators from the same place).  These are just an artifact of the notation.
//        <=  <-  Nonsense.
//        <-  <=  Nonsense
// Some states are illegal (the output pin never accepts a R-O allocator):
//        00  <=  !! Error !!
//        <=  <=  !! Error !!
//        ||  <=  !! Error !!
//        ->  <=  !! Error !!
// Three states appears to be inaccessible:
//        ->  ||  Inaccessible
//        ||  ->  Inaccessible
//        ||  <-  Inaccessible
// Some states only ever occur as intermediates with a pending reconnect which
// is guaranteed to finish in another state.
//        ->  00  ?? unstable goes to || 00
//        00  <-  ?? unstable goes to 00 ||
//        ->  <-  ?? unstable goes to -> ->
//        <-  ||  ?? unstable goes to <- <-
//        <-  ->  ?? unstable goes to <- <-
// And that leaves 11 possible resting states:
// 1      00  00  Nothing connected.
// 2      <-  00  Input pin connected.
// 3      <=  00  Input pin connected using R-O allocator.
// 4      ||  00  Needs several state changes to get here.
// 5      00  ||  Output pin connected using our allocator
// 6      00  ->  Downstream only connected
// 7      ||  ||  Undesirable but can be forced upon us.
// 8      <=  ||  Copy forced.  <=  -> is preferable
// 9      <=  ->  OK - forced to copy.
// 10     <-  <-  Transform in place (ideal)
// 11     ->  ->  Transform in place (ideal)
//
// The object of the exercise is to ensure that we finish up in states
// 10 or 11 whenever possible.  State 10 is only possible if the upstream
// filter has a R/W allocator (the AVI splitter notoriously
// doesn't) and state 11 is only possible if the downstream filter does
// offer an allocator.
//
// The transition table (entries marked * go via a reconnect)
//
// There are 8 possible transitions:
// A: Connect upstream to filter with R-O allocator that insists on using it.
// B: Connect upstream to filter with R-O allocator but chooses not to use it.
// C: Connect upstream to filter with R/W allocator and insists on using it.
// D: Connect upstream to filter with R/W allocator but chooses not to use it.
// E: Connect downstream to a filter that offers an allocator
// F: Connect downstream to a filter that does not offer an allocator
// G: disconnect upstream
// H: Disconnect downstream
//
//            A      B      C      D      E      F      G      H
//           ---------------------------------------------------------
// 00  00 1 | 3      3      2      2      6      5      .      .      |1  00  00
// <-  00 2 | .      .      .      .      *10/11 10     1      .      |2  <-  00
// <=  00 3 | .      .      .      .      *9/11  *7/8   1      .      |3  <=  00
// ||  00 4 | .      .      .      .      *8     *7     1      .      |4  ||  00
// 00  || 5 | 8      7      *10    7      .      .      .      1      |5  00  ||
// 00  -> 6 | 9      11     *10    11     .      .      .      1      |6  00  ->
// ||  || 7 | .      .      .      .      .      .      5      4      |7  ||  ||
// <=  || 8 | .      .      .      .      .      .      5      3      |8  <=  ||
// <=  -> 9 | .      .      .      .      .      .      6      3      |9  <=  ->
// <-  <- 10| .      .      .      .      .      .      *5/6   2      |10 <-  <-
// ->  -> 11| .      .      .      .      .      .      6      *2/3   |11 ->  ->
//           ---------------------------------------------------------
//            A      B      C      D      E      F      G      H
//
// All these states are accessible without requiring any filter to
// change its behaviour but not all transitions are accessible, for
// instance a transition from state 4 to anywhere other than
// state 8 requires that the upstream filter first offer a R-O allocator
// and then changes its mind and offer R/W.  This is NOT allowable - it
// leads to things like the output pin getting a R/W allocator from
// upstream and then the input pin being told it can only have a R-O one.
// Note that you CAN change (say) the upstream filter for a different one, but
// only as a disconnect / connect, not as a Reconnect.  (Exercise for
// the reader is to see how you get into state 4).
//
// The reconnection stuff goes as follows (some of the cases shown here as
// "no reconnect" may get one to finalise media type - an old story).
// If there is a reconnect where it says "no reconnect" here then the
// reconnection must not change the allocator choice.
//
// state 2: <- 00 transition E <- <- case C <- <- (no change)
//                                   case D -> <- and then to -> ->
//
// state 2: <- 00 transition F <- <- (no reconnect)
//
// state 3: <= 00 transition E <= -> case A <= -> (no change)
//                                   case B -> ->
//                transition F <= || case A <= || (no change)
//                                   case B || ||
//
// state 4: || 00 transition E || || case B -> || and then all cases to -> ->
//                           F || || case B || || (no change)
//
// state 5: 00 || transition A <= || (no reconnect)
//                           B || || (no reconnect)
//                           C <- || all cases     <- <-
//                           D || || (unfortunate, but upstream's choice)
//
// state 6: 00 -> transition A <= -> (no reconnect)
//                           B -> -> (no reconnect)
//                           C <- -> all cases <- <-
//                           D -> -> (no reconnect)
//
// state 10:<- <- transition G 00 <- case E 00 ->
//                                   case F 00 ||
//
// state 11:-> -> transition H -> 00 case A <= 00 (schizo)
//                                   case B <= 00
//                                   case C <- 00 (schizo)
//                                   case D <- 00
//
// The Rules:
// To sort out media types:
// The input is reconnected
//    if the input pin is connected and the output pin connects
// The output is reconnected
//    If the output pin is connected
//    and the input pin connects to a different media type
//
// To sort out allocators:
// The input is reconnected
//    if the output disconnects and the input was using a downstream allocator
// The output pin calls SetAllocator to pass on a new allocator
//    if the output is connected and
//       if the input disconnects and the output was using an upstream allocator
//       if the input acquires an allocator different from the output one
//          and that new allocator is not R-O
//
// Data is copied (i.e. call getbuffer and copy the data before transforming it)
//    if the two allocators are different.

// CHAINS of filters:
//
// We sit between two filters (call them A and Z).  We should finish up
// with the same allocator on both of our pins and that should be the
// same one that A and Z would have agreed on if we hadn't been in the
// way.  Furthermore, it should not matter how many in-place transforms
// are in the way.  Let B, C, D... be in-place transforms ("us").
// Here's how it goes:
//
// 1.
// A connects to B.  They agree on A's allocator.
//   A-a->B
//
// 2.
// B connects to C.  Same story. There is no point in a reconnect, but
// B will request an input reconnect anyway.
//   A-a->B-a->C
//
// 3.
// C connects to Z.
// C insists on using A's allocator, but compromises by requesting a reconnect.
// of C's input.
//   A-a->B-?->C-a->Z
//
// We now have pending reconnects on both A--->B and B--->C
//
// 4.
// The A--->B link is reconnected.
// A asks B for an allocator.  B sees that it has a downstream connection so
// asks its downstream input pin i.e. C's input pin for an allocator.  C sees
// that it too has a downstream connection so asks Z for an allocator.
//
// Even though Z's input pin is connected, it is being asked for an allocator.
// It could refuse, in which case the chain is done and will use A's allocator
// Alternatively, Z may supply one.  A chooses either Z's or A's own one.
// B's input pin gets NotifyAllocator called to tell it the decision and it
// propagates this downstream by calling ReceiveAllocator on its output pin
// which calls NotifyAllocator on the next input pin downstream etc.
// If the choice is Z then it goes:
//   A-z->B-a->C-a->Z
//   A-z->B-z->C-a->Z
//   A-z->B-z->C-z->Z
//
// And that's IT!!  Any further (essentially spurious) reconnects peter out
// with no change in the chain.

#include <streams.h>
#include <measure.h>
#include <transip.h>

// =================================================================
// Implements the CTransInPlaceFilter class
// =================================================================

CTransInPlaceFilter::CTransInPlaceFilter(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, REFCLSID clsid,
                                         __inout HRESULT *phr, bool bModifiesData)
    : CTransformFilter(pName, pUnk, clsid)
    , m_bModifiesData(bModifiesData)
{
#ifdef PERF
    RegisterPerfId();
#endif //  PERF

} // constructor

#ifdef UNICODE
CTransInPlaceFilter::CTransInPlaceFilter(__in_opt LPCSTR pName, __inout_opt LPUNKNOWN pUnk, REFCLSID clsid,
                                         __inout HRESULT *phr, bool bModifiesData)
    : CTransformFilter(pName, pUnk, clsid)
    , m_bModifiesData(bModifiesData)
{
#ifdef PERF
    RegisterPerfId();
#endif //  PERF

} // constructor
#endif

// return a non-addrefed CBasePin * for the user to addref if he holds onto it
// for longer than his pointer to us. We create the pins dynamically when they
// are asked for rather than in the constructor. This is because we want to
// give the derived class an oppportunity to return different pin objects

// As soon as any pin is needed we create both (this is different from the
// usual transform filter) because enumerators, allocators etc are passed
// through from one pin to another and it becomes very painful if the other
// pin isn't there.  If we fail to create either pin we ensure we fail both.

CBasePin *CTransInPlaceFilter::GetPin(int n)
{
    HRESULT hr = S_OK;

    // Create an input pin if not already done

    if (m_pInput == NULL)
    {

        m_pInput = new CTransInPlaceInputPin(NAME("TransInPlace input pin"), this // Owner filter
                                             ,
                                             &hr // Result code
                                             ,
                                             L"Input" // Pin name
        );

        // Constructor for CTransInPlaceInputPin can't fail
        ASSERT(SUCCEEDED(hr));
    }

    // Create an output pin if not already done

    if (m_pInput != NULL && m_pOutput == NULL)
    {

        m_pOutput = new CTransInPlaceOutputPin(NAME("TransInPlace output pin"), this // Owner filter
                                               ,
                                               &hr // Result code
                                               ,
                                               L"Output" // Pin name
        );

        // a failed return code should delete the object

        ASSERT(SUCCEEDED(hr));
        if (m_pOutput == NULL)
        {
            delete m_pInput;
            m_pInput = NULL;
        }
    }

    // Return the appropriate pin

    ASSERT(n >= 0 && n <= 1);
    if (n == 0)
    {
        return m_pInput;
    }
    else if (n == 1)
    {
        return m_pOutput;
    }
    else
    {
        return NULL;
    }

} // GetPin

// dir is the direction of our pin.
// pReceivePin is the pin we are connecting to.
HRESULT CTransInPlaceFilter::CompleteConnect(PIN_DIRECTION dir, IPin *pReceivePin)
{
    UNREFERENCED_PARAMETER(pReceivePin);
    ASSERT(m_pInput);
    ASSERT(m_pOutput);

    // if we are not part of a graph, then don't indirect the pointer
    // this probably prevents use of the filter without a filtergraph
    if (!m_pGraph)
    {
        return VFW_E_NOT_IN_GRAPH;
    }

    // Always reconnect the input to account for buffering changes
    //
    // Because we don't get to suggest a type on ReceiveConnection
    // we need another way of making sure the right type gets used.
    //
    // One way would be to have our EnumMediaTypes return our output
    // connection type first but more deterministic and simple is to
    // call ReconnectEx passing the type we want to reconnect with
    // via the base class ReconeectPin method.

    if (dir == PINDIR_OUTPUT)
    {
        if (m_pInput->IsConnected())
        {
            return ReconnectPin(m_pInput, &m_pOutput->CurrentMediaType());
        }
        return NOERROR;
    }

    ASSERT(dir == PINDIR_INPUT);

    // Reconnect output if necessary

    if (m_pOutput->IsConnected())
    {

        if (m_pInput->CurrentMediaType() != m_pOutput->CurrentMediaType())
        {
            return ReconnectPin(m_pOutput, &m_pInput->CurrentMediaType());
        }
    }
    return NOERROR;

} // ComnpleteConnect

//
// DecideBufferSize
//
// Tell the output pin's allocator what size buffers we require.
// *pAlloc will be the allocator our output pin is using.
//

HRESULT CTransInPlaceFilter::DecideBufferSize(IMemAllocator *pAlloc, __inout ALLOCATOR_PROPERTIES *pProperties)
{
    ALLOCATOR_PROPERTIES Request, Actual;
    HRESULT hr;

    // If we are connected upstream, get his views
    if (m_pInput->IsConnected())
    {
        // Get the input pin allocator, and get its size and count.
        // we don't care about his alignment and prefix.

        hr = InputPin()->PeekAllocator()->GetProperties(&Request);
        if (FAILED(hr))
        {
            // Input connected but with a secretive allocator - enough!
            return hr;
        }
    }
    else
    {
        // Propose one byte
        // If this isn't enough then when the other pin does get connected
        // we can revise it.
        ZeroMemory(&Request, sizeof(Request));
        Request.cBuffers = 1;
        Request.cbBuffer = 1;
    }

    DbgLog((LOG_MEMORY, 1, TEXT("Setting Allocator Requirements")));
    DbgLog((LOG_MEMORY, 1, TEXT("Count %d, Size %d"), Request.cBuffers, Request.cbBuffer));

    // Pass the allocator requirements to our output side
    // but do a little sanity checking first or we'll just hit
    // asserts in the allocator.

    pProperties->cBuffers = Request.cBuffers;
    pProperties->cbBuffer = Request.cbBuffer;
    pProperties->cbAlign = Request.cbAlign;
    if (pProperties->cBuffers <= 0)
    {
        pProperties->cBuffers = 1;
    }
    if (pProperties->cbBuffer <= 0)
    {
        pProperties->cbBuffer = 1;
    }
    hr = pAlloc->SetProperties(pProperties, &Actual);

    if (FAILED(hr))
    {
        return hr;
    }

    DbgLog((LOG_MEMORY, 1, TEXT("Obtained Allocator Requirements")));
    DbgLog((LOG_MEMORY, 1, TEXT("Count %d, Size %d, Alignment %d"), Actual.cBuffers, Actual.cbBuffer, Actual.cbAlign));

    // Make sure we got the right alignment and at least the minimum required

    if ((Request.cBuffers > Actual.cBuffers) || (Request.cbBuffer > Actual.cbBuffer) ||
        (Request.cbAlign > Actual.cbAlign))
    {
        return E_FAIL;
    }
    return NOERROR;

} // DecideBufferSize

//
// Copy
//
// return a pointer to an identical copy of pSample
__out_opt IMediaSample *CTransInPlaceFilter::Copy(IMediaSample *pSource)
{
    IMediaSample *pDest;

    HRESULT hr;
    REFERENCE_TIME tStart, tStop;
    const BOOL bTime = S_OK == pSource->GetTime(&tStart, &tStop);

    // this may block for an indeterminate amount of time
    hr = OutputPin()->PeekAllocator()->GetBuffer(&pDest, bTime ? &tStart : NULL, bTime ? &tStop : NULL,
                                                 m_bSampleSkipped ? AM_GBF_PREVFRAMESKIPPED : 0);

    if (FAILED(hr))
    {
        return NULL;
    }

    ASSERT(pDest);
    IMediaSample2 *pSample2;
    if (SUCCEEDED(pDest->QueryInterface(IID_IMediaSample2, (void **)&pSample2)))
    {
        HRESULT hrProps =
            pSample2->SetProperties(FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, pbBuffer), (PBYTE)m_pInput->SampleProps());
        pSample2->Release();
        if (FAILED(hrProps))
        {
            pDest->Release();
            return NULL;
        }
    }
    else
    {
        if (bTime)
        {
            pDest->SetTime(&tStart, &tStop);
        }

        if (S_OK == pSource->IsSyncPoint())
        {
            pDest->SetSyncPoint(TRUE);
        }
        if (S_OK == pSource->IsDiscontinuity() || m_bSampleSkipped)
        {
            pDest->SetDiscontinuity(TRUE);
        }
        if (S_OK == pSource->IsPreroll())
        {
            pDest->SetPreroll(TRUE);
        }

        // Copy the media type
        AM_MEDIA_TYPE *pMediaType;
        if (S_OK == pSource->GetMediaType(&pMediaType))
        {
            pDest->SetMediaType(pMediaType);
            DeleteMediaType(pMediaType);
        }
    }

    m_bSampleSkipped = FALSE;

    // Copy the sample media times
    REFERENCE_TIME TimeStart, TimeEnd;
    if (pSource->GetMediaTime(&TimeStart, &TimeEnd) == NOERROR)
    {
        pDest->SetMediaTime(&TimeStart, &TimeEnd);
    }

    // Copy the actual data length and the actual data.
    {
        const long lDataLength = pSource->GetActualDataLength();
        if (FAILED(pDest->SetActualDataLength(lDataLength)))
        {
            pDest->Release();
            return NULL;
        }

        // Copy the sample data
        {
            BYTE *pSourceBuffer, *pDestBuffer;
            long lSourceSize = pSource->GetSize();
            long lDestSize = pDest->GetSize();

            ASSERT(lDestSize >= lSourceSize && lDestSize >= lDataLength);

            if (FAILED(pSource->GetPointer(&pSourceBuffer)) || FAILED(pDest->GetPointer(&pDestBuffer)) ||
                lDestSize < lDataLength || lDataLength < 0)
            {
                pDest->Release();
                return NULL;
            }
            ASSERT(lDestSize == 0 || pSourceBuffer != NULL && pDestBuffer != NULL);

            CopyMemory((PVOID)pDestBuffer, (PVOID)pSourceBuffer, lDataLength);
        }
    }

    return pDest;

} // Copy

// override this to customize the transform process

HRESULT
CTransInPlaceFilter::Receive(IMediaSample *pSample)
{
    /*  Check for other streams and pass them on */
    AM_SAMPLE2_PROPERTIES *const pProps = m_pInput->SampleProps();
    if (pProps->dwStreamId != AM_STREAM_MEDIA)
    {
        return m_pOutput->Deliver(pSample);
    }
    HRESULT hr;

    // Start timing the TransInPlace (if PERF is defined)
    MSR_START(m_idTransInPlace);

    if (UsingDifferentAllocators())
    {

        // We have to copy the data.

        pSample = Copy(pSample);

        if (pSample == NULL)
        {
            MSR_STOP(m_idTransInPlace);
            return E_UNEXPECTED;
        }
    }

    // have the derived class transform the data
    hr = Transform(pSample);

    // Stop the clock and log it (if PERF is defined)
    MSR_STOP(m_idTransInPlace);

    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 1, TEXT("Error from TransInPlace")));
        if (UsingDifferentAllocators())
        {
            pSample->Release();
        }
        return hr;
    }

    // the Transform() function can return S_FALSE to indicate that the
    // sample should not be delivered; we only deliver the sample if it's
    // really S_OK (same as NOERROR, of course.)
    if (hr == NOERROR)
    {
        hr = m_pOutput->Deliver(pSample);
    }
    else
    {
        //  But it would be an error to return this private workaround
        //  to the caller ...
        if (S_FALSE == hr)
        {
            // S_FALSE returned from Transform is a PRIVATE agreement
            // We should return NOERROR from Receive() in this cause because
            // returning S_FALSE from Receive() means that this is the end
            // of the stream and no more data should be sent.
            m_bSampleSkipped = TRUE;
            if (!m_bQualityChanged)
            {
                NotifyEvent(EC_QUALITY_CHANGE, 0, 0);
                m_bQualityChanged = TRUE;
            }
            hr = NOERROR;
        }
    }

    // release the output buffer. If the connected pin still needs it,
    // it will have addrefed it itself.
    if (UsingDifferentAllocators())
    {
        pSample->Release();
    }

    return hr;

} // Receive

// =================================================================
// Implements the CTransInPlaceInputPin class
// =================================================================

// constructor

CTransInPlaceInputPin::CTransInPlaceInputPin(__in_opt LPCTSTR pObjectName, __inout CTransInPlaceFilter *pFilter,
                                             __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CTransformInputPin(pObjectName, pFilter, phr, pName)
    , m_bReadOnly(FALSE)
    , m_pTIPFilter(pFilter)
{
    DbgLog((LOG_TRACE, 2, TEXT("CTransInPlaceInputPin::CTransInPlaceInputPin")));

} // constructor

// =================================================================
// Implements IMemInputPin interface
// =================================================================

// If the downstream filter has one then offer that (even if our own output
// pin is not using it yet.  If the upstream filter chooses it then we will
// tell our output pin to ReceiveAllocator).
// Else if our output pin is using an allocator then offer that.
//     ( This could mean offering the upstream filter his own allocator,
//       it could mean offerring our own
//     ) or it could mean offering the one from downstream
// Else fail to offer any allocator at all.

STDMETHODIMP CTransInPlaceInputPin::GetAllocator(__deref_out IMemAllocator **ppAllocator)
{
    CheckPointer(ppAllocator, E_POINTER);
    ValidateReadWritePtr(ppAllocator, sizeof(IMemAllocator *));
    CAutoLock cObjectLock(m_pLock);

    HRESULT hr;

    if (m_pTIPFilter->m_pOutput->IsConnected())
    {
        //  Store the allocator we got
        hr = m_pTIPFilter->OutputPin()->ConnectedIMemInputPin()->GetAllocator(ppAllocator);
        if (SUCCEEDED(hr))
        {
            m_pTIPFilter->OutputPin()->SetAllocator(*ppAllocator);
        }
    }
    else
    {
        //  Help upstream filter (eg TIP filter which is having to do a copy)
        //  by providing a temp allocator here - we'll never use
        //  this allocator because when our output is connected we'll
        //  reconnect this pin
        hr = CTransformInputPin::GetAllocator(ppAllocator);
    }
    return hr;

} // GetAllocator

/* Get told which allocator the upstream output pin is actually going to use */

STDMETHODIMP
CTransInPlaceInputPin::NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly)
{
    HRESULT hr = S_OK;
    CheckPointer(pAllocator, E_POINTER);
    ValidateReadPtr(pAllocator, sizeof(IMemAllocator));

    CAutoLock cObjectLock(m_pLock);

    m_bReadOnly = bReadOnly;
    //  If we modify data then don't accept the allocator if it's
    //  the same as the output pin's allocator

    //  If our output is not connected just accept the allocator
    //  We're never going to use this allocator because when our
    //  output pin is connected we'll reconnect this pin
    if (!m_pTIPFilter->OutputPin()->IsConnected())
    {
        return CTransformInputPin::NotifyAllocator(pAllocator, bReadOnly);
    }

    //  If the allocator is read-only and we're modifying data
    //  and the allocator is the same as the output pin's
    //  then reject
    if (bReadOnly && m_pTIPFilter->m_bModifiesData)
    {
        IMemAllocator *pOutputAllocator = m_pTIPFilter->OutputPin()->PeekAllocator();

        //  Make sure we have an output allocator
        if (pOutputAllocator == NULL)
        {
            hr = m_pTIPFilter->OutputPin()->ConnectedIMemInputPin()->GetAllocator(&pOutputAllocator);
            if (FAILED(hr))
            {
                hr = CreateMemoryAllocator(&pOutputAllocator);
            }
            if (SUCCEEDED(hr))
            {
                m_pTIPFilter->OutputPin()->SetAllocator(pOutputAllocator);
                pOutputAllocator->Release();
            }
        }
        if (pAllocator == pOutputAllocator)
        {
            hr = E_FAIL;
        }
        else if (SUCCEEDED(hr))
        {
            //  Must copy so set the allocator properties on the output
            ALLOCATOR_PROPERTIES Props, Actual;
            hr = pAllocator->GetProperties(&Props);
            if (SUCCEEDED(hr))
            {
                hr = pOutputAllocator->SetProperties(&Props, &Actual);
            }
            if (SUCCEEDED(hr))
            {
                if ((Props.cBuffers > Actual.cBuffers) || (Props.cbBuffer > Actual.cbBuffer) ||
                    (Props.cbAlign > Actual.cbAlign))
                {
                    hr = E_FAIL;
                }
            }

            //  Set the allocator on the output pin
            if (SUCCEEDED(hr))
            {
                hr = m_pTIPFilter->OutputPin()->ConnectedIMemInputPin()->NotifyAllocator(pOutputAllocator, FALSE);
            }
        }
    }
    else
    {
        hr = m_pTIPFilter->OutputPin()->ConnectedIMemInputPin()->NotifyAllocator(pAllocator, bReadOnly);
        if (SUCCEEDED(hr))
        {
            m_pTIPFilter->OutputPin()->SetAllocator(pAllocator);
        }
    }

    if (SUCCEEDED(hr))
    {

        // It's possible that the old and the new are the same thing.
        // AddRef before release ensures that we don't unload it.
        pAllocator->AddRef();

        if (m_pAllocator != NULL)
            m_pAllocator->Release();

        m_pAllocator = pAllocator; // We have an allocator for the input pin
    }

    return hr;

} // NotifyAllocator

// EnumMediaTypes
// - pass through to our downstream filter
STDMETHODIMP CTransInPlaceInputPin::EnumMediaTypes(__deref_out IEnumMediaTypes **ppEnum)
{
    // Can only pass through if connected
    if (!m_pTIPFilter->m_pOutput->IsConnected())
        return VFW_E_NOT_CONNECTED;

    return m_pTIPFilter->m_pOutput->GetConnected()->EnumMediaTypes(ppEnum);

} // EnumMediaTypes

// CheckMediaType
// - agree to anything if not connected,
// otherwise pass through to the downstream filter.
// This assumes that the filter does not change the media type.

HRESULT CTransInPlaceInputPin::CheckMediaType(const CMediaType *pmt)
{
    HRESULT hr = m_pTIPFilter->CheckInputType(pmt);
    if (hr != S_OK)
        return hr;

    if (m_pTIPFilter->m_pOutput->IsConnected())
        return m_pTIPFilter->m_pOutput->GetConnected()->QueryAccept(pmt);
    else
        return S_OK;

} // CheckMediaType

// If upstream asks us what our requirements are, we will try to ask downstream
// if that doesn't work, we'll just take the defaults.
STDMETHODIMP
CTransInPlaceInputPin::GetAllocatorRequirements(__out ALLOCATOR_PROPERTIES *pProps)
{

    if (m_pTIPFilter->m_pOutput->IsConnected())
        return m_pTIPFilter->OutputPin()->ConnectedIMemInputPin()->GetAllocatorRequirements(pProps);
    else
        return E_NOTIMPL;

} // GetAllocatorRequirements

// CTransInPlaceInputPin::CompleteConnect() calls CBaseInputPin::CompleteConnect()
// and then calls CTransInPlaceFilter::CompleteConnect().  It does this because
// CTransInPlaceFilter::CompleteConnect() can reconnect a pin and we do not
// want to reconnect a pin if CBaseInputPin::CompleteConnect() fails.
HRESULT
CTransInPlaceInputPin::CompleteConnect(IPin *pReceivePin)
{
    HRESULT hr = CBaseInputPin::CompleteConnect(pReceivePin);
    if (FAILED(hr))
    {
        return hr;
    }

    return m_pTransformFilter->CompleteConnect(PINDIR_INPUT, pReceivePin);
} // CompleteConnect

// =================================================================
// Implements the CTransInPlaceOutputPin class
// =================================================================

// constructor

CTransInPlaceOutputPin::CTransInPlaceOutputPin(__in_opt LPCTSTR pObjectName, __inout CTransInPlaceFilter *pFilter,
                                               __inout HRESULT *phr, __in_opt LPCWSTR pPinName)
    : CTransformOutputPin(pObjectName, pFilter, phr, pPinName)
    , m_pTIPFilter(pFilter)
{
    DbgLog((LOG_TRACE, 2, TEXT("CTransInPlaceOutputPin::CTransInPlaceOutputPin")));

} // constructor

// EnumMediaTypes
// - pass through to our upstream filter
STDMETHODIMP CTransInPlaceOutputPin::EnumMediaTypes(__deref_out IEnumMediaTypes **ppEnum)
{
    // Can only pass through if connected.
    if (!m_pTIPFilter->m_pInput->IsConnected())
        return VFW_E_NOT_CONNECTED;

    return m_pTIPFilter->m_pInput->GetConnected()->EnumMediaTypes(ppEnum);

} // EnumMediaTypes

// CheckMediaType
// - agree to anything if not connected,
// otherwise pass through to the upstream filter.

HRESULT CTransInPlaceOutputPin::CheckMediaType(const CMediaType *pmt)
{
    // Don't accept any output pin type changes if we're copying
    // between allocators - it's too late to change the input
    // allocator size.
    if (m_pTIPFilter->UsingDifferentAllocators() && !m_pFilter->IsStopped())
    {
        if (*pmt == m_mt)
        {
            return S_OK;
        }
        else
        {
            return VFW_E_TYPE_NOT_ACCEPTED;
        }
    }

    // Assumes the type does not change.  That's why we're calling
    // CheckINPUTType here on the OUTPUT pin.
    HRESULT hr = m_pTIPFilter->CheckInputType(pmt);
    if (hr != S_OK)
        return hr;

    if (m_pTIPFilter->m_pInput->IsConnected())
        return m_pTIPFilter->m_pInput->GetConnected()->QueryAccept(pmt);
    else
        return S_OK;

} // CheckMediaType

/* Save the allocator pointer in the output pin
 */
void CTransInPlaceOutputPin::SetAllocator(IMemAllocator *pAllocator)
{
    pAllocator->AddRef();
    if (m_pAllocator)
    {
        m_pAllocator->Release();
    }
    m_pAllocator = pAllocator;
} // SetAllocator

// CTransInPlaceOutputPin::CompleteConnect() calls CBaseOutputPin::CompleteConnect()
// and then calls CTransInPlaceFilter::CompleteConnect().  It does this because
// CTransInPlaceFilter::CompleteConnect() can reconnect a pin and we do not want to
// reconnect a pin if CBaseOutputPin::CompleteConnect() fails.
// CBaseOutputPin::CompleteConnect() often fails when our output pin is being connected
// to the Video Mixing Renderer.
HRESULT
CTransInPlaceOutputPin::CompleteConnect(IPin *pReceivePin)
{
    HRESULT hr = CBaseOutputPin::CompleteConnect(pReceivePin);
    if (FAILED(hr))
    {
        return hr;
    }

    return m_pTransformFilter->CompleteConnect(PINDIR_OUTPUT, pReceivePin);
} // CompleteConnect
