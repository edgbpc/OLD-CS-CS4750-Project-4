#include "oss.h" //contains all other includes necessary, macro definations, and function prototypes


//Global Variables


int maxTimeBetweenNewProcsNS;
int maxTimeBetweenNewProcsSecs;


//file creation for log
char fileName[10] = "data.log";
FILE * fp;

pid_t childpid; //pid id for child processes

unsigned int *simClock; // simulated system clock  simClock [0] = seconds, simClock[1] = nanoseconds



int main (int argc, char *argv[]) {
	printf("at top of parent\n");


	//initalize variables
	//keys
	messageQueueKey = 59569;
	keySimClock = 59566;
	keyPCB = 59567;
	

	int maxProcess = 5; //default max processes if not specified 
	int processCount = 0; //current number of processes
	int processLimit = 100; //max number of processes allowed by assignment parameters
	int totalProcessesCreated = 0; //keeps track of all processes created	
	double terminateTime = 20;


	int bitVector[25];

	//message
	Message message;
	message.mesg_type = 1;	
	message.timeSlice = 55;

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
	simClock[0] = 55; //nanoseconds
	simClock[1] = 88; //seconds

	ProcessControlBlock *PCBTable = shmat(shmidPCB, NULL, 0);

	PCBTable[0].PCB_testBit = 1;

	printf("In Parent - PCBTable[0].PCB_testBit = %d\n", PCBTable[0].PCB_testBit);


//open file for writing	
	fp = fopen(fileName, "w");

//create mail boxes

	if ((messageBoxID = msgget(messageQueueKey, IPC_CREAT | 0666)) == -1){
		perror("oss: Could not create child mail box");
		return 1;
	}

	if(msgsnd(messageBoxID, &message, sizeof(message), 0) == -1){
		perror("oss: Failed to send message to child");
	}

	printf("IN PARENT - mess_type is %ld\n", message.mesg_type);
//MAIN PROCESS
			
			if ((childpid = fork()) == -1){
				perror("oss: failed to fork child");
			}
			if ((childpid == 0)){
					execl("./user", NULL);
				}
			//increment process count
			
		
		
			//wait for child process to send a message to it

		//CRITICAL SECTION		
		//receive a message		
//		if (msgrcv(messageBoxID, &message, sizeof(message), 1, 0) == -1){
//			perror("oss: Failed to received a message");
//		}
			


wait(NULL); // wait for all child processes to end

printf("simClock ended at %d.%d\n", simClock[1],simClock[0]);

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
		shmctl(shmidSimClock, IPC_RMID, NULL);
		msgctl(messageBoxID, IPC_RMID, NULL);
		wait(NULL);
		exit(0);
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
*/
