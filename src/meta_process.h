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
#ifndef META_PROCESS_H
#define META_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct process_tag* process;

process process_new(const char* appname);
void process_free(process p);

int process_shutting_down(process p);

int process_set_rootdir(process p, const char* path);
int process_set_username(process p, const char* username);

int process_add_object_to_start(
	process p,
	void *object,
	int do_func(void *),
	int undo_func(void *),
	int run_func(void *),
	int shutdown_func(void *));

int process_start(process p, int fork_and_close);
int process_wait_for_shutdown(process p);
int process_get_exitcode(process p, void* object);


#ifdef __cplusplus
}
#endif

#endif /* guard */

