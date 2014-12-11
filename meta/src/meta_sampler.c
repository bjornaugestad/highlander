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

#include <limits.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <meta_common.h>

#include "meta_sampler.h"

/* We use a struct from day 1 so that we easily can add e.g.
 * values for aggregate functions later.
 */
struct entity {
	long long* data;
};

/*
 * Implementation details:
 * -----------------------
 * Data entries for all entities share the same index, since we
 * sample values for all entities simultaneously.
 */
struct sampler_tag {
	struct entity* entities;

	/* The number of entities we sample data for */
	size_t nentity;

	/* The number of values we store before wrapping */
	size_t nvalue;

	pthread_rwlock_t lock;

	/* Index to use when adding new values */
	size_t idx;

	/* When the sampling  was done */
	time_t* times;

	/* Number of samples performed */
	size_t samplecount;
};


sampler sampler_new(size_t entities, size_t values)
{
	sampler s;
	size_t i;

	assert(entities > 0);
	assert(values > 0);

	/* IMPORTANT: entities MUST be calloc()ed for error handling to work . */
	if ((s = calloc(1, sizeof *s)) == NULL
	|| (s->times = malloc(sizeof *s->times * values)) == NULL
	|| (s->entities = calloc(entities, sizeof *s->entities)) == NULL) {
		goto err;
	}
	else {
		size_t j, cb;

		s->nentity = entities;
		s->nvalue = values;
		s->samplecount = 0;

		/* A little 'trick': sampler_start_update() will increment idx,
		 * so to be able to add data in slot 0, we initialize it to
		 * nvalue-1. That way sampler_start_update() will wrap to 0.
		 */
		s->idx = s->nvalue - 1;

		cb = values * sizeof(*s->entities[0].data);
		for (i = 0; i < entities; i++) {
			if ((s->entities[i].data = malloc(cb)) == NULL)
				goto err;

			/* Set all values to 'invalid' */
			for (j = 0; j < values; j++) {
				s->times[j] = (time_t)-1;
				s->entities[i].data[j] = LLONG_MIN;
			}
		}
	}

	/* Last thing we do to avoid having a flag indicating if we
	 * did this or not. */
	if (pthread_rwlock_init(&s->lock, NULL))
		goto err;

	return s;

/* Here we free allocated memory, if any */
err:
	if (s != NULL) {
		if (s->entities != NULL) {
			for (i = 0; i < entities; i++)
				free(s->entities[i].data);
		}

		free(s->entities);
		free(s->times);
		pthread_rwlock_destroy(&s->lock);
		free(s);
	}
	assert(0);
	return NULL;
}

sampler sampler_dup(sampler src)
{
	sampler dest;

	assert(src != NULL);

	if ((dest = sampler_new(src->nentity, src->nvalue)) == NULL)
		return NULL;

	sampler_copy(dest, src);
	return dest;
}

void sampler_copy(sampler dest, sampler src)
{
	size_t i, j;

	assert(dest != NULL);
	assert(src != NULL);
	assert(dest->nvalue == src->nvalue);
	assert(dest->nentity == src->nentity);

	/* Lock the src so that noone writes to it while copying. */
	pthread_rwlock_wrlock(&src->lock);

	/* Copy all the data sampled . There are so many pointers,
	 * memcpy() is not the best option. We copy each element
	 * manually (for now). */
	for (i = 0; i < src->nentity; i++) {
		for (j = 0; j < src->nvalue; j++)
			dest->entities[i].data[j] = src->entities[i].data[j];
	}

	/* copy misc data */
	dest->idx = src->idx;
	dest->samplecount = src->samplecount;
	size_t cb = sizeof *src->times * src->nvalue;
	memcpy(dest->times, src->times, cb);

	pthread_rwlock_unlock(&src->lock);
}

void sampler_free(sampler s)
{
	size_t i;

	if (s != NULL) {
		pthread_rwlock_destroy(&s->lock);
		free(s->times);

		for (i = 0; i < s->nentity; i++)
			free(s->entities[i].data);

		free(s->entities);
		free(s);
	}
}

