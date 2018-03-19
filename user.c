#include "oss.h"


int main (int argc, char *argv[]){
	
	//seed random number generator
	srand(time(NULL));
	
	int randomTerminateConstant = 5;
	int randomBlockConstant = 0; //used to help determine if a process will Block.  value of 5 indicates 50% change to block 

	if (signal(SIGINT, handle) == SIG_ERR){
		perror("signal failed");
	}

	//message queue key
	key_t messageQueueKey = 59569;

	//Shared Memory Keys
	key_t keySimClock = 59566;
	keyPCB = 59567;
	
	//Get Shared Memory
	if ((shmidSimClock = shmget(keySimClock, SHM_SIZE, 0666)) == -1){
		perror("user: could not get shmidSimClock\n");
		return 1;
	}
	

	if ((shmidPCB = shmget(keyPCB, 20*sizeof(ProcessControlBlock), 0666)) == -1){
		perror("user: could not get shmidPCB");
		return 1;
	}

	//Attach to shared memory and get simClock time
	//used to determine how long the child lived for
	 unsigned int * simClock= (int *)(shmat(shmidSimClock, 0, 0));
	ProcessControlBlock * PCBTable = (ProcessControlBlock *)(shmat(shmidPCB, 0, 0));
	

	printf("IN Child - simClock[0] is %d\n", simClock[0]);
	printf("In Child - simClock [1] is %d\n", simClock[1]);
	
	//message queue
	if ((messageBoxID = msgget(messageQueueKey, 0666)) == -1){
		perror("user: failed to acceess parent message box");
		}
	msgrcv(messageBoxID, &message, sizeof(message), 1, 0); //retrieve message from max box.  child is blocked unless there is a message to take from box
		
	message.mesg_type = 1;

	//PCBTable[message.PCBTableLocation].PCB_localSimPid = getpid();

	
	printf("IN CHILD - time slice is %d\n", message.timeSliceAssigned);
	
	//decide if terminates
	int willTerminate = (rand() % 10) + 1;
		//determine how much of its timeSLice to use from 0 to its max timeSliceAssigned
		if (willTerminate < randomTerminateConstant){
			//select how much of its timeSlice was used
			message.timeSliceUsed = rand() % (message.timeSliceAssigned + 1 - 0) + 0;
			printf("In CHILD - used %d amount of time slice\n", message.timeSliceUsed);
			//build message to send to oss
			
			message.timeSliceUnused = message.timeSliceAssigned - message.timeSliceUsed;
			message.didTerminate = true;

			//send message to oss
			if (msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
				perror("user: failed to send to parent box");
			}
		

			exit(2);	
		}

	//if willTerminate is false, drop to following code
	
	int getsBlockedByEvent = (rand() % 10) + 1;
		
		if (getsBlockedByEvent = randomBlockConstant){
			printf("dfjaeldkfja;l\n");

	
		} else {
			//executes of getsBlockedByEvent results in all time being used
			message.timeSliceUsed = message.timeSliceAssigned;
	//		message.PCBTableLocation = message.PCBTableLocation;
		
			printf("IN CHILD ELSE STATEMENT - timeSliceUsed is %d\n", message.timeSliceUsed);
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



	

