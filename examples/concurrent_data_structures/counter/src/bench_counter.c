#include <assert.h>
#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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
 * @brief Arguments for sweep_threads subcommand.
 */
typedef struct
{
    uint32_t mMinThreads; // Minimum number of threads
    uint32_t mMaxThreads; // Maximum number of threads
    uint32_t mStep;       // Step size for thread increments
    uint32_t mThreshold;  // Threshold for approximate counter
    uint32_t mIncrements; // Number of increments per thread
    uint32_t mWarmups;    // Number of warmup runs
    uint32_t mHotruns;    // Number of hot runs
} tBenchCounter_sweepThreadsArgs;

/**
 * @brief Arguments for sweep_threshold subcommand.
 */
typedef struct
{
    uint32_t mNumThreads;     // Number of threads (constant)
    uint32_t mStartThreshold; // Starting threshold value
    uint32_t mSteps;          // Number of threshold steps (multiply by 2 each step)
    uint32_t mIncrements;     // Number of increments per thread
    uint32_t mWarmups;        // Number of warmup runs
    uint32_t mHotruns;        // Number of hot runs
} tBenchCounter_sweepThresholdArgs;

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
                                              FILE *iOutputFilePtr)
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
            fprintf(iOutputFilePtr, "%s,%u,%u,%f,%u\n", aCounterNames[aDut], iNumThreads, iThreshold, aRuntime, aGlobalCount);

            sBenchCounter_DUTs[aDut].mInterfacePtr->mResetPtr(aCounterPtr);
        }

        // free memory
        sBenchCounter_DUTs[aDut].mInterfacePtr->mDestroyPtr(aCounterPtr);
        free(aCounterDriverThreadPtr);
        free(aContextPtr);
    }

    return 0;
}

/**
 * @brief Execute sweep_threads subcommand.
 *
 * Sweeps across different thread counts while keeping threshold constant.
 */
int BenchCounter_sweepThreads(const tBenchCounter_sweepThreadsArgs *iArgsPtr)
{
    // Get current wall-clock time for folder and file naming
    time_t aRawtime;
    struct tm *aTimeinfoPtr;
    char aTimestamp[64];
    char aFolderName[128];
    char aFilename[256];
    char aFilepath[384];
    FILE *aOutputFilePtr;

    time(&aRawtime);
    aTimeinfoPtr = localtime(&aRawtime);
    strftime(aTimestamp, sizeof(aTimestamp), "%Y%m%d_%H%M%S", aTimeinfoPtr);

    // Create benchmark folder
    snprintf(aFolderName, sizeof(aFolderName), "benchmark_%s", aTimestamp);
    if (mkdir(aFolderName, 0755) != 0)
    {
        perror("Failed to create benchmark directory");
        return 1;
    }

    // Create CSV filename for thread sweep
    snprintf(aFilename, sizeof(aFilename), "sweep_threads_threshold%u_increments%u_warmups%u_hotruns%u.csv",
             iArgsPtr->mThreshold, iArgsPtr->mIncrements, iArgsPtr->mWarmups, iArgsPtr->mHotruns);
    snprintf(aFilepath, sizeof(aFilepath), "%s/%s", aFolderName, aFilename);

    // Open CSV file for writing
    aOutputFilePtr = fopen(aFilepath, "w");
    if (aOutputFilePtr == NULL)
    {
        perror("Failed to create output file");
        return 1;
    }

    // Write CSV header
    fprintf(aOutputFilePtr, "counter,n_threads,threshold,time (ms),final_count\n");

    // Run parameter sweep across different thread counts
    for (uint32_t aThreads = iArgsPtr->mMinThreads; aThreads <= iArgsPtr->mMaxThreads; aThreads += iArgsPtr->mStep)
    {
        printf("Running benchmark with %u threads...\n", aThreads);
        BenchCounter_benchApproximateCounter(aThreads, iArgsPtr->mThreshold, iArgsPtr->mIncrements,
                                             iArgsPtr->mWarmups, iArgsPtr->mHotruns, aOutputFilePtr);
        fflush(aOutputFilePtr); // Ensure data is written after each run
    }

    // Close file and cleanup
    fclose(aOutputFilePtr);

    printf("Thread sweep completed. Results written to: %s\n", aFilepath);
    return 0;
}

