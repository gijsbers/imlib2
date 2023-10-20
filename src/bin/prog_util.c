/*
 * Utility functions
 */
#include "config.h"

#if USE_MONOTONIC_CLOCK
#include <time.h>
#else
#include <sys/time.h>
#endif

#include "prog_util.h"

unsigned int
time_us(void)
{
#if USE_MONOTONIC_CLOCK
   struct timespec     ts;

   clock_gettime(CLOCK_MONOTONIC, &ts);

   return (unsigned int)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
#else
   struct timeval      timev;

   gettimeofday(&timev, NULL);

   return (unsigned int)(timev.tv_sec * 1000000 + timev.tv_usec);
#endif
}
