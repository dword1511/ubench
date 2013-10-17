#ifndef UBENCH_MAIN_H
#define UBENCH_MAIN_H

#ifdef __linux
  #define _GNU_SOURCE
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "misc.h"
#include "pattern.h"
#include "config.h"

#ifdef EMBEDDED
  #define TEST_MAX_BLOCK MAX_BLOCK_SIZE_EMBEDDED
#else
  #define TEST_MAX_BLOCK MAX_BLOCK_SIZE
#endif

#if (defined __linux) && defined(EMBEDDED) && !defined(CLOCK_MONOTONIC_RAW)
  /* FIXME: Sometimes they are just missing things in the header. */
  #define CLOCK_MONOTONIC_RAW 4
#endif

#ifdef CLOCK_MONOTONIC_RAW
  #define TIMER CLOCK_MONOTONIC_RAW
#else
  #define TIMER CLOCK_MONOTONIC
  #define TIMER_NOT_NTP_PROOF
#endif

char     *myself;
char     fn[512];
int      fd;

#endif /* UBENCH_MAIN_H */
