// evil hello world -- kernel pointer passed to kernel
// kernel should destroy user environment in response

#include <inc/lib.h>
#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/memlayout.h>

// Call this function with ring0 privilege
void evil()
{
	// Kernel memory access
	*(char*)0xf010000a = 0;

	// Out put something via outb
	outb(0x3f8, 'I');
	outb(0x3f8, 'N');
	outb(0x3f8, ' ');
	outb(0x3f8, 'R');
	outb(0x3f8, 'I');
	outb(0x3f8, 'N');
	outb(0x3f8, 'G');
	outb(0x3f8, '0');
	outb(0x3f8, '!');
	outb(0x3f8, '!');
	outb(0x3f8, '!');
	outb(0x3f8, '\n');
}

static void
sgdt(struct Pseudodesc* gdtd)
{
	__asm __volatile("sgdt %0" :  "=m" (*gdtd));
}

struct Gatedesc entry_backup;
struct Gatedesc *gdt;
void (*func_to_call)(void);
void ring0_call_fun_ret() {
    func_to_call();
    gdt[GD_TSS0 >> 3] = entry_backup;
    asm volatile("leave\n\t");
    asm volatile("lret\n\t");
}

// Invoke a given function pointer with ring0 privilege, then return to ring3
void ring0_call(void (*fun_ptr)(void)) {
    // Here's some hints on how to achieve this.
    // 1. Store the GDT descripter to memory (sgdt instruction)
    // 2. Map GDT in user space (sys_map_kernel_page)
    // 3. Setup a CALLGATE in GDT (SETCALLGATE macro)
    // 4. Enter ring0 (lcall instruction)
    // 5. Call the function pointer
    // 6. Recover GDT entry modified in step 3 (if any)
    // 7. Leave ring0 (lret instruction)

    // Hint : use a wrapper function to call fun_ptr. Feel free
    //        to add any functions or global variables in this 
    //        file if necessary.

    // Lab3 : Your Code Here
    struct Pseudodesc gdtr;
    uint8_t *kern_page = (uint8_t*)0x008f0000;
    func_to_call = fun_ptr;
    sgdt(&gdtr);
    int ret = sys_map_kernel_page((void*)gdtr.pd_base, kern_page);
    if (ret!=0) {
        panic("fuck!\n");
    }
    gdt = (struct Gatedesc*)(kern_page + gdtr.pd_base % PGSIZE);
    entry_backup = gdt[GD_TSS0 >> 3];
    SETCALLGATE(gdt[GD_TSS0 >> 3], GD_KT, ring0_call_fun_ret, 3);
    cprintf("before lcall\n");
    asm volatile("lcall %0, $0\n"::"i"(GD_TSS0));
}

void
umain(int argc, char **argv)
{
        // call the evil function in ring0
	ring0_call(&evil);

	// call the evil function in ring3
	evil();
}

