#ifndef TRADITIONAL_COUNTER_H
#define TRADITIONAL_COUNTER_H

#include <counter_api.h>
#include <pthread.h>
#include <stdint.h>

/**
 * @brief Global TraditionalCounter interface. Defined in TraditionalCounter.c.
 */
extern const tCounter_interface gTraditionalCounter_interface;

#endif // TRADITIONAL_COUNTER_H