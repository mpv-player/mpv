/* $XConsortium: AsmMacros.h /main/13 1996/10/25 11:33:12 kaleb $ */
/*
 * (c) Copyright 1993,1994 by David Wexelblat <dwex@xfree86.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * DAVID WEXELBLAT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of David Wexelblat shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from David Wexelblat.
 *
 */
/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 * This software is furnished under license and may be used and copied only in
 * accordance with the following terms and conditions.  Subject to these
 * conditions, you may download, copy, install, use, modify and distribute
 * this software in source and/or binary form. No title or ownership is
 * transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce and retain
 *    this copyright notice and list of conditions as they appear in the source
 *    file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of Digital
 *    Equipment Corporation. Neither the "Digital Equipment Corporation" name
 *    nor any trademark or logo of Digital Equipment Corporation may be used
 *    to endorse or promote products derived from this software without the
 *    prior written permission of Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied warranties,
 *    including but not limited to, any implied warranties of merchantability,
 *    fitness for a particular purpose, or non-infringement are disclaimed. In
 *    no event shall DIGITAL be liable for any damages whatsoever, and in
 *    particular, DIGITAL shall not be liable for special, indirect,
 *    consequential, or incidental damages or damages for
 *    lost profits, loss of revenue or loss of use, whether such damages arise
 *    in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise, even if
 *    advised of the possibility of such damage.
 *
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/util/AsmMacros.h,v 1.1 2001/11/16 21:13:34 tsi Exp $ */

/*
 *   Modified for readability by Nick Kurshev
*/

#ifndef MPLAYER_ASMMACROS_H
#define MPLAYER_ASMMACROS_H

#if defined(__alpha__)
#include "sysdep/AsmMacros_alpha.h"
#elif defined(__ia64__)
#include "sysdep/AsmMacros_ia64.h"
#elif defined(__sparc__)
#include "sysdep/AsmMacros_sparc.h"
#elif defined( __arm32__ )
#include "sysdep/AsmMacros_arm32.h"
#elif defined(__powerpc__)
#include "sysdep/AsmMacros_powerpc.h"
#else
#include "sysdep/AsmMacros_x86.h"
#endif

#endif /* MPLAYER_ASMMACROS_H */
