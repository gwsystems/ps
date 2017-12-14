#ifndef PS_DELEGATE_H
#define PS_DELEGATE_H

#include <ps_plat.h>

#define PS_DEL_NUM_THD (1)
#define PS_DEL_NUM_PAR (1)
#define PS_DEL_THD_PER_PAR (1)
#define PS_DEL_NUM_ARGS (5)
#define PS_DEL_SERVER_BATCH (100)

typedef long (*ps_del_fn_t)(long);
typedef void (*ps_del_init_fn_t)(void *);

typedef enum PS_PACKED {
	FINISH = 0,
	PENDING,
	PROCESSING,
} task_state_t;

struct task {
	ps_del_fn_t funcptr;
	long ret;
	long args[PS_DEL_NUM_ARGS];
	int argc;
	task_state_t state;
} PS_PACKED PS_ALIGNED;

struct task_queue {
	struct task tasks[(PS_DEL_NUM_PAR - 1) * PS_DEL_THD_PER_PAR];
} PS_PACKED PS_ALIGNED;

struct partition_set {
	struct task_queue *taskqs[PS_DEL_NUM_PAR];
} PS_PACKED PS_ALIGNED;

struct server_context {
	int start[PS_DEL_THD_PER_PAR], end[PS_DEL_THD_PER_PAR];
	void *user[];
};

static inline long
ps_del_hash(void *key, int ksize)
{
	(void)ksize;
	return (long)key;
}

static inline int
ps_del_get_par(int id)
{ return id/PS_DEL_THD_PER_PAR; }

static inline long
ps_del_task_response(struct task *task)
{
	assert(task->state == FINISH);
	return task->ret;
}

static inline int 
ps_del_task_done(struct task *task)
{ return task->state == FINISH; }

void ps_del_init(void);
void ps_del_par_inint(ps_del_init_fn_t cb, size_t sz);
struct task *ps_del_prepare(coreid_t curr, int par, ps_del_fn_t func, void *key, int ksize, int arg_count, ...);
void ps_delegate(struct task *task);
void ps_del_run_server(coreid_t curr, int batch);

#define SERVER_EXE(task)                                                                               \
	__asm__ __volatile__ ("movq %3, %%r8 \n\t"                                                     \
			      "movq %4, %%rcx \n\t"                                                    \
			      "movq %5, %%rdx \n\t"                                                    \
			      "movq %6, %%rsi \n\t"                                                    \
			      "movq %7, %%rdi \n\t"                                                    \
			      "call *%1 \n\t"                                                          \
			      "movq %%rax, %0 \n\t" 	                                               \
			      : "=m" (task->ret)                                                       \
			      : "m" (task->funcptr),                                                   \
				"m" (task->argc),                                                      \
				"m" (task->args[4]),	                                               \
				"m" (task->args[3]),	                                               \
				"m" (task->args[2]),	                                               \
				"m" (task->args[1]),	                                               \
				"m" (task->args[0])	                                               \
			      : "memory", "rax", "rdi", "rsi", "r8", "r9", "r10", "r11", "rcx", "rdx") \

#define PS_DEL_EXE(func, key, ksize, ret, ...)                                                                      \
	do {                                                                                                        \
		coreid_t curr    = ps_coreid();                                                                     \
		int hashid       = (int)ps_del_hash((void *)key, ksize) % PS_DEL_NUM_THD;                           \
		int parid        = ps_del_get_par(hashid);       			                            \
		struct task *tsk = ps_del_prepare(curr, parid, (ps_del_fn_t)func, (void *)key, ksize, __VA_ARGS__); \
		if (parid == curr/PS_DEL_THD_PER_PAR) {                                                             \
			SERVER_EXE(tsk);		                                                            \
		} else {				                                                            \
			ps_delegate(tsk);                                                                           \
			while (!ps_del_task_done(tsk)) {                                                            \
				ps_del_run_server(curr, PS_DEL_SERVER_BATCH);	                                    \
			}                                                                                           \
		}					                                                            \
		ret = ps_del_task_response(tsk);			                                            \
	} while (0)					                                                            \

#endif	/* PS_DELEGATE_H */
