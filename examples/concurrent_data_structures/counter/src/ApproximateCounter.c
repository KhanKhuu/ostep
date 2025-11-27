#include <ApproximateCounter.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

void ApproximateCounter_init(ApproximateCounter_t *ioCounter, void *iInitParams)
{
    uint32_t aStatusCode;
    uint32_t aThread;
    uint32_t aThreshold;
    uint32_t aThreads;
    ApproximateCounterInitParams_t *aParams;

    // Cast void* to ApproximateCounterInitParams_t*
    aParams = (ApproximateCounterInitParams_t *)iInitParams;
    aThreshold = aParams->mThreshold;
    aThreads = aParams->mThreads;

    // initialize stack state
    ioCounter->mGlobal = 0;
    ioCounter->mThreshold = aThreshold;
    ioCounter->mThreads = aThreads;

    // allocate heap state
    ioCounter->mLocal = malloc(aThreads * sizeof(uint32_t));
    ioCounter->mLlock = malloc(aThreads * sizeof(pthread_mutex_t));

    // initialize global lock
    aStatusCode = pthread_mutex_init(&ioCounter->mGlock, NULL);
    assert(aStatusCode == 0);

    // initialize local locks
    for (aThread = 0; aThread < aThreads; ++aThread)
    {
        ioCounter->mLocal[aThread] = 0;
        aStatusCode = pthread_mutex_init(&ioCounter->mLlock[aThread], NULL);
        assert(aStatusCode == 0);
    }
}

void ApproximateCounter_update(ApproximateCounter_t *ioCounter, uint32_t iThread, int iAmount)
{

    pthread_mutex_lock(&ioCounter->mLlock[iThread]);
    ioCounter->mLocal[iThread] += iAmount;
    if (ioCounter->mLocal[iThread] >= ioCounter->mThreshold)
    {
        pthread_mutex_lock(&ioCounter->mGlock);
        ioCounter->mGlobal += ioCounter->mLocal[iThread];
        pthread_mutex_unlock(&ioCounter->mGlock);
        ioCounter->mLocal[iThread] = 0;
    }
    pthread_mutex_unlock(&ioCounter->mLlock[iThread]);
}

void ApproximateCounter_flush(ApproximateCounter_t *ioCounter, uint32_t iThread)
{
    pthread_mutex_lock(&ioCounter->mLlock[iThread]);
    pthread_mutex_lock(&ioCounter->mGlock);
    ioCounter->mGlobal += ioCounter->mLocal[iThread];
    pthread_mutex_unlock(&ioCounter->mGlock);
    ioCounter->mLocal[iThread] = 0;
    pthread_mutex_unlock(&ioCounter->mLlock[iThread]);
}

void ApproximateCounter_reset(ApproximateCounter_t *ioCounter)
{
    uint32_t aThread;
    uint32_t aThreads;

    aThreads = ioCounter->mThreads;

    // acquire all locks
    pthread_mutex_lock(&ioCounter->mGlock);
    for (aThread = 0; aThread < aThreads; ++aThread)
    {
        pthread_mutex_lock(&ioCounter->mLlock[aThread]);
    }
    // update values and release locks
    ioCounter->mGlobal = 0;
    for (aThread = 0; aThread < aThreads; ++aThread)
    {
        ioCounter->mLocal[aThread] = 0;
        pthread_mutex_unlock(&ioCounter->mLlock[aThread]);
    }
    pthread_mutex_unlock(&ioCounter->mGlock);
}

uint32_t ApproximateCounter_get(ApproximateCounter_t *iCounter)
{
    int aCount;
    pthread_mutex_lock(&iCounter->mGlock);
    aCount = iCounter->mGlobal;
    pthread_mutex_unlock(&iCounter->mGlock);
    return aCount;
}

void ApproximateCounter_destroy(ApproximateCounter_t *ioCounter)
{
    uint32_t aThread;
    uint32_t aThreads;
    uint32_t *aLocal;
    pthread_mutex_t *aGlock;
    pthread_mutex_t *aLlock;

    aGlock = &ioCounter->mGlock;
    aLocal = ioCounter->mLocal;
    aLlock = ioCounter->mLlock;
    aThreads = ioCounter->mThreads;
    for (aThread = 0; aThread < aThreads; ++aThread)
    {
        pthread_mutex_destroy(&aLlock[aThread]);
    }
    pthread_mutex_destroy(aGlock);
    free(aLocal);
    free(aLlock);
}
