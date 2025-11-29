#ifndef APPROXIMATE_COUNTER_H
#define APPROXIMATE_COUNTER_H

#include <counter_api.h>
#include <pthread.h>
#include <stdint.h>

/**
 * @brief Options for ApproximateCounter.
 */
typedef struct
{
    uint32_t mThreshold; // Local counter threshold before flushing to global
    uint32_t mThreads;   // Number of threads that will use this counter
} tApproximateCounter_Options;

/**
 * @brief Global ApproximateCounter interface. Defined in ApproximateCounter.c.
 */
extern const tCounter_interface gApproximateCounter_interface;

#endif // APPROXIMATE_COUNTER_H