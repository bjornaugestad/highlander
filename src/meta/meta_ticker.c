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
#include <pthread.h>
#include <time.h>

#include <meta_list.h>
#include <meta_ticker.h>

/*
 * The action to be performed when a tick occurs. We store a function pointer
 * and a void *to the argument of the function.
 */
struct action {
	void (*pfn)(void *arg);
	void *arg;
};

/*
 * Implementation of the ticker ADT
 */
struct ticker_tag {
	int usec;
	pthread_t id;
	list actions;
	int stop;
	int running;
};

static void *tickerfn(void *arg);

ticker ticker_new(int usec)
{
	ticker t;

	if ((t = malloc(sizeof *t)) == NULL
	||	(t->actions = list_new()) == NULL) {
		free(t);
		t = NULL;
	}
	else {
		t->usec = usec;
		t->stop = 0;
		t->running = 0;
	}

	return t;
}

void ticker_free(ticker t)
{
	ticker_stop(t);
	list_free(t->actions, NULL);
	free(t);
}

int ticker_add_action(ticker t, void(*pfn)(void*), void *arg)
{
	struct action *pa;

	if ((pa = malloc(sizeof *pa)) == NULL)
		return 0;

	pa->pfn = pfn;
	pa->arg = arg;
	return list_add(t->actions, pa) ? 1 : 0;
}

int ticker_start(ticker t)
{
	t->running = 1;
	t->stop = 0;
	if (pthread_create(&t->id, NULL, tickerfn, t)) {
		t->running = 0;
		return 0;
	}
	else
		return 1;
}

void ticker_stop(ticker t)
{
	struct timespec ts = {0, 100000};
	t->stop = 1;
	while (t->running)
		nanosleep(&ts, NULL);
}

static void *tickerfn(void *arg)
{
	ticker t = arg;
	struct timespec ts;

	ts.tv_sec = t->usec / 1000000;
	ts.tv_nsec= (t->usec % 1000000) * 1000;

	while (!t->stop) {
		nanosleep(&ts, NULL);
		if (!t->stop) {
			/* Perform the actions in the list */
			list_iterator i;
			for (i = list_first(t->actions); !list_end(i); i = list_next(i)) {
				struct action* pa = list_get(i);
				pa->pfn(pa->arg);
				if (t->stop) /* Check again */
					break;
			}
		}
	}

	t->running = 0;
	return NULL;
}
