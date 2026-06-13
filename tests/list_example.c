/*
 * Compilable version of the ps_list.h comment-block examples.
 * Build: see Makefile target or compile manually with -Wall -Wextra -fsanitize=address,undefined
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../ps_list.h"

struct foo {
	struct ps_list l;
	int val;
};

int main(void)
{
	struct ps_list_head h;
	struct foo *i, *tmp;
	int count;

	ps_list_head_init(&h);
	assert(ps_list_head_empty(&h));

	/* Allocate and add 3 nodes */
	for (int v = 0; v < 3; v++) {
		struct foo *n = malloc(sizeof *n);
		assert(n);
		n->val = v;
		ps_list_init(n, l);
		ps_list_head_add(&h, n, l);
	}

	/* Example 1: manual for-loop */
	count = 0;
	for (i = ps_list_head_first(&h, struct foo, l);
	     !ps_list_is_head(&h, i, l);
	     i = ps_list_next(i, l)) {
		count++;
	}
	assert(count == 3);

	/* Example 2: ps_list_foreach */
	count = 0;
	ps_list_foreach(&h, i, l) {
		count++;
	}
	assert(count == 3);

	/* Example 3: ps_list_foreach_del + free */
	ps_list_foreach_del(&h, i, tmp, l) {
		ps_list_rem(i, l);
		free(i);
	}
	assert(ps_list_head_empty(&h));

	printf("All examples OK\n");
	return 0;
}
