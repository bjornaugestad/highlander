#include <stdlib.h>
#include <assert.h>

#include <meta_array.h>

/**
 * Implementation of the array ADT.
 */
struct array_tag {
	int can_grow;		/* Can the array grow automatically? */
	size_t cUsed;		/* How many that currently is in use */
	size_t cAllocated;	/* How many that currently is allocated */
	void** elements;	/* Pointer to data */
};

array array_new(size_t nmemb, int can_grow)
{
	array p;

	assert(nmemb > 0);
	
	if( (p = mem_calloc(1, sizeof *p)) == NULL)
		;
	else if( (p->elements = mem_calloc(nmemb, sizeof *p->elements)) == NULL) {
		mem_free(p);
		p = NULL;
	}
	else {
		p->can_grow = can_grow;
		p->cUsed = 0;
		p->cAllocated = nmemb;
	}

	return p;
}

void array_free(array a, dtor free_fn)
{
	if(a != NULL) {
		if(free_fn) {
			while(a->cUsed--)
				(*free_fn)(a->elements[a->cUsed]);
		}

		mem_free(a->elements);
		mem_free(a);
	}
}

size_t array_nelem(array a)
{
	assert(NULL != a);
	return a->cUsed;
}

void* array_get(array a, size_t ielem)
{
	assert(NULL != a);
	assert(ielem < array_nelem(a));

	if(ielem >= a->cUsed)
		return NULL;
	else
		return a->elements[ielem];
}

int array_extend(array a, size_t nmemb)
{
	void** pnew;
	size_t n, cb;

	assert(NULL != a);
	assert(nmemb > 0);

	n = a->cAllocated + nmemb;
	cb = sizeof(*a->elements) * n;

	if( (pnew = mem_realloc(a->elements, cb)) == NULL)
		return 0;

	a->elements = pnew;
	a->cAllocated = n;
	return 1;
}

int array_add(array a, void* elem)
{
	assert(NULL != a);
	assert(NULL != elem);

	if(a->cUsed == a->cAllocated) {
		if(!a->can_grow || !array_extend(a, a->cUsed)) 
			return 0;
	}

	a->elements[a->cUsed++] = elem;
	return 1; 
}

#ifdef CHECK_ARRAY
#include <stdio.h>

int main(void)
{
	array a;
	size_t i, nelem = 10000;
	void* dummy;

	/* First a growable array */
	if( (a = array_new(nelem / 10, 1)) == NULL) {
		perror("array_new");
		return 1;
	}

	for(i = 0; i < nelem; i++) {
		dummy = (void*)(i+1);
		if(!array_add(a, dummy)) {
			perror("array_add");
			return 1;
		}
	}

	if(array_nelem(a) != nelem) {
		fprintf(stderr, "Mismatch between number of actual and expected items\n");
		return 1;
	}

	for(i = 0; i < nelem; i++) {
		if(array_get(a, i) == NULL) {
			fprintf(stderr, "Could not get array item %lu\n", (unsigned long)i);
			return 1;
		}
	}

	array_free(a, NULL);

	/* Now for a non-growable array */
	if( (a = array_new(nelem / 10, 0)) == NULL) {
		perror("array_new");
		return 1;
	}

	for(i = 0; i < nelem / 10; i++) {
		dummy = (void*)(i+1);
		if(!array_add(a, dummy)) {
			perror("array_add");
			return 1;
		}
	}

	/* Now we've filled all slots, the next call should fail */
	if(array_add(a, dummy) != 0) {
		fprintf(stderr, "Able to add to array which is full!\n");
		return 1;
	}

	if(array_nelem(a) != nelem / 10) {
		fprintf(stderr, "Mismatch between number of actual and expected items\n");
		return 1;
	}

	for(i = 0; i < nelem / 10; i++) {
		if(array_get(a, i) == NULL) {
			fprintf(stderr, "Could not get array item %lu\n", (unsigned long)i);
			return 1;
		}
	}

	array_free(a, NULL);
	return 0;
}
#endif

