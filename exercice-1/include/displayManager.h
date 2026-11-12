#ifndef MESSAGE_DISPLAY_H
#define MESSAGE_DISPLAY_H

/**
* Initializes display manager
* @return ERROR_SUCCESS, or ERROR_INIT
*/
unsigned int displayManagerInit(void);

/**
* Waits that display manager terminates
*/
void displayManagerJoin(void);

/**
* Releases the display manager state (no synchronization object owned)
*/
void displayManagerDestroy(void);

#endif
