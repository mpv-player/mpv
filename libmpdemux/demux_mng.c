/*
 * MNG file demuxer for MPlayer
 *
 * Copyright (C) 2008 Stefan Schuermans <stefan blinkenarea org>
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#define MNG_SUPPORT_READ
#define MNG_SUPPORT_DISPLAY
#include <libmng.h>

/**
 * \brief some small fixed start time > 0
 *
 * Start time must be > 0 for the variable frame time mechanism
 * (GIF, MATROSKA, MNG) in video.c to work for the first frame.
 */
#define MNG_START_PTS 0.01f

/**
 * \brief private context structure
 *
 * This structure is used as private data for MPlayer demuxer
 * and also as private data for the MNG library.
 *
 * All members ending in \p _ms are in milliseconds
 */
typedef struct {
    stream_t * stream;          ///< pointer to MNG data input stream
    mng_handle h_mng;           ///< MNG library image handle
    int header_processed;       ///< if MNG image header is processed
    mng_uint32 width;           ///< MNG image width
    mng_uint32 height;          ///< MNG image height
    int total_time_ms;          ///< total MNG animation time
    unsigned char * canvas;     /**< \brief canvas to draw the image onto
                                 *   \details
                                 *   \li lines top-down
                                 *   \li pixels left-to-right
                                 *   \li channels RGB
                                 *   \li no padding
                                 *   \li NULL if no canvas yet
                                 */
    int displaying;             /**< \brief if displaying already,
                                 *          i.e. if mng_display has
                                 *          already been called
                                 */
    int finished;               ///< if animation is finished
    int global_time_ms;         ///< current global time for MNG library
    int anim_cur_time_ms;       ///< current frame time in MNG animation
    int anim_frame_duration_ms; ///< current frame duration in MNG animation
    int show_cur_time_ms;       /**< \brief current time in the show process,
                                 *          i.e. time of last demux packet
                                 */
    int show_next_time_ms;      /**< \brief next time in the show process,
                                 *          i.e. time of next demux packet
                                 */
    int timer_ms;               /**< \brief number of milliseconds after which
                                 *          libmng wants to be called again
                                 */
} mng_priv_t;

/**
 * \brief MNG library callback: Allocate a new zero-filled memory block.
 * \param[in] size memory block size
 * \return pointer to new memory block
 */
static mng_ptr demux_mng_alloc(mng_size_t size)
{
    return calloc(1, size);
}

/**
 * \brief MNG library callback: Free memory block.
 * \param[in] ptr pointer to memory block
 * \param[in] size memory block size
 */
static void demux_mng_free(mng_ptr ptr, mng_size_t size)
{
    free(ptr);
}

/**
 * \brief MNG library callback: Open MNG stream.
 * \param[in] h_mng MNG library image handle
 * \return \p MNG_TRUE on success, \p MNG_FALSE on error (never happens)
 */
static mng_bool demux_mng_openstream(mng_handle h_mng)
{
    mng_priv_t * mng_priv = mng_get_userdata(h_mng);
    stream_t * stream = mng_priv->stream;

    // rewind stream to the beginning
    stream_seek(stream, stream->start_pos);

    return MNG_TRUE;
}

/**
 * \brief MNG library callback: Close MNG stream.
 * \param[in] h_mng MNG library image handle
 * \return \p MNG_TRUE on success, \p MNG_FALSE on error (never happens)
 */
static mng_bool demux_mng_closestream(mng_handle h_mng)
{
    return MNG_TRUE;
}

/**
 * \brief MNG library callback: Read data from stream.
 * \param[in] h_mng MNG library image handle
 * \param[in] buf pointer to buffer to fill with data
 * \param[in] size size of buffer
 * \param[out] read number of bytes read from stream
 * \return \p MNG_TRUE on success, \p MNG_FALSE on error (never happens)
 */
static mng_bool demux_mng_readdata(mng_handle h_mng, mng_ptr buf,
                                   mng_uint32 size, mng_uint32 * read)
{
    mng_priv_t * mng_priv = mng_get_userdata(h_mng);
    stream_t * stream = mng_priv->stream;

    // simply read data from stream and return number of bytes or error
    *read = stream_read(stream, buf, size);

    return MNG_TRUE;
}

