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
    #undef _GNU_SOURCE /* Lots of weird things happening there, let's just assume this isn't available. */
  #endif
  #define TIMER CLOCK_MONOTONIC_RAW

  #if defined(EMBEDDED) && !defined(CLOCK_MONOTONIC_RAW)
    #define CLOCK_MONOTONIC_RAW 4
  #endif
#else
  #define TIMER CLOCK_MONOTONIC /* This one is not NTP-proof like CLOCK_MONOTONIC_RAW. */
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
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/resource.h>


/* NOTE: ENTROPY_BYTES must be a factor of 1024. Collecting lots of entropy can take long time. */
#define ENTROPY_BYTES          4
#define MAX_PATH_LEN           512
#define TEST_FILE_NAME         "~ubench.tmp"
#ifdef EMBEDDED
  #define TEST_MAX_BLOCK       2 /* 2 MiB */
  #define DEFAULT_BENCH_SIZE   16
  #define DEFAULT_SIZE_PATTERN "51248abcdefgh"
#else
  #define TEST_MAX_BLOCK       8 /* 8 MiB */
  #define DEFAULT_BENCH_SIZE   256
  #define DEFAULT_SIZE_PATTERN "51248abcdefghij"
#endif

static uint8_t  *write_buf, *read_buf, *zeros;
static char     fn[MAX_PATH_LEN];
static int      fd = -1;
static uint16_t bench_size    = DEFAULT_BENCH_SIZE; /* in mega bytes, multiple of TEST_MAX_BLOCK. */
static char     *size_pattern = DEFAULT_SIZE_PATTERN;

static void signal_handler(int signum) {
  fprintf(stderr, "\nReceived signal, benchmark terminated.\nCleaning up...\n");

  if (fd != -1) {
    close(fd);
    unlink(fn);
  }

  exit(EINTR);
}

static void print_help(char *self) {
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
Benchmark size is measured in MBytes and should be multiple of 2 (MBytes).\n\
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
", VERSION, self, TEST_FILE_NAME, DEFAULT_BENCH_SIZE, DEFAULT_SIZE_PATTERN);
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
", VERSION, self, TEST_FILE_NAME, DEFAULT_BENCH_SIZE, DEFAULT_SIZE_PATTERN);
#endif
  exit(EINVAL);
}

static bool is_uint(char *s) {
  while (*s) {
    if (*s < '0' || *s > '9') return false;
    s ++;
  }

  return true;
}

static void check_pattern(char *s, char *self) {
  while (*s) {
    switch (*s) {
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
        print_help(self);
    }
    s ++;
  }
}

