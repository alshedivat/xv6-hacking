#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "defs.h"
#include "ksm.h"

// KSM segment structure
// NOTE: it is the same as the info structure, but doesn't contain global info
//       and it has some extra fields
struct ksmseg_t {
  int id;                 // Segment identifier      
  pde_t* pgdir;           // Page directory for the shared memory segment
  uint ksmsz;             // The size of the shared memory segment
  int cpid;               // PID of the creator
  int mpid;               // PID of the last modifier
  uint attached_num;      // Number of attached processes
  uint atime;             // Last attach time
  uint dettime;           // Last detach time
  uint deltime;           // Last delete time (setted after the segment destroyed)
};

// KSM global info structure
static struct ksmglobalinfo_t {
  uint total_shsg_num;    // Total number of existing shared segments
  uint total_shpg_num;    // Total number of existing shared pages
} ksm_globalinfo;

// KSM structures
struct ksmseg_t ksm_sgtable[KSM_SEG_MAX_NUM];
struct spinlock ksmlock;   // The lock we use for locking ksm global structure

// Initialize global structures
void ksminit(void) {
  initlock(&ksmlock, "ksm");
  memset(ksm_sgtable, 0, sizeof(ksm_sgtable));
  ksm_globalinfo.total_shsg_num = ksm_globalinfo.total_shpg_num = 0;
}

// Update ksm_freebitmap setting bits to 1 (freeing) or to 0
static void update_ksmbitmap(char* seg_bottom, uint pgnum, int free) {
  uint right = (KERNBASE - (uint)seg_bottom) / PGSIZE;
  int i;
  if (free) {
    for (i = right - 1; i + pgnum >= right; --i)
      proc->ksm_freebitmap[i/8] |= (1<<(i%8));
    proc->ksm_bottom = (proc->ksm_bottom == seg_bottom) ?
        (seg_bottom + pgnum*PGSIZE) : proc->ksm_bottom;
  } else {
    for (i = right - 1; i + pgnum >= right; --i)
      proc->ksm_freebitmap[i/8] &= ~(1<<(i%8));
    proc->ksm_bottom = (proc->ksm_bottom > seg_bottom) ?
        seg_bottom : proc->ksm_bottom;
  }
}

// Find an address of the first suitable shared memory free segment
// NOTE: addresses here are in user virtual memory
static char* find_first_freeseg(uint pgnum) {
  int max_bottom = (KERNBASE - PGROUNDUP(proc->sz)) / PGSIZE;
  int MAX_FREEBM_SZ = (KSM_FREEBM_SZ > max_bottom) ? max_bottom : KSM_FREEBM_SZ;

  int i = 0;
  while (i < MAX_FREEBM_SZ*8) {
    uint freeseg_pgnum = 0;
    for(;!(proc->ksm_freebitmap[i/8] & 1<<(i%8)) && i < MAX_FREEBM_SZ*8; ++i)
      ;
    for(;(proc->ksm_freebitmap[i/8] & 1<<(i%8)) && i < MAX_FREEBM_SZ*8; ++i)
      if (++freeseg_pgnum == pgnum)
        break;
    if (freeseg_pgnum == pgnum)
      return (char*)(KERNBASE - (i+1)*PGSIZE);
  }
  return 0;
}

// Copy the contents of the target page directory (tpd) into the
// source page directory (spd) starting from addresses t_start_a and s_start_a
// respectfully. If flag is equal to 0, unset PTE_W flag.
void copy_pgdir(pde_t* spd, pde_t* tpd, char* s_start_a, char* t_start_a,
                uint size, int flag) {
  pde_t* spte;
  pde_t* tpte;
  uint a;
  for (a = 0; a < size; a += PGSIZE) {
    tpte = walkpgdir(tpd, t_start_a + a, 0);
    if (!tpte) {
      a += (NPTENTRIES - 1) * PGSIZE;
      continue;
    }
    spte = walkpgdir(spd, s_start_a + a, 1);
    if (flag)
      *spte = *tpte;
    else
      *spte = *tpte & (~PTE_W);
  }
}

// Clean provided pgdir without deallocation physical pages
static void clean_pgdir(pde_t* pgdir, char* start_addr, uint size) {
  pde_t* pte;
  int a;
  for (a = 0; a < size; a += PGSIZE) {
    pte = walkpgdir(pgdir, start_addr + a, 0);
    if (!pte)
      panic("clean_pgdir: holes in ksm segment");
    *pte = 0;
  }
}

// Set time funtion. Just for purpose not to repeat the code.
static void set_time(uint* time) {
  acquire(&tickslock);
  *time = ticks;
  release(&tickslock);
}

// Destroy a KSM segment
// NOTE: the function should be called after locking ksmlock!
static void destroy_seg(int hd) {
  // Update the global info
  --ksm_globalinfo.total_shsg_num;
  ksm_globalinfo.total_shpg_num -= PGROUNDUP(ksm_sgtable[hd].ksmsz)/PGSIZE;

  // Deallocate the shared memory segment and earase internal pgdir
  freevm(ksm_sgtable[hd].pgdir, ksm_sgtable[hd].ksmsz);
  ksm_sgtable[hd] = (struct ksmseg_t){0};

  // Update delete timestamp
  set_time(&ksm_sgtable[hd].deltime);
}

