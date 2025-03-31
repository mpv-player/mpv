//------------------------------------------------------------------------------
// File: Vtrans.cpp
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <measure.h>
// #include <vtransfr.h>         // now in precomp file streams.h

CVideoTransformFilter::CVideoTransformFilter(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, REFCLSID clsid)
    : CTransformFilter(pName, pUnk, clsid)
    , m_itrLate(0)
    , m_nKeyFramePeriod(0) // No QM until we see at least 2 key frames
    , m_nFramesSinceKeyFrame(0)
    , m_bSkipping(FALSE)
    , m_tDecodeStart(0)
    , m_itrAvgDecode(300000) // 30mSec - probably allows skipping
    , m_bQualityChanged(FALSE)
{
#ifdef PERF
    RegisterPerfId();
#endif //  PERF
}

CVideoTransformFilter::~CVideoTransformFilter()
{
    // nothing to do
}

// Reset our quality management state

HRESULT CVideoTransformFilter::StartStreaming()
{
    m_itrLate = 0;
    m_nKeyFramePeriod = 0; // No QM until we see at least 2 key frames
    m_nFramesSinceKeyFrame = 0;
    m_bSkipping = FALSE;
    m_tDecodeStart = 0;
    m_itrAvgDecode = 300000; // 30mSec - probably allows skipping
    m_bQualityChanged = FALSE;
    m_bSampleSkipped = FALSE;
    return NOERROR;
}

// Overriden to reset quality management information

HRESULT CVideoTransformFilter::EndFlush()
{
    {
        //  Synchronize
        CAutoLock lck(&m_csReceive);

        // Reset our stats
        //
        // Note - we don't want to call derived classes here,
        // we only want to reset our internal variables and this
        // is a convenient way to do it
        CVideoTransformFilter::StartStreaming();
    }
    return CTransformFilter::EndFlush();
}

HRESULT CVideoTransformFilter::AbortPlayback(HRESULT hr)
{
    NotifyEvent(EC_ERRORABORT, hr, 0);
    m_pOutput->DeliverEndOfStream();
    return hr;
}

// Receive()
//
// Accept a sample from upstream, decide whether to process it
// or drop it.  If we process it then get a buffer from the
// allocator of the downstream connection, transform it into the
// new buffer and deliver it to the downstream filter.
// If we decide not to process it then we do not get a buffer.

// Remember that although this code will notice format changes coming into
// the input pin, it will NOT change its output format if that results
// in the filter needing to make a corresponding output format change.  Your
// derived filter will have to take care of that.  (eg. a palette change if
// the input and output is an 8 bit format).  If the input sample is discarded
// and nothing is sent out for this Receive, please remember to put the format
// change on the first output sample that you actually do send.
// If your filter will produce the same output type even when the input type
// changes, then this base class code will do everything you need.

