#include "types.h"
#include "user.h"
#include "ksm.h"
#include "sem.h"

enum SEMAPHORE_NAMES {
  semFullName = 1,
  semEmptyName,
  semMutexName,
};

enum KSM_SEG_KEYS {
  good_key = 1,
};

int main(int argc, char* argv[]) {
  const uint kIterationsNum = 20;
  const uint kBufferSize = 10;

  int semFull  = sem_get(semFullName,0);
  int semEmpty = sem_get(semEmptyName,kBufferSize);
  int semMutex = sem_get(semMutexName,1);

  int hd, ret_code;

  // Get some shared memory
  if (!(hd = ksmget(good_key, kBufferSize))) {
    printf(1,"ERROR: ksmget with key %d and size %d failed; it returned %d",
           good_key, kBufferSize, hd);
    exit();
  }
  if((ret_code = ksmattach(hd,1)) <= 0) {
    printf(1,"ERROR: ksmattach with handle %d failed; it returned %d",
           hd, ret_code);
    exit();
  }

  // Fill the shared buffer
  char* buffer = (char*)ret_code;
  int i;
  for(i = 0; i < kBufferSize; i++)
    buffer[i] = 'E';

  // Start producer/consumer simulation: 4 producers and 4 consumers
  int j;
  int pid = fork(); pid = fork(); pid = fork();

  // Producer
  if(pid != 0) {
    for(j = 0; j < kIterationsNum; ++j) {
      sem_wait(semEmpty);
      sem_wait(semMutex);
      printf(1, "\nProducer (PID %d)\n", getpid());
      for(i = 0; i < kBufferSize; ++i)
        if (buffer[i] == 'E') {
          buffer[i] = 'F';
          break;
        }
      for(i = 0; i < kBufferSize; ++i)
        printf(1, "%c ", buffer[i]);
      printf(1, "\n");
      sem_signal(semMutex);
      sem_signal(semFull);
    }

  // Consumer
  } else {
    for(j = 0; j < kIterationsNum; ++j) {
      sem_wait(semFull);
      sem_wait(semMutex);
      printf(1, "\nConsumer (PID %d)\n", getpid());
      for(i = 0; i < kBufferSize; ++i)
        if (buffer[i] == 'F') {
          buffer[i] = 'E';
          break;
        }
      for(i = 0; i < kBufferSize; ++i)
        printf(1, "%c ", buffer[i]);
      printf(1, "\n");
      sem_signal(semMutex);
      sem_signal(semEmpty);
    }
  }
  
  // Be a nice parent and wait for your children
  wait();
  wait();
  wait();
  exit();
}

