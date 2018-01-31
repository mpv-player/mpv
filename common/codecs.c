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

#include <assert.h>
#include "mpv_talloc.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "codecs.h"

void mp_add_decoder(struct mp_decoder_list *list, const char *codec,
                    const char *decoder, const char *desc)
{
    struct mp_decoder_entry entry = {
        .codec = talloc_strdup(list, codec),
        .decoder = talloc_strdup(list, decoder),
        .desc = talloc_strdup(list, desc),
    };
    MP_TARRAY_APPEND(list, list->entries, list->num_entries, entry);
}

// Add entry, but only if it's not yet on the list, and if the codec matches.
// If codec == NULL, don't compare codecs.
static void add_new(struct mp_decoder_list *to, struct mp_decoder_entry *entry,
                    const char *codec)
{
    if (!entry || (codec && strcmp(entry->codec, codec) != 0))
        return;
    mp_add_decoder(to, entry->codec, entry->decoder, entry->desc);
}

// Select a decoder from the given list for the given codec. The selection
// can be influenced by the selection string, which can specify a priority
// list of preferred decoders.
// This returns a list of decoders to try, with the preferred decoders first.
// The selection string corresponds to --vd/--ad directly, and has the
// following syntax:
//   selection = [<entry> ("," <entry>)*]
//       entry = <decoder>       // prefer decoder
//       entry = "-" <decoder>   // exclude a decoder
//       entry = "-"                            // don't add fallback decoders
// Forcing a decoder means it's added even if the codec mismatches.
struct mp_decoder_list *mp_select_decoders(struct mp_log *log,
                                           struct mp_decoder_list *all,
                                           const char *codec,
                                           const char *selection)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    if (!codec)
        codec = "unknown";
    bool stop = false;
    bstr sel = bstr0(selection);
    while (sel.len) {
        bstr entry;
        bstr_split_tok(sel, ",", &entry, &sel);
        if (bstr_equals0(entry, "-")) {
            mp_warn(log, "Excluding codecs is deprecated.\n");
            stop = true;
            break;
        }
        for (int n = 0; n < all->num_entries; n++) {
            struct mp_decoder_entry *cur = &all->entries[n];
            if (bstr_equals0(entry, cur->decoder))
                add_new(list, cur, codec);
        }
    }
    if (!stop) {
        // Add the remaining codecs which haven't been added yet
        for (int n = 0; n < all->num_entries; n++)
            add_new(list, &all->entries[n], codec);
    }
    return list;
}

void mp_append_decoders(struct mp_decoder_list *list, struct mp_decoder_list *a)
{
    for (int n = 0; n < a->num_entries; n++)
        add_new(list, &a->entries[n], NULL);
}

void mp_print_decoders(struct mp_log *log, int msgl, const char *header,
                       struct mp_decoder_list *list)
{
    mp_msg(log, msgl, "%s\n", header);
    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *entry = &list->entries[n];
        mp_msg(log, msgl, "    %s", entry->decoder);
        if (strcmp(entry->decoder, entry->codec) != 0)
            mp_msg(log, msgl, " (%s)", entry->codec);
        mp_msg(log, msgl, " - %s\n", entry->desc);
    }
    if (list->num_entries == 0)
        mp_msg(log, msgl, "    (no decoders)\n");
}
