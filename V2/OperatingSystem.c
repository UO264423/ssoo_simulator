#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int, int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();

void OperatingSystem_HandleClockInterrupt();

void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_AddBlockedQueue(int);
int OperatingSystem_ExtractFromBlocked();
int OperatingSystem_CheckPriority(int process);

//V1.10.a
char * statesNames [5]={"NEW","READY","EXECUTING","BLOCKED","EXIT"};

// In OperatingSystem.c
heapItem readyToRunQueue [NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES]={0,0};
char * queueNames [NUMBEROFQUEUES]={"USER","DAEMONS"}; 

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID=NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
//Ejercicio v1.8
int initialPID=PROCESSTABLEMAXSIZE-1;

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 


// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

//Ejercicio V2.4
int numberOfClockInterrupts=0;

// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sort by when to wakeup
heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses=0; 

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code
	programFile=fopen("OperatingSystemCode", "r");
	if (programFile==NULL){
		//Ejercicio V2.1
		OperatingSystem_ShowTime(SHUTDOWN);
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing Operating System!\n");
		exit(1);		
	}

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(programFile);

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++){
		processTable[i].busy=0;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+2);
		
	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);
	
	// Create all user processes from the information given in the command line
	//Se le asigna la variable en el ejercicio V1.15
	int numberOfUserProcess = OperatingSystem_LongTermScheduler();
	
	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")) {
		//Ejercicio V2.1
		OperatingSystem_ShowTime(SHUTDOWN);
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing SIP program!\n");
		exit(1);		
	}
	//Ejercicio V1.15
	if(numberOfUserProcess<1){
		OperatingSystem_ReadyToShutdown();
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
int OperatingSystem_PrepareStudentsDaemons(int programListDaemonsBase) {

	// Prepare aditionals daemons here
	// index for aditionals daemons program in programList
	// programList[programListDaemonsBase]=(PROGRAMS_DATA *) malloc(sizeof(PROGRAMS_DATA));
	// programList[programListDaemonsBase]->executableName="studentsDaemonNameProgram";
	// programList[programListDaemonsBase]->arrivalTime=0;
	// programList[programListDaemonsBase]->type=DAEMONPROGRAM; // daemon program
	// programListDaemonsBase++

	return programListDaemonsBase;
};


// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command line and daemons programs
int OperatingSystem_LongTermScheduler() {
	
  
	int PID, i,
		numberOfSuccessfullyCreatedProcesses=0;
	
	for (i=0; programList[i]!=NULL && i<PROGRAMSMAXNUMBER ; i++) {
		PID = OperatingSystem_CreateProcess(i);
		

		//V1.4b
		if(PID == NOFREEENTRY){
			//Ejercicio V2.1
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(103,ERROR,programList[i]->executableName);
			OperatingSystem_ReadyToShutdown();
		}

		//V1.5b
		else if(PID == PROGRAMDOESNOTEXIST){
			//Ejercicio V2.1
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104,ERROR,programList[i]->executableName, "it does not exist");
			OperatingSystem_ReadyToShutdown();
		}

		//V1.5c
		else if(PID == PROGRAMNOTVALID){
			//Ejercicio V2.1
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104,ERROR,programList[i]->executableName, "invalid priority or size");
			OperatingSystem_ReadyToShutdown();
		}

		//V1.6.B
		else if(PID == TOOBIGPROCESS){
			//Ejercicio V2.1
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(105,ERROR,programList[i]->executableName);
			OperatingSystem_ReadyToShutdown();
		}

		else{
			numberOfSuccessfullyCreatedProcesses++;

			if (programList[i]->type==USERPROGRAM){
				numberOfNotTerminatedUserProcesses++;
				// Move process to the ready state
				OperatingSystem_MoveToTheREADYState(PID, USERPROCESSQUEUE);
			}
			else{
				OperatingSystem_MoveToTheREADYState(PID, DAEMONSQUEUE);
			}
		}
	}
	//Ejercicio V2.7
	if (numberOfSuccessfullyCreatedProcesses > 0 ){
		OperatingSystem_PrintStatus();
	}
	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}


// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {
  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	int loadProgram;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID=OperatingSystem_ObtainAnEntryInTheProcessTable();

	//Ejercicio 1.4
	if(PID == NOFREEENTRY){
		return NOFREEENTRY;
	}
	// Check if programFile exists
	programFile=fopen(executableProgram->executableName, "r");
	//1.5.b
	if (programFile==NULL){
		return PROGRAMDOESNOTEXIST;
	}
	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(programFile);	

	//Ejercicio 1.5.c
	if(processSize == PROGRAMNOTVALID){
		return PROGRAMNOTVALID;
	}
	//Ejercicio 1.5.c
	else if(processSize == PROGRAMDOESNOTEXIST){
		return PROGRAMDOESNOTEXIST;
	}

	// Obtain the priority for the process
	priority=OperatingSystem_ObtainPriority(programFile);
	
	// Obtain enough memory space
 	loadingPhysicalAddress=OperatingSystem_ObtainMainMemory(processSize, PID);

	//Ejercicio 1.6.b
	if (loadingPhysicalAddress == TOOBIGPROCESS){
		return TOOBIGPROCESS;
	}

	//1.7.a
	if(OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize) == TOOBIGPROCESS){
		return TOOBIGPROCESS;
	}

	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	//Ejercicio V2.1
	OperatingSystem_ShowTime(INIT);
	// Show message "Process [PID] created from program [executableName]\n"
	ComputerSystem_DebugMessage(70,INIT,PID,executableProgram->executableName);
	
	return PID;
}

//Ejercicio 1.9.A


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {

 	if (processSize>MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;
	
 	return PID*MAINMEMORYSECTIONSIZE;
}


// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {
	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	

	//Ejercicio V2.1
	OperatingSystem_ShowTime(SYSPROC);
	// V1.10.b
	ComputerSystem_DebugMessage(111,SYSPROC,PID,programList[processTable[PID].programListIndex]->executableName,"NEW");

	processTable[PID].state=NEW;
	//Ejercicio V2.5a
	processTable[PID].whenToWakeUp = 0;
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;
	//Ejercicio V1.12
	processTable[PID].copyOfAccumulator = 0;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		//SolucionFallo V2
		processTable[PID].queueID = DAEMONSQUEUE;
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;
	} 
	else {
		//SolucionFallo V2
		processTable[PID].queueID = USERPROCESSQUEUE;
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;

	}

}


// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID, int queueID) {
	if (Heap_add(PID, readyToRunQueue[queueID],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[queueID] ,PROCESSTABLEMAXSIZE)>=0) {
		//Ejercicio V2.1
		OperatingSystem_ShowTime(SYSPROC);
		
		//V1.10.b
		ComputerSystem_DebugMessage(110,SYSPROC,PID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[processTable[PID].state],"READY");
		processTable[PID].state=READY;

	} 
	//Comentado por la V2.8
	//OperatingSystem_PrintReadyToRunQueue();
}


// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess;

	selectedProcess=OperatingSystem_ExtractFromReadyToRun();
	
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun() {
  
	int selectedProcess=NOPROCESS;

	// V.11c
	if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] >=1){
		selectedProcess = Heap_poll(readyToRunQueue[USERPROCESSQUEUE], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
	}
	// La cola de procesos de usuario esta vacia
	else { 
		if (numberOfReadyToRunProcesses[DAEMONSQUEUE] >=1 ){ 
			selectedProcess = Heap_poll(readyToRunQueue[DAEMONSQUEUE], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[DAEMONSQUEUE]);
		}
	}
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}


// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	//Ejercicio V2.1
	OperatingSystem_ShowTime(SYSPROC);
	//V1.10.b
	ComputerSystem_DebugMessage(110,SYSPROC,PID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[processTable[PID].state],"EXECUTING");
	// Change the process' state
	processTable[PID].state=EXECUTING;
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}

