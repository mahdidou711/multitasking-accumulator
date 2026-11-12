#ifndef I_ACQUISITION_MANAGER_H
#define I_ACQUISITION_MANAGER_H

#include "msg.h"

/**
* Status returned by getMessage (Q6: InputStatus = {MESSAGE, END, ABORT}).
*/
typedef enum
{
	INPUT_MESSAGE, /* a private copy of a ready block was delivered */
	INPUT_END,     /* acquisition closed and no ready block left */
	INPUT_ABORT    /* acquisition aborted */
} InputStatus;

/**
* Get the number of produced messages (A: acquisitions recorded after getInput returned).
*/
unsigned int getProducedCount(void);

/**
* Public alias of getProducedCount, name used by the subject (p. 6).
*/
unsigned int getProducerCount(void);

/**
* Gets a message if any, otherwise waits for a message.
* @param out private copy of the oldest ready block (valid only for INPUT_MESSAGE)
* @return INPUT_MESSAGE, INPUT_END or INPUT_ABORT
*/
InputStatus getMessage(MSG_BLOCK* out);

#endif
