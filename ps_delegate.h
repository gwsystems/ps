#ifndef PS_DELEGATE_H
#define PS_DELEGATE_H

#include <ps_plat.h>
#include <numa.h>
#include <del_config.h>

#define HYPER_THREADING_OPT
#define PS_DEL_THD_PER_PAR (10)
#define PS_DEL_NUM_THD (PS_DEL_THD_PER_PAR * PS_DEL_NUM_PAR)
#define TASK_QUEUE_SIZE (PS_NUMCORES)
/* #define TASK_QUEUE_SIZE (2*PS_CORE_PER_NUMA*PS_DEL_NUM_PAR) */
#define TASK_RING_SIZE (1) 	/* need to be power of 2 */
#define TASK_RING_MASK (TASK_RING_SIZE - 1)
#define PS_DEL_NUM_ARGS (4)
#define PS_DEL_SERVER_BATCH (TASK_RING_SIZE)
/* #define PS_DEL_SERVER_BATCH (1) */

typedef long (*ps_del_fn_t)(long, long, long, long, long);
typedef long (*ps_del_merge_fn_t)(void *, void *);
typedef void (*ps_del_init_fn_t)(void *);

typedef enum PS_PACKED {
	FINISH = 0,
	PENDING,
	PROCESSING,
} task_state_t;

struct task {
	ps_del_fn_t funcptr;
	long ret;
	long args[1 + PS_DEL_NUM_ARGS];
	int argc;
	volatile task_state_t state;
} PS_PACKED PS_ALIGNED;

struct task_ring {
	struct task ring[TASK_RING_SIZE];
} PS_PACKED PS_ALIGNED;

struct task_queue {
	struct task_ring tasks[TASK_QUEUE_SIZE];
} PS_PACKED PS_ALIGNED;

struct server_context {
	/* struct task *tasks[PS_NUMCORES]; */
	int start[PS_DEL_THD_PER_PAR], end[PS_DEL_THD_PER_PAR];
	int parid;
	/* long lock[TASK_QUEUE_SIZE]; */
	void *user[];
} PS_PACKED PS_ALIGNED;

struct req_info {
	struct task_ring *req;
	int tail;
};

struct client_context {
	struct task local;
	struct req_info requests[PS_DEL_NUM_PAR];
	/* struct task_ring *requests[PS_DEL_NUM_PAR]; */
	/* int tail[PS_DEL_NUM_PAR]; */
	int parid, thid, offset, start, end, check;
	int head[PS_NUMCORES];
	long yes, no, spin;
} PS_PACKED PS_ALIGNED; 

extern __thread struct client_context cli_contxts;

static inline long
ps_del_hash(void *key, long ksize)
{
	(void)ksize;
	return (long)key;
}

static inline int
ps_del_get_par(int id)
{ return id/PS_DEL_THD_PER_PAR; }
/* { return id % 8; } */

static inline void *
ps_del_get_server_user(struct server_context *svr)
{ return svr->user; }

static inline struct task *
ps_del_get_task(int src_par, int dst_par, int local, int block)
{
	struct task *tsk;
	struct task_ring *tsk_r;
	int tail;

	if (src_par == dst_par || local) {
		tsk = &cli_contxts.local;
	} else {
		/* tsk = &(cli_contxts.requests[dst_par]->ring[0]); */
		tsk_r = cli_contxts.requests[dst_par].req;
		tail  = cli_contxts.requests[dst_par].tail;
		tsk   = &(tsk_r->ring[tail]);
		cli_contxts.requests[dst_par].tail = (tail + 1) & TASK_RING_MASK;
	}

	return tsk;
}

static inline long
ps_del_task_response(struct task *task)
{
	assert(task->state == FINISH);
	return task->ret;
}

static inline int 
ps_del_task_done(struct task *task)
{ return task->state == FINISH; }

#ifdef HYPER_THREADING_OPT
static inline coreid_t
coreid2thid(coreid_t coreid)
{
	int tot_core, hyper;
	tot_core = PS_NUMLOCALITIES * PS_CORE_PER_NUMA;
	hyper    = coreid / tot_core;
	coreid  -= hyper * tot_core;
	return (coreid % PS_NUMLOCALITIES) * PS_CORE_PER_NUMA + coreid / PS_NUMLOCALITIES + hyper * tot_core;
}

