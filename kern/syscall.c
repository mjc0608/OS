/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/spinlock.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
    user_mem_assert(curenv, (void*)s, len, PTE_U | PTE_P);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

static int
sys_map_kernel_page(void* kpage, void* va)
{
	int r;
	struct Page* p = pa2page(PADDR(kpage));
	if(p ==NULL)
		return E_INVAL;
	r = page_insert(curenv->env_pgdir, p, va, PTE_U | PTE_W);
	return r;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

    struct Env *env;
    int ret;
    ret = env_alloc(&env, curenv->env_id);
    if (ret < 0) return ret;
    env->env_tf = curenv->env_tf;
    env->env_status = ENV_NOT_RUNNABLE;
    env->env_tf.tf_regs.reg_eax = 0;

    return env->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.

    struct Env *env;
    int ret;
    ret = envid2env(envid, &env, 1);
    if (ret < 0) return ret;
    if (status != ENV_RUNNABLE && status != ENV_RUNNABLE)
        return -E_INVAL;
    env->env_status = status;
    return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	//panic("sys_env_set_trapframe not implemented");

    struct Env *e;
    int r;

    r = envid2env(envid, &e, 1);
    if (r < 0) return r;

    if (!tf) return -E_INVAL;

    e->env_tf = *tf;
    e->env_tf.tf_cs |= 3;

    return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
    struct Env *env;
    if (envid2env(envid, &env, 1)<0) return -E_BAD_ENV;
    env->env_pgfault_upcall = func;
    //cprintf("[%08x] pgfault handler: %x\n", env->env_id, func);
    return 0;
}

static int
sys_env_set_user_fault_upcall(envid_t envid, int faultid, void *func)
{
    struct Env *env;
    if (envid2env(envid, &env, 1)<0) return -E_BAD_ENV;

    switch (faultid) {
        case T_DIVIDE:
            env->env_divzero_upcall = func;
            break;
        case T_GPFLT:
            env->env_gpflt_upcall = func;
            break;
        case T_ILLOP:
            env->env_illop_upcall = func;
            break;
    }

    return 0;

}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.

    struct Env *env;
    struct Page *pg;
    int ret;
    if (!((perm & PTE_U) && (perm & PTE_P))) return -E_INVAL;
    if (perm & (~PTE_SYSCALL)) return -E_INVAL;
    if ((uint32_t)va > UTOP || PGOFF(va)!=0) return -E_INVAL;
	if ((ret = envid2env(envid, &env, 1)) < 0)
		return -E_BAD_ENV;
    pg = page_alloc(ALLOC_ZERO);
    if (!pg) return -E_NO_MEM;

    ret = page_insert(env->env_pgdir, pg, va, perm);
    if (ret < 0) {
        page_free(pg);
        return -E_NO_MEM;
    }
    return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
    struct Env *src_env, *dst_env;
    int ret;
    struct Page *pg;
    if ((uint32_t)srcva > UTOP || ROUNDUP(srcva, PGSIZE)!=srcva
            || (uint32_t)dstva > UTOP || ROUNDUP(dstva, PGSIZE)!=dstva) {
        return -E_INVAL;
    }
    if (!((perm & PTE_U) && (perm & PTE_P))) return -E_INVAL;
    if (perm & (~PTE_SYSCALL)) return -E_INVAL;
    if (envid2env(srcenvid, &src_env, 1) < 0 ||
            envid2env(dstenvid, &dst_env, 1) < 0) {
        return -E_BAD_ENV;
    }

    pte_t *pte;
    pg = page_lookup(src_env->env_pgdir, srcva, &pte);
    if (!pg) return -E_INVAL;
    if ((perm & PTE_W) && !(*pte & PTE_W)) return -E_INVAL;
    if (page_insert(dst_env->env_pgdir, pg, dstva, perm)<0) return -E_NO_MEM;
    return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
    struct Env *env;
    if (envid2env(envid, &env, 1)<0) return -E_BAD_ENV;
    if ((uint32_t)va > UTOP || ROUNDUP(va, PGSIZE)!=va) {
        return -E_INVAL;
    }

    page_remove(env->env_pgdir, va);
    return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	//panic("sys_ipc_try_send not implemented");
    struct Env *dst_env;
    struct Page *pg;
    int r;
    pte_t *pte;
    r = envid2env(envid, &dst_env, 0);
    if (r < 0) return -E_BAD_ENV;

    if (!dst_env->env_ipc_recving) return -E_IPC_NOT_RECV;

    if ((uint32_t)srcva < UTOP) {
        if (ROUNDDOWN((uint32_t)srcva, PGSIZE)!=(uint32_t)srcva) return -E_INVAL;
        if (!((perm & PTE_U) && (perm & PTE_P))) return -E_INVAL;
        if (perm & (~PTE_SYSCALL)) return -E_INVAL;
        pg = page_lookup(curenv->env_pgdir, srcva, &pte);
        if (pg==NULL || (!(*pte & PTE_W) && (perm & PTE_W))) return -E_INVAL;
        r = page_insert(dst_env->env_pgdir, pg, dst_env->env_ipc_dstva, perm);
        if (r < 0) return -E_NO_MEM;
    }

    dst_env->env_ipc_value = value;
    dst_env->env_ipc_recving = 0;
    dst_env->env_ipc_from = curenv->env_id;
    dst_env->env_ipc_perm = perm;
    dst_env->env_tf.tf_regs.reg_eax = 0;
    dst_env->env_status = ENV_RUNNABLE;
    //cprintf("fucking %x get %d from fucking %x\n", dst_env->env_id, value, curenv->env_id);
    return 0;
}


// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	//panic("sys_ipc_recv not implemented");

    if ((uint32_t)dstva < UTOP && ROUNDUP((uint32_t)dstva, PGSIZE)!=(uint32_t)dstva) {
        // panic("fuck????????");
        return -E_INVAL;
    }

    curenv->env_status = ENV_NOT_RUNNABLE;
    curenv->env_ipc_dstva = dstva;
    curenv->env_ipc_recving = 1;

    //cprintf("thisenv=%x\n", curenv->env_id);

	return 0;
}

static void
region_alloc(struct Env *e, void *va, size_t len)
{
    uint32_t start_addr = ROUNDDOWN((uint32_t)va, PGSIZE);
    uint32_t end_addr = ROUNDUP((uint32_t)va+len, PGSIZE);
    int i;

    for (i=start_addr; i<end_addr; i+=PGSIZE) {
        struct Page *pp = page_alloc(0);
        if (!pp) {
            panic("failed to alloc page!\n");
        }
        int ret = page_insert(e->env_pgdir, pp, (void*)i, PTE_U | PTE_W);
        if (ret!=0) {
            panic("failed to insert page!\n");
        }
    }
}


static int
sys_sbrk(uint32_t inc)
{
	// LAB3: your code sbrk here...
    // may buggy
    uint32_t heap_top_roundup = ROUNDUP(curenv->heap_top, PGSIZE);
    int i;

    if (heap_top_roundup > curenv->heap_top + inc) {
        curenv->heap_top += inc;
        return curenv->heap_top;
    }

    region_alloc(curenv, (void*)heap_top_roundup, inc-(heap_top_roundup-curenv->heap_top));
    curenv->heap_top += inc;

	return curenv->heap_top;
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	panic("sys_time_msec not implemented");
}


// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

    int32_t ret = -E_INVAL;
    uint32_t srcenv, dstenv;

    switch(syscallno) {
        case SYS_cputs:
            sys_cputs((char*)a1, a2);
            ret = 0;
            break;
        case SYS_cgetc:
            ret = sys_cgetc();
            break;
        case SYS_getenvid:
            ret = sys_getenvid();
            break;
        case SYS_env_destroy:
            ret = sys_env_destroy(a1);
            break;
        case SYS_map_kernel_page:
            ret = sys_map_kernel_page((void*)a1, (void*)a2);
            break;
        case SYS_sbrk:
            ret = sys_sbrk(a1);
            break;
        case SYS_yield:
            sys_yield();
            ret = 0;
            break;
        case SYS_page_map:
            srcenv = (a1 >> 16) & 0xffff;
            dstenv = a1 & 0xffff;
            //cprintf("fuck: %x %x %x\n", a1, srcenv, dstenv);
            ret = sys_page_map(srcenv, (void*)a2, dstenv, (void*)a3, a4);
            break;
        case SYS_page_unmap:
            ret = sys_page_unmap(a1, (void*)a2);
            break;
        case SYS_page_alloc:
            //cprintf("page_alloc: %x, %x, %x\n", a1, a2, a3);
            ret = sys_page_alloc(a1, (void*)a2, a3);
            break;
        case SYS_env_set_status:
            ret = sys_env_set_status(a1, a2);
            break;
        case SYS_exofork:
            ret = sys_exofork();
            break;
        case SYS_env_set_pgfault_upcall:
            //cprintf("env %x set user fault handler: 0x%x\n", a1, a2);
            ret = sys_env_set_pgfault_upcall(a1, (void*)a2);
            break;
        case SYS_env_set_user_fault_upcall:
            ret = sys_env_set_user_fault_upcall(a1, a2, (void*)a3);
            break;
        case SYS_ipc_try_send:
            ret = sys_ipc_try_send(a1, a2, (void*)a3, a4);
            curenv->env_tf.tf_regs.reg_eax = ret;
            sched_yield();
            break;
        case SYS_ipc_recv:
            ret = sys_ipc_recv((void*)a1);
            curenv->env_tf.tf_regs.reg_eax = ret;
            sched_yield();
            ret = 0;
            break;
        case SYS_env_set_trapframe:
            ret = sys_env_set_trapframe(a1, (struct Trapframe*)a2);
            break;
    }

    return ret;
}

