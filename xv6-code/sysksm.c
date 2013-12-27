#include "types.h"
#include "defs.h"
#include "ksm.h"

int
sys_ksmget(void) {
  int key;
  int size;

  if (argint(0, &key) < 0 || argint(1, &size) < 0)
    return -1;

  return ksmget(key, (uint)size);
}

int
sys_ksmattach(void)
{
  int hd;
  int flag;

  if (argint(0, &hd) < 0 || argint(1, &flag) < 0)
    return -1;

  return ksmattach(hd, flag);
}

int
sys_ksmdetach(void)
{
  int hd;

  if (argint(0, &hd) < 0)
    return -1;

  return ksmdetach(hd);
}

int
sys_ksmdelete(void)
{
  int hd;

  if (argint(0, &hd) < 0)
    return -1;

  return ksmdelete(hd);
}

int
sys_ksminfo(void)
{
  int hd;
  struct ksminfo_t* info;

  if (argint(0, &hd) < 0 || argptr(1, (void*)&info, sizeof(*info)) < 0)
    return -1;
  return ksminfo(hd, info);
}