/**
 * \brief MNG library callback: Header information is processed now.
 * \param[in] h_mng MNG library image handle
 * \param[in] width image width
 * \param[in] height image height
 * \return \p MNG_TRUE on success, \p MNG_FALSE on error
 */
static mng_bool demux_mng_processheader(mng_handle h_mng, mng_uint32 width,
                                        mng_uint32 height)
{
    mng_priv_t * mng_priv = mng_get_userdata(h_mng);

    // remember size in private data
    mng_priv->header_processed = 1;
    mng_priv->width            = width;
    mng_priv->height           = height;

    // get total animation time
    mng_priv->total_time_ms = mng_get_playtime(h_mng);

    // allocate canvas
    mng_priv->canvas = malloc(height * width * 4);
    if (!mng_priv->canvas) {
        mp_msg(MSGT_DEMUX, MSGL_ERR,
               "demux_mng: could not allocate canvas of size %dx%d\n",
               width, height);
        return MNG_FALSE;
    }

    return MNG_TRUE;
}

/**
 * \brief MNG library callback: Get access to a canvas line.
 * \param[in] h_mng MNG library image handle
 * \param[in] line y coordinate of line to access
 * \return pointer to line on success, \p MNG_NULL on error
 */
static mng_ptr demux_mng_getcanvasline(mng_handle h_mng, mng_uint32 line)
{
    mng_priv_t * mng_priv = mng_get_userdata(h_mng);

    // return pointer to canvas line
    if (line < mng_priv->height && mng_priv->canvas)
        return (mng_ptr)(mng_priv->canvas + line * mng_priv->width * 4);
    else
        return (mng_ptr)MNG_NULL;
}

/**
 * \brief MNG library callback: A part of the canvas should be shown.
 *
 * This function is called by libmng whenever it thinks a
 * rectangular part of the display should be updated. This
 * can happen multiple times for a frame and/or a single time
 * for a frame. Only the the part of the display occupied by
 * the rectangle defined by x, y, width, height is to be updated.
 * It is possible that some parts of the display are not updated
 * for many frames. There is no chance here to find out if the
 * current frame is completed with this update or not.
 *
 * This mechanism does not match MPlayer's demuxer architecture,
 * so it will not be used exactly as intended by libmng.
 * A new frame is generated in the demux_mng_fill_buffer() function
 * whenever libmng tells us to wait for some time.
 *
 * \param[in] h_mng MNG library image handle
 * \param[in] x rectangle's left edge
 * \param[in] y rectangle's top edge
 * \param[in] width rectangle's width
 * \param[in] height rectangle's heigt
 * \return \p MNG_TRUE on success, \p MNG_FALSE on error (never happens)
 */
static mng_bool demux_mng_refresh(mng_handle h_mng, mng_uint32 x, mng_uint32 y,
                                  mng_uint32 width, mng_uint32 height)
{
    // nothing to do here, the image data is already on the canvas
    return MNG_TRUE;
}

/**
 * \brief MNG library callback: Get how many milliseconds have passed.
 * \param[in] h_mng MNG library image handle
 * \return global time in milliseconds
 */
static mng_uint32 demux_mng_gettickcount(mng_handle h_mng)
{
    mng_priv_t * mng_priv = mng_get_userdata(h_mng);

    // return current global time
    return mng_priv->global_time_ms;
}

/**
 * \brief MNG library callback: Please call again after some milliseconds.
 * \param[in] h_mng MNG library image handle
 * \param[in] msecs number of milliseconds after which to call again
 * \return \p MNG_TRUE on success, \p MNG_FALSE on error (never happens)
 */
static mng_bool demux_mng_settimer(mng_handle h_mng, mng_uint32 msecs)
{
    mng_priv_t * mng_priv = mng_get_userdata(h_mng);

    // Save number of milliseconds after which to call the MNG library again
    // in private data.
    mng_priv->timer_ms = msecs;
    return MNG_TRUE;
}

/**
 * \brief MPlayer callback: Check if stream contains MNG data.
 * \param[in] demuxer demuxer structure
 * \return demuxer type constant, \p 0 if unknown
 */
static int demux_mng_check_file(demuxer_t *demuxer)
{
    char buf[4];
    if (stream_read(demuxer->stream, buf, 4) != 4)
        return 0;
    if (memcmp(buf, "\x8AMNG", 4))
        return 0;
    return DEMUXER_TYPE_MNG;
}

