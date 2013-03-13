#ifndef META_TICKER_H
#define META_TICKER_H

/**
 * A ticker is a thread which at certain intervals performs
 * some caller defined actions. 
 */
typedef struct ticker_tag* ticker;

/* Create a ticker which wakes up every usec microseconds
 * and then performs the actions it knows about.
 */
ticker ticker_new(int usec);
void   ticker_free(ticker t);

/* Add an action to be executed every tick */
int ticker_add_action(ticker t, void(*pfn)(void*), void* arg);

/* Start the ticker. */
int ticker_start(ticker t);
void ticker_stop(ticker t);

#endif /* guard */
 
