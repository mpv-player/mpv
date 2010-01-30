/*
 * Direct Hardware Access (DHA) kernel helper
 *
 * Copyright (C) 2002 Alex Beregszaszi <alex@fsn.hu>
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

#ifndef MPLAYER_DHAHELPER_H
#define MPLAYER_DHAHELPER_H

#include <linux/ioctl.h>

/* feel free to change */
#define DEFAULT_MAJOR	180

#define API_VERSION	0x1

typedef struct dhahelper_port_s
{
#define PORT_OP_READ	1
#define PORT_OP_WRITE	2
    int		operation;
    int		size;
    int		addr;
    int		value;
} dhahelper_port_t;

typedef struct dhahelper_memory_s
{
#define MEMORY_OP_MAP	1
#define MEMORY_OP_UNMAP	2
    int		operation;
    int		start;
    int		offset;
    int		size;
    int		ret;
#define MEMORY_FLAG_NOCACHE 1
    int		flags;
} dhahelper_memory_t;

typedef struct dhahelper_mtrr_s
{
#define MTRR_OP_ADD	1
#define MTRR_OP_DEL	2
    int		operation;
    int		start;
    int		size;
    int		type;
} dhahelper_mtrr_t;

typedef struct dhahelper_pci_s
{
#define PCI_OP_READ	1
#define PCI_OP_WRITE	1
    int		operation;
    int		bus;
    int		dev;
    int		func;
    int		cmd;
    int		size;
    int		ret;
} dhahelper_pci_t;

#define DHAHELPER_GET_VERSION	_IOW('D', 0, int)
#define DHAHELPER_PORT		_IOWR('D', 1, dhahelper_port_t)
#define DHAHELPER_MEMORY	_IOWR('D', 2, dhahelper_memory_t)
#define DHAHELPER_MTRR		_IOWR('D', 3, dhahelper_mtrr_t)
#define DHAHELPER_PCI		_IOWR('D', 4, dhahelper_pci_t)

#endif /* MPLAYER_DHAHELPER_H */
