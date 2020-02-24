/*
 ============================================================================
 Name        : simulator.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
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
# include <semaphore.h>
# include "shmemsem.h"
# include <stdbool.h>

//global variables
int billion=1000000000;
int emptypcbcounter;
int allocation[18][10],left[18][10],available[10];
int requestsgranted=0;
int requestsdenied=0;
int released=0;
int terminated=0;
pid_t alive[110];
int million=1000000;
SharedData *shmem;
int shmid;
FILE *fp;
int rann,rans;
char *logname="log.dat";//log file name for program -l
bool timeup=false;
bool filelines=true;
int mm[256][2];
int mmfilled=-1;
int lru[256][4];
int lrufilled=-1;
int dirtybit[256];
int ldirtybit[256];
int fspeed=0;
int lspeed=0;
int fpgfault=0;
int lpgfault=0;

bool pdone[18];
bool framesfull=false;

void cleanup();
void catch_alarm(int sig);
void killprocesses();
void updatePCB(int process);

void printmatrix();

void printpcb();
int emptyPCB();
void startprocess();
void checklines();

void checklines()
{
	FILE *fp;
    int count = 0;  // Line counter (result)

    char c;  // To store a character read from file


    // Open the file
    fp = fopen("log1.dat", "r");

    // Check if file exists
    if (fp == NULL)
    {
        printf("Could not open file ");

    }

    // Extract characters from file and store in character c
    for (c = getc(fp); c != EOF; c = getc(fp))
        if (c == '\n') // Increment count if this character is newline
            count = count + 1;
    if(count>=1000)
    	filelines=false;
    else
    	filelines= true;
}
void killprocesses()
{
	printf("\nkilling the following processes:\n");
	int i;
	for(i=0;i<18;i++)
	{
		if(shmem->pcb[i][0]>0)
		{
			printf("%d->%d\n",i,shmem->pcb[i][0]);
		}
	}
	printf("OSS %d\n",getpid());

	int corpse,status;
	for(i=0;i<18;i++)
	{
		if(shmem->pcb[i][0]>0)
		{
			int pid=shmem->pcb[i][0];
			kill(pid,SIGKILL);
			while ((corpse = waitpid(pid, &status, 0)) != pid && corpse != -1)
 		   {
 			char pmsg[64];
 			snprintf(pmsg, sizeof(pmsg), "oss: PID %d exited with status 0x%.4X", corpse, status);
 			perror(pmsg);
 		   }
		}
	}

}
void cleanup()
{
	int bc=shmem->blockedcounter+1;
	int pc=shmem->pidcounter+1;
	if(timeup==true)
	{
		printf("time is up due to alarm\n");
	}
	float pf=(float)fpgfault/shmem->numofiterations;
	float lf=(float) lpgfault/shmem->numofiterations;
	printf("Doing cleanup\n");
	//printf("Number of processes generated = %d\n",pc);
	printf("Number of page faults in FIFO: %d in LRU: %d\n",fpgfault,lpgfault);
	printf("Number of memory access requests granted by OSS: %d \n",shmem->numofiterations);
	printf("Number of page faults per memory access FIFO: %.2f LRU: %.2f\n",pf,lf);
	printf("Average speed of access for FIFO: %d in LRU: %d\n",fspeed,lspeed);



	printf("OSS Exiting at time %d:%d\n",shmem->seconds,shmem->nanoseconds);
	fp = fopen(logname, "a+");
    if (fp == NULL)
    {
    	perror("oss: Unable to open the output file:");
    }
    else
    {
    	fprintf(fp,"Number of processes generated=%d\n",pc);
    	fprintf(fp,"Number of page faults in FIFO: %d in LRU: %d\n",fpgfault,lpgfault);
    	fprintf(fp,"Average speed of access for FIFO: %d in LRU: %d\n",fspeed,lspeed);
    	fprintf(fp,"Number of page faults per memory access FIFO: %.2f LRU: %.2f\n",pf,lf);
    	//fprintf(fp,"Number of: requests granted by OSS=%d that terminated=%d \n",requestsgranted,requestsdenied,released,terminated,bc);
    	fprintf(fp,"Number of memory accesses granted by OSS: %d \nOSS Exiting at time %d:%d\n",shmem->numofiterations,shmem->seconds,shmem->nanoseconds);
    }
    fclose(fp);

	killprocesses();
	shmdt(shmem);// Free up shared memory
	shmctl(shmid, IPC_RMID,NULL);
	sleep(1);
	exit(0);
}
void updatePCB(int process)
{
	int i;
	for(i=0;i<18;i++)
	{
		if(shmem->pcb[i][0]==process)
		{
			shmem->pcb[i][0]=0;
			shmem->pcb[i][1]=0;
			shmem->pcb[i][2]=0;
			break;
		}
	}
}
//clearing matrix for process that has terminated
void clearprocess(int pid, int counter)
{
	int i;

	//clearing process from main memory
	for(i=0;i<256;i++)
	{
		if(mm[i][0]==pid)
		{
			mm[i][0]=0;
			mm[i][1]=0;
			dirtybit[i]=0;
		}
		if(lru[i][0]==pid)
		{
			lru[i][0]=0;
			lru[i][1]=0;
			lru[i][2]=0;
			lru[i][3]=0;
			ldirtybit[i]=0;
		}
	}

	for(i=0;i<32;i++)
	{
		shmem->pagetable[counter][i]=-1;
		shmem->pagetable[counter][i]=-1;
	}

}
void printmatrix()
{
	fp = fopen(logname, "a+");
	if (fp == NULL)
    {
	    perror("oss: Unable to open the output file:");
	}
    else
	{
	int i,j;
	printf("\nPagetable");
	fprintf(fp,"\nPagetable");
	for(i=0;i<18;i++)
	{
		if(shmem->pcb[i][0]>0)
		{
			fprintf(fp,"\n %d  ",shmem->pcb[i][0]);
			printf("\n %d  ",shmem->pcb[i][0]);
			printf("\nFIFO\n");
			fprintf(fp,"\nFIFO\n");
			for(j=0;j<32;j++)
			{
				//if(shmem->pagetable[i][j]>-1)
				{
					fprintf(fp,"%d-> %d, ",j,shmem->pagetable[i][j]);

					printf("%d-> %d, ",j,shmem->pagetable[i][j]);
				}
			}
			printf("\nLRU\n");
			fprintf(fp,"\nLRU\n");
			for(j=0;j<32;j++)
			{
				//if(shmem->lpagetable[i][j]>-1)
				{
					fprintf(fp,"%d-> %d, ",j,shmem->lpagetable[i][j]);

					printf("%d-> %d, ",j,shmem->lpagetable[i][j]);
				}
			}
		}
	}
	fprintf(fp,"\n               Main memory\n");
	    printf("\n                   Main memory\n");
	fprintf(fp,"      FIFO                               LRU\n");
	    printf("      FIFO                               LRU\n");
	    printf("     Dirty  pno  page   Dirty pno  page s   ns\n");
	fprintf(fp,"     Dirty  pno  page   Dirty pno  page s   ns\n");
	for(i=0;i<256;i++)
	{
		if(mm[i][0]>0)
		{
			if(i<10)
			{
				fprintf(fp,"  ");
				printf("  ");
			}
			else if(i>9 && i<100)
			{
				fprintf(fp," ");
				printf(" ");
			}
			if(mm[i][1]<10)
			{
				fprintf(fp,"%d-> %d    %d   %d         ",i,dirtybit[i],mm[i][0],mm[i][1]);
				    printf("%d-> %d    %d   %d         ",i,dirtybit[i],mm[i][0],mm[i][1]);
			}
			else
			{
				fprintf(fp,"%d-> %d    %d  %d         ",i,dirtybit[i],mm[i][0],mm[i][1]);
				    printf("%d-> %d    %d  %d         ",i,dirtybit[i],mm[i][0],mm[i][1]);
			}
			if(lru[i][1]<10)
			{
				    printf("%d  %d   %d  %d  %d\n",ldirtybit[i],lru[i][0],lru[i][1],lru[i][2],lru[i][3]);
				fprintf(fp,"%d  %d   %d  %d  %d\n",ldirtybit[i],lru[i][0],lru[i][1],lru[i][2],lru[i][3]);
			}
			else
			{
				    printf("%d  %d  %d  %d  %d\n",ldirtybit[i],lru[i][0],lru[i][1],lru[i][2],lru[i][3]);
				fprintf(fp,"%d  %d  %d  %d  %d\n",ldirtybit[i],lru[i][0],lru[i][1],lru[i][2],lru[i][3]);
			}
		}
	}
	}
	fclose(fp);
}

void printpcb()
{
	printf("\ncontents of PCB:\n");
	int i;
	for(i=0;i<18;i++)
	{
		if(shmem->pcb[i][0]>0)
		{
			printf("%d-> %d %d %d \n",i,shmem->pcb[i][0],shmem->pcb[i][1],shmem->pcb[i][2]);
		}
	}
}
void catch_alarm (int sig)//SIGALRM handler
{
	timeup=true;
	cleanup();
}
int emptyPCB()
{
	int i;
	for(i=0;i<18;i++)
	{
		if(shmem->pcb[i][0]==0)
		{
			//printf("%d is empty\n",i);
			return (i);
		}
	}
	//printf("No empty spot in PCB\n");
	return (-1);
}
int findlowest()
{
	int i;
	int lowests=lru[0][2];
	for(i=0;i<256;i++)
	{
		if(lru[i][2]<lowests)
		{
			lowests=lru[i][2];
		}
	}
	int lowestnum=lru[0][3];
	int lowestcounter=0;
	for(i=0;i<256;i++)
	{
		if(lru[i][3]<lowestnum && lowests==lru[i][2] && lru[i][3]>0)
		{
			lowestnum=lru[i][3];
			lowestcounter=i;
		}

	}
	printf("lowest number is %d which is at position %d\n",lowestnum,lowestcounter);
	return lowestcounter;
}
void startprocess()
{
	signal(SIGINT, cleanup);
	signal (SIGALRM, catch_alarm);
    alarm (3);
	int i,sum,err;
	while(1)
	{
		if(shmem->numofiterations>=40)
		{
			million=100000000;
		}
		//if(shmem->numofiterations>=40)
		{
			//cleanup();
		}

		if((err=sem_wait(&(shmem->osssem)))==-1)
		{
			perror("OSS: error in semwait for osssem");
			cleanup();
		}
		/*fp = fopen(logname, "a+");
	    if (fp == NULL)
	    {
	    	perror("oss: Unable to open the output file:");
	    }
	    else
	    {
	    	fprintf(fp,"oss has semaphore \n");
	    }
	    fclose(fp);*/
		//while(shmem->nanoseconds<=rann)
		{
			shmem->nanoseconds=shmem->nanoseconds+100000;
			if(shmem->nanoseconds>=billion)
			{
				shmem->seconds=shmem->seconds+1;
				shmem->nanoseconds=shmem->nanoseconds-billion;
			}
		}

		rann = shmem->nanoseconds+(rand() % 500);
		rans=shmem->seconds;

		if((emptypcbcounter=emptyPCB())>=0)
		{
			pid_t pID = fork();

			if (pID < 0)
			{
				perror("oss: Failed to fork:");
				//exit(EXIT_FAILURE);
			}

			else if (pID == 0)
			{
				static char *args[]={"./user",NULL};
				int status;
				if(( status= (execv(args[0], args)))==-1)
				{
					perror("oss:failed to execv");
					//exit(EXIT_FAILURE);
				}
			}

			if(pID>0)
			{
				shmem->processnum[emptypcbcounter]=pID;
				shmem->pcb[emptypcbcounter][0]=pID;
				shmem->pcb[emptypcbcounter][1]=emptypcbcounter;
				shmem->pcb[emptypcbcounter][2]=shmem->seconds;
				shmem->pcb[emptypcbcounter][3]=shmem->nanoseconds;
				//printf("pdone[%d]=true\n",emptypcbcounter);
				pdone[emptypcbcounter]=true;
				//printpcb();
				printf("created process %d at time %d:%d at PCB position %d \n",pID,shmem->seconds,shmem->nanoseconds,emptypcbcounter);
				if(shmem->processnumcounter<18)
				{
					shmem->processnumcounter++;//used to keep track of how many total processes have started so far.
				}
				//printpcb();
				FILE *fp;
				fp = fopen("log.dat", "a+");
			    if (fp == NULL)
			        {
			        	perror("oss: Unable to open the output file:");
			        }
			    else
			    {
			    	fprintf(fp,"created process %d at time %d:%d at PCB position %d \n",pID,shmem->seconds,shmem->nanoseconds,emptypcbcounter);
			    }
			    fclose(fp);
			}
		}
		sem_post(&(shmem->osssem));
		sum=0;
	    for (i = 0; i < million; i++)
	    sum += i;
	    int initialspeed=0;
	    printf("\nGranting pending requests in queue (if any)\n");
	    FILE *fp;
		fp = fopen("log.dat", "a+");
	    if (fp == NULL)
	        {
	        	perror("oss: Unable to open the output file:");
	        }
	    else
	    {
	    	fprintf(fp,"Granting pending requests in queue (if any)\n");
	    }
	    fclose(fp);


	    for(i=0;i<18;i++)
	    {
	    	if(pdone[i]==true)
	    	{
	    		//printf("\ni=%d\n",i);
	    		sem_wait(&(shmem->mutexclock[i]));
	    		//printf("sem_wait on process %d semaphore done\n",i);
	    		fp = fopen(logname, "a+");
	            if (fp == NULL)
	            {
	               	perror("oss: Unable to open the output file:");
	            }
	            else
                {
                //	fprintf(fp,"sem wait on process %d done\n",i);
                }

	            fclose(fp);
	    		if(shmem->sharedmsg[i][0]!=0)
	    		{
	    			shmem->numofiterations++;

	    			if(shmem->sharedmsg[i][1]==-1)
	    			{
	    				printf("\n process %d wants to terminate at time %d %d\n",shmem->sharedmsg[i][0],shmem->seconds,shmem->nanoseconds);

	    				fp = fopen(logname, "a+");
	    	            if (fp == NULL)
	    	            {
	    	               	perror("oss: Unable to open the output file:");
	    	            }
	    	            else
	                    {
	                    	fprintf(fp,"\n process %d wants to terminate at time %d %d\n",shmem->sharedmsg[i][0],shmem->seconds,shmem->nanoseconds);
	                    }

	    	            fclose(fp);
	    	            terminated++;
	    	            int pidforwait=shmem->sharedmsg[i][0];
	                    int corpse,status;
	    	            while ((corpse = waitpid(pidforwait, &status, 0)) != pidforwait && corpse != -1)
	    	    		   {
	    	    			char pmsg[64];
	    	    			snprintf(pmsg, sizeof(pmsg), "oss: PID %d exited with status 0x%.4X", corpse, status);
	    	    			perror(pmsg);
	    	    		   }
	    	            //clearing PCB
	    	            updatePCB(pidforwait);
	    	            //clearing matrices
	    	            clearprocess(shmem->sharedmsg[i][0],shmem->sharedmsg[i][2]);
	                    shmem->sharedmsg[i][0]=0;
	                    shmem->sharedmsg[i][1]=0;
	                    shmem->sharedmsg[i][2]=0;
	    			}
	    			else
	    			{
	    				initialspeed=shmem->nanoseconds;
	    				int currentpid=shmem->sharedmsg[i][0];
	    				int requestpage=shmem->sharedmsg[i][1];
	    				int pno=shmem->sharedmsg[i][2];
	    				int rorw=shmem->sharedmsg[i][3];
	    				int requesttimes=shmem->seconds;
	    				int requesttimen=shmem->nanoseconds;
	    				fp = fopen(logname, "a+");
	                    if (fp == NULL)
	                    {
	                    	perror("oss: Unable to open the output file:");
	                    }
	                    else
	                    {
	                    	//printf("\nentering data in file");
	                    	fprintf(fp,"\nprocess %d is requesting page %d at time %d %d",currentpid, requestpage,shmem->seconds,shmem->nanoseconds);
	                    	if(rorw==0)
		                    {
		                    	fprintf(fp," for reading\n");
		                    }
		                    else if(rorw==1)
		                    {
		                    	fprintf(fp," for writing\n");
		                    }

	                    }
	                    fclose(fp);
	                    printf("\nprocess %d is requesting page %d at time %d %d",currentpid, requestpage,shmem->seconds,shmem->nanoseconds);
	                    if(rorw==0)
	                    {
	                    	printf(" for reading\n");
	                    }
	                    else if(rorw==1)
	                    {
	                    	printf(" for writing\n");
	                    }
	                    //FIFO
	                    //if the page is not in main memory
	    					if(shmem->pagetable[pno][requestpage]==-1)
	    					{
	    						fpgfault++;
	    						printf("FIFO: Page fault\n");
	    						fp = fopen(logname, "a+");
	    		                if (fp == NULL)
	    		                {
	    		                	perror("oss: Unable to open the output file:");
	    		                }
	    		                else
	    		                {
	    		                	//printf("\nentering data in file");
	    		                	fprintf(fp,"FIFO: Page fault\n");
	    		                }
	    		                fclose(fp);
	    		                shmem->nanoseconds+=14000000;
	    	    	            //printf(" Page fault nanoseconds=%d\n",shmem->nanoseconds);

	    		                mmfilled++;
	    		                if(mmfilled==256)//256
	    		                {
	    		                	mmfilled=0;
	    		                }
	    		                //if there is already something in the frame replace the frame out
	    		                if(mm[mmfilled][0]>0)
	    		                {
	    		                	fp = fopen(logname, "a+");
		    	    	            if (fp == NULL)
		    	    	            {
		    	    	               	perror("oss: Unable to open the output file:");
		    	    	            }
		    	    	            else
		    	                    {
		    	    	            	fprintf(fp,"FIFO: Page Replacement\n");
		    	    	            	printf("FIFO: Page Replacement\n");
		    	    	            	fprintf(fp,"There is already something in frame process %d frame %d \n",mm[mmfilled][0],mm[mmfilled][1]);

	    		                	int c,k;
	    		                	for(k=0;k<18;k++)
	    		                	{
	    		                		if(shmem->pcb[k][0]==mm[mmfilled][0])
	    		                		{
	    		                			c=k;
	    		                			break;
	    		                		}
	    		                	}
	    		                	fprintf(fp,"clearing pagetable [%d]",c);
	    		                	shmem->pagetable[c][mm[mmfilled][1]]=-1;

		    	                    }
		    	    	            fclose(fp);
		    	    	            shmem->nanoseconds+=14000000;
	    		                }

	    		                //printf("nanoseconds=%d\n",shmem->nanoseconds);
	    		                mm[mmfilled][0]=currentpid;
	    		                mm[mmfilled][1]=requestpage;
	    		                dirtybit[mmfilled]=rorw;
	    		                //printf("pno %d requestpage %d\n",pno,requestpage);
	    		                shmem->pagetable[pno][requestpage]=mmfilled;
	    		                fp = fopen(logname, "a+");
	    	    	            if (fp == NULL)
	    	    	            {
	    	    	               	perror("oss: Unable to open the output file:");
	    	    	            }
	    	    	            else
	    	                    {
	    	    	            	fprintf(fp,"putting in frame %d \n",shmem->pagetable[pno][requestpage]);
	    	                    }
	    	    	            fclose(fp);
	    		                printf("putting in frame %d \n",shmem->pagetable[pno][requestpage]);

	    					}
	    					else//page is already in the main memory
	    					{
	    						printf("FIFO: Page already in frame table, request granted\n");
	    						shmem->nanoseconds+=10;

	    					}
	    					int temp=shmem->nanoseconds-initialspeed;
	    					if(fspeed!=0)
	    					{
	    						fspeed=(fspeed+temp)/2;
	    					}
	    					else
	    					{
	    						fspeed=temp;
	    					}

	    					//LRU
	    					initialspeed=shmem->nanoseconds;
	    					if(shmem->lpagetable[pno][requestpage]==-1)
	    					{
	    						lpgfault++;
	    						printf("LRU: Page fault\n");
	    						fp = fopen(logname, "a+");
	    		                if (fp == NULL)
	    		                {
	    		                	perror("oss: Unable to open the output file:");
	    		                }
	    		                else
	    		                {
	    		                	//printf("\nentering data in file");
	    		                	fprintf(fp,"LRU: Page fault\n");
	    		                }
	    		                fclose(fp);
	    		                shmem->nanoseconds+=14000000;
	    	    	            //printf(" Page fault nanoseconds=%d\n",shmem->nanoseconds);

	    		                lrufilled++;
	    		                if(lrufilled==256)//256
	    		                {
	    		                	framesfull=true;
	    		                }
	    		                if(framesfull==true)
	    		                {
	    		                	printf("Frames are full so checking the lowest\n");
	    		                	lrufilled=findlowest();
	    		                }
	    		                //if there is already something in the frame replace the frame out
	    		                if(lru[lrufilled][0]>0)
	    		                {
	    		                	fp = fopen(logname, "a+");
		    	    	            if (fp == NULL)
		    	    	            {
		    	    	               	perror("oss: Unable to open the output file:");
		    	    	            }
		    	    	            else
		    	                    {
		    	    	            	fprintf(fp,"LRU: Page Replacement\n");
		    	    	            	printf("LRU: Page Replacement\n");
		    	    	            	fprintf(fp,"There is already something in frame process %d frame %d \n",lru[lrufilled][0],lru[lrufilled][1]);

	    		                	int c,k;
	    		                	for(k=0;k<18;k++)
	    		                	{
	    		                		if(shmem->pcb[k][0]==lru[lrufilled][0])
	    		                		{
	    		                			c=k;
	    		                			break;
	    		                		}
	    		                	}
	    		                	fprintf(fp,"clearing pagetable [%d]",c);
	    		                	shmem->lpagetable[c][lru[lrufilled][1]]=-1;
		    	                    }
		    	    	            fclose(fp);
		    	    	            shmem->nanoseconds+=14000000;
	    		                }

	    		                //printf("nanoseconds=%d\n",shmem->nanoseconds);
	    		                lru[lrufilled][0]=currentpid;
	    		                lru[lrufilled][1]=requestpage;
	    		                lru[lrufilled][2]=requesttimes;
	    		                lru[lrufilled][3]=requesttimen;
	    		                ldirtybit[lrufilled]=rorw;

	    		                //printf("pno %d requestpage %d\n",pno,requestpage);
	    		                shmem->lpagetable[pno][requestpage]=lrufilled;
	    		                fp = fopen(logname, "a+");
	    	    	            if (fp == NULL)
	    	    	            {
	    	    	               	perror("oss: Unable to open the output file:");
	    	    	            }
	    	    	            else
	    	                    {
	    	    	            	fprintf(fp,"putting in frame %d \n",shmem->lpagetable[pno][requestpage]);
	    	                    }
	    	    	            fclose(fp);
	    		                printf("LRU: putting in frame %d \n",shmem->lpagetable[pno][requestpage]);

	    					}
	    					else//page is already in the main memory
	    					{
	    						printf("LRU: Page already in frame table, request granted\n");
	    						shmem->nanoseconds+=10;
	    						int frameno=shmem->lpagetable[pno][requestpage];
	    						lru[frameno][2]=requesttimes;
	    						lru[frameno][3]=requesttimen;
	    					}

	    					int temp1=shmem->nanoseconds-initialspeed;
	    					if(lspeed!=0)
	    					{
	    						lspeed=(lspeed+temp1)/2;
	    					}
	    					else
	    					{
	    						lspeed=temp1;
	    					}
	    					shmem->sharedmsg[i][0]=0;
	    					shmem->sharedmsg[i][1]=0;
	    					shmem->sharedmsg[i][2]=0;

	    			}//end else

	    		}//end shared memory!=0
	    		else
	    		{
	    			//printf("shared memory for %d ==0\n",i);
	    			fp = fopen(logname, "a+");
    	            if (fp == NULL)
    	            {
    	               	perror("oss: Unable to open the output file:");
    	            }
    	            else
                    {
                   // 	fprintf(fp,"shared memory for %dth process ==0\n",i);
                    }

    	            fclose(fp);
	    		}

	    		sem_post(&(shmem->mutexclock[i]));
	    		//delay
	    		//sum=0;
	    	    //for (i = 0; i < million; i++)
	    	    //sum += i;
	    	}//end if p[done]==true
	    	else
	    	{
	    		//printf("pdone[%d]==false\n",i);
	    	}
	    }//end for
	    if(shmem->numofiterations>0)
	    {
	    	printmatrix();
	    }


	}//end while
}

