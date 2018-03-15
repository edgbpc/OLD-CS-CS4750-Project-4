#include "oss.h"

Message message;

int main (int argc, char *argv[]){

	if (signal(SIGINT, handle) == SIG_ERR){
		perror("signal failed");
	}

	//message queue key
	key_t messageQueueKey = 59569;

	//Shared Memory Keys
	key_t keySimClock = 59566;
	keyPCB = 59567;
	

//TESTING ONLY
//char sampleMessage[20] = "A test message";
//strcpy(message.mesg_text, sampleMessage);
//msgsnd(msgid, &message, sizeof(message), 0);
//
//printf("Data sent from child was %s\n", message.mesg_text);
//END TESTING


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
	
	printf("IN CHILD - PCBTable[0].testBit is %d\n", PCBTable[0].PCB_testBit);

	printf("IN Child - simClock[0] is %d\n", simClock[0]);
	printf("In Child - simClock [1] is %d\n", simClock[1]);
	
	//message queue
//	if ((messageBoxID = msgget(messageQueueKey, 0666)) == -1){
//		perror("user: failed to acceess parent message box");
//		}


	//CRITICAL SECTION


//		msgrcv(messageBoxID, &message, sizeof(message), 1, 0); //retrieve message from max box.  child is blocked unless there is a message to take from box

		printf("IN CHILD - mesg_type is %ld\n", message.mesg_type);
		printf("IN CHILD - time slice is %d\n", message.timeSliceAssigned);

return 0;

}

void handle(int signo){
	if(signo == SIGINT){
		fprintf(stderr, "User process %d shut down by signal\n", getpid());
		exit(0);
	}
}



	