static inline coreid_t
thid2parid(coreid_t thid)
{
	return thid / PS_DEL_THD_PER_PAR;
}

static inline coreid_t
thid2offset(int thid)
{
	return thid % PS_DEL_THD_PER_PAR;
}

static inline coreid_t
parid2numa(int parid)
{
	return parid % PS_NUMLOCALITIES;
}

static inline coreid_t
parid2firstcore(int parid)
{
	return parid * PS_DEL_THD_PER_PAR;
}

#else

/* assumption 1: */
/* Linux kernel maps 0 to the first core in the first socket, 1 to the first core in the second socket ... */
/* 4 to the second core in the first socket, 5 to the second core in the second socket ... */
/* 40 to the second hyper-thread in the first core in the first socket,  */
/* 41 to the second hyper-thread in the first core in the second socket ... (see plat/os/linux/ps_os.c) */
/* assumption 2: */
/* I map thd 0 to the first core, thd 1 to the second core in the first socket ... */
/* thd 40 to second hyper-thread in the first core, thd 41 to the second hyper-thread in the second core in the first socket ... */
/* assumption 3: */
/* I group the first `PS_DEL_THD_PER_PAR` hyper-threads into the first partition, second `PS_DEL_THD_PER_PAR` hyper-threads into the second partition ... */
/* For example, PS_DEL_THD_PER_PAR = 10, if there is no hyper-threads, thd (0, 1, ... 9)/core (0, 4, ... 36) are the first partition ... */
/* if there are hyper-threads, then thd (0, 1, ... 5, 40, ... 45)/core (0, 4, ...16, 40, ... 56) are the first partition ... */
/* assumption 4: */
/* assign partition to NUMA node depends on the physical cores in the partition */
static inline coreid_t
coreid2thid(coreid_t coreid)
{
	int tot_core, hyper;
	tot_core = PS_NUMLOCALITIES * PS_CORE_PER_NUMA;
	hyper    = coreid / tot_core;
	coreid  -= hyper * tot_core;
	return (coreid % PS_NUMLOCALITIES) * PS_CORE_PER_NUMA + coreid / PS_NUMLOCALITIES + hyper * tot_core;
}

static inline coreid_t
thid2parid(coreid_t thid)
{
	int tot_core, tot_thd;
	tot_core = PS_NUMLOCALITIES * PS_CORE_PER_NUMA;
	tot_thd  = PS_DEL_NUM_THD;
	if (thid >= tot_core) thid -= tot_core;
	if (tot_thd < tot_core) tot_thd = tot_core;
	if (tot_thd > thid + tot_core) tot_thd = thid + tot_core;
	thid += tot_thd - tot_core;
	return thid / PS_DEL_THD_PER_PAR;
}

static inline coreid_t
thid2offset(int thid)
{
	int ret, hyper;
	ret = thid % PS_DEL_THD_PER_PAR;
	hyper = thid / (PS_NUMLOCALITIES * PS_CORE_PER_NUMA);
	if (thid % PS_DEL_THD_PER_PAR < PS_DEL_THD_PER_PAR / 2) {
		ret += hyper * PS_DEL_THD_PER_PAR / 2;
	} else {
		hyper = !hyper && (PS_DEL_NUM_THD > (thid + PS_NUMLOCALITIES * PS_CORE_PER_NUMA));
		ret -= hyper * PS_DEL_THD_PER_PAR / 2;;
	}
	return ret;
}

static inline coreid_t
parid2numa(int parid)
{
	int hyper;
	if (PS_DEL_NUM_PAR < PS_NUMLOCALITIES) hyper = 0;
	else hyper = PS_DEL_NUM_PAR - PS_NUMLOCALITIES;
	if (parid < hyper) return parid / 2;
	else return parid - hyper;
	/* return parid % PS_NUMLOCALITIES; */
}

