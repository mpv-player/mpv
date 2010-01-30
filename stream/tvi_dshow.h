/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_TVI_DSHOW_H
#define MPLAYER_TVI_DSHOW_H

/// \defgroup tvi_dshow TV driver (Directshow)

#define INITGUID
#include <inttypes.h>
#include <windows.h>
//#include <ole2.h>
#include <vfw.h>
#include "loader/dshow/mediatype.h"
#include "loader/dshow/guids.h"

#define wtoa(strW,strA,lenA) WideCharToMultiByte(0,0,strW,-1,strA,lenA,NULL,NULL)
#define atow(strA,strW,lenW) MultiByteToWideChar(0,0,strA,strlen(strA),strW,lenW)

typedef struct DISPPARAMS *LPDISPPARAMS;
typedef struct IFileSinkFilter *LPFILESINKFILTER;
typedef struct IAMCopyCaptureFileProgress *LPAMCOPYCAPTUREFILEPROGRESS;
typedef struct IErrorLog *LPERRORLOG;
typedef struct IAMTunerNotification *LPAMTUNERNOTIFICATION;
typedef struct IFilterGraph *LPFILTERGRAPH;
typedef struct IBaseFilter *LPBASEFILTER;
typedef struct IPin *LPPIN;
typedef struct IEnumPins *LPENUMPINS;
typedef struct IEnumFilters *LPENUMFILTERS;
typedef struct IEnumMediaTypes *LPENUMMEDIATYPES;
typedef struct IReferenceClock *LPREFERENCECLOCK;
typedef struct IMediaSample *LPMEDIASAMPLE;
typedef struct IVideoWindow *LPVIDEOWINDOW;

typedef struct
{
    long cBuffers;
    long cbBuffer;
    long cbAlign;
    long cbPrefix;
}ALLOCATOR_PROPERTIES;

typedef
    enum tagTunerInputType { TunerInputCable = 0,
    TunerInputAntenna = TunerInputCable + 1
} TunerInputType;
typedef enum tagAMTunerModeType {
    AMTUNER_MODE_DEFAULT = 0x0000,
    AMTUNER_MODE_TV = 0x0001,
    AMTUNER_MODE_FM_RADIO = 0x0002,
    AMTUNER_MODE_AM_RADIO = 0x0004,
    AMTUNER_MODE_DSS = 0x0008
} AMTunerModeType;
enum tagAMTunerSubChannel { AMTUNER_SUBCHAN_NO_TUNE = -2,
    AMTUNER_SUBCHAN_DEFAULT = -1
} AMTunerSubChannel;
typedef enum tagVideoProcAmpProperty {
    VideoProcAmp_Brightness,
    VideoProcAmp_Contrast,
    VideoProcAmp_Hue,
    VideoProcAmp_Saturation,
    VideoProcAmp_Sharpness,
    VideoProcAmp_Gamma,
    VideoProcAmp_ColorEnable,
    VideoProcAmp_WhiteBalance,
    VideoProcAmp_BacklightCompensation,
    VideoProcAmp_Gain
} VideoProcAmpProperty;

typedef long OAFilterState;
typedef
    enum tagAnalogVideoStandard { AnalogVideo_None = 0,
    AnalogVideo_NTSC_M = 0x1,
    AnalogVideo_NTSC_M_J = 0x2,
    AnalogVideo_NTSC_433 = 0x4,
    AnalogVideo_PAL_B = 0x10,
    AnalogVideo_PAL_D = 0x20,
    AnalogVideo_PAL_G = 0x40,
    AnalogVideo_PAL_H = 0x80,
    AnalogVideo_PAL_I = 0x100,
    AnalogVideo_PAL_M = 0x200,
    AnalogVideo_PAL_N = 0x400,
    AnalogVideo_PAL_60 = 0x800,
    AnalogVideo_SECAM_B = 0x1000,
    AnalogVideo_SECAM_D = 0x2000,
    AnalogVideo_SECAM_G = 0x4000,
    AnalogVideo_SECAM_H = 0x8000,
    AnalogVideo_SECAM_K = 0x10000,
    AnalogVideo_SECAM_K1 = 0x20000,
    AnalogVideo_SECAM_L = 0x40000,
    AnalogVideo_SECAM_L1 = 0x80000
} AnalogVideoStandard;