/**
 * @brief Execute sweep_threshold subcommand.
 *
 * Sweeps across different threshold values while keeping thread count constant.
 */
int BenchCounter_sweepThreshold(const tBenchCounter_sweepThresholdArgs *iArgsPtr)
{
    // Get current wall-clock time for folder and file naming
    time_t aRawtime;
    struct tm *aTimeinfoPtr;
    char aTimestamp[64];
    char aFolderName[128];
    char aFilename[256];
    char aFilepath[384];
    FILE *aOutputFilePtr;

    time(&aRawtime);
    aTimeinfoPtr = localtime(&aRawtime);
    strftime(aTimestamp, sizeof(aTimestamp), "%Y%m%d_%H%M%S", aTimeinfoPtr);

    // Create benchmark folder
    snprintf(aFolderName, sizeof(aFolderName), "benchmark_%s", aTimestamp);
    if (mkdir(aFolderName, 0755) != 0)
    {
        perror("Failed to create benchmark directory");
        return 1;
    }

    // Create CSV filename for threshold sweep
    snprintf(aFilename, sizeof(aFilename), "sweep_threshold_threads%u_increments%u_warmups%u_hotruns%u.csv",
             iArgsPtr->mNumThreads, iArgsPtr->mIncrements, iArgsPtr->mWarmups, iArgsPtr->mHotruns);
    snprintf(aFilepath, sizeof(aFilepath), "%s/%s", aFolderName, aFilename);

    // Open CSV file for writing
    aOutputFilePtr = fopen(aFilepath, "w");
    if (aOutputFilePtr == NULL)
    {
        perror("Failed to create output file");
        return 1;
    }

    // Write CSV header
    fprintf(aOutputFilePtr, "counter,n_threads,threshold,time (ms),final_count\n");

    // Run parameter sweep across different threshold values
    uint32_t aThreshold = iArgsPtr->mStartThreshold;
    for (uint32_t aStep = 0; aStep < iArgsPtr->mSteps; ++aStep)
    {
        printf("Running benchmark with threshold %u...\n", aThreshold);
        BenchCounter_benchApproximateCounter(iArgsPtr->mNumThreads, aThreshold, iArgsPtr->mIncrements,
                                             iArgsPtr->mWarmups, iArgsPtr->mHotruns, aOutputFilePtr);
        fflush(aOutputFilePtr); // Ensure data is written after each run
        aThreshold *= 2;        // Multiply by 2 for next step
    }

    // Close file and cleanup
    fclose(aOutputFilePtr);

    printf("Threshold sweep completed. Results written to: %s\n", aFilepath);
    return 0;
}

/**
 * @brief Print usage information.
 */
void BenchCounter_printUsage(const char *aProgramNamePtr)
{
    printf("Usage: %s <subcommand> [options]\n\n", aProgramNamePtr);
    printf("Subcommands:\n");
    printf("  sweep_threads   - Sweep across different thread counts\n");
    printf("  sweep_threshold - Sweep across different threshold values\n\n");

    printf("sweep_threads options:\n");
    printf("  --min-threads <n>    Minimum number of threads (default: 1)\n");
    printf("  --max-threads <n>    Maximum number of threads (default: 16)\n");
    printf("  --step <n>           Step size for thread increments (default: 1)\n");
    printf("  --threshold <n>      Threshold for approximate counter (default: 4096)\n");
    printf("  --increments <n>     Number of increments per thread (default: 100000)\n");
    printf("  --warmups <n>        Number of warmup runs (default: 15)\n");
    printf("  --hotruns <n>        Number of hot runs (default: 30)\n\n");

    printf("sweep_threshold options:\n");
    printf("  --num-threads <n>      Number of threads (constant) (default: 8)\n");
    printf("  --start-threshold <n>  Starting threshold value (default: 1)\n");
    printf("  --steps <n>            Number of threshold steps (default: 16)\n");
    printf("  --increments <n>       Number of increments per thread (default: 100000)\n");
    printf("  --warmups <n>          Number of warmup runs (default: 15)\n");
    printf("  --hotruns <n>          Number of hot runs (default: 30)\n");
}

