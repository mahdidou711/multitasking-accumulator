#ifndef ACQUISITION_MANAGER_H
#define ACQUISITION_MANAGER_H

/**
* Initializes the acquisitions
* @return ERROR_SUCCESS, or ERROR_INIT (created producers must then be aborted and joined)
*/
unsigned int acquisitionManagerInit(void);

/**
* Waits that acquisitions terminate (joins only the created producers)
*/
void acquisitionManagerJoin(void);

/**
* Closes the acquisition: no more blocks; ready blocks remain readable
*/
void acquisitionManagerClose(void);

/**
* Aborts the acquisition: wakes every waiting task, no new commitment
*/
void acquisitionManagerAbort(void);

/**
* Destroys the initialized synchronization objects (after all joins)
*/
void acquisitionManagerDestroy(void);

#endif
