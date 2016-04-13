/*
 * Copyright (C) 2009-2103 Ron Pedde (ron@pedde.com)
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

#include <stdio.h>
#include <stdarg.h>

#include "debug.h"

static int debug_threshold = 2;

void debug_level(int newlevel) {
    debug_threshold = newlevel;
}


void debug_printf(int level, char *format, ...) {
    va_list args;
    if(level > debug_threshold)
        return;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
