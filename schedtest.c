#include "types.h"
#include "user.h"
#include "pstat.h"

int main(int argc, char **argv) {
	struct pstat *pst = (struct pstat*)malloc(sizeof(struct pstat));
  int sliceA = atoi(argv[1]); 			// 2
  int sleepA = atoi(argv[2]); 			// 3
  int sliceB = atoi(argv[3]); 			// 5
  int sleepB = atoi(argv[4]); 			// 5
  int sleepParent = atoi(argv[5]); 	// 100
  int pid;
	int pidA = -1;
	int pidB = -1;
	int compticksA = -1;
	int compticksB = -1;

  if ((pid = fork2(sliceA)) == 0) {
    // ChildA
    char *args[] = {"loop", (char *)sleepA, 0};
    exec(args[0], args);
    printf(1, "exec fail\n");
  }
	pidA = pid;
  //printf(1, "%d ", pid);

  if ((pid = fork2(sliceB)) == 0) {
    // ChildB
    char *args[] = {"loop", (char *)sleepB, 0};
    exec(args[0], args);
    printf(1, "exec fail\n");
  }
	pidB = pid;
  //printf(1, "%d \n", pid);

  // parent sleeps here
  sleep(sleepParent);

  // calls getpinfo
  getpinfo(pst);
  
	// print the compticksA and compticksB
	//printf(1, "pidA: %d | pidB: %d\n", pidA, pidB);
	for (int i = 0; i < NPROC; i++) {
		if (pst->pid[i] == pidA) {
			//printf(1, "pidA: %d\n", pidA);
			compticksA = pst->compticks[i];
		} else if (pst->pid[i] == pidB) {
			//printf(1, "pidB: %d\n", pidB);
			compticksB = pst->compticks[i];
		}
	}

	printf(1, "%d %d\n", compticksA, compticksB);

  wait();
  wait();

  exit();
  return 0;
}
