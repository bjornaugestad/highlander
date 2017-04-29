/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_ATOMIC_H
#define META_ATOMIC_H

#include <stdint.h>
#include <pthread.h>

typedef struct {uint16_t val; pthread_mutex_t lock;} atomic_u16;
typedef struct {uint32_t val; pthread_mutex_t lock;} atomic_u32;
typedef struct {int val;	  pthread_mutex_t lock;} atomic_int;
typedef struct {unsigned long val;		pthread_mutex_t lock;} atomic_ulong;
typedef struct {unsigned long long val; pthread_mutex_t lock;} atomic_ull;

// Initialize a variable
#define ATOMIC_INITIALIZER {0, PTHREAD_MUTEX_INITIALIZER}

// t == type, n == (short)name
#define ATOMIC_INIT(t, n)\
    static inline void atomic_##n##_init(atomic_##n *p) {\
        p->val = 0;\
        pthread_mutex_init(&p->lock, NULL);\
    }

#define ATOMIC_DESTROY(t, n)\
    static inline void atomic_##n##_destroy(atomic_##n *p) {\
        pthread_mutex_destroy(&p->lock);\
    }


#define ATOMIC_GET(t, n) \
    static inline t atomic_##n##_get(atomic_##n* p) \
    {\
        pthread_mutex_lock(&p->lock);\
        t v = p->val;\
        pthread_mutex_unlock(&p->lock);\
        return v;\
    }


#define ATOMIC_SET(t, n) \
    static inline void atomic_##n##_set(atomic_##n * p, t val)\
    {\
        pthread_mutex_lock(&p->lock);\
        p->val = val;\
        pthread_mutex_unlock(&p->lock);\
    }

#define ATOMIC_ADD(t, n)\
    static inline t atomic_##n##_add(atomic_##n* p, t value)\
    {\
        pthread_mutex_lock(&p->lock);\
        p->val += value;\
        t val = p->val;\
        pthread_mutex_unlock(&p->lock);\
        return val;\
    }

#define ATOMIC_INC(t, n)\
    static inline t atomic_##n##_inc(atomic_##n* p)\
    {\
        pthread_mutex_lock(&p->lock);\
        p->val++;\
        t val = p->val;\
        pthread_mutex_unlock(&p->lock);\
        return val;\
    }

#define ATOMIC_DEC(t, n)\
    static inline t atomic_##n##_dec(atomic_##n* p)\
    {\
        pthread_mutex_lock(&p->lock);\
        p->val--;\
        t val = p->val;\
        pthread_mutex_unlock(&p->lock);\
        return val;\
    }

#define ATOMIC_TYPE(t, n)\
    ATOMIC_INIT(t, n)\
    ATOMIC_DESTROY(t, n)\
    ATOMIC_GET(t, n)\
    ATOMIC_SET(t, n)\
    ATOMIC_INC(t, n)\
    ATOMIC_DEC(t, n)\
    ATOMIC_ADD(t, n)

ATOMIC_TYPE(unsigned long long, ull)
ATOMIC_TYPE(unsigned long, ulong)
ATOMIC_TYPE(uint16_t, u16)
ATOMIC_TYPE(uint32_t, u32)
ATOMIC_TYPE(int, int)


#endif
