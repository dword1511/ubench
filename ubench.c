/*************************************************************************
 * ubench.c                                                              *
 * A simple storage device benchmark tool for POSIX systems              *
 * that operates on the top of a file system.                            *
 *                                                                       *
 * Copyright (C) 2013  dword1511 <zhangchi866@gmail.com>                 *
 *                                                                       *
 * This program is free software: you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 *  This program is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *  GNU General Public License for more details.                         *
 *                                                                       *
 *  You should have received a copy of the GNU General Public License    *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 *************************************************************************/

// Platform setup first
#ifdef __linux
  #ifndef EMBEDDED
    #define _GNU_SOURCE
  #else
    #undef _GNU_SOURCE
  #endif
  #define TIMER CLOCK_MONOTONIC_RAW

  #if defined(EMBEDDED) && !defined(CLOCK_MONOTONIC_RAW)
    #define CLOCK_MONOTONIC_RAW 4
  #endif
#else
  // This one is not NTP-proof like CLOCK_MONOTONIC_RAW.
  #define TIMER CLOCK_MONOTONIC
  #warning "Falling back to CLOCK_MONOTONIC!"
#endif

#ifdef _FORCE_FALLBACK
  #define TIMER CLOCK_MONOTONIC
  #undef _GNU_SOURCE
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

#include "rand.h"

#define TEST_FILE_NAME "~ubench.tmp"
#ifdef EMBEDDED
  #define TEST_MAX_BLOCK 2 // 2MB
#else
  #define TEST_MAX_BLOCK 8 // 8MB
#endif

// TODO: Try 2-fd method: write with regular + fdatasync and read with O_DIRECT.

char     *myself;
uint8_t  *write_buf, *read_buf, *zeros;
char     fn[512];
int      fd            = -1;
#ifdef EMBEDDED
uint16_t bench_size    = 16; // in mega bytes, larger than 2MB
char     *size_pattern = "51248abcdefgh";
#else
uint16_t bench_size    = 256; // in mega bytes, larger than 8MB
char     *size_pattern = "51248abcdefghij";
#endif

void signal_handler(int signum) {
  fprintf(stderr, "\nReceived signal, benchmark terminated.\nCleaning up...\n");

  if(fd != -1) {
    close(fd);
    unlink(fn);
  }

  _exit(EINTR);
}

void print_help(void) {
#ifdef EMBEDDED
  fprintf(stderr, "\
ubench: A Simple Storage Device Benchmark Tool for POSIX\n\
(That operates on the top of a file system)\n\
Version %s for embedded devices\n\
(C)dword1511 <zhangchi866@gmail.com>\n\
Project homepage: https://github.com/dword1511/ubench\n\
\n\
  Usage: %s -p mount_point <-h>\n\
         <-s benchmark_size>\n\
         <-a packet_size_pattern>\n\
\n\
Benchmark size is measured in MBytes and should be multiple of 8 (MBytes).\n\
Packet size pattern are described below:\n\
5 = 0.5KB 1 =   1KB 2 =   2KB 4 =   4KB 8 =   8KB\n\
a =  16KB b =  32KB c =  64KB d = 128KB e = 256KB\n\
f = 512KB g =   1MB h =   2MB\n\
For example, '-a 548dhh' means 0.5KB-4KB-8KB-128KB-2MB-2MB packet size sequence.\n\
For the letters, only those in lower case are accepted.\n\
\n\
Benchmark file name is hard-coded as '%s'.\n\
Default benchmark size     : %dMB\n\
Default pcaket size pattern: %s\n\
", VERSION, myself, TEST_FILE_NAME, bench_size, size_pattern);
#else
  fprintf(stderr, "\
ubench: A Simple Storage Device Benchmark Tool for POSIX\n\
(That operates on the top of a file system)\n\
Version %s\n\
(C)dword1511 <zhangchi866@gmail.com>\n\
Project homepage: https://github.com/dword1511/ubench\n\
\n\
  Usage: %s -p mount_point <-h>\n\
         <-s benchmark_size>\n\
         <-a packet_size_pattern>\n\
\n\
Benchmark size is measured in MBytes and should be multiple of 8 (MBytes).\n\
Packet size pattern are described below:\n\
5 = 0.5KB 1 =   1KB 2 =   2KB 4 =   4KB 8 =   8KB\n\
a =  16KB b =  32KB c =  64KB d = 128KB e = 256KB\n\
f = 512KB g =   1MB h =   2MB i =   4MB j =   8MB\n\
For example, '-a 548djj' means 0.5KB-4KB-8KB-128KB-8MB-8MB packet size sequence.\n\
For the letters, only those in lower case are accepted.\n\
\n\
Benchmark file name is hard-coded as '%s'.\n\
Default benchmark size     : %dMB\n\
Default pcaket size pattern: %s\n\
", VERSION, myself, TEST_FILE_NAME, bench_size, size_pattern);
#endif
  _exit(EINVAL);
}

