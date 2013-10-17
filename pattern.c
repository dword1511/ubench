/* Stuff affected by pattern setting.                       */
/* FIXME: Make the settings automated with macro.           */
/*        May need to generate help information on the fly. */

#include "ubench.h"

#ifdef EMBEDDED
uint32_t bench_size    = 16; // in mega bytes, larger than 2MB
char     *size_pattern = "51248abcdefgh";
#else
uint32_t bench_size    = 256; // in mega bytes, larger than 8MB
char     *size_pattern = "51248abcdefghij";
#endif

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

uint32_t get_packet_size(char packet) {
  uint32_t packet_size = 512;

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
    default : abort(); /* Internal error */
  }

  return packet_size;
}
