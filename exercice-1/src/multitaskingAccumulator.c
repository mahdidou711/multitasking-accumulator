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

	if (status != ERROR_SUCCESS)
	{
		printf("[multitaskingAccumulator]Initialization failed, aborting the created tasks\n");
		acquisitionManagerAbort();
		messageAdderAbort();
	}
	else
	{
		printf("[multitaskingAccumulator]Task initialization done.\n");
		printf("[multitaskingAccumulator]Scheduling in progress...\n");
	}

	if (acquisitionManagerJoin() != ERROR_SUCCESS)
	{
		status = ERROR_TERMINATION;
		acquisitionManagerAbort();
		messageAdderAbort();
	}
	else if (status == ERROR_SUCCESS)
		acquisitionManagerClose();

	if (messageAdderJoin() != ERROR_SUCCESS)
	{
		status = ERROR_TERMINATION;
		acquisitionManagerAbort();
		messageAdderAbort();
	}
	if (displayManagerJoin() != ERROR_SUCCESS)
		status = ERROR_TERMINATION;

	//A non-joined thread may still access any module's shared objects.
	if (status != ERROR_TERMINATION)
	{
		displayManagerDestroy();
		messageAdderDestroy();
		acquisitionManagerDestroy();
		printf("[multitaskingAccumulator]Threads terminated\n");
	}
	else
		fprintf(stderr, "[multitaskingAccumulator]Termination failed; shared objects retained\n");

	exit(status);

}