typedef LONG_PTR OAHWND;
typedef enum tagPhysicalConnectorType { PhysConn_Video_Tuner = 1,
    PhysConn_Video_Composite = PhysConn_Video_Tuner + 1,
    PhysConn_Video_SVideo = PhysConn_Video_Composite + 1,
    PhysConn_Video_RGB = PhysConn_Video_SVideo + 1,
    PhysConn_Video_YRYBY = PhysConn_Video_RGB + 1,
    PhysConn_Video_SerialDigital = PhysConn_Video_YRYBY + 1,
    PhysConn_Video_ParallelDigital = PhysConn_Video_SerialDigital + 1,
    PhysConn_Video_SCSI = PhysConn_Video_ParallelDigital + 1,
    PhysConn_Video_AUX = PhysConn_Video_SCSI + 1,
    PhysConn_Video_1394 = PhysConn_Video_AUX + 1,
    PhysConn_Video_USB = PhysConn_Video_1394 + 1,
    PhysConn_Video_VideoDecoder = PhysConn_Video_USB + 1,
    PhysConn_Video_VideoEncoder = PhysConn_Video_VideoDecoder + 1,
    PhysConn_Video_SCART = PhysConn_Video_VideoEncoder + 1,
    PhysConn_Video_Black = PhysConn_Video_SCART + 1,
    PhysConn_Audio_Tuner = 0x1000,
    PhysConn_Audio_Line = PhysConn_Audio_Tuner + 1,
    PhysConn_Audio_Mic = PhysConn_Audio_Line + 1,
    PhysConn_Audio_AESDigital = PhysConn_Audio_Mic + 1,
    PhysConn_Audio_SPDIFDigital = PhysConn_Audio_AESDigital + 1,
    PhysConn_Audio_SCSI = PhysConn_Audio_SPDIFDigital + 1,
    PhysConn_Audio_AUX = PhysConn_Audio_SCSI + 1,
    PhysConn_Audio_1394 = PhysConn_Audio_AUX + 1,
    PhysConn_Audio_USB = PhysConn_Audio_1394 + 1,
    PhysConn_Audio_AudioDecoder = PhysConn_Audio_USB + 1
} PhysicalConnectorType;

typedef struct VIDEO_STREAM_CONFIG_CAPS {
    GUID guid;			// will be MEDIATYPE_Video
    ULONG VideoStandard;	// logical OR of all AnalogVideoStandards
    // supported
    SIZE InputSize;		// the inherent size of the incoming signal
    // (every pixel unique)
    SIZE MinCroppingSize;	// smallest rcSrc cropping rect allowed
    SIZE MaxCroppingSize;	// largest rcSrc cropping rect allowed
    int CropGranularityX;	// granularity of cropping size
    int CropGranularityY;
    int CropAlignX;		// alignment of cropping rect
    int CropAlignY;
    SIZE MinOutputSize;		// smallest bitmap stream can produce
    SIZE MaxOutputSize;		// largest  bitmap stream can produce
    int OutputGranularityX;	// granularity of output bitmap size
    int OutputGranularityY;
    int StretchTapsX;		// 0, no stretch, 1 pix dup, 2 interp, ...
    int StretchTapsY;		//    Describes quality of hardware scaler
    int ShrinkTapsX;		//
    int ShrinkTapsY;		//
    LONGLONG MinFrameInterval;	// 100 nS units
    LONGLONG MaxFrameInterval;
    LONG MinBitsPerSecond;
    LONG MaxBitsPerSecond;
} VIDEO_STREAM_CONFIG_CAPS, *PVIDEO_STREAM_CONFIG_CAPS;

typedef struct AUDIO_STREAM_CONFIG_CAPS {
    GUID guid;
    ULONG MinimumChannels;
    ULONG MaximumChannels;
    ULONG ChannelsGranularity;
    ULONG MinimumBitsPerSample;
    ULONG MaximumBitsPerSample;
    ULONG BitsPerSampleGranularity;
    ULONG MinimumSampleFrequency;
    ULONG MaximumSampleFrequency;
    ULONG SampleFrequencyGranularity;
} AUDIO_STREAM_CONFIG_CAPS;

typedef enum tagVideoProcAmpFlags {
    VideoProcAmp_Flags_Auto = 0x0001,
    VideoProcAmp_Flags_Manual = 0x0002
} VideoProcAmpFlags;
typedef enum {
    PINDIR_INPUT = 0,
    PINDIR_OUTPUT
} PIN_DIRECTION;

#define KSPROPERTY_SUPPORT_GET  1
#define KSPROPERTY_SUPPORT_SET  2
typedef struct {
    GUID Set;
    ULONG Id;
    ULONG Flags;
} KSIDENTIFIER;

typedef KSIDENTIFIER KSPROPERTY;


