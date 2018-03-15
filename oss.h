#ifndef OSS_HEADER_FILE
#define OSS_HEADER_FILE


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>


//macro :definations

#define SHM_SIZE 100
#define BILLION 1000000000L //taken from book

//prototypes
static int setperiodic(double sec);
void handle(int signo);


//structures
typedef struct {
	int PCB_testBit;
	int PCB_CPUTimeUsed;
	int PCB_totalTimeInSystem;
	int PCB_timeUsedLastBurst;
	int PCB_localSimPid;
	int PCB_processPriority;

} ProcessControlBlock;


typedef struct {
	long mesg_type; //always 1
	int timeSlice;
} Message;

//variables
//
//

int messageBoxID;
int shmidSimClock;
int shmidPCB;

//message queue key
key_t messageQueueKey;

//Shared Memory Keys
key_t keySimClock;
key_t keyPCB;




#endif