static void assemble_data(void) {
  uint32_t x, y;
  uint8_t rand_1k[1024];
  FILE *fentropy;

  if (posix_memalign((void **) &write_buf, getpagesize(), TEST_MAX_BLOCK * 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate %d MiB of page-aligned RAM.\n", TEST_MAX_BLOCK);
    exit(errno);
  }
  if (posix_memalign((void **) &read_buf, getpagesize(), TEST_MAX_BLOCK * 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate %d MiB of page-aligned RAM.\n", TEST_MAX_BLOCK);
    exit(errno);
  }
  if (posix_memalign((void **) &zeros, getpagesize(), 1024 * 1024) != 0) {
    fprintf(stderr, "Failed to allocate 1 MiB of page-aligned RAM.\n");
    exit(errno);
  }

  fentropy = fopen("/dev/random", "rb");
  if (fentropy == NULL) {
    fprintf(stderr, "Failed to open /dev/random for entropy: %s\n", strerror(errno));
    exit(errno);
  }

  memset(rand_1k, 0, 1024);

  fprintf(stdout, "Getting %u bits of entropy...", ENTROPY_BYTES * 8U);
  fread(rand_1k, ENTROPY_BYTES, 1, fentropy);
  fclose(fentropy);
  fprintf(stdout, " done: ");
  for (x = 0; x < ENTROPY_BYTES; x ++) {
    fprintf(stdout, "%02x", rand_1k[x]);
  }
  fprintf(stdout, "\n");

  for (x = 0; x < ENTROPY_BYTES; x ++) {
    srandom(rand_1k[x]);
    for (y = 0; y < (1024 / ENTROPY_BYTES); y ++) {
      rand_1k[y * ENTROPY_BYTES + x] = random();
    }
  }

  for (x = 0; x < 1024; x ++) {
    for (y = 0; y < 1024; y ++) {
      write_buf[x * 1024 + y] = rand_1k[x] ^ rand_1k[y];
    }
  }

  for (x = 0; x < TEST_MAX_BLOCK; x ++) {
    for (y = 0; y < 1024 * 1024; y ++) {
      write_buf[x * 1024 * 1024 + y] = write_buf[y];
    }
  }

  for (x = 0; x < 1024 * 1024; x ++) {
    zeros[x] = 0;
  }
}

static void fill_out(int fd, uint16_t size, char *path) {
  uint16_t i;

  fprintf(stdout, "Benchmark size: %hd MiB.\n", bench_size);
  fprintf(stdout, "Filling out the benchmark file...");

  for(i = 0; i < bench_size; i ++) {
    if(write(fd, zeros, 1024 * 1024) != 1024 * 1024) {
      fprintf(stderr, "\nFailed to write to \"%s\".\n", path);
      unlink(path);
      exit(errno);
    }
#ifndef _GNU_SOURCE
    fdatasync(fd);
#endif
    if(i % 4 == 3) {
      fprintf(stdout, ".");
    }
  }

  fsync(fd);
  fprintf(stdout, "done.\n");
}

static uint32_t calc_ms(struct timespec before, struct timespec after) {
  return (after.tv_sec - before.tv_sec) * 1000 + (after.tv_nsec - before.tv_nsec) / 1000000;
}

static void do_bench(int fd, char packet, uint16_t size) {
  if(fd == -1 || size < 8) abort();
  static uint64_t        i, loops, packet_size;
  static struct timespec tp_pre, tp_post;
  static struct stat     stat_buf;

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
  if(packet_size == 512) {
    fprintf(stdout, " 0.5 ");
  } else {
    fprintf(stdout, "%4ld ", packet_size / 1024UL);
  }

  // Clear the effects from last loop
  lseek(fd, 0, SEEK_SET);
  fsync(fd);
  sleep(1);

  // Write benchmark
  clock_gettime(TIMER, &tp_pre);
#ifdef _GNU_SOURCE
  for(i = 0; i < loops; i ++) {
    write(fd, write_buf, packet_size);
  }
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
    exit(errno);
  } else {
    fprintf(stdout, "%8ld ", size * 1024UL * 1024UL / calc_ms(tp_pre, tp_post));
  }

  // Clear the effects from last loop
  lseek(fd, 0, SEEK_SET);
  fsync(fd);
  sleep(1);

  // Read benchmark
  clock_gettime(TIMER, &tp_pre);
#ifdef _GNU_SOURCE
  for (i = 0; i < loops; i ++) {
    read(fd, read_buf, packet_size);
  }
#else
  // The not-garuanteed method plus really poor performance
  // because you need to call posix_fadvise every time.
  // Maybe time each loop and finally sum up will make it better.
  // Then remove the warning in main function.
  for (i = 0; i < loops; i ++) {
    posix_fadvise(fd, 0, (off_t)size * 1024 * 1024, POSIX_FADV_DONTNEED);
    read(fd, read_buf, packet_size);
  }
#endif
  clock_gettime(TIMER, &tp_post);
  if (stat(fn, &stat_buf) == -1) {
    fprintf(stderr, "\nFile \"%s\" disappeared!\nPlease check device status.\n", fn);
    exit(errno);
  } else {
    fprintf(stdout, "%8ld\n", size * 1024UL * 1024UL / calc_ms(tp_pre, tp_post));
  }
}

static void print_info(char *path) {
  time_t t;
  struct utsname uname_buf;
  struct statvfs stvfs_buf;
  long   page_size;

#ifdef EMBEDDED
  fprintf(stdout, "This is ubench version %s for embedded devices.\n", VERSION);
#else
  fprintf(stdout, "This is ubench version %s.\n", VERSION);
#endif

  t = time(NULL);
  fprintf(stdout, "Time: %s", ctime(&t)); /* ctime comes with newline. */

  if (uname(&uname_buf) != 0) {
    fprintf(stdout, "Failed to get system information: %s\n", strerror(errno));
  } else {
    fprintf(stdout, "OS: %s %s\nArch: %s\n", uname_buf.sysname, uname_buf.release, uname_buf.machine);
  }

  page_size = sysconf(_SC_PAGESIZE) / 1024;
  fprintf(stdout, "RAM: %.2f MiB free out of %.2f MiB, %ld KiB pages\n", sysconf(_SC_AVPHYS_PAGES) / 1024.0f * page_size, sysconf(_SC_PHYS_PAGES) / 1024.0f * page_size, page_size);

  // TODO: also print processor speed info.

  if (statvfs(path, &stvfs_buf) != 0) {
    fprintf(stdout, "Failed to get system information: %s\n", strerror(errno));
  } else {
    float br = stvfs_buf.f_frsize / 1024.0f / 1024.0f / 1024.0f;
    if (stvfs_buf.f_flag & ST_RDONLY) {
      fprintf(stderr, "Target file system is read-only! Cannot proceed.");
      exit(EACCES);
    }
    fprintf(stdout, "Target Disk: %.2f GiB free out of %.2f GiB, %lu%s KiB blocks%s%s%s%s%s\n",
            stvfs_buf.f_bfree * br, stvfs_buf.f_blocks * br, stvfs_buf.f_bsize / 1024, (stvfs_buf.f_bsize == 512) ? ".5" : "", /* FSID is not very useful, so skipped. */
            (stvfs_buf.f_flag & ST_NOATIME) ? ", NOATIME" : "",
            (stvfs_buf.f_flag & ST_NODIRATIME) ? ", NODIRATIME" : "",
            (stvfs_buf.f_flag & ST_RDONLY) ? ", RO" : "",
            (stvfs_buf.f_flag & ST_RELATIME) ? ", RELATIME" : "",
            (stvfs_buf.f_flag & ST_SYNCHRONOUS) ? ", SYNC" : ""
           );
    if (stvfs_buf.f_flag & ST_SYNCHRONOUS) {
      fprintf(stdout, "Warning: target is mounted with SYNC flag, which might affect results.\n");
    }
  }
  // TODO: also print some information about target device (rather than fs).

#ifndef _GNU_SOURCE
  fprintf(stdout, "Warning: O_DIRECT is not supported on this platform.\n\
Read speed will be lower than its actual value while packet size is small.\n");
#endif
}

static void print_stats(void) {
  struct rusage usage;

  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    fprintf(stdout, "Failed to get statistics: %s\n", strerror(errno));
  } else {
    fprintf(stdout, "CPU Time: %.3f s USER / %.3f s SYS\n", usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6, usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6);
    fprintf(stdout, "Peak RAM Usage: %.3f MiB, %ld Soft Page Faults, %ld Hard Page Faults\n", usage.ru_maxrss / 1024.0f, usage.ru_minflt, usage.ru_majflt); /* ru_maxrss is in KiB. */
#ifdef __linux
    fprintf(stdout, "Block IO: %ld In, %ld Out\n", usage.ru_inblock, usage.ru_oublock);
    fprintf(stdout, "Context Switches: %ld Voluntary, %ld Involuntary\n", usage.ru_nvcsw, usage.ru_nivcsw);
#endif
  }
}

