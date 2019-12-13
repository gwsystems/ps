/* This tests assumes  */
/* #define PS_DEL_NUM_THD (4) */
/* #define PS_DEL_NUM_PAR (2) */
/* #define PS_DEL_THD_PER_PAR (2) */

#include <stdio.h>
#include <stdlib.h>

#include <ps_slab.h>
#include <ps_delegate.h>

long
del_func0(struct server_context *srv)
{
	coreid_t coreid, numaid, parid, thid;
	(void)srv;
	ps_tsc_locality(&coreid, &numaid);
	thid  = coreid2thid(coreid);
	parid = thid2parid(thid);
	printf("core %d thid %d par %d numa %d del func0\n", coreid, thid, parid, numaid);
	return -2;
}

void
del_func1(struct server_context *srv, int a1)
{
	coreid_t coreid, numaid, parid, thid;
	(void)srv;
	ps_tsc_locality(&coreid, &numaid);
	thid  = coreid2thid(coreid);
	parid = thid2parid(thid);
	printf("core %d thid %d par %d numa %d del func1 a1 %d\n", coreid, thid, parid, numaid, a1);
}

void
del_func2(struct server_context *srv, int a1, int a2)
{
	coreid_t coreid, numaid, parid, thid;
	(void)srv;
	ps_tsc_locality(&coreid, &numaid);
	thid  = coreid2thid(coreid);
	parid = thid2parid(thid);
	printf("core %d thid %d par %d numa %d del func2 a1 %d a2 %d\n", coreid, thid, parid, numaid, a1, a2);
}

void
del_func3(struct server_context *srv, int a1, int a2, int a3)
{
	coreid_t coreid, numaid, parid, thid;
	(void)srv;
	ps_tsc_locality(&coreid, &numaid);
	thid  = coreid2thid(coreid);
	parid = thid2parid(thid);
	printf("core %d thid %d par %d numa %d del func3 a1 %d a2 %d a3 %d\n", coreid, thid, parid, numaid, a1, a2, a3);
}

void
del_func4(struct server_context *srv, int a1, int a2, int a3, int a4)
{
	coreid_t coreid, numaid, parid, thid;
	(void)srv;
	ps_tsc_locality(&coreid, &numaid);
	thid  = coreid2thid(coreid);
	parid = thid2parid(thid);
	printf("core %d thid %d par %d numa %d del func4 a1 %d a2 %d a3 %d a4 %d\n", coreid, thid, parid, numaid, a1, a2, a3, a4);
}

void *
single_core_single_par_test(void *a)
{
	long ret;
	(void)ret;

	thd_set_affinity(pthread_self(), (long)a);
	ps_del_par_init(NULL, 0);
	PS_DEL_EXE(del_func0, 0, 2, ret, 4, 3, 4);
	PS_DEL_EXE(del_func1, 0, 2, ret, 4, 3, 4);
	PS_DEL_EXE(del_func2, 0, 2, ret, 4, 3, 4);
	PS_DEL_EXE(del_func3, 0, 2, ret, 4, 3, 4);
	PS_DEL_EXE(del_func4, 0, 2, ret, 4, 3, 4);
	return NULL;
}

void
two_core_single_par_test(void)
{
	long i;
	pthread_t child[2];

	for(i=0; i<2; i++) {
		if (pthread_create(&child[i], 0, single_core_single_par_test, (void *)i)) {
			perror("pthread create of child\n");
			exit(-1);
		}
	}
	for(i=0; i<2; i++) {
		pthread_join(child[i], NULL);
	}
}

void exit_func(void)
{ pthread_exit(0); }

void exe_func(void)
{
	long ret;
	(void)ret;

	PS_DEL_EXE(del_func0, 0, 2, ret, 4, 3, 4);
	PS_DEL_EXE(del_func1, 1, 2, ret, 4, 3, 4);
	PS_DEL_EXE(del_func2, 2, 2, ret, 4, 3, 4);
	PS_DEL_EXE(del_func3, 3, 2, ret, 4, 3, 4);
	PS_DEL_EXE(del_func4, 4, 2, ret, 4, 3, 4);
}

void exe_range_func(void)
{
	long ret;
	(void)ret;

	PS_DEL_RANGE_EXE(del_func0, 0, 4, ret, NULL, 4, 7, 8);
	PS_DEL_RANGE_EXE(del_func1, 1, 4, ret, NULL, 4, 7, 8);
	PS_DEL_RANGE_EXE(del_func2, 2, 4, ret, NULL, 4, 7, 8);
	PS_DEL_RANGE_EXE(del_func3, 3, 4, ret, NULL, 4, 7, 8);
	PS_DEL_RANGE_EXE(del_func4, 4, 4, ret, NULL, 4, 7, 8);
}

void *
two_par_test0(void *a)
{
	coreid_t curr, par;

	curr = (coreid_t)(long)a;
	par  = thid2parid(curr);
	thd_set_affinity(pthread_self(), curr);
	ps_del_par_init(NULL, 0);
	while (1) {
		ps_del_run_server(par, PS_DEL_SERVER_BATCH);
	}
	return NULL;
}

