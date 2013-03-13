/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>


#include <meta_slotbuf.h>

struct slotbuf_tag {
	void** data;
	size_t size;
	int can_overwrite;
	pthread_mutex_t lock;
	dtor pfn;
};

slotbuf slotbuf_new(size_t size, int can_overwrite, dtor pfn)
{
	slotbuf p;

	assert(size > 0);

	if( (p = mem_malloc(sizeof *p)) == NULL
	||  (p->data = mem_calloc(size, sizeof *p->data)) == NULL) {
		mem_free(p);
		p = NULL;
	}
	else {
		p->can_overwrite = can_overwrite;
		p->size = size;
		pthread_mutex_init(&p->lock, NULL);
		p->pfn = pfn;
	}

	return p;
}

void slotbuf_free(slotbuf p)
{
	if(p != NULL) {
		size_t i;
		for(i = 0; i < p->size; i++) {
			if(p->data[i] != NULL && p->pfn != NULL) 
				p->pfn(p->data[i]);
		}

		mem_free(p->data);
		pthread_mutex_destroy(&p->lock);
		mem_free(p);
	}
}

int slotbuf_set(slotbuf p, size_t i, void* value)
{
	size_t idx;
	assert(p != NULL);
	assert(value != NULL);
	
	idx = i % p->size;
	if(p->data[idx] != NULL) {
		if(!p->can_overwrite) 
			return 0;
		else if(p->pfn != NULL)
			p->pfn(p->data[idx]);
	}

	p->data[idx] = value;
	return 1;
}

void* slotbuf_get(slotbuf p, size_t i)
{
	size_t idx;
	void *data;

	assert(p != NULL);
	
	idx = i % p->size;
	data = p->data[idx];
	p->data[idx] = NULL;
	return data;
}

size_t slotbuf_nelem(slotbuf p)
{
	size_t i, n;

	assert(p != NULL);
	for(i = n = 0; i < p->size; i++) 
		if(p->data[i] != NULL)
			n++;

	return n;
}

void* slotbuf_peek(slotbuf p, size_t i)
{
	size_t idx;
	assert(p != NULL);
	
	idx = i % p->size;
	return p->data[idx];
}

int slotbuf_has_data(slotbuf p, size_t i)
{
	size_t idx;
	assert(p != NULL);
	
	idx = i % p->size;
	return p->data[idx] != NULL;
}

void slotbuf_lock(slotbuf p)
{
	assert(p != NULL);
	if(pthread_mutex_lock(&p->lock))
		abort();
}

void slotbuf_unlock(slotbuf p)
{
	assert(p != NULL);
	if(pthread_mutex_unlock(&p->lock))
		abort();
}