int main(int argc, char *argv[]) {
  struct stat stat_buf;
  char        *mount_point;
  int         o;

  setbuf(stdout, NULL);
  mount_point = NULL;

  // Check argument
  opterr = 0;
  while ((o = getopt(argc, argv, "a:hp:s:")) != -1) {
    switch(o) {
      case 'h': print_help(argv[0]);
      case 's':
        if (!is_uint(optarg)) {
          fprintf(stderr, "Invalid benchmark size: %s.\n\n", optarg);
          print_help(argv[0]);
        }
        sscanf(optarg, "%hu", &bench_size);
        if ((bench_size / TEST_MAX_BLOCK) <= 0) {
          fprintf(stderr, "Invalid benchmark size: %s.\n\n", optarg);
          print_help(argv[0]);
        }
        if ((bench_size % TEST_MAX_BLOCK) != 0) {
          bench_size = (bench_size / TEST_MAX_BLOCK) * TEST_MAX_BLOCK;
          fprintf(stdout, "Shrinking benchmark size to %d MiB.\n", bench_size);
        }
        break;
      case 'a':
        size_pattern = argv[optind - 1];
        check_pattern(size_pattern, argv[0]);
        break;
      case 'p':
        mount_point = argv[optind - 1];
        break;
      case '?':
        if (optopt == 'c') {
          fprintf(stderr, "Option -%c requires an argument.\n\n", optopt);
          print_help(argv[0]);
        }
        if (isprint(optopt)) {
          fprintf(stderr, "Unknown option `-%c'.\n\n", optopt);
          print_help(argv[0]);
        }
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        return EINVAL;
      default: {
        fprintf(stderr, "getopt returned unexpected `\\x%x'.\n", o);
        abort();
      }
    }
  }
  if (mount_point == NULL) {
    fprintf(stderr, "Missing mount point.\n\n");
    print_help(argv[0]);
  }
  if (MAX_PATH_LEN <= snprintf(fn, MAX_PATH_LEN, "%s/%s", mount_point, TEST_FILE_NAME)) {
    fprintf(stderr, "The mount point path provided is too looooong!\n");
    return E2BIG;
  }

  // Check file existence
  if (lstat(fn, &stat_buf) != -1) {
    fprintf(stderr, "Benchmark file \"%s\" already exists.\n\
Please make sure no ubench is running on the device, \
then remove it manually and try to start again.\n", fn);
    return EEXIST;
  } else if (errno != ENOENT) {
    fprintf(stderr, "Something unexpected happened \
while checking the existence of the benchmark file \"%s\".\n", fn);
    return errno;
  }

  // Capture Ctrl-C after fn is set
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);

  // Attempt to open file for binary reading and writting