typedef struct {
    KSPROPERTY Property;
    ULONG Mode;			// IN: KSPROPERTY_TUNER_MODE
    ULONG StandardsSupported;	// KS_AnalogVideo_* (if TV or DSS)
    ULONG MinFrequency;		// Hz
    ULONG MaxFrequency;		// Hz
    ULONG TuningGranularity;	// Hz
    ULONG NumberOfInputs;	// count of inputs
    ULONG SettlingTime;		// milliSeconds
    ULONG Strategy;		// KS_TUNER_STRATEGY
} KSPROPERTY_TUNER_MODE_CAPS_S, *PKSPROPERTY_TUNER_MODE_CAPS_S;

typedef struct {
    KSPROPERTY Property;
    ULONG Mode;			// IN: KSPROPERTY_TUNER_MODE
} KSPROPERTY_TUNER_MODE_S, *PKSPROPERTY_TUNER_MODE_S;

typedef struct {
    KSPROPERTY Property;
    ULONG Frequency;		// Hz
    ULONG LastFrequency;	// Hz (last known good)
    ULONG TuningFlags;		// KS_TUNER_TUNING_FLAGS
    ULONG VideoSubChannel;	// DSS
    ULONG AudioSubChannel;	// DSS
    ULONG Channel;		// VBI decoders
    ULONG Country;		// VBI decoders
} KSPROPERTY_TUNER_FREQUENCY_S, *PKSPROPERTY_TUNER_FREQUENCY_S;
typedef struct {
    KSPROPERTY Property;
    ULONG CurrentFrequency;
    ULONG PLLOffset;
    ULONG SignalStrength;
    ULONG Busy;
} KSPROPERTY_TUNER_STATUS_S, *PKSPROPERTY_TUNER_STATUS_S;
typedef enum {
    KS_TUNER_TUNING_EXACT = 1,	// No fine tuning
    KS_TUNER_TUNING_FINE,	// Fine grained search
    KS_TUNER_TUNING_COARSE,	// Coarse search
} KS_TUNER_TUNING_FLAGS;

typedef enum {
    KSPROPERTY_TUNER_CAPS,	// R  -overall device capabilities
    KSPROPERTY_TUNER_MODE_CAPS,	// R  -capabilities in this mode
    KSPROPERTY_TUNER_MODE,	// RW -set a mode (TV, FM, AM, DSS)
    KSPROPERTY_TUNER_STANDARD,	// R  -get TV standard (only if TV mode)
    KSPROPERTY_TUNER_FREQUENCY,	// RW -set/get frequency
    KSPROPERTY_TUNER_INPUT,	// RW -select an input
    KSPROPERTY_TUNER_STATUS,	// R  -tuning status
    KSPROPERTY_TUNER_IF_MEDIUM	// R O-Medium for IF or Transport Pin
} KSPROPERTY_TUNER;
typedef enum {
    KS_TUNER_STRATEGY_PLL = 0X01,	// Tune by PLL offset
    KS_TUNER_STRATEGY_SIGNAL_STRENGTH = 0X02,	// Tune by signal strength
    KS_TUNER_STRATEGY_DRIVER_TUNES = 0X04,	// Driver does fine tuning
} KS_TUNER_STRATEGY;
typedef enum tagTVAudioMode {
    AMTVAUDIO_MODE_MONO = 0x0001,
    AMTVAUDIO_MODE_STEREO = 0x0002,
    AMTVAUDIO_MODE_LANG_A = 0x0010,
    AMTVAUDIO_MODE_LANG_B = 0x0020,
    AMTVAUDIO_MODE_LANG_C = 0x0040,
} TVAudioMode;

typedef struct FilterInfo {
    WCHAR achName[128];
    LPFILTERGRAPH pGraph;
} FILTER_INFO;

typedef struct PinInfo {
    LPBASEFILTER pFilter;
    PIN_DIRECTION dir;
    unsigned short achName[128];
} PIN_INFO;
//-----------------------------------


