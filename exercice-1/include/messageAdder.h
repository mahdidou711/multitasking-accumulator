#ifndef MESSAGE_ADDER_H
#define MESSAGE_ADDER_H

/**
* Initializes message adder module
* @return ERROR_SUCCESS, or ERROR_INIT
*/
unsigned int messageAdderInit(void);

/**
* Waits that message adder terminates
*/
void messageAdderJoin(void);

/**
* Aborts the message adder: wakes every waiting task, no new commitment
*/
void messageAdderAbort(void);

/**
* Destroys the initialized synchronization objects (after all joins)
*/
void messageAdderDestroy(void);

#endif
