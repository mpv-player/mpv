/* -*- c-basic-offset: 2; indent-tabs-mode: nil -*- */
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

#include "config.h"

#include <stdio.h>
#include <ctype.h>

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#include "cmd_print.h"


typedef struct
{
  uint8_t bits[8];
  uint8_t examined[8];
} cmd_t;


static const char *cmp_op_table[] = {
  NULL, "&", "==", "!=", ">=", ">", "<=", "<"
};
static const char *set_op_table[] = {
  NULL, "=", "<->", "+=", "-=", "*=", "/=", "%=", "rnd", "&=", "|=", "^="
};

static const char *link_table[] = {
  "LinkNoLink",  "LinkTopC",    "LinkNextC",   "LinkPrevC",
  NULL,          "LinkTopPG",   "LinkNextPG",  "LinkPrevPG",
  NULL,          "LinkTopPGC",  "LinkNextPGC", "LinkPrevPGC",
  "LinkGoUpPGC", "LinkTailPGC", NULL,          NULL,
  "RSM"
};

static const char *system_reg_table[] = {
  "Menu Description Language Code",
  "Audio Stream Number",
  "Sub-picture Stream Number",
  "Angle Number",
  "Title Track Number",
  "VTS Title Track Number",
  "VTS PGC Number",
  "PTT Number for One_Sequential_PGC_Title",
  "Highlighted Button Number",
  "Navigation Timer",
  "Title PGC Number for Navigation Timer",
  "Audio Mixing Mode for Karaoke",
  "Country Code for Parental Management",
  "Parental Level",
  "Player Configurations for Video",
  "Player Configurations for Audio",
  "Initial Language Code for Audio",
  "Initial Language Code Extension for Audio",
  "Initial Language Code for Sub-picture",
  "Initial Language Code Extension for Sub-picture",
  "Player Regional Code",
  "Reserved 21",
  "Reserved 22",
  "Reserved 23"
};

static const char *system_reg_abbr_table[] = {
  NULL,
  "ASTN",
  "SPSTN",
  "AGLN",
  "TTN",
  "VTS_TTN",
  "TT_PGCN",
  "PTTN",
  "HL_BTNN",
  "NVTMR",
  "NV_PGCN",
  NULL,
  "CC_PLT",
  "PLT",
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};
    


static unsigned int bits(cmd_t *cmd, int byte, int bit, int count) {
  unsigned int val = 0;
  unsigned int bit_mask;
  
  while(count--) {
    if(bit > 7) {
      bit = 0;
      byte++;
    }
    bit_mask = 0x01 << (7-bit);
    val <<= 1;
    if((cmd->bits[byte]) & bit_mask)
      val |= 1;
    cmd->examined[byte] |= bit_mask;
    bit++;
  }
  return val;
}


static void print_system_reg(unsigned int reg) {
  if(reg < sizeof(system_reg_abbr_table) / sizeof(char *))
    fprintf(stdout, system_reg_table[reg]);
  else
    fprintf(stdout, " WARNING: Unknown system register ");
}

static void print_reg(unsigned int reg) {
  if(reg & 0x80)
    print_system_reg(reg & 0x7f);
  else
    if(reg < 16)
      fprintf(stdout, "g[%u]", reg);
    else
      fprintf(stdout, " WARNING: Unknown general register ");
}

static void print_cmp_op(unsigned int op) {
  if(op < sizeof(cmp_op_table) / sizeof(char *) && cmp_op_table[op] != NULL)
    fprintf(stdout, " %s ", cmp_op_table[op]);
  else
    fprintf(stdout, " WARNING: Unknown compare op ");
}

static void print_set_op(unsigned int op) {
  if(op < sizeof(set_op_table) / sizeof(char *) && set_op_table[op] != NULL)
    fprintf(stdout, " %s ", set_op_table[op]);
  else
    fprintf(stdout, " WARNING: Unknown set op ");
}

static void print_reg_or_data(cmd_t *cmd, unsigned int immediate, int byte) {
  if(immediate) {
    int i = bits(cmd,byte,0,16);
    
    fprintf(stdout, "0x%x", i);
    if(isprint(i & 0xff) && isprint((i>>8) & 0xff))
      fprintf(stdout, " (\"%c%c\")", (char)((i>>8) & 0xff), (char)(i & 0xff));
  } else {
    print_reg(bits(cmd,byte + 1,0,8));
  }
}