void ksm_copy_proc(struct proc* sp, struct proc* tp) {
  sp->ksm_bottom = tp->ksm_bottom;
  memmove(sp->ksm_freebitmap, tp->ksm_freebitmap, PGSIZE);
  memmove(sp->ksm_mstable, tp->ksm_mstable, PGSIZE);
  copy_pgdir(sp->pgdir, tp->pgdir, sp->ksm_bottom, tp->ksm_bottom,
             KERNBASE - (uint)tp->ksm_bottom, 1);
  
  acquire(&ksmlock);
  // Update all attached counters
  int hd;
  for (hd = 0; hd < KSM_SEG_MAX_NUM; ++hd)
    if (sp->ksm_mstable[hd].bottom) {
      ++ksm_sgtable[hd].attached_num;
      ksm_sgtable[hd].mpid = sp->pid;
      set_time(&ksm_sgtable[hd].atime);
    }
  release(&ksmlock);
}

// Allocate the memory, manage the internal structures, and
// return a handle or an error code.
int ksmget(int key, uint size) {
  if (key <= 0)
    return ERR_BAD_KEY;
  if (size > KSM_SEG_MAXSZ)
    return ERR_SEG_WRONG_SIZE;

  acquire(&ksmlock);
  // Search for a KSM segment with the same key
  int handle = -1;
  int i;
  for (i = 0; i < KSM_SEG_MAX_NUM; ++i) {
    if (handle < 0 && ksm_sgtable[i].id == 0)
      handle = i;
    if (key == ksm_sgtable[i].id) {
      if (size == 0 || ksm_sgtable[i].ksmsz == size) {
        ksm_sgtable[i].mpid = proc->pid;
        set_time(&proc->ksm_mstable[i].gettime);
        release(&ksmlock);
        return i + 1;
      } else {
        release(&ksmlock);
        return ERR_SEG_KEY_TAKEN;
      }
    }
  }
  if (handle < 0) {
    release(&ksmlock);
    return ERR_SEG_NOT_AVAILABLE;
  }

  // Segment has not been created yet, should first perform all the allocation
  // procedures, and hence we cannot accept size == 0.
  if (size == 0) {
    release(&ksmlock);
    return ERR_SEG_NOT_CREATED;
  }

  // Allocate a new segment
  ksm_sgtable[handle].pgdir = (pde_t*)kalloc();
  memset(ksm_sgtable[handle].pgdir, 0, PGSIZE);
  if (allocuvm(ksm_sgtable[handle].pgdir, 0, size) == 0) {
    kfree((char*)ksm_sgtable[handle].pgdir);
    ksm_sgtable[handle].pgdir = 0;
    release(&ksmlock);
    return ERR_MEMORY_FULL;
  }

  // Set the get time for this segment for this process
  set_time(&proc->ksm_mstable[handle].gettime);

  // Fill in the info
  ksm_sgtable[handle].id = key;
  ksm_sgtable[handle].ksmsz = size;
  ksm_sgtable[handle].cpid = ksm_sgtable[handle].mpid = proc->pid;
  ksm_globalinfo.total_shsg_num += 1;
  ksm_globalinfo.total_shpg_num += PGROUNDUP(size) / PGSIZE;

  release(&ksmlock);
  return handle + 1;
}

// Take handle, call allocuvm in the current process mapping the shared memory
// (allocated by ksmget) into the prosess' page directory. Write the necessary
// data into the info.
// NOTE: we prevent attaching the segments the process has not (ksm)got.
int ksmattach(int hd, int flag) {
  --hd;  // convert the handle into a proper index

  if (hd < 0 || hd >= KSM_SEG_MAX_NUM)
    return ERR_BAD_HANDLE;

  // Disallow multiple attachments of the same segment
  // Return the address of already attached segment
  if (proc->ksm_mstable[hd].bottom != 0)
    return (int)proc->ksm_mstable[hd].bottom;

  acquire(&ksmlock);
  // Check if the segment actually doesn't exist
  if (ksm_sgtable[hd].id == 0) {
    release(&ksmlock);
    return ERR_SEG_NOT_CREATED;
  }

  // Check if the process is eligible to attach the requested segment
  if (proc->ksm_mstable[hd].gettime <= ksm_sgtable[hd].deltime) {
    release(&ksmlock);
    return ERR_SEG_NOT_ELIGIBLE;
  }

  // Update attached_num to prevent segment deletion while attachment
  ++ksm_sgtable[hd].attached_num;
  release(&ksmlock);
  
  // Find a suitable free segment in user virtual memory.
  uint seg_pgnum = PGROUNDUP(ksm_sgtable[hd].ksmsz) / PGSIZE;
  char* free_ksmseg;
  if (!(free_ksmseg = find_first_freeseg(seg_pgnum))) {
    acquire(&ksmlock);
    --ksm_sgtable[hd].attached_num;
    release(&ksmlock);
    return ERR_USER_MEMORY_FULL;
  }
  update_ksmbitmap(free_ksmseg, seg_pgnum, 0);
  proc->ksm_mstable[hd].bottom = free_ksmseg;
  proc->ksm_mstable[hd].pgnum = seg_pgnum;

  acquire(&ksmlock);
  // Put the ksm_pgdir into the proc->pgdir
  copy_pgdir(proc->pgdir, ksm_sgtable[hd].pgdir, free_ksmseg, 0,
             ksm_sgtable[hd].ksmsz, flag);

  // Update all the rest segment info
  ksm_sgtable[hd].mpid = proc->pid;
  set_time(&ksm_sgtable[hd].atime);
  release(&ksmlock);
  return (int)free_ksmseg;
}