static inline coreid_t
parid2firstcore(int parid)
{
	int hyper;
	if (PS_DEL_NUM_PAR < PS_NUMLOCALITIES) hyper = 0;
	else hyper = PS_DEL_NUM_PAR - PS_NUMLOCALITIES;
	if (parid > hyper) return (parid - hyper) * PS_DEL_THD_PER_PAR;
	else return (parid / 2) * PS_DEL_THD_PER_PAR + (parid % 2) * PS_DEL_THD_PER_PAR / 2;
}

#endif

void ps_del_init(void);
void ps_del_par_init(ps_del_init_fn_t cb, size_t sz);
void ps_del_par_exit(ps_del_init_fn_t cb, size_t sz);
void ps_del_prepare(struct task *tsk, int dst_par, ps_del_fn_t func, void *key, long ksize, int arg_count, ...);
void ps_delegate(struct task *task);
int ps_del_run_server(coreid_t parid, int batch);

#define SERVER_EXE(task)                                                                                                    \
	do {								                                                    \
		task->ret = (long)task->funcptr(task->args[0], task->args[1], task->args[2], task->args[3], task->args[4]); \
	} while (0)							                                                    \

/* #define SPIN_LOOP(block, task, par)					\ */
/* 	while (block && !ps_del_task_done(task)) { \ */
/* 		ps_del_run_server(par, PS_DEL_SERVER_BATCH);\ */
/* 	} 				                                                   \ */

#define SPIN_LOOP(block, task, par)					\
	do { \
		ps_del_run_server(par, PS_DEL_SERVER_BATCH);\
	} while (block && !ps_del_task_done(task))				                                                   \

				/* int n =ps_del_run_server(curr, src_par, PS_DEL_SERVER_BATCH); \ */
				/* if (n>0) cli_contxts.yes += n;\ */
				/* else cli_contxts.no++;\ */
				/* cli_contxts.spin++; \ */

#define PS_DEL_EXE_WITH_BLOCK(block, local, func, key, ksize, retval, ...) \
	do {                                                                                                                \
		coreid_t src_par, dst_par;				                                                    \
		struct task *tsk;					                                                    \
		src_par = cli_contxts.parid;				                                                    \
		/* dst_par = ps_del_get_par(ps_del_hash((void *)(key), (ksize)) % PS_DEL_NUM_THD); */\
		dst_par = ps_del_hash((void *)(key), (ksize)) % PS_DEL_NUM_PAR;\
		tsk     = ps_del_get_task(src_par, dst_par, local, block);	                                            \
		SPIN_LOOP(1, tsk, src_par);				\
		ps_del_prepare(tsk, dst_par, (ps_del_fn_t)func, (void *)(key), (ksize), __VA_ARGS__);                       \
		/* if (src_par == 1) printf("src %d dst %d tsk %p\n", src_par, dst_par, (void *)tsk);  */\
		if (local || dst_par == src_par) {                                                                          \
			SERVER_EXE(tsk);				                                                    \
			/* ps_del_run_server(curr, src_par, PS_DEL_SERVER_BATCH);  */\
		} else {				                                                                    \
			ps_delegate(tsk);                                                                                   \
			SPIN_LOOP(block, tsk, src_par);			\
		}					                                                                    \
		if (block) retval = ps_del_task_response(tsk);			                                            \
	} while (0)					                                                                    \

#define PS_DEL_ASYNC_EXE(func, key, ksize, ret, ...)                                                                        \
PS_DEL_EXE_WITH_BLOCK(0, 0, func, key, ksize, ret, __VA_ARGS__)	                                                            \

#define PS_DEL_LOCAL_EXE(func, key, ksize, ret, ...)                                                                        \
PS_DEL_EXE_WITH_BLOCK(1, 1, func, key, ksize, ret, __VA_ARGS__)	                                                            \

#define PS_DEL_EXE(func, key, ksize, ret, ...)                                                                              \
PS_DEL_EXE_WITH_BLOCK(1, 0, func, key, ksize, ret, __VA_ARGS__)	                                                            \