void
two_core_two_par_test0(void)
{
	pthread_t child;
	long ret;
	(void)ret;

	if (pthread_create(&child, 0, two_par_test0, (void *)2)) {
		perror("pthread create of child\n");
		exit(-1);
	}
	exe_func();
	PS_DEL_ASYNC_EXE(exit_func, 1, 2, ret, 4, 3, 4);
	pthread_join(child, NULL);
	ps_del_par_exit(NULL, 0);
}

volatile long stop;
void *
two_par_test1(void *a)
{
	coreid_t curr, par;

	curr = (coreid_t)(long)a;
	par  = thid2parid(curr);
	thd_set_affinity(pthread_self(), curr);
	ps_del_par_init(NULL, 0);
	exe_func();
	exe_range_func();
	ps_faa((unsigned long *)&stop, 1);
	while (stop != 2) {
		ps_del_run_server(par, 10);
	}
	return NULL;
}

void
two_core_two_par_test1(void)
{
	pthread_t child[2];
	int i;

	stop = 0;
	ps_del_init();
	if (pthread_create(&child[0], 0, two_par_test1, (void *)0)) {
		perror("pthread create of child\n");
		exit(-1);
	}
	if (pthread_create(&child[1], 0, two_par_test1, (void *)2)) {
		perror("pthread create of child\n");
		exit(-1);
	}
	for(i=0; i<2; i++) {
		pthread_join(child[i], NULL);
	}
}

#define BENCH_ITER 1024

long
empty(void)
{ return thid2parid(coreid2thid(ps_coreid())); }

void *
bench(void *a)
{
	coreid_t curr, par;
	long ret, i;
	struct ps_thread_data *data = (struct ps_thread_data *)a;

	curr = data->thid;
	par  = thid2parid(curr);
	thd_set_affinity(pthread_self(), curr);
	ps_del_par_init(NULL, 0);
	pthread_barrier_wait(data->barrier);
	data->tsc_start = ps_tsc();
	for(i=curr; i < curr + BENCH_ITER; i++) {
		PS_DEL_EXE(empty, i, 2, ret, 4, 3, 4);
		assert(ret == i % PS_DEL_THD_PER_PAR);
	}
	data->tsc_end = ps_tsc();
	data->tsc_tot= data->tsc_end - data->tsc_start;
	data->nop = BENCH_ITER;
	ps_faa((unsigned long *)&stop, 1);
	while (stop != PS_DEL_NUM_THD) {
		ps_del_run_server(par, 50);
	}

	return NULL;
}

void
four_core_two_par_bench(void)
{
	pthread_t child[PS_DEL_NUM_THD];
	struct ps_thread_data *data;
	pthread_barrier_t barrier;
	long i;

	stop = 0;
	ps_del_init();
	pthread_barrier_init(&barrier, NULL, PS_DEL_NUM_THD);
	data = (struct ps_thread_data *)malloc(PS_DEL_NUM_THD * sizeof(struct ps_thread_data));
	for(i=0; i<PS_DEL_NUM_THD; i++) {
		data[i].thid = i;
		data[i].barrier = &barrier;
		if (pthread_create(&child[i], 0, bench, (void *)&data[i])) {
			perror("pthread create of child\n");
			exit(-1);
		}
	}
	for(i=0; i<PS_DEL_NUM_THD; i++) {
		pthread_join(child[i], NULL);
	}
	for(i=0; i<PS_DEL_NUM_THD; i++) {
		printf("thd %ld time %ld (cyc) nop %ld avg %ld (cyc)\n", data[i].thid, data[i].tsc_tot, data[i].nop, data[i].tsc_tot / data[i].nop);
	}
	free(data);
	pthread_barrier_destroy(&barrier);
}

void
two_core_two_par_range_test0(void)
{
	pthread_t child;
	long ret;
	(void)ret;

	ps_del_par_init(NULL, 0);
	if (pthread_create(&child, 0, two_par_test0, (void *)2)) {
		perror("pthread create of child\n");
		exit(-1);
	}
	exe_range_func();
	PS_DEL_ASYNC_EXE(exit_func, 1, 2, ret, 4, 3, 4);
	pthread_join(child, NULL);
}

int
main(void)
{
	if(numa_available() < 0) {
		printf("System does not support NUMA API!\n");
	}
	printf("total NUMS nodes %d\n", numa_max_node() + 1);
	thd_set_affinity(pthread_self(), 0);
	ps_del_init();

	single_core_single_par_test((void *)0);
	printf("============= single core single partition test pass ==========\n");
	two_core_single_par_test();
	printf("============= two cores single partition test pass ==========\n");
	two_core_two_par_test0();
	printf("============= two cores two partitions point test pass ==========\n");
	two_core_two_par_range_test0();
	printf("============= two cores two partitions range test0 pass ==========\n");
	two_core_two_par_test1();
	printf("============= two cores two partitions test pass ==========\n");
	four_core_two_par_bench();

	/* int i;	for(i=0; i<80; i++) printf("thd %d core %d\n", coreid2thid(i), i); */
	return 0;
}
