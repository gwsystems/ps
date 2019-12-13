#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ps_slab.h>
#include <ps_delegate.h>

#define BENCH_ITER 1000000
/* #define BENCH_ITER 10000000 */
static volatile long stop;
#define DUR 50
int dur, delay;

static inline void
spin_delay(int d)
{
	unsigned long start=0, end=0;
	if (!d) return ;
	start = ps_tsc();
	do {
		end = ps_tsc();
	} while (end - start < d);
}

long
empty(struct server_context *srv, long a)
{
	unsigned long start, end;
	(void)srv;
	spin_delay(dur);
	return a;
}
/* { return thid2parid(coreid2thid(ps_coreid())); } */

void *
bench(void *a)
{
	coreid_t curr, par;
	long ret, i, id;
	unsigned long start, end;
	struct ps_thread_data *data = (struct ps_thread_data *)a;

	curr = data->thid;
	thd_set_affinity(pthread_self(), curr);
	ps_del_par_init(NULL, 0);
	par  = cli_contxts.parid;
	pthread_barrier_wait(data->barrier);
	id = curr;
	start = ps_tsc();
	i = id;
	/* for(i=id; i < id + 1 * BENCH_ITER; i+=1) { */
	while (stop == 0) {
		/* PS_DEL_ASYNC_EXE(empty, i, 2, ret, 0, 3, 4); */
		PS_DEL_EXE(empty, i, 2, ret, 0, 3, 4);
		assert(ret == i);
		i++;
		spin_delay(delay);
	}
	end = ps_tsc();;
	data->tsc_start = start;
	data->tsc_end = end;
	data->nop = i - id;
	ps_faa((unsigned long *)&stop, 1);
	while (stop != 1 + PS_DEL_NUM_THD) {
		ps_del_run_server(par, PS_DEL_SERVER_BATCH);
	}

	/* ps_del_account(); */
	return NULL;
}

void
run_bench(void)
{
	pthread_t child[PS_DEL_NUM_THD];
	pthread_barrier_t barrier;
	struct ps_thread_data *data;
	long i, sum = 0;
	struct timespec tt_start, tt_end;
	struct timespec timeout;
	int test_duration = 10000;

	stop = 0;
	timeout.tv_sec = test_duration / 1000;
	timeout.tv_nsec = (test_duration % 1000) * 1000000;
	pthread_barrier_init(&barrier, NULL, 1 + PS_DEL_NUM_THD);
	data = (struct ps_thread_data *)malloc(PS_DEL_NUM_THD * sizeof(struct ps_thread_data));
	init_thd_data(data, PS_DEL_NUM_THD, &barrier);
	for(i=0; i<PS_DEL_NUM_THD; i++) {
		if (pthread_create(&child[i], 0, bench, (void *)&data[i])) {
			perror("pthread create of child\n");
			exit(-1);
		}
	}
	pthread_barrier_wait(&barrier);
	clock_gettime(CLOCK_MONOTONIC, &tt_start);
	nanosleep(&timeout, NULL);
	stop = 1;
	clock_gettime(CLOCK_MONOTONIC, &tt_end);

	for(i=0; i<PS_DEL_NUM_THD; i++) pthread_join(child[i], 0);
	process_thd_data(data, PS_DEL_NUM_THD, "ps del empty");
	for(i=0; i<PS_DEL_NUM_THD; i++) sum += (long)data[i].nop;
	unsigned long tstart = (tt_start.tv_sec * 1000000000LL) + tt_start.tv_nsec;
	unsigned long finish = (tt_end.tv_sec * 1000000000LL) + tt_end.tv_nsec;
	double duration = (double)(finish - tstart)/1000000000LL;
	double ops_per_sec = (double)sum/duration;
	double mops_per_sec = ops_per_sec / 1000000LL;
	printf("#Mops %.3f\n", (double)mops_per_sec);
	free(data);
	pthread_barrier_destroy(&barrier);
}

int main (int argc, char ** argv)
{
	int c;
	if(numa_available() < 0) printf("System does not support NUMA API!\n");
	while((c = getopt(argc, argv, "l:d:")) != -1){
		switch (c)
		{
		case 'l':
		{
			delay = atoi(optarg);
			break;
		}
		case 'd':
		{
			dur = atoi(optarg);
			break;
		}
		}
	}
	printf("total NUMS nodes %d delay %d lenght %d\n", numa_max_node() + 1, delay, dur);
	ps_del_init();
	run_bench();
	return 0;
}
