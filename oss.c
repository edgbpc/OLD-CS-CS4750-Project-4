#include "oss.h" //contains all other includes necessary, macro definations, and function prototypes


//Global Variables

int maxTimeBetweenNewProcsNS = 200000;
int maxTimeBetweenNewProcsSecs = 1;
int processTypeWeight = 90;
int baseQuantum = 10000; //10 milliseconds
int PCBIndex;
char fileName[10] = "data.log";
FILE * fp;
pid_t childPid; //pid id for child processes
unsigned int *simClock; // simulated system clock  simClock [0] = seconds, simClock[1] = nanoseconds
ProcessControlBlock *PCBTable;
ProcessControlBlock PCBTableEntry;
int bitVector[18];
int processPriority;

int main (int argc, char *argv[]) {
	
	int OSSPid = getpid();	
	
	//Create Queues
	Queue* roundRobinQueue = createQueue(18);
	Queue* blockedQueue = createQueue(18);
	Queue* feedbackQueueLevel1 = createQueue(18); 
	Queue* feedbackQueueLevel2 = createQueue(18); 
	Queue* feedbackQueueLevel3 = createQueue(18); 

	//initalize variables
	//keys
	messageQueueKey = 59569;
	keySimClock = 59566;
	keyPCB = 59567;
	
	int maxProcess = 18; //hard cap on total number of processes
	int processCount = 0; //current number of processes
	int processLimit = 100; //max number of processes allowed by assignment parameters
	int totalProcessesCreated = 0; //keeps track of all processes created	
	double terminateTime = 3; //used by setperiodic to terminate program

//open file for writing	
	fp = fopen(fileName, "w");
//seed random number generator
	srand(time(NULL));
//signal handler to catch ctrl c
	if(signal(SIGINT, handle) == SIG_ERR){
		perror("Signal Failed");
	}
	if(signal(SIGALRM, handle) == SIG_ERR){
		perror("Signal Failed");
	}	
//set timer. from book
	if (setperiodic(terminateTime) == -1){
		perror("oss: failed to set run timer");
		return 1;
	}
//Create Shared Memory
	if ((shmidSimClock = shmget(keySimClock, SHM_SIZE, IPC_CREAT | 0666)) == -1){
		perror("oss: could not create shared memory for simClock");
		return 1;
	}
	if ((shmidPCB = shmget(keyPCB, 18*sizeof(ProcessControlBlock), IPC_CREAT | 0666)) == -1){
		perror("oss: could not create shared memory for PCB");
		return 1;
	}
//Attach to shared memory and initalize clock to 0
	simClock = shmat(shmidSimClock, NULL, 0);
	simClock[0] = 0; //nanoseconds
	simClock[1] = 0; //seconds
	PCBTable = shmat(shmidPCB, NULL, 0);
	
//create mail box
	if ((messageBoxID = msgget(messageQueueKey, IPC_CREAT | 0666)) == -1){
		perror("oss: Could not create child mail box");
		return 1;
	}

int counter = 0;
int newProcess = 1;

//advance clock to first process
int untilNextSecs = rand() % maxTimeBetweenNewProcsSecs;
int untilNextNS = rand() % maxTimeBetweenNewProcsNS;

simClock[0] += untilNextNS;
simClock[1] += untilNextSecs;

while(counter < 20){ //setting max loops because program will crash if left to run infinitely.
	counter++;
	newProcess = 0; //turn off newProcess until the next time a new process is due
	//find the next time a new process is to be created
	
	//really uncertain about this logic
	int randomSecs = rand() % maxTimeBetweenNewProcsSecs;
	int randomNS = rand() % maxTimeBetweenNewProcsNS;

	untilNextSecs += randomSecs;
	untilNextNS += randomNS;

	// if the simclock has reached a time for a new process, set the newProcess flag to 1
	// not to sure about this logic
	if (simClock[0] >= untilNextNS && simClock[1] >= untilNextSecs){
		newProcess = 1;
	} else { //otherwise increment the clock to the next time a process should be created
		simClock[0] += untilNextNS; 
		simClock[1] += untilNextSecs;
	}	

	while (newProcess == 1){	
		newProcess = 0; //change flag back to 0
		//create PCBTable Entry
		//returns first available index.  returns -1 if no locations are available to be written to in the PCB
		if ((PCBIndex = FindIndex(bitVector, 18 , 0)) == -1){
			fprintf(fp, "OSS: Attempted to create process. No available locations in PCB\n"); //if no location is available
			fflush(fp);
		}
		//if its time for a new process fork child process
			if ((childPid = fork()) == -1){ 
			perror("oss: failed to fork child");
		}	
		//exec child process
		if (childPid == 0){
			execl("./user", NULL);	
		} else {
			//parent process sets priority and initalizes the PCBTable Entry
		
			int priority = (rand() % 100) + 1; //generate random number between 1 and 10
			//if priority is <= 9, then the processPriority will be user.  this weights 
			//the process so mostly user processes are created.  Change variable
			// processWeightType to increase or decrease change of user process	
			if (priority >= processTypeWeight){
				processPriority = 0; //real time process
			} else {
				processPriority = 1; //user process
			}
			if (bitVector[PCBIndex] == 0){	//0 indicates that a table location is available to be used
//				printf("OSS: creating PCBTableEntry\n");
				PCBTable[PCBIndex].PCB_CPUTimeUsed = 0;
				PCBTable[PCBIndex].PCB_timeUsedLastBurst = 0;
				PCBTable[PCBIndex].PCB_localSimPid = childPid;
				PCBTable[PCBIndex].PCB_processBlocked = 0;
				PCBTable[PCBIndex].PCB_processPriority = processPriority;
				PCBTable[PCBIndex].PCB_index = PCBIndex;
				fprintf(fp, "OSS: Generating process with PID %d ", PCBTable[PCBIndex].PCB_localSimPid);
				bitVector[PCBIndex] = 1;
			}
		}
		//IF REAL TIME PROCESS QUEUE IN ROUND ROBIN, OTHERWISE QUEUE TO FEEDBACK QUEUE
		if ((PCBTable[PCBIndex].PCB_processPriority) == 0){ //Queue to 0 level if real tiem process
			enqueue(roundRobinQueue, childPid);
		} else {
			enqueue(feedbackQueueLevel1, childPid); //Queue to to 1 Level if user process.
		}
		fprintf(fp, "and putting it in Queue %d at time %d.%d\n", PCBTable[PCBIndex].PCB_processPriority, simClock[1], simClock[0]);
		fflush(fp);
		//SCHEDULING ALGORITHM
		int index;
		int n = sizeof(PCBTable)/sizeof(PCBTable[0]); //get the number of elements in pcbtable

		//check the blocked Queue first
		if(!isEmpty(blockedQueue)){
			for (index = 0; index <=n; index++){
				//uncertain about this logic
				if(simClock[0] >= PCBTable[index].PCB_blockedWakeUpNS && simClock[1] >= PCBTable[index].PCB_blockedWakeUpSecs){//if the simClock passed the wake up time
					fprintf(fp, "OSS: Waking up process %d\n", PCBTable[index].PCB_localSimPid);
					fflush(fp);
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = PCBTable[index].PCB_timeSliceUnused;
					message.PCBTableLocation = PCBTable[index].PCB_index;
					dequeue(blockedQueue);
				}
			}	
		//check roundRobnQueue
		} else 	if(!isEmpty(roundRobinQueue)){  
			for (index = 0; index <= n; index++){ //loop through the PCBTable to find the 
				if(PCBTable[index].PCB_localSimPid == front(roundRobinQueue)){
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = assignTimeSlice(PCBTable[index].PCB_processPriority);
					
					message.PCBTableLocation = PCBTable[index].PCB_index;
					//message.PCBTableLocation = index;
					dequeue(roundRobinQueue);
				}
			}
		//check feedbackQueueLevel1
		} else if(!isEmpty(feedbackQueueLevel1)){
			for (index = 0; index <= n; index++){
				if(PCBTable[index].PCB_localSimPid == front(feedbackQueueLevel1)){
					printf("Found %d in location %d\n", front(feedbackQueueLevel1), index);
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = assignTimeSlice(PCBTable[index].PCB_processPriority);
					message.PCBTableLocation = PCBTable[index].PCB_index;
				//	message.PCBTableLocation = index;
					dequeue(feedbackQueueLevel1);
				}
			}
		//check feedbackQueueLevel2
		} else if(!isEmpty(feedbackQueueLevel2)){
			for (index = 0; index <= n; index++){
				if(PCBTable[index].PCB_localSimPid == front(feedbackQueueLevel2)){
					printf("Found %d in location %d\n", front(feedbackQueueLevel2), index);
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = assignTimeSlice(PCBTable[index].PCB_processPriority);
					message.PCBTableLocation = PCBTable[index].PCB_index;
					//message.PCBTableLocation = index;
					dequeue(feedbackQueueLevel2);
				}
			}
		//check feedbackQueueLevel3
		} else if(!isEmpty(feedbackQueueLevel3)){
			for (index = 0; index <= n; index++){
				if(PCBTable[index].PCB_localSimPid == front(feedbackQueueLevel3)){
					printf("Found %d in location %d\n", front(feedbackQueueLevel3), index);
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = assignTimeSlice(PCBTable[index].PCB_processPriority);
					message.PCBTableLocation = PCBTable[index].PCB_index;
					//message.PCBTableLocation = index;
					dequeue(feedbackQueueLevel3);
				}
			}
		} 
		//launch the process
		//represents the amount of time it took to schedule and launch the process
		int timeToSchedule = (rand() % 9900) + 100;
		simClock[0] += timeToSchedule;
		//send message to child process 
		printf("OSS: Sending message with message type %d\n", message.mesg_type);
		if (PCBTable[message.PCBTableLocation].PCB_processBlocked == 1){
			fprintf(fp, "Dispatching process with PID %d from Blocked Queue at time %d.%d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, simClock[1], simClock[0]);
			fflush(fp);
	
			}else if(PCBTable[message.PCBTableLocation].PCB_processBlocked == 0){	
				fprintf(fp, "Dispatching process with PID %d from queue %d at time %d.%d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_processPriority, simClock[1], simClock[0]);
				fflush(fp);
			}
		if(msgsnd(messageBoxID, &message, sizeof(message), IPC_NOWAIT) ==  -1){
			perror("oss: failed to send message to user");
		}
		//receive a message
		printf("OSS is waiting for a message\n");
		if (msgrcv(messageBoxID, &message, sizeof(message), getpid(), 0) == -1){
			perror("oss: Failed to received a message");
		}
		printf("OSS received a message from Process PID %d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid);

		fprintf(fp, "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst);
		fflush(fp);
		//Process the message
		if (PCBTable[message.PCBTableLocation].PCB_processBlocked == 1){
			enqueue(blockedQueue, PCBTable[message.PCBTableLocation].PCB_localSimPid);
			fprintf(fp, "Process %d was blocked by operation.  Placing into Blocked Queue\n", PCBTable[message.PCBTableLocation].PCB_localSimPid);
			fflush(fp);
		//roundRobinProcesses
		} else if (PCBTable[message.PCBTableLocation].PCB_processPriority == 0){
			if(message.didTerminate == false){
				PCBTable[message.PCBTableLocation].PCB_processPriority = 0; //real time processes stay in 0 Queue	
				enqueue(roundRobinQueue, PCBTable[message.PCBTableLocation].PCB_localSimPid);
				fprintf(fp,"Process %d did not terminate.  Placing into Queue %d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_processPriority);
				fflush(fp);
			} else if (message.didTerminate == true){
				fprintf(fp, "Process with PID %d terminated, it used total of %d nanoseconds of CPU time\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem);
				fflush(fp);
				bitVector[message.PCBTableLocation] = 0;
				kill(PCBTable[message.PCBTableLocation].PCB_localSimPid, SIGKILL);
			}
		//feedbackQueueLevel1
		} else if (PCBTable[message.PCBTableLocation].PCB_processPriority == 1){
			if(message.didTerminate == false){
				PCBTable[message.PCBTableLocation].PCB_processPriority = 2; //change process priority to next level
				enqueue(feedbackQueueLevel2, PCBTable[message.PCBTableLocation].PCB_localSimPid); //queue into next level of queue
				fprintf(fp,"Process %d did not terminate.  Placing into Queue %d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_processPriority);
				fflush(fp);
			} else if (message.didTerminate == true){
				fprintf(fp, "Process with PID %d terminated, it used total of %d nanoseconds of CPU time\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem);
				fflush(fp);
				bitVector[message.PCBTableLocation] = 0;
				kill(PCBTable[message.PCBTableLocation].PCB_localSimPid, SIGKILL);
			}
		//feedbackQueueLevel2
		} else if (PCBTable[message.PCBTableLocation].PCB_processPriority == 2){
			if(message.didTerminate == false){
				PCBTable[message.PCBTableLocation].PCB_processPriority = 3; //change process priority to next level
				enqueue(feedbackQueueLevel3, PCBTable[message.PCBTableLocation].PCB_localSimPid); //queue into next level of queue
				fprintf(fp,"Process %d did not terminate.  Placing into Queue %d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_processPriority);
				fflush(fp);
			} else if (message.didTerminate == true){
				fprintf(fp, "Process with PID %d terminated, it used total of %d nanoseconds of CPU time\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem);
				fflush(fp);
				bitVector[message.PCBTableLocation] = 0;
				kill(PCBTable[message.PCBTableLocation].PCB_localSimPid, SIGKILL);
			}
		//feedbackQueueLevel3
		} else if (PCBTable[message.PCBTableLocation].PCB_processPriority == 3){
			if(message.didTerminate == false){
				PCBTable[message.PCBTableLocation].PCB_processPriority = 1; //wrap around to 1st feedbackQueueLevel1
				enqueue(feedbackQueueLevel1, PCBTable[message.PCBTableLocation].PCB_localSimPid); //queues into feedback level 1
				fprintf(fp,"Process %d did not terminate.  Placing into Queue %d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_processPriority);
				fflush(fp);
			} else if (message.didTerminate == true){
				fprintf(fp, "Process with PID %d terminated, it used total of %d nanoseconds of CPU time\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem);
				fflush(fp);
				bitVector[message.PCBTableLocation] = 0;
				kill(PCBTable[message.PCBTableLocation].PCB_localSimPid, SIGKILL);
			}
		
		}
	} //inner while loop
} //outer while loop


//wait(NULL);
printf("****************************simClock ended at %d.%d\n", simClock[1],simClock[0]);

printf("created %d processes\n", totalProcessesCreated);
//free  up memory queue and shared memory
shmdt(simClock);
shmdt(PCBTable);
shmctl(shmidPCB, IPC_RMID, NULL);
shmctl(shmidSimClock, IPC_RMID, NULL);
msgctl(messageBoxID, IPC_RMID, NULL);

kill(0, SIGKILL); //without this, some user processes continue to run even after OSS terminates
return 0;
}

//TAKEN FROM BOOK
static int setperiodic(double sec) {
   timer_t timerid;
   struct itimerspec value;
   if (timer_create(CLOCK_REALTIME, NULL, &timerid) == -1)
      return -1;
   value.it_interval.tv_sec = (long)sec;
   value.it_interval.tv_nsec = (sec - value.it_interval.tv_sec)*BILLION;
   if (value.it_interval.tv_nsec >= BILLION) {
      value.it_interval.tv_sec++;
      value.it_interval.tv_nsec -= BILLION;
   }
   value.it_value = value.it_interval;
   return timer_settime(timerid, 0, &value, NULL);
}

void handle(int signo){
	if (signo == SIGINT || signo == SIGALRM){
		printf("*********Signal was received************\n");
		kill(0, SIGKILL);
		shmctl(shmidPCB, IPC_RMID, NULL);
		shmctl(shmidSimClock, IPC_RMID, NULL);
		msgctl(messageBoxID, IPC_RMID, NULL);
		shmdt(simClock);	
		shmdt(PCBTable);
		wait(NULL);
		exit(0);
	}
}

//this function finds the first location in the bit array that is 0 indicating its available to accept a new ProcessControlBlock
int FindIndex(int bitVector[], int size, int value){
	int indexFound = 0;
	int index;
	for (index = 0; index < size; index++){
		if (bitVector[index] == value){
			indexFound = 1;
	//		printf("Found %d index\n", index);
			break;
		}
	}
	if (indexFound == 1){
	//	printf("---------------------returning %d\n", index);
		return index;
	} else {
		return -1;	
	}

}
//function assigns timeSlice based on process{Priority
int assignTimeSlice(int processPriority){
	int timeSlice;
	switch(processPriority){
		case 0:
			timeSlice = baseQuantum;
			break;
		case 1: timeSlice = 2*baseQuantum;
			break;
		case 2: timeSlice = 4*baseQuantum;
			break;
		case 3: timeSlice = 8*baseQuantum;
			break;
	}
	return timeSlice;

}



//Queue code comes from https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation

Queue* createQueue(unsigned capacity)
{
    Queue* queue = (Queue*) malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0; 
    queue->rear = capacity - 1;  // This is important, see the enqueue
    queue->array = (int*) malloc(queue->capacity * sizeof(int));
    return queue;
}
 
int isFull(Queue* queue){
	  return (queue->size == queue->capacity); 
 }
  
int isEmpty(Queue* queue){
	return (queue->size == 0);
 }
  
void enqueue(Queue* queue, int item){
       if (isFull(queue))
		  return;
       queue->rear = (queue->rear + 1)%queue->capacity;
       queue->array[queue->rear] = item;
       queue->size = queue->size + 1;
       printf("%d enqueued to queue\n",item);
}
                                
int dequeue(Queue* queue){
	if (isEmpty(queue))
		return INT_MIN;
        int item = queue->array[queue->front];
        queue->front = (queue->front + 1)%queue->capacity;
        queue->size = queue->size - 1;
        return item;
}
                                                             
int front(Queue* queue){
	if (isEmpty(queue))
	        return INT_MIN;	
	return queue->array[queue->front];
}
int rear(Queue* queue){
        if (isEmpty(queue))
                return INT_MIN;
        return queue->array[queue->rear];
}


/*
 KEPT FOR FUTURE USE
//getopt
	char option;
	while ((option = getopt(argc, argv, "s:hl:t:")) != -1){
		switch (option){
			case 's' : maxProcess = atoi(optarg);
				break;
			case 'h': print_usage();
				exit(0);
				break;
			case 'l': 
				strcpy(fileName, optarg);
				break;
			case 't':
				terminateTime = atof(optarg);
				break;
			default : print_usage(); 
				exit(EXIT_FAILURE);
		}
	}

void print_usage(){
	printf("Execution syntax oss -options value\n");
	printf("Options:\n");
	printf("-h: this help message\n");
	printf("-l: specify file name for log file.  Default: data.log\n");
	printf("-s: specify max limit of processes that can be ran.  Default: 5\n");
	printf("-t: specify max time duration in seconds.  Default: 20\n");
}
*/

