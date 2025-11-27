#include <TraditionalCounter.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

void TraditionalCounter_init(TraditionalCounter_t *ioCounter, void *iInitParams)
{
    uint32_t aStatusCode;

    // iInitParams is ignored for TraditionalCounter (should be NULL)
    // TraditionalCounter requires no initialization parameters

    // initialize stack state
    ioCounter->mGlobal = 0;

    // initialize global lock
    aStatusCode = pthread_mutex_init(&ioCounter->mGlock, NULL);
    assert(aStatusCode == 0);
}

void TraditionalCounter_update(TraditionalCounter_t *ioCounter, uint32_t iThread, int iAmount)
{
    pthread_mutex_lock(&ioCounter->mGlock);
    ioCounter->mGlobal += iAmount;
    pthread_mutex_unlock(&ioCounter->mGlock);
}

void TraditionalCounter_flush(TraditionalCounter_t *ioCounter, uint32_t iThread)
{
    // Do nothing
}

void TraditionalCounter_reset(TraditionalCounter_t *ioCounter)
{
    pthread_mutex_lock(&ioCounter->mGlock);
    ioCounter->mGlobal = 0;
    pthread_mutex_unlock(&ioCounter->mGlock);
}

uint32_t TraditionalCounter_get(TraditionalCounter_t *iCounter)
{
    uint32_t aCount;

    pthread_mutex_lock(&iCounter->mGlock);
    aCount = iCounter->mGlobal;
    pthread_mutex_unlock(&iCounter->mGlock);

    return aCount;
}

void TraditionalCounter_destroy(TraditionalCounter_t *ioCounter)
{
    pthread_mutex_t *aGlock;
    aGlock = &ioCounter->mGlock;
    pthread_mutex_destroy(aGlock);
}
