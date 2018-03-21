#include "oss.h" //contains all other includes necessary, macro definations, and function prototypes


//Global Variables


int maxTimeBetweenNewProcsNS = 50;
int maxTimeBetweenNewProcsSecs = 1;
int processTypeWeight = 5;
int baseQuantum = 10000000; //10 milliseconds

//file creation for log
char fileName[10] = "data.log";
FILE * fp;

pid_t childpid; //pid id for child processes

unsigned int *simClock; // simulated system clock  simClock [0] = seconds, simClock[1] = nanoseconds

ProcessControlBlock *PCBTable;
ProcessControlBlock PCBTableEntry;


int bitVector[18];

int processPriority;

int main (int argc, char *argv[]) {
	printf("at top of parent\n");

	
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

	//message
	message.mesg_type = 1;	
//	message.timeSliceAssigned = 55;

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
	
	//begin testing
	PCBTable[0].PCB_testBit = 1;
	printf("In Parent - PCBTable[0].PCB_testBit = %d\n", PCBTable[0].PCB_testBit); 
	//end testing

//create mail box
	if ((messageBoxID = msgget(messageQueueKey, IPC_CREAT | 0666)) == -1){
		perror("oss: Could not create child mail box");
		return 1;
	}

	
	if(msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
		perror("oss: Failed to send message to child");
	}


while(totalProcessesCreated < 1){
		//generate random simulated times a new process will start
		//
		if (isEmpty(roundRobinQueue)){
			printf("roundRobinQueue empty\n");
			int randomSecs = rand() % maxTimeBetweenNewProcsSecs;
			int randomNS = rand() % maxTimeBetweenNewProcsNS;

			printf("randomSec is %d\n", randomSecs);
			printf("randomNS is %d\n", randomNS);

			//advance simClock to time for next scheduled process
			simClock[0] += randomNS;
			simClock[1] += randomSecs;
			}
		
		//generate realtime or user priority
		int priority = (rand() % 10) + 1; //generate random number between 1 and 10
		//if priority is <= 9, then the processPriority will be user.  this weights 
		//the process so mostly user processes are created.  Change variable
		// processWeightType to increase or decrease change of user process	
		if (priority <= processTypeWeight){
			processPriority = 0;
		}
	
		//create PCBTable Entry
		int PCBIndex = FindIndex(bitVector, 18, 0); // get first available index location 
		printf("PCBIndex is %d\n", PCBIndex);	
		printf("bitVector[PCBIndex] is %d\n", bitVector[PCBIndex]);	
		if (bitVector[PCBIndex] == 0){	//0 indicates that a table location is available to be used
			printf("creating PCBTableEntry\n");
			PCBTable[PCBIndex].PCB_testBit = 1; //initalizing PCBTable Entry
			PCBTable[PCBIndex].PCB_CPUTimeUsed += 0;
			PCBTable[PCBIndex].PCB_timeUsedLastBurst += 0;
			PCBTable[PCBIndex].PCB_processPriority = 0; //// WILL NEED TO CHANGE THIS
					
			bitVector[PCBIndex] = 1;
		} 
		
	//	if (PCBIndex = -1){ //if FindIndex returned -1 no locations were available to create a PCB entry	
	//		int randomSecs = rand() % maxTimeBetweenNewProcsSecs;
	//		int randomNS = rand() % maxTimeBetweenNewProcsNS;
		

			//advance simClock to time for next scheduled process
	//		simClock[0] += randomNS;
	//		simClock[1] += randomSecs;
	//	}

		int OSSPid = getpid();	
		printf("OSSPid is %d\n", OSSPid);

		if ((childpid = fork()) == -1){  //only forks a child if it there was a table entry created
			perror("oss: failed to fork child");
		}
			
			//increment process counters
			totalProcessesCreated++;
			processCount++;	
			//
		printf("child pid is %d\n", childpid);
		PCBTable[PCBIndex].PCB_localSimPid = childpid;
	
		enqueue(roundRobinQueue, childpid);

		printf("PCBTable[PCBIndex].PCB_localSimPid is %d\n", PCBTable[PCBIndex].PCB_localSimPid);
		printf("Before if childpid == 0 PID in queue is %d\n", front(roundRobinQueue));
		
		
		printf("PID %d SCHEDULED\n", front(roundRobinQueue));
while (1){		
		if (childpid == 0){

			fprintf(fp,"OSS: Generating Process %d and putting it in Queue %d at time %d.%d\n", getpid(), processPriority, simClock[1], simClock[0]);
			fflush(fp);
			printf("top of if(childpid == 0)\n"); 
			execl("./user", NULL);	
		} else {
		printf("Process PID %d is running scheduleProcess and dispatch\n", getpid());	
		scheduleProcess(roundRobinQueue, PCBIndex);				
		dispatch();
		

//	wait(NULL);

		//receive a message		
			if (msgrcv(messageBoxID, &message, sizeof(message), 1, 0) == -1){
				perror("oss: Failed to received a message");
			}
		}
		printf("IN PARENT - MESSAGE RECEIVED\n");
		printf("IN PARENT, CHILD USED - %d time\n", PCBTable[message.PCBTableLocation].PCB_timeUsedLastBurst);
		printf("IN PARENT, localsimPid is %d\n", PCBTable[message.PCBTableLocation].PCB_localSimPid);
	
		//if message indicates the process terminated, set the bitVector back to 0 indicating ok to use this block location again for another process
		if(message.didTerminate == true){
			printf("didTerminate is %d\n", message.didTerminate);
			bitVector[message.PCBTableLocation] = 0;
			processCount--;
			exit(1);
			break;
		}
		//if  process did not terminate, enqueue back into the Queue
		if(message.didTerminate == false){
			printf("Process did not terminate");
			fprintf(fp, "Queuing PID %d into Queue 0\n", PCBTable[message.PCBTableLocation].PCB_localSimPid);
			fflush(fp);
			enqueue(roundRobinQueue, PCBTable[message.PCBTableLocation].PCB_localSimPid);
			printf("PID %d enqueued\n", PCBTable[message.PCBTableLocation].PCB_localSimPid);

		}
		printf("PID in queue is  %d\n", front(roundRobinQueue));
	
	

	
	if (totalProcessesCreated == processLimit)
		break;
	}
}
//	break;
sleep(5);
printf("simClock ended at %d.%d\n", simClock[1],simClock[0]);
printf("created %d processes\n", totalProcessesCreated);
//free  up memory queue and shared memory
shmdt(simClock);
shmdt(PCBTable);
shmctl(shmidPCB, IPC_RMID, NULL);
shmctl(shmidSimClock, IPC_RMID, NULL);
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
		
		shmdt(simClock);	
		shmdt(PCBTable);
		shmctl(shmidPCB, IPC_RMID, NULL);
		shmctl(shmidSimClock, IPC_RMID, NULL);
		msgctl(messageBoxID, IPC_RMID, NULL);
		wait(NULL);
		exit(0);
	}
}

