#include "types.h"
#include "user.h"

int main(int argc, char **argv) {
  int sliceA = atoi(argv[1]);
  int sleepA = atoi(argv[2]);
  int sliceB = atoi(argv[3]);
  int sleepB = atoi(argv[4]);
  int sleepParent = atoi(argv[5]);
  int pid;

  if ((pid = fork2(sliceA)) == 0) {
    // this is ChildA
    // TODO give it a fucking timeslice of sliceA
    char *args[] = {"loop", (char *)sleepA, 0};
    exec(args[0], args);
    printf(1, "exec fail\n");
  }
  printf(1, "running process: %d\n", pid);

  if ((pid = fork2(sliceB)) == 0) {
    // this is the goddamn ChildB
    // TODO give it a retarded sliceB
    char *args[] = {"loop", (char *)sleepB, 0};
    exec(args[0], args);
    printf(1, "exec fail\n");
  }
  printf(1, "running process: %d\n", pid);

  // parent sleeps here
  sleep(sleepParent);

  // calls getpinfo
  // TODO do that

  wait();
  wait();

  exit();
  return 0;
}