/**
 * \brief MPlayer callback: Fill buffer from MNG stream.
 * \param[in] demuxer demuxer structure
 * \param[in] ds demuxer stream
 * \return \p 1 on success, \p 0 on error
 */
static int demux_mng_fill_buffer(demuxer_t * demuxer,
                                 demux_stream_t * ds)
{
    mng_priv_t * mng_priv = demuxer->priv;
    mng_handle h_mng = mng_priv->h_mng;
    mng_retcode mng_ret;
    demux_packet_t * dp;

    // exit if animation is finished
    if (mng_priv->finished)
        return 0;

    // advance animation to requested next show time
    while (mng_priv->anim_cur_time_ms + mng_priv->anim_frame_duration_ms
           <= mng_priv->show_next_time_ms && !mng_priv->finished) {

        // advance global and animation time
        mng_priv->global_time_ms += mng_priv->anim_frame_duration_ms;
        mng_priv->anim_cur_time_ms += mng_priv->anim_frame_duration_ms;

        // Clear variable MNG library will write number of milliseconds to
        // (via settimer callback).
        mng_priv->timer_ms = 0;

        // get next image from MNG library
        if (mng_priv->displaying)
            mng_ret = mng_display_resume(h_mng); // resume displaying MNG data
                                                 // to canvas
        else
            mng_ret = mng_display(h_mng); // start displaying MNG data to canvas
        if (mng_ret && mng_ret != MNG_NEEDTIMERWAIT) {
            mp_msg(MSGT_DEMUX, MSGL_ERR,
                   "demux_mng: could not display MNG data to canvas: "
                   "mng_retcode %d\n", mng_ret);
            return 0;
        }
        mng_priv->displaying = 1; // mng_display() has been called now
        mng_priv->finished   = mng_ret == 0; // animation is finished iff
                                             // mng_display() returned 0

        // save current frame duration
        mng_priv->anim_frame_duration_ms = mng_priv->timer_ms < 1
                                           ? 1 : mng_priv->timer_ms;

    } // while (mng_priv->anim_cur_time_ms + ...

    // create a new demuxer packet
    dp = new_demux_packet(mng_priv->height * mng_priv->width * 4);

    // copy image data into demuxer packet
    memcpy(dp->buffer, mng_priv->canvas,
           mng_priv->height * mng_priv->width * 4);

    // set current show time to requested show time
    mng_priv->show_cur_time_ms = mng_priv->show_next_time_ms;

    // get time of next frame to show
    mng_priv->show_next_time_ms = mng_priv->anim_cur_time_ms
                                + mng_priv->anim_frame_duration_ms;

    // Set position and timing information in demuxer video and demuxer packet.
    //  - Time must be time of next frame and always be > 0 for the variable
    //    frame time mechanism (GIF, MATROSKA, MNG) in video.c to work.
    demuxer->video->dpos++;
    dp->pts = (float)mng_priv->show_next_time_ms / 1000.0f + MNG_START_PTS;
    dp->pos = stream_tell(demuxer->stream);
    ds_add_packet(demuxer->video, dp);

    return 1;
}

/**
 * \brief MPlayer callback: Open MNG stream.
 * \param[in] demuxer demuxer structure
 * \return demuxer structure on success, \p NULL on error
 */
