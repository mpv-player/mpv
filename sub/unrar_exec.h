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

#ifndef MPLAYER_UNRAR_EXEC_H
#define MPLAYER_UNRAR_EXEC_H

struct RAR_archive_entry
{
  char           *Name;
  unsigned long  PackSize;
  unsigned long  UnpSize;
  unsigned long  FileCRC;
  unsigned long  FileTime;
  unsigned char  UnpVer;
  unsigned char  Method;
  unsigned long  FileAttr;
};

typedef struct  archivelist
{
  struct RAR_archive_entry item;
  struct archivelist         *next;
} ArchiveList_struct;

extern char* unrar_executable;

int unrar_exec_get(unsigned char **output, unsigned long *size,
                   const char *filename, const char *rarfile);

int unrar_exec_list(const char *rarfile, ArchiveList_struct **list);

void unrar_exec_freelist(ArchiveList_struct *list);

#endif /* MPLAYER_UNRAR_EXEC_H */