#define PS_DEL_RANGE_EXE_WITH_BLOCK(block, func, key, ksize, retval, merge, ...)                                            \
	do {								                                                    \
		coreid_t src_par, par;				                                                            \
		long local_ret;						                                                    \
		struct task *tsk[PS_DEL_NUM_PAR];			                                                    \
		ps_del_merge_fn_t merge_func = (ps_del_merge_fn_t)merge;	                                            \
		src_par = cli_contxts.parid;				                                                    \
		for(par = (src_par + 1) % PS_DEL_NUM_PAR; par != src_par; par = (par+1) % PS_DEL_NUM_PAR) {                 \
			tsk[par]  = ps_del_get_task(src_par, par, 0, block);	\
			SPIN_LOOP(1, tsk[par], src_par);			\
			ps_del_prepare(tsk[par], par, (ps_del_fn_t)func, (void *)(key), (ksize), __VA_ARGS__);   \
			/* printf("b del par %d locl %d sta %d\n", par, local_ret, ps_del_task_done(tsk[par]));  */\
			ps_delegate(tsk[par]);                                                                              \
		}							                                                    \
		tsk[src_par]  = ps_del_get_task(src_par, src_par, 0, block);	\
		ps_del_prepare(tsk[src_par], src_par, (ps_del_fn_t)func, (void *)(key), (ksize), __VA_ARGS__);    \
		SERVER_EXE(tsk[src_par]);					                                            \
		retval = ps_del_task_response(tsk[src_par]);			                                            \
		if (!block) break;					                                                    \
		for(par = (src_par + 1) % PS_DEL_NUM_PAR; par != src_par; par = (par+1) % PS_DEL_NUM_PAR) {                 \
			SPIN_LOOP(block, tsk[par], src_par);		\
			local_ret = ps_del_task_response(tsk[par]); \
			if (merge) retval = merge_func((void *)(long)retval, (void *)local_ret); \
		} \
	} while (0) \

#define PS_DEL_RANGE_EXE(func, key1, ksize1, ret, merge, ...)		\
PS_DEL_RANGE_EXE_WITH_BLOCK(1, func, key1, ksize1, ret, merge, __VA_ARGS__)	\





void ps_del_account(void);

#define THD_DATA_PAD 40

/* latency is in cycle, run_time is in ms, and thput is Kops/sec */
struct ps_thread_data {
	long thid, nop, nread, nwrite;
	unsigned long tsc_start, tsc_end, tsc_tot;
	long run_time, thput, latency;
	pthread_barrier_t *barrier;
	char pad[THD_DATA_PAD];
} PS_PACKED PS_ALIGNED;

static inline void
init_thd_data(struct ps_thread_data *data, int nthd, pthread_barrier_t *barrier)
{
	int i;
	for(i=0; i<nthd; i++) {
		memset(data + i, 0, sizeof(struct ps_thread_data));
		data[i].thid = i;
		data[i].barrier = barrier;
	}
}

static inline void
process_thd_data(struct ps_thread_data *data, int nthd, char *label)
{
	int i;
	long tot_latency, tot_thput;
	tot_latency = tot_thput = 0;
	for(i=0; i<nthd; i++) {
		assert(i == data[i].thid);
		/* assert(data[i].nop == data[i].nread + data[i].nwrite); */
		data[i].tsc_tot= data[i].tsc_end - data[i].tsc_start;
		data[i].latency = data[i].tsc_tot / data[i].nop;
		data[i].run_time = data[i].tsc_tot / PS_CPU_FREQ;
		data[i].thput = data[i].nop / data[i].run_time;
		tot_latency += data[i].latency;
		tot_thput += data[i].thput;
	}
	printf("%s thd num %d\n", label, nthd);
	/* for(i=0; i<nthd; i++) { */
	/* 	printf("thd %d nop %ld (r %ld w %ld) time %ld ms, latency %ld thput %ld\n", i, data[i].nop, data[i].nread, data[i].nwrite, data[i].run_time, data[i].latency, data[i].thput); */
	/* } */
	printf("avg latency %ld tot thput %ld\n", tot_latency / nthd, tot_thput);
}

#endif	/* PS_DELEGATE_H */
