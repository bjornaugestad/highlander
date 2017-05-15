/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_STACK_H
#define META_STACK_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stack_tag* stack;

stack stack_new(void)
    __attribute__((malloc));

void stack_free(stack s);

status_t stack_push(stack s, void *p)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

void *stack_top(stack s)
    __attribute__((nonnull(1)));

void stack_pop(stack s)
    __attribute__((nonnull(1)));

size_t stack_nelem(stack s)
    __attribute__((nonnull(1)));

void *stack_get(stack s, size_t i)
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
