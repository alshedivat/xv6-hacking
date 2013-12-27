// KSM info structure (to be returned on sys call ksminfo)
struct ksminfo_t {
  uint ksmsz;             // The size of the shared memory segment
  int cpid;               // PID of the creator
  int mpid;               // PID of the last modifier
  uint attached_num;      // Number of attached processes
  uint gtime;             // Last get time by the current process
  uint atime;             // Last attach time
  uint dettime;           // Last detach time
  uint deltime;           // Last delete time
  uint total_shsg_num;    // Total number of existing shared segments
  uint total_shpg_num;    // Total number of existing shared pages
};

// KSM constants
#define KSM_SEG_MAX_NUM         64
#define KSM_ID_MAXLEN           255
#define KSM_SEG_MAXSZ           2*1024*1024
//#define KSM_FREEBM_SZ           KSM_SEG_MAX_NUM*KSM_SEG_MAXSZ/(8*PGSIZE)
#define KSM_FREEBM_SZ           PGSIZE

// KSM flags
#define KSM_TOBE_DEL            -1

// KSM error codes
enum KSM_ERRORS {
  ERR_BAD_KEY = -10,
  ERR_BAD_HANDLE,
  ERR_MEMORY_FULL,
  ERR_USER_MEMORY_FULL,
  ERR_SEG_KEY_TAKEN,
  ERR_SEG_WRONG_SIZE,
  ERR_SEG_NOT_CREATED,
  ERR_SEG_WRONG_DETACH,
  ERR_SEG_NOT_ELIGIBLE,
  ERR_SEG_NOT_AVAILABLE,
};

