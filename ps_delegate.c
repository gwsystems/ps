#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <ps_slab.h>
#include <ps_delegate.h>

/* TODO: change this to per partion numa allocation */
struct partition_set partitions;
struct server_context *svr_cntxts[PS_DEL_NUM_PAR];
__thread struct task self_task;

void
ps_del_init(void)
{
	int i;

	for(i=0; i<PS_DEL_NUM_PAR; i++) partitions.taskqs[i] = (struct task_queue *)malloc(sizeof(struct task_queue));
}

void
ps_del_par_inint(ps_del_init_fn_t cb, size_t sz)
{
	coreid_t curr;
	int i, sock, nthd, avg;

	curr = ps_coreid();
	if (curr % PS_DEL_THD_PER_PAR) return ; /* only the first core in the partition does init */
	sock = curr / PS_DEL_THD_PER_PAR;
	svr_cntxts[sock] = (struct server_context*)malloc(sizeof(struct server_context) + sz);
	if (sock == PS_DEL_NUM_PAR - 1) nthd = PS_DEL_THD_PER_PAR;
	else nthd = PS_DEL_NUM_THD - (PS_DEL_NUM_PAR - 1) * PS_DEL_THD_PER_PAR;
	avg = (PS_DEL_NUM_PAR - 1) * PS_DEL_THD_PER_PAR / nthd;
	for(i=0; i<PS_DEL_THD_PER_PAR && curr+i < PS_DEL_NUM_THD; i++) {
		svr_cntxts[sock]->start[i] = i * avg;
		svr_cntxts[sock]->end[i]   = (i+1) * avg;
	}
	if (cb) cb(svr_cntxts[sock]->user);
}

struct task *
ps_del_prepare(coreid_t curr, int par, ps_del_fn_t func, void *key, int ksize, int arg_count, ...)
{
	int i, sock;
	struct task_queue *tskq;
	struct task *tsk;
	va_list args;
	va_start(args, arg_count);

	sock = curr / PS_DEL_THD_PER_PAR;
	tskq = partitions.taskqs[par];
	if (sock == par) tsk = &self_task;
	else if (sock < par) tsk = &(tskq->tasks[curr]);
	else tsk = &(tskq->tasks[curr - PS_DEL_THD_PER_PAR]);
	tsk->funcptr = func;
	tsk->argc = arg_count;
	tsk->args[0] = (long)key;
	tsk->args[1] = (long)ksize;
	for(i=2; i < arg_count - 2; i++) tsk->args[i] = va_arg(args, long);

	va_end(args);
	return tsk;
}

void
ps_delegate(struct task *task)
{
	assert(task->state == FINISH);
	task->state = PENDING;
}

void
ps_del_run_server(coreid_t curr, int batch)
{
	int sock, id, i, start, end;
	struct task_queue *tskq;
	struct task *tsk;

	sock  = curr / PS_DEL_THD_PER_PAR;
	id    = curr % PS_DEL_THD_PER_PAR;
	start = svr_cntxts[sock]->start[id];
	end   = svr_cntxts[sock]->end[id];
	tskq  = partitions.taskqs[sock];
	for(i=start; i<end && batch>0; i++, batch -= (end-start)) {
		tsk = &(tskq->tasks[i]);
		if (tsk->state != PENDING) continue;
		SERVER_EXE(tsk);
		tsk->state = FINISH;
	}
}
