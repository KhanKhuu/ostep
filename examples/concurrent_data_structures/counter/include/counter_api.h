#ifndef COUNTER_API_H
#define COUNTER_API_H

#include <stdint.h>

/**
 * @brief The first field of every counter instance must be this "base" class.
 */
typedef struct
{
    uint32_t mCounterId; // Identifier of the counter
} tCounter_instance;

/**
 * @brief Allocate memory and initialize a counter instance
 *
 * @param iBasePtr Pointer to the "base class" structure to copy into first
 *                 field of new counter instance.
 * @param iOptionsPtr Pointer to counter-specific parameters. Null if unused by
 *                    the counter implementation.
 * @return Pointer to a new counter instance.
 */
typedef tCounter_instance *(tCounter_create)(const tCounter_instance *iBasePtr,
                                             const void *iOptionsPtr);

/**
 * @brief Perform end of life cleanup for a kernel instance. Free memory and
 *        invalidate the instance.
 *
 * @param ioInstancePtr Pointer to the counter instance.
 */
typedef void(tCounter_destroy)(tCounter_instance *ioInstancePtr);

/**
 * @brief Reset a counter to post-creaion state.
 *
 * @param ioInstancePtr Pointer to the counter instance.
 */
typedef void(tCounter_reset)(tCounter_instance *ioInstancePtr);

/**
 * @brief Flush a local counter thread to global count. Used by multi-lock
 *        counters only.
 *
 * @param ioInstancePtr Pointer to the counter instance.
 * @param iThread Local thread ID.
 */
typedef void(tCounter_flush)(tCounter_instance *ioInstancePtr,
                             const uint32_t iThread);

/**
 * @brief Increment a counter.
 *
 * @param ioInstancePtr Pointer to the counter instance.
 * @param iThread Local thread ID (only used by multi-lock counters).
 */
typedef void(tCounter_increment)(tCounter_instance *ioInstancePtr,
                                 const uint32_t iThread,
                                 const uint32_t iAmount);

/**
 * @brief Get a counter's count.
 *
 * @param ioInstancePtr Pointer to the counter instance.
 * @param oCount Address to write count to.
 */
typedef void(tCounter_get)(tCounter_instance *ioInstancePtr,
                           uint32_t *oCount);

/**
 * @brief Counter interface.
 */
typedef struct
{
    tCounter_create *mCreatePtr;
    tCounter_destroy *mDestroyPtr;
    tCounter_reset *mResetPtr;
    tCounter_flush *mFlushPtr;
    tCounter_increment *mIncrementPtr;
    tCounter_get *mGetPtr;
} tCounter_interface;

#endif // COUNTER_API_H