void main()
{
	int i,j;

	// Create our key
	key_t shmkey = ftok("makefile",777);
	if (shmkey == -1) {
		perror("oss:Ftok failed");
		exit(-1);
	}

	// Get our shm id
	shmid = shmget(shmkey, sizeof(SharedData), 0666 | IPC_CREAT);
	if (shmid == -1) {
		perror("oss:Failed to allocate shared memory region");
		exit(-1);
	}

	// Attach to our shared memory
	shmem = (SharedData*)shmat(shmid, (void *)0, 0);
	if (shmem == (void *)-1) {
		perror("oss:Failed to attach to shared memory region");
		exit(-1);
	}
	for(i=0;i<18;i++)
	{
		for(j=0;j<4;j++)
		{
			shmem->pcb[i][j]=0;

		}
	}
	for(i=0;i<18;i++)
	{
		for(j=0;j<32;j++)
		{
			shmem->pagetable[i][j]=-1;
			shmem->lpagetable[i][j]=-1;
		}
	}

	shmem->pidcounter=-1;
	emptypcbcounter=0;
	for(i=0;i<18;i++)
	{
		sem_init(&(shmem->mutexclock[i]),1,1);
	}
	sem_init(&(shmem->osssem),1,1);
	shmem->nanoseconds=0;
	shmem->seconds=0;
	for(i=0;i<18;i++)
	{
		pdone[i]=false;
		for(j=0;j<4;j++)
		{
			shmem->sharedmsg[i][j]=0;
		}
	}

    shmem->processnumcounter=0;
    shmem->blockedcounter=-1;
    shmem->allblocked=0;
    shmem->numofiterations=0;
	rann = (rand() % 500);
	rans=shmem->seconds;
	srand(time(NULL) ^ (getpid()<<16));
	for(i=0;i<256;i++)
	{
		dirtybit[i]=-1;
		for(j=0;j<2;j++)
		{
			mm[i][j]=0;
		}
	}
	for(i=0;i<256;i++)
	{
		for(j=0;j<3;j++)
		{
			lru[i][j]=0;
		}
	}
	fp = fopen(logname, "w+");
    if (fp == NULL)
        {
        	perror("oss: Unable to open the output file:");
        }
    fclose(fp);
	startprocess();
}
