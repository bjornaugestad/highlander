#ifndef META_SAMPLER_H
#define META_SAMPLER_H

#include <stddef.h>
#include <stdio.h>

/**
 * @file meta_sampler.h - Implements a sampling storage ADT
 *
 * Lots of times we want to sample data on some periodic interval
 * and store data for each interval. You may want to sample stock 
 * prices, cpu consumption, webserver load, or any other interesting
 * data.
 * The problem is that you need to store the data somewhere, and probably
 * store quite a lot of the data as well. Later you may also want to
 * aggregate the data, e.g. create hourly statistics from a dataset
 * containing stats per minute. Some like to perform computations on
 * the datasets as well, like computing the min/max/high/low/avg value
 * for some value.
 * 
 * Some like to sample more than one value as well, I personally would
 * like to be able to sample e.g. all 200+ events from Oracle by
 * the minute for a long period of time, and later present it graphically.
 *
 * The oldest values should probably be discarded as well.
 *
 * This ADT implements a two-dimensional array. The first dimension
 * is what we sample data about(the entity dimension), and the second
 * dimension is the data sampled(the data dimension).
 *
 * Each dimension has a max number of entries. Data in the data dimension
 * will be discarded in a fifd (first in first discarded) way.
 *
 * Sampling frequencies:
 * The minimum resolution is one second and there is no upper limit.
 * 
 * Size of samples:
 * The sampler should be able to handle 10.000 entities with 10.000
 * data values for each entity. The main memory consumer is the data
 * dimension, which will use sizeof(long long) bytes per entry, normally
 * 8 bytes per entry. A 10K by 10K array will therefore consume 800MB+
 * of RAM, quite a lot. A more common scenario may be that you sample
 * data for 200 different entities, once every minute and want data for
 * the last 24 hours. This means that you need approximately
 * (24*60*8*200) bytes, or 2250KB+, lets say 3MB. 
 *
 * Transactional update:
 * The dataset is updated in a transactional way. You must 
 * call sampler_start_update() to be able to update sampling values
 * and call sampler_commit() to commit your changes. There is no
 * rollback. There are many reasons for this quite odd functionality
 *
 * 	- All samples get the same timestamp, which means that we can store
 *    just one time_t value per interval, instead of one time_t value
 *    per sample/interval. This reduces memory requirements
 *    and also makes it a lot easier to aggregate and compare data.
 *
 *  - Number of calls to pthread_mutex_lock()/unlock() are drastically
 *    reduced. If you're sampling 1000 values per second, you don't want
 *    to lock and unlock 2000 times a second.
 *
 * Reading and writing in a MT world:
 * ----------------------------------
 * We will use a rwlock to synchronize access to the data, which means
 * that multiple readers are OK, but only one thread can write.
 *
 * Missing data and illegal values:
 * --------------------------------
 * The dataset reserves LLONG_MIN for internal use. This value is used
 * to indicate that no sample is present. This is important since we
 * cannot require the user to provide data for all entities every time
 * a sample is added to the dataset. LLONG_MIN is therefore reserved for
 * internal use and the debug version will assert that LLONG_MIN is never
 * used for data.
 *
 * Memory consumption and performance considerations:
 * --------------------------------------------------
 * All memory needed is allocated when the sampler ADT is created. This
 * reduces the memory fragmentation and makes it fast to update the values.
 *
 * Optimizations:
 * --------------
 * It is both possible and easy to precompute lots of aggregates
 * (sum/min/max/...), but assuming that the dataset is updates more often
 * than read, I will not implement this now.
 */

typedef struct sampler_tag* sampler;

/**
 * Create a new sampler object. Remember that your entity id's must
 * be zero-based and contigous. 
 */
sampler sampler_new(size_t entities, size_t values);
void    sampler_free(sampler s);

/* Duplicate a sampler object. Remember to free it. */
sampler sampler_dup(sampler src);

