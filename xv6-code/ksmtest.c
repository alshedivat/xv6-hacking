#include "types.h"
#include "user.h"
#include "ksm.h"

enum keys {
  bad_key   = 0,
  good_key  = 10,
  good_key1 = 20,
};

enum handles {
  bad_handle   = 0,
  good_handle  = 10,
  good_handle1 = 2,
};

enum sizes {
  zero_size      = 0,
  too_large_size = 2*1024*1024 + 1,
  normal_size    = 1*1024*1024,
};

void printksm(int hd) {
  struct ksminfo_t ksminf;
  printf(1, "-------------------------\n");
  printf(1, "KSMINFO: with handle = %d\n", hd);
  printf(1, "-------------------------\n");

  if (ksminfo(hd, &ksminf) == 0) {
    printf(1, "cpid           = %d\n", ksminf.cpid);
    printf(1, "mpid           = %d\n", ksminf.mpid);
    printf(1, "ksmsz          = %d\n", ksminf.ksmsz);
    printf(1, "attached_num   = %d\n", ksminf.attached_num);
    printf(1, "gtime          = %d\n", ksminf.gtime);
    printf(1, "atime          = %d\n", ksminf.atime);
    printf(1, "dettime        = %d\n", ksminf.dettime);
    printf(1, "deltime        = %d\n", ksminf.deltime);
    printf(1, "total_shrg_num = %d\n", ksminf.total_shsg_num);
    printf(1, "total_shpg_num = %d\n", ksminf.total_shpg_num);
  } else {
    printf(1, "No segment with such a handle!\n");
  }
  printf(1, "-------------------------\n");
}

static void test_ksm_get(void) {
  printf(1, "========================\n");
  printf(1, "Testing KSMGET          \n");
  printf(1, "========================\n");

  int hd, hd1, hd2;

  // Try getting with bad key
  hd = ksmget(bad_key, normal_size);
  if (hd == ERR_BAD_KEY)
    printf(1, "SUCCESS: returned proper error code for a bad key\n");
  else
    printf(1, "FAILURE: returned %d for a bad key\n", hd);

  // Try getting with too large size
  hd = ksmget(good_key, too_large_size);
  if (hd == ERR_SEG_WRONG_SIZE)
    printf(1, "SUCCESS: returned proper error code for too large size\n");
  else
    printf(1, "FAILURE: returned %d for too large size\n", hd);

  // Try getting with zero size before segment was created (by somebody)
  hd = ksmget(good_key, zero_size);
  if (hd == ERR_SEG_NOT_CREATED)
    printf(1, "SUCCESS: returned proper error code for zero size "
              "since the segment has not been created yet\n");
  else
    printf(1, "FAILURE: returned %d for zero size and not created segment\n", hd);

  // Try getting with good key and with normal size
  hd = ksmget(good_key, normal_size);
  if (hd > 0)
    printf(1, "SUCCESS: returned %d which is a proper handler\n", hd);
  else
    printf(1, "FAILURE: returned %d which is not a proper handler\n", hd);

  // Try getting with the same key, but different size
  hd = ksmget(good_key, normal_size + 1);
  if (hd == ERR_SEG_KEY_TAKEN)
    printf(1, "SUCCESS: returned proper error code for already taken key\n");
  else
    printf(1, "FAILURE: returned %d for already taken key\n", hd);

  // Try getting twice with the same key and compare the handles returned
  hd1 = ksmget(good_key, normal_size);
  hd2 = ksmget(good_key, zero_size);
  if (hd1 == hd2)
    printf(1, "SUCCESS: handles for the same keys are equal\n");
  else
    printf(1, "FAILURE: handles for the same keys are (%d,%d)\n", hd1, hd2);

// Try exhaust the limit of possible shared segments and call ksmget once more
// NOTE: this test was passed and now is disabled not to eat up all the segments
//
//  int name;
//  for (name = 1; name <= KSM_SEG_MAX_NUM + 1; ++name)
//    hd = ksmget(name, normal_size);
//  hd = ksmget(100, normal_size);
//  if (hd == ERR_SEG_NOT_AVAILABLE)
//    printf(1, "SUCCESS: returned proper error code for too many segments\n");
//  else
//    printf(1, "FAILURE: returned %d for to many segments created\n", hd);
}

void test_ksm_attach(void) {
  printf(1, "========================\n");
  printf(1, "Testing KSMATTACH       \n");
  printf(1, "========================\n");

  int hd;
  int ksm_addr;

  // Try attaching with a bad handle
  ksm_addr = ksmattach(bad_handle, 1);
  if (ksm_addr == ERR_BAD_HANDLE)
    printf(1, "SUCCESS: returned proper error code for a bad handle attach\n");
  else
    printf(1, "FAILURE: returned %d for a bad handle attach\n", ksm_addr);

  // Try attaching with good handle before segment created
  ksm_addr = ksmattach(good_handle, 1);
  if (ksm_addr == ERR_SEG_NOT_CREATED)
    printf(1, "SUCCESS: returned proper error code for a good segment attach "
              "before the segment is created\n");
  else
    printf(1, "FAILURE: returned %d for an attach before creation\n", ksm_addr);

  // Try attaching with good handle before getting
  if (fork() == 0) {
    ksmget(good_key1, normal_size);
    exit();
  }
  wait();
  ksm_addr = ksmattach(good_handle1, 1);
  if (ksm_addr == ERR_SEG_NOT_ELIGIBLE)
    printf(1, "SUCCESS: returned proper error code for a not eligible "
              "segment attach\n");
  else {
    printf(1, "FAILURE: returned %p for an attach before get\n", ksm_addr);
    printksm(good_handle1);
  }

  // Try attaching after getting a handle
  hd = ksmget(good_key, normal_size);
  ksm_addr = ksmattach(hd, 1);
  if (ksm_addr > 0)
    printf(1, "SUCCESS: handle %d returns %p\n", hd, (char*)ksm_addr);
  else {
    printf(1, "FAILURE: handle %d returns %d\n", hd, ksm_addr);
    printksm(hd);
  }
}

