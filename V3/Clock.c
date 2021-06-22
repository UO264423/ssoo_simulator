#include "Clock.h"
#include "Processor.h"
#include "ComputerSystemBase.h"

int tics=0;


void Clock_Update() {

	tics++;
    // ComputerSystem_DebugMessage(97,CLOCK,tics);
	//Ejercicio V2.2d
	int raise = tics % intervalBetweenInterrupts;
	//Cada 5 intervalBetween... ticks
	if(raise == 0){
		Processor_RaiseInterrupt(CLOCKINT_BIT);
	}
}


int Clock_GetTime() {

	return tics;
}