//this function finds the first location in the bit array that is 0 indicating its available to accept a new ProcessControlBlock
int FindIndex(int bitVector[], int size, int value){

	int index;
	for (index = 0; index < size; index++){
		if (bitVector[index] == value){
			//printf("IN FIND INDEX - index is %d\n", index);
			break;
		} 
	}

	return index;
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
//function reads through the processControlBlock table to look for processes it can dispatch
void dispatch(){
	int dispatchTime = rand() % 9900 + 100; //range 100 to 10000
	simClock[0] += dispatchTime;
	printf("IN PARENT - DISPATCH IS SENDING A MESSAGE\n");	
	if(msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
		perror("oss: Failed to send message to child");
	}
	fprintf(fp, "Dispatching %d at time %d.%d\n", getpid(), simClock[1], simClock[0]);
	fflush(fp);
}

void scheduleProcess(Queue* queue, int PCBIndex){
	int index;
	int n = sizeof(PCBTable)/sizeof(PCBTable[0]); //get the number of elements in PCBTable
	
	//check if any processes are in the 0 queue
	if(!isEmpty(queue)){	
	//	for (index = 0; index <= n; index++){
			if(PCBTable[0].PCB_localSimPid == front(queue)){
				printf("EUREKA~\n");
				message.timeSliceAssigned = assignTimeSlice(PCBTable[0].PCB_processPriority);
				message.PCBTableLocation = 0;
				printf("IN SCHEDULER PROCESS FUNCTION - PCBTABLELOCATION IS %d\n", message.PCBTableLocation);
				printf("IN SCHEDULER - TIME SLICED ASSINGED WAS %d\n", message.timeSliceAssigned);
	//		}
			dequeue(queue);  //dequeue it when  scheduled
	//		break;	//break the for loop* 
		}
		
	}
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

