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

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "mp_msg.h"
#include "edl.h"
#include "help_mp.h"

char *edl_filename; // file to extract EDL entries from (-edl)
char *edl_output_filename; // file to put EDL entries in (-edlout)

/**
 *  Allocates a new EDL record and makes sure allocation was successful.
 *
 *  \return New allocated EDL record.
 *  \brief Allocate new EDL record
 */

static edl_record_ptr edl_alloc_new(edl_record_ptr next_edl_record)
{
    edl_record_ptr new_record = calloc(1, sizeof(struct edl_record));
    if (!new_record) {
        mp_msg(MSGT_CPLAYER, MSGL_FATAL, MSGTR_EdlOutOfMem);
        exit(1);
    }

    if (next_edl_record) // if this isn't the first record, tell the previous one what the new one is.
        next_edl_record->next = new_record;
    new_record->prev = next_edl_record;
    new_record->next = NULL;

    return new_record;
}

/**
 *  Goes through entire EDL records and frees all memory.
 *  Assumes next_edl_record is valid or NULL.
 *
 *  \brief Free EDL memory
 */

void free_edl(edl_record_ptr next_edl_record)
{
    edl_record_ptr tmp;
    while (next_edl_record) {
        tmp = next_edl_record->next;
        free(next_edl_record);
        next_edl_record = tmp;
    }
}

/** Parses edl_filename to fill EDL operations queue.
 * Prints out how many EDL operations recorded total.
 *  \brief Fills EDL operations queue.
 */

edl_record_ptr edl_parse_file(void)
{
    FILE *fd;
    char line[100];
    float start, stop;
    int action;
    int record_count = 0;
    int lineCount = 0;
    edl_record_ptr edl_records = NULL;
    edl_record_ptr next_edl_record = NULL;

    if (edl_filename)
    {
        if ((fd = fopen(edl_filename, "r")) == NULL)
        {
            return NULL;
        }

        while (fgets(line, 99, fd) != NULL)
        {
            lineCount++;

            if ((sscanf(line, "%f %f %d", &start, &stop, &action))
                != 3)
            {
                mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlBadlyFormattedLine,
                       lineCount);
                continue;
            }

            if (next_edl_record && start <= next_edl_record->stop_sec)
            {
                mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlNOValidLine, line);
                mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlBadLineOverlap,
                       next_edl_record->stop_sec, start);
                continue;
            }

            if (stop <= start)
            {
                mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlNOValidLine,
                       line);
                mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlBadLineBadStop);
                continue;
            }

            next_edl_record = edl_alloc_new(next_edl_record);

            if (!edl_records) edl_records = next_edl_record;

            next_edl_record->action = action;

            if (action == EDL_MUTE)
            {
                next_edl_record->length_sec = 0;
                next_edl_record->start_sec = start;
                next_edl_record->stop_sec = start;

                next_edl_record = edl_alloc_new(next_edl_record);

                next_edl_record->action = action;
                next_edl_record->length_sec = 0;
                next_edl_record->start_sec = stop;
                next_edl_record->stop_sec = stop;
            } else
            {
                next_edl_record->length_sec = stop - start;
                next_edl_record->start_sec = start;
                next_edl_record->stop_sec = stop;
            }

            record_count++;
        }

        fclose(fd);
    }

    if (edl_records)
        mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_EdlRecordsNo, record_count);
    else
        mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_EdlQueueEmpty);

    return edl_records;
}