size_t sampler_samplecount(sampler s)
{
	size_t n;

	assert(s != NULL);
	if (s->samplecount < s->nvalue)
		n = s->samplecount;
	else
		n = s->nvalue;

	return n;
}

void sampler_start_update(sampler s, time_t t)
{
	size_t i;

	assert(s != NULL);

	/* Lock the object */
	pthread_rwlock_wrlock(&s->lock);

	/* use the next stot to store data for this update,
	 * reusing previous slots if needed */
	s->idx++;
	if (s->idx == s->nvalue) {
		s->idx = 0;
	}

	/* Clear all existing values, if any */
	for (i = 0; i < s->nentity; i++)
		s->entities[i].data[s->idx] = LLONG_MIN;

	/* Update the time entry */
	s->times[s->idx] = t;
}

void sampler_commit(sampler s)
{
	assert(s != NULL);

	s->samplecount++;
	pthread_rwlock_unlock(&s->lock);
}

void sampler_add(sampler s, size_t entity_id, long long value)
{
	assert(s != NULL);
	assert(entity_id < s->nentity);
	assert(value != LLONG_MIN);

	s->entities[entity_id].data[s->idx] = value;
}

/* The index i is zero based, but we must remap that index
 * into a time-sequential index. It basically goes like this,
 * using 10 values and the letters B-K as examples:
 * 1) The data can be stored as FGHIJKBCDE
 * 2) The letter A is missing since we've added 11 samples
 *	  and A was therefore overwritten with K.
 * 3) index 0 should be for the letter B, which is the oldest entry.
 * 4) Letter B has index 6.
 *
 * We already store the idx, which tells us where to write the next
 * time. That variable is updated on every _commit(), which means that
 * idx now points to the virtual index 0. Now we must handle wrapping,
 * as index 9 is for the letter K, which has physical index 5.
 * The formula below returns,
 *
 *			idx = (s->idx + i) % s->nvalue;
 *
 * for the different values (0..9), assuming
 * that s->idx == 5, the following values:
 * (0 + 5) % 10 == 5	== K
 * (1 + 5) % 10 == 6	== B
 * (2 + 5) % 10 == 7	== C
 * (3 + 5) % 10 == 8	== D
 * (4 + 5) % 10 == 9	== E
 * (5 + 5) % 10 == 0	== F
 * (6 + 5) % 10 == 1	== G
 * (7 + 5) % 10 == 2	== H
 * (8 + 5) % 10 == 3	== I
 * (9 + 5) % 10 == 4	== J
 *
 * This formula is off by one, which means that we must add 1, but before
 * or after the modulus? Does it even matter? :-) Yes, of course it
 * matters, we *must* add it prior to %, if not we will never be able
 * to return data for index 0 since the smallest index always will be 1.
 *
 */
static inline size_t map_index(sampler s, size_t i)
{
	size_t idx;

	assert(i < s->nvalue);

	if (s->samplecount < s->nvalue)
		idx = i;
	else
		idx = (s->idx + i + 1) % s->nvalue;

	return idx;
}

void sampler_start_read(sampler s)
{
	assert(s != NULL);
	pthread_rwlock_rdlock(&s->lock);
}

void sampler_stop_read(sampler s)
{
	assert(s != NULL);
	pthread_rwlock_unlock(&s->lock);
}

time_t sampler_time(sampler s, size_t i)
{
	size_t idx;
	time_t t;

	assert(s != NULL);
	assert(i < s->nvalue);

	idx = map_index(s, i);
	t = s->times[idx];

	assert(t != (time_t)-1);
	return t;
}

int sampler_get(sampler s, size_t entity_id, size_t i, long long* pval)
{
	size_t idx;

	assert(s != NULL);
	assert(entity_id < s->nentity);
	assert(i < s->nvalue);
	assert(pval != NULL);

	idx = map_index(s, i);
	*pval = s->entities[entity_id].data[idx];

	return *pval != LLONG_MIN;
}

int sampler_avg(sampler s, size_t eid, size_t from, size_t to, long long* pval)
{
	size_t i, valid_nelem = 0;
	long long sum = 0LL;

	for (i = from; i < to; i++) {
		long long val;
		int ok = sampler_get(s, eid, i, &val);
		if (ok) {
			valid_nelem++;
			sum += val;
		}
	}

	if (valid_nelem > 0) {
		*pval = sum	 / valid_nelem;
		return 1;
	}
	else
		return 0;
}

