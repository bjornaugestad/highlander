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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <assert.h>

#include <meta_array.h>

/*
 * Implementation of the array ADT.
 */
struct array_tag {
	int can_grow;		/* Can the array grow automatically? */
	size_t nused;		/* How many that currently is in use */
	size_t nallocated;	/* How many that currently is allocated */
	void** elements;	/* Pointer to data */
};

array array_new(size_t nmemb, int can_grow)
{
	array p;

	assert(nmemb > 0);

	if ((p = calloc(1, sizeof *p)) == NULL)
		return NULL;

	if ((p->elements = calloc(nmemb, sizeof *p->elements)) == NULL) {
		free(p);
		return NULL;
	}

	p->can_grow = can_grow;
	p->nused = 0;
	p->nallocated = nmemb;

	return p;
}

void array_free(array a, dtor free_fn)
{
	if (a != NULL) {
		if (free_fn) {
			while (a->nused--)
				(*free_fn)(a->elements[a->nused]);
		}

		free(a->elements);
		free(a);
	}
}

size_t array_nelem(array a)
{
	assert(NULL != a);
	return a->nused;
}

void *array_get(array a, size_t ielem)
{
	assert(NULL != a);
	assert(ielem < array_nelem(a));

	if (ielem >= a->nused)
		return NULL;
	
	return a->elements[ielem];
}

int array_extend(array a, size_t nmemb)
{
	void *tmp;
	size_t n, size;

	assert(NULL != a);
	assert(nmemb > 0);

	n = a->nallocated + nmemb;
	size = sizeof *a->elements * n;

	if ((tmp = realloc(a->elements, size)) == NULL)
		return 0;

	a->elements = tmp;
	a->nallocated = n;
	return 1;
}

int array_add(array a, void *elem)
{
	assert(NULL != a);

	if (a->nused == a->nallocated) {
		if (!a->can_grow || !array_extend(a, a->nused))
			return 0;
	}

	a->elements[a->nused++] = elem;
	return 1;
}

#ifdef CHECK_ARRAY
#include <stdio.h>

int main(void)
{
	array a;
	size_t i, nelem = 10000;
	void *dummy;

	/* First a growable array */
	if ((a = array_new(nelem / 10, 1)) == NULL) {
		perror("array_new");
		return 1;
	}

	for (i = 0; i < nelem; i++) {
		dummy = (void*)(i+1);
		if (!array_add(a, dummy)) {
			perror("array_add");
			return 1;
		}
	}

	if (array_nelem(a) != nelem) {
		fprintf(stderr, "Mismatch between number of actual and expected items\n");
		return 1;
	}

	for (i = 0; i < nelem; i++) {
		if (array_get(a, i) == NULL) {
			fprintf(stderr, "Could not get array item %lu\n", (unsigned long)i);
			return 1;
		}
	}

	array_free(a, NULL);

	/* Now for a non-growable array */
	if ((a = array_new(nelem / 10, 0)) == NULL) {
		perror("array_new");
		return 1;
	}

	for (i = 0; i < nelem / 10; i++) {
		dummy = (void*)(i+1);
		if (!array_add(a, dummy)) {
			perror("array_add");
			return 1;
		}
	}

	/* Now we've filled all slots, the next call should fail */
	if (array_add(a, dummy) != 0) {
		fprintf(stderr, "Able to add to array which is full!\n");
		return 1;
	}

	if (array_nelem(a) != nelem / 10) {
		fprintf(stderr, "Mismatch between number of actual and expected items\n");
		return 1;
	}

	for (i = 0; i < nelem / 10; i++) {
		if (array_get(a, i) == NULL) {
			fprintf(stderr, "Could not get array item %lu\n", (unsigned long)i);
			return 1;
		}
	}

	array_free(a, NULL);
	return 0;
}
#endif
