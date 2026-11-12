#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "multitaskingAccumulator.h"
#include "acquisitionManager.h"
#include "displayManager.h"
#include "messageAdder.h"
#include "debug.h"


//The entry point of the process
int main( void )
{
	unsigned int status = ERROR_SUCCESS;

	printf("[multitaskingAccumulator]Software initialization in progress...\n");
	//Checked initializations: each step only if the previous one succeeded
	if (acquisitionManagerInit() != ERROR_SUCCESS)
		status = ERROR_INIT;
	else if (messageAdderInit() != ERROR_SUCCESS)
		status = ERROR_INIT;
	else if (displayManagerInit() != ERROR_SUCCESS)
		status = ERROR_INIT;

	if (status == ERROR_SUCCESS)
	{
		printf("[multitaskingAccumulator]Task initialization done.\n");
		printf("[multitaskingAccumulator]Scheduling in progress...\n");
		//Normal closing: producers first, then drain the buffer and the publications
		acquisitionManagerJoin();
		acquisitionManagerClose();
		messageAdderJoin();
		displayManagerJoin();
	}
	else
	{
		printf("[multitaskingAccumulator]Initialization failed, aborting the created tasks\n");
		acquisitionManagerAbort();
		messageAdderAbort();
		acquisitionManagerJoin();
		messageAdderJoin();
		displayManagerJoin();
	}

	//Destroy only after every join
	displayManagerDestroy();
	messageAdderDestroy();
	acquisitionManagerDestroy();

	printf("[multitaskingAccumulator]Threads terminated\n");

	exit(status == ERROR_SUCCESS ? EXIT_SUCCESS : ERROR_INIT);

}
