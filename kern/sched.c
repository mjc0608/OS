#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/spinlock.h>


// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;
	int i;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING) and never choose an
	// idle environment (env_type == ENV_TYPE_IDLE).  If there are
	// no runnable environments, simply drop through to the code
	// below to switch to this CPU's idle environment.

	// LAB 4: Your code here.

#if 0
    if (curenv == NULL) {
	    for (i = 0; i < NENV; i++) {
		    if (envs[i].env_type != ENV_TYPE_IDLE &&
		        (envs[i].env_status == ENV_RUNNABLE ||
    		     envs[i].env_status == ENV_RUNNING))
	    		break;
    	}
        if (i == NENV) {
		    cprintf("No more runnable environments!\n");
            unlock_kernel();
    		while (1)
    			monitor(NULL);
    	}
        else {
            env_run(envs+i);
        }
    }
#endif
    //cprintf("cpu %d enter yield\n", cpunum());
    if (curenv==NULL) {
        //cprintf("cpu %d first round\n", cpunum());
        goto null_round;
    }

    struct Env *next;
    int curenv_index = ENVX(curenv->env_id);
    int iterenv_index = curenv_index + 1;

    while (iterenv_index != curenv_index) {
        if (envs[iterenv_index].env_status == ENV_RUNNABLE
                && envs[iterenv_index].env_type != ENV_TYPE_IDLE) {
            //cprintf("cpu %d start user program\n", cpunum());
            env_run(envs+iterenv_index);
        }
        iterenv_index++;
        iterenv_index%=NENV;
    }

    if (iterenv_index == curenv_index &&
        envs[iterenv_index].env_status == ENV_RUNNING) {
        env_run(envs+iterenv_index);
    }
    //cprintf("cpu %d cannot find runable\n", cpunum());

null_round:
	// For debugging and testing purposes, if there are no
	// runnable environments other than the idle environments,
	// drop into the kernel monitor.
    for (i = 0; i < NENV; i++) {
        if (envs[i].env_type != ENV_TYPE_IDLE &&
            (envs[i].env_status == ENV_RUNNABLE ||
             envs[i].env_status == ENV_RUNNING))
            break;
    }
    if (i == NENV) {
        cprintf("No more runnable environments!\n");
        while (1)
            monitor(NULL);
    }

	// Run this CPU's idle environment when nothing else is runnable.
	idle = &envs[cpunum()];
	if (!(idle->env_status == ENV_RUNNABLE || idle->env_status == ENV_RUNNING))
		panic("CPU %d: No idle environment!", cpunum());
    //cprintf("cpu %d start idle\n", cpunum());
	env_run(idle);
}
