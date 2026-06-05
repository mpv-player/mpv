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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>

#include "waveform_cache.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "options/path.h"
#include "mpv_talloc.h"

#define CACHE_MAGIC      "MPVWF\0"
#define CACHE_MAGIC_SIZE 6
#define CACHE_VERSION    1
#define MAX_SAMPLES      10000000   // 10 million — sanity limit

// On-disk binary header (48 bytes, naturally aligned)
struct cache_header {
    char     magic[6];         // "MPVWF\0"
    uint16_t version;          // CACHE_VERSION
    uint32_t sample_fps;       // samples-per-second used during scan
    uint32_t sample_count;     // number of waveform_sample records
    uint32_t reserved0;        // padding / future use
    double   duration;         // audio duration in seconds
    uint64_t file_hash;        // waveform_file_hash() of the source file
    uint32_t reserved[4];      // future expansion
};

// ── Hash ─────────────────────────────────────────────────────────────────────

// FNV-1a 64-bit
static uint64_t hash_fnv1a(const uint8_t *data, size_t len)
{
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t waveform_file_hash(const char *path, int64_t size, int64_t mtime)
{
    if (!path)
        return 0;

    size_t plen = strlen(path);
    size_t total = plen + sizeof(size) + sizeof(mtime);

    // Stack-allocate for typical paths; heap only when needed
    uint8_t *buf = talloc_size(NULL, total);
    if (!buf)
        return 0;

    memcpy(buf,                      path,  plen);
    memcpy(buf + plen,               &size, sizeof(size));
    memcpy(buf + plen + sizeof(size), &mtime, sizeof(mtime));

    uint64_t hash = hash_fnv1a(buf, total);
    talloc_free(buf);
    return hash;
}

// ── Cache path ───────────────────────────────────────────────────────────────

// Returns a talloc'd path like:
//   ~/.config/mpv/cache/waveform/<hash>-<fps>fps.mpvwfc
// talloc_ctx is the owner; pass NULL for a root allocation.
static char *cache_file_path(void *talloc_ctx, struct mpv_global *global,
                             int fps, uint64_t file_hash)
{
    char filename[64];
    snprintf(filename, sizeof(filename), "waveform/%016"PRIx64"-%dfps.mpvwfc",
             file_hash, fps);

    // mp_find_user_file creates the parent dir if needed (via mp_mk_user_dir)
    mp_mk_user_dir(global, "cache", "waveform");
    return mp_find_user_file(talloc_ctx, global, "cache", filename);
}

// ── Validation ───────────────────────────────────────────────────────────────

static bool validate_data(struct waveform_data *data)
{
    if (!data || !data->samples || data->count <= 0)
        return false;
    if (data->count > MAX_SAMPLES)
        return false;

    float peak = 0.0f;
    double last_t = -1.0;

    for (int i = 0; i < data->count; i++) {
        struct waveform_sample *s = &data->samples[i];

        if (!isfinite(s->rms) || s->rms < 0.0f || s->rms > 1.0f)
            return false;
        if (!isfinite(s->time) || s->time < 0.0f)
            return false;
        if (i > 0 && s->time <= last_t)
            return false;

        last_t = s->time;
        if (s->rms > peak)
            peak = s->rms;
    }

    // Reject silence / DC-only audio
    return peak >= 1e-9f;
}

// ── Public API ───────────────────────────────────────────────────────────────

bool waveform_cache_save(struct mp_log *log, struct mpv_global *global,
                         const char *video_path, struct waveform_data *data,
                         int fps)
{
    if (!video_path || !data || fps <= 0)
        return false;

    if (!validate_data(data)) {
        mp_warn(log, "waveform: refusing to cache invalid data\n");
        return false;
    }

    struct stat st;
    if (stat(video_path, &st) != 0) {
        mp_warn(log, "waveform: stat(%s) failed: %s\n",
                video_path, strerror(errno));
        return false;
    }

    uint64_t file_hash = waveform_file_hash(video_path,
                                            (int64_t)st.st_size,
                                            (int64_t)st.st_mtime);

    void *tmp = talloc_new(NULL);
    char *cache_path = cache_file_path(tmp, global, fps, file_hash);
    if (!cache_path) {
        talloc_free(tmp);
        return false;
    }

    char *temp_path = talloc_asprintf(tmp, "%s.tmp", cache_path);

    FILE *f = fopen(temp_path, "wb");
    if (!f) {
        mp_warn(log, "waveform: cannot open %s for writing: %s\n",
                temp_path, strerror(errno));
        talloc_free(tmp);
        return false;
    }

    struct cache_header hdr = {0};
    memcpy(hdr.magic, CACHE_MAGIC, CACHE_MAGIC_SIZE);
    hdr.version      = CACHE_VERSION;
    hdr.sample_fps   = (uint32_t)fps;
    hdr.sample_count = (uint32_t)data->count;
    hdr.duration     = data->duration;
    hdr.file_hash    = file_hash;

    bool ok = (fwrite(&hdr, sizeof(hdr), 1, f) == 1) &&
              (fwrite(data->samples, sizeof(struct waveform_sample),
                      data->count, f) == (size_t)data->count);
    fclose(f);

    if (!ok) {
        mp_warn(log, "waveform: write error on %s\n", temp_path);
        remove(temp_path);
        talloc_free(tmp);
        return false;
    }

    // Atomic swap: remove old file first on Windows (rename() can't overwrite)
#ifdef _WIN32
    remove(cache_path);
#endif
    if (rename(temp_path, cache_path) != 0) {
        mp_warn(log, "waveform: rename failed: %s\n", strerror(errno));
        remove(temp_path);
        talloc_free(tmp);
        return false;
    }

    mp_verbose(log, "waveform: saved cache %s (%d samples)\n",
               cache_path, data->count);

    talloc_free(tmp);
    return true;
}

struct waveform_data *waveform_cache_load(struct mp_log *log,
                                          struct mpv_global *global,
                                          const char *video_path,
                                          int fps, uint64_t file_hash)
{
    if (!video_path || fps <= 0)
        return NULL;

    void *tmp = talloc_new(NULL);
    char *cache_path = cache_file_path(tmp, global, fps, file_hash);
    if (!cache_path) {
        talloc_free(tmp);
        return NULL;
    }

    FILE *f = fopen(cache_path, "rb");
    if (!f) {
        talloc_free(tmp);
        return NULL; // Cache miss — not an error
    }

    struct cache_header hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1)
        goto corrupt;

    if (memcmp(hdr.magic, CACHE_MAGIC, CACHE_MAGIC_SIZE) != 0) {
        mp_warn(log, "waveform: bad magic in %s\n", cache_path);
        goto corrupt;
    }
    if (hdr.version != CACHE_VERSION) {
        mp_dbg(log, "waveform: version mismatch in %s (got %u)\n",
               cache_path, hdr.version);
        goto corrupt;
    }
    if (hdr.sample_fps != (uint32_t)fps) {
        mp_warn(log, "waveform: FPS mismatch in %s\n", cache_path);
        goto corrupt;
    }
    if (hdr.file_hash != file_hash) {
        mp_verbose(log, "waveform: hash mismatch in %s — file changed\n",
                   cache_path);
        goto corrupt;
    }
    if (hdr.sample_count == 0 || hdr.sample_count > MAX_SAMPLES) {
        mp_warn(log, "waveform: bad sample_count %u in %s\n",
                hdr.sample_count, cache_path);
        goto corrupt;
    }

    struct waveform_data *data = talloc_zero(NULL, struct waveform_data);
    data->samples = talloc_array(data, struct waveform_sample, hdr.sample_count);
    data->count   = (int)hdr.sample_count;
    data->fps     = (int)hdr.sample_fps;
    data->duration = hdr.duration;

    if (fread(data->samples, sizeof(struct waveform_sample),
              hdr.sample_count, f) != hdr.sample_count)
    {
        mp_warn(log, "waveform: truncated cache %s\n", cache_path);
        talloc_free(data);
        goto corrupt;
    }
    fclose(f);

    if (!validate_data(data)) {
        mp_warn(log, "waveform: data validation failed for %s\n", cache_path);
        talloc_free(data);
        remove(cache_path);
        talloc_free(tmp);
        return NULL;
    }

    mp_verbose(log, "waveform: loaded cache %s (%d samples)\n",
               cache_path, data->count);
    talloc_free(tmp);
    return data;

corrupt:
    fclose(f);
    remove(cache_path);  // delete corrupt file so it's regenerated
    talloc_free(tmp);
    return NULL;
}

void waveform_cache_invalidate(struct mp_log *log, struct mpv_global *global,
                                const char *video_path, int fps)
{
    if (!video_path || fps <= 0)
        return;

    struct stat st;
    if (stat(video_path, &st) != 0)
        return;

    uint64_t file_hash = waveform_file_hash(video_path,
                                            (int64_t)st.st_size,
                                            (int64_t)st.st_mtime);
    void *tmp = talloc_new(NULL);
    char *path = cache_file_path(tmp, global, fps, file_hash);
    if (path && remove(path) == 0)
        mp_verbose(log, "waveform: invalidated cache %s\n", path);
    talloc_free(tmp);
}