static demuxer_t * demux_mng_open(demuxer_t * demuxer)
{
    mng_priv_t * mng_priv;
    mng_handle h_mng;
    mng_retcode mng_ret;
    sh_video_t * sh_video;

    // create private data structure
    mng_priv = calloc(1, sizeof(mng_priv_t));

    //stream pointer into private data
    mng_priv->stream = demuxer->stream;

    // initialize MNG image instance
    h_mng = mng_initialize((mng_ptr)mng_priv, demux_mng_alloc,
                           demux_mng_free, MNG_NULL);
    if (!h_mng) {
        mp_msg(MSGT_DEMUX, MSGL_ERR,
               "demux_mng: could not initialize MNG image instance\n");
        free(mng_priv);
        return NULL;
    }

    // MNG image handle into private data
    mng_priv->h_mng = h_mng;

    // set required MNG callbacks
    if (mng_setcb_openstream(h_mng, demux_mng_openstream) ||
        mng_setcb_closestream(h_mng, demux_mng_closestream) ||
        mng_setcb_readdata(h_mng, demux_mng_readdata) ||
        mng_setcb_processheader(h_mng, demux_mng_processheader) ||
        mng_setcb_getcanvasline(h_mng, demux_mng_getcanvasline) ||
        mng_setcb_refresh(h_mng, demux_mng_refresh) ||
        mng_setcb_gettickcount(h_mng, demux_mng_gettickcount) ||
        mng_setcb_settimer(h_mng, demux_mng_settimer) ||
        mng_set_canvasstyle(h_mng, MNG_CANVAS_RGBA8)) {
        mp_msg(MSGT_DEMUX, MSGL_ERR,
               "demux_mng: could not set MNG callbacks\n");
        mng_cleanup(&h_mng);
        free(mng_priv);
        return NULL;
    }

    // start reading MNG data
    mng_ret = mng_read(h_mng);
    if (mng_ret) {
        mp_msg(MSGT_DEMUX, MSGL_ERR,
               "demux_mng: could not start reading MNG data: "
               "mng_retcode %d\n", mng_ret);
        mng_cleanup(&h_mng);
        free(mng_priv);
        return NULL;
    }

    // check that MNG header is processed now
    if (!mng_priv->header_processed) {
        mp_msg(MSGT_DEMUX, MSGL_ERR,
               "demux_mng: internal error: header not processed\n");
        mng_cleanup(&h_mng);
        free(mng_priv);
        return NULL;
    }

    // create a new video stream header
    sh_video = new_sh_video(demuxer, 0);

    // Make sure the demuxer knows about the new video stream header
    // (even though new_sh_video() ought to take care of it).
    // (Thanks to demux_gif.c for this.)
    demuxer->video->sh = sh_video;

    // Make sure that the video demuxer stream header knows about its
    // parent video demuxer stream (this is getting wacky), or else
    // video_read_properties() will choke.
    // (Thanks to demux_gif.c for this.)
    sh_video->ds = demuxer->video;

    // set format of pixels in video packets
    sh_video->format = mmioFOURCC(32, 'B', 'G', 'R');

    // set framerate to some value (MNG does not have a fixed framerate)
    sh_video->fps       = 5.0f;
    sh_video->frametime = 1.0f / sh_video->fps;

    // set video frame parameters
    sh_video->bih                = malloc(sizeof(BITMAPINFOHEADER));
    sh_video->bih->biCompression = sh_video->format;
    sh_video->bih->biWidth       = mng_priv->width;
    sh_video->bih->biHeight      = mng_priv->height;
    sh_video->bih->biBitCount    = 32;
    sh_video->bih->biPlanes      = 1;

    // Set start time to something > 0.
    //  - This is required for the variable frame time mechanism
    //    (GIF, MATROSKA, MNG) in video.c to work for the first frame.
    sh_video->ds->pts = MNG_START_PTS;

    // set private data in demuxer and return demuxer
    demuxer->priv = mng_priv;
    return demuxer;
}

/**
 * \brief MPlayer callback: Close MNG stream.
 * \param[in] demuxer demuxer structure
 */
static void demux_mng_close(demuxer_t* demuxer)
{
    mng_priv_t * mng_priv = demuxer->priv;

    if (mng_priv) {

        // shutdown MNG image instance
        if (mng_priv->h_mng)
            mng_cleanup(&mng_priv->h_mng);

        // free private data
        if (mng_priv->canvas)
            free(mng_priv->canvas);

        free(mng_priv);
    }
}

/**
 * \brief MPlayer callback: Seek in MNG stream.
 * \param[in] demuxer demuxer structure
 * \param[in] rel_seek_secs relative seek time in seconds
 * \param[in] audio_delay unused, MNG does not contain audio
 * \param[in] flags bit flags, \p 1: absolute, \p 2: fractional position
 */
