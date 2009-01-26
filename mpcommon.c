#include <stdlib.h>
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "mplayer.h"
#include "libvo/sub.h"
#include "libvo/video_out.h"
#include "cpudetect.h"
#include "help_mp.h"
#include "mp_msg.h"
#include "spudec.h"
#include "version.h"
#include "vobsub.h"
#ifdef CONFIG_TV_TELETEXT
#include "stream/tv.h"
#endif
#include "libavutil/intreadwrite.h"
#include "m_option.h"

double sub_last_pts = -303;

#ifdef CONFIG_ASS
#include "libass/ass.h"
#include "libass/ass_mp.h"
ass_track_t* ass_track = 0; // current track to render
#endif

sub_data* subdata = NULL;
subtitle* vo_sub_last = NULL;


void print_version(const char* name)
{
    mp_msg(MSGT_CPLAYER, MSGL_INFO, MP_TITLE, name);

    /* Test for CPU capabilities (and corresponding OS support) for optimizing */
    GetCpuCaps(&gCpuCaps);
#if ARCH_X86
    mp_msg(MSGT_CPLAYER, MSGL_V,
	   "CPUflags:  MMX: %d MMX2: %d 3DNow: %d 3DNowExt: %d SSE: %d SSE2: %d SSSE3: %d\n",
	   gCpuCaps.hasMMX, gCpuCaps.hasMMX2,
	   gCpuCaps.has3DNow, gCpuCaps.has3DNowExt,
	   gCpuCaps.hasSSE, gCpuCaps.hasSSE2, gCpuCaps.hasSSSE3);
#ifdef RUNTIME_CPUDETECT
    mp_msg(MSGT_CPLAYER,MSGL_V, MSGTR_CompiledWithRuntimeDetection);
#else
    mp_msg(MSGT_CPLAYER,MSGL_V, MSGTR_CompiledWithCPUExtensions);
if (HAVE_MMX)
    mp_msg(MSGT_CPLAYER,MSGL_V," MMX");
if (HAVE_MMX2)
    mp_msg(MSGT_CPLAYER,MSGL_V," MMX2");
if (HAVE_AMD3DNOW)
    mp_msg(MSGT_CPLAYER,MSGL_V," 3DNow");
if (HAVE_AMD3DNOWEXT)
    mp_msg(MSGT_CPLAYER,MSGL_V," 3DNowExt");
if (HAVE_SSE)
    mp_msg(MSGT_CPLAYER,MSGL_V," SSE");
if (HAVE_SSE2)
    mp_msg(MSGT_CPLAYER,MSGL_V," SSE2");
if (HAVE_SSSE3)
    mp_msg(MSGT_CPLAYER,MSGL_V," SSSE3");
if (HAVE_CMOV)
    mp_msg(MSGT_CPLAYER,MSGL_V," CMOV");
    mp_msg(MSGT_CPLAYER,MSGL_V,"\n");
#endif /* RUNTIME_CPUDETECT */
#endif /* ARCH_X86 */
}


