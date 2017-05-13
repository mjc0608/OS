// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/x86.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
//#undef panic
//#define panic(x) do{ cprintf(x); cprintf("\n"); fuckit(); } while(0)
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    if ((vpd[PDX(addr)] & PTE_P) == 0) panic("page not present, addr=0x%x", addr);
    if ((vpt[PGNUM(addr)] & PTE_P) == 0) panic("page not present");
    if ((vpt[PGNUM(addr)] & PTE_COW) == 0) panic("not a COW page");
    if ((err & FEC_WR) == 0) panic("not a write fault");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
    r = sys_page_alloc(0, (void*)PFTEMP, PTE_U | PTE_W | PTE_P);
    if (r<0) panic("failed to alloc page");
    uint32_t page_start = ROUNDDOWN((uint32_t)addr, PGSIZE);
    memmove((void*)PFTEMP, (void*)page_start, PGSIZE);
    r = sys_page_map(0, (void*)PFTEMP, 0, (void*)page_start, PTE_U | PTE_W | PTE_P);
    if (r<0) panic("failed to map page");
    r = sys_page_unmap(0, (void*)PFTEMP);
    if (r<0) panic("failed to unmap page");

	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//panic("duppage not implemented");

    uint32_t va = pn * PGSIZE;
    if (va >= UTOP) panic("above UTOP");

    pte_t pte = vpt[PGNUM(va)];
    pde_t pde = vpd[PDX(va)];

    if ((pde & PTE_P) == 0) panic("page not exist");
    if ((pte & PTE_P) == 0) panic("page not exist");

    if (pte & (PTE_W | PTE_COW)) {
        r = sys_page_map(0, (void*)va, envid, (void*)va, PTE_U | PTE_P | PTE_COW);
        if (r<0) panic("failed to map page");
        r = sys_page_map(0, (void*)va, 0, (void*)va, PTE_U | PTE_P | PTE_COW);
        if (r<0) panic("failed to map page");
    }
    else {
        r = sys_page_map(0, (void*)va, envid, (void*)va, PTE_U | PTE_P);
        if (r<0) panic("failed to map page");
    }

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
extern void _pgfault_upcall(void);

envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");

    int r;
    envid_t envid;
    uint8_t *addr;

    set_pgfault_handler(pgfault);

    //cprintf("before: esp: %x, ebp: %x\n", read_esp(), read_ebp());
    envid = sys_exofork();
    //cprintf("after: esp: %x, ebp: %x\n", read_esp(), read_ebp());
    if (envid < 0) panic("exofork failed");
    if (envid == 0) {
        //`cprintf("control reach here\n\n\n\n\n\n\n\n\n\n\n\n");
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    for (addr = (uint8_t*)UTEXT; (uint32_t)addr < UTOP; addr += PGSIZE) {
        if ((uint32_t)addr == UXSTACKTOP - PGSIZE) {
            r = sys_page_alloc(envid, (void*)(UXSTACKTOP- PGSIZE), PTE_P | PTE_U | PTE_W);
            if (r<0) panic("alloc uxstack failed");
        }
        else {
            if ((vpd[PDX(addr)] & PTE_P) && (vpt[PGNUM(addr)] & PTE_P)
                    && (vpt[PGNUM(addr)] & PTE_U)) {
                duppage(envid, (uint32_t)addr/PGSIZE);
            }
        }
    }

    r = sys_env_set_pgfault_upcall(envid, (void*)_pgfault_upcall);
    if (r < 0) panic("set pgfault failed");

    r = sys_env_set_status(envid, ENV_RUNNABLE);
    if (r < 0) panic("set runable failed");

    return envid;

}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
