#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "mp_msg.h"
#include "edl.h"
#include "help_mp.h"

#ifdef USE_EDL

/**
 *  We can't do -edl and -edlout at the same time
 *  so we check that here.
 *
 *  \return EDL_ERROR on error and 1 otherwise.
 *  \brief Makes sure EDL has been called correctly.
 */

int edl_check_mode(void)
{
    if (edl_filename && edl_output_filename)
    {
        return (EDL_ERROR);
    }

    return (1);
}

/** Calculates the total amount of edl_records we will need
 *  to hold the EDL operations queue, we need one edl_record
 *  for each SKIP and two for each MUTE.
 *  \return Number of necessary EDL entries, EDL_ERROR when file can't be read.
 *  \brief Counts needed EDL entries.
 */

int edl_count_entries(void)
{
    FILE *fd = NULL;
    int entries = 0;
    int action = 0;
    float start = 0;
    float stop = 0;
    char line[100];

    if (edl_filename)
    {
        if ((fd = fopen(edl_filename, "r")) == NULL)
        {
            mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlCantOpenForRead,
                   edl_filename);
            return (EDL_ERROR);
        } else
        {
            while (fgets(line, 99, fd) != NULL)
            {
                if ((sscanf(line, "%f %f %d", &start, &stop, &action)) ==
                    3)
                {
                    if (action == EDL_SKIP)
                        entries += 1;
                    if (action == EDL_MUTE)
                        entries += 2;
                } else
                {
                    mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlNOValidLine, line);
                    return (EDL_ERROR);
                }

            }
        }
    } else
    {
        return (EDL_ERROR);
    }

    return (entries);
}

/** Parses edl_filename to fill EDL operations queue.
 *  \return Number of stored EDL records or EDL_ERROR when file can't be read.
 *  \brief Fills EDL operations queue.
 */

int edl_parse_file(edl_record_ptr edl_records)
{
    FILE *fd;
    char line[100];
    float start, stop;
    int action;
    int record_count = 0;
    int lineCount = 0;
    struct edl_record *next_edl_record = edl_records;

    if (edl_filename)
    {
        if ((fd = fopen(edl_filename, "r")) == NULL)
        {
            return (EDL_ERROR);
        } else
        {
            while (fgets(line, 99, fd) != NULL)
            {
                lineCount++;
                if ((sscanf(line, "%f %f %d", &start, &stop, &action))
                    != 3)
                {
                    mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlBadlyFormattedLine,
                           lineCount + 1);
                    continue;
                } else
                {
                    if (record_count > 0)
                    {
                        if (start <= (next_edl_record - 1)->stop_sec)
                        {
                            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                                   MSGTR_EdlNOValidLine, line);
                            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                                   MSGTR_EdlBadLineOverlap,
                                   (next_edl_record - 1)->stop_sec, start);
                            continue;
                        }
                    }
                    if (stop <= start)
                    {
                        mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlNOValidLine,
                               line);
                        mp_msg(MSGT_CPLAYER, MSGL_WARN, MSGTR_EdlBadLineBadStop);
                        continue;
                    }
                    next_edl_record->action = action;
                    if (action == EDL_MUTE)
                    {
                        next_edl_record->length_sec = 0;
                        next_edl_record->start_sec = start;
                        next_edl_record->stop_sec = start;
                        next_edl_record->mute_state = EDL_MUTE_START;
                        next_edl_record++;
                        (next_edl_record - 1)->next = next_edl_record;
                        next_edl_record->action = action;
                        next_edl_record->length_sec = 0;
                        next_edl_record->start_sec = stop;
                        next_edl_record->stop_sec = stop;
                        next_edl_record->mute_state = EDL_MUTE_END;

                    } else
                    {
                        next_edl_record->length_sec = stop - start;
                        next_edl_record->start_sec = start;
                        next_edl_record->stop_sec = stop;
                    }
                    next_edl_record++;

                    if (record_count >= 0)
                    {
                        (next_edl_record - 1)->next = next_edl_record;
                    }

                    record_count++;
                }
            }

            if (record_count > 0)
            {
                (next_edl_record - 1)->next = NULL;
            }
        }
        fclose(fd);
    } else
    {
        return (EDL_ERROR);
    }

    return (record_count);
}

#endif