static void print_reg_or_data_2(cmd_t *cmd, unsigned int immediate, int byte) {
  if(immediate)
    fprintf(stdout, "0x%x", bits(cmd,byte,1,7));
  else
    fprintf(stdout, "g[%u]", bits(cmd,byte,4,4));
}

static void print_if_version_1(cmd_t *cmd) {
  unsigned int op = bits(cmd,1,1,3);
  
  if(op) {
    fprintf(stdout, "if (");
    print_reg(bits(cmd,3,0,8));
    print_cmp_op(op);
    print_reg_or_data(cmd,bits(cmd,1,0,1), 4);
    fprintf(stdout, ") ");
  }
}

static void print_if_version_2(cmd_t *cmd) {
  unsigned int op = bits(cmd,1,1,3);
  
  if(op) {
    fprintf(stdout, "if (");
    print_reg(bits(cmd,6,0,8));
    print_cmp_op(op);
    print_reg(bits(cmd,7,0,8));
    fprintf(stdout, ") ");
  }
}

static void print_if_version_3(cmd_t *cmd) {
  unsigned int op = bits(cmd,1,1,3);
  
  if(op) {
    fprintf(stdout, "if (");
    print_reg(bits(cmd,2,0,8));
    print_cmp_op(op);
    print_reg_or_data(cmd,bits(cmd,1,0,1), 6);
    fprintf(stdout, ") ");
  }
}

static void print_if_version_4(cmd_t *cmd) {
  unsigned int op = bits(cmd,1,1,3);
  
  if(op) {
    fprintf(stdout, "if (");
    print_reg(bits(cmd,1,4,4));
    print_cmp_op(op);
    print_reg_or_data(cmd,bits(cmd,1,0,1), 4);
    fprintf(stdout, ") ");
  }
}

static void print_if_version_5(cmd_t *cmd) {
  unsigned int op = bits(cmd,1,1,3);
  
  if(op) {
    fprintf(stdout, "if (");
    print_reg(bits(cmd,4,0,8));
    print_cmp_op(op);
    print_reg(bits(cmd,5,0,8));
    fprintf(stdout, ") ");
  }
}

static void print_special_instruction(cmd_t *cmd) {
  unsigned int op = bits(cmd,1,4,4);
  
  switch(op) {
  case 0: // NOP
    fprintf(stdout, "Nop");
    break;
  case 1: // Goto line
    fprintf(stdout, "Goto %u", bits(cmd,7,0,8));
    break;
  case 2: // Break
    fprintf(stdout, "Break");
    break;
  case 3: // Parental level
    fprintf(stdout, "SetTmpPML %u, Goto %u", 
            bits(cmd,6,4,4), bits(cmd,7,0,8));
    break;
  default:
    fprintf(stdout, "WARNING: Unknown special instruction (%u)", 
            bits(cmd,1,4,4));
  }
}

static void print_linksub_instruction(cmd_t *cmd) {
  unsigned int linkop = bits(cmd,7,3,5);
  unsigned int button = bits(cmd,6,0,6);
  
  if(linkop < sizeof(link_table)/sizeof(char *) && link_table[linkop] != NULL)
    fprintf(stdout, "%s (button %u)", link_table[linkop], button);
  else
    fprintf(stdout, "WARNING: Unknown linksub instruction (%u)", linkop);
}

static void print_link_instruction(cmd_t *cmd, int optional) {
  unsigned int op = bits(cmd,1,4,4);
  
  if(optional && op)
    fprintf(stdout, ", ");
  
  switch(op) {
  case 0:
    if(!optional)
      fprintf(stdout, "WARNING: NOP (link)!");
    break;
  case 1:
    print_linksub_instruction(cmd);
    break;
  case 4:
    fprintf(stdout, "LinkPGCN %u", bits(cmd,6,1,15));
    break;
  case 5:
    fprintf(stdout, "LinkPTT %u (button %u)", 
            bits(cmd,6,6,10), bits(cmd,6,0,6));
    break;
  case 6:
    fprintf(stdout, "LinkPGN %u (button %u)", 
            bits(cmd,7,1,7), bits(cmd,6,0,6));
    break;
  case 7:
    fprintf(stdout, "LinkCN %u (button %u)", 
            bits(cmd,7,0,8), bits(cmd,6,0,6));
    break;
  default:
    fprintf(stdout, "WARNING: Unknown link instruction");
  }
}