int sampler_min(sampler s, size_t eid, size_t from, size_t to, long long* pval)
{
	size_t i, valid_nelem = 0;

	*pval = LLONG_MAX;
	for (i = from; i < to; i++) {
		long long val;
		int ok = sampler_get(s, eid, i, &val);
		if (ok) {
			if (*pval > val)
				*pval = val;
		}
	}

	return valid_nelem > 0;
}

int sampler_max(sampler s, size_t eid, size_t from, size_t to, long long* pval)
{
	size_t i, valid_nelem = 0;

	*pval = LLONG_MIN;
	for (i = from; i < to; i++) {
		long long val;
		int ok = sampler_get(s, eid, i, &val);
		if (ok) {
			if (*pval < val)
				*pval = val;
		}
	}

	return valid_nelem > 0;
}

int sampler_first(sampler s, size_t eid, size_t from, size_t to, long long* pval)
{
	size_t i;

	for (i = from; i < to; i++) {
		long long val;
		if (sampler_get(s, eid, i, &val)) {
			*pval = val;
			return 1;
		}
	}

	/* Found no valid items */
	return 0;
}

int sampler_last(sampler s, size_t eid, size_t from, size_t to, long long* pval)
{
	size_t i;
	int status = 0;

	for (i = from; i < to; i++) {
		long long val;
		if (sampler_get(s, eid, i, &val)) {
			*pval = val;
			status = 1;
		}
	}

	return status;
}

static inline int
aggregate_any(sampler s, size_t eid, size_t from, size_t to, int aggval, long long* pval)
{
	switch (aggval) {
		case SAMPLER_AGG_MIN:
			return sampler_min(s, eid, from, to, pval);

		case SAMPLER_AGG_MAX:
			return sampler_max(s, eid, from, to, pval);

		case SAMPLER_AGG_AVG:
			return sampler_avg(s, eid, from, to, pval);

		case SAMPLER_AGG_FIRST:
			return sampler_first(s, eid, from, to, pval);

		case SAMPLER_AGG_LAST:
			return sampler_last(s, eid, from, to, pval);

		default:
			assert(0 && "Unknown aggregate type");
			break;
	}

	return 0;
}

int sampler_aggregate(
	sampler dest,
	sampler src,
	size_t nsamples,
	unsigned int resolution,
	int aggval)
{
	size_t i;

	assert(dest != NULL);
	assert(src != NULL);

	for (i = 0; i < nsamples; i++) {
		size_t from = i * resolution;
		size_t to = from + resolution;
		size_t eid;

		time_t start = sampler_time(src, from);

		sampler_start_update(dest, start);
		for (eid = 0; eid < src->nentity; eid++) {
			long long val = 0;

			if (aggregate_any(src, eid, from, to, aggval, &val))
				sampler_add(dest, eid, val);
		}

		sampler_commit(dest);
	}

	return 1;
}

#ifdef CHECK_SAMPLER
#include <unistd.h>

static const size_t nentity = 1, nsamples = 3600;
#if 1
static sampler sampled_data;
static int shutting_down;

static void *writer(void *arg)
{
	size_t i;
	time_t now;

	(void)arg;
	while (!shutting_down) {
		now = time(NULL);
		sampler_start_update(sampled_data, now);
		for (i = 0; i < nentity; i++) {
			long long val = rand();
			sampler_add(sampled_data, i, val);
		}

		sampler_commit(sampled_data);
		sleep(1);
	}

	return NULL;
}

static void *reader(void *arg)
{
	int* id = arg;

	while (!shutting_down) {
		size_t i, csamples;

		sampler_start_read(sampled_data);
		csamples = sampler_samplecount(sampled_data);
		for (i = 0; i < nentity; i++) {
			long long val;

			if (!sampler_avg(sampled_data, i, 0, csamples, &val)) {
				fprintf(stderr, "reader(%d), entity %zu: No data\n", *id, i);
			}
			#if 0
			else {
				fprintf(stderr, "reader(%d), entity %zu: avg: %lld\n",
					*id, i, val);
			}
			#else
			(void)id;
			#endif
		}

		sampler_stop_read(sampled_data);
		sleep(1);
	}

	return 0;
}
#endif