#undef INTERFACE
#define INTERFACE IPin
DECLARE_INTERFACE(IPin)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(Connect) (THIS_ IPin *, AM_MEDIA_TYPE *);
    STDMETHOD(ReceiveConnection) (THIS_ IPin *, const AM_MEDIA_TYPE *);
    STDMETHOD(Disconnect) (THIS);
    STDMETHOD(ConnectedTo) (THIS_ IPin **);
    STDMETHOD(ConnectionMediaType) (THIS_ AM_MEDIA_TYPE * pmt);
    STDMETHOD(QueryPinInfo) (THIS_ PIN_INFO *);
    STDMETHOD(QueryDirection) (THIS_ PIN_DIRECTION *);
    STDMETHOD(QueryId) (THIS_ unsigned short **);
    STDMETHOD(QueryAccept) (THIS_ const AM_MEDIA_TYPE *);
    STDMETHOD(EnumMediaTypes) (THIS_ LPENUMMEDIATYPES *);
    STDMETHOD(QueryInternalConnections) (THIS_ IPin **, unsigned long *);
    STDMETHOD(EndOfStream) (THIS);
    STDMETHOD(BeginFlush) (THIS);
    STDMETHOD(EndFlush) (THIS);
    STDMETHOD(NewSegment) (THIS_ REFERENCE_TIME, REFERENCE_TIME, double);
};

#undef INTERFACE
#define INTERFACE IBaseFilter
DECLARE_INTERFACE(IBaseFilter)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(GetClassID) (THIS_ CLSID * pClassID);
    STDMETHOD(Stop) (THIS);
    STDMETHOD(Pause) (THIS);
    STDMETHOD(Run) (THIS_ REFERENCE_TIME tStart);
    STDMETHOD(GetState) (THIS_ unsigned long, void *);
    STDMETHOD(SetSyncSource) (THIS_ LPREFERENCECLOCK);
    STDMETHOD(GetSyncSource) (THIS_ LPREFERENCECLOCK *);
    STDMETHOD(EnumPins) (THIS_ LPENUMPINS *);
    STDMETHOD(FindPin) (THIS_ const unsigned short *, LPPIN *);
    STDMETHOD(QueryFilterInfo) (THIS_ void *);
    STDMETHOD(JoinFilterGraph) (THIS_ LPFILTERGRAPH,
				const unsigned short *);
    STDMETHOD(QueryVendorInfo) (THIS_ unsigned short **);
};

#undef INTERFACE
#define INTERFACE IAMTVTuner
DECLARE_INTERFACE(IAMTVTuner)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(put_Channel) (THIS_ long, long, long);
    STDMETHOD(get_Channel) (THIS_ long *, long *, long *);
    STDMETHOD(ChannelMinMax) (THIS_ long *, long *);
    STDMETHOD(put_CountryCode) (THIS_ long);
    STDMETHOD(get_CountryCode) (THIS_ long *);
    STDMETHOD(put_TuningSpace) (THIS_ long);
    STDMETHOD(get_TuningSpace) (THIS_ long *);
    STDMETHOD(Logon) (THIS_ HANDLE);
    STDMETHOD(Logout) (IAMTVTuner *);
    STDMETHOD(SignalPresen) (THIS_ long *);
    STDMETHOD(put_Mode) (THIS_ AMTunerModeType);
    STDMETHOD(get_Mode) (THIS_ AMTunerModeType *);
    STDMETHOD(GetAvailableModes) (THIS_ long *);
    STDMETHOD(RegisterNotificationCallBack) (THIS_ LPAMTUNERNOTIFICATION,
					     long);
    STDMETHOD(UnRegisterNotificationCallBack) (THIS_
					       LPAMTUNERNOTIFICATION);
    STDMETHOD(get_AvailableTVFormats) (THIS_ long *);
    STDMETHOD(get_TVFormat) (THIS_ long *);
    STDMETHOD(AutoTune) (THIS_ long, long *);
    STDMETHOD(StoreAutoTune) (IAMTVTuner *);
    STDMETHOD(get_NumInputConnections) (THIS_ long *);
    STDMETHOD(put_InputType) (THIS_ long, TunerInputType);
    STDMETHOD(get_InputType) (THIS_ long, TunerInputType *);
    STDMETHOD(put_ConnectInput) (THIS_ long);
    STDMETHOD(get_ConnectInput) (THIS_ long *);
    STDMETHOD(get_VideoFrequency) (THIS_ long *);
    STDMETHOD(get_AudioFrequency) (THIS_ long *);
};

