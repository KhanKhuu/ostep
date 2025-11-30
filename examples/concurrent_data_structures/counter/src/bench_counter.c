#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <ApproximateCounter.h>
#include <TraditionalCounter.h>

enum
{
    kBenchCounter_idxApprox = 0,
    kBenchCounter_idxTrad,
    kBenchCounter_idxCount
};

typedef struct
{
    const tCounter_interface *mInterfacePtr;
    uint32_t mComponentId;
} tBenchCounter_DUT;

static const tBenchCounter_DUT sBenchCounter_DUTs[] =
    {
        {&gApproximateCounter_interface,
         kBenchCounter_idxApprox},
        {&gTraditionalCounter_interface,
         kBenchCounter_idxTrad}};

/**
 * @brief Thread worker context.
 */
typedef struct __tBenchCounter_context
{
    uint32_t mThread;                        // Thread ID (unique among the workers in a workload)
    uint32_t mNumIncrements;                 // Number of times to increment the counter
    tCounter_instance *mCounterPtr;          // Shared counter for all threads to increment
    const tCounter_interface *mInterfacePtr; // Interface to use with the counter instance
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
    const tCounter_interface *aInterfacePtr;
    tBenchCounter_context *aWorkerContext;

    aWorkerContext = (tBenchCounter_context *)ioWorkerContext;

    aThread = aWorkerContext->mThread;
    aCounterPtr = aWorkerContext->mCounterPtr;
    aInterfacePtr = aWorkerContext->mInterfacePtr;
    aNumIncrements = aWorkerContext->mNumIncrements;

    for (aIncrement = 0; aIncrement < aNumIncrements; ++aIncrement)
    {
        aInterfacePtr->mIncrementPtr(aCounterPtr, aThread, 1);
    }

    aInterfacePtr->mFlushPtr(aCounterPtr, aThread); // flush the remaining local count
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
                                              uint32_t iNumHotRuns,
                                              FILE *iOutputFile)
{
    uint32_t aGlobalCount;
    uint32_t aRun;
    uint32_t aStatusCode;
    uint32_t aThread;
    uint32_t aDut;
    double aRuntime;
    double aT0;
    double aT1;
    pthread_t *aCounterDriverThreadPtr;
    tBenchCounter_context *aContextPtr;
    tCounter_instance aBasePtr;
    tCounter_instance *aCounterPtr;
    tApproximateCounter_options aOptions;

    // Counter type descriptors
    const char *aCounterNames[] = {"approximate", "traditional"};

    for (aDut = kBenchCounter_idxApprox; aDut < kBenchCounter_idxCount; ++aDut)
    {
        // Allocate heap scratch
        aCounterDriverThreadPtr = malloc(iNumThreads * sizeof(pthread_t));
        assert(aCounterDriverThreadPtr != NULL);
        aContextPtr = malloc(iNumThreads * sizeof(tBenchCounter_context));
        assert(aContextPtr != NULL);

        // Create counter
        aBasePtr.mCounterId = 0;
        aOptions.mThreshold = iThreshold;
        aOptions.mThreads = iNumThreads;
        aCounterPtr =
            sBenchCounter_DUTs[aDut].mInterfacePtr->mCreatePtr(&aBasePtr,
                                                               &aOptions);
        assert(aCounterPtr != NULL);

        // Set up counter driver worker thread inputs
        for (aThread = 0; aThread < iNumThreads; ++aThread)
        {
            aContextPtr[aThread].mThread = aThread;
            aContextPtr[aThread].mNumIncrements = iNumIncrements;
            aContextPtr[aThread].mCounterPtr = aCounterPtr;
            aContextPtr[aThread].mInterfacePtr =
                sBenchCounter_DUTs[aDut].mInterfacePtr;
        }

        // Warm-up Runs
        for (aRun = 0; aRun < iNumWarmups; ++aRun)
        {
            BenchCounter_runWorkload(aCounterDriverThreadPtr, aContextPtr, iNumThreads);
            sBenchCounter_DUTs[aDut].mInterfacePtr->mResetPtr(aCounterPtr);
        }

        // Hot Runs
        for (aRun = 0; aRun < iNumHotRuns; ++aRun)
        {
            aT0 = now_ms();
            BenchCounter_runWorkload(aCounterDriverThreadPtr, aContextPtr, iNumThreads);
            aT1 = now_ms();

            aRuntime = aT1 - aT0;
            sBenchCounter_DUTs[aDut].mInterfacePtr->mGetPtr(aCounterPtr,
                                                            &aGlobalCount);
            fprintf(iOutputFile, "%s,%u,%u,%f,%u\n", aCounterNames[aDut], iNumThreads, iThreshold, aRuntime, aGlobalCount);

            sBenchCounter_DUTs[aDut].mInterfacePtr->mResetPtr(aCounterPtr);
        }

        // free memory
        sBenchCounter_DUTs[aDut].mInterfacePtr->mDestroyPtr(aCounterPtr);
        free(aCounterDriverThreadPtr);
        free(aContextPtr);
    }

    return 0;
}

int main(int argc, const char **argv)
{
    printf("Welcome to Concurrent Counter Driver\n");

    // Parameters for benchmark
    uint32_t iThreshold = 4096;
    uint32_t iNumHotRuns = 30;
    uint8_t threadCounts[] = {1, 2, 4, 8, 16};
    int numThreadSweeps = sizeof(threadCounts) / sizeof(threadCounts[0]);

    // Get current wall-clock time for folder and file naming
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[64];
    char folder_name[128];
    char filename[256];
    char filepath[384];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);

    // Create benchmark folder
    snprintf(folder_name, sizeof(folder_name), "benchmark_%s", timestamp);
    if (mkdir(folder_name, 0755) != 0)
    {
        perror("Failed to create benchmark directory");
        return 1;
    }

    // Create CSV filename with parameters (removed threads since it varies)
    snprintf(filename, sizeof(filename), "bench_threshold%u_hotruns%u.csv",
             iThreshold, iNumHotRuns);
    snprintf(filepath, sizeof(filepath), "%s/%s", folder_name, filename);

    // Open CSV file for writing
    FILE *output_file = fopen(filepath, "w");
    if (output_file == NULL)
    {
        perror("Failed to create output file");
        return 1;
    }

    // Write CSV header
    fprintf(output_file, "counter,n_threads,threshold,time (ms),final_count\n");

    // Run parameter sweep across different thread counts
    for (int i = 0; i < numThreadSweeps; i++)
    {
        printf("Running benchmark with %u threads...\n", threadCounts[i]);
        BenchCounter_benchApproximateCounter(threadCounts[i], iThreshold, 1000000, 15, iNumHotRuns, output_file);
        fflush(output_file); // Ensure data is written after each run
    }

    // Close file and cleanup
    fclose(output_file);

    printf("Parameter sweep completed. Results written to: %s\n", filepath);
    return 0;
}