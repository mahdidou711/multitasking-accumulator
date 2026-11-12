#ifndef I_MESSAGE_ADDER_H
#define I_MESSAGE_ADDER_H

#include <stdbool.h>
#include <time.h>
#include "msg.h"

/**
* Private copy of one published sum and its version S (Q6: Publication).
*/
typedef struct
{
	MSG_BLOCK sum;
	unsigned int version;
} Publication;

/**
* Frozen S,R and the publication removed by the capture (Q6: DiagnosticCapture).
*/
typedef struct
{
	unsigned int S;
	unsigned int R;
	bool hasPending;
	Publication pending;
} DiagnosticCapture;

/**
* Q6: DisplayEvent = {PUBLICATION, DIAGNOSTIC_DUE, END, ABORT}.
*/
typedef enum
{
	DISPLAY_PUBLICATION,
	DISPLAY_DIAGNOSTIC_DUE,
	DISPLAY_END,
	DISPLAY_ABORT
} DisplayEvent;

/**
* Q6: CaptureStatus = {OK, ABORT}.
*/
typedef enum
{
	CAPTURE_OK,
	CAPTURE_ABORT
} CaptureStatus;

/**
* Gets a message that represents the current value of the sum.
*/
MSG_BLOCK getCurrentSum(void);

/**
* Get the number of consumed messages: T = S + R (published additions and rejections).
*/
unsigned int getConsumedCount(void);

/**
* Waits for the next display event. Priority: ABORT, END (closed and empty),
* DIAGNOSTIC_DUE (deadline reached on CLOCK_MONOTONIC), PUBLICATION.
* @param deadline absolute CLOCK_MONOTONIC deadline of the next diagnostic
* @param out private copy of the publication (valid only for DISPLAY_PUBLICATION)
*/
DisplayEvent nextDisplayEvent(const struct timespec* deadline, Publication* out);

/**
* Freezes S and R, copies them and removes the pending publication, if any.
* The mutex is released before return.
*/
CaptureStatus beginDiagnostic(DiagnosticCapture* out);

/**
* Ends the freeze started by beginDiagnostic and wakes the waiting commits.
*/
void endDiagnostic(void);

#endif
