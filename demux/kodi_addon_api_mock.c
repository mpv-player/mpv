#include "kodi_addon_api_mock.h"
#include "mpv_talloc.h"
#include "common/msg.h"
#include <stdlib.h>

static DEMUX_PACKET* mock_allocate_demux_packet(void* kodiInstance, int data_size)
{
    DEMUX_PACKET* pkt = talloc(NULL, DEMUX_PACKET);
    if (!pkt)
        return NULL;
    *pkt = (DEMUX_PACKET){0};
    if (data_size > 0) {
        pkt->pData = talloc_size(pkt, data_size);
        if (!pkt->pData) {
            talloc_free(pkt);
            return NULL;
        }
    }
    pkt->iSize = data_size;
    return pkt;
}

static void mock_free_demux_packet(void* kodiInstance, DEMUX_PACKET* packet)
{
    talloc_free(packet);
}

static void mock_log_msg(const KODI_ADDON_BACKEND_HDL hdl, const int addonLogLevel, const char* strMessage)
{
    // TODO: Map addonLogLevel to mpv log level
    mp_msg(hdl, MSGL_INFO, "[kodi-addon] %s", strMessage);
}

static bool mock_is_type(const struct IInstanceInfo* instance, int type)
{
    // TODO: Implement this properly
    return true;
}

static char* mock_get_setting_string(const KODI_ADDON_BACKEND_HDL hdl, const char* id)
{
    // Return empty string for now
    return "";
}

static bool mock_get_setting_bool(const KODI_ADDON_BACKEND_HDL hdl, const char* id, bool* value)
{
    *value = false;
    return true;
}

static bool mock_get_setting_int(const KODI_ADDON_BACKEND_HDL hdl, const char* id, int* value)
{
    *value = 0;
    return true;
}

static bool mock_get_setting_float(const KODI_ADDON_BACKEND_HDL hdl, const char* id, float* value)
{
    *value = 0.0f;
    return true;
}

void kodi_api_init(AddonToKodiFuncTable_InputStream* toKodi, AddonGlobalInterface* global, IInstanceInfo* instanceInfo, struct mp_log* log)
{
    toKodi->kodiInstance = log;
    toKodi->allocate_demux_packet = mock_allocate_demux_packet;
    toKodi->free_demux_packet = mock_free_demux_packet;

    global->addon_log_msg = mock_log_msg;
    global->get_setting_string = mock_get_setting_string;
    global->get_setting_bool = mock_get_setting_bool;
    global->get_setting_int = mock_get_setting_int;
    global->get_setting_float = mock_get_setting_float;

    instanceInfo->IsType = mock_is_type;
}
