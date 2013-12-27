//#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "mmu.h"
#include "ksm.h"

void printksm(int id) {

  struct ksminfo_t ksminf;
  ksminfo(id, &ksminf);
  printf(1, "-------------------------\n");
  printf(1, "KSMINFO: with handle = %d\n", id);
  printf(1, "-------------------------\n");
  printf(1, "cpid          = %d\n", ksminf.cpid);
  printf(1, "mpid          = %d\n", ksminf.mpid);
  printf(1, "ksmsz         = %d\n", ksminf.ksmsz);
  printf(1, "attached_nr   = %d\n", ksminf.attached_num);
  printf(1, "atime         = %d\n", ksminf.atime);
  printf(1, "dettime       = %d\n", ksminf.dettime);
  printf(1, "deltime       = %d\n", ksminf.deltime);
  printf(1, "total_shrg_nr = %d\n", ksminf.total_shsg_num);
  printf(1, "total_shpg_nr = %d\n", ksminf.total_shpg_num);
  printf(1, "-------------------------\n");
}

int main(int argc, char *argv[]) {

  int r;
  void* p;

  printf(1, "ksmtest starting\n");
  printf(1, "PGUSED: return = %d\n", pgused());

  printf(1, "==================\n");
  printf(1, "Testing KSMGET    \n");
  printf(1, "==================\n");

  int largeksm;
  largeksm = ksmget(1, 3100000); //more than 1MB
  if(largeksm == ERR_SEG_WRONG_SIZE) {
    printf(1, "SUCCESS: 2MB size limit check\n");
  } else {
    printf(1, "FAILURE: 2MB size limit check\n");
  }

  largeksm = ksmget(2, 2147483640); //slightly less than 2GB
  if(largeksm == ERR_SEG_WRONG_SIZE) {
    printf(1, "SUCCESS: start VA must be > process size\n");
  } else {
    printf(1, "FAILURE: start VA must be > process size\n");
  }

  int myksmid;
  myksmid = ksmget(3, 1000000);
  if(myksmid > 0) {
    printf(1, "SUCCESS: returns handle = %d\n", myksmid);
  } else {
    printf(1, "FAILURE: returns handle = %d\n", myksmid);
  }

  printksm(myksmid);

  int myksmid2;
  myksmid2 = ksmget(3, 1000000);
  if(myksmid == myksmid2) {
    printf(1, "SUCCESS: returns same handle for same name (%d,%d)\n", myksmid, myksmid2);
  } else {
    printf(1, "FAILURE: returns handle for same name (%d,%d)\n", myksmid, myksmid2);
  }

  printksm(myksmid2);

  int diff;
  diff = ksmget(4, 500);
  if(diff != myksmid) {
    printf(1, "SUCCESS: returns unique handle = %d\n", diff);
  } else {
    printf(1, "FAILURE: returns unique handle = %d\n", diff);
  }

  printksm(diff);

  printf(1, "==================\n");
  printf(1, "Testing KSMATTACH \n");
  printf(1, "==================\n");
  r = ksmattach(myksmid, 1); // rw:1, r:0
  if(r > 0) {
    printf(1, "SUCCESS: handle %d returns %p\n", myksmid, (void*) r);
  } else {
    printf(1, "FAILURE: handle %d returns %d\n", myksmid, r);
  }
  printksm(myksmid);

  p = (void*) ksmattach(diff, 1); // rw:1, r:0
  if(p > 0) {
    printf(1, "SUCCESS: handle %d returns %p\n", diff, p);
  } else {
    printf(1, "FAILURE: handle %d returns %p\n", diff, p);
  }
  printksm(diff);

  printf(1, "==================\n");
  printf(1, "Testing KSMDELETE \n");
  printf(1, "==================\n");
  r = ksmdelete(myksmid);
  if(r == 0) {
    printf(1, "SUCCESS: handle %d returns %d\n", myksmid, r);
  } else {
    printf(1, "FAILURE: handle %d returns %d\n", myksmid, r);
  }
  printksm(myksmid);

  printf(1, "==================\n");
  printf(1, "Testing KSMDETACH \n");
  printf(1, "==================\n");
  printf(1, "PGUSED: return = %d\n", pgused());
  r = ksmdetach(myksmid);
  if(r == 0) {
    printf(1, "SUCCESS: handle %d returns %d\n", myksmid, r);
  } else {
    printf(1, "FAILURE: handle %d returns %d\n", myksmid, r);
  }
  printksm(myksmid);
  printf(1, "PGUSED: return = %d\n", pgused());

  printf(1, "================== \n");
  printf(1, "Testing FORK & EXIT\n");
  printf(1, "================== \n");
  printksm(diff);
  int pid;
  printf(1, "parent (pid = %d) will now fork\n", getpid());
  pid = fork();
  if(pid != 0) { //parent
    printksm(diff);
    printf(1, "parent (pid = %d) will now fork again\n", getpid());
    pid = fork();
    if(pid != 0) {
      printksm(diff);
      printf(1, "parent (pid = %d) will now wait\n", getpid());
      wait();
      printksm(diff);
      printf(1, "parent (pid = %d) will now wait again\n", getpid());
      wait();
      printksm(diff);
    }
  }
  exit();
}
