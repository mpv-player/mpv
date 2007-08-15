/* -*- c-basic-offset: 2; indent-tabs-mode: nil -*- */
#ifndef CMD_PRINT_H_INCLUDED
#define CMD_PRINT_H_INCLUDED

/*
 * Copyright (C) 2000, 2001, 2002, 2003 Martin Norbäck, Håkan Hjort
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <dvdread/ifo_types.h>

/**
 * Pretty printing of the DVD commands (vm instructions).
 */

#ifdef __cplusplus
extern "C" {
#endif
  
/**
 * Prints a text representation of the commands to stdout.
 *
 * @param command Pointer to the DVD command to be printed.
 */
void cmdPrint_mnemonic(vm_cmd_t *command);
  
/**
 * Prints row, then a hex dump of the command followed by the text
 * representation of the commands, as given by cmdPrint_mnemonic to
 * stdout.
 *
 * @param command Pointer to the DVD command to be printed.  */
void cmdPrint_CMD(int row, vm_cmd_t *command);

#ifdef __cplusplus
};
#endif
#endif /* CMD_PRINT_H_INCLUDED */
