#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

    int len;
    int ret;
    uint8_t buffer[2048];

    while (1) {
        len = sys_net_try_receive(buffer);
        if (len < 0) {
            sys_yield();
            continue;
        }

        ret = sys_page_alloc(0, &nsipcbuf, PTE_P|PTE_U|PTE_W);
        if (ret < 0) panic("fuckit, failed to alloc new page");

        nsipcbuf.pkt.jp_len = len;
        memmove(nsipcbuf.pkt.jp_data, buffer, len);

        ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_U|PTE_W);
    }
}