/* Copy all properties for a sampler object from src to dest ,
 * where dest must equal src.*/
void sampler_copy(sampler dest, sampler src);

/**
 * Prepares the dataset for update. This basically means locking the 
 * sampler object in write mode, computing the proper index offset
 * for the values which will be added, and setting the data entries
 * to LLONG_MIN. The latter is done in case we don't provide values
 * for all entities.
 */
void sampler_start_update(sampler s, time_t t);
void sampler_add(sampler s, size_t entity_id, long long value);
void sampler_commit(sampler s);

/**
 * Returns the number of samples we have data for. 
 * Use it when you want to iterate on the values, as in
 * @code
 *    
 *    size_t i, nelem;
 *    sampler s;
 *    ...
 *    sampler_start_read(s);
 *    nelem = sampler_samplecount(s);
 *    for(i = 0; i < nelem; i++) {
 *       long long value;
 *       sampler_get(s, entity, i, &value);
 *    }
 *    sampler_stop_read(s);
 * @endcode
 */
size_t sampler_samplecount(sampler s);

/**
 * We must explicitly start and stop reading from the sampler,
 * and cannot just lock in the _get() function. The reason is that 
 * to get a valid view of the data in the sampler we cannot allow
 * a writer to change data while we're reading. I don't like this 
 * anymore than you do. 
 * 
 * Remember read as fast as possible(as needed) so that we don't
 * block any writer thread, causing that thread to lose data. Imagine
 * that you sample data each second and the writer thread is blocked
 * for more than a second. Not a good thing. Use the sampler_dup() function
 * to get a duplicate of the sampler if that is needed. Could be a smart thing
 * to do if you sample lots of data very frequently and e.g. want to create a 
 * png image of the data in the sample. 
 */
void sampler_start_read(sampler s);
void sampler_stop_read(sampler s);

/**
 * Returns a value for an entity in the pval parameter and returns 1
 * if the entity had a value for that index. Returns 0 if no value existed.
 *
 */
int sampler_get(sampler s, size_t entity_id, size_t i, long long* pval);

/**
 * eid == entity_id
 * Note that from is inclusive and to is exclusive, 
 * so use [0, sampler_samplecount()] to get all entries 
 */
int sampler_avg(sampler s, size_t eid, size_t from, size_t to, long long* pval);
int sampler_min(sampler s, size_t eid, size_t from, size_t to, long long* pval);
int sampler_max(sampler s, size_t eid, size_t from, size_t to, long long* pval);
int sampler_first(sampler s, size_t eid, size_t from, size_t to, long long* pval);
int sampler_last(sampler s, size_t eid, size_t from, size_t to, long long* pval);

/** Returns the time of the sample */
time_t sampler_time(sampler s, size_t i);

/**
 * Creates a new sampler instance, with values aggregated from the 
 * src sampler. The resolution parameter will be used to find 
 * the from/to values in the src sampler. A quick example: 
 * If the src sampler has a resolution of minutes and you want to aggregate
 * on hours, use 60 for resolution. 
 *
 * Note that resolution is exclusive, if you sample each second and want
 * to aggregate up to minutes, a resolution of 60 is fine and 59 is not needed. 
 * 
 * The new sampler will have the same number of entities as the 
 * source sampler, but you can choose the number of data entries. 
 * This makes it easy to use the new sampler for other sampling purposes.
 *
 * The aggval parameter is one of the SAMPLER_AGG_xxx constants. They
 * let you choose which value to use when you aggregate the values.
 */
#define SAMPLER_AGG_MIN 1
#define SAMPLER_AGG_MAX 2
#define SAMPLER_AGG_AVG 3
#define SAMPLER_AGG_FIRST 4
#define SAMPLER_AGG_LAST  5

int sampler_aggregate(
	sampler dest,
	sampler src,
	size_t nsamples,
	unsigned int resolution,
	int aggval);


#endif

