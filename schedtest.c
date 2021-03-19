#include "types.h"
#include "user.h"
#include "pstat.h"
#include "stddef.h"

int main(int argc, char **argv) {
	if (argc != 6) {
		printf(2, "schedtest: incorrect arg format\n");
		exit();
	}

  //struct pstat *pst = 0;
  int sliceA = atoi(argv[1]); 			// 2
  char* sleepA = argv[2]; 			// 3
  int sliceB = atoi(argv[3]); 			// 5
  char* sleepB = argv[4]; 			// 5
  int sleepParent = atoi(argv[5]); 	// 100
  
	int pid;
	int pidA = -1;
	int pidB = -1;
	int compticksA = -1;
	int compticksB = -1;

  if ((pid = fork2(sliceA)) == 0) {
    // ChildA
    //(char *)sleepA
    char *args[] = {"loop", sleepA, NULL};
    exec(args[0], args);
    printf(1, "exec fail\n");
  }
	pidA = pid;
	//printf(1, "timesliceA: %d\n", getslice(pidA));
  //printf(1, "%d ", pid);

  if ((pid = fork2(sliceB)) == 0) {
    // ChildB
    char *args[] = {"loop", sleepB, NULL};
    exec(args[0], args);
    printf(1, "exec fail\n");
  }
	pidB = pid;
  //printf(1, "%d \n", pid);
	//printf(1, "timesliceb: %d\n", getslice(pidB));

  // parent sleeps here
  sleep(sleepParent);
	struct pstat *pst = (struct pstat*)malloc(sizeof(struct pstat));

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

	printf(1, "%d %d\n", compticksA + 1, compticksB + 1);

  wait();
  wait();

	printf(1, "still working \n");

  exit();
  return 0;
}
