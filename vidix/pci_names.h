/*
 * VIDIX - VIDeo Interface for *niX.
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

#ifndef MPLAYER_PCI_NAMES_H
#define MPLAYER_PCI_NAMES_H

struct device_id_s {
    unsigned short  id;
    const char     *name;
};

struct vendor_id_s {
    unsigned short            id;
    const char               *name;
    const struct device_id_s *dev_list;
};
const char *pci_vendor_name(unsigned short id);
const char *pci_device_name(unsigned short vendor_id,
                            unsigned short device_id);

#endif /* MPLAYER_PCI_NAMES_H */
