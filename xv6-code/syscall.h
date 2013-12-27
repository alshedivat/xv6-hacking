// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21

// Kaust Shared Memory
#define SYS_ksmget     101
#define SYS_ksmattach  102
#define SYS_ksmdetach  103
#define SYS_ksmdelete  104
#define SYS_ksminfo    105
#define SYS_pgused     106

// Semaphores
#define SYS_sem_get    201
#define SYS_sem_delete 202
#define SYS_sem_signal 203
#define SYS_sem_wait   204