int is_uint(char *s) {
  while(*s) {
    if(*s < '0' || *s > '9') return 0;
    s ++;
  }

  return 1;
}

void check_pattern(char *s) {
  while(*s) {
    switch(*s) {
      case '5':
      case '1':
      case '2':
      case '4':
      case '8':
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
      case 'g':
      case 'h':
#ifndef EMBEDDED
      case 'i':
      case 'j':
#endif
        break;
      default:
        fprintf(stderr, "Invalid packet size symbol: %c.\n\n", *s);
        print_help();
    }
    s ++;
  }
}

void assemble_data(void) {
  uint32_t x, y;

  if(posix_memalign((void **) &write_buf, getpagesize(), TEST_MAX_BLOCK * 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate %dMB of page-aligned RAM.\n", TEST_MAX_BLOCK);
    _exit(errno);
  }
  if(posix_memalign((void **) &read_buf, getpagesize(), TEST_MAX_BLOCK * 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate %dMB of page-aligned RAM.\n", TEST_MAX_BLOCK);
    _exit(errno);
  }
  if(posix_memalign((void **) &zeros, getpagesize(), 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate 1MB of page-aligned RAM.\n");
    _exit(errno);
  }

  for(x = 0; x < 1024; x ++) {
    for(y = 0; y < 1024; y ++) write_buf[x * 1024 + y] = rand_kilo[x] ^ rand_kilo[y];
  }
  for(x = 0; x < TEST_MAX_BLOCK; x ++) {
    for(y = 0; y < 1024 * 1024; y ++) write_buf[x * 1024 * 1024 + y] = write_buf[y];
  }

  for(x = 0; x < 1024 * 1024; x ++) zeros[x] = 0;
}

void fill_out(int fd, uint16_t size, char *path) {
  uint16_t i;

  fprintf(stdout, "Benchmark size set to: %hdMB.\n", bench_size);
  fprintf(stdout, "Filling out the benchmark file...");

  for(i = 0; i < bench_size; i ++) {
    if(write(fd, zeros, 1024 * 1024) != 1024 * 1024) {
      fprintf(stderr, "\nFailed to write to \"%s\".\n", path);
      _exit(errno);
    }
#ifndef _GNU_SOURCE
    fdatasync(fd);
#endif
    if(i % 4 == 3)fprintf(stdout, ".");
  }

  fsync(fd);
  fprintf(stdout, "done.\n");
}

uint32_t calc_ms(struct timespec before, struct timespec after) {
  return (after.tv_sec - before.tv_sec) * 1000 + (after.tv_nsec - before.tv_nsec) / 1000000;
}

void do_bench(int fd, char packet, uint16_t size) {
  if(fd == -1 || size < 8) abort();
  static uint64_t        i, loops, packet_size;
  static struct timespec tp_pre, tp_post;
  static struct stat stat_buf;

  // Translate symbol
  packet_size = 512;
  switch(packet) {
    case 'j': packet_size *= 2;
    case 'i': packet_size *= 2;
    case 'h': packet_size *= 2;
    case 'g': packet_size *= 2;
    case 'f': packet_size *= 2;
    case 'e': packet_size *= 2;
    case 'd': packet_size *= 2;
    case 'c': packet_size *= 2;
    case 'b': packet_size *= 2;
    case 'a': packet_size *= 2;
    case '8': packet_size *= 2;
    case '4': packet_size *= 2;
    case '2': packet_size *= 2;
    case '1': packet_size *= 2;
    case '5': break;
    default : abort();
  }

  // Reduce benchmark size for small packet runs.
  loops = size * 1024UL * 1024UL / packet_size;
  if(loops > 8192UL) {
    loops = 8192UL;
    size  = loops * packet_size / (1024UL * 1024UL);
  }

  // Print information
  if(packet_size == 512) fprintf(stdout, " 0.5 ");
  else fprintf(stdout, "%4ld ", packet_size / 1024UL);

  // Clear the effects from last loop
  lseek(fd, 0, SEEK_SET);
  fsync(fd);
  sleep(1);

  // Write benchmark
  clock_gettime(TIMER, &tp_pre);
#ifdef _GNU_SOURCE
  for(i = 0; i < loops; i ++) write(fd, write_buf, packet_size);
#else
  // Not-so-good performance mode
  for(i = 0; i < loops; i ++) {
    write(fd, write_buf, packet_size);
    fdatasync(fd);
  }
#endif
  fdatasync(fd);
  clock_gettime(TIMER, &tp_post);
  if(stat(fn, &stat_buf) == -1) {
    fprintf(stderr, "\nFile \"%s\" disappeared!\nPlease check device status.\n", fn);
    _exit(errno);
  }
  else fprintf(stdout, "%8ld ", size * 1024UL * 1024UL / calc_ms(tp_pre, tp_post));

  // Clear the effects from last loop
  lseek(fd, 0, SEEK_SET);
  fsync(fd);
  sleep(1);

  // Read benchmark
  clock_gettime(TIMER, &tp_pre);
#ifdef _GNU_SOURCE
  for(i = 0; i < loops; i ++) read(fd, read_buf, packet_size);
#else
  // The not-garuanteed method plus really poor performance
  // because you need to call posix_fadvise every time.
  // Maybe time each loop and finally sum up will make it better.
  // Then remove the warning in main function.
  for(i = 0; i < loops; i ++) {
    posix_fadvise(fd, 0, (off_t)size * 1024 * 1024, POSIX_FADV_DONTNEED);
    read(fd, read_buf, packet_size);
  }
#endif
  clock_gettime(TIMER, &tp_post);
  if(stat(fn, &stat_buf) == -1) {
    fprintf(stderr, "\nFile \"%s\" disappeared!\nPlease check device status.\n", fn);
    _exit(errno);
  }
  else fprintf(stdout, "%8ld\n", size * 1024UL * 1024UL / calc_ms(tp_pre, tp_post));
}

int main(int argc, char *argv[]) {
  struct stat    stat_buf;
  struct utsname uname_buf;
  char           *mount_point;
  int            o;

  setbuf(stdout, NULL);
  myself      = argv[0];
  mount_point = NULL;

  // Check argument
  opterr = 0;
  while((o = getopt(argc, argv, "a:hp:s:")) != -1) {
    switch(o) {
      case 'h': print_help();
      case 's':
        if(!is_uint(optarg)) {
          fprintf(stderr, "Invalid benchmark size: %s.\n\n", optarg);
          print_help();
        }
        sscanf(optarg, "%hu", &bench_size);
        if((bench_size % 8) != 0 || bench_size == 0) {
          fprintf(stderr, "Invalid benchmark size: %s.\n\n", optarg);
          print_help();
        }
        break;
      case 'a':
        size_pattern = argv[optind - 1];
        check_pattern(size_pattern);
        break;
      case 'p':
        mount_point = argv[optind - 1];
        break;
      case '?':
        if(optopt == 'c') {
          fprintf(stderr, "Option -%c requires an argument.\n\n", optopt);
          print_help();
        }
        if(isprint(optopt)) {
          fprintf(stderr, "Unknown option `-%c'.\n\n", optopt);
          print_help();
        }
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        return EINVAL;
      default: {
        fprintf(stderr, "getopt returned unexpected `\\x%x'.\n", o);
        abort();
      }
    }
  }
  if(mount_point == NULL) {
    fprintf(stderr, "Missing mount point.\n\n");
    print_help();
  }
  if(sizeof(TEST_FILE_NAME) + strlen(mount_point) > 510) {
    fprintf(stderr, "The mount point path provided is too looooong!\n");
    return E2BIG;
  }
  sprintf(fn, "%s/%s", mount_point, TEST_FILE_NAME);

  // TODO: Check mountpoint (noatime,async or so) and print device information

  // Check file existence
  if(lstat(fn, &stat_buf) != -1) {
    fprintf(stderr, "Benchmark file \"%s\" already exists.\n\
Please make sure no ubench is running on the device, \
then remove it manually and try to start again.\n", fn);
    return EEXIST;
  }
  else if(errno != ENOENT) {
    fprintf(stderr, "Something unexpected happened \
while checking the existence of the benchmark file \"%s\".\n", fn);
    return errno;
  }

  // Capture Ctrl-C after fn is set
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Attempt to open file for binary reading and writting
#ifdef _GNU_SOURCE
  fd = open(fn, O_RDWR | O_CREAT | O_NOATIME | O_DIRECT, 0644);
#else
  // O_NOATIME and O_DIRECT are GNU extension,
  // but the read test is not garuanteed to work without O_DIRECT.
  fd = open(fn, O_RDWR | O_CREAT, 0644);
#endif
  if(fd == -1) {
    int err = errno;
    fprintf(stderr, "Unable to open \"%s\" for benchmark: %s.\n", fn, strerror(errno));
    if(err == EINVAL) fprintf(stderr, "\
======================\n\
It seems that opening the benchmark file with the O_DIRECT flag has failed.\n\
Please make sure you are not running the benchmark on a RAM or MTD device.\n");
    fprintf(stderr, "\nCleaning up...\n");
    unlink(fn);
    return err;
  }

  // Everything looks right, let's start.
  assemble_data();

#ifdef EMBEDDED
  fprintf(stdout, "This is ubench version %s for embedded devices.\n", VERSION);
#else
  fprintf(stdout, "This is ubench version %s.\n", VERSION);
#endif
  // TODO: also print mainboard / RAM / processor speed info.
  // TODO: also print some information about target device.
  if(uname(&uname_buf) == -1) {
    fprintf(stdout, "Failed to get system information due to error %s.\n", strerror(errno));
  }
  else {
    fprintf(stdout, "OS: %s %s\nArch: %s\n", \
    uname_buf.sysname, uname_buf.release, uname_buf.machine);
  }
#ifndef _GNU_SOURCE
  fprintf(stdout, "Warning: O_DIRECT is not supported on this platform.\n\
Read speed will be lower than its actual value while packet size is small.\n");
#endif
  fill_out(fd, bench_size, fn);

  // Do the actual job
  // TODO: add time stamp one start and stop messages
  fprintf(stdout, "Benchmark started.\n\nSIZE    WRITE     READ\n KiB     KB/s     KB/s\n\
======================\n");
  while(*size_pattern) {
    do_bench(fd, *size_pattern, bench_size);
    size_pattern++;
  }
  fprintf(stdout, "\nBenchmark finished.\n");

  // Hopefully users would not move test file around.
  // Unlink by fd is no where to be found.
  close(fd);
  return unlink(fn);
}
