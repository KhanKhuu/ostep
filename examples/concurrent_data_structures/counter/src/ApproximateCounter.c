#include <ApproximateCounter.h>
#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Scalable counter using per-thread local counters and periodic flushing.
 */
typedef struct
{
    tCounter_instance mBase; // base class (must be first field)
    uint32_t mGlobal;        // global count
    pthread_mutex_t mGlock;  // global count lock
    uint32_t mThreads;       // number of local counter threads
    uint32_t *mLocal;        // local counts (one per thread)
    pthread_mutex_t *mLlock; // local counts locks (one per thread)
    uint32_t mThreshold;     // update frequency
} tApproximateCounter_instance;

/**
 * @brief Initialize the approximate counter.
 *
 * @param ioBasePtr Counter base to initialize.
 * @param iInitParams Pointer to ApproximateCounterInitParams_t containing threshold and threads.
 */

static tCounter_instance *ApproximateCounter_create(const tCounter_instance *iBasePtr, const void *iOptionsPtr)
{
    uint32_t aStatusCode;
    uint32_t aThread;
    uint32_t aThreshold;
    uint32_t aThreads;
    tApproximateCounter_options *aOptionsPtr;
    tApproximateCounter_instance *aCounterPtr;

    assert(iBasePtr != NULL); // required parameter

    // get options
    if (iOptionsPtr != NULL)
    {
        aOptionsPtr = (tApproximateCounter_options *)iOptionsPtr;
        aThreshold = aOptionsPtr->mThreshold;
        aThreads = aOptionsPtr->mThreads;
    }
    else
    {
        // use defaults
        aThreshold = 1024;
        aThreads = 8;
    }

    // allocate approximate counter instance
    aCounterPtr = malloc(sizeof(tApproximateCounter_instance));
    assert(aCounterPtr != NULL);
    memset(aCounterPtr, 0, sizeof(tApproximateCounter_instance)); // blank slate

    // copy the base into the instance
    memcpy(aCounterPtr, iBasePtr, sizeof(tCounter_instance));

    // initialize counter shallow state
    aCounterPtr->mGlobal = 0;
    aCounterPtr->mThreshold = aThreshold;
    aCounterPtr->mThreads = aThreads;

    // allocate counter heap state
    aCounterPtr->mLocal = malloc(aThreads * sizeof(uint32_t));
    aCounterPtr->mLlock = malloc(aThreads * sizeof(pthread_mutex_t));

    // initialize global lock
    aStatusCode = pthread_mutex_init(&aCounterPtr->mGlock, NULL);
    assert(aStatusCode == 0);

    // initialize local locks
    for (aThread = 0; aThread < aThreads; ++aThread)
    {
        aCounterPtr->mLocal[aThread] = 0;
        aStatusCode = pthread_mutex_init(&aCounterPtr->mLlock[aThread], NULL);
        assert(aStatusCode == 0);
    }

    return (tCounter_instance *)aCounterPtr;
}

/**
 * @brief Clean up counter resources. Free all memory allocated in _create.
 *
 * @param ioInstancePtr Counter instance to destroy.
 */
static void ApproximateCounter_destroy(tCounter_instance *ioInstancePtr)
{
    uint32_t aThread;
    uint32_t aThreads;
    uint32_t *aLocal;
    pthread_mutex_t *aGlock;
    pthread_mutex_t *aLlock;
    tApproximateCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tApproximateCounter_instance *)ioInstancePtr;

    aGlock = &aCounterPtr->mGlock;
    aLocal = aCounterPtr->mLocal;
    aLlock = aCounterPtr->mLlock;
    aThreads = aCounterPtr->mThreads;
    for (aThread = 0; aThread < aThreads; ++aThread)
    {
        pthread_mutex_destroy(&aLlock[aThread]);
    }
    pthread_mutex_destroy(aGlock);
    free(aLocal);
    free(aLlock);
    free(aCounterPtr);
}

