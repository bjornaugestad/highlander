/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_STACK_H
#define META_STACK_H

#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stack_tag* stack;

stack stack_new(void)
    __attribute__((malloc, warn_unused_result));

void stack_free(stack s);

status_t stack_push(stack s, void *p)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

void *stack_top(stack s)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

void stack_pop(stack s)
    __attribute__((nonnull));

size_t stack_nelem(stack s)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

void *stack_get(stack s, size_t i)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