#ifdef _GNU_SOURCE
  fd = open(fn, O_RDWR | O_CREAT | O_NOATIME | O_DIRECT, 0644);
#else
  // O_NOATIME and O_DIRECT are GNU extension,
  // but the read test is not garuanteed to work without O_DIRECT.
  fd = open(fn, O_RDWR | O_CREAT, 0644);
#endif
  if (fd == -1) {
    int err = errno;
    fprintf(stderr, "Unable to open \"%s\" for benchmark: %s.\n", fn, strerror(errno));
    if (err == EINVAL) {
      fprintf(stderr, "\
======================\n\
It seems that opening the benchmark file with the O_DIRECT flag has failed.\n\
Please make sure you are not running the benchmark on a RAM or MTD device.\n");
    }
    fprintf(stderr, "\nCleaning up...\n");
    unlink(fn);
    return err;
  }

  // Everything looks right, let's start.
  assemble_data();
  print_info(fn);
  fill_out(fd, bench_size, fn);

  // Do the actual job
  fprintf(stdout, "Benchmark started.\n\nSIZE    WRITE     READ\n KiB     KB/s     KB/s\n\
======================\n");
  while (*size_pattern) {
    do_bench(fd, *size_pattern, bench_size);
    size_pattern++;
  }
  fprintf(stdout, "\nBenchmark finished.\n");
  print_stats();

  // Hopefully users would not move test file around.
  // Unlink by fd is no where to be found.
  close(fd);
  return unlink(fn);
}
