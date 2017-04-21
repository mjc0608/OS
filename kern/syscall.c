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

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

    switch(syscallno) {
        case SYS_cputs:
            sys_cputs((char*)a1, a2);
            return 0;
        case SYS_cgetc:
            return sys_cgetc();
        case SYS_getenvid:
            return sys_getenvid();
        case SYS_env_destroy:
            return sys_env_destroy(a1);
        case SYS_map_kernel_page:
            return sys_map_kernel_page((void*)a1, (void*)a2);
        case SYS_sbrk:
            return sys_sbrk(a1);
    }

    return -E_INVAL;
}

