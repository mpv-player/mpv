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

#ifndef MP_CODECS_H
#define MP_CODECS_H

struct mp_log;

struct mp_decoder_entry {
    const char *codec;          // name of the codec (e.g. "mp3")
    const char *decoder;        // decoder name (e.g. "mp3float")
    const char *desc;           // human readable description
};

struct mp_decoder_list {
    struct mp_decoder_entry *entries;
    int num_entries;
};

void mp_add_decoder(struct mp_decoder_list *list, const char *codec,
                    const char *decoder, const char *desc);

struct mp_decoder_list *mp_select_decoders(struct mp_log *log,
                                           struct mp_decoder_list *all,
                                           const char *codec,
                                           const char *selection);

void mp_append_decoders(struct mp_decoder_list *list, struct mp_decoder_list *a);

struct mp_log;
void mp_print_decoders(struct mp_log *log, int msgl, const char *header,
                       struct mp_decoder_list *list);

#endif
