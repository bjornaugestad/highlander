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

#include <meta_stack.h>
#include <meta_list.h>

/*
 * A stack is implemented (in v1) as a list. We insert new items
 * in the front of the list when we push items onto the stack. We
 * delete the first item from the list when we pop items from the stack.
 */
struct stack_tag {
    list lst;
};

stack stack_new(void)
{
    stack new;

    if ((new = malloc(sizeof *new)) == NULL)
        return NULL;
        
    if ((new->lst = list_new()) == NULL) {
        free(new);
        return NULL;
    }

    return new;
}

/*
 * We use sublist_free to free the list. This way
 * we don't free items in the list.
 */
void stack_free(stack this)
{
    if (this != NULL) {
        if (this->lst != NULL)
            sublist_free(this->lst);

        free(this);
    }
}

status_t stack_push(stack this, void *p)
{
    assert(this != NULL);
    assert(p != NULL);

    if (list_insert(this->lst, p) == NULL)
        return failure;

    return success;
}

void *stack_top(stack this)
{
    void *top = NULL;
    list_iterator i;

    assert(this != NULL);
    assert(list_size(this->lst) != 0);

    i = list_first(this->lst);
    if (!list_end(i))
        top = list_get(i);

    assert(top != NULL);
    return top;
}

void stack_pop(stack this)
{
    list_iterator i;

    assert(this != NULL);
    assert(list_size(this->lst) != 0);

    i = list_first(this->lst);
    if (!list_end(i))
        list_remove_node(this->lst, i);
}

size_t stack_nelem(stack this)
{
    assert(this != NULL);

    return list_size(this->lst);
}

void *stack_get(stack this, size_t i)
{
    assert(this != NULL);

    return list_get_item(this->lst, i);
}

#ifdef CHECK_STACK
#include <stdio.h>

/*
 * What do we want to test here?
 */

int main(void)
{
    size_t i, nelem = 10000;
    char *str;
    stack s = stack_new();

    for (i = 0; i < nelem; i++) {
        if ((str = malloc(10)) == NULL) {
            fprintf(stderr, "Out of memory");
            exit(EXIT_FAILURE);
        }

        sprintf(str, "%zu", i);
        if (!stack_push(s, str))
            exit(EXIT_FAILURE);
    }

    assert(nelem == stack_nelem(s));
    while (stack_nelem(s) > 0) {
        str = stack_top(s);
        free(str);
        stack_pop(s);
    }

    stack_free(s);
    return 0;
}
#endif
