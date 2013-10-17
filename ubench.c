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

#include "ubench.h"

// TODO: Check data consistency on none-embedded platform.

int      fd            = -1;

/* Current method does not work for ext4 filesystem on my hard drive (conventional or SSD) */
void do_bench(int fd, char packet, uint32_t size) {
  if(fd == -1 || size < 8) abort();
  static uint32_t        i, loops, packet_size;
  static uint64_t        t;
  static struct timespec tp_pre, tp_post;
  static struct stat     stat_buf;

  // Translate symbol
  packet_size = get_packet_size(packet);

  // Reduce benchmark size for small packet runs.
  loops = size / packet_size;
  if((packet_size < SMALL_PACKET_SIZE) && (loops > SMALL_MAX_LOOP)) {
    loops = SMALL_MAX_LOOP;
    size  = loops * packet_size;
  }

  // Print information
  if(packet_size == 512) fprintf(stdout, " 0.5 ");
  else fprintf(stdout, "%4d ", packet_size / 1024);

  // Clear the effects from last loop or anything else
  lseek(fd, 0, SEEK_SET);
  fsync(fd);
  sleep(1);

  // Write benchmark
#if 1
  t = 0;
  for(i = 0; i < loops; i ++) {
    clock_gettime(TIMER, &tp_pre);
    /* TODO: Check data integrity */
    write(fd, write_buf, packet_size);
    fdatasync(fd);
    clock_gettime(TIMER, &tp_post);

    t += calc_us(tp_pre, tp_post);
    if(stat(fn, &stat_buf) == -1) {
      fprintf(stderr, "\nFile \"%s\" disappeared!\nPlease check device status.\n\
Info: current packet size: %u, writing packet %u of %u.\n", fn, packet_size, i + 1, loops);
      fprintf(stdout, "\nBenchmark terminated: device or filesystem failure.\n");
      _exit(errno);
    }
  }
#else
  clock_gettime(TIMER, &tp_pre);

  for(i = 0; i < loops; i ++) {
    /* TODO: Check data integrity */
    write(fd, write_buf, packet_size);
    fdatasync(fd);
  }

  clock_gettime(TIMER, &tp_post);

  if(stat(fn, &stat_buf) == -1) {
    fprintf(stderr, "\nFile \"%s\" disappeared!\nPlease check device status.\n", fn);
    fprintf(stdout, "\nBenchmark terminated: device or filesystem failure.\n");
    _exit(errno);
  }

  t = calc_us(tp_pre, tp_post);
#endif

  fprintf(stdout, "%8d ", (uint32_t)((uint64_t)size * 1000 / t));

  // Clear the effects from last loop
  lseek(fd, 0, SEEK_SET);
  fsync(fd);
  sleep(1);

  // Read benchmark
  t = 0;


  for(i = 0; i < loops; i ++) {
    /* Under Linux POSIX_FADV_DONTNEED drops cache and POSIX_FADV_RANDOM disables readahead. */
    /* Does not work for ext4 ? */
    fsync(fd);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    fsync(fd);
    posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);

    clock_gettime(TIMER, &tp_pre);
    /* TODO: Check data integrity */
    read(fd, read_buf, packet_size);
    clock_gettime(TIMER, &tp_post);

    t += calc_us(tp_pre, tp_post);
    if(stat(fn, &stat_buf) == -1) {
      fprintf(stderr, "\nFile \"%s\" disappeared!\nPlease check device status.\n\
Info: current packet size: %u, reading packet %u of %u.\n", fn, packet_size, i + 1, loops);
      fprintf(stdout, "\nBenchmark terminated: device or filesystem failure.\n");
      _exit(errno);
    }
  }

  fprintf(stdout, "%8d\n", (uint32_t)((uint64_t)size * 1000 / t));
}

int main(int argc, char *argv[]) {
  struct stat    stat_buf;
  struct utsname uname_buf;
  char           *mount_point;
  char           o;

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
        sscanf(optarg, "%u", &bench_size);
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
Please make sure no ubench is running on the same filesystem, \
and carefully check if it is the desired benchmark file, \
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
#ifdef O_NOATIME
  fd = open(fn, O_RDWR | O_CREAT | O_NOATIME, 0644);
#else
  fprintf(stdout, "NOTICE: It seems that O_NOATIME is not available, falling back (may result in lower performance).\n");
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

  // Warn about possible faulty timing.
#ifdef TIMER_NOT_NTP_PROOF
  fprintf(stdout, "NOTICE: It seems that this type of system does not support CLOCK_MONOTONIC_RAW, timing might not be NTP-proof.\n")
#endif

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

  fill_out(fd, bench_size, fn);

  // Do the actual job
  // TODO: add time stamp one start and stop messages
  fprintf(stdout, "Benchmark started.\n\nSIZE    WRITE     READ\n KiB     KB/s     KB/s\n\
======================\n");
  bench_size *= 1024 * 1024;
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
