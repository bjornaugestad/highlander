/*
    libcoremeta - A library of useful C functions and ADT's
    Copyright (C) 2000-2006 B. Augestad, bjorn.augestad@gmail.com 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef META_CONFIGFILE_H
#define META_CONFIGFILE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct configfile_tag* configfile;

configfile configfile_read(const char *path);

int configfile_exists(configfile cf, const char* name);
int configfile_get_string(configfile cf, const char *name, char *value, size_t cb);
int configfile_get_long(configfile cf, const char *name, long *value);
int configfile_get_ulong(configfile cf, const char *name, unsigned long *value);
int configfile_get_int(configfile cf, const char *name, int *value);
int configfile_get_uint(configfile cf, const char *name, unsigned int *value);

void configfile_free(configfile cf);


#ifdef __cplusplus
}
#endif

#endif

