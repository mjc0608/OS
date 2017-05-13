// User-level page fault handler support.
// Rather than register the C page fault handler directly with the
// kernel as the page fault handler, we register the assembly language
// wrapper in pfentry.S, which in turns calls the registered C
// function.

#include <inc/lib.h>


extern void _divzero_upcall(void);
extern void _gpflt_upcall(void);
extern void _illop_upcall(void);
bool uxstack_set;

void (*_divzero_handler)(struct UTrapframe *utf);
void (*_gpflt_handler)(struct UTrapframe *utf);
void (*_illop_handler)(struct UTrapframe *utf);

void
set_divzero_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_divzero_handler == 0) {
        if (!uxstack_set && sys_page_alloc(0, (void*)(UXSTACKTOP-PGSIZE), PTE_U | PTE_W | PTE_P)) {
            panic("failed to alloc page\n");
        }
        uxstack_set = 1;

        sys_env_set_user_fault_upcall(0, T_DIVIDE, _divzero_upcall);
	}

	_divzero_handler = handler;
}

void
set_gpflt_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_gpflt_handler == 0) {
        if (!uxstack_set && sys_page_alloc(0, (void*)(UXSTACKTOP-PGSIZE), PTE_U | PTE_W | PTE_P)) {
            panic("failed to alloc page\n");
        }
        uxstack_set = 1;

        sys_env_set_user_fault_upcall(0, T_GPFLT, _gpflt_upcall);
	}

	_gpflt_handler = handler;
}

void
set_illop_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_illop_handler == 0) {
        if (!uxstack_set && sys_page_alloc(0, (void*)(UXSTACKTOP-PGSIZE), PTE_U | PTE_W | PTE_P)) {
            panic("failed to alloc page\n");
        }
        uxstack_set = 1;

        sys_env_set_user_fault_upcall(0, T_ILLOP, _illop_upcall);
	}

	_illop_handler = handler;
}