HRESULT CVideoTransformFilter::Receive(IMediaSample *pSample)
{
    // If the next filter downstream is the video renderer, then it may
    // be able to operate in DirectDraw mode which saves copying the data
    // and gives higher performance.  In that case the buffer which we
    // get from GetDeliveryBuffer will be a DirectDraw buffer, and
    // drawing into this buffer draws directly onto the display surface.
    // This means that any waiting for the correct time to draw occurs
    // during GetDeliveryBuffer, and that once the buffer is given to us
    // the video renderer will count it in its statistics as a frame drawn.
    // This means that any decision to drop the frame must be taken before
    // calling GetDeliveryBuffer.

    ASSERT(CritCheckIn(&m_csReceive));
    AM_MEDIA_TYPE *pmtOut, *pmt;
#ifdef DEBUG
    FOURCCMap fccOut;
#endif
    HRESULT hr;
    ASSERT(pSample);
    IMediaSample *pOutSample;

    // If no output pin to deliver to then no point sending us data
    ASSERT(m_pOutput != NULL);

    // The source filter may dynamically ask us to start transforming from a
    // different media type than the one we're using now.  If we don't, we'll
    // draw garbage. (typically, this is a palette change in the movie,
    // but could be something more sinister like the compression type changing,
    // or even the video size changing)

#define rcS1 ((VIDEOINFOHEADER *)(pmt->pbFormat))->rcSource
#define rcT1 ((VIDEOINFOHEADER *)(pmt->pbFormat))->rcTarget

    pSample->GetMediaType(&pmt);
    if (pmt != NULL && pmt->pbFormat != NULL)
    {

        // spew some debug output
        ASSERT(!IsEqualGUID(pmt->majortype, GUID_NULL));
#ifdef DEBUG
        fccOut.SetFOURCC(&pmt->subtype);
        LONG lCompression = HEADER(pmt->pbFormat)->biCompression;
        LONG lBitCount = HEADER(pmt->pbFormat)->biBitCount;
        LONG lStride = (HEADER(pmt->pbFormat)->biWidth * lBitCount + 7) / 8;
        lStride = (lStride + 3) & ~3;
        DbgLog((LOG_TRACE, 3, TEXT("*Changing input type on the fly to")));
        DbgLog((LOG_TRACE, 3, TEXT("FourCC: %lx Compression: %lx BitCount: %ld"), fccOut.GetFOURCC(), lCompression,
                lBitCount));
        DbgLog((LOG_TRACE, 3, TEXT("biHeight: %ld rcDst: (%ld, %ld, %ld, %ld)"), HEADER(pmt->pbFormat)->biHeight,
                rcT1.left, rcT1.top, rcT1.right, rcT1.bottom));
        DbgLog((LOG_TRACE, 3, TEXT("rcSrc: (%ld, %ld, %ld, %ld) Stride: %ld"), rcS1.left, rcS1.top, rcS1.right,
                rcS1.bottom, lStride));
#endif

        // now switch to using the new format.  I am assuming that the
        // derived filter will do the right thing when its media type is
        // switched and streaming is restarted.

        StopStreaming();
        m_pInput->CurrentMediaType() = *pmt;
        DeleteMediaType(pmt);
        // if this fails, playback will stop, so signal an error
        hr = StartStreaming();
        if (FAILED(hr))
        {
            return AbortPlayback(hr);
        }
    }

    // Now that we have noticed any format changes on the input sample, it's
    // OK to discard it.

    if (ShouldSkipFrame(pSample))
    {
        MSR_NOTE(m_idSkip);
        m_bSampleSkipped = TRUE;
        return NOERROR;
    }

    // Set up the output sample
    hr = InitializeOutputSample(pSample, &pOutSample);

    if (FAILED(hr))
    {
        return hr;
    }

    m_bSampleSkipped = FALSE;

    // The renderer may ask us to on-the-fly to start transforming to a
    // different format.  If we don't obey it, we'll draw garbage

#define rcS ((VIDEOINFOHEADER *)(pmtOut->pbFormat))->rcSource
#define rcT ((VIDEOINFOHEADER *)(pmtOut->pbFormat))->rcTarget

    pOutSample->GetMediaType(&pmtOut);
    if (pmtOut != NULL && pmtOut->pbFormat != NULL)
    {

        // spew some debug output
        ASSERT(!IsEqualGUID(pmtOut->majortype, GUID_NULL));
#ifdef DEBUG
        fccOut.SetFOURCC(&pmtOut->subtype);
        LONG lCompression = HEADER(pmtOut->pbFormat)->biCompression;
        LONG lBitCount = HEADER(pmtOut->pbFormat)->biBitCount;
        LONG lStride = (HEADER(pmtOut->pbFormat)->biWidth * lBitCount + 7) / 8;
        lStride = (lStride + 3) & ~3;
        DbgLog((LOG_TRACE, 3, TEXT("*Changing output type on the fly to")));
        DbgLog((LOG_TRACE, 3, TEXT("FourCC: %lx Compression: %lx BitCount: %ld"), fccOut.GetFOURCC(), lCompression,
                lBitCount));
        DbgLog((LOG_TRACE, 3, TEXT("biHeight: %ld rcDst: (%ld, %ld, %ld, %ld)"), HEADER(pmtOut->pbFormat)->biHeight,
                rcT.left, rcT.top, rcT.right, rcT.bottom));
        DbgLog((LOG_TRACE, 3, TEXT("rcSrc: (%ld, %ld, %ld, %ld) Stride: %ld"), rcS.left, rcS.top, rcS.right, rcS.bottom,
                lStride));
#endif

        // now switch to using the new format.  I am assuming that the
        // derived filter will do the right thing when its media type is
        // switched and streaming is restarted.

        StopStreaming();
        m_pOutput->CurrentMediaType() = *pmtOut;
        DeleteMediaType(pmtOut);
        hr = StartStreaming();

        if (SUCCEEDED(hr))
        {
            // a new format, means a new empty buffer, so wait for a keyframe
            // before passing anything on to the renderer.
            // !!! a keyframe may never come, so give up after 30 frames
            DbgLog((LOG_TRACE, 3, TEXT("Output format change means we must wait for a keyframe")));
            m_nWaitForKey = 30;

            // if this fails, playback will stop, so signal an error
        }
        else
        {

            //  Must release the sample before calling AbortPlayback
            //  because we might be holding the win16 lock or
            //  ddraw lock
            pOutSample->Release();
            AbortPlayback(hr);
            return hr;
        }
    }

    // After a discontinuity, we need to wait for the next key frame
    if (pSample->IsDiscontinuity() == S_OK)
    {
        DbgLog((LOG_TRACE, 3, TEXT("Non-key discontinuity - wait for keyframe")));
        m_nWaitForKey = 30;
    }

    // Start timing the transform (and log it if PERF is defined)

    if (SUCCEEDED(hr))
    {
        m_tDecodeStart = timeGetTime();
        MSR_START(m_idTransform);

        // have the derived class transform the data
        hr = Transform(pSample, pOutSample);

        // Stop the clock (and log it if PERF is defined)
        MSR_STOP(m_idTransform);
        m_tDecodeStart = timeGetTime() - m_tDecodeStart;
        m_itrAvgDecode = m_tDecodeStart * (10000 / 16) + 15 * (m_itrAvgDecode / 16);

        // Maybe we're waiting for a keyframe still?
        if (m_nWaitForKey)
            m_nWaitForKey--;
        if (m_nWaitForKey && pSample->IsSyncPoint() == S_OK)
            m_nWaitForKey = FALSE;

        // if so, then we don't want to pass this on to the renderer
        if (m_nWaitForKey && hr == NOERROR)
        {
            DbgLog((LOG_TRACE, 3, TEXT("still waiting for a keyframe")));
            hr = S_FALSE;
        }
    }

    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 1, TEXT("Error from video transform")));
    }
    else
    {
        // the Transform() function can return S_FALSE to indicate that the
        // sample should not be delivered; we only deliver the sample if it's
        // really S_OK (same as NOERROR, of course.)
        // Try not to return S_FALSE to a direct draw buffer (it's wasteful)
        // Try to take the decision earlier - before you get it.

        if (hr == NOERROR)
        {
            hr = m_pOutput->Deliver(pOutSample);
        }
        else
        {
            // S_FALSE returned from Transform is a PRIVATE agreement
            // We should return NOERROR from Receive() in this case because returning S_FALSE
            // from Receive() means that this is the end of the stream and no more data should
            // be sent.
            if (S_FALSE == hr)
            {

                //  We must Release() the sample before doing anything
                //  like calling the filter graph because having the
                //  sample means we may have the DirectDraw lock
                //  (== win16 lock on some versions)
                pOutSample->Release();
                m_bSampleSkipped = TRUE;
                if (!m_bQualityChanged)
                {
                    m_bQualityChanged = TRUE;
                    NotifyEvent(EC_QUALITY_CHANGE, 0, 0);
                }
                return NOERROR;
            }
        }
    }

    // release the output buffer. If the connected pin still needs it,
    // it will have addrefed it itself.
    pOutSample->Release();
    ASSERT(CritCheckIn(&m_csReceive));

    return hr;
}

