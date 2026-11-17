#define _GNU_SOURCE /* gettid, POSIX declarations under -std=c11 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "acquisitionManager.h"
#include "msg.h"
#include "iSensor.h"
#include "multitaskingAccumulator.h"
#include "iAcquisitionManager.h"
#include "debug.h"

//Capacity of the common buffer (design: 4 blocks; overridable for tests)
#ifndef ACQUISITION_BUFFER_SIZE
#define ACQUISITION_BUFFER_SIZE 4
#endif

//Slot life cycle: LIBRE -> RESERVEE -> PRETE -> EN_LECTURE -> LIBRE
typedef enum
{
	SLOT_LIBRE,
	SLOT_RESERVEE,
	SLOT_PRETE,
	SLOT_EN_LECTURE
} SlotState;

//Acquisition count (A); every concurrent access holds pCountLock.
static unsigned int produceCount = 0;
static atomic_bool pCountLock = false;

pthread_t producers[PRODUCER_COUNT];

static void *produce(void *params);

//Stable input identifier given to each producer
static unsigned int inputIds[PRODUCER_COUNT];
//Number of producers effectively created (only these are joined)
static unsigned int createdThreads = 0;
static bool producerJoined[PRODUCER_COUNT];

/**
* Common buffer and its metadata, all protected by mAcq.
* A slot is written by getInput or copied by getMessage outside the mutex,
* only while it is exclusively owned (RESERVEE or EN_LECTURE).
*/
static volatile MSG_BLOCK buffer[ACQUISITION_BUFFER_SIZE];
static SlotState states[ACQUISITION_BUFFER_SIZE];
//Stack of free slot indices
static unsigned int freeIndices[ACQUISITION_BUFFER_SIZE];
static unsigned int freeCount = 0;
//FIFO of ready slot indices, in publication order (not reservation order)
static unsigned int readyIndices[ACQUISITION_BUFFER_SIZE];
static unsigned int readyHead = 0;
static unsigned int readyCount = 0;
//Blocks removed from the buffer (D), internal counter for invariants
static unsigned int removedCount = 0;
static bool closed = false;
static bool aborted = false;

/**
* Mutex and conditions (no semaphore). Owned by this module only.
*/
static pthread_mutex_t mAcq;
static pthread_cond_t cvFree;
static pthread_cond_t cvReady;
static bool mAcqInitialized = false;
static bool cvFreeInitialized = false;
static bool cvReadyInitialized = false;

/*
* Creates the synchronization elements.
* @return ERROR_SUCCESS if the init is ok, ERROR_INIT otherwise
*/
static unsigned int createSynchronizationObjects(void);

/*
* Increments the produce count. Called during publication with mAcq held;
* counter synchronization is independent of the buffer mutex.
*/
static void incrementProducerCount(void);

static void pCountLockTake(void);
static void pCountLockRelease(void);

/*
* Reserves a free slot, waiting on cvFree if none.
* @return true with an exclusive slot, false on abort
*/
static bool reserveSlot(unsigned int *index);

/*
* Records the acquisition (A++) and makes the slot ready.
* @return true if published, false on abort (the reservation is given back)
*/
static bool publishSlot(unsigned int index);

