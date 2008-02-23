#ifndef MPLAYER_SELECT_H
#define MPLAYER_SELECT_H

enum {
    kComponentOpenSelect        = -1,                           /* ComponentInstance for this open */
    kComponentCloseSelect       = -2,                           /* ComponentInstance for this close */
    kComponentCanDoSelect       = -3,                           /* selector # being queried */
    kComponentVersionSelect     = -4,                           /* no params */
    kComponentRegisterSelect    = -5,                           /* no params */
    kComponentTargetSelect      = -6,                           /* ComponentInstance for top of call chain */
    kComponentUnregisterSelect  = -7,                           /* no params */
    kComponentGetMPWorkFunctionSelect = -8,                     /* some params */
    kComponentExecuteWiredActionSelect = -9,                    /* QTAtomContainer actionContainer, QTAtom actionAtom, QTCustomActionTargetPtr target, QTEventRecordPtr event */
    kComponentGetPublicResourceSelect = -10                     /* OSType resourceType, short resourceId, Handle *resource */
};

/* selectors for component calls */
enum {
    kImageCodecGetCodecInfoSelect              = 0x0000,
    kImageCodecGetCompressionTimeSelect        = 0x0001,
    kImageCodecGetMaxCompressionSizeSelect     = 0x0002,
    kImageCodecPreCompressSelect               = 0x0003,
    kImageCodecBandCompressSelect              = 0x0004,
    kImageCodecPreDecompressSelect             = 0x0005,
    kImageCodecBandDecompressSelect            = 0x0006,
    kImageCodecBusySelect                      = 0x0007,
    kImageCodecGetCompressedImageSizeSelect    = 0x0008,
    kImageCodecGetSimilaritySelect             = 0x0009,
    kImageCodecTrimImageSelect                 = 0x000A,
    kImageCodecRequestSettingsSelect           = 0x000B,
    kImageCodecGetSettingsSelect               = 0x000C,
    kImageCodecSetSettingsSelect               = 0x000D,
    kImageCodecFlushSelect                     = 0x000E,
    kImageCodecSetTimeCodeSelect               = 0x000F,
    kImageCodecIsImageDescriptionEquivalentSelect = 0x0010,
    kImageCodecNewMemorySelect                 = 0x0011,
    kImageCodecDisposeMemorySelect             = 0x0012,
    kImageCodecHitTestDataSelect               = 0x0013,
    kImageCodecNewImageBufferMemorySelect      = 0x0014,
    kImageCodecExtractAndCombineFieldsSelect   = 0x0015,
    kImageCodecGetMaxCompressionSizeWithSourcesSelect = 0x0016,
    kImageCodecSetTimeBaseSelect               = 0x0017,
    kImageCodecSourceChangedSelect             = 0x0018,
    kImageCodecFlushFrameSelect                = 0x0019,
    kImageCodecGetSettingsAsTextSelect         = 0x001A,
    kImageCodecGetParameterListHandleSelect    = 0x001B,
    kImageCodecGetParameterListSelect          = 0x001C,
    kImageCodecCreateStandardParameterDialogSelect = 0x001D,
    kImageCodecIsStandardParameterDialogEventSelect = 0x001E,
    kImageCodecDismissStandardParameterDialogSelect = 0x001F,
    kImageCodecStandardParameterDialogDoActionSelect = 0x0020,
    kImageCodecNewImageGWorldSelect            = 0x0021,
    kImageCodecDisposeImageGWorldSelect        = 0x0022,
    kImageCodecHitTestDataWithFlagsSelect      = 0x0023,
    kImageCodecValidateParametersSelect        = 0x0024,
    kImageCodecGetBaseMPWorkFunctionSelect     = 0x0025,
    kImageCodecRequestGammaLevelSelect         = 0x0028,
    kImageCodecGetSourceDataGammaLevelSelect   = 0x0029,
    kImageCodecGetDecompressLatencySelect      = 0x002B,
    kImageCodecPreflightSelect                 = 0x0200,
    kImageCodecInitializeSelect                = 0x0201,
    kImageCodecBeginBandSelect                 = 0x0202,
    kImageCodecDrawBandSelect                  = 0x0203,
    kImageCodecEndBandSelect                   = 0x0204,
    kImageCodecQueueStartingSelect             = 0x0205,
    kImageCodecQueueStoppingSelect             = 0x0206,
    kImageCodecDroppingFrameSelect             = 0x0207,
    kImageCodecScheduleFrameSelect             = 0x0208,
    kImageCodecCancelTriggerSelect             = 0x0209
};

#endif /* MPLAYER_SELECT_H */
