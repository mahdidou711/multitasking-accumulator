#ifndef MESSAGE_DISPLAY_H
#define MESSAGE_DISPLAY_H

/**
* Initializes display manager
* @return ERROR_SUCCESS, or ERROR_INIT
*/
unsigned int displayManagerInit(void);

/**
* @return ERROR_SUCCESS, or ERROR_TERMINATION (unjoined state retained)
* Waits that display manager terminates
*/
unsigned int displayManagerJoin(void);

/**
* Releases the display manager state (no synchronization object owned)
*/
void displayManagerDestroy(void);

#endif