BOOL CVideoTransformFilter::ShouldSkipFrame(IMediaSample *pIn)
{
    REFERENCE_TIME trStart, trStopAt;
    HRESULT hr = pIn->GetTime(&trStart, &trStopAt);

    // Don't skip frames with no timestamps
    if (hr != S_OK)
        return FALSE;

    int itrFrame = (int)(trStopAt - trStart); // frame duration

    if (S_OK == pIn->IsSyncPoint())
    {
        MSR_INTEGER(m_idFrameType, 1);
        if (m_nKeyFramePeriod < m_nFramesSinceKeyFrame)
        {
            // record the max
            m_nKeyFramePeriod = m_nFramesSinceKeyFrame;
        }
        m_nFramesSinceKeyFrame = 0;
        m_bSkipping = FALSE;
    }
    else
    {
        MSR_INTEGER(m_idFrameType, 2);
        if (m_nFramesSinceKeyFrame > m_nKeyFramePeriod && m_nKeyFramePeriod > 0)
        {
            // We haven't seen the key frame yet, but we were clearly being
            // overoptimistic about how frequent they are.
            m_nKeyFramePeriod = m_nFramesSinceKeyFrame;
        }
    }

    // Whatever we might otherwise decide,
    // if we are taking only a small fraction of the required frame time to decode
    // then any quality problems are actually coming from somewhere else.
    // Could be a net problem at the source for instance.  In this case there's
    // no point in us skipping frames here.
    if (m_itrAvgDecode * 4 > itrFrame)
    {

        // Don't skip unless we are at least a whole frame late.
        // (We would skip B frames if more than 1/2 frame late, but they're safe).
        if (m_itrLate > itrFrame)
        {

            // Don't skip unless the anticipated key frame would be no more than
            // 1 frame early.  If the renderer has not been waiting (we *guess*
            // it hasn't because we're late) then it will allow frames to be
            // played early by up to a frame.

            // Let T = Stream time from now to anticipated next key frame
            // = (frame duration) * (KeyFramePeriod - FramesSinceKeyFrame)
            // So we skip if T - Late < one frame  i.e.
            //   (duration) * (freq - FramesSince) - Late < duration
            // or (duration) * (freq - FramesSince - 1) < Late

            // We don't dare skip until we have seen some key frames and have
            // some idea how often they occur and they are reasonably frequent.
            if (m_nKeyFramePeriod > 0)
            {
                // It would be crazy - but we could have a stream with key frames
                // a very long way apart - and if they are further than about
                // 3.5 minutes apart then we could get arithmetic overflow in
                // reference time units.  Therefore we switch to mSec at this point
                int it = (itrFrame / 10000) * (m_nKeyFramePeriod - m_nFramesSinceKeyFrame - 1);
                MSR_INTEGER(m_idTimeTillKey, it);

                // For debug - might want to see the details - dump them as scratch pad
#ifdef VTRANSPERF
                MSR_INTEGER(0, itrFrame);
                MSR_INTEGER(0, m_nFramesSinceKeyFrame);
                MSR_INTEGER(0, m_nKeyFramePeriod);
#endif
                if (m_itrLate / 10000 > it)
                {
                    m_bSkipping = TRUE;
                    // Now we are committed.  Once we start skipping, we
                    // cannot stop until we hit a key frame.
                }
                else
                {
#ifdef VTRANSPERF
                    MSR_INTEGER(0, 777770); // not near enough to next key
#endif
                }
            }
            else
            {
#ifdef VTRANSPERF
                MSR_INTEGER(0, 777771); // Next key not predictable
#endif
            }
        }
        else
        {
#ifdef VTRANSPERF
            MSR_INTEGER(0, 777772); // Less than one frame late
            MSR_INTEGER(0, m_itrLate);
            MSR_INTEGER(0, itrFrame);
#endif
        }
    }
    else
    {
#ifdef VTRANSPERF
        MSR_INTEGER(0, 777773); // Decode time short - not not worth skipping
        MSR_INTEGER(0, m_itrAvgDecode);
        MSR_INTEGER(0, itrFrame);
#endif
    }

    ++m_nFramesSinceKeyFrame;

    if (m_bSkipping)
    {
        // We will count down the lateness as we skip each frame.
        // We re-assess each frame.  The key frame might not arrive when expected.
        // We reset m_itrLate if we get a new Quality message, but actually that's
        // not likely because we're not sending frames on to the Renderer.  In
        // fact if we DID get another one it would mean that there's a long
        // pipe between us and the renderer and we might need an altogether
        // better strategy to avoid hunting!
        m_itrLate = m_itrLate - itrFrame;
    }

    MSR_INTEGER(m_idLate, (int)m_itrLate / 10000); // Note how late we think we are
    if (m_bSkipping)
    {
        if (!m_bQualityChanged)
        {
            m_bQualityChanged = TRUE;
            NotifyEvent(EC_QUALITY_CHANGE, 0, 0);
        }
    }
    return m_bSkipping;
}

HRESULT CVideoTransformFilter::AlterQuality(Quality q)
{
    // to reduce the amount of 64 bit arithmetic, m_itrLate is an int.
    // +, -, >, == etc  are not too bad, but * and / are painful.
    if (m_itrLate > 300000000)
    {
        // Avoid overflow and silliness - more than 30 secs late is already silly
        m_itrLate = 300000000;
    }
    else
    {
        m_itrLate = (int)q.Late;
    }
    // We ignore the other fields

    // We're actually not very good at handling this.  In non-direct draw mode
    // most of the time can be spent in the renderer which can skip any frame.
    // In that case we'd rather the renderer handled things.
    // Nevertheless we will keep an eye on it and if we really start getting
    // a very long way behind then we will actually skip - but we'll still tell
    // the renderer (or whoever is downstream) that they should handle quality.

    return E_FAIL; // Tell the renderer to do his thing.
}

// This will avoid several hundred useless warnings if compiled -W4 by MS VC++ v4
#pragma warning(disable : 4514)