static void demux_mng_seek(demuxer_t * demuxer, float rel_seek_secs,
                           float audio_delay, int flags)
{
    mng_priv_t * mng_priv = demuxer->priv;
    mng_handle h_mng = mng_priv->h_mng;
    mng_retcode mng_ret;
    int seek_ms, pos_ms;

    // exit if not ready to seek (header not yet read or not yet displaying)
    if (!mng_priv->header_processed || !mng_priv->displaying)
        return;

    // get number of milliseconds to seek to
    if (flags & 2) // seek by fractional position (0.0 ... 1.0)
        seek_ms = (int)(rel_seek_secs * (float)mng_priv->total_time_ms);
    else // seek by time in seconds
        seek_ms = (int)(rel_seek_secs * 1000.0f + 0.5f);

    // get new position in milliseconds
    if (flags & 1) // absolute
        pos_ms = seek_ms;
    else // relative
        pos_ms = mng_priv->show_cur_time_ms + seek_ms;

    // fix position
    if (pos_ms < 0)
        pos_ms = 0;
    if (pos_ms > mng_priv->total_time_ms)
        pos_ms = mng_priv->total_time_ms;

    // FIXME
    // In principle there is a function to seek in MNG: mng_display_gotime().
    //  - Using it did not work out (documentation is very brief,
    //    example code does not exist?).
    //  - The following code works, but its performance is quite bad.

    // seeking forward
    if (pos_ms >= mng_priv->show_cur_time_ms) {

        // Simply advance show time to seek position.
        //  - Everything else will be handled in demux_mng_fill_buffer().
        mng_priv->show_next_time_ms = pos_ms;

    } // if (pos_ms > mng_priv->show_time_ms)

    // seeking backward
    else { // if (pos_ms > mng_priv->show_time_ms)

        // Clear variable MNG library will write number of milliseconds to
        // (via settimer callback).
        mng_priv->timer_ms = 0;

        // Restart displaying and advance show time to seek position.
        //  - Everything else will be handled in demux_mng_fill_buffer().
        mng_ret = mng_display_reset(h_mng);
        // If a timer wait is needed, fool libmng that requested time
        // passed and try again.
        if (mng_ret == MNG_NEEDTIMERWAIT) {
            mng_priv->global_time_ms += mng_priv->timer_ms;
            mng_ret = mng_display_reset(h_mng);
        }
        if (mng_ret) {
            mp_msg(MSGT_DEMUX, MSGL_ERR,
                   "demux_mng: could not reset MNG display state: "
                   "mng_retcode %d\n", mng_ret);
            return;
        }
        mng_priv->displaying             = 0;
        mng_priv->finished               = 0;
        mng_priv->anim_cur_time_ms       = 0;
        mng_priv->anim_frame_duration_ms = 0;
        mng_priv->show_next_time_ms      = pos_ms;

    } // if (pos_ms > mng_priv->show_time_ms) ... else
}

/**
 * \brief MPlayer callback: Control MNG stream.
 * \param[in] demuxer demuxer structure
 * \param[in] cmd code of control command to perform
 * \param[in,out] arg command argument
 * \return demuxer control response code
 */
static int demux_mng_control(demuxer_t * demuxer, int cmd, void * arg)
{
    mng_priv_t * mng_priv = demuxer->priv;

    switch(cmd) {

      // get total movie length
      case DEMUXER_CTRL_GET_TIME_LENGTH:
          if (mng_priv->header_processed) {
              *(double *)arg = (double)mng_priv->total_time_ms / 1000.0;
              return DEMUXER_CTRL_OK;
          } else {
              return DEMUXER_CTRL_DONTKNOW;
          }
          break;

      // get position in movie
      case DEMUXER_CTRL_GET_PERCENT_POS:
          if (mng_priv->header_processed && mng_priv->total_time_ms > 0) {
              *(int *)arg = (100 * mng_priv->show_cur_time_ms
                             + mng_priv->total_time_ms / 2)
                            / mng_priv->total_time_ms;
              return DEMUXER_CTRL_OK;
          } else {
              return DEMUXER_CTRL_DONTKNOW;
          }
          break;

      default:
          return DEMUXER_CTRL_NOTIMPL;

    } // switch (cmd)
}

const demuxer_desc_t demuxer_desc_mng = {
    "MNG demuxer",
    "mng",
    "MNG",
    "Stefan Schuermans <stefan@blinkenarea.org>",
    "MNG files, using libmng",
    DEMUXER_TYPE_MNG,
    0, // unsafe autodetect (only checking magic at beginning of stream)
    demux_mng_check_file,
    demux_mng_fill_buffer,
    demux_mng_open,
    demux_mng_close,
    demux_mng_seek,
    demux_mng_control
};
