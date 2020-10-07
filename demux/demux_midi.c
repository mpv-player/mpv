#include <math.h>

#include <fluidsynth.h>

#include "codec_tags.h"
#include "demux.h"
#include "stream/stream.h"

// Arbitrary.
#define FRAME_SAMPLES 1024
#define SAMPLERATE 48000

struct priv {
    struct sh_stream *sh;
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    fluid_player_t *player;
    uint64_t samples;
    struct demux_packet *first_pkt;
};

static bool check_midi(uint8_t *buf, int size)
{
    if (size < 4 + 4 + 6 + 4)
        return false;
    if (memcmp(buf, "MThd", 4))
        return false;
    if (buf[4] || buf[5] || buf[6] || buf[7] != 6)
        return false; // length must always be 6
    if (buf[8] || buf[9] > 2)
        return false; // version is always 0/1/2
    if (memcmp(&buf[4 + 4 + 6], "MTrk", 4))
        return false; // expect a MTrk chunk to follow
    // Regarding bit 15 (SMPTE format flag): fluidsynth doesn't support it
    int division = (buf[12] << 8) | buf[13];
    return division > 0 && !(division & (1 << 15));
}

static bool d_read_packet(struct demuxer *demuxer, struct demux_packet **pkt)
{
    struct priv *p = demuxer->priv;

    if (p->first_pkt) {
        *pkt = p->first_pkt;
        p->first_pkt = NULL;
        return true;
    }

    if (fluid_player_get_status(p->player) != FLUID_PLAYER_PLAYING)
        return false;

    struct demux_packet *dp = new_demux_packet(FRAME_SAMPLES * 4 * 2);
    if (!dp)
        return true;

    fluid_synth_write_float(p->synth, FRAME_SAMPLES,
                            dp->buffer, 0, 2,
                            dp->buffer, 1, 2);

    dp->pts = p->samples / (double)p->sh->codec->samplerate;
    p->samples += FRAME_SAMPLES;

    dp->stream = p->sh->index;
    dp->keyframe = true;
    *pkt = dp;

    return true;
}

static void d_close(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    if (p->player)
        delete_fluid_player(p->player);
    if (p->synth)
        delete_fluid_synth(p->synth);
    if (p->settings)
        delete_fluid_settings(p->settings);
    talloc_free(p->first_pkt);
}

static void no(int level, const char *message, void *data)
{
}

static int try_open_file(struct demuxer *demuxer, enum demux_check check)
{
    struct priv *p = talloc_zero(demuxer, struct priv);
    demuxer->priv = p;

    uint8_t probe[STREAM_BUFFER_SIZE];
    int len = stream_read_peek(demuxer->stream, probe, sizeof(probe));
    if (len < 1 || !check_midi(probe, len))
        return -1;

    bstr data = stream_read_complete(demuxer->stream, demuxer, 1000000);
    if (data.start == NULL)
        return -1;

    // Another idiot API with a global log callback and defaulting to stderr (or
    // stdout on win32 - lol?). Shut it up to disable particularly stupid
    // messages, such as about SDL (wtf? oh yes, they mess with SDL's fucking
    // stupid global state too, even if you're not asking for it).
    int fucking_stupid[] = {FLUID_PANIC, FLUID_ERR, FLUID_WARN, FLUID_INFO,
                            FLUID_DBG};
    for (int n = 0; n < MP_ARRAY_SIZE(fucking_stupid); n++)
        fluid_set_log_function(fucking_stupid[n], no, NULL);

    p->settings = new_fluid_settings();
    if (!p->settings)
        goto error;
    if (fluid_settings_setstr(p->settings, "player.timing-source", "sample"))
        goto error;
    if (fluid_settings_setnum(p->settings, "synth.sample-rate", SAMPLERATE))
        goto error;
    p->synth = new_fluid_synth(p->settings);
    if (!p->synth)
        goto error;
    p->player = new_fluid_player(p->synth);
    if (!p->player)
        goto error;

    char *soundfont;
    if (fluid_settings_dupstr(p->settings, "synth.default-soundfont", &soundfont))
        soundfont = NULL;
    if (!soundfont) {
        MP_ERR(demuxer, "No sound font available.\n");
        goto error;
    }
    int soundfont_st = fluid_synth_sfload(p->synth, soundfont, 1);
    fluid_free(soundfont);
    if (!soundfont_st) {
        MP_ERR(demuxer, "Failed to load sound font available.\n");
        goto error;
    }

    if (fluid_player_add_mem(p->player, data.start, data.len))
        goto error;

    p->sh = demux_alloc_sh_stream(STREAM_AUDIO);
    struct mp_codec_params *c = p->sh->codec;
    c->channels = (struct mp_chmap)MP_CHMAP_INIT_STEREO;
    c->samplerate = SAMPLERATE;
    c->native_tb_num = 1;
    c->native_tb_den = c->samplerate;
    mp_set_pcm_codec(p->sh->codec, true, true, 32, false);

    demux_add_sh_stream(demuxer, p->sh);

    if (fluid_player_play(p->player))
        goto error;

    // Fluidsynth has some sort of internal playlist, and it advances to the
    // current one only if you "pull" some samples. That means we don't know
    // whether the MIDI file can even be loaded by Fluidsynth until we read
    // same data.
    // This heuristic may fail on very short MIDI files.
    d_read_packet(demuxer, &p->first_pkt);
    if (fluid_player_get_status(p->player) != FLUID_PLAYER_PLAYING)
        goto error;

    talloc_free(data.start);
    demuxer->seekable = false;
    demux_close_stream(demuxer);

    return 0;

error:
    MP_ERR(demuxer, "Fluidsynth failed to initialize.\n");
    return -1;
}

const struct demuxer_desc demuxer_desc_midi = {
    .name = "midi",
    .desc = "MIDI via fluidsynth",
    .open = try_open_file,
    .close = d_close,
    .read_packet = d_read_packet,
};
