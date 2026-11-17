#define _GNU_SOURCE /* gettid, POSIX clocks under -std=c11 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "messageAdder.h"
#include "msg.h"
#include "iMessageAdder.h"
#include "multitaskingAccumulator.h"
#include "iAcquisitionManager.h"
#include "debug.h"

//Q6: cvDisplay uses CLOCK_MONOTONIC; no REALTIME fallback.
#if !defined(_POSIX_MONOTONIC_CLOCK) || (_POSIX_MONOTONIC_CLOCK < 0) \
	|| !defined(_POSIX_CLOCK_SELECTION) || (_POSIX_CLOCK_SELECTION < 0)
#error "Unsupported configuration: CLOCK_MONOTONIC condition variables are required"
#endif

//Q6 CommitStatus = {OK, ABORT}, private to this module
typedef enum
{
	COMMIT_OK,
	COMMIT_ABORT
} CommitStatus;

//consumer thread
pthread_t consumer;
static bool consumerCreated = false;
//Message computed: shared publication place (Q6 currentSum), protected by mAdder
volatile MSG_BLOCK out;
//Private working sum, only touched by the consumer thread
static MSG_BLOCK workingSum;
//S: published additions (version of the sum); R: rejections. Protected by mAdder.
static unsigned int sumCount = 0;
static unsigned int rejectCount = 0;
//full: one non overwritable publication is waiting for the display manager
static bool full = false;
static bool closed = false;
static bool aborted = false;
static bool diagnosticInProgress = false;

/**
 * Mutex and conditions (no semaphore). Owned by this module only.
 */
static pthread_mutex_t mAdder;
static pthread_cond_t cvDisplay; /* CLOCK_MONOTONIC */
static pthread_cond_t cvCommit;
static bool mAdderInitialized = false;
static bool cvDisplayInitialized = false;
static bool cvCommitInitialized = false;

/**
 * Consumer entry point.
 */
static void *sum( void *parameters );

/**
 * Publishes workingSum and S+1 once the place is empty and no diagnostic is running.
 */
static CommitStatus publishSum(void);

/**
 * Commits a rejection (R+1) once no diagnostic is running.
 */
static CommitStatus commitRejection(void);

/**
 * Closes the publications after the last block (wakes the display manager).
 */
static void closePublications(void);

/**
 * Creates mutex and conditions; cvDisplay is configured on CLOCK_MONOTONIC.
 */
static unsigned int createSynchronizationObjects(void);

