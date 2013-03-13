#include <stdlib.h>
#include <assert.h>

#include <meta_stack.h>
#include <meta_list.h>

/**
 * A stack is implemented (in v1) as a list. We insert new items
 * in the front of the list when we push items onto the stack. We
 * delete the first item from the list when we pop items from the stack.
 */
struct stack_tag {
	list lst;
};

stack stack_new(void)
{
	stack s;

	if( (s = mem_malloc(sizeof *s)) != NULL) {
		if( (s->lst = list_new()) == NULL) {
			mem_free(s);
			s = NULL;
		}
	}

	return s;
}

void stack_free(stack s)
{
	/**
	 * We use sublist_free to free the list. This way
	 * we don't free items in the list.
	 */

	if(s != NULL) {
		if(s->lst != NULL)
			sublist_free(s->lst);

		mem_free(s);
	}
}

int stack_push(stack s, void *p)
{
	assert(s != NULL);
	assert(p != NULL);

	if(list_insert(s->lst, p) == NULL)
		return 0;
	else
		return 1;
}

void *stack_top(stack s)
{
	void *d = NULL;
	list_iterator i;

	assert(s != NULL);
	assert(list_size(s->lst) != 0);

	i = list_first(s->lst);
	if(!list_end(i))
		d = list_get(i);

	assert(d != NULL);
	return d;
}

void stack_pop(stack s)
{
	list_iterator i;

	assert(s != NULL);
	assert(list_size(s->lst) != 0);

	i = list_first(s->lst);
	if(!list_end(i))
		list_remove_node(s->lst, i);
}

size_t stack_nelem(stack s)
{
	assert(s != NULL);

	return list_size(s->lst);
}

void* stack_get(stack s, size_t i)
{
	assert(s != NULL);

	return list_get_item(s->lst, i);
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

	for(i = 0; i < nelem; i++) {
		if( (str = malloc(10)) == NULL) {
			fprintf(stderr, "Out of memory");
			exit(EXIT_FAILURE);
		}

		sprintf(str, "%zu", i);
		stack_push(s, str);
	}

	assert(nelem == stack_nelem(s));
	while(stack_nelem(s) > 0) {
		str = stack_top(s);
		free(str);
		stack_pop(s);
	}

	stack_free(s);
	return 0;
}
#endif

