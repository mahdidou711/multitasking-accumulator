#define _GNU_SOURCE /* gettid, POSIX clocks under -std=c11 */
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "displayManager.h"
#include "iDisplay.h"
#include "iAcquisitionManager.h"
#include "iMessageAdder.h"
#include "msg.h"
#include "multitaskingAccumulator.h"
#include "debug.h"

// DisplayManager thread.
pthread_t displayThread;
static bool displayCreated = false;

/**
 * Private state of the display thread (only this thread reads or writes it).
 * L: version of the last sum emitted on IDisplay.messageDisplay.
 */
static unsigned int lastVersion = 0;
static bool hasDisplayed = false;

/**
 * Display manager entry point.
 * */
static void *display( void *parameters );

/**
 * Emits one published sum, then records its version L.
 */
static void emitPublication(Publication* pub);

/**
 * Coherent diagnostic: print(A, S+R) and the complement A,S,R,B (same snapshot).
 * @param final true for the final balance after the last version
 */
static CaptureStatus emitDiagnostic(bool final);

unsigned int displayManagerInit(void){
	int rc;
	lastVersion = 0;
	hasDisplayed = false;
	displayCreated = false;
	rc = pthread_create(&displayThread, NULL, display, NULL);
	if (rc != 0)
	{
		printf("[displayManager]pthread_create failed: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	displayCreated = true;
	return ERROR_SUCCESS;
}

void displayManagerJoin(void){
	if (displayCreated)
	{
		pthread_join(displayThread, NULL);
		displayCreated = false;
		printf("[displayManager]Display joined\n");
	}
}

void displayManagerDestroy(void){
	//No synchronization object is owned by this module
	lastVersion = 0;
	hasDisplayed = false;
}

static void emitPublication(Publication* pub)
{
	messageDisplay(&pub->sum);
	lastVersion = pub->version;
	hasDisplayed = true;
}

static CaptureStatus emitDiagnostic(bool final)
{
	DiagnosticCapture capture;
	unsigned int acquired;

	//1. Freeze S,R and take the pending publication, in one section
	if (beginDiagnostic(&capture) == CAPTURE_ABORT)
		return CAPTURE_ABORT;
	//2. Align the displayed sum on S (at most one emission)
	if (capture.hasPending)
	{
		emitPublication(&capture.pending);
	}
	else if (!hasDisplayed && capture.S == 0)
	{
		Publication zero;
		memset(&zero, 0, sizeof zero);
		emitPublication(&zero);
	}
	//3. Single read of A while S,R are frozen
	acquired = getProducedCount();
	//4. Release the commitments before printing
	endDiagnostic();
	//5. Diagnostic through IDisplay, then the explicit complement (same snapshot)
	print(acquired, capture.S + capture.R);
	printf("[displayManager]%s: A=%u S=%u R=%u B=%u\n",
		final ? "Final diagnostic" : "Diagnostic",
		acquired, capture.S, capture.R, acquired - capture.S - capture.R);
	if (lastVersion != capture.S)
		printf("[displayManager]Internal error: displayed version %u differs from S=%u\n", lastVersion, capture.S);
	fflush(stdout);
	return CAPTURE_OK;
}

static void addSeconds(struct timespec* result, const struct timespec* origin, unsigned long seconds)
{
	result->tv_sec = origin->tv_sec + (time_t)seconds;
	result->tv_nsec = origin->tv_nsec;
}

static void *display( void *parameters )
{
	(void)parameters;
	struct timespec t0, deadline, now;
	unsigned long tick = 1;
	Publication pub;
	bool running = true;
	D(printf("[displayManager]Thread created for display with id %d\n", gettid()));

	//Diagnostic grid t0 + k*DISPLAY_SLEEP_TIME, k >= 1, on the monotonic clock
	clock_gettime(CLOCK_MONOTONIC, &t0);
	addSeconds(&deadline, &t0, tick * DISPLAY_SLEEP_TIME);

	while (running)
	{
		switch (nextDisplayEvent(&deadline, &pub))
		{
		case DISPLAY_PUBLICATION:
			emitPublication(&pub);
			break;
		case DISPLAY_DIAGNOSTIC_DUE:
			if (emitDiagnostic(false) == CAPTURE_ABORT)
			{
				running = false;
				break;
			}
			//Next tick strictly after the end of the diagnostic: no burst
			clock_gettime(CLOCK_MONOTONIC, &now);
			do
			{
				tick++;
				addSeconds(&deadline, &t0, tick * DISPLAY_SLEEP_TIME);
			} while (deadline.tv_sec < now.tv_sec
				|| (deadline.tv_sec == now.tv_sec && deadline.tv_nsec <= now.tv_nsec));
			break;
		case DISPLAY_END:
			//Last version already emitted: final balance without waiting the deadline
			emitDiagnostic(true);
			running = false;
			break;
		case DISPLAY_ABORT:
		default:
			running = false;
			break;
		}
	}
	printf("[displayManager] %d termination\n", gettid());
	return NULL;
}
