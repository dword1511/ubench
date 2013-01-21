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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h> 
#include "rand.h"
#include "version.h"

#define TEST_FILE_NAME "~ubench.tmp"
#define TEST_MAX_BLOCK 8 // 8MB

// TODO: Try 2-fd method: write with regular + fdatasync and read with O_DIRECT.

char     *myself;
uint8_t  *rand_8mb, *read_buf, *zeros;
char     fn[512];
int      fd            = -1;
uint16_t bench_size    = 128; // in mega bytes, larger than 8MB
char     *size_pattern = "51248abcdefghij";

void signal_handler(int signum) {
  fprintf(stderr, "\nReceived SIGTERM, benchmark terminated.\nCleaning up...\n");

  if(fd != -1) {
    close(fd);
    unlink(fn);
  }

  _exit(EINTR);
}

void print_help(void) {
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
      case 'i':
      case 'j':
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

  if(posix_memalign((void **) &rand_8mb, getpagesize(), TEST_MAX_BLOCK * 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate 8MB of page-aligned RAM.\n");
    _exit(errno);
  }
  if(posix_memalign((void **) &read_buf, getpagesize(), TEST_MAX_BLOCK * 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate 8MB of page-aligned RAM.\n");
    _exit(errno);
  }
  if(posix_memalign((void **) &zeros, getpagesize(), 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate 1MB of page-aligned RAM.\n");
    _exit(errno);
  }

  for(x = 0; x < 1024; x ++) {
    for(y = 0; y < 1024; y ++) rand_8mb[x * 1024 + y] = rand_kilo[x] ^ rand_kilo[y];
  }
  for(x = 0; x < 8; x ++) {
    for(y = 0; y < 1024 * 1024; y ++) rand_8mb[x * 1024 * 1024 + y] = rand_8mb[y];
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
  static uint32_t        i, loops, packet_size;
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
  loops = size * 1024 * 1024 / packet_size;
  if(loops > 8192) {
    loops = 8192;
    size  = loops * packet_size / (1024 * 1024);
  }

  // Print information
  if(packet_size == 512) fprintf(stdout, " 0.5 ");
  else fprintf(stdout, "%4d ", packet_size / 1024);

  // Clear the effects from last loop
  lseek(fd, 0, SEEK_SET);
  fsync(fd);
  sleep(1);

  // Write benchmark
  clock_gettime(CLOCK_MONOTONIC_RAW, &tp_pre);
  for(i = 0; i < loops; i ++) write(fd, rand_8mb, packet_size);
  fdatasync(fd);
  clock_gettime(CLOCK_MONOTONIC_RAW, &tp_post);
  if(stat(fn, &stat_buf) == -1) {
    fprintf(stderr, "\nFile \"%s\" disappeared!\nPlease check device status.\n", fn);
    _exit(errno);
  }
  else fprintf(stdout, "%8d ", (uint32_t) size * 1024 * 1024 / calc_ms(tp_pre, tp_post));

  // Clear the effects from last loop
  lseek(fd, 0, SEEK_SET);
  fsync(fd);
  sleep(1);

  // Read benchmark
  clock_gettime(CLOCK_MONOTONIC_RAW, &tp_pre);
  for(i = 0; i < loops; i ++) read(fd, read_buf, packet_size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &tp_post);
  if(stat(fn, &stat_buf) == -1) {
    fprintf(stderr, "\nFile \"%s\" disappeared!\nPlease check device status.\n", fn);
    _exit(errno);
  }
  else fprintf(stdout, "%8d\n", (uint32_t) size * 1024 * 1024 / calc_ms(tp_pre, tp_post));
}

int main(int argc, char *argv[]) {
  struct stat stat_buf;
  char        *mount_point;
  char        o;

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
      default: abort();
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

  assemble_data();
  fprintf(stdout, "This is ubench version %s.\n", VERSION);

  // Capture Ctrl-C after fn is set
  signal(SIGINT, signal_handler);

  // Attempt to open file for binary reading and writting
  fd = open(fn, O_RDWR | O_CREAT | O_NOATIME | O_DIRECT, 0644);
  if(fd == -1) {
    fprintf(stderr, "Unable to open \"%s\" for benchmark.\n", fn);
    return errno;
  }

  fill_out(fd, bench_size, fn);

  // Do the actual job
  fprintf(stdout, "Benchmark started.\n\nSIZE    WRITE     READ\n KiB     KB/s     KB/s\n\
======================\n");
  while(*size_pattern) {
    do_bench(fd, *size_pattern, bench_size);
    size_pattern++;
  }

  // Hopefully users would not move test file around.
  // Unlink by fd is no where to be found.
  close(fd);
  return unlink(fn);
}
