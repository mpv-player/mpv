#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "mp_msg.h"
#include "edl.h"

#ifdef USE_EDL

int edl_check_mode(void)
{
    if (edl_filename && edl_output_filename)
    {
        return (EDL_ERROR);
    }

    return (1);
}

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
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "Invalid EDL file, cant open '%s' for reading!\n",
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
                    mp_msg(MSGT_CPLAYER, MSGL_WARN,
                           "Invalid EDL line: %s\n", line);
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
                    mp_msg(MSGT_CPLAYER, MSGL_WARN,
                           "Badly formated EDL line [%d]. Discarding!\n",
                           lineCount + 1, line);
                    continue;
                } else
                {
                    if (record_count > 0)
                    {
                        if (start <= (next_edl_record - 1)->stop_sec)
                        {
                            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                                   "Invalid EDL line [%d]: %s",
                                   lineCount, line);
                            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                                   "Last stop position was [%f]; next start is [%f]. Entries must be in chronological order and cannot overlap. Discarding!\n",
                                   (next_edl_record - 1)->stop_sec, start);
                            continue;
                        }
                    }
                    if (stop <= start)
                    {
                        mp_msg(MSGT_CPLAYER, MSGL_WARN,
                               "Invalid EDL line [%d]: %s", lineCount,
                               line);
                        mp_msg(MSGT_CPLAYER, MSGL_WARN,
                               "Stop time must follow start time. Discarding!\n");
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
