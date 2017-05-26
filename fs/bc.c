
#include "fs.h"

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (vpd[PDX(va)] & PTE_P) && (vpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (vpt[PGNUM(va)] & PTE_D) != 0;
}

#define CACHE_MAX 15
static uint32_t* cacheitems[CACHE_MAX];
static uint64_t total_mapped = 0;

static bool
va_is_accessed(void *va)
{
    return (vpt[PGNUM(va)] & PTE_A) != 0;
}

static int
va_get_recent_level(void *va)
{
    return ((vpt[PGNUM(va)] & PTE_AVAIL) >> 9) & 0x3;
}

static void
va_set_recent_level(void *va, int level)
{
    int perm = PTE_W | PTE_U | PTE_P | ((level&0x3)<<9);
    int r = sys_page_map(0, va, 0, va, perm);
    if (r < 0) panic("failed to remap");
}

static bool
va_reduce_recent_level(void *va)
{
    int level = va_get_recent_level(va);
//    cprintf("level: va: 0x%08x, level: %d\n", va, level);
    if (level == 0) {
        flush_block(va);
        sys_page_unmap(0, va);
        return 1;
    }
    level--;
    va_set_recent_level(va, level);
    return 0;
}


int before_fuck() {
    // do nothing
    cprintf("fuck it!\n");
    return 1;
}

// Fault any disk block that is read or written in to memory by
// loading it from disk.
// Hint: Use ide_read and BLKSECTS.

static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;
    int i;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page, and mark the
	// page not-dirty (since reading the data from disk will mark
	// the page dirty).
	//
	// LAB 5: Your code here
	//panic("bc_pgfault not implemented");

    void *round_addr = ROUNDDOWN(addr, PGSIZE);
    r = sys_page_alloc(0, round_addr, PTE_W | PTE_U | PTE_P);
    if (r < 0) panic("cannot alloc page");
    r = ide_read(blockno * BLKSECTS, round_addr, BLKSECTS);
    if (r < 0) panic("cannot read from disk");
    r = sys_page_map(0, round_addr, 0, round_addr, PTE_W | PTE_U | PTE_P);
    if (r < 0) panic("cannot remap page");

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);

//    cprintf("enter bc_pgfault, va: %08x, bno: %d\n", addr, blockno);
//    if (addr == (void*)0x1001902c && total_mapped == 3) before_fuck();
insert:
    if (total_mapped < CACHE_MAX) {
//        cprintf("try to find one slot\n");
        for (i = 0; i < CACHE_MAX; i++) {
            if (cacheitems[i] == 0) {
                cacheitems[i] = round_addr;
                total_mapped++;
//                cprintf("find slot %d\n", i);
                goto out;
            }
            else if (cacheitems[i] == round_addr) {
//                cprintf("fuck the lab, this is because the fucking test script\n");
                goto out;
            }
        }
    }
    else {
        while (1) {
//            cprintf("iterating\n");
            for (i = 0; i < CACHE_MAX; i++) {
                uint32_t *e_addr = cacheitems[i];
                if (e_addr == 0 || e_addr == round_addr) continue;
                if (e_addr == (void*)super) continue;
                if (va_is_accessed(e_addr)) {
//                    cprintf("va 0x%08x is accessed\n", e_addr);
                    flush_block(e_addr);
                    sys_page_map(0, e_addr, 0, e_addr, PTE_SYSCALL);
                }
                else {
#if 0
                    cprintf("va 0x%08x is flushed out\n", e_addr);
                    flush_block(e_addr);
                    sys_page_unmap(0, e_addr);
                    cacheitems[i] = 0;
                    total_mapped--;
//                    if (total_mapped < CACHE_MAX) break;
#else
                    int deleted = va_reduce_recent_level(e_addr);
                    if (deleted) {
                        cacheitems[i] = 0;
                        total_mapped--;
                    }
#endif
                }
            }
            if (total_mapped < CACHE_MAX) break;
        }
    }

    for (i = 0; i < CACHE_MAX; i++) {
        if (cacheitems[i] == 0) {
            cacheitems[i] = round_addr;
            total_mapped++;
//            cprintf("find slot %d\n", i);
            goto out;
        }
        else if (cacheitems[i] == round_addr) {
//            cprintf("fuck the lab, this is because the fucking test script\n");
            goto out;
        }
    }

out:

    do {
#if 0
        cprintf("total: %d\n", total_mapped);
        if (total_mapped <= CACHE_MAX) {
            for (i = 0; i < CACHE_MAX; i++) {
                cprintf("slot %d: %08x\n", i, cacheitems[i]);
            }
        }
        cprintf("------------------------------------------------------\n");
        //if (blockno == 25 && total_mapped == 4) before_fuck();
#endif
    } while(0);

}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	//panic("flush_block not implemented");
    addr = ROUNDDOWN(addr, PGSIZE);
    if (!va_is_mapped(addr)) return;
    if (!va_is_dirty(addr)) return;

    int r;
    r = ide_write(blockno * BLKSECTS, addr, BLKSECTS);
    if (r < 0) panic("unable to write back block");
    r = sys_page_map(0, addr, 0, addr, PTE_SYSCALL);
    if (r < 0) panic("failed to map page");

}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
    int i;
    for (i = 0; i < CACHE_MAX; i++) {
        cacheitems[i] = 0;
    }
	set_pgfault_handler(bc_pgfault);
	check_bc();
}

