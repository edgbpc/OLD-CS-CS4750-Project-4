#include "oss.h" //contains all other includes necessary, macro definations, and function prototypes


//Global Variables


int maxTimeBetweenNewProcsNS = 50;
int maxTimeBetweenNewProcsSecs = 1;
int processTypeWeight = 95;


//file creation for log
char fileName[10] = "data.log";
FILE * fp;

pid_t childpid; //pid id for child processes

unsigned int *simClock; // simulated system clock  simClock [0] = seconds, simClock[1] = nanoseconds

ProcessControlBlock *PCBTable;

	int bitVector[25];

int main (int argc, char *argv[]) {
	printf("at top of parent\n");


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



	//message
	Message message;
	message.mesg_type = 1;	
	message.timeSliceAssigned = 55;

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

	if ((shmidPCB = shmget(keyPCB, 20*sizeof(ProcessControlBlock), IPC_CREAT | 0666)) == -1){
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

//open file for writing	
	fp = fopen(fileName, "w");

//create mail box
	if ((messageBoxID = msgget(messageQueueKey, IPC_CREAT | 0666)) == -1){
		perror("oss: Could not create child mail box");
		return 1;
	}

	

	
//	if(msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
//		perror("oss: Failed to send message to child");
//	}

	srand(time(NULL)); //seed random number generator
	
	message.PCBTableLocation = 0; //to give the first process its PCB Table location

//	while(totalProcessesCreated != processLimit ){  //keep running until process limit is hit
		
		//generate random simulated times a new process will start
		int randomSecs = rand() % maxTimeBetweenNewProcsSecs;
		int randomNS = rand() % maxTimeBetweenNewProcsNS;

		totalProcessesCreated++;
		processCount++;
		//begin testing
		printf("IN PARENT - mess_type is %ld\n", message.mesg_type); 
		//end testing
//MAIN PROCESS


		int PCBIndex = FindIndex(bitVector, 25, 0); // get first available index location 

		//populate values for ProcessControlBlock
		PCBTable[PCBIndex].PCB_testBit = 333; //testing

		//set corresponding bitVector
		bitVector[PCBIndex=] = 1;  //indicates that the corresponding index in the control table is used


		
			
			if ((childpid = fork()) == -1){
				perror("oss: failed to fork child");
			}
			if ((childpid == 0)){
				
				execl("./user", NULL);
				}
			//increment process count
			
		
wait(NULL);		
			//wait for child process to send a message to it

		//CRITICAL SECTION		
		//receive a message		
//		if (msgrcv(messageBoxID, &message, sizeof(message), 1, 0) == -1){
//			perror("oss: Failed to received a message");
//		}


//} END WHILE LOOP

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


void print_usage(){
	printf("Execution syntax oss -options value\n");
	printf("Options:\n");
	printf("-h: this help message\n");
	printf("-l: specify file name for log file.  Default: data.log\n");
	printf("-s: specify max limit of processes that can be ran.  Default: 5\n");
	printf("-t: specify max time duration in seconds.  Default: 20\n");
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
// 	BEGIN TESTING
//	int index;	
//	printf("looking for %d\n", value);
//	for (index = 0; index < 25; index++){
//			printf("bit vector is %d\n", bitVector[index]);
//			}
//	END TESTING

	int yindex;
	for (yindex = 0; yindex < size; yindex++){
		if (bitVector[yindex] == value){
			//printf("IN FIND INDEX - index is %d\n", yindex);
			break;
		}
	}

	return yindex;
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
*/
