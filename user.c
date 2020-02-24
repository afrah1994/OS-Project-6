/*
 * child.c
 *
 *  Created on: Oct 1, 2019
 *      Author: afrah
 */
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <time.h>
# include <stdlib.h>
# include <dirent.h>
# include <stdio.h>
# include <string.h>
# include <getopt.h>
# include <stdbool.h>
# include <ctype.h>
# include <sys/wait.h>
# include <signal.h>
# include <sys/mman.h>
# include <sys/time.h>
# include <stdint.h>
# include <fcntl.h>
# include <sys/shm.h>
# include "shmemsem.h"
# include <sys/ipc.h>
# include <semaphore.h>
# include <stdbool.h>
# include <pthread.h>

SharedData *shmem;
int shmid;
int processnum=-1;
int million=1000000;
int main(int argc, char **argv)
{
		srand(time(NULL) ^ (getpid()<<16));

		//printf("hello from user %d\n",getpid());
		int sum,i;
		int resources[10]={0},resourcecounter=-1;

		int locals; int localn, checktermination,B=300;
		int numofrequests=0;
		// Create our key
		key_t shmkey = ftok("makefile",777);
		if (shmkey == -1) {
			perror("user:Ftok failed");
			exit(-1);
		}

		// Get our shm id
		shmid = shmget(shmkey, sizeof(SharedData), 0600 | IPC_CREAT);
		if (shmid == -1) {
			perror("user:Failed to allocate shared memory region");
			exit(-1);
		}

		// Attach to our shared memory
		shmem = (SharedData*)shmat(shmid, (void *)0, 0);
		if (shmem == (void *)-1) {
			perror("user:Failed to attach to shared memory region");
			exit(-1);
		}
		shmem->pidcounter++;
		int pid=getpid();
		//getting the index for the processes in the matrices
		int createdn=shmem->nanoseconds;
		locals=shmem->seconds;
		localn=shmem->nanoseconds;
		bool pnumdone=false;
		sem_wait(&(shmem->osssem));
		for(i=0;i<18;i++)
		{
			if(shmem->pcb[i][0]==pid)
			{
				processnum=i;
				pnumdone=true;
				break;
			}
		}
		bool Iamalive=true,maxdone=false,sharedmsgdone=false;
		sem_post(&(shmem->osssem));

		bool timereached=false;
		checktermination = 200;
		bool term=false;

		do
		{
			if(processnum>-1 && pnumdone==true)
			{
				sem_wait(&(shmem->mutexclock[processnum]));

				//printf("user %d has the semaphore\n",getpid());
				if(locals>=0 && localn>=70000)
				{
					term=true;
				}
				if((locals==shmem->seconds && ((shmem->nanoseconds-localn) >= B)) || shmem->seconds>locals)
				{
					timereached=true;
					if(shmem->sharedmsg[processnum][0]==0)
					{

						sharedmsgdone=true;
						int ct;
						if(locals==shmem->seconds)
						{
							ct=shmem->nanoseconds-createdn;
						}
						else
						{
							ct=10000;
						}
						if(term==true && ct > checktermination && numofrequests>=1)
						{
							FILE *fp;
							fp = fopen("log.dat", "a+");
						    if (fp == NULL)
						        {
						        	perror("oss: Unable to open the output file:");
						        }
						    else
						    {
						    	fprintf(fp,"reached term \n");
						    }
						    fclose(fp);
							int myend=rand()%3;
							if(myend==0)
							{
								shmem->sharedmsg[processnum][0]=getpid();
								shmem->sharedmsg[processnum][1]=-1;
								shmem->sharedmsg[processnum][2]=processnum;
								Iamalive=false;
								sem_post(&(shmem->mutexclock[processnum]));
								shmdt(shmem);
								exit(0);
							}
						}
						int request=(rand()%32);
						int readorwrite=rand()%2;
						shmem->sharedmsg[processnum][0]=getpid();
						shmem->sharedmsg[processnum][1]=request;
						shmem->sharedmsg[processnum][2]=processnum;
						shmem->sharedmsg[processnum][3]=readorwrite;

								FILE *fp;
								fp = fopen("log.dat", "a+");
							    if (fp == NULL)
							        {
							        	perror("oss: Unable to open the output file:");
							        }
							    else
							    {
							    	//fprintf(fp,"from user %d requesting %d \n",getpid(),request);
							    }
							    fclose(fp);
					}//end if shared msg=0
					else
					{
						FILE *fp;
						fp = fopen("log.dat", "a+");
					    if (fp == NULL)
					        {
					        	perror("oss: Unable to open the output file:");
					        }
					    else
					    {
					    	//fprintf(fp,"shared msg not = 0 \n");
					    }
					    fclose(fp);
					}

				}//endif time reached
				else
				{
					FILE *fp;
					fp = fopen("log.dat", "a+");
				    if (fp == NULL)
				        {
				        	perror("oss: Unable to open the output file:");
				        }
				    else
				    {
				    	//fprintf(fp,"did not reach time \n");
				    }
				    fclose(fp);
				}
				//we will update the local time only if it had a chance to go request/release
				//if we update anyway then everytime the user checked its time its local would be always be updated and this way the threshold B would never be met
				if(timereached==true&&sharedmsgdone==true)
				{
					locals=shmem->seconds;
					localn=shmem->nanoseconds;
					timereached=false;
					sharedmsgdone=false;
				}
				sem_post(&(shmem->mutexclock[processnum]));
				sum = 0;
				for ( i = 0; i < million; i++)
				sum += i;
			}
		}while(Iamalive);
		exit(0);

}


