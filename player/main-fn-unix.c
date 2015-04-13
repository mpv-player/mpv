/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "core.h"

#if HAVE_COCOA_APPLICATION
#include "osdep/macosx_application.h"
#endif

int main(int argc, char *argv[])
{
#if HAVE_COCOA_APPLICATION
    return cocoa_main(mpv_main, argc, argv);
#else
    return mpv_main(argc, argv);
#endif
}