static void print_jump_instruction(cmd_t *cmd) {
  switch(bits(cmd,1,4,4)) {
  case 1:
    fprintf(stdout, "Exit");
    break;
  case 2:
    fprintf(stdout, "JumpTT %u", bits(cmd,5,1,7));
    break;
  case 3:
    fprintf(stdout, "JumpVTS_TT %u", bits(cmd,5,1,7));
    break;
  case 5:
    fprintf(stdout, "JumpVTS_PTT %u:%u", bits(cmd,5,1,7), bits(cmd,2,6,10));
    break;
  case 6:
    switch(bits(cmd,5,0,2)) {
    case 0:
      fprintf(stdout, "JumpSS FP");
      break;
    case 1:
      fprintf(stdout, "JumpSS VMGM (menu %u)", bits(cmd,5,4,4));
      break;
    case 2:
      fprintf(stdout, "JumpSS VTSM (vts %u, title %u, menu %u)", 
              bits(cmd,4,0,8), bits(cmd,3,0,8), bits(cmd,5,4,4));
      break;
    case 3:
      fprintf(stdout, "JumpSS VMGM (pgc %u)", bits(cmd,2,1,15));
      break;
    }
    break;
  case 8:
    switch(bits(cmd,5,0,2)) {
    case 0:
      fprintf(stdout, "CallSS FP (rsm_cell %u)",
              bits(cmd,4,0,8));
      break;
    case 1:
      fprintf(stdout, "CallSS VMGM (menu %u, rsm_cell %u)",
              bits(cmd,5,4,4), bits(cmd,4,0,8));
      break;
    case 2:
      fprintf(stdout, "CallSS VTSM (menu %u, rsm_cell %u)",
              bits(cmd,5,4,4), bits(cmd,4,0,8));
      break;
    case 3:
      fprintf(stdout, "CallSS VMGM (pgc %u, rsm_cell %u)", 
              bits(cmd,2,1,15), bits(cmd,4,0,8));
      break;
    }
    break;
  default:
    fprintf(stdout, "WARNING: Unknown Jump/Call instruction");
  }
}

static void print_system_set(cmd_t *cmd) {
  int i;
  
  switch(bits(cmd,0,4,4)) {
  case 1: // Set system reg 1 &| 2 &| 3 (Audio, Subp. Angle)
    for(i = 1; i <= 3; i++) {
      if(bits(cmd,2+i,0,1)) {
        print_system_reg((unsigned int)i);
        fprintf(stdout, " = ");
        print_reg_or_data_2(cmd,bits(cmd,0,3,1), 2 + i);
        fprintf(stdout, " ");
      }
    }
    break;
  case 2: // Set system reg 9 & 10 (Navigation timer, Title PGC number)
    print_system_reg(9);
    fprintf(stdout, " = ");
    print_reg_or_data(cmd,bits(cmd,0,3,1), 2);
    fprintf(stdout, " ");
    print_system_reg(10);
    fprintf(stdout, " = %u", bits(cmd,5,0,8)); // ??
    break;
  case 3: // Mode: Counter / Register + Set
    fprintf(stdout, "SetMode ");
    if(bits(cmd,5,0,1))
      fprintf(stdout, "Counter ");
    else
      fprintf(stdout, "Register ");
    print_reg(bits(cmd,5,4,4));
    print_set_op(0x1); // '='
    print_reg_or_data(cmd,bits(cmd,0,3,1), 2);
    break;
  case 6: // Set system reg 8 (Highlighted button)
    print_system_reg(8);
    if(bits(cmd,0,3,1)) // immediate
      fprintf(stdout, " = 0x%x (button no %u)", 
              bits(cmd,4,0,16), bits(cmd,4,0,6));
    else
      fprintf(stdout, " = g[%u]", bits(cmd,5,4,4));
    break;
  default:
    fprintf(stdout, "WARNING: Unknown system set instruction (%u)", 
            bits(cmd,0,4,4));
  }
}

static void print_set_version_1(cmd_t *cmd) {
  unsigned int set_op = bits(cmd,0,4,4);
  
  if(set_op) {
    print_reg(bits(cmd,3,0,8));
    print_set_op(set_op);
    print_reg_or_data(cmd,bits(cmd,0,3,1), 4);
  } else {
    fprintf(stdout, "NOP");
  }
}

