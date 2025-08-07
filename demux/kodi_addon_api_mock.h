#pragma once

#include <stdint.h>
#include <stdbool.h>

#define INPUTSTREAM_MAX_STREAM_COUNT 256
#define STREAM_MAX_PROPERTY_COUNT 100 // A reasonable guess

// Forward declaration
struct AddonInstance_InputStream;
struct DEMUX_CRYPTO_INFO;

typedef void* KODI_HANDLE;

typedef struct DEMUX_PACKET {
    uint8_t* pData;
    int iSize;
    int iStreamId;
    int64_t demuxerId;
    int iGroupId;
    void* pSideData;
    int iSideDataElems;
    double pts;
    double dts;
    double duration;
    int dispTime;
    bool recoveryPoint;
    struct DEMUX_CRYPTO_INFO* cryptoInfo;
} DEMUX_PACKET;

typedef struct DEMUX_CRYPTO_INFO {
    // Add fields here based on actual definition
    // For now, it's just a placeholder
    int dummy;
} DEMUX_CRYPTO_INFO;

typedef struct LISTITEMPROPERTY {
    const char* m_strKey;
    const char* m_strValue;
} LISTITEMPROPERTY;

typedef struct INPUTSTREAM_PROPERTY {
    const char* m_strURL;
    const char* m_mimeType;
    unsigned int m_nCountInfoValues;
    LISTITEMPROPERTY m_ListItemProperties[STREAM_MAX_PROPERTY_COUNT];
    const char* m_libFolder;
    const char* m_profileFolder;
} INPUTSTREAM_PROPERTY;

typedef struct AddonToKodiFuncTable_InputStream {
    KODI_HANDLE kodiInstance;
    DEMUX_PACKET* (*allocate_demux_packet)(void* kodiInstance, int data_size);
    void (*free_demux_packet)(void* kodiInstance, DEMUX_PACKET* packet);
} AddonToKodiFuncTable_InputStream;

typedef struct KodiToAddonFuncTable_InputStream {
    KODI_HANDLE addonInstance;
    bool (*open)(const struct AddonInstance_InputStream* instance, INPUTSTREAM_PROPERTY* props);
    void (*close)(const struct AddonInstance_InputStream* instance);
    bool (*get_stream_ids)(const struct AddonInstance_InputStream* instance, struct INPUTSTREAM_IDS* ids);
    bool (*get_stream)(const struct AddonInstance_InputStream* instance, int streamid, struct INPUTSTREAM_INFO* info, KODI_HANDLE* demuxStream, KODI_HANDLE (*transfer_stream)(KODI_HANDLE handle, int streamId, struct INPUTSTREAM_INFO* stream));
    void (*enable_stream)(const struct AddonInstance_InputStream* instance, int streamid, bool enable);
    bool (*open_stream)(const struct AddonInstance_InputStream* instance, int streamid);
    void (*demux_reset)(const struct AddonInstance_InputStream* instance);
    void (*demux_abort)(const struct AddonInstance_InputStream* instance);
    void (*demux_flush)(const struct AddonInstance_InputStream* instance);
    DEMUX_PACKET* (*demux_read)(const struct AddonInstance_InputStream* instance);
    bool (*demux_seek_time)(const struct AddonInstance_InputStream* instance, double time, bool backwards, double* startpts);
    void (*demux_set_speed)(const struct AddonInstance_InputStream* instance, int speed);
} KodiToAddonFuncTable_InputStream;

typedef struct AddonInstance_InputStream {
    void* props; // AddonProps_InputStream
    AddonToKodiFuncTable_InputStream* toKodi;
    KodiToAddonFuncTable_InputStream* toAddon;
} AddonInstance_InputStream;

typedef void (*addon_log_msg_fn)(const KODI_ADDON_BACKEND_HDL hdl, const int addonLogLevel, const char* strMessage);

typedef struct AddonGlobalInterface {
    addon_log_msg_fn addon_log_msg;
    char* (*get_setting_string)(const KODI_ADDON_BACKEND_HDL hdl, const char* id);
    bool (*get_setting_bool)(const KODI_ADDON_BACKEND_HDL hdl, const char* id, bool* value);
    bool (*get_setting_int)(const KODI_ADDON_BACKEND_HDL hdl, const char* id, int* value);
    bool (*get_setting_float)(const KODI_ADDON_BACKEND_HDL hdl, const char* id, float* value);
    // Add other function pointers here
} AddonGlobalInterface;

typedef struct IInstanceInfo {
    bool (*IsType)(const struct IInstanceInfo* instance, int type);
    // Add other members here
} IInstanceInfo;
