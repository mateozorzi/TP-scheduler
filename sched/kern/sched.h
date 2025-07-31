/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_SCHED_H
#define JOS_KERN_SCHED_H
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

// This function does not return.
void sched_yield(void) __attribute__((noreturn));

void sched_destroy_env(envid_t envid);
void sched_push_env(envid_t env_id, int queue);

extern struct MLFQ_sched mlfq_sched;

#define NUMBER_OF_QUEUE 3

struct MLFQ_queue {
	envid_t envs[NENV];
	int last;
	int beginning;
};
struct MLFQ_sched {
	struct MLFQ_queue q0;
	struct MLFQ_queue q1;
	struct MLFQ_queue q2;
	int total_executions;
	int current_alive_process;
};

struct sched_stats {
	int total_executions;  // Number of time of scheduler executed
	int total_executions_envs;
	int execute_envs[10000];  // Number of runnable environments
	int total_boosts;  // Number of times the scheduler boosted the environments
	int total_halt;  // Number of times the scheduler halted the CPU
};


#endif  // !JOS_KERN_SCHED_H