static void print_set_version_2(cmd_t *cmd) {
  unsigned int set_op = bits(cmd,0,4,4);
  
  if(set_op) {
    print_reg(bits(cmd,1,4,4));
    print_set_op(set_op);
    print_reg_or_data(cmd,bits(cmd,0,3,1), 2);
  } else {
    fprintf(stdout, "NOP");
  }
}

static void print_set_version_3(cmd_t *cmd) {
  unsigned int set_op = bits(cmd,0,4,4);
  
  if(set_op) {
    print_reg(bits(cmd,1,4,4));
    print_set_op(set_op);
    if(bits(cmd,0,3,1)) { // print_reg_or_data
      unsigned int i = bits(cmd,2,0,16);
      
      fprintf(stdout, "0x%x", i);
      if(isprint(i & 0xff) && isprint((i>>8) & 0xff))
        fprintf(stdout, " (\"%c%c\")", 
                (char)((i>>8) & 0xff), (char)(i & 0xff));
    } else {
      print_reg(bits(cmd,2,0,8));
    }
  } else {
    fprintf(stdout, "NOP");
  }
}

static void print_command(cmd_t *cmd) {
  switch(bits(cmd,0,0,3)) { /* three first bits */
  case 0: // Special instructions
    print_if_version_1(cmd);
    print_special_instruction(cmd);
    break;
  case 1: // Jump/Call or Link instructions
    if(bits(cmd,0,3,1)) {
      print_if_version_2(cmd);
      print_jump_instruction(cmd);
    } else {
      print_if_version_1(cmd);
      print_link_instruction(cmd,0); // must be pressent
    }
    break;
  case 2: // Set System Parameters instructions
    print_if_version_2(cmd);
    print_system_set(cmd);
    print_link_instruction(cmd,1); // either 'if' or 'link'
    break;
  case 3: // Set General Parameters instructions
    print_if_version_3(cmd);
    print_set_version_1(cmd);
    print_link_instruction(cmd,1); // either 'if' or 'link'
    break;
  case 4: // Set, Compare -> LinkSub instructions
    print_set_version_2(cmd);
    fprintf(stdout, ", ");
    print_if_version_4(cmd);
    print_linksub_instruction(cmd);
    break;
  case 5: // Compare -> (Set and LinkSub) instructions
    if(bits(cmd,0,3,1))
      print_if_version_5(cmd);
    else
      print_if_version_1(cmd);
    fprintf(stdout, "{ ");
    print_set_version_3(cmd);
    fprintf(stdout, ", ");
    print_linksub_instruction(cmd);
    fprintf(stdout, " }");
    break;
  case 6: // Compare -> Set, always LinkSub instructions
    if(bits(cmd,0,3,1))
      print_if_version_5(cmd);
    else
      print_if_version_1(cmd);
    fprintf(stdout, "{ ");
    print_set_version_3(cmd);
    fprintf(stdout, " } ");
    print_linksub_instruction(cmd);
    break;
  default:
    fprintf(stdout, "WARNING: Unknown instruction type (%i)", 
            bits(cmd,0,0,3));
  }
}

void cmdPrint_mnemonic(vm_cmd_t *command)  {
  int i, extra_bits;
  cmd_t cmd;
  
  for(i = 0; i < 8; i++) {
    cmd.bits[i] = command->bytes[i];
    cmd.examined[i] = 0;
  }

  print_command(&cmd);
  
  // Check if there still are bits set that were not examined
  extra_bits = 0;
  for(i = 0; i < 8; i++)
    if(cmd.bits[i] & ~ cmd.examined[i]) {
      extra_bits = 1;
      break;
    }
  if(extra_bits) {
    fprintf(stdout, " [WARNING, unknown bits:");
    for(i = 0; i < 8; i++)
      fprintf(stdout, " %02x", cmd.bits[i] & ~ cmd.examined[i]);
    fprintf(stdout, "]");
  }
}

void cmdPrint_CMD(int row, vm_cmd_t *command) {
  int i;

  fprintf(stdout, "(%03d) ", row + 1);
  for(i = 0; i < 8; i++)
    fprintf(stdout, "%02x ", command->bytes[i]);
  fprintf(stdout, "| ");

  cmdPrint_mnemonic(command);
  fprintf(stdout, "\n");
}
