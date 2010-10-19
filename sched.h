#define SCHED_GRANULARITY 9 /* in fair clock units (1/running_procs) */
#define SCHED_LATENCY 2
int sched_init();
int sched_suspend(unsigned int pid);
int sched_resume(unsigned int pid);
int sched_reset(unsigned int pid);
int sched_run();

