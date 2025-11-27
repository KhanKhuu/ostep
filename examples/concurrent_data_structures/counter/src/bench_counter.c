#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <ApproximateCounter.h>

/**
 * @brief Thread worker context.
 */
typedef struct __BenchCounter_workerContext_t
{
    uint32_t mThread;               // Thread ID (unique among the workers in a workload)
    uint32_t mNumIncrements;        // Number of times to increment the counter
    ApproximateCounter_t *mCounter; // Shared counter for all threads to increment
} BenchCounter_workerContext_t;

/**
 * @brief Thread worker method.
 *
 * Does a single thread's work on a counter. This amounts to incrementing
 * the input counter one-million times.
 *
 * @param ioWorkerContext Input context for the thread worker.
 */
void *BenchCounter_worker(void *ioWorkerContext)
{
    uint32_t aIncrement;
    uint32_t aNumIncrements;
    uint32_t aThread;
    ApproximateCounter_t *aCounter;
    BenchCounter_workerContext_t *aWorkerContext;

    aWorkerContext = (BenchCounter_workerContext_t *)ioWorkerContext;

    aThread = aWorkerContext->mThread;
    aCounter = aWorkerContext->mCounter;
    aNumIncrements = aWorkerContext->mNumIncrements;

    for (aIncrement = 0; aIncrement < aNumIncrements; ++aIncrement)
    {
        ApproximateCounter_update(aCounter, aThread, 1);
    }

    ApproximateCounter_flush(aCounter, aThread); // flush the remaining local count
                                                 // to the global count
    return NULL;
}

/**
 * @brief Returns the current monotonic time in milliseconds.
 *
 * The returned value represents elapsed time since an unspecified starting
 * point (typically system boot). It is suitable for interval measurement and
 * benchmarking. It does not represent "wall-clock" time.
 */
static inline double now_ms()
{
    struct timespec aTimeSpec;
    clock_gettime(CLOCK_MONOTONIC, &aTimeSpec);
    return (double)aTimeSpec.tv_sec * 1000.0 +
           (double)aTimeSpec.tv_nsec / 1e6;
}

/**
 * @brief Runs a single workload.
 *
 * This function runs the workload under measurement. It creates each thread
 * and waits for all threads to finish running.
 *
 * @param iCounterDriverThread Uninitialized thread objects to create and run.
 * @param iCounterDriverInput Context for each thread worker containing counter and
 *                            thread id.
 * @param iNumThreads Number of threads to create and run.
 *
 * @return uint32_t Measured run-time of the workload.
 */
uint32_t BenchCounter_runWorkload(pthread_t *iCounterDriverThread,
                                  BenchCounter_workerContext_t *iCounterDriverInput,
                                  uint32_t iNumThreads)
{
    uint32_t aStatusCode;
    uint32_t aThread;

    // Kick off the threads
    for (aThread = 0; aThread < iNumThreads; ++aThread)
    {
        aStatusCode = pthread_create(&iCounterDriverThread[aThread],
                                     NULL,
                                     BenchCounter_worker,
                                     &iCounterDriverInput[aThread]);
        assert(aStatusCode == 0);
    }

    // Wait for threads to finish
    for (aThread = 0; aThread < iNumThreads; ++aThread)
    {
        aStatusCode = pthread_join(iCounterDriverThread[aThread],
                                   NULL);
        assert(aStatusCode == 0);
    }

    return -1;
}

/**
 * @brief Benchmark a counter.
 *
 * Sets up a multi-threaded workload to drive a counter and measures
 * timing statistics for the workload. The timing measurements are computed
 * for the entire workload (how long it takes to complete). It runs the workload
 * a number of times before beginning measurements to bring the CPU frequency up
 * to a stable value, warm the working memory and caches, and clear out any first-
 * run tasks like dynamic loading, allocator initializion, thread stack/memory
 * set-up, etc.
 *
 * @param iNumThreads Number of threads to run with.
 * @param iThreshold Approximate counter threshold (see approximate_counter.h).
 * @param iNumIncrements How many times to increment each local thread's counter.
 * @param iNumWarmups How many times to run the workload and discard the results
 *                    before taking measurements.
 * @param iNumHotRuns How many times to run the workload while taking measurements.
 *
 * @return uint32_t Median runtime of all the measured runs.
 */
uint32_t BenchCounter_benchApproximateCounter(uint8_t iNumThreads,
                                              uint32_t iThreshold,
                                              uint32_t iNumIncrements,
                                              uint32_t iNumWarmups,
                                              uint32_t iNumHotRuns)
{
    uint32_t aGlobalCount;
    uint32_t aRun;
    uint32_t aStatusCode;
    uint32_t aThread;
    double aRuntime;
    double aT0;
    double aT1;
    pthread_t *aCounterDriverThread;
    BenchCounter_workerContext_t *aWorkerContext;
    ApproximateCounter_t *aCounter;

    // Allocate heap scratch
    aCounterDriverThread = malloc(iNumThreads * sizeof(pthread_t));
    aWorkerContext = malloc(iNumThreads * sizeof(BenchCounter_workerContext_t));
    aCounter = (ApproximateCounter_t *)malloc(sizeof(ApproximateCounter_t));

    // Initialize counter
    ApproximateCounterInitParams_t aInitParams;
    aInitParams.mThreshold = iThreshold;
    aInitParams.mThreads = iNumThreads;
    ApproximateCounter_init(aCounter, &aInitParams);

    // Set up counter driver worker thread inputs
    for (aThread = 0; aThread < iNumThreads; ++aThread)
    {
        aWorkerContext[aThread].mThread = aThread;
        aWorkerContext[aThread].mNumIncrements = iNumIncrements;
        aWorkerContext[aThread].mCounter = aCounter;
    }

    // Warm-up Runs
    for (aRun = 0; aRun < iNumWarmups; ++aRun)
    {
        BenchCounter_runWorkload(aCounterDriverThread, aWorkerContext, iNumThreads);
        ApproximateCounter_reset(aCounter);
    }

    // Hot Runs
    for (aRun = 0; aRun < iNumHotRuns; ++aRun)
    {
        aT0 = now_ms();
        BenchCounter_runWorkload(aCounterDriverThread, aWorkerContext, iNumThreads);
        aT1 = now_ms();

        aRuntime = aT1 - aT0;
        aGlobalCount = ApproximateCounter_get(aCounter);
        printf("Global Count: %u\nRun Time: %f\n", aGlobalCount, aRuntime);

        ApproximateCounter_reset(aCounter);
    }

    free(aCounter);
    free(aCounterDriverThread);
    free(aWorkerContext);

    return 0;
}

int main(int argc, const char **argv)
{
    printf("Welcome to Concurrent Counter Driver\n");
    BenchCounter_benchApproximateCounter(4, 1024, 1000000, 15, 30);
    return 0;
}