int main(void)
{
	size_t i;

	#if 1
	{
		sampler s;
		size_t niter = 10;
		/* Test the new/free functions for performance and leaks */
		for (i = 0; i < niter; i++) {
			if ((s = sampler_new(nentity, nsamples)) == NULL)
				abort();

			sampler_free(s);
		}
	}
	#endif

#define TEST_AGGREGATE
#ifdef TEST_AGGREGATE
	/* Test the aggregate functions. Here's how:
	 * 1) Create 3600 samples, one for each second in an hour.
	 * 2) Aggregate that up to 1 sampler with 60 minutes
	 * 3) Aggregate that up to 1 sampler with 1 hour.
	 * The data will be dummy data to speed things up.
	 */
	 {
		sampler secs, minutes, hour;
		time_t now;
		size_t eid;
		long long val;

		#if 0
		now = time(NULL) - 3600;
		#else
		// We want more readable time values while testing.
		now = 0;
		#endif
		secs = sampler_new(nentity, 3600);
		minutes = sampler_new(nentity, 60);
		hour = sampler_new(nentity, 24);

		for (i = 0; i < 3600; i++) {
			/* Add values for this second */
			sampler_start_update(secs, now++);

			for (eid = 0; eid < nentity; eid++) {
				val = i % 10;
				sampler_add(secs, eid, val);
			}
			sampler_commit(secs);
		}

		/* Verify that we have data for all 3600 seconds. */
		#if 1
		fprintf(stderr, "Secs: Sample count:%zu, nvalue %zu\n",
			secs->samplecount, secs->nvalue);
		for (i = 0; i < 3600; i++) {
			if (secs->entities[0].data[i] == LLONG_MIN) {
				fprintf(stderr, "WTF?\n");
				abort();
			}
		}

		/* Now we know that we have values for each and every second */
		#endif

		fprintf(stderr, "Aggregating secs->minutes\n");
		sampler_aggregate(minutes, secs, 60, 60, SAMPLER_AGG_AVG);

		#if 1
		fprintf(stderr, "Dumping minutes for eid 0\n");
		fprintf(stderr, "Minutes: Sample count:%zu, nvalue %zu\n",
			minutes->samplecount, minutes->nvalue);
		for (i = sampler_samplecount(minutes) - 3; i < sampler_samplecount(minutes); i++) {
			if (sampler_get(minutes, 0, i, &val))
				fprintf(stderr, "Minute: %zu:  Value: %lld\n", i, val);
			else
				fprintf(stderr, "Minute: %zu: No value found\n", i);
		}
		#endif

		fprintf(stderr, "Aggregating minutes->hour\n");
		sampler_aggregate(hour, minutes, 1, 60, SAMPLER_AGG_AVG);

		fprintf(stderr, "Dumping hour for eid 0\n");
		sleep(1);
		for (i = 0; i < sampler_samplecount(hour); i++) {
			if (sampler_get(hour, 0, i, &val))
				fprintf(stderr, "Hour: %zu:	 Value: %lld\n", i, val);
			else
				fprintf(stderr, "Hour: %zu: No value found\n", i);
		}

		sampler sdup = sampler_dup(secs);
		sampler_copy(sdup, secs);
		sampler_free(sdup);
		 sampler_free(hour);
		 sampler_free(secs);
		 sampler_free(minutes);

	 }
#endif


	{
		pthread_t writerthread, reader1, reader2;
		int id1 = 1, id2 = 2;
		sampled_data = sampler_new(nentity, nsamples);

		/* Start the writer thread */
		pthread_create(&writerthread, NULL, writer, NULL);
		pthread_create(&reader1, NULL, reader, &id1);
		pthread_create(&reader2, NULL, reader, &id2);

		fprintf(stderr, "Main thread sleeping\n");
		sleep(5);
		fprintf(stderr, "Main thread shutting down\n");
		shutting_down = 1;

		pthread_join(writerthread, NULL);
		sampler_free(sampled_data);
	}

	return 0;
}
#endif