int main(int argc, char **argv)
{
    printf("Welcome to Concurrent Counter Driver\n");

    if (argc < 2)
    {
        BenchCounter_printUsage(argv[0]);
        return 1;
    }

    const char *aSubcommandPtr = argv[1];

    if (strcmp(aSubcommandPtr, "sweep_threads") == 0)
    {
        tBenchCounter_sweepThreadsArgs aArgs = {
            .mMinThreads = 1,
            .mMaxThreads = 16,
            .mStep = 1,
            .mThreshold = 4096,
            .mIncrements = 100000,
            .mWarmups = 15,
            .mHotruns = 30};

        static struct option aLongOptions[] = {
            {"min-threads", required_argument, 0, 0},
            {"max-threads", required_argument, 0, 1},
            {"step", required_argument, 0, 2},
            {"threshold", required_argument, 0, 3},
            {"increments", required_argument, 0, 4},
            {"warmups", required_argument, 0, 5},
            {"hotruns", required_argument, 0, 6},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        int aOptionIndex = 0;
        int aC;
        optind = 2; // Skip program name and subcommand

        while ((aC = getopt_long(argc, argv, "h", aLongOptions, &aOptionIndex)) != -1)
        {
            switch (aC)
            {
            case 0:
                aArgs.mMinThreads = (uint32_t)atoi(optarg);
                break;
            case 1:
                aArgs.mMaxThreads = (uint32_t)atoi(optarg);
                break;
            case 2:
                aArgs.mStep = (uint32_t)atoi(optarg);
                break;
            case 3:
                aArgs.mThreshold = (uint32_t)atoi(optarg);
                break;
            case 4:
                aArgs.mIncrements = (uint32_t)atoi(optarg);
                break;
            case 5:
                aArgs.mWarmups = (uint32_t)atoi(optarg);
                break;
            case 6:
                aArgs.mHotruns = (uint32_t)atoi(optarg);
                break;
            case 'h':
                BenchCounter_printUsage(argv[0]);
                return 0;
            case '?':
                BenchCounter_printUsage(argv[0]);
                return 1;
            default:
                break;
            }
        }

        return BenchCounter_sweepThreads(&aArgs);
    }
    else if (strcmp(aSubcommandPtr, "sweep_threshold") == 0)
    {
        tBenchCounter_sweepThresholdArgs aArgs = {
            .mNumThreads = 8,
            .mStartThreshold = 1,
            .mSteps = 16,
            .mIncrements = 100000,
            .mWarmups = 15,
            .mHotruns = 30};

        static struct option aLongOptions[] = {
            {"num-threads", required_argument, 0, 0},
            {"start-threshold", required_argument, 0, 1},
            {"steps", required_argument, 0, 2},
            {"increments", required_argument, 0, 3},
            {"warmups", required_argument, 0, 4},
            {"hotruns", required_argument, 0, 5},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        int aOptionIndex = 0;
        int aC;
        optind = 2; // Skip program name and subcommand

        while ((aC = getopt_long(argc, argv, "h", aLongOptions, &aOptionIndex)) != -1)
        {
            switch (aC)
            {
            case 0:
                aArgs.mNumThreads = (uint32_t)atoi(optarg);
                break;
            case 1:
                aArgs.mStartThreshold = (uint32_t)atoi(optarg);
                break;
            case 2:
                aArgs.mSteps = (uint32_t)atoi(optarg);
                break;
            case 3:
                aArgs.mIncrements = (uint32_t)atoi(optarg);
                break;
            case 4:
                aArgs.mWarmups = (uint32_t)atoi(optarg);
                break;
            case 5:
                aArgs.mHotruns = (uint32_t)atoi(optarg);
                break;
            case 'h':
                BenchCounter_printUsage(argv[0]);
                return 0;
            case '?':
                BenchCounter_printUsage(argv[0]);
                return 1;
            default:
                break;
            }
        }

        return BenchCounter_sweepThreshold(&aArgs);
    }
    else
    {
        printf("Unknown subcommand: %s\n\n", aSubcommandPtr);
        BenchCounter_printUsage(argv[0]);
        return 1;
    }
}