void test_ksm_detach(void) {
  printf(1, "========================\n");
  printf(1, "Testing KSMDETACH       \n");
  printf(1, "========================\n");

  int hd, ksm_addr, ksm_addr1;
  int det_code;

  // Try detach with a bad handle
  det_code = ksmdetach(bad_handle);
  if (det_code == ERR_BAD_HANDLE)
    printf(1, "SUCCESS: returned proper error code for a bad handle detach\n");
  else
    printf(1, "FAILURE: returned %d for a bad handle detach\n", det_code);

  // Try detach with a good handle but the segment we didn't attach
  det_code = ksmdetach(good_handle1);
  if (det_code == ERR_SEG_WRONG_DETACH)
    printf(1, "SUCCESS: returned proper error code for detach before attach\n");
  else
    printf(1, "FAILURE: returned %p for a detach before attach\n", det_code);
  printksm(good_handle1);

  // Try detach with a good handle and the segment that was attached before
  hd = ksmget(good_key, normal_size);
  ksm_addr = ksmattach(hd, 1);
  printksm(hd);
  det_code = ksmdetach(hd);
  if (det_code == 0)
    printf(1, "SUCCESS: dettached segment with handle %d\n", hd);
  else
    printf(1, "FAILURE: detach with handle %d failed with error code %d",
           hd, det_code);
  printksm(hd);

  // Try attach the same segment once again and see if the address is the same
  ksm_addr1 = ksmattach(hd, 1);
  if (ksm_addr == ksm_addr1)
    printf(1, "SUCCESS: address after detach-attach doesn't chage: (%p,%p)\n",
           ksm_addr, ksm_addr1);
  else
    printf(1, "FAILURE: address changed after detach-attach: (%p,%p)",
           ksm_addr, ksm_addr1);
  printksm(hd);
}

void test_ksm_delete(void) {
  printf(1, "========================\n");
  printf(1, "Testing KSMDELETE       \n");
  printf(1, "========================\n");

  int hd;
//  int ksm_addr, ksm_addr1;
  int det_code, del_code;

  // Try delete with a bad handle
  del_code = ksmdelete(bad_handle);
  if (del_code == ERR_BAD_HANDLE)
    printf(1, "SUCCESS: returned proper error code for a bad handle delete\n");
  else
    printf(1, "FAILURE: returned %d for a bad handle delete\n", del_code);

  // Try delete with not created segment
  del_code = ksmdelete(good_handle);
  if (del_code == ERR_SEG_NOT_CREATED)
    printf(1, "SUCCESS: returned proper error code for deletion"
              "not created segment\n");
  else
    printf(1, "FAILURE: returned %d for deletion not created segment\n", del_code);

  // Try delete with a good handle but the segment we didn't get
  del_code = ksmdelete(good_handle1);
  if (del_code == ERR_SEG_NOT_ELIGIBLE)
    printf(1, "SUCCESS: returned proper error code for delete before get\n");
  else
    printf(1, "FAILURE: returned %p for a delete before get\n", del_code);
  printksm(good_handle1);

  // Perform deletion on detach
  hd = ksmget(good_key, normal_size);
  del_code = ksmdelete(hd);
  det_code = ksmdetach(hd);
  if (del_code == 0 && det_code == 0)
    printf(1, "SUCCESS: delete and detach performed as it is expected\n");
  else
    printf(1, "FAILURE: delete and detached performed with an error. "
              "Delete code: %d, detach code: %d\n", del_code, det_code);
  printksm(hd);
  
  // Perform deletion before attaching
  hd = ksmget(good_key1, normal_size);
  printksm(hd);
  del_code = ksmdelete(hd);
  if (del_code == 0)
    printf(1, "SUCCESS: delete before attach performed as it is expected\n");
  else
    printf(1, "FAILURE: delete performed with an error; delete code: %d\n", del_code);
  printksm(0);
}

void test_ksm_fork_exit(void) {
  printf(1, "========================\n");
  printf(1, "Testing KSM on fork/exit\n");
  printf(1, "========================\n");

  int hd;
  int ksm_addr;
  int pid;
  
  // Fork write to ksm segment in child, read in parent, test memory
  if ((pid = fork()) != 0) {
    // In parent
    hd = ksmget(good_key, normal_size);
    ksm_addr = ksmattach(hd, 0);
  } else {
    // In child
    hd = ksmget(good_key, normal_size);
    ksm_addr = ksmattach(hd, 1);
    if (ksm_addr > 0)
      printf(1, "Child attached segment with handle %d to address %p\n", hd, ksm_addr);
    else
      printf(1, "Child failed to attach segment: handle %d, adderess %d\n", hd, ksm_addr);
    printksm(hd);

    char* greeting = "Hello KSM!";
    strcpy((char*)ksm_addr, greeting);
    exit();
  }
  wait();
  printf(1, "Shared memory segment contains: '%s'\n", ksm_addr);
  printksm(hd);
}

int main(int argc, char* argv[]) {
  printf(1, "Number of allocated pages before the tests: %d\n", pgused());
  test_ksm_get();
  test_ksm_attach();
  test_ksm_detach();
  test_ksm_delete();
  test_ksm_fork_exit();

  printf(1, "==================  \n");
  printf(1, "Number of allocated pages after the tests: %d\n", pgused());

  exit();
}
