/*
 * This file was ported to MPlayer from xine CVS asmrp.h,v 1.1 2002/12/12 22:14:54
 */

/*
 * Copyright (C) 2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * a parser for real's asm rules
 *
 * grammar for these rules:
 *

   rule_book  = { '#' rule ';'}
   rule       = condition {',' assignment}
   assignment = id '=' const
   const      = ( number | string )
   condition  = comp_expr { ( '&&' | '||' ) comp_expr }
   comp_expr  = operand { ( '<' | '<=' | '==' | '>=' | '>' ) operand }
   operand    = ( '$' id | num | '(' condition ')' )

 */

#ifndef HAVE_ASMRP_H
#define HAVE_ASMRP_H

#define MAX_RULEMATCHES 16

int asmrp_match (const char *rules, int bandwidth, int *matches) ;

#endif
