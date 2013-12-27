#include "types.h"
#include "defs.h"
#include "sem.h"

int
sys_sem_get(void) {
  int name;
  int value;

  if (argint(0, &name) < 0 || argint(1, &value) < 0)
    return -1;

  return sem_get((uint)name, value);
}

int
sys_sem_delete(void)
{
  int hd;

  if (argint(0, &hd) < 0)
    return -1;

  return sem_delete(hd);
}

int
sys_sem_signal(void)
{
  int hd;

  if (argint(0, &hd) < 0)
    return -1;

  return sem_signal(hd);
}

int
sys_sem_wait(void)
{
  int hd;

  if (argint(0, &hd) < 0)
    return -1;

  return sem_wait(hd);
}
