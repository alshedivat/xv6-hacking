// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

#define QEMUSZ 512*1024*1024  // QEMU predefined size (see Makefile)

void freerange(void *vstart, void *vend);
extern char end[];  // first address after kernel loaded from ELF file
char* bottom;       // first page after the freebitmap
int BMSIZE;         // freebitmap size

struct {
  struct spinlock lock;
  int use_lock;
  int working_byte;
  char* ceiling;
  char* freebitmap;
} kmem;

// Auxillary functions for working with bitmap
void update_working_byte(int byte_ind) {
  // Keep working byte being the smallest non zero byte in the freebitmap
  if (kmem.freebitmap[byte_ind] && byte_ind < kmem.working_byte)
    kmem.working_byte = byte_ind;

  // If working byte became a zero byte, go and find the first non zero one
  while (!kmem.freebitmap[kmem.working_byte])
    ++kmem.working_byte;
}

void set_bit(char* v, int bit) {
  uint bit_num = (v - bottom) / PGSIZE;
  uint byte_ind = bit_num / 8;
  uint bit_pos = bit_num % 8;

  if (bit)
    kmem.freebitmap[byte_ind] |= (1<<bit_pos);
  else
    kmem.freebitmap[byte_ind] &= ~(1<<bit_pos);

  update_working_byte(byte_ind);
}

char* get_free_page(void) {
  int bit_pos = __builtin_ctz(kmem.freebitmap[kmem.working_byte]);
  int bit_num = kmem.working_byte * 8 + bit_pos;
  char* v = (char*)(bottom + bit_num * PGSIZE);

  if (v >= kmem.ceiling)
    return 0;

  return v;
}

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;

  // Place the bitmap right at the "end"
  kmem.freebitmap = end;
  BMSIZE = (QEMUSZ - v2p(end)) / (8 * PGSIZE + 1);
  bottom = (char*)PGROUNDUP((uint)end + BMSIZE);

  // Initialize freebitmap:
  // - memset the freebitmap with ones,
  // - memorize the ceiling virtual address
  // - set working byte to 0
  memset(kmem.freebitmap, 255, BMSIZE);
  kmem.ceiling = vend;
  kmem.working_byte = 0;
}

void
kinit2(void *vstart, void *vend)
{
  kmem.ceiling = vend;
  kmem.use_lock = 1;
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  if((uint)v % PGSIZE || v < end || v2p(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);

  // Set the propper bit for the free page
  set_bit(v, 1);

  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  if(kmem.use_lock)
    acquire(&kmem.lock);

  // Get a free page and update the freebitmap
  char* r = get_free_page();
  if (r)
    set_bit(r, 0);

  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

