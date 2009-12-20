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

#include <stddef.h>
#include "pci_names.h"
#include "pci_vendor_ids.h"

const char *pci_vendor_name(unsigned short id)
{
    unsigned i;
    for (i = 0; i < sizeof(vendor_ids) / sizeof(struct vendor_id_s); i++) {
        if (vendor_ids[i].id == id)
            return vendor_ids[i].name;
    }
    return NULL;
}

const char *pci_device_name(unsigned short vendor_id, unsigned short device_id)
{
    unsigned i, j;
    for (i = 0; i < sizeof(vendor_ids) / sizeof(struct vendor_id_s); i++) {
        if (vendor_ids[i].id == vendor_id) {
            j = 0;
            while (vendor_ids[i].dev_list[j].id != 0xFFFF) {
                if (vendor_ids[i].dev_list[j].id == device_id)
                    return vendor_ids[i].dev_list[j].name;
                j++;
            };
            break;
        }
    }
    return NULL;
}
