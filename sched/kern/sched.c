#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/syscall.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/sched.h>

#define MAX_MLFQ_EXECUTIONS 10

int total_executions = 0;
int total_boosts = 0;
int total_halt = 0;


struct MLFQ_sched mlfq_sched = { .q0 = { .last = 0, .beginning = 0 },
	                         .q1 = { .last = 0, .beginning = 0 },
	                         .q2 = { .last = 0, .beginning = 0 },
	                         .total_executions = 0,
	                         .current_alive_process = 0 };


struct MLFQ_queue *
get_queue(int queue)
{
	if (queue < 0) {
		return NULL;
	}

	switch (queue) {
	case 0:
		return &mlfq_sched.q0;
	case 1:
		return &mlfq_sched.q1;
	default:
		return &mlfq_sched.q2;
	}
}

void
sched_push_queue(envid_t env_id)
{
	struct Env *env = &envs[ENVX(env_id)];
	int queue = env->current_queue;
	struct MLFQ_queue *q = get_queue(queue);

	int index = q->last % NENV;
	q->envs[index] = env_id;
	q->last++;
	mlfq_sched.current_alive_process++;
}

void
sched_remove_env(struct MLFQ_queue *queue, int position)
{
	if (position == queue->beginning) {
		queue->beginning++;
		return;
	}

	envid_t first = queue->envs[queue->beginning % NENV];
	envid_t target = queue->envs[position];

	envid_t aux = first;
	queue->envs[queue->beginning % NENV] = target;
	queue->envs[position] = aux;

	queue->beginning++;
}

void
sched_destroy_env(envid_t env_id)
{
	struct Env *env = &envs[ENVX(env_id)];
	int queue = env->current_queue;
	struct MLFQ_queue *q = get_queue(queue);

	for (int i = q->beginning; i < q->last; i++) {
		if (q->envs[i % NENV] == env_id) {
			sched_remove_env(q, i);
			mlfq_sched.current_alive_process--;
			break;
		}
	}
}
void
sched_push_env(envid_t env_id, int queue)
{
	struct MLFQ_queue *q = get_queue(queue);

	int index = q->last % NENV;
	envs[ENVX(env_id)].current_queue = queue;
	q->envs[index] = env_id;
	q->last++;
}

void
boost_enviroments()
{
	total_boosts++;
	// we boost all queues except the first one
	for (int i = 1; i < NUMBER_OF_QUEUE; i++) {
		struct MLFQ_queue *q = get_queue(i);
		for (int j = q->beginning; j < q->last; j++) {
			envid_t env_id = q->envs[j % NENV];
			sched_push_env(env_id, 0);
		}
	}
	// we reset all the queues
	mlfq_sched.q1.last = 0;
	mlfq_sched.q1.beginning = 0;

	mlfq_sched.q2.last = 0;
	mlfq_sched.q2.beginning = 0;
}

void sched_halt(void);

void
priority_queue()
{
	// lets get current enviroment
	struct Env *env = curenv;

	// lets get the current queue containing a running enviroment
	struct MLFQ_queue *best_q = NULL;
	int best_q_index = -1;
	bool best_q_found = false;

	for (int i = 0; i < NUMBER_OF_QUEUE; i++) {
		// i need to check here if the queue contains a running enviroment
		struct MLFQ_queue *temp_q = get_queue(i);
		for (int j = temp_q->beginning; j < temp_q->last; j++) {
			envid_t env_id = temp_q->envs[j % NENV];
			struct Env *temp_env = &envs[ENVX(env_id)];

			// its runnable?
			if (temp_env->env_status == ENV_RUNNABLE) {
				best_q = temp_q;
				best_q_index = i;
				best_q_found = true;
				break;
			}
		}
		if (best_q_found) {
			break;
		}
	}

	// at this point, we have the best queue to run or we dont have any
	// runnable enviroment if we dont have any runnable enviroment, we need
	// check if current env is still running else we need to halt
	if (best_q_index == -1) {
		if (curenv && curenv->env_status == ENV_RUNNING) {
			// stats

			env_run(curenv);
		} else {
			sched_halt();
		}
	}

	// we have either best queue or we have a running enviroment
	for (int i = best_q->beginning; i < best_q->last; i++) {
		int index = i % NENV;
		envid_t next_env_id = best_q->envs[index];
		struct Env *next_env = &envs[ENVX(next_env_id)];

		if (next_env->env_status == ENV_RUNNABLE) {
			// we remove it from the queue
			// we need to check if its the first one
			sched_remove_env(best_q, index);

			// now to the next enviroment we need to get a new id
			int next_env_id_queue_index =
			        best_q_index >= NUMBER_OF_QUEUE - 1
			                ? NUMBER_OF_QUEUE - 1
			                : best_q_index + 1;

			// we need to push the next enviroment to the next queue
			sched_push_env(next_env_id, next_env_id_queue_index);
			mlfq_sched.total_executions++;

			env_run(next_env);  // we run it
		}
	}
}

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	total_executions++;
#ifdef SCHED_ROUND_ROBIN
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running. Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// Your code here - Round robin


	int index = curenv ? ENVX(curenv->env_id) + 1 : 0;
	for (int i = 0; i < NENV; i++) {
		if (envs[index].env_status == ENV_RUNNABLE) {
			env_run(&envs[index]);
		}
		index = (index + 1) % NENV;
	}

	if (curenv && curenv->env_status == ENV_RUNNING) {
		env_run(curenv);
	}

	sched_halt();

#endif

#ifdef SCHED_PRIORITIES
	// Implement simple priorities scheduling.
	//
	// Environments now have a "priority" so it must be consider
	// when the selection is performed.
	//
	// Be careful to not fall in "starvation" such that only one
	// environment is selected and run every time.

	// Your code here - Priorities
	if (mlfq_sched.total_executions >= MAX_MLFQ_EXECUTIONS) {
		boost_enviroments();
		mlfq_sched.total_executions = 0;
	}

	priority_queue();

	// sched_halt();
#endif

	// Without scheduler, keep runing the last environment while it exists
	if (curenv) {
		env_run(curenv);
	}

	// sched_halt never returns
	sched_halt();

	panic("should not return");
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	total_halt++;

	int i;


	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		cprintf("Number of time of scheduler executed: %d\n",
		        total_executions);
		cprintf("\nHistory of executed environments:\n\n");

		for (int i = 0; i < NENV; i++) {
			if (envs[i].env_id != 0) {
				cprintf(" env_id %d -", envs[i].env_id);
				cprintf(" env_runs %d \n", envs[i].env_runs);
			}
		}

		cprintf("\n");
		cprintf("Number of times the scheduler boosted the "
		        "environments: %d\n",
		        total_boosts);
		cprintf("Number of times the scheduler halted the CPU: %d\n",
		        total_halt);

		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Once the scheduler has finishied it's work, print statistics on
	// performance. Your code here

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile("movl $0, %%ebp\n"
	             "movl %0, %%esp\n"
	             "pushl $0\n"
	             "pushl $0\n"
	             "sti\n"
	             "1:\n"
	             "hlt\n"
	             "jmp 1b\n"
	             :
	             : "a"(thiscpu->cpu_ts.ts_esp0));
}
