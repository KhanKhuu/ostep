#include <TraditionalCounter.h>
#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Simple thread-safe counter using global lock.
 */
typedef struct
{
    tCounter_instance mBase; // base class (must be first field)
    uint32_t mGlobal;        // global count
    pthread_mutex_t mGlock;  // global count lock
} tTraditionalCounter_instance;

/**
 * @brief Allocate and initialize the traditional counter.
 *
 * @param iBasePtr Counter base to initialize.
 * @param iOptionsPtr Ignored (should be NULL).
 * @return Pointer to new counter instance.
 */
static tCounter_instance *TraditionalCounter_create(const tCounter_instance *iBasePtr, const void *iOptionsPtr)
{
    uint32_t aStatusCode;
    tTraditionalCounter_instance *aCounterPtr;

    assert(iBasePtr != NULL); // required parameter

    // iOptionsPtr is ignored for TraditionalCounter (should be NULL)
    // TraditionalCounter requires no initialization parameters

    // allocate traditional counter instance
    aCounterPtr = malloc(sizeof(tTraditionalCounter_instance));
    assert(aCounterPtr != NULL);
    memset(aCounterPtr, 0, sizeof(tTraditionalCounter_instance)); // blank slate

    // copy the base into the instance
    memcpy(aCounterPtr, iBasePtr, sizeof(tCounter_instance));

    // initialize counter state
    aCounterPtr->mGlobal = 0;

    // initialize global lock
    aStatusCode = pthread_mutex_init(&aCounterPtr->mGlock, NULL);
    assert(aStatusCode == 0);

    return (tCounter_instance *)aCounterPtr;
}

/**
 * @brief Clean up counter resources. Free all memory allocated in _create.
 *
 * @param ioInstancePtr Counter instance to destroy.
 */
static void TraditionalCounter_destroy(tCounter_instance *ioInstancePtr)
{
    pthread_mutex_t *aGlock;
    tTraditionalCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tTraditionalCounter_instance *)ioInstancePtr;

    aGlock = &aCounterPtr->mGlock;
    pthread_mutex_destroy(aGlock);
    free(aCounterPtr);
}

/**
 * @brief Reset counter to zero.
 *
 * @param ioInstancePtr Counter to reset.
 */
static void TraditionalCounter_reset(tCounter_instance *ioInstancePtr)
{
    tTraditionalCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tTraditionalCounter_instance *)ioInstancePtr;

    pthread_mutex_lock(&aCounterPtr->mGlock);
    aCounterPtr->mGlobal = 0;
    pthread_mutex_unlock(&aCounterPtr->mGlock);
}

/**
 * @brief Flush pending updates.
 *
 * No-op for traditional counter (all updates are immediate).
 *
 * @param ioInstancePtr Counter instance.
 * @param iThread Ignored.
 */
static void TraditionalCounter_flush(tCounter_instance *ioInstancePtr, const uint32_t iThread)
{
    // Do nothing - all updates are immediate for traditional counter
}

/**
 * @brief Update counter by specified amount.
 *
 * @param ioInstancePtr Counter to update.
 * @param iThread Ignored (for API compatibility).
 * @param iAmount Amount to add to counter.
 */
static void TraditionalCounter_increment(tCounter_instance *ioInstancePtr,
                                         const uint32_t iThread,
                                         const uint32_t iAmount)
{
    tTraditionalCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tTraditionalCounter_instance *)ioInstancePtr;

    pthread_mutex_lock(&aCounterPtr->mGlock);
    aCounterPtr->mGlobal += iAmount;
    pthread_mutex_unlock(&aCounterPtr->mGlock);
}

/**
 * @brief Get current counter value.
 *
 * @param ioInstancePtr Counter to read from.
 * @param oCount Address to write count to.
 */
static void TraditionalCounter_get(tCounter_instance *ioInstancePtr,
                                   uint32_t *oCount)
{
    tTraditionalCounter_instance *aCounterPtr;

    if (ioInstancePtr == NULL)
    {
        return;
    }

    aCounterPtr = (tTraditionalCounter_instance *)ioInstancePtr;

    pthread_mutex_lock(&aCounterPtr->mGlock);
    *oCount = aCounterPtr->mGlobal;
    pthread_mutex_unlock(&aCounterPtr->mGlock);
}

const tCounter_interface gTraditionalCounter_interface =
    {
        TraditionalCounter_create,
        TraditionalCounter_destroy,
        TraditionalCounter_reset,
        TraditionalCounter_flush,
        TraditionalCounter_increment,
        TraditionalCounter_get};
