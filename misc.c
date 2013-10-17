/* Assorted firends of the main routine */

#include "ubench.h"
#include "rand.h"

/* Currently page-aligned for performance, but does not have to */
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

    fdatasync(fd);
    if(i % 4 == 3)fprintf(stdout, ".");
  }

  fsync(fd);
  fprintf(stdout, "done.\n");
}

/* May need to be changed to nano-second precision in the future, who knows? */
uint64_t calc_us(struct timespec before, struct timespec after) {
  return (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_nsec - before.tv_nsec) / 1000;
}

void signal_handler(int signum) {
  fprintf(stderr, "\nReceived signal %d, benchmark terminated.\nCleaning up...\n", signum);

  if(fd != -1) {
    close(fd);
    unlink(fn);
  }

  _exit(EINTR);
}

int is_uint(char *s) {
  while(*s) {
    if(*s < '0' || *s > '9') return 0;
    s ++;
  }

  return 1;
}