// In OperatingSystem.c Exercise 2-b of V2
void OperatingSystem_HandleClockInterrupt(){ 
	//Ejercicio V2.4
	OperatingSystem_ShowTime(INTERRUPT);
	numberOfClockInterrupts = numberOfClockInterrupts+1;
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts); //OperatingSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts)
	//Ejercicio V2.6
	int unlocked = 0;
	int locked = 0;
	int readyProcess = 0;
	int process = Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses);
	
	

	//Ejercicio V2.6
	while ( (processTable[process].whenToWakeUp == numberOfClockInterrupts) && (numberOfSleepingProcesses > 0)){
		readyProcess = OperatingSystem_ExtractFromBlocked(); 
		OperatingSystem_MoveToTheREADYState(readyProcess, processTable[readyProcess].queueID); 
		unlocked = 1; 
		locked = 1;
		process = Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses); 
	}
	if (unlocked){
		OperatingSystem_PrintStatus();
	}
	//Ejercicio V2.6
	if (locked){
			int new = Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE], numberOfReadyToRunProcesses[USERPROCESSQUEUE]); 
			int priority = OperatingSystem_CheckPriority(new);
			if (priority){
				OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
				ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, new, programList[processTable[new].programListIndex]->executableName);
				OperatingSystem_PreemptRunningProcess();
				OperatingSystem_ShortTermScheduler();
				OperatingSystem_Dispatch(new);
				//Ejercicio V2.6
				OperatingSystem_PrintStatus();
			}

	}
} 


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);
	//Ejercicio V1.13
	Processor_SetAccumulator(processTable[PID].copyOfAccumulator);
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
void OperatingSystem_PreemptRunningProcess() {

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state 11.c
	OperatingSystem_MoveToTheREADYState(executingProcessID,processTable[executingProcessID].queueID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}


// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID) {
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-1);
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-2);

	//Ejercicio V1.13c
	processTable[PID].copyOfAccumulator = Processor_GetAccumulator();
	
}


