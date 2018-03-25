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
	double terminateTime = 20; //used by setperiodic to terminate program

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
//sends a messag to the box	
//	printf("IN PARENT - SENDING FIRST MESSAGE TO BOX\n");
//	message.mesg_type = 1;	
//	if(msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
//		perror("oss: Failed to send message to child");
//	}

int counter = 0;
int newProcess;
int untilNextSecs;
int untilNextNS;

while(counter < 20 ){ 
	newProcess = 0; //each time loop runs, rest new proces to 0	

	//find the next time a new process is to be created
	//increments throughout life of OSS
	int randomSecs = rand() % maxTimeBetweenNewProcsSecs;
	int randomNS = rand() % maxTimeBetweenNewProcsNS;

	untilNextSecs += randomSecs;
	untilNextNS += randomNS;

	// if the simclock has reached a time for a new process, set the newProcess flag to 1
	if (simClock[0] >= untilNextNS && simClock[1] >= untilNextSecs){
		newProcess = 1;
	} else { //otherwise increment the clock to the next time a process should be created
		simClock[0] += untilNextNS; 
		simClock[1] += untilNextSecs;
		newProcess = 1;
	}	
		
		//create PCBTable Entry
		//returns first available index.  returns -1 if no locations are available to be written to in the PCB
		if ((PCBIndex = FindIndex(bitVector, 18 , 0)) == -1){
			printf("No available locatiosns in PCB"); //if no location is available
		}

		//if its time for a new process fork child process
		if (newProcess == 1){
			if ((childPid = fork()) == -1){ 
				perror("oss: failed to fork child");
			}
		} else {
			continue; //restart the while loop if it  wasn't time for a new process
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

			printf("*****  Created process with priority of %d\n", processPriority);
			
			if (bitVector[PCBIndex] == 0){	//0 indicates that a table location is available to be used
//				printf("OSS: creating PCBTableEntry\n");
				PCBTable[PCBIndex].PCB_CPUTimeUsed += 0;
				PCBTable[PCBIndex].PCB_timeUsedLastBurst += 0;
				PCBTable[PCBIndex].PCB_processPriority = processPriority; //assigns processPriority 
				PCBTable[PCBIndex].PCB_localSimPid = childPid;
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

		//check if any processes are in the 0 queue
		if(!isEmpty(roundRobinQueue)){	
			for (index = 0; index <= n; index++){ //loop through the PCBTable to find the 
				if(PCBTable[index].PCB_localSimPid == front(roundRobinQueue)){
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = assignTimeSlice(PCBTable[index].PCB_processPriority);
					message.PCBTableLocation = index;
					dequeue(roundRobinQueue);
				}
			}
		//check feedbackQueueLevel1
		} else if(!isEmpty(feedbackQueueLevel1)){
			for (index = 0; index <= n; index++){
				if(PCBTable[index].PCB_localSimPid == front(feedbackQueueLevel1)){
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = assignTimeSlice(PCBTable[index].PCB_processPriority);
					message.PCBTableLocation = index;
					dequeue(feedbackQueueLevel1);
				}
			}
		//check feedbackQueueLevel2
		} else if(!isEmpty(feedbackQueueLevel2)){
			for (index = 0; index <= n; index++){
				if(PCBTable[index].PCB_localSimPid == front(feedbackQueueLevel2)){
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = assignTimeSlice(PCBTable[index].PCB_processPriority);
					message.PCBTableLocation = index;
					dequeue(feedbackQueueLevel2);
				}
			}
		//check feedbackQueueLevel3
		} else if(!isEmpty(feedbackQueueLevel3)){
			for (index = 0; index <= n; index++){
				if(PCBTable[index].PCB_localSimPid == front(feedbackQueueLevel3)){
					message.mesg_type = PCBTable[index].PCB_localSimPid;
					message.timeSliceAssigned = assignTimeSlice(PCBTable[index].PCB_processPriority);
					message.PCBTableLocation = index;
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
		fprintf(fp, "Dispatching process with PID %d from queue %d at time %d.%d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_processPriority, simClock[1], simClock[0]);
		fflush(fp);
		
		if(msgsnd(messageBoxID, &message, sizeof(message), IPC_NOWAIT) ==  -1){
			perror("oss: failed to send message to user");
		}
		
		//receive a message
		printf("OSS is waiting for a message\n");
		if (msgrcv(messageBoxID, &message, sizeof(message), 1, 0) == -1){
			perror("oss: Failed to received a message");
		}
		printf("OSS received a message from Process PID %d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid);

		fprintf(fp, "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst);
		fflush(fp);
		
		//roundRobinProcesses
		if (PCBTable[message.PCBTableLocation].PCB_processPriority == 0){
			if(message.didTerminate == false){
			
				enqueue(roundRobinQueue, PCBTable[message.PCBTableLocation].PCB_localSimPid);
				fprintf(fp,"Process %d did not terminate.  Placing into Queue %d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_processPriority);
				fflush(fp);

			} else if (message.didTerminate == true){

			fprintf(fp, "Process with PID %d terminated, it used total of %d nanoseconds of CPU time\n", PCBTable[message.PCBTableLocation].PCB_localSimPid, PCBTable[message.PCBTableLocation].PCB_totalTimeInSystem);
			fflush(fp);
			bitVector[message.PCBTableLocation] = 0;
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

			}
		}
counter++;
}


//wait(NULL);
kill(0, SIGKILL);
printf("****************************simClock ended at %d.%d\n", simClock[1],simClock[0]);
printf("created %d processes\n", totalProcessesCreated);
//free  up memory queue and shared memory
shmctl(shmidPCB, IPC_RMID, NULL);
shmctl(shmidSimClock, IPC_RMID, NULL);
shmdt(simClock);
shmdt(PCBTable);
msgctl(messageBoxID, IPC_RMID, NULL);

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


//convert the timeClock sec and nanoseconds
void timeConvert(){
	if (simClock[0] > 1000000000){ //if nanoseconds clock is greater then 1B convert to seconds
		simClock[0] += simClock[0]/1000000000; //add number of fully divisible by nanoseconds to seconds clock.  will truncate
		simClock[1] = simClock[0]%1000000000; //assign remainder of nanoseconds to nanosecond clock
	}
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

