//------------------------------------------------------------------------------
// File: VTrans.h
//
// Desc: DirectShow base classes - defines a video transform class.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

// This class is derived from CTransformFilter, but is specialised to handle
// the requirements of video quality control by frame dropping.
// This is a non-in-place transform, (i.e. it copies the data) such as a decoder.

class CVideoTransformFilter : public CTransformFilter
{
  public:
    CVideoTransformFilter(__in_opt LPCTSTR, __inout_opt LPUNKNOWN, REFCLSID clsid);
    ~CVideoTransformFilter();
    HRESULT EndFlush();

    // =================================================================
    // ----- override these bits ---------------------------------------
    // =================================================================
    // The following methods are in CTransformFilter which is inherited.
    // They are mentioned here for completeness
    //
    // These MUST be supplied in a derived class
    //
    // NOTE:
    // virtual HRESULT Transform(IMediaSample * pIn, IMediaSample *pOut);
    // virtual HRESULT CheckInputType(const CMediaType* mtIn) PURE;
    // virtual HRESULT CheckTransform
    //     (const CMediaType* mtIn, const CMediaType* mtOut) PURE;
    // static CCOMObject * CreateInstance(LPUNKNOWN, HRESULT *);
    // virtual HRESULT DecideBufferSize
    //     (IMemAllocator * pAllocator, ALLOCATOR_PROPERTIES *pprop) PURE;
    // virtual HRESULT GetMediaType(int iPosition, CMediaType *pMediaType) PURE;
    //
    // These MAY also be overridden
    //
    // virtual HRESULT StopStreaming();
    // virtual HRESULT SetMediaType(PIN_DIRECTION direction,const CMediaType *pmt);
    // virtual HRESULT CheckConnect(PIN_DIRECTION dir,IPin *pPin);
    // virtual HRESULT BreakConnect(PIN_DIRECTION dir);
    // virtual HRESULT CompleteConnect(PIN_DIRECTION direction,IPin *pReceivePin);
    // virtual HRESULT EndOfStream(void);
    // virtual HRESULT BeginFlush(void);
    // virtual HRESULT EndFlush(void);
    // virtual HRESULT NewSegment
    //     (REFERENCE_TIME tStart,REFERENCE_TIME tStop,double dRate);
#ifdef PERF

    // If you override this - ensure that you register all these ids
    // as well as any of your own,
    virtual void RegisterPerfId()
    {
        m_idSkip = MSR_REGISTER(TEXT("Video Transform Skip frame"));
        m_idFrameType = MSR_REGISTER(TEXT("Video transform frame type"));
        m_idLate = MSR_REGISTER(TEXT("Video Transform Lateness"));
        m_idTimeTillKey = MSR_REGISTER(TEXT("Video Transform Estd. time to next key"));
        CTransformFilter::RegisterPerfId();
    }
#endif

  protected:
    // =========== QUALITY MANAGEMENT IMPLEMENTATION ========================
    // Frames are assumed to come in three types:
    // Type 1: an AVI key frame or an MPEG I frame.
    //        This frame can be decoded with no history.
    //        Dropping this frame means that no further frame can be decoded
    //        until the next type 1 frame.
    //        Type 1 frames are sync points.
    // Type 2: an AVI non-key frame or an MPEG P frame.
    //        This frame cannot be decoded unless the previous type 1 frame was
    //        decoded and all type 2 frames since have been decoded.
    //        Dropping this frame means that no further frame can be decoded
    //        until the next type 1 frame.
    // Type 3: An MPEG B frame.
    //        This frame cannot be decoded unless the previous type 1 or 2 frame
    //        has been decoded AND the subsequent type 1 or 2 frame has also
    //        been decoded.  (This requires decoding the frames out of sequence).
    //        Dropping this frame affects no other frames.  This implementation
    //        does not allow for these.  All non-sync-point frames are treated
    //        as being type 2.
    //
    // The spacing of frames of type 1 in a file is not guaranteed.  There MUST
    // be a type 1 frame at (well, near) the start of the file in order to start
    // decoding at all.  After that there could be one every half second or so,
    // there could be one at the start of each scene (aka "cut", "shot") or
    // there could be no more at all.
    // If there is only a single type 1 frame then NO FRAMES CAN BE DROPPED
    // without losing all the rest of the movie.  There is no way to tell whether
    // this is the case, so we find that we are in the gambling business.
    // To try to improve the odds, we record the greatest interval between type 1s
    // that we have seen and we bet on things being no worse than this in the
    // future.

    // You can tell if it's a type 1 frame by calling IsSyncPoint().
    // there is no architected way to test for a type 3, so you should override
    // the quality management here if you have B-frames.

    int m_nKeyFramePeriod; // the largest observed interval between type 1 frames
                           // 1 means every frame is type 1, 2 means every other.

    int m_nFramesSinceKeyFrame; // Used to count frames since the last type 1.
                                // becomes the new m_nKeyFramePeriod if greater.

    BOOL m_bSkipping; // we are skipping to the next type 1 frame

#ifdef PERF
    int m_idFrameType;   // MSR id Frame type.  1=Key, 2="non-key"
    int m_idSkip;        // MSR id skipping
    int m_idLate;        // MSR id lateness
    int m_idTimeTillKey; // MSR id for guessed time till next key frame.
#endif

    virtual HRESULT StartStreaming();

    HRESULT AbortPlayback(HRESULT hr); // if something bad happens

    HRESULT Receive(IMediaSample *pSample);

    HRESULT AlterQuality(Quality q);

    BOOL ShouldSkipFrame(IMediaSample *pIn);

    int m_itrLate;      // lateness from last Quality message
                        // (this overflows at 214 secs late).
    int m_tDecodeStart; // timeGetTime when decode started.
    int m_itrAvgDecode; // Average decode time in reference units.

    BOOL m_bNoSkip; // debug - no skipping.

    // We send an EC_QUALITY_CHANGE notification to the app if we have to degrade.
    // We send one when we start degrading, not one for every frame, this means
    // we track whether we've sent one yet.
    BOOL m_bQualityChanged;

    // When non-zero, don't pass anything to renderer until next keyframe
    // If there are few keys, give up and eventually draw something
    int m_nWaitForKey;
};