#undef INTERFACE
#define INTERFACE IMediaControl
DECLARE_INTERFACE(IMediaControl)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(GetTypeInfoCount) (THIS_ UINT *);
    STDMETHOD(GetTypeInfo) (THIS_ UINT, LCID, LPTYPEINFO *);
    STDMETHOD(GetIDsOfNames) (THIS_ REFIID, LPOLESTR *, UINT, LCID,
			      DISPID *);
    STDMETHOD(Invoke) (THIS_ DISPID, REFIID, LCID, WORD, LPDISPPARAMS,
		       VARIANT *, EXCEPINFO *, UINT *);
    STDMETHOD(Run) (THIS);
    STDMETHOD(Pause) (THIS);
    STDMETHOD(Stop) (THIS);
    STDMETHOD(GetState) (THIS_ LONG, OAFilterState *);
    STDMETHOD(RenderFile) (THIS_ BSTR);
    STDMETHOD(AddSourceFilter) (THIS_ BSTR, LPDISPATCH *);
    STDMETHOD(get_FilterCollection) (THIS_ LPDISPATCH *);
    STDMETHOD(get_RegFilterCollection) (THIS_ LPDISPATCH *);
    STDMETHOD(StopWhenReady) (IMediaControl *);
};

#undef INTERFACE
#define INTERFACE IGraphBuilder
DECLARE_INTERFACE(IGraphBuilder)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(AddFilter) (THIS_ IBaseFilter *, LPCWSTR);
    STDMETHOD(RemoveFilter) (THIS_ IBaseFilter *);
    STDMETHOD(EnumFilters) (THIS_ LPENUMFILTERS *);
    STDMETHOD(FindFilterByName) (THIS_ LPCWSTR, IBaseFilter **);
    STDMETHOD(ConnectDirect) (THIS_ IPin *, IPin *, const AM_MEDIA_TYPE *);
    STDMETHOD(Reconnect) (THIS_ IPin *);
    STDMETHOD(Disconnect) (THIS_ IPin *);
    STDMETHOD(SetDefaultSyncSource) (IGraphBuilder *);
    STDMETHOD(Connect) (THIS_ IPin *, IPin *);
    STDMETHOD(Render) (THIS_ IPin *);
    STDMETHOD(RenderFile) (THIS_ LPCWSTR, LPCWSTR);
    STDMETHOD(AddSourceFilter) (THIS_ LPCWSTR, LPCWSTR, IBaseFilter **);
    STDMETHOD(SetLogFile) (THIS_ DWORD_PTR);
    STDMETHOD(Abort) (IGraphBuilder *);
    STDMETHOD(ShouldOperationContinue) (IGraphBuilder *);
};


#undef INTERFACE
#define INTERFACE ICaptureGraphBuilder2
DECLARE_INTERFACE(ICaptureGraphBuilder2)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(SetFiltergraph) (THIS_ IGraphBuilder *);
    STDMETHOD(GetFiltergraph) (THIS_ IGraphBuilder **);
    STDMETHOD(SetOutputFileName) (THIS_ const GUID *, LPCOLESTR,
				  IBaseFilter **, LPFILESINKFILTER *);
    STDMETHOD(FindInterface) (THIS_ const GUID *, const GUID *,
			      IBaseFilter *, REFIID, void **);
    STDMETHOD(RenderStream) (THIS_ const GUID *, const GUID *, IUnknown *,
			     IBaseFilter *, IBaseFilter *);
    STDMETHOD(ControlStream) (THIS_ const GUID *, const GUID *,
			      IBaseFilter *, REFERENCE_TIME *,
			      REFERENCE_TIME *, WORD, WORD);
    STDMETHOD(AllocCapFile) (THIS_ LPCOLESTR, DWORDLONG);
    STDMETHOD(CopyCaptureFile) (THIS_ LPOLESTR, LPOLESTR, int,
				LPAMCOPYCAPTUREFILEPROGRESS);
    STDMETHOD(FindPin) (THIS_ IUnknown *, PIN_DIRECTION, const GUID *,
			const GUID *, BOOL, int, IPin **);
};

#undef INTERFACE
#define INTERFACE ICreateDevEnum
DECLARE_INTERFACE(ICreateDevEnum)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(CreateClassEnumerator) (THIS_ REFCLSID, IEnumMoniker **,
				      DWORD);
};

#undef INTERFACE
#define INTERFACE IAMCrossbar
DECLARE_INTERFACE(IAMCrossbar)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(get_PinCounts) (THIS_ long *, long *);
    STDMETHOD(CanRoute) (THIS_ long, long);
    STDMETHOD(Route) (THIS_ long, long);
    STDMETHOD(get_IsRoutedTo) (THIS_ long, long *);
    STDMETHOD(get_CrossbarPinInfo) (THIS_ BOOL, long, long *, long *);
};

#ifndef __IPropertyBag_INTERFACE_DEFINED__
#define __IPropertyBag_INTERFACE_DEFINED__
#undef INTERFACE
#define INTERFACE IPropertyBag
DECLARE_INTERFACE(IPropertyBag)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(Read) (THIS_ LPCOLESTR, LPVARIANT, LPERRORLOG);
    STDMETHOD(Write) (THIS_ LPCOLESTR, LPVARIANT);
};
#endif

