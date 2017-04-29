/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_TICKER_H
#define META_TICKER_H

typedef struct ticker_tag* ticker;

ticker ticker_new(int usec);
void ticker_free(ticker t);
status_t ticker_add_action(ticker t, void(*pfn)(void*), void *arg);
status_t ticker_start(ticker t);
void ticker_stop(ticker t);

#endif