void update_subtitles(sh_video_t *sh_video, demux_stream_t *d_dvdsub, int reset)
{
    unsigned char *packet=NULL;
    int len;
    char type = d_dvdsub->sh ? ((sh_sub_t *)d_dvdsub->sh)->type : 'v';
    static subtitle subs;
    if (reset) {
        sub_clear_text(&subs, MP_NOPTS_VALUE);
        if (vo_sub) {
            vo_sub = NULL;
            vo_osd_changed(OSDTYPE_SUBTITLE);
        }
        if (vo_spudec) {
            spudec_reset(vo_spudec);
            vo_osd_changed(OSDTYPE_SPU);
        }
    }
    // find sub
    if (subdata) {
        double pts = sh_video->pts;
        if (sub_fps==0) sub_fps = sh_video->fps;
        current_module = "find_sub";
        if (pts > sub_last_pts || pts < sub_last_pts-1.0) {
            find_sub(subdata, (pts+sub_delay) *
                     (subdata->sub_uses_time ? 100. : sub_fps)); 
            if (vo_sub) vo_sub_last = vo_sub;
            // FIXME! frame counter...
            sub_last_pts = pts;
        }
    }

    // DVD sub:
    if (vo_config_count && vo_spudec &&
        (vobsub_id >= 0 || (dvdsub_id >= 0 && type == 'v'))) {
        int timestamp;
        current_module = "spudec";
        spudec_heartbeat(vo_spudec, 90000*sh_video->timer);
        /* Get a sub packet from the DVD or a vobsub and make a timestamp
         * relative to sh_video->timer */
        while(1) {
            // Vobsub
            len = 0;
            if (vo_vobsub) {
                if (sh_video->pts+sub_delay >= 0) {
                    len = vobsub_get_packet(vo_vobsub, sh_video->pts+sub_delay,
                                            (void**)&packet, &timestamp);
                    if (len > 0) {
                        timestamp -= (sh_video->pts + sub_delay - sh_video->timer)*90000;
                        mp_dbg(MSGT_CPLAYER,MSGL_V,"\rVOB sub: len=%d v_pts=%5.3f v_timer=%5.3f sub=%5.3f ts=%d \n",len,sh_video->pts,sh_video->timer,timestamp / 90000.0,timestamp);
                    }
                }
            } else {
                // DVD sub
                len = ds_get_packet_sub(d_dvdsub, (unsigned char**)&packet);
                if (len > 0) {
                    // XXX This is wrong, sh_video->pts can be arbitrarily
                    // much behind demuxing position. Unfortunately using
                    // d_video->pts which would have been the simplest
                    // improvement doesn't work because mpeg specific hacks
                    // in video.c set d_video->pts to 0.
                    float x = d_dvdsub->pts - sh_video->pts;
                    if (x > -20 && x < 20) // prevent missing subs on pts reset
                        timestamp = 90000*(sh_video->timer + d_dvdsub->pts
                                           + sub_delay - sh_video->pts);
                    else timestamp = 90000*(sh_video->timer + sub_delay);
                    mp_dbg(MSGT_CPLAYER, MSGL_V, "\rDVD sub: len=%d  "
                           "v_pts=%5.3f  s_pts=%5.3f  ts=%d \n", len,
                           sh_video->pts, d_dvdsub->pts, timestamp);
                }
            }
            if (len<=0 || !packet) break;
            if (vo_vobsub || timestamp >= 0)
                spudec_assemble(vo_spudec, packet, len, timestamp);
        }

        if (spudec_changed(vo_spudec))
            vo_osd_changed(OSDTYPE_SPU);
    } else if (dvdsub_id >= 0 && (type == 't' || type == 'm' || type == 'a')) {
        double curpts = sh_video->pts + sub_delay;
        double endpts;
        vo_sub = &subs;
        while (d_dvdsub->first) {
            double pts = ds_get_next_pts(d_dvdsub);
            if (pts > curpts)
                break;
            endpts = d_dvdsub->first->endpts;
            len = ds_get_packet_sub(d_dvdsub, &packet);
            if (type == 'm') {
                if (len < 2) continue;
                len = FFMIN(len - 2, AV_RB16(packet));
                packet += 2;
            }
#ifdef CONFIG_ASS
            if (ass_enabled) {
                sh_sub_t* sh = d_dvdsub->sh;
                ass_track = sh ? sh->ass_track : NULL;
                if (!ass_track) continue;
                if (type == 'a') { // ssa/ass subs with libass
                    ass_process_chunk(ass_track, packet, len,
                                      (long long)(pts*1000 + 0.5),
                                      (long long)((endpts-pts)*1000 + 0.5));
                } else { // plaintext subs with libass
                    vo_sub = NULL;
                    if (pts != MP_NOPTS_VALUE) {
                        if (endpts == MP_NOPTS_VALUE) endpts = pts + 3;
                        sub_clear_text(&subs, MP_NOPTS_VALUE);
                        sub_add_text(&subs, packet, len, endpts);
                        subs.start = pts * 100;
                        subs.end = endpts * 100;
                        ass_process_subtitle(ass_track, &subs);
                    }
                }
                continue;
            }
#endif
            if (pts != MP_NOPTS_VALUE) {
                if (endpts == MP_NOPTS_VALUE)
                    sub_clear_text(&subs, MP_NOPTS_VALUE);
                if (type == 'a') { // ssa/ass subs without libass => convert to plaintext
                    int i;
                    unsigned char* p = packet;
                    for (i=0; i < 8 && *p != '\0'; p++)
                        if (*p == ',')
                            i++;
                    if (*p == '\0')  /* Broken line? */
                        continue;
                    len -= p - packet;
                    packet = p;
                }
                sub_add_text(&subs, packet, len, endpts);
                vo_osd_changed(OSDTYPE_SUBTITLE);
            }
        }
        if (sub_clear_text(&subs, curpts))
            vo_osd_changed(OSDTYPE_SUBTITLE);
    }
    current_module=NULL;
}