#undef INTERFACE
#define INTERFACE IAMStreamConfig
DECLARE_INTERFACE(IAMStreamConfig)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    HRESULT(STDMETHODCALLTYPE * SetFormat) (THIS_ AM_MEDIA_TYPE *);
    HRESULT(STDMETHODCALLTYPE * GetFormat) (THIS_ AM_MEDIA_TYPE **);
    HRESULT(STDMETHODCALLTYPE * GetNumberOfCapabilities) (THIS_ int *,int *);
    HRESULT(STDMETHODCALLTYPE * GetStreamCaps) (THIS_ int,AM_MEDIA_TYPE **, BYTE *);
};

#undef INTERFACE
#define INTERFACE IAMVideoProcAmp
DECLARE_INTERFACE(IAMVideoProcAmp)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(GetRange) (THIS_ long, long *, long *, long *, long *,long *);
    STDMETHOD(Set) (THIS_ long, long, long);
    STDMETHOD(Get) (THIS_ long, long *, long *);
};

#undef INTERFACE
#define INTERFACE IKsPropertySet
DECLARE_INTERFACE(IKsPropertySet)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    HRESULT(STDMETHODCALLTYPE * Set) (THIS_ REFGUID, DWORD, LPVOID, DWORD,LPVOID, DWORD);
    HRESULT(STDMETHODCALLTYPE * Get) (THIS_ REFGUID, DWORD, LPVOID, DWORD,LPVOID, DWORD, DWORD *);
    HRESULT(STDMETHODCALLTYPE * QuerySupported) (THIS_ REFGUID, DWORD,DWORD *);
};

#undef INTERFACE
#define INTERFACE IAMAnalogVideoDecoder
DECLARE_INTERFACE(IAMAnalogVideoDecoder)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(get_AvailableTVFormats) (THIS_ long *);
    STDMETHOD(put_TVFormat) (THIS_ long);
    STDMETHOD(get_TVFormat) (THIS_ long *);
    STDMETHOD(get_HorizontalLocked) (THIS_ long *);
    STDMETHOD(put_VCRHorizontalLocking) (THIS_ long);
    STDMETHOD(get_VCRHorizontalLocking) (THIS_ long *);
    STDMETHOD(get_NumberOfLines) (THIS_ long *);
    STDMETHOD(put_OutputEnable) (THIS_ long);
    STDMETHOD(get_OutputEnable) (THIS_ long *);
};

#undef INTERFACE
#define INTERFACE IAMTVAudio
DECLARE_INTERFACE(IAMTVAudio)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(GetHardwareSupportedTVAudioModes) (THIS_ long *);
    STDMETHOD(GetAvailableTVAudioModes) (THIS_ long *);
    STDMETHOD(get_TVAudioMode) (THIS_ long *);
    STDMETHOD(put_TVAudioMode) (THIS_ long);
    STDMETHOD(RegisterNotificationCallBack) (THIS_ LPAMTUNERNOTIFICATION,
					     long);
    STDMETHOD(UnRegisterNotificationCallBack) (THIS_
					       LPAMTUNERNOTIFICATION);
};


#undef INTERFACE
#define INTERFACE ISampleGrabberCB
DECLARE_INTERFACE(ISampleGrabberCB)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(SampleCB) (THIS_ double, LPMEDIASAMPLE);
    STDMETHOD(BufferCB) (THIS_ double, BYTE *, long);
};

#undef INTERFACE
#define INTERFACE ISampleGrabber
DECLARE_INTERFACE(ISampleGrabber)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(SetOneShot) (THIS_ BOOL);
    STDMETHOD(SetMediaType) (THIS_ const AM_MEDIA_TYPE *);
    STDMETHOD(GetConnectedMediaType) (THIS_ AM_MEDIA_TYPE *);
    STDMETHOD(SetBufferSamples) (THIS_ BOOL);
    STDMETHOD(GetCurrentBuffer) (THIS_ long *, long *);
    STDMETHOD(GetCurrentSample) (THIS_ LPMEDIASAMPLE *);
    STDMETHOD(SetCallback) (THIS_ ISampleGrabberCB *, long);
};

