#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "defs.h"
#include "sem.h"

struct sem_t {
  uint name;
  int value;
  uint deltime;
};

struct sem_t sem_table[MAXSEMNUM];
struct spinlock semlock;

static void set_time(uint* time) {
  acquire(&tickslock);
  *time = ticks;
  release(&tickslock);
}

void sem_init(void) {
  initlock(&semlock, "sem");
  memset(sem_table, 0, sizeof(sem_table));
}

int sem_get(uint name, int value) {
  if (name == 0)
    return ERR_BAD_SEM_NAME;
  if (value < 0 || value > MAX_SEM_VAL)
    return ERR_BAD_SEM_VAL;

  acquire(&semlock);
  int handle = -1;
  int i;
  for (i = 0; i < MAXSEMNUM; ++i) {
    if (handle < 0 && sem_table[i].name == 0)
      handle = i;
    if (name == sem_table[i].name) {
      set_time(&proc->sem_gettimes[i]);
      release(&semlock);
      return i + 1;
    }
  }
  if (handle < 0) {
    release(&semlock);
    return ERR_OUT_OF_SEM;
  }

  sem_table[handle].name  = name;
  sem_table[handle].value = value;
  set_time(&proc->sem_gettimes[handle]);

  release(&semlock);
  return handle + 1;
}

int sem_delete(int handle) {
  --handle;  // convert handle in a proper index
  if (handle < 0 || handle >= MAXSEMNUM)
    return ERR_SEM_BAD_HANDLE;

  acquire(&semlock);
  if (sem_table[handle].name == 0) {
    release(&semlock);
    return ERR_SEM_DOES_NOT_EXIST;
  }

  if (proc->sem_gettimes[handle] <= sem_table[handle].deltime) {
    release(&semlock);
    return ERR_SEM_NOT_ELLIGIBLE;
  }

  int sem_old_val = sem_table[handle].value;
  sem_table[handle] = (struct sem_t){0};

  // Set the deletion time
  set_time(&sem_table[handle].deltime);

  // Others might be waiting for this semaphore.
  // We need to wake them up!
  if (sem_old_val == 0) {
    wakeup(&sem_table[handle]);
    release(&semlock);
    return SEM_OK;
  }

  release(&semlock);
  return SEM_OK;
}

int sem_signal(int handle) {
  --handle;  // convert handle in a proper index
  if (handle < 0 || handle >= MAXSEMNUM)
    return ERR_SEM_BAD_HANDLE;

  acquire(&semlock);
  if (sem_table[handle].name == 0) {
    release(&semlock);
    return ERR_SEM_DOES_NOT_EXIST;
  }

  if (proc->sem_gettimes[handle] <= sem_table[handle].deltime) {
    release(&semlock);
    return ERR_SEM_NOT_ELLIGIBLE;
  }

  ++sem_table[handle].value;
  wakeup(&sem_table[handle]);
  
  release(&semlock);
  return SEM_OK;
}

int sem_wait(int handle) {
  --handle;  // convert handle in a proper index
  if (handle < 0 || handle >= MAXSEMNUM)
    return ERR_SEM_BAD_HANDLE;

  acquire(&semlock);
  if (sem_table[handle].name == 0) {
    release(&semlock);
    return ERR_SEM_DOES_NOT_EXIST;
  }

  if (proc->sem_gettimes[handle] <= sem_table[handle].deltime) {
    release(&semlock);
    return ERR_SEM_NOT_ELLIGIBLE;
  }

  while (sem_table[handle].value == 0) {
    // Somebody deleted the semaphore, and probably
    // somebody other created new with this handle)
    if (sem_table[handle].name == 0 ||
        proc->sem_gettimes[handle] <= sem_table[handle].deltime) {
      release(&semlock);
      return ERR_SEM_DOES_NOT_EXIST;
    }
    sleep(&sem_table[handle], &semlock);
  }

  // Decrease the value of the semaphore
  --sem_table[handle].value;

  release(&semlock);
  return SEM_OK;
}