// Exception management routine
void OperatingSystem_HandleException() {
	//Ejercicio V2.1
	OperatingSystem_ShowTime(SYSPROC);
  
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(71,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
	
	OperatingSystem_TerminateProcess();

	//Ejercicio V2.7
	OperatingSystem_PrintStatus();
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {
  
	//Ejercicio V2.1
	OperatingSystem_ShowTime(SYSPROC);
	int selectedProcess;
	

	//V1.10.b
	ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[processTable[executingProcessID].state],"EXIT");
  	
	processTable[executingProcessID].state=EXIT;
	
	if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	if (numberOfNotTerminatedUserProcesses==0) {
		if (executingProcessID==sipID) {
			// finishing sipID, change PC to address of OS HALT instruction
			OperatingSystem_TerminatingSIP();
			//Ejercicio V2.1
			OperatingSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(99,SHUTDOWN,"The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID, queue, nextProcess, whenToWakeUpTemp ;

	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterA();
	
	switch (systemCallID) {
		case SYSCALL_PRINTEXECPID:
			//Ejercicio V2.1
			OperatingSystem_ShowTime(SYSPROC);
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			ComputerSystem_DebugMessage(72,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_END:
			//Ejercicio V2.1
			OperatingSystem_ShowTime(SYSPROC);
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			ComputerSystem_DebugMessage(73,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateProcess();
			//Ejercicio V2.7
			OperatingSystem_PrintStatus();
			break;

		//Ejercicio V1.4
		case SYSCALL_YIELD:
			queue = processTable[executingProcessID].queueID;
			nextProcess = Heap_getFirst(readyToRunQueue[queue], numberOfReadyToRunProcesses[queue]);
			
			
			// Si hay algo en la cola
			if (numberOfReadyToRunProcesses[queue] > 0){
				if (processTable[nextProcess].priority  == processTable[executingProcessID].priority ){
					//Ejercicio V2.1
					OperatingSystem_ShowTime(SYSPROC);
					ComputerSystem_DebugMessage(115, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, nextProcess, programList[processTable[processTable[nextProcess].priority].programListIndex]->executableName);
					// Detiene el proceso en ejecucion
					OperatingSystem_PreemptRunningProcess(); 
					//Nuevo proceso
					OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
					//Ejercicio V2.7
					OperatingSystem_PrintStatus();
					}
				}
			break;

		//Ejercicio V2.5d
		case SYSCALL_SLEEP:
			
			whenToWakeUpTemp = 1 + numberOfClockInterrupts + Processor_GetAccumulator();
			if(whenToWakeUpTemp >=0){//Comprobamos que no sea negativo
				processTable[executingProcessID].whenToWakeUp=whenToWakeUpTemp;
			}
			else{
				processTable[executingProcessID].whenToWakeUp=whenToWakeUpTemp * -1;
			}
			
			OperatingSystem_AddBlockedQueue(executingProcessID);
			OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
			//Ejercicio V2.5e
			OperatingSystem_PrintStatus();
			break;

	}
}
	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();
			break;
		//Ejercicio V2.2c
		case CLOCKINT_BIT: // CLOCKINT_BIT=9
			OperatingSystem_HandleClockInterrupt();
			break;
	}
}

	int OperatingSystem_CheckQueues(){
		if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0){ 
			return USERPROCESSQUEUE;
		}
		if (numberOfReadyToRunProcesses[DAEMONSQUEUE] > 0){ 
			return DAEMONSQUEUE;
		}
		return -1;
	}	


int OperatingSystem_CheckPriority(int process){
	if ((processTable[executingProcessID].queueID != processTable[process].queueID) && (processTable[executingProcessID].queueID == DAEMONSQUEUE)){
		return 1;
	}
	if ((processTable[executingProcessID].queueID == processTable[process].queueID) && (processTable[process].priority < processTable[executingProcessID].priority)){
		return 1;
	}
	
	return 0;
}


//V1.8
void OperatingSystem_PrintReadyToRunQueue(){
	/**
	//Ready-to-run_processes_queue:
	ComputerSystem_DebugMessage(106,SHORTTERMSCHEDULE);
	//Tabulador
	ComputerSystem_DebugMessage(107,SHORTTERMSCHEDULE);
	//Comprobamos si la lista esta vacia
	if(numberOfReadyToRunProcesses!=0){
		for(int i=0; i < numberOfReadyToRunProcesses; i++ ){
			//Distinguimos si es el ultimo elemento o si no, para asi imprimirlo sin coma o no.
			if(i!= numberOfReadyToRunProcesses-1){
				ComputerSystem_DebugMessage(108, readyToRunQueue[i].info,readyToRunQueue[i].insertionOrder);
			}
			else{
				ComputerSystem_DebugMessage(109, readyToRunQueue[i].info,readyToRunQueue[i].insertionOrder);
			}
		}
	}
	*/

	//Se comenta lo de arriba para el EJERCICIO 11.b
	//Ejercicio V2.1
	OperatingSystem_ShowTime(SYSPROC);
	//Ready-to-run_processes_queue:
	ComputerSystem_DebugMessage(106,SHORTTERMSCHEDULE);
	for(int queue=0; queue < NUMBEROFQUEUES; queue++){
		//Comprobamos que la cola no esta vacia
		if(numberOfReadyToRunProcesses[queue] != 0 ){ 
			ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, queueNames[queue]);
			//Todos los elementos de la cola
			for(int i=0;i<numberOfReadyToRunProcesses[queue];i++){
				int priority = processTable[readyToRunQueue[queue][i].info].priority;

				if(i==0){
					if(i==numberOfReadyToRunProcesses[queue]+1){
						//Si solo hay uno
						ComputerSystem_DebugMessage(113, SHORTTERMSCHEDULE, readyToRunQueue[queue][i].info, priority);
					}
					else{
						ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, readyToRunQueue[queue][i].info, priority);
					}
				}
				else{
					if(i==numberOfReadyToRunProcesses[queue]+1)
						//Imprime el proceso como primero.
						ComputerSystem_DebugMessage(109, SHORTTERMSCHEDULE, readyToRunQueue[queue][i].info, priority);
					else
						//Lo imprime con una coma delante, porque es un elemento del medio
						ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, readyToRunQueue[queue][i].info, priority);
				}
			}
		}
		else{
			//La cola esta vacia.
			ComputerSystem_DebugMessage(114, SHORTTERMSCHEDULE, queueNames[queue]);
		}
	}
}
	//Ejercicio 5d
	void OperatingSystem_AddBlockedQueue(int executingProcessID){
		OperatingSystem_SaveContext(executingProcessID);
		if (Heap_add(executingProcessID, sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses, PROCESSTABLEMAXSIZE)>-1) {
			//Ejercicio V2.1
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(110, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, statesNames[processTable[executingProcessID].state], statesNames[3]);
			processTable[executingProcessID].state = BLOCKED;
		}
		executingProcessID = NOPROCESS;
	}

	//--------------------Ejercicio V2.6------------------------
	// Ejercicio V2.6a
	int OperatingSystem_ExtractFromBlocked(){
		if (numberOfSleepingProcesses > 0){
			return Heap_poll(sleepingProcessesQueue, QUEUE_PRIORITY, &numberOfSleepingProcesses);
		}else{
			return NOPROCESS;
		}
	}
//-------------------FIN Ejercicio V2.6------------------------