#undef INTERFACE
#define INTERFACE IFilterGraph
DECLARE_INTERFACE(IFilterGraph)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(AddFilter) (THIS_ LPBASEFILTER, LPCWSTR);
    STDMETHOD(RemoveFilter) (THIS_ LPBASEFILTER);
    STDMETHOD(EnumFilters) (THIS_ LPENUMFILTERS *);
    STDMETHOD(FindFilterByName) (THIS_ LPCWSTR, LPBASEFILTER *);
    STDMETHOD(ConnectDirect) (THIS_ IPin *, IPin *, const AM_MEDIA_TYPE *);
    STDMETHOD(Reconnect) (THIS_ LPPIN);
    STDMETHOD(Disconnect) (THIS_ LPPIN);
    STDMETHOD(SetDefaultSyncSource) (THIS);
};

#undef INTERFACE
#define INTERFACE IAMAudioInputMixer
DECLARE_INTERFACE(IAMAudioInputMixer)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(put_Enable) (THIS_ BOOL);
    STDMETHOD(get_Enable) (THIS_ BOOL *);
    STDMETHOD(put_Mono) (THIS_ BOOL);
    STDMETHOD(get_Mono) (THIS_ BOOL *);
    STDMETHOD(put_MixLevel) (THIS_ double);
    STDMETHOD(get_MixLevel) (THIS_ double *);
    STDMETHOD(put_Pan) (THIS_ double);
    STDMETHOD(get_Pan) (THIS_ double *);
    STDMETHOD(put_Loudness) (THIS_ BOOL);
    STDMETHOD(get_Loudness) (THIS_ BOOL *);
    STDMETHOD(put_Treble) (THIS_ double);
    STDMETHOD(get_Treble) (THIS_ double *);
    STDMETHOD(get_TrebleRange) (THIS_ double *);
    STDMETHOD(put_Bass) (THIS_ double);
    STDMETHOD(get_Bass) (THIS_ double *);
    STDMETHOD(get_BassRange) (THIS_ double *);
};


#undef INTERFACE
#define INTERFACE IMediaSample
DECLARE_INTERFACE(IMediaSample)
{
    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **);
    STDMETHOD_(long, AddRef) (THIS);
    STDMETHOD_(long, Release) (THIS);
    STDMETHOD(GetPointer )(THIS_ unsigned char** );
    STDMETHOD_(LONG,GetSize )(THIS);
    STDMETHOD(GetTime )(THIS_ REFERENCE_TIME* ,REFERENCE_TIME* );
    STDMETHOD(SetTime )(THIS_ REFERENCE_TIME* ,REFERENCE_TIME* );
    STDMETHOD(IsSyncPoint )(THIS);
    STDMETHOD(SetSyncPoint )(THIS_ long );
    STDMETHOD(IsPreroll )(THIS);
    STDMETHOD(SetPreroll )(THIS_ long );
    STDMETHOD_(LONG,GetActualDataLength)(THIS);
    STDMETHOD(SetActualDataLength )(THIS_ long );
    STDMETHOD(GetMediaType )(THIS_ AM_MEDIA_TYPE** );
    STDMETHOD(SetMediaType )(THIS_ AM_MEDIA_TYPE* );
    STDMETHOD(IsDiscontinuity )(THIS);
    STDMETHOD(SetDiscontinuity )(THIS_ long );
    STDMETHOD(GetMediaTime )(THIS_ long long* ,long long* );
    STDMETHOD(SetMediaTime )(THIS_ long long* ,long long* );
};


#undef INTERFACE
#define INTERFACE IAMBufferNegotiation
DECLARE_INTERFACE(IAMBufferNegotiation)
{
    STDMETHOD(QueryInterface )(THIS_ REFIID ,void **);
    STDMETHOD_(ULONG,AddRef )(THIS);
    STDMETHOD_(ULONG,Release )(THIS);
    STDMETHOD(SuggestAllocatorProperties )(THIS_ const ALLOCATOR_PROPERTIES *);
    STDMETHOD(GetAllocatorProperties )(THIS_ ALLOCATOR_PROPERTIES *);
};


