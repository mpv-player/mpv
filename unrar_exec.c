/*
 * List files and extract file from rars by using external executable unrar.
 *
 * Copyright (C) 2005 Jindrich Makovicka <makovick gmail com>
 * Copyright (C) 2007 Ulion <ulion2002 gmail com>
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

#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include "unrar_exec.h"

#include "mp_msg.h"

#define UNRAR_LIST 1
#define UNRAR_EXTRACT 2

char* unrar_executable = NULL;

static FILE* launch_pipe(pid_t *apid, const char *executable, int action,
                         const char *archive, const char *filename)
{
  if (!executable || access(executable, R_OK | X_OK)) return NULL;
  if (access(archive, R_OK)) return NULL;
  {
    int mypipe[2];
    pid_t pid;

    if (pipe(mypipe)) {
        mp_msg(MSGT_GLOBAL, MSGL_ERR, "UnRAR: Cannot create pipe.\n");
        return NULL;
    }

    pid = fork();
    if (pid == 0) {
        /* This is the child process. Execute the unrar executable. */
        close(mypipe[0]);
        // Close MPlayer's stdin, stdout and stderr so the unrar binary
        // can not mess them up.
        // TODO: Close all other files except the pipe.
        close(0); close(1); close(2);
        // Assign new stdin, stdout and stderr and check they actually got the
        // right descriptors.
        if (open("/dev/null", O_RDONLY) != 0 || dup(mypipe[1]) != 1
                || open("/dev/null", O_WRONLY) != 2)
            _exit(EXIT_FAILURE);
        if (action == UNRAR_LIST)
            execl(executable, executable, "v", archive, NULL);
        else if (action == UNRAR_EXTRACT)
            execl(executable, executable, "p", "-inul", "-p-",
                  archive,filename,NULL);
        mp_msg(MSGT_GLOBAL, MSGL_ERR, "UnRAR: Cannot execute %s\n", executable);
        _exit(EXIT_FAILURE);
    }
    if (pid < 0) {
        /* The fork failed.  Report failure.  */
        mp_msg(MSGT_GLOBAL, MSGL_ERR, "UnRAR: Fork failed\n");
        return NULL;
    }
    /* This is the parent process. Prepare the pipe stream. */
    close(mypipe[1]);
    *apid = pid;
    if (action == UNRAR_LIST)
        mp_msg(MSGT_GLOBAL, MSGL_V,
               "UnRAR: call unrar with command line: %s v %s\n",
               executable, archive);
    else if (action == UNRAR_EXTRACT)
        mp_msg(MSGT_GLOBAL, MSGL_V,
               "UnRAR: call unrar with command line: %s p -inul -p- %s %s\n",
               executable, archive, filename);
    return fdopen(mypipe[0], "r");
  }
}

#define ALLOC_INCR 1 * 1024 * 1024
int unrar_exec_get(unsigned char **output, unsigned long *size,
                   const char *filename, const char *rarfile)
{
    int bufsize = ALLOC_INCR, bytesread;
    pid_t pid;
    int status = 0;
    FILE *rar_pipe;

    rar_pipe=launch_pipe(&pid,unrar_executable,UNRAR_EXTRACT,rarfile,filename);
    if (!rar_pipe) return 0;

    *size = 0;

    *output = malloc(bufsize);

    while (*output) {
        bytesread=fread(*output+*size, 1, bufsize-*size, rar_pipe);
        if (bytesread <= 0)
            break;
        *size += bytesread;
        if (*size == bufsize) {
            char *p;
            bufsize += ALLOC_INCR;
            p = realloc(*output, bufsize);
            if (!p)
                free(*output);
            *output = p;
        }
    }
    fclose(rar_pipe);
    pid = waitpid(pid, &status, 0);
    if (!*output || !*size || (pid == -1 && errno != ECHILD) ||
            (pid > 0 && status)) {
        free(*output);
        *output = NULL;
        *size = 0;
        return 0;
    }
    if (bufsize > *size) {
        char *p = realloc(*output, *size);
        if (p)
            *output = p;
    }
    mp_msg(MSGT_GLOBAL, MSGL_V, "UnRAR: got file %s len %lu\n", filename,*size);
    return 1;
}

#define PARSE_NAME 0
#define PARSE_PROPS 1

int unrar_exec_list(const char *rarfile, ArchiveList_struct **list)
{
    char buf[1024], fname[1024];
    char *p;
    pid_t pid;
    int status = 0, file_num = -1, ignore_next_line = 0, state = PARSE_NAME;
    FILE *rar_pipe;
    ArchiveList_struct *alist = NULL, *current = NULL, *new;

    rar_pipe = launch_pipe(&pid, unrar_executable, UNRAR_LIST, rarfile, NULL);
    if (!rar_pipe) return -1;
    while (fgets(buf, sizeof(buf), rar_pipe)) {
        int packsize, unpsize, ratio, day, month, year, hour, min;
        int llen = strlen(buf);
        // If read nothing, we got a file_num -1.
        if (file_num == -1)
            file_num = 0;
        if (buf[llen-1] != '\n')
            // The line is too long, ignore it.
            ignore_next_line = 2;
        if (ignore_next_line) {
            --ignore_next_line;
            state = PARSE_NAME;
            continue;
        }
        // Trim the line.
        while (llen > 0 && strchr(" \t\n\r\v\f", buf[llen-1]))
            --llen;
        buf[llen] = '\0';
        p = buf;
        while (*p && strchr(" \t\n\r\v\f", *p))
            ++p;
        if (!*p) {
            state = PARSE_NAME;
            continue;
        }

        if (state == PARSE_PROPS && sscanf(p, "%d %d %d%% %d-%d-%d %d:%d",
                                           &unpsize, &packsize, &ratio, &day,
                                           &month, &year, &hour, &min) == 8) {
            new = calloc(1, sizeof(ArchiveList_struct));
            if (!new) {
                file_num = -1;
                break;
            }
            if (!current)
                alist = new;
            else
                current->next = new;
            current = new;
            current->item.Name = strdup(fname);
            state = PARSE_NAME;
            if (!current->item.Name) {
                file_num = -1;
                break;
            }
            current->item.PackSize = packsize;
            current->item.UnpSize = unpsize;
            ++file_num;
            continue;
        }
        strcpy(fname, p);
        state = PARSE_PROPS;
    }
    fclose(rar_pipe);
    pid = waitpid(pid, &status, 0);
    if (file_num < 0 || (pid == -1 && errno != ECHILD) ||
            (pid > 0 && status)) {
        unrar_exec_freelist(alist);
        return -1;
    }
    if (!alist)
        return -1;
    *list = alist;
    mp_msg(MSGT_GLOBAL, MSGL_V, "UnRAR: list got %d files\n", file_num);
    return file_num;
}

void unrar_exec_freelist(ArchiveList_struct *list)
{
    ArchiveList_struct* tmp;

    while (list) {
        tmp = list->next;
        free(list->item.Name);
        free(list);
        list = tmp;
    }
}

