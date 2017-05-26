
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

static bool
va_is_accessed(void *va)
{
    return (vpt[PGNUM(va)] & PTE_A) != 0;
}

static int
va_get_recent_level(void *va)
{
    return ((vpt[PGNUM(va)] & PTE_AVAIL) >> 9) & 0x1;
}

static void
va_set_recent_level(void *va, int level)
{
    int perm = PTE_W | PTE_U | PTE_P | ((level&0x1)<<9);
    int r = sys_page_map(0, va, 0, va, perm);
    if (r < 0) panic("failed to remap");
}

static void
va_reduce_recent_level(void *va)
{
    int level = va_get_recent_level(va);
    if (level == 0) {
        sys_page_unmap(0, va);
        return;
    }
    level--;
    va_set_recent_level(va, level);
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


    // eviction
    uint32_t *e_addr;
    for (e_addr = (uint32_t*)DISKMAP; e_addr<(uint32_t*)(DISKMAP+DISKSIZE); e_addr+=PGSIZE) {
        if (e_addr == round_addr) continue;
        if (va_is_mapped(e_addr)) {
            if (!va_is_accessed(e_addr)) {
                va_reduce_recent_level(e_addr);
            }
        }
    }
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
	set_pgfault_handler(bc_pgfault);
	check_bc();
}

