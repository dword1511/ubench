#ifndef UBENCH_CONFIG_H
#define UBENCH_CONFIG_H

#define VERSION "r2b0"

#define TEST_FILE_NAME "~ubench.tmp"

/* Max block size for benchmark, in MiB */
#define MAX_BLOCK_SIZE          8
#define MAX_BLOCK_SIZE_EMBEDDED 2

/* Reduced loops for small packets */
#define SMALL_MAX_LOOP          2048
#define SMALL_PACKET_SIZE       65536

#endif /* UBENCH_CONFIG_H */
