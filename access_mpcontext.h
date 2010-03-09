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

#ifndef MPLAYER_ACCESS_MPCONTEXT_H
#define MPLAYER_ACCESS_MPCONTEXT_H

struct MPContext;
const void *mpctx_get_video_out(struct MPContext *mpctx);
const void *mpctx_get_audio_out(struct MPContext *mpctx);
void *mpctx_get_demuxer(struct MPContext *mpctx);
void *mpctx_get_playtree_iter(struct MPContext *mpctx);
void *mpctx_get_mixer(struct MPContext *mpctx);
int mpctx_get_global_sub_size(struct MPContext *mpctx);
int mpctx_get_osd_function(struct MPContext *mpctx);

#endif /* MPLAYER_ACCESS_MPCONTEXT_H */
