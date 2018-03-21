#include "oss.h"


int main (int argc, char *argv[]){
	
printf("IN CHILD - CHILD WITH %d IS EXECUTING\n", getpid());
//seed random number generator
	srand(time(NULL)+getpid());
	
	int randomTerminateConstant = 11;
	int randomBlockConstant = 0; //used to help determine if a process will Block.  value of 5 indicates 50% change to block 

	if (signal(SIGINT, handle) == SIG_ERR){
		perror("signal failed");
	}


	//Shared Memory Keys
	key_t keySimClock = 59566;
	keyPCB = 59567;
	
	//Get Shared Memory
	if ((shmidSimClock = shmget(keySimClock, SHM_SIZE, 0666)) == -1){
		perror("user: could not get shmidSimClock\n");
		return 1;
	}
	

	if ((shmidPCB = shmget(keyPCB, 18*sizeof(ProcessControlBlock), 0666)) == -1){
		perror("user: could not get shmidPCB");
		return 1;
	}

	//Attach to shared memory and get simClock time
	//used to determine how long the child lived for
	 unsigned int * simClock= (int *)(shmat(shmidSimClock, 0, 0));
	ProcessControlBlock * PCBTable = (ProcessControlBlock *)(shmat(shmidPCB, 0, 0));
	

	printf("IN Child - simClock[0] is %d\n", simClock[0]);
	printf("In Child - simClock [1] is %d\n", simClock[1]);
	

	//message queue key
	key_t messageQueueKey = 59569;
		
	message.mesg_type = 1;

	//message queue
	if ((messageBoxID = msgget(messageQueueKey, 0666)) == -1){
		perror("user: failed to acceess parent message box");
		}
	printf("IN CHILD - CHILD WITH PID %d IS WAITING FOR A MESSAGE\n", getpid());
	msgrcv(messageBoxID, &message, sizeof(message), 1, 0); //retrieve message from max box.  child is blocked unless there is a message to take from box
	//PCBTable[message.PCBTableLocation].PCB_localSimPid = getpid(); 
	printf("IN CHILD - RECEIVED A MESSAGE FROM PID %d\n", getppid());
	
	printf("IN CHILD - time slice is %d\n", message.timeSliceAssigned);
	
	//decide if terminates
	int willTerminate = (rand() % 10) + 1;
		printf("Process %d testing if terminate\n", getpid());
		//determine how much of its timeSLice to use from 0 to its max timeSliceAssigned
	if (willTerminate >= randomTerminateConstant){
		//select how much of its timeSlice was used
		int timeSliceUsed = rand() % (message.timeSliceAssigned + 1 - 0) + 0;
		printf("In CHILD - used %d amount of time slice\n", timeSliceUsed);
		//build message to send to oss
		
		//update time used in PCBTable
		PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst = timeSliceUsed;
		PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem += timeSliceUsed;
		PCBTable[message.PCBTableLocation].PCB_localSimPid = getpid();
			
		message.didTerminate = true;			
		message.PCBTableLocation = message.PCBTableLocation;
	
		//send message to oss
		if (msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
			perror("user: failed to send to parent box");
		}
		
		exit(0);	
	}

	//if willTerminate is false, drop to following code
	
	int getsBlockedByEvent = (rand() % 10) + 1;
		printf("Process %d testing if blocked by event\n", getpid());
		if (getsBlockedByEvent == randomBlockConstant){
			//wait for an event lasts r.s seconds
			int eventTimeLastedNS = rand() % 6; //range [0-5]  (r)
			int eventTimeLastedSecs = rand() % 1001; //range [0-1000] (s)
			int preempted = (rand() % 99) + 1; //range [1-99] (p)

			message.didTerminate = false;
			if (msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
				perror("user: failed to send to parent box");
			}

	
		} else {
			printf("IN CHILD GETS BLOCK EVENT IN ELSE\n");
			//executes of getsBlockedByEvent results in all time being usedi
			int timeSliceUsed = message.timeSliceAssigned;

			printf("PCBTableLocation is %d\n", message.PCBTableLocation);

			PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst = timeSliceUsed;
			PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem += timeSliceUsed;
			PCBTable[message.PCBTableLocation].PCB_localSimPid = getpid();

			
			message.PCBTableLocation = message.PCBTableLocation;
			message.didTerminate = false;
	
			printf("IN CHILD ELSE STATEMENT - timeSliceUsed is %d\n", timeSliceUsed);
			printf("IN CHILD - DID NOT TERMINATE.  SENDING MESSAGE TO OSS\n");	
			if (msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
				perror("user: failed to send to parent box");
			}
	
			
		}
		
}

void handle(int signo){
	if(signo == SIGINT){
		fprintf(stderr, "User process %d shut down by signal\n", getpid());
		exit(0);
	}
}



	