int ksmdetach(int hd) {
  --hd;  // convert handle into proper index
  if (hd < 0 || hd >= KSM_SEG_MAX_NUM)
    return ERR_BAD_HANDLE;

  // Check if the process has attached this segment before detaching it
  if (proc->ksm_mstable[hd].bottom == 0)
    return ERR_SEG_WRONG_DETACH;

  // Attached but not created is actually a bug, but this check be for debugging
  acquire(&ksmlock);
  if (ksm_sgtable[hd].id == 0) {
    release(&ksmlock);
    return ERR_SEG_NOT_CREATED;
  }
  release(&ksmlock);

  // Updating the proc's pgdir, bitmap, and mstable
  clean_pgdir(proc->pgdir, proc->ksm_mstable[hd].bottom,
              proc->ksm_mstable[hd].pgnum*PGSIZE);
  update_ksmbitmap(proc->ksm_mstable[hd].bottom,
                   proc->ksm_mstable[hd].pgnum, 1);
  proc->ksm_mstable[hd].bottom = 0;
  proc->ksm_mstable[hd].pgnum = 0;
  
  acquire(&ksmlock);
  // Updating shared structures
  ksm_sgtable[hd].mpid = proc->pid;
  set_time(&ksm_sgtable[hd].dettime);
  --ksm_sgtable[hd].attached_num;

  // Check if the segment was assigned for deletion
  if (ksm_sgtable[hd].attached_num == 0 && ksm_sgtable[hd].id == KSM_TOBE_DEL)
    destroy_seg(hd);
  release(&ksmlock);

  return 0;
}

int ksmdelete(int hd) {
  --hd;  // convert the handle into a proper index
  if (hd < 0 || hd >= KSM_SEG_MAX_NUM)
    return ERR_BAD_HANDLE;

  acquire(&ksmlock);
  // Check if the requested segment exists
  if (ksm_sgtable[hd].id == 0) {
    release(&ksmlock);
    return ERR_SEG_NOT_CREATED;
  }

  // Check if the process is eligible to delete the requested segment
  if (proc->ksm_mstable[hd].gettime <= ksm_sgtable[hd].deltime) {
    release(&ksmlock);
    return ERR_SEG_NOT_ELIGIBLE;
  }

  // Mark segment to be deleted and check if there is any attaches
  ksm_sgtable[hd].id = KSM_TOBE_DEL;
  if (ksm_sgtable[hd].attached_num == 0)
    destroy_seg(hd);
  release(&ksmlock);

  return 0;
}

int ksminfo(int hd, struct ksminfo_t* info) {
  if (hd < 0 || hd > KSM_SEG_MAX_NUM)
    return ERR_BAD_HANDLE;

  --hd;  // convert handle into proper index

  acquire(&ksmlock);
  if (ksm_sgtable[hd].id == 0) {
    release(&ksmlock);
    return ERR_SEG_NOT_CREATED;
  }

  if (proc->ksm_mstable[hd].gettime <= ksm_sgtable[hd].deltime) {
    release(&ksmlock);
    return ERR_SEG_NOT_ELIGIBLE;
  }

  info->ksmsz          = (hd < 0) ? 0 : ksm_sgtable[hd].ksmsz;
  info->cpid           = (hd < 0) ? 0 : ksm_sgtable[hd].cpid;
  info->mpid           = (hd < 0) ? 0 : ksm_sgtable[hd].mpid;
  info->attached_num   = (hd < 0) ? 0 : ksm_sgtable[hd].attached_num;
  info->gtime          = (hd < 0) ? 0 : proc->ksm_mstable[hd].gettime;
  info->atime          = (hd < 0) ? 0 : ksm_sgtable[hd].atime;
  info->dettime        = (hd < 0) ? 0 : ksm_sgtable[hd].dettime;
  info->deltime        = (hd < 0) ? 0 : ksm_sgtable[hd].deltime;
  info->total_shsg_num = ksm_globalinfo.total_shsg_num;
  info->total_shpg_num = ksm_globalinfo.total_shpg_num;

  release(&ksmlock);

  return 0;
}
