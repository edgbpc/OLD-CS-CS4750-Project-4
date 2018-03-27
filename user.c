#include "oss.h"

unsigned int * simClock;

int main (int argc, char *argv[]){
	printf("USER has launced\n");
	int myPID = getpid();
	int PCBTableLocation;
	//seed random number generator
	
	time_t timeSeed;
	srand((int)time(&timeSeed) % getpid()); //%getpid used because children were all getting the same "random" time to run. 

	int randomBlockConstant = 25 ; //used to help determine if a process will Block.  
	int randomTerminateConstant = 90; //change this to increase or decrease chance process will terminate.  currently 50% to terminate
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
	simClock= (int *)(shmat(shmidSimClock, 0, 0));
	ProcessControlBlock * PCBTable = (ProcessControlBlock *)(shmat(shmidPCB, 0, 0));
	
	//message queue key
	key_t messageQueueKey = 59569;
	
	//get message box
	if ((messageBoxID = msgget(messageQueueKey, 0666)) == -1){
		perror("user: failed to acceess parent message box");
	}

	//record when this process was created
	int processCreatedNS = simClock[0];
	int processCreatedSecs = simClock[1];
	int running = 0;  //used to govern the inner loop starts out 0 so outer while immediately drops to being blocked waiting for a message
	int timeSlice;
	int location;
	int timeSliceUsed;


while (1){ //runs continously once process created but outer while loop will be blocked until message received from OSS.  
	
	int willTerminate = (rand() % 100) + 1;
	int getsBlockedByEvent = (rand() % 100) + 1;
	
	while (running == 1){ //intially check will fail, dropping down to waiting for a message from OSS.
	
		//checks if program willTerminate.
		if (willTerminate <= randomTerminateConstant){
			//select how much of its timeSlice was used
			timeSliceUsed = rand() % (timeSlice + 1 - 0) + 0;
			//update time used in PCBTable
			PCBTable[message.PCBTableLocation].PCB_processBlocked = 0;
			PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst = timeSliceUsed;
			PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem += timeSliceUsed;
			
			//notify OSS that process terminated
			message.mesg_type = getppid();
			message.didTerminate = true;	
			message.PCBTableLocation = location;
			if (msgsnd(messageBoxID, &message, sizeof(message), IPC_NOWAIT) == -1){ //IPC_NOWAIT specified so eecution contineus to break
				perror("user--------------: failed to send to parent box");
				}
			running = 0;
			exit(1);	
			printf("I should never print\n");
		} 
	 	if (getsBlockedByEvent <= randomBlockConstant){
			//wait for an event lasts r.s secondsi
			int eventTimeLastedNS = rand() % 6; //range [0-5]  (r)
			int eventTimeLastedSecs = rand() % 1001; //range [0-1000] (s)
			int preempted = (rand() % 99) + 1; //range [1-99] (p)
			PCBTable[message.PCBTableLocation].PCB_processBlocked = 1;
			PCBTable[message.PCBTableLocation].PCB_blockedWakeUpNS = simClock[0] + eventTimeLastedNS;
			PCBTable[message.PCBTableLocation].PCB_blockedWakeUpSecs = simClock[1] + eventTimeLastedSecs;
			PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst = preempted;
			PCBTable[message.PCBTableLocation].PCB_timeSliceUnused = timeSlice - preempted;
			PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem += preempted;
			running = 0;
			message.didTerminate = false;
			message.mesg_type = getppid();
			message.PCBTableLocation = location;

			if (msgsnd(messageBoxID, &message, sizeof(message), IPC_NOWAIT) == -1){ //IPC_NOWAIT specified so eecution contineus to break
				perror("user--------------: failed to send to parent box");
			}
			break; //break inner loop
		} else { 
			//executes of getsBlockedByEvent results in all time being usedi
			timeSliceUsed = timeSlice;
			//printf("-----------USER: PCBTableLocation is %d\n", message.PCBTableLocation);
			PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst = timeSliceUsed;
			PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem += timeSliceUsed;
			PCBTable[message.PCBTableLocation].PCB_localSimPid = getpid();
			PCBTable[message.PCBTableLocation].PCB_processBlocked = 0;
			message.didTerminate = false;
			message.mesg_type = getppid();
			message.PCBTableLocation = location;
			running = 0;
		
			if (msgsnd(messageBoxID, &message, sizeof(message), IPC_NOWAIT) == -1){ //IPC_NOWAIT specified so eecution contineus to break
				perror("user--------------: failed to send to parent box");
			}
		}
		break;
	}

	msgrcv(messageBoxID, &message, sizeof(message), myPID, 0); //retrieve message from message box.  child is blocked unless there is a message to take from box
	
	if (PCBTable[message.PCBTableLocation].PCB_processBlocked == 1){ //if the process was previously blocked, use its remaining timeSlice
		timeSlice = PCBTable[message.PCBTableLocation].PCB_timeSliceUnused; //assigned amount of unused timeSlice
	} else {
		timeSlice = message.timeSliceAssigned; //otherwise, process as full timeSlice
	}
	
	location = message.PCBTableLocation;
	PCBTable[location].PCB_processBlocked = 0; //unmark the process as blocked.  
	running = 1;

}

}
void handle(int signo){
	if(signo == SIGINT){
		fprintf(stderr, "User process %d shut down by signal\n", getpid());
		exit(0);
	}
}



	

