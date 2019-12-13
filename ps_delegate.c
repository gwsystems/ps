#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <ps_slab.h>
#include <ps_delegate.h>

struct task_queue *taskqs[PS_DEL_NUM_PAR];
/* struct task_queue *taskqs[PS_NUMLOCALITIES]; */
struct server_context *svr_cntxts[PS_DEL_NUM_PAR];
__thread struct client_context cli_contxts;

static inline void
ps_cli_cntxt_init(coreid_t parid)
{
	int id, start, end;

	id    = cli_contxts.offset;
	start = svr_cntxts[parid]->start[id];
	end   = svr_cntxts[parid]->end[id];
	cli_contxts.check = cli_contxts.start = start;
	cli_contxts.end   = end;
}

void
ps_del_init(void)
{
	int i;

	for(i=0; i<PS_DEL_NUM_PAR; i++) {
		taskqs[i] = (struct task_queue *)numa_alloc_onnode(sizeof(struct task_queue), parid2numa(i));
		memset(taskqs[i], 0, sizeof(struct task_queue));
	}
	memset(svr_cntxts, 0, sizeof(svr_cntxts));
	/* for(i=0; i<PS_NUMLOCALITIES; i++) { */
	/* 	taskqs[i] = (struct task_queue *)numa_alloc_onnode(sizeof(struct task_queue), i); */
	/* 	memset(taskqs[i], 0, sizeof(struct task_queue)); */
	/* } */
}

void
ps_del_par_init(ps_del_init_fn_t cb, size_t sz)
{
	coreid_t curr, par;
	int i, j, avg, off, hyper, nthd;
	struct task_queue *tskq;
	struct server_context *local;

	memset(&cli_contxts, 0, sizeof(struct client_context));
	curr  = coreid2thid(ps_coreid());
	par   = thid2parid(curr);
	hyper = curr / (PS_NUMLOCALITIES * PS_CORE_PER_NUMA);
	off   = curr - hyper * PS_NUMLOCALITIES * PS_CORE_PER_NUMA;
	off   = off * 2 + hyper;
	for(i=0; i<PS_DEL_NUM_PAR; i++) {
		tskq  = taskqs[i];
		cli_contxts.requests[i].req  = &(tskq->tasks[off]);
	}
	/* tskq  = taskqs[parid2numa(par)]; */
	/* hyper = curr / (PS_NUMLOCALITIES * PS_CORE_PER_NUMA); */
	/* off = 2 * (curr % PS_CORE_PER_NUMA) + hyper; */
	/* for(i=0; i<PS_DEL_NUM_PAR; i++) { */
	/* 	cli_contxts.requests[i]  = &(tskq->tasks[i*PS_CORE_PER_NUMA*2 + off]); */
	/* 	memset(cli_contxts.requests[i], 0, sizeof(struct task)); */
	/* } */
	memset(&cli_contxts.local, 0, sizeof(struct task ));
	cli_contxts.parid  = par;
	cli_contxts.thid   = curr;
	cli_contxts.offset = thid2offset(curr);
	/* printf("thd %d core %d par %d off %d first %d numa %d\n", curr, ps_coreid(), par, cli_contxts.offset, parid2firstcore(par), parid2numa(par)); */
	if (curr != parid2firstcore(par) || svr_cntxts[par]) {
		while (!svr_cntxts[par]) { __asm__ __volatile__ ("rep;nop;":::"memory"); } /* wait for the first core to finish */
		goto ret ; /* only the first core in the partition init server context*/
	}

	local = (struct server_context*)numa_alloc_local(sizeof(struct server_context) + sz);
	/* for(i=0; i<PS_NUMLOCALITIES; i++) { */
	/* 	for(j=0; j<PS_CORE_PER_NUMA * 2; j++) { */
	/* 		svr_cntxts[par]->tasks[i*PS_CORE_PER_NUMA*2 + j] = &(taskqs[i]->tasks[par*PS_CORE_PER_NUMA*2 +j]); */
	/* 	} */
	/* } */
	if (par < PS_DEL_NUM_PAR - 1) nthd = PS_DEL_THD_PER_PAR;
	else nthd = PS_DEL_NUM_THD - (PS_DEL_NUM_PAR - 1) * PS_DEL_THD_PER_PAR;
	/* avg = PS_NUMCORES / nthd; */
	avg = TASK_QUEUE_SIZE / nthd;
	for(i=0; i<PS_DEL_THD_PER_PAR; i++) {
		local->start[i] = i * avg;
		local->end[i]   = (i+1) * avg;
	}
	local->parid = par;
	if (!ps_cas((unsigned long *)&svr_cntxts[par], 0, (unsigned long)local)) numa_free(local, sizeof(struct server_context) + sz);
	else if (cb) cb(svr_cntxts[par]->user);
ret:
	ps_cli_cntxt_init(par);
}

void
ps_del_par_exit(ps_del_init_fn_t cb, size_t sz)
{
	coreid_t i, curr, par;
	struct server_context *local;

	curr = coreid2thid(ps_coreid());
	par = thid2parid(curr);
	for(i=0; i<PS_DEL_NUM_PAR; i++) {
		memset(cli_contxts.requests[i].req, 0, sizeof(struct task_ring));
	}
	memset(&cli_contxts.local, 0, sizeof(struct task ));
	cli_contxts.no = cli_contxts.yes = 0;
	if (curr % PS_DEL_THD_PER_PAR == 0 && svr_cntxts[par]) {
		local = svr_cntxts[par];
		if (ps_cas((unsigned long *)&svr_cntxts[par], (unsigned long)local, 0)) {
			if (cb) cb(local->user);
			numa_free(local, sizeof(struct server_context) + sz);
		}
	}
}

