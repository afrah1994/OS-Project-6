#ifndef SHMEMSEM_H
#include <semaphore.h>

typedef struct {
	int seconds;
	int nanoseconds;
	int sharedmsg[18][4];
	int pcb[18][4];
	sem_t mutexclock[18];
	sem_t osssem;
	int pidcounter;
	int numofiterations;
	int maxclaims[18][10];
	int allocation[18][10];
	int left[18][10];
	int available[10];
	int processnum[18];
	int processnumcounter;

	int P0[32];
	int P1[32];
	int P2[32];
	int P3[32];
	int P4[32];
	int P5[32];
	int P6[32];
	int P7[32];
	int P8[32];
	int P9[32];
	int P10[32];
	int P11[32];
	int P12[32];
	int P13[32];
	int P14[32];
	int P15[32];
	int P16[32];
	int P17[32];
	int pagetable[18][32];
	int lpagetable[18][32];

	int blockedqueue[18][3];
	int blockedcounter;
	int allblocked;//set to one when there is only 1 free process in the system

} SharedData;

#define SHMEMSEM_H
#endif
/*r0,r1,r2 are shared resources and r3-r9 are non sharable.
 * PCB
[0]=PID
[1]=Which process (0-17) it is occupying
[2]=clock seconds
[3]=clock nanoseconds
//For the resources each row has the information of which process is holding that resource,
  18 possible processes for sharable resources
  and only 1 possible for non sharable resources

 sharedmsg
 [0]-carries PID
 [1]- 1 for request 2 for release
 [2]- resource number
 [3]- quantity

 blockedqueue:
 [0]-carries PID
 [1]-carried resource number that it is blocked on
 [2]-amount of resource it requested

 blockedcounter has the number of processes in blocked queue
*/
