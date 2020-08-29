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

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#define PROJECT_DEBUG_C
#include "im_alive.h"
#include "debug.h"

/* current debug level */
static int debug_level = 0;
static FILE *debug_file = NULL;
static FILE *debug_stderr = NULL;
static char *debug_levels[] = {
        "++++    ",
        "PANIC   ",
        "ALERT   ",
        "ERROR   ",
        "NOTICE  ",
        "INFO    ",
        "DEBUG   ",
        "TRACE   ",
        "-----   "
        };

int debug_init(configuration *config) {
    // on startup use default log mode
    if (!config) {
        debug_level=DBG_ERROR; //
        debug_stderr = stderr; // on no config enable stderr output
    } else {
        set_debug_level(config->loglevel);
        debug_stderr = (config->verbose==0)? NULL:stderr;

        if(debug_file) fclose(debug_file); // close file before repoen. may fail
        debug_file=fopen(config->logfile,"w");
        if (!debug_file) { // on error opening logfile, use stderr
            debug_stderr=stderr;
            return -1;
        }
    }
    return 0;
}

void set_debug_level(int level) {
    if (level<0) level=0;
    if (level>8) level=8;
    debug_level = level;
}

int get_debug_level() {
  return debug_level;
}

char *getTime() {
	static char myDate[24]; /* YYYY/MM/DD-hh:mm:ss */
	int res;
	time_t t = time(NULL);
	struct tm *myTime=localtime(&t);
	/* res = strftime(myDate,24,"%Y/%m/%d-%H:%M:%S ",myTime); */
	res = strftime(myDate,24,"%H:%M:%S ",myTime);
	myDate[res]='\0';
	return myDate;
}

void debug_print(int level, char *file, int line, char *format, ...) {
    va_list ap;
    if (debug_level<level) return;
    if (debug_stderr) {
        fprintf(debug_stderr,"%s: %s - %s:%d: ", debug_levels[level],getTime(), file, line); // preamble
        va_start(ap, format);
        vfprintf(debug_stderr,format, ap); // message
        va_end(ap);
        fprintf(debug_stderr,"\n"); // postamble
    }
    if (debug_file) {
        fprintf(debug_file,"%s: %s - %s:%d: ", debug_levels[level],getTime(), file, line); // preamble
        va_start(ap, format);
        vfprintf(debug_file,format, ap); // message
        va_end(ap);
        fprintf(debug_file,"\n"); // postamble
        fflush(debug_file);
    }
}

#undef __DEBUG_C_
