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
typedef struct __tBenchCounter_context
{
    uint32_t mThread;               // Thread ID (unique among the workers in a workload)
    uint32_t mNumIncrements;        // Number of times to increment the counter
    tCounter_instance *mCounterPtr; // Shared counter for all threads to increment
} tBenchCounter_context;

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
    tCounter_instance *aCounterPtr;
    tBenchCounter_context *aWorkerContext;

    aWorkerContext = (tBenchCounter_context *)ioWorkerContext;

    aThread = aWorkerContext->mThread;
    aCounterPtr = aWorkerContext->mCounterPtr;
    aNumIncrements = aWorkerContext->mNumIncrements;

    for (aIncrement = 0; aIncrement < aNumIncrements; ++aIncrement)
    {
        gApproximateCounter_interface.mIncrementPtr(aCounterPtr, aThread, 1);
    }

    gApproximateCounter_interface.mFlushPtr(aCounterPtr, aThread); // flush the remaining local count
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
 * @param iCounterDriverThreadPtr Uninitialized thread objects to create and run.
 * @param iCounterDriverContextPtr Context for each thread worker containing counter
 *                                 instance and thread id.
 * @param iNumThreads Number of threads to create and run.
 *
 * @return uint32_t Measured run-time of the workload.
 */
uint32_t BenchCounter_runWorkload(pthread_t *iCounterDriverThreadPtr,
                                  tBenchCounter_context *iCounterDriverContextPtr,
                                  uint32_t iNumThreads)
{
    uint32_t aStatusCode;
    uint32_t aThread;

    // Kick off the threads
    for (aThread = 0; aThread < iNumThreads; ++aThread)
    {
        aStatusCode = pthread_create(&iCounterDriverThreadPtr[aThread],
                                     NULL,
                                     BenchCounter_worker,
                                     &iCounterDriverContextPtr[aThread]);
        assert(aStatusCode == 0);
    }

    // Wait for threads to finish
    for (aThread = 0; aThread < iNumThreads; ++aThread)
    {
        aStatusCode = pthread_join(iCounterDriverThreadPtr[aThread],
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
    pthread_t *aCounterDriverThreadPtr;
    tBenchCounter_context *aContextPtr;
    tCounter_instance aBasePtr;
    tCounter_instance *aCounterPtr;
    tApproximateCounter_options aOptions;

    // Allocate heap scratch
    aCounterDriverThreadPtr = malloc(iNumThreads * sizeof(pthread_t));
    aContextPtr = malloc(iNumThreads * sizeof(tBenchCounter_context));

    // Create counter
    aBasePtr.mCounterId = 0;
    aOptions.mThreshold = iThreshold;
    aOptions.mThreads = iNumThreads;
    aCounterPtr = gApproximateCounter_interface.mCreatePtr(&aBasePtr, &aOptions);

    // Set up counter driver worker thread inputs
    for (aThread = 0; aThread < iNumThreads; ++aThread)
    {
        aContextPtr[aThread].mThread = aThread;
        aContextPtr[aThread].mNumIncrements = iNumIncrements;
        aContextPtr[aThread].mCounterPtr = aCounterPtr;
    }

    // Warm-up Runs
    for (aRun = 0; aRun < iNumWarmups; ++aRun)
    {
        BenchCounter_runWorkload(aCounterDriverThreadPtr, aContextPtr, iNumThreads);
        gApproximateCounter_interface.mResetPtr(aCounterPtr);
    }

    // Hot Runs
    for (aRun = 0; aRun < iNumHotRuns; ++aRun)
    {
        aT0 = now_ms();
        BenchCounter_runWorkload(aCounterDriverThreadPtr, aContextPtr, iNumThreads);
        aT1 = now_ms();

        aRuntime = aT1 - aT0;
        gApproximateCounter_interface.mGetPtr(aCounterPtr, &aGlobalCount);
        printf("Global Count: %u\nRun Time: %f\n", aGlobalCount, aRuntime);

        gApproximateCounter_interface.mResetPtr(aCounterPtr);
    }

    // free memory
    gApproximateCounter_interface.mDestroyPtr(aCounterPtr);
    free(aCounterDriverThreadPtr);
    free(aContextPtr);

    return 0;
}

int main(int argc, const char **argv)
{
    printf("Welcome to Concurrent Counter Driver\n");
    BenchCounter_benchApproximateCounter(4, 1024, 1000000, 15, 30);
    return 0;
}