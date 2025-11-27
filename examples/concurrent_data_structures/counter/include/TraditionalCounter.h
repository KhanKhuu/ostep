#ifndef TRADITIONAL_COUNTER_H
#define TRADITIONAL_COUNTER_H

#include <pthread.h>
#include <stdint.h>

/**
 * @brief Simple thread-safe counter using global lock.
 */
typedef struct __TraditionalCounter_t
{
    uint32_t mGlobal;       // global count
    pthread_mutex_t mGlock; // global count lock
} TraditionalCounter_t;

/**
 * @brief Initialize the counter.
 *
 * @param ioCounter Counter instance to initialize.
 * @param iInitParams Ignored (should be NULL).
 */
void TraditionalCounter_init(TraditionalCounter_t *ioCounter, void *iInitParams);

/**
 * @brief Update counter by specified amount.
 *
 * @param ioCounter Counter to update.
 * @param iThread Ignored (for API compatibility).
 * @param iAmount Amount to add to counter.
 */
void TraditionalCounter_update(TraditionalCounter_t *ioCounter, uint32_t iThread, int iAmount);

/**
 * @brief Flush pending updates.
 *
 * No-op for traditional counter (all updates are immediate).
 *
 * @param ioCounter Counter instance.
 * @param iThread Ignored.
 */
void TraditionalCounter_flush(TraditionalCounter_t *ioCounter, uint32_t iThread);

/**
 * @brief Reset counter to zero.
 *
 * @param ioCounter Counter to reset.
 */
void TraditionalCounter_reset(TraditionalCounter_t *ioCounter);

/**
 * @brief Get current counter value.
 *
 * @param iCounter Counter to read from.
 * @return Current count value.
 */
uint32_t TraditionalCounter_get(TraditionalCounter_t *iCounter);

/**
 * @brief Clean up counter resources.
 *
 * @param ioCounter Counter to destroy.
 */
void TraditionalCounter_destroy(TraditionalCounter_t *ioCounter);

#endif // TRADITIONAL_COUNTER_H