void
ps_del_prepare(struct task *tsk, int dst_par, ps_del_fn_t func, void *key, long ksize, int arg_count, ...)
{
	int i;
	va_list args;
	va_start(args, arg_count);

	assert(arg_count <= PS_DEL_NUM_ARGS);
	tsk->funcptr = func;
	/* tsk->argc    = arg_count; */
	tsk->args[0] = (long)svr_cntxts[dst_par];
	tsk->args[1] = (long)key;
	tsk->args[2] = (long)ksize;
	for(i=3; i <= arg_count; i++) tsk->args[i] = va_arg(args, long);
	va_end(args);
}

void
ps_delegate(struct task *task)
{
	ps_cc_barrier();
	assert(task->state == FINISH);
	task->state = PENDING;
}

int
ps_del_run_server(coreid_t parid, int batch)
{
	int id, i, j, start, end, ret = 0;
	struct task_queue *tskq;
	/* struct task **tskq; */
	struct task *tsk;
	struct task_ring *tsk_r;
	/* struct server_context *svr; */

	/* id    = cli_contxts.offset; */
	/* svr   = svr_cntxts[parid]; */
	start = cli_contxts.start;
	end   = cli_contxts.end;
	tskq  = taskqs[parid];
	/* tskq  = svr->tasks; */
	/* do { */
		/* process striped tasks */
		/* for(tsk = &(tskq->tasks[id * 2]), i=0; i<PS_NUMLOCALITIES; i++) { */
		/* 	if (tsk->state == PENDING) { */
		/* 		SERVER_EXE(tsk); */
		/* 		ps_cc_barrier(); */
		/* 		tsk->state = FINISH; */
		/* 		ret++; */
		/* 	} */
		/* 	tsk += 1; */

		/* 	if (tsk->state == PENDING) { */
		/* 		SERVER_EXE(tsk); */
		/* 		ps_cc_barrier(); */
		/* 		tsk->state = FINISH; */
		/* 		ret++; */
		/* 	} */
		/* 	tsk += (PS_CORE_PER_NUMA * 2 - 1); */
		/* } */
		/* batch -= (PS_NUMLOCALITIES * 2); */

		/* process continous tasks */
		/* for(i=start; i<end; i++) { */
		/* 	tsk = &(tskq->tasks[i].ring[0]); */
		/* 	if (tsk->state != PENDING) continue; */
		/* 	SERVER_EXE(tsk); */
		/* 	ps_cc_barrier(); */
		/* 	tsk->state = FINISH; */
		/* 	ret++; */
		/* } */
		/* batch -= (end-start); */

		/* process all tasks with locks */
		/* for(i=id+1; i!=id; i = (i+1) % TASK_QUEUE_SIZE) { */
		/* 	tsk = &(tskq->tasks[i]); */
		/* 	if (tsk->state == PENDING && */
		/* 	    !svr->lock[i] && */
		/* 	    ps_cas((unsigned long *)&(svr->lock[i]), 0, 1)) { */
		/* 		SERVER_EXE(tsk); */
		/* 		ps_cc_barrier(); */
		/* 		tsk->state = FINISH; */
		/* 		ps_cc_barrier(); */
		/* 		svr->lock[i] = 0; */
		/* 		ret++; */
		/* 	} */
		/* } */
		/* batch -= TASK_QUEUE_SIZE; */

		/* process one task each time */
		i = cli_contxts.check;
		j = cli_contxts.head[i];
		tsk_r = &(tskq->tasks[i]);
		while (batch > 0) {
			tsk = &(tsk_r->ring[j]);
			if (tsk->state == PENDING) {
				SERVER_EXE(tsk);
				ps_cc_barrier();
				tsk->state = FINISH;
				ret++;
				j = (j+1) & TASK_RING_MASK;
				batch--;
			} else {
				break;
				/* if (--batch > 0) { */
				/* 	cli_contxts.head[i++] = j; */
				/* 	if (i == end) i = start; */
				/* 	tsk_r = &(tskq->tasks[i]); */
				/* 	j = cli_contxts.head[i]; */
				/* } */
			}
		}
		cli_contxts.head[i++] = j;
		if (i == end) i = start;
		cli_contxts.check = i;
	/* 	batch = 0; */

	/* } while (batch > 0); */

	return ret;
}

void
ps_del_account(void)
{
	if (!cli_contxts.yes) cli_contxts.yes = 1;
	printf("core %d yes %ld no %ld ratio %ld spin %ld\n", ps_coreid(), cli_contxts.yes, cli_contxts.no, cli_contxts.no/cli_contxts.yes, cli_contxts.spin);
	/* printf(" par %d th %d off %d s %d e %d\n", cli_contxts.parid, cli_contxts.thid, cli_contxts.offset, */
	/*        svr_cntxts[cli_contxts.parid]->start[cli_contxts.offset], svr_cntxts[cli_contxts.parid]->end[cli_contxts.offset]); */
}
