#include "oss.h"


int main (int argc, char *argv[]){
	int myPID = getpid();
//	printf("I received  %s from command line\n", argv[1]);
//	int processPid = atoi(argv[1]);
//	printf("I converted %s to a %d\n", argv[1], processPid);

	int PCBTableLocation;
	//seed random number generator
	srand(time(NULL)+getpid());
	
	int randomBlockConstant = 0; //used to help determine if a process will Block.  value of 5 indicates 50% change to block 
	int randomTerminateConstant = 25; //change this to increase or decrease chance process will terminate  
	//signal handling
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
	
	//message queue key
	key_t messageQueueKey = 59569;
	
	//get message box
	if ((messageBoxID = msgget(messageQueueKey, 0666)) == -1){
		perror("user: failed to acceess parent message box");
	}

	int running = 0;
while (1){
	printf("USER:  simClock[0] is %d\n", simClock[0]);
	printf("USER: simClock [1] is %d\n", simClock[1]);
	//decide if terminates
	int willTerminate = (rand() % 100) + 1;
	printf("Process %d testing if terminate\n", getpid());
	//decide if blocked by an event
//	int getsBlockedByEvent = (rand() % 10) + 1;
	int getsBlockedByEvent = 100;
	printf("Process %d deciding if blocked by event\n", getpid());
	if (running == 1){
		printf("USER: RECEIVED A MESSAGE FROM PID %d\n", getppid());
		printf("USER: time slice is %d\n", message.timeSliceAssigned);
		//checks if program willTerminate.
		if (willTerminate <= randomTerminateConstant){
			//select how much of its timeSlice was used
			int timeSliceUsed = rand() % (message.timeSliceAssigned + 1 - 0) + 0;
			printf("USER: used %d amount of time slice\n", timeSliceUsed);
			//update time used in PCBTable
			PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst = timeSliceUsed;
			PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem += timeSliceUsed;
//			PCBTable[message.PCBTableLocation].PCB_localSimPid = getpid();
			//notify OSS that process terminates
			message.mesg_type = 1;
			message.didTerminate = true;	
			printf("Telling the OSS that USER termianted\n");
			if (msgsnd(messageBoxID, &message, sizeof(message), IPC_NOWAIT) == -1){ //IPC_NOWAIT specified so eecution contineus to break
				perror("user--------------: failed to send to parent box");
					}
			exit(1);	
			printf("I should never print\n");
		} else 	if (getsBlockedByEvent == randomBlockConstant){
			//wait for an event lasts r.s seconds
			int eventTimeLastedNS = rand() % 6; //range [0-5]  (r)
			int eventTimeLastedSecs = rand() % 1001; //range [0-1000] (s)
			int preempted = (rand() % 99) + 1; //range [1-99] (p)
//			message.didTerminate = false;
		} else {
			//printf("-----------USER: PCB Location is %d\n", message.PCBTableLocation);
			//printf("------------USER: IN CHILD GETS BLOCK EVENT IN ELSE\n");
			//executes of getsBlockedByEvent results in all time being usedi
			int timeSliceUsed = message.timeSliceAssigned;
			//printf("-----------USER: PCBTableLocation is %d\n", message.PCBTableLocation);
			PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst = timeSliceUsed;
			PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem += timeSliceUsed;
			PCBTable[message.PCBTableLocation].PCB_localSimPid = getpid();
			//printf("USER: Time used %d\n", PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem);
			//message.PCBTableLocation = message.PCBTableLocation;
			//message.didTerminate = false;
			//printf("-------------USER: IN CHILD ELSE STATEMENT - timeSliceUsed is %d\n", timeSliceUsed);
			//printf("------------USER: IN CHILD - DID NOT TERMINATE.  SENDING MESSAGE TO OSS\n");	
			running = 0;
		}
		printf("running is %d\n", running);
	message.mesg_type = 1;
	if (msgsnd(messageBoxID, &message, sizeof(message), IPC_NOWAIT) == -1){ //IPC_NOWAIT specified so eecution contineus to break
		perror("user--------------: failed to send to parent box");
	}
	//break; //break the inner while loop
	}
	printf("Waiting for a message from %d\n", getppid());
	msgrcv(messageBoxID, &message, sizeof(message), myPID, 0); //retrieve message from message box.  child is blocked unless there is a message to take from box
	printf("Received message from %d\n", getppid());
	printf("message type was %d\n", message.mesg_type);
	running = 1;
}
}
void handle(int signo){
	if(signo == SIGINT){
		fprintf(stderr, "User process %d shut down by signal\n", getpid());
		exit(0);
	}
}



	

