/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include "talloc.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "codecs.h"

void mp_add_decoder(struct mp_decoder_list *list, const char *family,
                    const char *codec, const char *decoder, const char *desc)
{
    struct mp_decoder_entry entry = {
        .family = talloc_strdup(list, family),
        .codec = talloc_strdup(list, codec),
        .decoder = talloc_strdup(list, decoder),
        .desc = talloc_strdup(list, desc),
    };
    MP_TARRAY_APPEND(list, list->entries, list->num_entries, entry);
}

static void mp_add_decoder_entry(struct mp_decoder_list *list,
                                 struct mp_decoder_entry *entry)
{
    mp_add_decoder(list, entry->family, entry->codec, entry->decoder,
                   entry->desc);
}

static struct mp_decoder_entry *find_decoder(struct mp_decoder_list *list,
                                             bstr family, bstr decoder)
{
    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *cur = &list->entries[n];
        if (bstr_equals0(decoder, cur->decoder) &&
            bstr_equals0(family, cur->family))
            return cur;
    }
    return NULL;
}

// Add entry, but only if it's not yet on the list, and if the codec matches.
// If codec == NULL, don't compare codecs.
static void add_new(struct mp_decoder_list *to, struct mp_decoder_entry *entry,
                    const char *codec)
{
    if (!entry || (codec && strcmp(entry->codec, codec) != 0))
        return;
    if (!find_decoder(to, bstr0(entry->family), bstr0(entry->decoder)))
        mp_add_decoder_entry(to, entry);
}

// Select a decoder from the given list for the given codec. The selection
// can be influenced by the selection string, which can specify a priority
// list of preferred decoders.
// This returns a list of decoders to try, with the preferred decoders first.
// The selection string corresponds to --vd/--ad directly, and has the
// following syntax:
//   selection = [<entry> ("," <entry>)*]
//       entry = <family> ":" <decoder>         // prefer decoder
//       entry = <family> ":*"                  // prefer all decoders
//       entry = "+" <family> ":" <decoder>     // force a decoder
//       entry = "-" <family> ":" <decoder>     // exclude a decoder
//       entry = "-"                            // don't add fallback decoders
// Forcing a decoder means it's added even if the codec mismatches.
struct mp_decoder_list *mp_select_decoders(struct mp_decoder_list *all,
                                           const char *codec,
                                           const char *selection)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    struct mp_decoder_list *remove = talloc_zero(NULL, struct mp_decoder_list);
    if (!codec)
        codec = "unknown";
    bool stop = false;
    bstr sel = bstr0(selection);
    while (sel.len) {
        bstr entry;
        bstr_split_tok(sel, ",", &entry, &sel);
        if (bstr_equals0(entry, "-")) {
            stop = true;
            break;
        }
        bool force = bstr_eatstart0(&entry, "+");
        bool exclude = !force && bstr_eatstart0(&entry, "-");
        struct mp_decoder_list *dest = exclude ? remove : list;
        bstr family, decoder;
        if (!bstr_split_tok(entry, ":", &family, &decoder)) {
            family = entry;
            decoder = bstr0("*");
        }
        if (bstr_equals0(decoder, "*")) {
            for (int n = 0; n < all->num_entries; n++) {
                struct mp_decoder_entry *cur = &all->entries[n];
                if (bstr_equals0(family, cur->family))
                    add_new(dest, cur, codec);
            }
        } else {
            add_new(dest, find_decoder(all, family, decoder),
                    force ? NULL : codec);
        }
    }
    if (!stop) {
        // Add the remaining codecs which haven't been added yet
        for (int n = 0; n < all->num_entries; n++)
            add_new(list, &all->entries[n], codec);
    }
    for (int n = 0; n < remove->num_entries; n++) {
        struct mp_decoder_entry *ex = &remove->entries[n];
        struct mp_decoder_entry *del =
            find_decoder(list, bstr0(ex->family), bstr0(ex->decoder));
        if (del) {
            int index = del - &list->entries[0];
            MP_TARRAY_REMOVE_AT(list->entries, list->num_entries, index);
        }
    }
    talloc_free(remove);
    return list;
}

void mp_print_decoders(struct mp_log *log, int msgl, const char *header,
                       struct mp_decoder_list *list)
{
    mp_msg(log, msgl, "%s\n", header);
    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *entry = &list->entries[n];
        mp_msg(log, msgl, "    %s:%s", entry->family, entry->decoder);
        if (strcmp(entry->decoder, entry->codec) != 0)
            mp_msg(log, msgl, " (%s)", entry->codec);
        mp_msg(log, msgl, " - %s\n", entry->desc);
    }
    if (list->num_entries == 0)
        mp_msg(log, msgl, "    (no decoders)\n");
}