void update_teletext(sh_video_t *sh_video, demuxer_t *demuxer, int reset)
{
#ifdef CONFIG_TV_TELETEXT
    tvi_handle_t* tvh=demuxer->priv;
    int page_changed;

    if (demuxer->type != DEMUXER_TYPE_TV || !tvh) return;

    //Also forcing page update when such ioctl is not supported or call error occured
    if(tvh->functions->control(tvh->priv,TV_VBI_CONTROL_IS_CHANGED,&page_changed)!=TVI_CONTROL_TRUE)
        page_changed=1;

    if(!page_changed)
        return;

    if(tvh->functions->control(tvh->priv,TV_VBI_CONTROL_GET_VBIPAGE,&vo_osd_teletext_page)!=TVI_CONTROL_TRUE)
        vo_osd_teletext_page=NULL;
    if(tvh->functions->control(tvh->priv,TV_VBI_CONTROL_GET_HALF_PAGE,&vo_osd_teletext_half)!=TVI_CONTROL_TRUE)
        vo_osd_teletext_half=0;
    if(tvh->functions->control(tvh->priv,TV_VBI_CONTROL_GET_MODE,&vo_osd_teletext_mode)!=TVI_CONTROL_TRUE)
        vo_osd_teletext_mode=0;
    if(tvh->functions->control(tvh->priv,TV_VBI_CONTROL_GET_FORMAT,&vo_osd_teletext_format)!=TVI_CONTROL_TRUE)
        vo_osd_teletext_format=0;
    vo_osd_changed(OSDTYPE_TELETEXT);

    tvh->functions->control(tvh->priv,TV_VBI_CONTROL_MARK_UNCHANGED,NULL);
#endif
}

int select_audio(demuxer_t* demuxer, int audio_id, char* audio_lang)
{
    if (audio_id == -1 && audio_lang)
        audio_id = demuxer_audio_track_by_lang(demuxer, audio_lang);
    if (audio_id == -1)
        audio_id = demuxer_default_audio_track(demuxer);
    if (audio_id != -1) // -1 (automatic) is the default behaviour of demuxers
        demuxer_switch_audio(demuxer, audio_id);
    if (audio_id == -2) { // some demuxers don't yet know how to switch to no sound
        demuxer->audio->id = -2;
        demuxer->audio->sh = NULL;
    }
    return demuxer->audio->id;
}

/* Parse -noconfig common to both programs */
int disable_system_conf=0;
int disable_user_conf=0;
#ifdef CONFIG_GUI
int disable_gui_conf=0;
#endif /* CONFIG_GUI */

/* Disable all configuration files */
static void noconfig_all(void)
{
    disable_system_conf = 1;
    disable_user_conf = 1;
#ifdef CONFIG_GUI
    disable_gui_conf = 1;
#endif /* CONFIG_GUI */
}

const m_option_t noconfig_opts[] = {
    {"all", noconfig_all, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 0, NULL},
    {"system", &disable_system_conf, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 1, NULL},
    {"user", &disable_user_conf, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 1, NULL},
#ifdef CONFIG_GUI
    {"gui", &disable_gui_conf, CONF_TYPE_FLAG, CONF_GLOBAL|CONF_NOCFG|CONF_PRE_PARSE, 0, 1, NULL},
#endif /* CONFIG_GUI */
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