#undef INTERFACE
#define INTERFACE IVideoWindow
DECLARE_INTERFACE(IVideoWindow)
{
    STDMETHOD(QueryInterface )(THIS_ REFIID ,void **);
    STDMETHOD_(ULONG,AddRef )(THIS);
    STDMETHOD_(ULONG,Release )(THIS);
    STDMETHOD(GetTypeInfoCount) (THIS_ UINT * );
    STDMETHOD(GetTypeInfo) (THIS_  UINT ,LCID , ITypeInfo ** );
    STDMETHOD(GetIDsOfNames) (THIS_  REFIID ,LPOLESTR * , UINT ,LCID , DISPID * );
    STDMETHOD(Invoke) (THIS_  DISPID ,REFIID , LCID , WORD ,void *, VARIANT * ,EXCEPINFO * , UINT * );
    STDMETHOD(put_Caption) (THIS_  BSTR );
    STDMETHOD(get_Caption) (THIS_  BSTR * );
    STDMETHOD(put_WindowStyle) (THIS_ long );
    STDMETHOD(get_WindowStyle) (THIS_ long *);
    STDMETHOD(put_WindowStyleEx) (THIS_ long );
    STDMETHOD(get_WindowStyleEx) (THIS_ long *);
    STDMETHOD(put_AutoShow) (THIS_  long );
    STDMETHOD(get_AutoShow) (THIS_  long *);
    STDMETHOD(put_WindowState) (THIS_ long );
    STDMETHOD(get_WindowState) (THIS_ long *);
    STDMETHOD(put_BackgroundPalette) (THIS_ long );
    STDMETHOD(get_BackgroundPalette) (THIS_ long *);
    STDMETHOD(put_Visible) (THIS_  long );
    STDMETHOD(get_Visible) (THIS_  long *);
    STDMETHOD(put_Left) (THIS_  long );
    STDMETHOD(get_Left) (THIS_  long *);
    STDMETHOD(put_Width) (THIS_  long );
    STDMETHOD(get_Width) (THIS_  long *);
    STDMETHOD(put_Top) (THIS_  long );
    STDMETHOD(get_Top) (THIS_  long *);
    STDMETHOD(put_Height) (THIS_  long );
    STDMETHOD(get_Height) (THIS_  long *);
    STDMETHOD(put_Owner) (THIS_  OAHWND );
    STDMETHOD(get_Owner) (THIS_  OAHWND * );
    STDMETHOD(put_MessageDrain) (THIS_  OAHWND );
    STDMETHOD(get_MessageDrain) (THIS_ OAHWND * );
    STDMETHOD(get_BorderColor) (THIS_  long *);
    STDMETHOD(put_BorderColor) (THIS_  long );
    STDMETHOD(get_FullScreenMode) (THIS_ long *);
    STDMETHOD(put_FullScreenMode) (THIS_ long );
    STDMETHOD(SetWindowForeground) (THIS_ long );
    STDMETHOD(NotifyOwnerMessage) (THIS_  OAHWND ,long , LONG_PTR ,LONG_PTR );
    STDMETHOD(SetWindowPosition) (THIS_  long ,long , long ,long );
    STDMETHOD(GetWindowPosition) (THIS_  long *,long *, long *,long *);
    STDMETHOD(GetMinIdealImageSize) (THIS_ long *, long *);
    STDMETHOD(GetMaxIdealImageSize) (THIS_ long *, long *);
    STDMETHOD(GetRestorePosition) (THIS_  long *,long *, long *,long *);
    STDMETHOD(HideCursor) (THIS_  long );
    STDMETHOD(IsCursorHidden) (THIS_ long *);
};

#ifndef DECLARE_ENUMERATOR_
#define DECLARE_ENUMERATOR_(I,T) \
    DECLARE_INTERFACE_(I,IUnknown) \
    { \
        STDMETHOD(QueryInterface)(I*, REFIID,PVOID*); \
        STDMETHOD_(ULONG,AddRef)(I*); \
        STDMETHOD_(ULONG,Release)(I*); \
        STDMETHOD(Next)(I*, ULONG,T*,ULONG*); \
        STDMETHOD(Skip)(I*, ULONG); \
        STDMETHOD(Reset)(I*); \
        STDMETHOD(Clone)(I*, I**); \
    }
#endif
DECLARE_ENUMERATOR_(IEnumFilters, LPBASEFILTER);
DECLARE_ENUMERATOR_(IEnumPins, LPPIN);
DECLARE_ENUMERATOR_(IEnumMediaTypes, AM_MEDIA_TYPE *);

#define OLE_CALL(p,method)               (p)->lpVtbl->method(p)
#ifdef __GNUC__
#define OLE_CALL_ARGS(p, method, a1, args...) (p)->lpVtbl->method(p, a1, ##args)
#else
#define OLE_CALL_ARGS(p, method, ...) (p)->lpVtbl->method(p, __VA_ARGS__)
#endif
#define OLE_RELEASE_SAFE(p) if(p){ OLE_CALL((IUnknown*)p,Release); p=NULL;}
#define OLE_QUERYINTERFACE(p,iface,ptr) OLE_CALL_ARGS((IUnknown*)p,QueryInterface,&iface,(void*)&ptr)

#endif	/* MPLAYER_TVI_DSHOW_H */
