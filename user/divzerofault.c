#include <inc/lib.h>

int zero;

void
handler(struct UTrapframe *utf)
{
    cprintf("wtf! 1/zero!\n");
    zero = 1;
}

void
umain(int argc, char **argv)
{
	zero = 0;
    set_divzero_handler(handler);
	cprintf("1/0 is %08x!\n", 1/zero);
}
