/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/common.h"
#include "common/msg.h"
#include "common/playlist.h"
#include "osdep/io.h"
#include "demux.h"
#include "kodi_addon_api_mock.h"

// This is a guess. The real value should be taken from the kodi headers.
#define ADDON_INSTANCE_INPUTSTREAM 1

typedef void* (*CreateInstanceFn)(const IInstanceInfo*);

struct adaptive_priv {
    void* handle;
    AddonInstance_InputStream* addon_instance;
    AddonGlobalInterface global_iface;
    IInstanceInfo instance_info;
    AddonToKodiFuncTable_InputStream to_kodi_funcs;
    KodiToAddonFuncTable_InputStream to_addon_funcs;
};

void kodi_api_init(AddonToKodiFuncTable_InputStream* toKodi, AddonGlobalInterface* global, IInstanceInfo* instanceInfo, struct mp_log* log);

static int adaptive_open(struct demuxer *demuxer, enum demux_check check)
{
    struct mp_log *log = demuxer->log;
    struct adaptive_priv *priv = talloc_zero(demuxer, struct adaptive_priv);
    demuxer->priv = priv;

    mp_msg(log, MSGL_V, "Opening adaptive demuxer...\n");

    // TODO: Get properties from demuxer->stream->playlist_entry->params
    // for now, we'll just hardcode some test values

    const char* lib_path = "inputstream.adaptive.dll"; // TODO: make this configurable
    priv->handle = mp_dlopen(lib_path, RTLD_LAZY);
    if (!priv->handle) {
        mp_msg(log, MSGL_ERR, "Failed to load %s\n", lib_path);
        return -1;
    }

    CreateInstanceFn create_instance = (CreateInstanceFn)mp_dlsym(priv->handle, "CreateInstance");
    if (!create_instance) {
        mp_msg(log, MSGL_ERR, "Failed to find CreateInstance in %s\n", lib_path);
        mp_dlclose(priv->handle);
        return -1;
    }

    kodi_api_init(&priv->to_kodi_funcs, &priv->global_iface, &priv->instance_info, log);

    priv->addon_instance = (AddonInstance_InputStream*)create_instance(&priv->instance_info);
    if (!priv->addon_instance) {
        mp_msg(log, MSGL_ERR, "CreateInstance failed\n");
        mp_dlclose(priv->handle);
        return -1;
    }

    INPUTSTREAM_PROPERTY props = {0};
    props.m_strURL = demuxer->stream->url;
    props.m_nCountInfoValues = demuxer->params->num_playlist_params;
    for (int i = 0; i < demuxer->params->num_playlist_params; i++) {
        props.m_ListItemProperties[i].m_strKey = demuxer->params->playlist_params[i].name.start;
        props.m_ListItemProperties[i].m_strValue = demuxer->params->playlist_params[i].value.start;
    }

    if (!priv->addon_instance->toAddon->open(priv->addon_instance, &props)) {
        mp_msg(log, MSGL_ERR, "addon->open() failed\n");
        mp_dlclose(priv->handle);
        return -1;
    }

    mp_msg(log, MSGL_V, "Successfully opened adaptive demuxer.\n");

    return 0;
}

#include "demux/packet.h"

static bool adaptive_read_packet(struct demuxer *demuxer, struct demux_packet **pkt)
{
    struct adaptive_priv *priv = demuxer->priv;
    if (!priv || !priv->addon_instance)
        return false;

    DEMUX_PACKET* kodi_pkt = priv->addon_instance->toAddon->demux_read(priv->addon_instance);
    if (!kodi_pkt)
        return false;

    if (kodi_pkt->iSize == 0) {
        // Empty packet, probably waiting for data
        priv->addon_instance->toKodi->free_demux_packet(priv->addon_instance, kodi_pkt);
        return true; // try again
    }

    *pkt = new_demux_packet(kodi_pkt->iSize);
    if (!*pkt) {
        priv->addon_instance->toKodi->free_demux_packet(priv->addon_instance, kodi_pkt);
        return false;
    }

    memcpy((*pkt)->data, kodi_pkt->pData, kodi_pkt->iSize);
    (*pkt)->pts = kodi_pkt->pts;
    (*pkt)->dts = kodi_pkt->dts;
    (*pkt)->duration = kodi_pkt->duration;
    (*pkt)->stream = kodi_pkt->iStreamId;
    // TODO: map other flags

    priv->addon_instance->toKodi->free_demux_packet(priv->addon_instance, kodi_pkt);

    return true;
}

static void adaptive_close(struct demuxer *demuxer)
{
    struct adaptive_priv *priv = demuxer->priv;
    if (!priv)
        return;

    if (priv->addon_instance)
        priv->addon_instance->toAddon->close(priv->addon_instance);

    if (priv->handle)
        mp_dlclose(priv->handle);
}

const demuxer_desc_t demuxer_desc_adaptive = {
    .name = "adaptive",
    .desc = "Kodi Adaptive InputStream Demuxer",
    .open = adaptive_open,
    .read_packet = adaptive_read_packet,
    .close = adaptive_close,
};