/**
 * @brief Reset all counters to zero.
 *
 * @param ioInstancePtr Counter to reset.
 */
static void ApproximateCounter_reset(tCounter_instance *ioInstancePtr)
{
    uint32_t aThread;
    uint32_t aThreads;
    tApproximateCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tApproximateCounter_instance *)ioInstancePtr;

    // get number of threads parameter of the instance
    aThreads = aCounterPtr->mThreads;

    // acquire all locks
    pthread_mutex_lock(&aCounterPtr->mGlock);
    for (aThread = 0; aThread < aThreads; ++aThread)
    {
        pthread_mutex_lock(&aCounterPtr->mLlock[aThread]);
    }
    // reset state and release locks
    aCounterPtr->mGlobal = 0;
    for (aThread = 0; aThread < aThreads; ++aThread)
    {
        aCounterPtr->mLocal[aThread] = 0;
        pthread_mutex_unlock(&aCounterPtr->mLlock[aThread]);
    }
    pthread_mutex_unlock(&aCounterPtr->mGlock);
}

/**
 * @brief Flush thread's local count to global counter.
 *
 * @param ioInstancePtr Counter instance.
 * @param iThread Thread ID to flush.
 */
static void ApproximateCounter_flush(tCounter_instance *ioInstancePtr,
                                     const uint32_t iThread)
{
    tApproximateCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tApproximateCounter_instance *)ioInstancePtr;

    pthread_mutex_lock(&aCounterPtr->mLlock[iThread]);
    pthread_mutex_lock(&aCounterPtr->mGlock);
    aCounterPtr->mGlobal += aCounterPtr->mLocal[iThread];
    pthread_mutex_unlock(&aCounterPtr->mGlock);
    aCounterPtr->mLocal[iThread] = 0;
    pthread_mutex_unlock(&aCounterPtr->mLlock[iThread]);
}

/**
 * @brief Increment thread-local counter, flushing to global when threshold is reached.
 *
 * @param ioInstancePtr Counter to update.
 * @param iThread Thread ID (0 to num_threads-1).
 * @param iAmount Amount to add to local counter.
 */
static void ApproximateCounter_increment(tCounter_instance *ioInstancePtr,
                                         const uint32_t iThread,
                                         const uint32_t iAmount)
{

    tApproximateCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tApproximateCounter_instance *)ioInstancePtr;

    pthread_mutex_lock(&aCounterPtr->mLlock[iThread]);
    aCounterPtr->mLocal[iThread] += iAmount;
    if (aCounterPtr->mLocal[iThread] >= aCounterPtr->mThreshold)
    {
        pthread_mutex_lock(&aCounterPtr->mGlock);
        aCounterPtr->mGlobal += aCounterPtr->mLocal[iThread];
        pthread_mutex_unlock(&aCounterPtr->mGlock);
        aCounterPtr->mLocal[iThread] = 0;
    }
    pthread_mutex_unlock(&aCounterPtr->mLlock[iThread]);
}

/**
 * @brief Get approximate counter value.
 *
 * Returns global count plus all unflushed local counts.
 *
 * @param ioInstancePtr Counter to read from.
 * @param oCount Pointer to write count to.
 */
static void ApproximateCounter_get(tCounter_instance *ioInstancePtr,
                                   uint32_t *oCount)
{
    tApproximateCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tApproximateCounter_instance *)ioInstancePtr;

    pthread_mutex_lock(&aCounterPtr->mGlock);
    *oCount = aCounterPtr->mGlobal;
    pthread_mutex_unlock(&aCounterPtr->mGlock);
}

const tCounter_interface gApproximateCounter_interface =
    {
        ApproximateCounter_create,
        ApproximateCounter_destroy,
        ApproximateCounter_reset,
        ApproximateCounter_flush,
        ApproximateCounter_increment,
        ApproximateCounter_get};
