/*
 * Copyright (C) 2003 Mario Strasser <mast@gmx.net>,
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * $Id$
 */

#ifndef PROJECT_DEBUG_H
#define PROJECT_DEBUG_H

#include "im_alive.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define DBG_NONE 0
#define DBG_PANIC 1
#define DBG_ALERT 2
#define DBG_ERROR 3
#define DBG_NOTICE 4
#define DBG_INFO 5
#define DBG_DEBUG 6
#define DBG_TRACE 7
#define DBG_ALL 8

#define LOG_FILE "/var/log/im_alive.log"

/*
#define debug(l,a, ...) debug_print(l, __FILE__, a, ##__VA_ARGS__ )
 */
#define debug(l,a, ...) debug_print(l, __FILE__, __LINE__, a, ##__VA_ARGS__ )

#ifndef PROJECT_DEBUG_C_
#define EXTERN extern
#else
#define EXTERN
#endif


/*
 * set_debug_level() sets the current debug level.
 */
EXTERN int debug_init(configuration *config);
EXTERN void set_debug_level(int level);
EXTERN int get_debug_level();
#define unsuported(a,b) debug(DBG_INFO,"Module:%s Operation:%s is not supported",(a),(b))

/*
 * debug_print() prints the given debug-message if the current debug-level 
 * is greater or equal to the defined level. The format string as well as all
 * further arguments are interpreted as by the printf() function. 
 */
EXTERN void debug_print(int level, char *file, int line, char *format, ...);
#undef EXTERN

#endif /* PROJECT_DEBUG_H */
