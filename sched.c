#include "red_black_tree.h"
#include "sched.h"

#define MAX(a,b) ((a) > (b)) ? (a) : (b)

struct proc;
extern int tick(unsigned int pid);

struct sched_ent
{
	unsigned int pid;
	unsigned int vruntime;
	rb_red_blk_node *n;
};

struct cfs_rq
{
	rb_red_blk_tree *t;
	unsigned int min_vruntime;
	unsigned int fair_clock;
	unsigned int current;
	unsigned int nr_running;
};

static struct sched_ent task_table[10] =
{
	{.pid=0, .vruntime=0, .n=NULL},
	{.pid=1, .vruntime=0, .n=NULL},
	{.pid=2, .vruntime=0, .n=NULL},
	{.pid=3, .vruntime=0, .n=NULL},
	{.pid=4, .vruntime=0, .n=NULL},
	{.pid=5, .vruntime=0, .n=NULL},
	{.pid=6, .vruntime=0, .n=NULL},
	{.pid=7, .vruntime=0, .n=NULL},
	{.pid=8, .vruntime=0, .n=NULL},
	{.pid=9, .vruntime=0, .n=NULL}
};

static struct cfs_rq rq = {.t=NULL, .min_vruntime=0, .fair_clock=0, .current=0, .nr_running=0};

static rb_red_blk_node *leftmost(rb_red_blk_tree *t)
{
	rb_red_blk_node *n;

	for (n = t->root; n->left != t->nil; n = n->left);
	return n;
}

static int key_comp(const void* k1, const void* k2)
{
	int x1, x2;

	/* avoid vruntime overflow by subtracting min_vruntime */
	x1 = *((unsigned int*)k1)-rq.min_vruntime;
	x2 = *((unsigned int*)k2)-rq.min_vruntime;
	if (x1 == x2)
		return 0;
	return x1 < x2 ? -1 : 1;
}

int sched_init()
{
	rq.t = RBTreeCreate(key_comp, NullFunction, NullFunction, NullFunction, NullFunction);
	if (rq.t == NULL)
		return 1;
	return 0;
}

int sched_suspend(unsigned int pid)
{
	rb_red_blk_node *n;
	struct sched_ent *taskp;

	if (pid > 9) {
		fprintf(stderr, "sched_suspend: invalid PID %u\n", pid);
		return 1;
	}
	
	fprintf(stderr, "CTXT SWITCH (suspend)\n");
	taskp = &task_table[pid];
	if (rq.current == pid) {
		if (rq.nr_running > 1) {
			/* context switch */
			n = leftmost(rq.t);
			rq.current = *((unsigned int*)n->info);
			task_table[rq.current].n = NULL;
			RBDelete(rq.t, n);
		}
	} else {
		RBDelete(rq.t, taskp->n);
		taskp->n = NULL;
	}
	rq.nr_running--;
	return 0;
}

int sched_resume(unsigned int pid)
{
	struct sched_ent *newp, *curp;

	if (pid > 9) {
		fprintf(stderr, "sched_resume: invalid PID %u\n", pid);
		return 1;
	}

	fprintf(stderr, "CTXT SWITCH (resume)\n");

	newp = &task_table[pid];
	curp = &task_table[rq.current];
	/* don't let a process that has slept for an hour starve all the others */
	newp->vruntime = MAX(newp->vruntime, rq.min_vruntime-SCHED_LATENCY);
	rq.min_vruntime = newp->vruntime;
	rq.current = newp->pid;
	if (rq.nr_running > 0)
		curp->n = RBTreeInsert(rq.t, &(curp->vruntime), &(curp->pid));
	rq.nr_running++;
	return 0;
}

/* Note: a sched_suspend *must* be called after a reset */
int sched_reset(unsigned int pid)
{
	if (pid > 9) {
		fprintf(stderr, "reset: invalid PID %u\n", pid);
		return 1;
	}
	task_table[pid].vruntime = 0;
	return 0;
}

int sched_run()
{
	unsigned int next_tick;
	unsigned int i;
	rb_red_blk_node *n;
	struct sched_ent *curp;

	while (rq.nr_running > 0) {
		next_tick = rq.fair_clock+SCHED_GRANULARITY;
		while (rq.fair_clock != next_tick && rq.nr_running > 0) {
			if (++i % rq.nr_running == 0)
				rq.fair_clock++;
			task_table[rq.current].vruntime++;
			/* Note: a HALT here may remove the last running proc */
			tick(rq.current);
		}
		if (rq.nr_running < 2)
			continue;
		n = leftmost(rq.t);
		curp = &task_table[rq.current];
		if (key_comp(&(curp->vruntime), n->key) > 0) {
			fprintf(stderr, "CTXT SWITCH (timeout)\n");
			/* no longer the most unfairly treated proc, time for context switch */
			rq.min_vruntime = *((unsigned int*)n->key);
			rq.current = *((unsigned int*)n->info);
			curp->n = RBTreeInsert(rq.t, &(curp->vruntime), &(curp->pid));
			RBDelete(rq.t, n);
			task_table[rq.current].n = NULL;
		}
	}
	return 0;
}
