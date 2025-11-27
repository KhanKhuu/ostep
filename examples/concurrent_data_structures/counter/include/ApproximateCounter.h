#ifndef APPROXIMATE_COUNTER_H
#define APPROXIMATE_COUNTER_H

#include <pthread.h>
#include <stdint.h>

/**
 * @brief Initialization parameters for ApproximateCounter.
 */
typedef struct __ApproximateCounterInitParams_t
{
    uint32_t mThreshold; // Local counter threshold before flushing to global
    uint32_t mThreads;   // Number of threads that will use this counter
} ApproximateCounterInitParams_t;

/**
 * @brief Scalable counter using per-thread local counters and periodic flushing.
 */
typedef struct __ApproximateCounter_t
{
    uint32_t mGlobal;        // global count
    pthread_mutex_t mGlock;  // global count lock
    uint32_t mThreads;       // number of local counter threads
    uint32_t *mLocal;        // local counts (one per thread)
    pthread_mutex_t *mLlock; // local counts locks (one per thread)
    uint32_t mThreshold;     // update frequency
} ApproximateCounter_t;

/**
 * @brief Initialize the approximate counter.
 *
 * @param ioCounter Counter instance to initialize.
 * @param iInitParams Pointer to ApproximateCounterInitParams_t containing threshold and threads.
 */
void ApproximateCounter_init(ApproximateCounter_t *ioCounter, void *iInitParams);

/**
 * @brief Update thread-local counter, flushing to global when threshold is reached.
 *
 * @param ioCounter Counter to update.
 * @param iThread Thread ID (0 to num_threads-1).
 * @param iAmount Amount to add to local counter.
 */
void ApproximateCounter_update(ApproximateCounter_t *ioCounter, uint32_t iThread, int iAmount);

/**
 * @brief Flush thread's local count to global counter.
 *
 * @param ioCounter Counter instance.
 * @param iThread Thread ID to flush.
 */
void ApproximateCounter_flush(ApproximateCounter_t *ioCounter, uint32_t iThread);

/**
 * @brief Reset all counters to zero.
 *
 * @param ioCounter Counter to reset.
 */
void ApproximateCounter_reset(ApproximateCounter_t *ioCounter);

/**
 * @brief Get approximate counter value.
 *
 * Returns global count plus all unflushed local counts.
 *
 * @param iCounter Counter to read from.
 * @return Approximate current count value.
 */
uint32_t ApproximateCounter_get(ApproximateCounter_t *iCounter);

/**
 * @brief Clean up counter resources.
 *
 * @param ioCounter Counter to destroy.
 */
void ApproximateCounter_destroy(ApproximateCounter_t *ioCounter);

#endif // APPROXIMATE_COUNTER_H