static unsigned int createSynchronizationObjects(void)
{
	int rc;
	if ((rc = pthread_mutex_init(&mAcq, NULL)) != 0)
	{
		fprintf(stdout, "[acquisitionManager]pthread_mutex_init failed: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	mAcqInitialized = true;
	if ((rc = pthread_cond_init(&cvFree, NULL)) != 0)
	{
		fprintf(stdout, "[acquisitionManager]pthread_cond_init(cvFree) failed: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	cvFreeInitialized = true;
	if ((rc = pthread_cond_init(&cvReady, NULL)) != 0)
	{
		fprintf(stdout, "[acquisitionManager]pthread_cond_init(cvReady) failed: %s\n", strerror(rc));
		return ERROR_INIT;
	}
	cvReadyInitialized = true;
	printf("[acquisitionManager]Mutex and conditions created\n");
	return ERROR_SUCCESS;
}

static void pCountLockTake(void)
{
	bool expected = false;
	while (!atomic_compare_exchange_weak_explicit(&pCountLock, &expected, true,
		memory_order_acquire, memory_order_relaxed))
	{
		//CAS overwrites expected on failure; always retry from the free state.
		expected = false;
	}
}

static void pCountLockRelease(void)
{
	atomic_store_explicit(&pCountLock, false, memory_order_release);
}

static void incrementProducerCount(void)
{
	pCountLockTake();
	produceCount++;
	pCountLockRelease();
}

unsigned int getProducedCount(void)
{
	unsigned int p = 0;
	pCountLockTake();
	p = produceCount;
	pCountLockRelease();
	return p;
}

unsigned int getProducerCount(void)
{
	return getProducedCount();
}

static bool reserveSlot(unsigned int *index)
{
	pthread_mutex_lock(&mAcq);
	while (freeCount == 0 && !aborted)
		pthread_cond_wait(&cvFree, &mAcq);
	if (aborted)
	{
		pthread_mutex_unlock(&mAcq);
		return false;
	}
	*index = freeIndices[--freeCount];
	states[*index] = SLOT_RESERVEE;
	pthread_mutex_unlock(&mAcq);
	return true;
}

static bool publishSlot(unsigned int index)
{
	pthread_mutex_lock(&mAcq);
	if (aborted)
	{
		//Give the reservation back, no new commitment
		states[index] = SLOT_LIBRE;
		freeIndices[freeCount++] = index;
		pthread_cond_signal(&cvFree);
		pthread_mutex_unlock(&mAcq);
		return false;
	}
	incrementProducerCount();
	states[index] = SLOT_PRETE;
	readyIndices[(readyHead + readyCount) % ACQUISITION_BUFFER_SIZE] = index;
	readyCount++;
	pthread_cond_signal(&cvReady);
	pthread_mutex_unlock(&mAcq);
	return true;
}

InputStatus getMessage(MSG_BLOCK* out)
{
	unsigned int index;
	pthread_mutex_lock(&mAcq);
	while (readyCount == 0 && !closed && !aborted)
		pthread_cond_wait(&cvReady, &mAcq);
	if (aborted)
	{
		pthread_mutex_unlock(&mAcq);
		return INPUT_ABORT;
	}
	if (readyCount == 0)
	{
		//closed and empty
		pthread_mutex_unlock(&mAcq);
		return INPUT_END;
	}
	index = readyIndices[readyHead];
	readyHead = (readyHead + 1) % ACQUISITION_BUFFER_SIZE;
	readyCount--;
	removedCount++;
	states[index] = SLOT_EN_LECTURE;
	pthread_mutex_unlock(&mAcq);

	//Private copy outside the mutex: the slot is exclusively owned
	*out = buffer[index];

	pthread_mutex_lock(&mAcq);
	states[index] = SLOT_LIBRE;
	freeIndices[freeCount++] = index;
	pthread_cond_signal(&cvFree);
	pthread_mutex_unlock(&mAcq);
	return INPUT_MESSAGE;
}

unsigned int acquisitionManagerInit(void)
{
	unsigned int i;
	int rc;
	printf("[acquisitionManager]Synchronization initialization in progress...\n");
	fflush( stdout );

	//No concurrent users during initialization (all previous threads joined).
	atomic_init(&pCountLock, false);
	produceCount = 0;
	removedCount = 0;
	createdThreads = 0;
	memset(producerJoined, 0, sizeof producerJoined);
	closed = false;
	aborted = false;
	readyHead = 0;
	readyCount = 0;
	freeCount = 0;
	for (i = 0; i < ACQUISITION_BUFFER_SIZE; i++)
	{
		states[i] = SLOT_LIBRE;
		freeIndices[freeCount++] = i;
	}

	if (createSynchronizationObjects() == ERROR_INIT)
		return ERROR_INIT;

	printf("[acquisitionManager]Synchronization initialization done.\n");

	for (i = 0; i < PRODUCER_COUNT; i++)
	{
		inputIds[i] = i;
		rc = pthread_create(&producers[i], NULL, produce, &inputIds[i]);
		if (rc != 0)
		{
			printf("[acquisitionManager]pthread_create failed for producer %u: %s\n", i, strerror(rc));
			return ERROR_INIT;
		}
		createdThreads++;
	}

	return ERROR_SUCCESS;
}

unsigned int acquisitionManagerJoin(void)
{
	unsigned int status = ERROR_SUCCESS;
	for (unsigned int i = 0; i < createdThreads; i++)
	{
		if (producerJoined[i])
			continue;
		int rc = pthread_join(producers[i], NULL);
		if (rc != 0)
		{
			fprintf(stderr, "[acquisitionManager]pthread_join failed for producer %u: %s\n", i, strerror(rc));
			status = ERROR_TERMINATION;
		}
		else
			producerJoined[i] = true;
	}
	if (status == ERROR_SUCCESS)
		printf("[acquisitionManager]Producers joined\n");
	return status;
}

void acquisitionManagerClose(void)
{
	pthread_mutex_lock(&mAcq);
	closed = true;
	pthread_cond_broadcast(&cvReady);
	pthread_mutex_unlock(&mAcq);
}

void acquisitionManagerAbort(void)
{
	if (!(mAcqInitialized && cvFreeInitialized && cvReadyInitialized))
		return;
	pthread_mutex_lock(&mAcq);
	aborted = true;
	pthread_cond_broadcast(&cvFree);
	pthread_cond_broadcast(&cvReady);
	pthread_mutex_unlock(&mAcq);
}

void acquisitionManagerDestroy(void)
{
	for (unsigned int i = 0; i < createdThreads; i++)
		if (!producerJoined[i])
			return;
	if (cvReadyInitialized)
		pthread_cond_destroy(&cvReady);
	if (cvFreeInitialized)
		pthread_cond_destroy(&cvFree);
	if (mAcqInitialized)
		pthread_mutex_destroy(&mAcq);
	cvReadyInitialized = cvFreeInitialized = mAcqInitialized = false;
	printf("[acquisitionManager]Mutex and conditions cleaned\n");
}

void *produce(void* params)
{
	const unsigned int inputId = *(const unsigned int *)params;
	unsigned int index;
	D(printf("[acquisitionManager]Producer created with id %d\n", gettid()));
	unsigned int i = 0;
	while (i < PRODUCER_LOOP_LIMIT)
	{
		i++;
		sleep(PRODUCER_SLEEP_TIME+(rand() % 5));
		if (!reserveSlot(&index))
			break;
		//Fill the exclusive slot outside the mutex: producers work in parallel
		getInput(inputId, &buffer[index]);
		if (!publishSlot(index))
			break;
	}
	printf("[acquisitionManager] %d termination\n", gettid());
	return NULL;
}