static unsigned int createSynchronizationObjects(void)
{
	int rc;
	pthread_condattr_t attr;
	struct timespec now;

	if ((rc = pthread_mutex_init(&mAdder, NULL)) != 0)
	{
		printf("[messageAdder]pthread_mutex_init failed: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	mAdderInitialized = true;
	if ((rc = pthread_cond_init(&cvCommit, NULL)) != 0)
	{
		printf("[messageAdder]pthread_cond_init(cvCommit) failed: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	cvCommitInitialized = true;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
	{
		printf("[messageAdder]Unsupported configuration: CLOCK_MONOTONIC unavailable: %s\n", strerror(errno));
		return ERROR_INIT;
	}
	if ((rc = pthread_condattr_init(&attr)) != 0)
	{
		printf("[messageAdder]pthread_condattr_init failed: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	if (rc != 0)
	{
		printf("[messageAdder]Unsupported configuration: pthread_condattr_setclock(CLOCK_MONOTONIC) refused: %s\n", strerror(rc));
		int destroyRc = pthread_condattr_destroy(&attr);
		if (destroyRc != 0)
			fprintf(stderr, "[messageAdder]pthread_condattr_destroy failed: %s\n", strerror(destroyRc));
		return ERROR_INIT;
	}
	rc = pthread_cond_init(&cvDisplay, &attr);
	int destroyRc = pthread_condattr_destroy(&attr);
	if (destroyRc != 0)
		fprintf(stderr, "[messageAdder]pthread_condattr_destroy failed: %s\n", strerror(destroyRc));
	if (rc != 0)
	{
		printf("[messageAdder]pthread_cond_init(cvDisplay) failed: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	cvDisplayInitialized = true;
	if (destroyRc != 0)
		return ERROR_INIT;
	printf("[messageAdder]Mutex and conditions created (cvDisplay on CLOCK_MONOTONIC)\n");
	return ERROR_SUCCESS;
}

MSG_BLOCK getCurrentSum(void){
	MSG_BLOCK current;
	pthread_mutex_lock(&mAdder);
	current = out;
	pthread_mutex_unlock(&mAdder);
	return current;
}

unsigned int getConsumedCount(void){
	unsigned int t;
	pthread_mutex_lock(&mAdder);
	t = sumCount + rejectCount;
	pthread_mutex_unlock(&mAdder);
	return t;
}

static bool deadlineReached(const struct timespec* deadline)
{
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return true; /* checked at init; never expected */
	return (now.tv_sec > deadline->tv_sec)
		|| (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec);
}

DisplayEvent nextDisplayEvent(const struct timespec* deadline, Publication* pub)
{
	DisplayEvent event;
	pthread_mutex_lock(&mAdder);
	for (;;)
	{
		if (aborted)
		{
			event = DISPLAY_ABORT;
			break;
		}
		if (closed && !full)
		{
			event = DISPLAY_END;
			break;
		}
		if (deadlineReached(deadline))
		{
			event = DISPLAY_DIAGNOSTIC_DUE;
			break;
		}
		if (full)
		{
			pub->sum = out;
			pub->version = sumCount;
			full = false;
			pthread_cond_broadcast(&cvCommit);
			event = DISPLAY_PUBLICATION;
			break;
		}
		//Wakeups and ETIMEDOUT only lead to re-examine the predicates
		pthread_cond_timedwait(&cvDisplay, &mAdder, deadline);
	}
	pthread_mutex_unlock(&mAdder);
	return event;
}

CaptureStatus beginDiagnostic(DiagnosticCapture* capture)
{
	pthread_mutex_lock(&mAdder);
	if (aborted)
	{
		pthread_mutex_unlock(&mAdder);
		return CAPTURE_ABORT;
	}
	diagnosticInProgress = true;
	capture->S = sumCount;
	capture->R = rejectCount;
	capture->hasPending = full;
	if (full)
	{
		capture->pending.sum = out;
		capture->pending.version = sumCount;
		full = false;
	}
	pthread_mutex_unlock(&mAdder);
	return CAPTURE_OK;
}

void endDiagnostic(void)
{
	pthread_mutex_lock(&mAdder);
	diagnosticInProgress = false;
	pthread_cond_broadcast(&cvCommit);
	pthread_mutex_unlock(&mAdder);
}

static CommitStatus publishSum(void)
{
	pthread_mutex_lock(&mAdder);
	while ((full || diagnosticInProgress) && !aborted)
		pthread_cond_wait(&cvCommit, &mAdder);
	if (aborted)
	{
		pthread_mutex_unlock(&mAdder);
		return COMMIT_ABORT;
	}
	out = workingSum;
	sumCount++;
	full = true;
	pthread_cond_signal(&cvDisplay);
	pthread_mutex_unlock(&mAdder);
	return COMMIT_OK;
}

static CommitStatus commitRejection(void)
{
	pthread_mutex_lock(&mAdder);
	while (diagnosticInProgress && !aborted)
		pthread_cond_wait(&cvCommit, &mAdder);
	if (aborted)
	{
		pthread_mutex_unlock(&mAdder);
		return COMMIT_ABORT;
	}
	rejectCount++;
	pthread_mutex_unlock(&mAdder);
	return COMMIT_OK;
}

static void closePublications(void)
{
	pthread_mutex_lock(&mAdder);
	closed = true;
	pthread_cond_broadcast(&cvDisplay);
	pthread_mutex_unlock(&mAdder);
}

unsigned int messageAdderInit(void){
	int rc;
	out.checksum = 0;
	workingSum.checksum = 0;
	for (size_t i = 0; i < DATA_SIZE; i++)
	{
		out.mData[i] = 0;
		workingSum.mData[i] = 0;
	}
	sumCount = 0;
	rejectCount = 0;
	full = false;
	closed = false;
	aborted = false;
	diagnosticInProgress = false;
	consumerCreated = false;

	if (createSynchronizationObjects() == ERROR_INIT)
		return ERROR_INIT;

	rc = pthread_create(&consumer, NULL, sum, NULL);
	if (rc != 0)
	{
		printf("[messageAdder]pthread_create failed for consumer: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	consumerCreated = true;
	return ERROR_SUCCESS;
}

unsigned int messageAdderJoin(void){
	if (consumerCreated)
	{
		int rc = pthread_join(consumer, NULL);
		if (rc != 0)
		{
			fprintf(stderr, "[messageAdder]pthread_join failed: %s\n", strerror(rc));
			return ERROR_TERMINATION;
		}
		consumerCreated = false;
		printf("[messageAdder]Consumer joined\n");
	}
	return ERROR_SUCCESS;
}

void messageAdderAbort(void){
	if (!(mAdderInitialized && cvCommitInitialized && cvDisplayInitialized))
		return;
	pthread_mutex_lock(&mAdder);
	aborted = true;
	diagnosticInProgress = false;
	pthread_cond_broadcast(&cvCommit);
	pthread_cond_broadcast(&cvDisplay);
	pthread_mutex_unlock(&mAdder);
}

void messageAdderDestroy(void){
	if (consumerCreated)
		return;
	if (cvDisplayInitialized)
		pthread_cond_destroy(&cvDisplay);
	if (cvCommitInitialized)
		pthread_cond_destroy(&cvCommit);
	if (mAdderInitialized)
		pthread_mutex_destroy(&mAdder);
	cvDisplayInitialized = cvCommitInitialized = mAdderInitialized = false;
	printf("[messageAdder]Mutex and conditions cleaned\n");
}

static void *sum( void *parameters )
{
	(void)parameters;
	MSG_BLOCK block;
	D(printf("[messageAdder]Thread created for sum with id %d\n", gettid()));
	for (;;)
	{
		InputStatus status = getMessage(&block);
		if (status == INPUT_ABORT)
			break;
		if (status == INPUT_END)
		{
			//Every ready block has been processed: close the publications
			closePublications();
			break;
		}
		//E3: sanity check before any addition (messageCheck logs the corruption)
		if (messageCheck(&block))
		{
			messageAdd(&workingSum, &block);
			if (publishSum() == COMMIT_ABORT)
				break;
		}
		else
		{
			printf("[messageAdder]Invalid message rejected, not added to the sum\n");
			if (commitRejection() == COMMIT_ABORT)
				break;
		}
	}
	printf("[messageAdder] %d termination\n", gettid());
	return NULL;
}
